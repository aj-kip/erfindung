/****************************************************************************

    File: ErfiApu.hpp
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

#ifndef MACRO_HEADER_ERFINDUNG_AUDIO_PU_HPP
#define MACRO_HEADER_ERFINDUNG_AUDIO_PU_HPP

#include <limits>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <array>
#include <functional>
#include <bitset>

#include <cstdint>

#include <SFML/Audio/SoundStream.hpp>

enum class Channel {
    TRIANGLE ,
    PULSE_ONE,
    PULSE_TWO,
    NOISE    ,
    CHANNEL_COUNT
};

enum class ApuRateType {
    NOTE ,
    TEMPO,
    DUTY_CYCLE_WINDOW
};

enum class DutyCycleOption {
    ONE_HALF,
    ONE_THIRD,
    ONE_QUARTER,
    ONE_FIFTH
};

class Apu : private sf::SoundStream {
private:
    struct ChannelInfo {
        int tempo;
        std::bitset<sizeof(int32_t)*8> dc_window;
    };
    using ChannelNoteInfo = std::vector<ChannelInfo>;
    using ChannelSamples = std::vector<std::vector<std::int16_t>>;

public:
    // "Hardware" fixed sample rate
    static constexpr const int SAMPLE_RATE = 11025;

    // arbitary
    static constexpr const int INSTRUCTIONS_PER_THREAD_SYNC = 8;

    struct ApuInst {
        ApuInst(){}
        ApuInst(Channel c, ApuRateType t, int32_t val):
            channel(c), type(t), value(val) {}
        Channel     channel;
        ApuRateType type   ;
        int32_t     value  ;
    };
    using InstructionQueue = std::queue<ApuInst>;

    class Interface {
    public:
        Interface(InstructionQueue * inst_queue);
        Interface() = delete;
        ~Interface();

        Interface(const Interface &) = delete;
        Interface(Interface &&) = delete;

        Interface & operator = (const Interface &) = delete;
        Interface & operator = (Interface &&) = delete;

        void enqueue(Channel, ApuRateType, int);
        void enqueue(ApuInst);

    private:
        InstructionQueue * m_inst_queue;
    };

    // I'm going to follow the NES somewhat (this will be a simplification
    // four channels of mono sound:
    // 2-pulse square waves
    // 1 triangle wave
    // 1 noise channel
    // PSG-like
    // follows same principles involving a command buffer
    // involuntarily multi-threaded (by API designer)
    Apu();

    // apu.access([](Apu::Interface & ai) {
    //    ;
    //    ai.push_note(C::TRIANGLE_WAVE, ...)
    // });
    template <typename Func>
    void access(Func && f) {
        std::unique_lock<std::mutex> ul(m_note_mutex); (void)ul;
        Interface ai(&m_insts);
        f(std::ref(ai));
    }

#   if 0
    void set_notes_per_second(int nps) {
        std::unique_lock<std::mutex> ul(m_note_mutex); (void)ul;
        m_samples_per_note = SAMPLE_RATE / nps;
    }

    void push_note(int, int pitch) {
        std::unique_lock<std::mutex> ul(m_note_mutex); (void)ul;
        m_notes.push_back(pitch);
    }
#   endif

private:
    using Int16 = std::int16_t;

    struct ThreadControl {
        std::mutex mtx;
        std::condition_variable apu_side;
        std::condition_variable main_thread;
    };

    bool onGetData(Chunk & data) override final;

    void onSeek(sf::Time) override final {}

    std::vector<Int16> & select_channel(Channel c)
        { return m_samples_per_channel[static_cast<std::size_t>(c)]; }

    int * select_channel_tempo(Channel c)
        { return &m_channel_info[static_cast<std::size_t>(c)].tempo; }

    std::bitset<32> * select_duty_cycle_window(Channel c)
        { return &m_channel_info[static_cast<std::size_t>(c)].dc_window; }

    std::vector<Int16> m_samples;
    InstructionQueue m_insts;

    std::mutex m_note_mutex;

    ChannelNoteInfo m_channel_info;
    ThreadControl m_thread_control;
    ChannelSamples m_samples_per_channel;
};

#endif
