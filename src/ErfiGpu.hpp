/****************************************************************************

    File: ErfiGpu.hpp
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

#ifndef MACRO_HEADER_GUARD_ERFIN_GPU_HPP
#define MACRO_HEADER_GUARD_ERFIN_GPU_HPP

#include "ErfiDefs.hpp"

#include <map>
#include <queue>
#include <vector>
#include <thread>
#include <memory>
#include <bitset>
#include <condition_variable>

namespace erfin {

struct GpuContext; // implementation detail

class ErfiGpu {
public:
    using MiniSprite = std::bitset<MINI_SPRITE_BIT_COUNT>;
    using VideoMemory = std::vector<bool>;

    static const int SCREEN_WIDTH;
    static const int SCREEN_HEIGHT;

    ErfiGpu();
    ~ErfiGpu();

    // finishes all previous draw operations
    // swaps command buffers
    // swaps graphics buffers
    // waits for frame time, time out
    // begins/resumes all draw operations
    void wait(MemorySpace & mem);

    // high-level functions
    // indicies now "hard coded"
    void upload_sprite(UInt32 address, UInt32 width, UInt32 height, UInt32 index);

    void draw_sprite  (UInt32 x, UInt32 y, UInt32 index);
    void screen_clear ();

    // low-level functions
    void io_write(UInt32);
    UInt32 read() const;

    // you know what?
    // you do your own sprite indicies...
    // it can be hard-coded or handled by software

    // 0b --- sss ii,ii,ii,ii,ii

    const VideoMemory & current_screen() const;

    static bool is_valid_sprite_index(UInt32);

private:
    using CondVar = std::condition_variable;
    friend struct GpuContext;

    struct ThreadControl {
        ThreadControl():
            finish(false), gpu_thread_ready(false), command_buffer_swaped(false)
        {}
        CondVar buffer_ready;
        CondVar gpu_ready;
        std::mutex mtx;
        bool finish;
        bool gpu_thread_ready;
        bool command_buffer_swaped;
    };

    static void do_gpu_tasks
        (std::unique_ptr<GpuContext> & context, const UInt32 * memory,
         ThreadControl & cv);

    ThreadControl m_thread_control;
    std::thread m_gfx_thread;

    std::unique_lock<std::mutex> m_gpus_lock;

    std::unique_ptr<GpuContext> m_cold;
    std::unique_ptr<GpuContext> m_hot ; // hot as in "touch it and get burned"
};

} // end of erfin namespace

#endif
