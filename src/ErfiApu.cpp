/****************************************************************************

    File: ErfiApu.cpp
    Author: Andrew Janke
    License: GPLv3

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*****************************************************************************/

#include "ErfiApu.hpp"

#include <bitset>
#include <iostream>

#include <cmath>
#include <cassert>

namespace {

using Int16 = std::int16_t;

using ChannelSamples = std::vector<std::vector<std::int16_t>>;

constexpr const Int16 MIN_AMP = std::numeric_limits<Int16>::min();
constexpr const Int16 MAX_AMP = std::numeric_limits<Int16>::max();
static constexpr const int SAMPLE_RATE = Apu::SAMPLE_RATE;

template <typename T>
T mag(T t) { return t < T(0) ? -t : t; }

enum class DoNTimes { CONTINUE, BREAK };

// probably should use SFINAE u.u
template <typename Func>
void do_n_times(int n, Func && f) { do { f(); } while(--n); }

template <typename Func>
void do_n_times_until(int n, Func && f) {
    // HA, use case for do while!
    static_assert(std::is_same<decltype(f()), DoNTimes>::value,
                  "Your function must return a DoNTimes enum value.");
    do { if (f() == DoNTimes::BREAK) break; } while(--n);
}

template <typename IntType>
void set_bitset(IntType, std::bitset<sizeof(IntType)*8> &);

using BaseWaveFunction = Int16(*)(Int16);
BaseWaveFunction select_base_wave_function(Channel);

template <typename Container>
void remove_first(Container & c, std::size_t num);

void generate_note(std::vector<Int16> & samples,
                   BaseWaveFunction base_function, int pitch, int tempo,
                   const std::bitset<32> duty_cycles);



} // end of anonymous namespace

Apu::Apu():
    m_channel_info(static_cast<std::size_t>(Channel::CHANNEL_COUNT)),
    m_samples_per_channel(static_cast<std::size_t>(Channel::CHANNEL_COUNT)),
    m_sample_frames(0)
{
    initialize(1, unsigned(SAMPLE_RATE));
    play();
}

void Apu::enqueue(Channel c, ApuRateType t, int val)
    { m_insts_cold.emplace(c, t, val); }

void Apu::enqueue(ApuInst i) { m_insts_cold.push(i); }

void Apu::update(double et) {

    std::unique_lock<std::mutex> locker(m_thread_control.queue_mutex);
    (void)locker;
    while (!m_insts_cold.empty()) {
        m_insts.push(m_insts_cold.front());
        m_insts_cold.pop();
    }
    m_sample_frames += int(std::round(et*double(SAMPLE_RATE)));
    std::cout << "update called " << et << std::endl;

    m_thread_control.queue_ready.notify_one();
}

void Apu::wait_for_play_thread_then_update() {
    //std::unique_lock<std::mutex> locker_(m_thread_control.queue_mutex);
    //(void)locker_;

    std::unique_lock<std::mutex> locker(m_thread_control.wait_for_play);
    m_thread_control.play_ready.wait(locker, [this](){ return m_thread_control.play_has_been_reached; });

    while (!m_insts_cold.empty()) {
        m_insts.push(m_insts_cold.front());
        m_insts_cold.pop();
    }

    m_sample_frames = ALL_POSSIBLE_SAMPLE_FRAMES;
    m_thread_control.queue_ready.notify_one();

}

/* private override final */ bool Apu::onGetData(Chunk & data) {

    int insts = 0;
    int absolute_sample_count = 0;



    // lock apu instruction queue only
    // spurious wake ups are OK! as an empty queue will result in no additional
    // samples being pushed to the audio device
    {

    std::unique_lock<std::mutex> locker(m_thread_control.queue_mutex);
    std::cout << "APU now waiting..." << std::endl;
    {
    // for waiting to play method, otherwise meaningless
    std::unique_lock<std::mutex> ul(m_thread_control.wait_for_play); (void)ul;
    m_thread_control.play_has_been_reached = true;
    m_thread_control.play_ready.notify_one();
    //m_thread_control.queue_ready.wait(locker);
    }
    m_thread_control.queue_ready.wait(locker);
    std::swap(absolute_sample_count, m_sample_frames);

    // edge case: we reach here with update call without any instructions
    // being enqueued
    if (absolute_sample_count == 0) {
        std::cout << "No time passage!..." << std::endl;
        const static Int16 i = 0;
        data.sampleCount = 1;
        data.samples     = &i;
        return true;
    }



    insts = m_insts.size();
    do_n_times_until(INSTRUCTIONS_PER_THREAD_SYNC, [&, this]() {
        if (m_insts.empty()) return DoNTimes::BREAK;

        const auto & inst = m_insts.front();

        switch (inst.type) {
        case ApuRateType::NOTE :
            generate_note(select_channel(inst.channel),
                          select_base_wave_function(inst.channel),
                          inst.value, *select_channel_tempo(inst.channel),
                          *select_duty_cycle_window(inst.channel));
            break;
        case ApuRateType::TEMPO:
            *select_channel_tempo(inst.channel) = SAMPLE_RATE / inst.value;
            break;
        case ApuRateType::DUTY_CYCLE_WINDOW: {
            std::bitset<32> & dc_window = *select_duty_cycle_window(inst.channel);
            set_bitset(inst.value, dc_window);
            break;
        }
        default:
            throw std::runtime_error("Illegal type.");
            break;
        }
        m_insts.pop();
        return DoNTimes::CONTINUE;
    });
    }

    merge_samples(m_samples_per_channel, m_samples, absolute_sample_count);

    // prevents the API from closing the stream
    if (m_samples.empty()) m_samples.push_back(0);

    data.sampleCount = m_samples.size();
    data.samples     = &m_samples[0];

    std::cout << "From " << insts << " instructions, " << data.sampleCount << " samples pushed out." << std::endl;

    return true;
}

/* private static */ void Apu::merge_samples
    (ChannelSamples & channel_samples, std::vector<Int16> & output_samples,
     int sample_count)
{
    // no harm possible, but should be clearer on which member variables
    // "belong" to which thread
    output_samples.clear();
    for (const auto & samples_cont : channel_samples) {
        // for sample count sentinel value: ALL_POSSIBLE_SAMPLE_FRAMES
        // loop behavior will be okay, so long as that comparator is "!="
        for (std::size_t i = 0; i != samples_cont.size() && int(i) != sample_count; ++i) {
            if (i >= output_samples.size())
                output_samples.push_back(samples_cont[i]);
            else
                output_samples[i] += samples_cont[i];
        }
    }
    if (sample_count == ALL_POSSIBLE_SAMPLE_FRAMES) {
        for (const auto & samples_cont : channel_samples)
            sample_count = std::max(sample_count, int(samples_cont.size()));
    }
    for (auto & samples_cont : channel_samples)
        remove_first(samples_cont, sample_count);
    output_samples.resize(sample_count);
}

namespace {

class DutyCycleIterator {
public:
    DutyCycleIterator(const std::bitset<32> &);
    void operator ++ ();
    BaseWaveFunction duty_cycle_function() const;
private:
    int m_position;
    const std::bitset<32> * m_duty_cycle_window;
};

BaseWaveFunction select_base_wave_function(Channel c) {
    using Ch = Channel;
    switch (c) {
    case Ch::TRIANGLE : return [](Int16 t) -> Int16 {
        Int16 rv = (mag(t) > MAX_AMP / 2) ? MAX_AMP - t : t;
        return (rv - (MAX_AMP / 4))*2;
    };
    case Ch::PULSE_ONE:
    case Ch::PULSE_TWO:
    case Ch::NOISE    : return [] (Int16) { return MAX_AMP; };
    case Ch::CHANNEL_COUNT: default: break;
    }
    throw std::runtime_error("Invalid wave function specified.");
}

template <typename IntType>
void set_bitset(IntType x, std::bitset<sizeof(IntType)*8> & bset) {
    for (int pos = 0; x != 0; x >>= 1, ++pos) {
        bset.set(std::size_t(pos), (x & 1) != 0);
    }
}

void generate_note(std::vector<Int16> & samples,
                   BaseWaveFunction base_function, int pitch, int tempo,
                   const std::bitset<32> duty_cycles)
{
    // The duty cycle period is equal to the pitch's period (as if it were
    // a Sine wave
    //Int16 (*base_function)(Int16);
    //int pitch; // in Hertz
    //int tempo; // in samples per note
    //std::bitset<32> duty_cycles;
    DutyCycleIterator dci(duty_cycles);

    // zero hertz is taken as silence
    // (If it's not moving, it doesn't make a sound)
    if (pitch == 0) {
        do_n_times(tempo, [&samples]() { samples.push_back(0); });
        return;
    }

    int wave_position = MIN_AMP; // <- we can apply phase shift here
    for (int sample_position = 0; sample_position != tempo; ++sample_position) {
        samples.push_back(
            dci.duty_cycle_function() // through duty cycle window function
            (base_function(Int16(wave_position))) // compute sample amplitude
        );
        wave_position += pitch;
        if (wave_position > MAX_AMP) {
            // next period
            ++dci;
            wave_position %= MAX_AMP;
        }
    }
}

template <typename Container>
void remove_first(Container & c, std::size_t num) {
    if (num < c.size())
        c.erase(c.begin(), c.begin() + num);
    else
        c.clear();
}

// ----------------------------------------------------------------------------

DutyCycleIterator::DutyCycleIterator(const std::bitset<32> & win_):
    m_position(32 - 2),
    m_duty_cycle_window(&win_)
{}

void DutyCycleIterator::operator ++ () {
    m_position = (m_position + 2) % 32;
}

BaseWaveFunction DutyCycleIterator::duty_cycle_function() const {
    using DO = DutyCycleOption;
    int value = int(m_duty_cycle_window->test(m_position)) |
                int(m_duty_cycle_window->test(m_position + 1)) << 1;
    // careful not to overflow!
    static constexpr const Int16 THIRD_THERSHOLD = -2*(MAX_AMP / 3);
    static constexpr const Int16 QUART_THERSHOLD = -  (MAX_AMP / 2);
    static constexpr const Int16 FIFTH_THERSHOLD = -2*(MAX_AMP / 3);
    switch (static_cast<DutyCycleOption>(value)) {
    case DO::ONE_HALF:
        return [](Int16 x) -> Int16 { return (x > 0) ? 0 : x; };
    case DO::ONE_THIRD:
        return [](Int16 x) -> Int16 { return (x > THIRD_THERSHOLD) ? 0 : x; };
    case DO::ONE_QUARTER:
        return [](Int16 x) -> Int16 { return (x > QUART_THERSHOLD) ? 0 : x; };
    case DO::ONE_FIFTH:
        return [](Int16 x) -> Int16 { return (x > FIFTH_THERSHOLD) ? 0 : x; };
    default:
        assert(false);
        throw std::runtime_error("Impossible branch?!");
        break;
    }
    return nullptr;
}

} // end of <anonymous> namespace
