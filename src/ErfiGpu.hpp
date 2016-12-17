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

namespace erfin {

class Gpu {
public:
    Gpu();
    void set_index(UInt32 index);
    void load(UInt32 addr);
    void size(UInt32 w, UInt32 h);
    void draw(UInt32 x, UInt32 y);
    void draw_flush(MemorySpace & mem);

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

    enum class GOpCode : UInt8 {
        SET_INDEX,
        LOAD,
        SIZE,
        DRAW
    };
    struct GInst {
        GOpCode op;
        UInt32 arg0;
        UInt32 arg1;
    };
    struct SpriteMeta {
        int width;
        int height;
        std::vector<bool> pixels;
    };

    void draw_action(GInst & inst, SpriteMeta * smeta);
    void load_action(GInst & inst, SpriteMeta * smeta, MemorySpace & mem);

    std::queue<GInst> m_cmd_queue;
    std::map<UInt32, SpriteMeta> m_sprite_map;
    int m_screen_width;
    std::vector<bool> m_screen_pixels;
};

} // end of erfin namespace

#endif
