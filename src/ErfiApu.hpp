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

#include <vector>
#include <queue>
#include <random>
#include <bitset>

#include <cstdint>

#include "ErfiDefs.hpp"

namespace erfin {

class SfmlAudioDevice;

// Dev notes:
// unimplemented: duty-cycles, noise channel
// duty-cycles
// - as multipliers?
// - - one for "full-on" then
// pulse channels are just straight square waves
//
class Apu {
public:
    friend class ApuAttorney;

    struct ApuInst {
        ApuInst(){}
        ApuInst(Channel c, ApuInstructionType t, int32_t val):
            channel(c), type(t), value(val) {}
        Channel            channel;
        ApuInstructionType type   ;
        int32_t            value  ;
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

    ~Apu();

    void enqueue(Channel, ApuInstructionType, int);

    void enqueue(ApuInst);

    void update();

    void io_write(UInt32);

private:
    // ------------------------------------------------------------------------

    // "Hardware" fixed sample rate
    static constexpr const int SAMPLE_RATE = 11025;

    using DutyCycleWindow  = std::bitset<sizeof(int32_t)*8>;
    using InstructionQueue = std::queue<UInt32>;

    static const constexpr int ALL_POSSIBLE_SAMPLE_FRAMES = -1;

    // channel stuff
    using Int16 = std::int16_t;
    struct ChannelInfo {
        int tempo;
        DutyCycleWindow dc_window;
    };
    using ChannelNoteInfo = std::vector<ChannelInfo>;
    using ChannelSamples = std::vector<std::vector<std::int16_t>>;

    std::vector<Int16> & select_channel(Channel c)
        { return m_samples_per_channel[static_cast<std::size_t>(c)]; }

    int * select_channel_tempo(Channel c)
        { return &m_channel_info[static_cast<std::size_t>(c)].tempo; }

    DutyCycleWindow * select_duty_cycle_window(Channel c)
        { return &m_channel_info[static_cast<std::size_t>(c)].dc_window; }

    void process_instructions();
    void generate_note(Channel, int note);

    static void merge_samples(ChannelSamples &, std::vector<Int16> &,
                              int sample_count);

    std::vector<Int16> m_samples;
    InstructionQueue m_insts;

    ChannelNoteInfo m_channel_info;
    ChannelSamples m_samples_per_channel;
    SfmlAudioDevice * m_audio_device;
    std::default_random_engine m_rng;
};

class ApuAttorney {
    friend class SfmlAudioDevice;
    static constexpr const int SAMPLE_RATE = Apu::SAMPLE_RATE;
};

} // end of erfin namespace

#endif
