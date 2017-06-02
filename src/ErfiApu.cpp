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

#include <SFML/Audio/SoundStream.hpp>

#include <bitset>
#include <iostream>
#include <mutex>

#include <cmath>
#include <cassert>

namespace {

using Int16 = std::int16_t;
using Error = std::runtime_error;

constexpr const Int16 MIN_AMP = std::numeric_limits<Int16>::min();
constexpr const Int16 MAX_AMP = std::numeric_limits<Int16>::max();

constexpr const std::size_t DUTY_CYCLE_WINDOW_SIZE = sizeof(int32_t)*8;
using DutyCycleWindow = std::bitset<DUTY_CYCLE_WINDOW_SIZE>;

using ChannelSamples  = std::vector<std::vector<std::int16_t>>;

template <typename T>
T mag(T t) { return t < T(0) ? -t : t; }

enum class DoNTimes { CONTINUE, BREAK };

// probably should use SFINAE u.u
template <typename Func>
void do_n_times(int n, Func && f) { do { f(); } while(--n); }

template <typename Func>
void do_n_times_until(int n, Func && f);

template <typename IntType>
void set_bitset(IntType, std::bitset<sizeof(IntType)*8> &);

using BaseWaveFunction = Int16(*)(Int16);
BaseWaveFunction select_base_wave_function(erfin::Channel);

template <typename Container>
void remove_first(Container & c, std::size_t num);

void generate_note(std::vector<Int16> & samples,
                   BaseWaveFunction base_function, int pitch, int tempo,
                   const DutyCycleWindow duty_cycles);

} // end of anonymous namespace

namespace erfin {

class SfmlAudioDevice : private sf::SoundStream {
public:
    SfmlAudioDevice();
    void upload_samples(const std::vector<Int16> &);
    ~SfmlAudioDevice() { stop(); }
private:
    bool onGetData(Chunk & data) override final;

    void onSeek(sf::Time) override final {}

    std::vector<Int16> m_samples;
    std::vector<Int16> m_samples_hot;
    std::mutex m_samples_mutex;
};

Apu::Apu():
    m_channel_info       (static_cast<std::size_t>(Channel::CHANNEL_COUNT)),
    m_samples_per_channel(static_cast<std::size_t>(Channel::CHANNEL_COUNT)),
    m_sample_frames      (0),
    m_audio_device       (new SfmlAudioDevice())
{
    static_assert(std::is_same<DutyCycleWindow, ::DutyCycleWindow>::value,
                  "Implementation detail DutyCycleWindow definition needs to "
                  "be updated.");
}

Apu::~Apu() { delete m_audio_device; }

void Apu::enqueue(Channel c, ApuInstructionType t, int val) {
    for (auto i : { UInt32(c), UInt32(t), UInt32(val) })
        m_insts.push(i);
}

void Apu::enqueue(ApuInst i) { enqueue(i.channel, i.type, i.value); }

void Apu::update() {
    process_instructions();
    merge_samples(m_samples_per_channel, m_samples, ALL_POSSIBLE_SAMPLE_FRAMES);
    m_audio_device->upload_samples(m_samples);
    m_samples.clear();
}

void Apu::io_write(UInt32 data) { m_insts.push(data); }

/* private */ void Apu::process_instructions() {
    static constexpr const char * const INVALID_INST_ERROR_MSG =
        "APU was provided an invalid instruction value, this could be a "
        "result of pushing values out of order to the APU.";

    static constexpr const char * const INVALID_CHNL_ERROR_MSG =
        "APU was provided an invalid channel value, this could be a "
        "result of pushing values out of order to the APU.";

    auto front_and_pop = [this]() -> UInt32
        { auto i = m_insts.front(); m_insts.pop(); return i; };

    while (!m_insts.empty() && m_insts.size() >= 3) {
        auto channel   = static_cast<Channel>(front_and_pop());
        auto inst_type = static_cast<ApuInstructionType>(front_and_pop());
        auto value     = int(front_and_pop());

        if (!is_valid_value(inst_type)) throw Error(INVALID_INST_ERROR_MSG);
        if (!is_valid_value(channel  )) throw Error(INVALID_CHNL_ERROR_MSG);

        switch (inst_type) {
        case ApuInstructionType::NOTE :
            generate_note(select_channel(channel),
                          select_base_wave_function(channel),
                          value, *select_channel_tempo(channel),
                          *select_duty_cycle_window(channel));
            break;
        case ApuInstructionType::TEMPO:
            *select_channel_tempo(channel) = SAMPLE_RATE / value;
            break;
        case ApuInstructionType::DUTY_CYCLE_WINDOW:
            set_bitset(value, *select_duty_cycle_window(channel));
            break;
        default: std::terminate(); // must change is_valid_value for inst_type!
        }
    }
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
                output_samples[i] = samples_cont[i];
        }
    }
    if (sample_count == ALL_POSSIBLE_SAMPLE_FRAMES) {
        for (const auto & samples_cont : channel_samples)
            sample_count = std::max(sample_count, int(samples_cont.size()));
    }
    for (auto & samples_cont : channel_samples)
        remove_first(samples_cont, std::size_t(sample_count));
    output_samples.resize(std::size_t(sample_count));
}

} // end of erfin namespace

namespace {

class DutyCycleIterator {
public:
    static constexpr const int BITS_PER_DUTY_CYCLE_FUNCTION = 2;

    DutyCycleIterator(const DutyCycleWindow &);
    void operator ++ ();
    BaseWaveFunction duty_cycle_function() const;
private:
    int m_position;
    const DutyCycleWindow * m_duty_cycle_window;
};

BaseWaveFunction select_base_wave_function(erfin::Channel c) {
    using Ch = erfin::Channel;
    static constexpr const Int16 MAX = std::numeric_limits<Int16>::max();
    switch (c) {
    //      | .
    //      |. .
    // -.---.---.--
    //   . .|
    //    . |
    case Ch::TRIANGLE : return [](Int16 t) -> Int16 {
        if (mag(t) < MAX / 2) return t;
        // I also like it when the intercepts for the following function
        // segments are doubled (we'll have to control for overflow then
        if (t < 0) return Int16(-int(t) - int(MAX));
        if (t > 0) return Int16(-int(t) + int(MAX));
        assert(false);
        return 0; // gcc seems to be pessimistic about flow control
    };
    case Ch::PULSE_ONE:
    case Ch::PULSE_TWO:
    case Ch::NOISE    :
        return [] (Int16 x) -> Int16 { return (x < 0) ? -MAX : MAX; };
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

template <typename Func>
void do_n_times_until(int n, Func && f) {
    // HA, use case for do while!
    static_assert(std::is_same<decltype(f()), DoNTimes>::value,
                  "Your function must return a DoNTimes enum value.");
    do { if (f() == DoNTimes::BREAK) break; } while(--n);
}

void generate_note(std::vector<Int16> & samples,
                   BaseWaveFunction base_function, int pitch, int tempo,
                   const DutyCycleWindow duty_cycles)
{
    // The duty cycle period is equal to the pitch's period (as if it were
    // a Sine wave
    //Int16 (*base_function)(Int16);
    //int pitch; // in Hertz
    //int tempo; // in samples per note
    //std::bitset<32> duty_cycles;
    DutyCycleIterator dci(duty_cycles);
    if (tempo == 0) {
        throw Error("Tempo was not set for this channel, cannot generate note!");
    }

    // zero hertz is taken as silence
    // (If it's not moving, it doesn't make a sound)
    if (pitch == 0) {
        do_n_times(tempo, [&samples]() { samples.push_back(0); });
        return;
    }

    int wave_position = -MAX_AMP; // <- we can apply phase shift here
    int last_sample = 0;
    for (int sample_position = 0; sample_position != tempo; ++sample_position) {
        samples.push_back(
            //dci.duty_cycle_function()(Int16(wave_position))* // duty cycle function
            base_function(Int16(wave_position))       // compute sample amplitude
        );
        wave_position += pitch;
        if (wave_position > MAX_AMP) {
            // next period
            ++dci;
            last_sample = sample_position;
            wave_position = -MAX_AMP;
        }
    }
    for (int i = last_sample; i != tempo; ++i)
        samples[std::size_t(i)] = 0;
}

template <typename Container>
void remove_first(Container & c, std::size_t num) {
    if (num < c.size())
        c.erase(c.begin(), c.begin() + unsigned(num));
    else
        c.clear();
}

} // end of <anonymous> namespace

namespace erfin {

SfmlAudioDevice::SfmlAudioDevice() {
    initialize(1, ApuAttorney::SAMPLE_RATE);
}

void SfmlAudioDevice::upload_samples(const std::vector<Int16> & samples_) {
    std::unique_lock<std::mutex> lock(m_samples_mutex);
    (void)lock;
    m_samples.insert(m_samples.end(), samples_.begin(), samples_.end());
    if (!m_samples.empty() && getStatus() == Stopped)
        play();
#   if 0
    std::cout << "Uploaded " << samples_.size() << " samples." << std::endl;
#   endif
}

/* private override final */ bool SfmlAudioDevice::onGetData(Chunk & data) {
    {
    std::unique_lock<std::mutex> lock(m_samples_mutex);
    (void)lock;
#   if 0
    std::cout << "Swapping in " << m_samples.size() << " samples." << std::endl;
#   endif
    m_samples_hot.swap(m_samples);
    m_samples.clear();
    }
    if (m_samples_hot.empty()) {
        m_samples_hot.resize(1000, 0);
    }
    if (m_samples_hot.empty()) {
        return false;
    } else {
        data.sampleCount =  m_samples_hot.size();
        data.samples     = &m_samples_hot.front();
    }

    return true;
}

} // end of erfin namespace

// ----------------------------------------------------------------------------

namespace {

DutyCycleIterator::DutyCycleIterator(const DutyCycleWindow & win_):
    m_position(DUTY_CYCLE_WINDOW_SIZE - BITS_PER_DUTY_CYCLE_FUNCTION),
    m_duty_cycle_window(&win_)
{}

void DutyCycleIterator::operator ++ () {
    m_position = (m_position + BITS_PER_DUTY_CYCLE_FUNCTION) %
                 int(DUTY_CYCLE_WINDOW_SIZE);
}

BaseWaveFunction DutyCycleIterator::duty_cycle_function() const {
    static_assert(BITS_PER_DUTY_CYCLE_FUNCTION == 2,
                  "This function needs to be fixed to take into account "
                  "for the new 'sub-window' size.");

    using DO = erfin::DutyCycleOption;

    const auto pos = std::size_t(m_position);
    int value = int(m_duty_cycle_window->test(pos)) |
                int(m_duty_cycle_window->test(pos + 1)) << 1;

    // careful not to overflow!
    static constexpr const Int16 THIRD_THERSHOLD = -2*(MAX_AMP / 3);
    static constexpr const Int16 QUART_THERSHOLD = -  (MAX_AMP / 2);
    static constexpr const Int16 FIFTH_THERSHOLD = -2*(MAX_AMP / 3);

    switch (static_cast<DO>(value)) {
    case DO::ONE_HALF:
        return [](Int16 x) -> Int16 { return (x > 0) ? 0 : 1; };
    case DO::ONE_THIRD:
        return [](Int16 x) -> Int16 { return (x > THIRD_THERSHOLD) ? 0 : 1; };
    case DO::ONE_QUARTER:
        return [](Int16 x) -> Int16 { return (x > QUART_THERSHOLD) ? 0 : 1; };
    case DO::ONE_FIFTH:
        return [](Int16 x) -> Int16 { return (x > FIFTH_THERSHOLD) ? 0 : 1; };
    default:
        assert(false);
        throw std::runtime_error("Impossible branch?!");
    }
}

} // end of <anonymous> namespace
