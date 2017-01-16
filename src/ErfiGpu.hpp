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

struct GpuContext; // implementation detail

class ErfiGpu {
public:
    ErfiGpu();
    ~ErfiGpu();

    // finishes all previous draw operations (join)
    // swaps command buffers
    // swaps graphics buffers
    // waits for frame time, time out
    // begins/resumes all draw operations ("split")
    void wait(MemorySpace & mem);

    UInt32 upload_sprite(UInt32 width, UInt32 height, UInt32 address);
    void   unload_sprite(UInt32 index);
    void   draw_sprite  (UInt32 x, UInt32 y, UInt32 index);
    void   screen_clear ();
#   if 0
    template <typename Func>
    void draw_pixels(Func f) {
        int x = 0, y = 0;
        for (auto b : m_cold.pixels) {
            f(x, y, b);
            ++x;
            if (x == m_screen_width) {
                x = 0;
                ++y;
            }
        }
    }
#   endif
    static const int SCREEN_WIDTH;
    static const int SCREEN_HEIGHT;

private:
    void upload_sprite(UInt32 index, UInt32 width, UInt32 height, UInt32 address);

    using VideoMemory = std::vector<bool>;
    friend struct GpuContext;

    struct SpriteMeta {
        SpriteMeta(): width(0), height(0), delete_flag(false) {}
        SpriteMeta(UInt32 w_, UInt32 h_): width(w_), height(h_), delete_flag(false) {}

        UInt32 width;
        UInt32 height;
        std::vector<bool> pixels;
        bool delete_flag;
    };

    static void do_gpu_tasks(GpuContext & context, const UInt32 * memory);

    std::map<UInt32, SpriteMeta> m_sprite_map;
    int m_screen_width;
    UInt32 m_index_pos;
    std::thread m_gfx_thread;

    std::unique_ptr<GpuContext> m_cold;
    std::unique_ptr<GpuContext> m_hot ; // hot as in "touch it and get burned"
};

} // end of erfin namespace

#endif
