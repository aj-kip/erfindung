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
#include <future>
#include <thread>

namespace erfin {

namespace gpu_enum_types {

enum GpuOpCode_e {
    UPLOAD = enum_types::UPLOAD_SPRITE ,
    UNLOAD = enum_types::UNLOAD_SPRITE ,
    DRAW   = enum_types::DRAW_SPRITE   ,
    CLEAR  = enum_types::SCREEN_CLEAR
};

} // end of gpu_enum_types namespace

using GpuOpCode = gpu_enum_types::GpuOpCode_e;

class GpuCommandBuffer {
public:
    friend class GpuAttorney; // for pop ?
    // command buffer methods

    void unload_sprite(UInt32 index);
    void draw_sprite  (UInt32 x, UInt32 y, UInt32 index);
    void screen_clear ();
protected:
    void upload_sprite(UInt32 index, UInt32 width, UInt32 height, UInt32 address);

    UInt32 pop();
    bool is_empty() const { return m_queue.empty(); }
    void swap(GpuCommandBuffer & other) { m_queue.swap(other.m_queue); }
private:
    std::queue<UInt32> m_queue;
};

class ErfiGpu;

class GpuAttorney {
    friend class ErfiGpu;
    static UInt32 pop(GpuCommandBuffer & inst) { return inst.pop(); }
    static bool is_empty(const GpuCommandBuffer & inst)
        { return inst.is_empty(); }
};

class ErfiGpu : public GpuCommandBuffer {
public:
    ErfiGpu();

    // finishes all previous draw operations (join)
    // swaps command buffers
    // swaps graphics buffers
    // waits for frame time, time out
    // begins/resumes all draw operations ("split")
    void wait(MemorySpace & mem);

    UInt32 upload_sprite(UInt32 width, UInt32 height, UInt32 address);

    template <typename Func>
    void draw_pixels(Func f) {
        int x = 0, y = 0;
        for (auto b : m_screen_pixels) {
            f(x, y, b);
            ++x;
            if (x == m_screen_width) {
                x = 0;
                ++y;
            }
        }
    }

    static const int SCREEN_WIDTH;
    static const int SCREEN_HEIGHT;

private:
    using VideoMemory = std::vector<bool>;
    using GpuUpdateTask = std::packaged_task<int(GpuCommandBuffer&, VideoMemory&)>;

    struct SpriteMeta {
        int width;
        int height;
        std::vector<bool> pixels;
    };

    static int do_gpu_tasks(GpuCommandBuffer & commands, VideoMemory & video_mem);

    std::map<UInt32, SpriteMeta> m_sprite_map;
    int m_screen_width;
    VideoMemory m_screen_pixels;

    UInt32 m_index_pos;

    std::thread m_gfx_thread;
    struct {
        GpuUpdateTask gpu_task;
        GpuCommandBuffer command_buffer;
        VideoMemory working_pixels;
        std::future<int> signal;
    } m_hot;
};

} // end of erfin namespace

#endif
