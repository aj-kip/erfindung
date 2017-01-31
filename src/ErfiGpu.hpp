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
    UPLOAD = std::size_t(enum_types::SystemCallValue_e::UPLOAD_SPRITE),
    UNLOAD = std::size_t(enum_types::SystemCallValue_e::UNLOAD_SPRITE),
    DRAW   = std::size_t(enum_types::SystemCallValue_e::DRAW_SPRITE  ),
    CLEAR  = std::size_t(enum_types::SystemCallValue_e::SCREEN_CLEAR )
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

    template <typename Func>
    bool draw_pixels(Func f);

    static const int SCREEN_WIDTH;
    static const int SCREEN_HEIGHT;

private:

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

    void upload_sprite(UInt32 index, UInt32 width, UInt32 height, UInt32 address);
    std::size_t next_set_pixel(std::size_t i);

    static void do_gpu_tasks(GpuContext & context, const UInt32 * memory);

    std::map<UInt32, SpriteMeta> m_sprite_map;
    //int m_screen_width;
    UInt32 m_index_pos;
    std::thread m_gfx_thread;

    std::unique_ptr<GpuContext> m_cold;
    std::unique_ptr<GpuContext> m_hot ; // hot as in "touch it and get burned"
};

template <typename Func>
bool ErfiGpu::draw_pixels(Func f) {
    //static int cnt = 1;

    //if ((cnt = ((cnt + 1) % 2)) == 0) return false;

    const std::size_t END = std::size_t(-1);
    for (std::size_t i = next_set_pixel(0); i != END; i = next_set_pixel(i)) {
        int x = int(i) % SCREEN_WIDTH;
        int y = int(i) / SCREEN_WIDTH;
        f(x, y);
    }
    return true;
}

} // end of erfin namespace

#endif
