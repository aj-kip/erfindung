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
#include <iostream> // thread_safe_print
#include <atomic>

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

template <typename T>
void thread_safe_print(const T & obj) {
    static std::mutex mtx;
    std::unique_lock<std::mutex> ul(mtx); (void)ul;
    std::cout << obj;
}

class Apu : private sf::SoundStream {
public:

    struct ApuInst {
        ApuInst(){}
        ApuInst(Channel c, ApuRateType t, int32_t val):
            channel(c), type(t), value(val) {}
        Channel     channel;
        ApuRateType type   ;
        int32_t     value  ;
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

    void enqueue(Channel, ApuRateType, int);

    void enqueue(ApuInst);

    void update(double et);

    /** @brief Blocks the current the thread until the audio thread is up,
     *         running and ready for instructions, then finally calls update.
     *  The purpose of this function is for programs (whether C++ or Erfindung)
     *  that wish to play music/sound and nothing else. It is present for
     *  debugging and tinkering purposes. \n
     *  Update is called with elapsed time set to zero.
     */
    void wait_for_play_thread_then_update();

private:

    // ------------------------------------------------------------------------

    // "Hardware" fixed sample rate
    static constexpr const int SAMPLE_RATE = 11025;

    using DutyCycleWindow = std::bitset<sizeof(int32_t)*8>;

    static const constexpr int ALL_POSSIBLE_SAMPLE_FRAMES = -1;

    using InstructionQueue = std::queue<ApuInst>;

    // channel stuff
    using Int16 = std::int16_t;
    struct ChannelInfo {
        int tempo;
        DutyCycleWindow dc_window;
    };
    using ChannelNoteInfo = std::vector<ChannelInfo>;
    using ChannelSamples = std::vector<std::vector<std::int16_t>>;

    static constexpr const int INSTRUCTIONS_PER_THREAD_SYNC = 16;
    struct ThreadControl {
        // usual instruction queue resource access control
        std::mutex queue_mutex;
        std::condition_variable queue_ready;
        // for wait_for_play_thread_then_update method
        std::mutex wait_for_play;
        std::condition_variable play_ready;
        bool play_has_been_reached;
    };

    bool onGetData(Chunk & data) override final;

    void onSeek(sf::Time) override final {}

    std::vector<Int16> & select_channel(Channel c)
        { return m_samples_per_channel[static_cast<std::size_t>(c)]; }

    int * select_channel_tempo(Channel c)
        { return &m_channel_info[static_cast<std::size_t>(c)].tempo; }

    DutyCycleWindow * select_duty_cycle_window(Channel c)
        { return &m_channel_info[static_cast<std::size_t>(c)].dc_window; }

    static void merge_samples(ChannelSamples &, std::vector<Int16> &,
                              int sample_count);

    std::vector<Int16> m_samples;
    InstructionQueue m_insts;
    InstructionQueue m_insts_cold;

    ChannelNoteInfo m_channel_info;
    ThreadControl m_thread_control;
    ChannelSamples m_samples_per_channel;
    int m_sample_frames;
};

#endif
