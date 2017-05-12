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

#include <cassert>

namespace {

using Int16 = std::int16_t;

using ChannelSamples = std::vector<std::vector<std::int16_t>>;

constexpr const Int16 MIN_AMP = std::numeric_limits<Int16>::min();
constexpr const Int16 MAX_AMP = std::numeric_limits<Int16>::max();
static constexpr const int SAMPLE_RATE = Apu::SAMPLE_RATE;

template <typename T>
T mag(T t) { return t < T(0) ? -t : t; }

template <typename Func>
void do_n_times(int n, Func && f) {
    // HA, use case for do while!
    do { f(); } while(--n);
}

template <typename IntType>
void set_bitset(IntType, std::bitset<sizeof(IntType)*8> &);

using BaseWaveFunction = Int16(*)(Int16);
BaseWaveFunction select_base_wave_function(Channel);

void generate_note(std::vector<Int16> & samples,
                   BaseWaveFunction base_function, int pitch, int tempo,
                   const std::bitset<32> duty_cycles);

} // end of anonymous namespace

Apu::Interface::Interface(InstructionQueue * inst_queue):
    m_inst_queue(inst_queue)
{}

Apu::Interface::~Interface() {
#   ifdef MACRO_DEBUG
    m_inst_queue = nullptr; // safety
#   endif
}

void Apu::Interface::enqueue(Channel c, ApuRateType t, int val)
    { m_inst_queue->emplace(c, t, val); }

void Apu::Interface::enqueue(ApuInst i) { m_inst_queue->push(i); }

Apu::Apu():
    m_channel_info(static_cast<std::size_t>(Channel::CHANNEL_COUNT)),
    m_samples_per_channel(static_cast<std::size_t>(Channel::CHANNEL_COUNT))
{
    initialize(1, unsigned(SAMPLE_RATE));
    play();
}

/* private override final */ bool Apu::onGetData(Chunk & data) {
    (void)data;
    std::unique_lock<std::mutex> ul(m_note_mutex); (void)ul;
    do_n_times(INSTRUCTIONS_PER_THREAD_SYNC, [&, this]() {
        if (m_insts.empty()) return;
        const auto & inst = m_insts.front();

        switch (inst.type) {
        case ApuRateType::NOTE : {
            generate_note(select_channel(inst.channel),
                          select_base_wave_function(inst.channel),
                          inst.value, *select_channel_tempo(inst.channel),
                          *select_duty_cycle_window(inst.channel));
            break;
        }
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
    });
#   if 0
    auto the_min = std::numeric_limits<std::size_t>::max();
    for (const auto & samples_cont : m_samples_per_channel)
        the_min = std::min(the_min, samples_cont.size());
    m_samples.reserve(the_min);

    for (decltype(the_min) i = 0; i != the_min; ++i) {
        Int16 sum = 0;
        for (const auto & samples_cont : m_samples_per_channel)
            sum += samples_cont[i];
        m_samples.push_back(sum);
    }

    auto erase_first_min = [the_min](std::vector<Int16> & cont)
        { cont.erase(cont.begin(), cont.begin() + the_min); };

    for (std::vector<Int16> & samples_cont : m_samples_per_channel)
        erase_first_min(samples_cont);
#   endif
    m_samples.clear();
    for (const auto & samples_cont : m_samples_per_channel) {
        for (std::size_t i = 0; i != samples_cont.size(); ++i) {
            if (i >= m_samples.size())
                m_samples.push_back(samples_cont[i]);
            else
                m_samples[i] += samples_cont[i];
        }
    }
    data.sampleCount = m_samples.size();
    data.samples     = &m_samples[0];
#   if 0
    using UInt16 = std::uint16_t;
    constexpr const Int16 max = std::numeric_limits<Int16>::max();

    std::vector<int> notes;
    {
    std::unique_lock<std::mutex> ul(m_note_mutex); (void)ul;
    if (m_notes.empty()) {
        return silence(data);
    }
    m_notes.swap(notes);
    }
    m_samples.clear();
    m_samples.reserve(notes.size()*std::size_t(m_samples_per_note));
    for (int note : notes) {
        if (note == 0) {
            do_n_times(m_samples_per_note, [this]() {
                m_samples.push_back(0);
            });
        } else {
            int x = 0;
            for (int i = 0; i != m_samples_per_note; ++i) {
                if (i < (m_samples_per_note*7)/10)
                    m_samples.push_back(triangle_wave(UInt16(x)));
                else
                    m_samples.push_back(0);
                x = (x + note) % max;
            }
        }
    }
    data.sampleCount = m_samples.size();
    data.samples     = &m_samples[0];
#   endif
    return true;
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
