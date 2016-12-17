/****************************************************************************

    File: ErfiGpu.cpp
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

#include "ErfiGpu.hpp"
#include <cassert>

namespace erfin {

/* static */ const int Gpu::SCREEN_WIDTH  = 320;
/* static */ const int Gpu::SCREEN_HEIGHT = 240;

Gpu::Gpu():
    m_screen_width(SCREEN_WIDTH)
{
    m_screen_pixels.resize(SCREEN_WIDTH*SCREEN_HEIGHT, false);
}

void Gpu::set_index(UInt32 index) {
    m_sprite_map[index] = { 0, 0, std::vector<bool>() };
    m_cmd_queue.push({ GOpCode::SET_INDEX, index, 0 });
}

void Gpu::load(UInt32 addr) {
    m_cmd_queue.push({ GOpCode::LOAD, addr, 0 });
}

void Gpu::size(UInt32 w, UInt32 h) {
    m_cmd_queue.push({ GOpCode::SIZE, w, h });
}

void Gpu::draw(UInt32 x, UInt32 y) {
    m_cmd_queue.push({ GOpCode::DRAW, x, y });
}

void Gpu::draw_flush(MemorySpace & mem) {
    const UInt32 INVALID_INDEX = UInt32(-1);
    UInt32 cur_index = INVALID_INDEX;
    while (!m_cmd_queue.empty()) {
        GInst & inst = m_cmd_queue.front();
        SpriteMeta * smeta = nullptr;
        if (cur_index != INVALID_INDEX)
            smeta = &(m_sprite_map[cur_index]);
        switch (inst.op) {
        case GOpCode::SET_INDEX:
            cur_index = inst.arg0;
            break;
        case GOpCode::LOAD:
            load_action(inst, smeta, mem);
            break;
        case GOpCode::SIZE:
            if (!smeta) break;
            smeta->pixels.clear();
            smeta->pixels.resize(inst.arg0*inst.arg1, false);
            smeta->width = inst.arg0;
            smeta->height = inst.arg1;
            break;
        case GOpCode::DRAW:
            draw_action(inst, smeta);
            break;
        default: assert(false); break;
        }

        m_cmd_queue.pop();
    }
}

/* private */ void Gpu::draw_action(GInst & inst, SpriteMeta * smeta) {
    if (!smeta) return;

    assert(int(smeta->pixels.size()) == smeta->height*smeta->width);

    for (int y = 0; y != smeta->height; ++y) {
        int y_comp = y + inst.arg1;
        if (y_comp >= int(m_screen_pixels.size()/m_screen_width))
            return;
        for (int x = 0; x != smeta->width ; ++x) {
            int x_comp = x + inst.arg0;
            int index = x_comp + y_comp*m_screen_width;
            if (index >= int(m_screen_pixels.size()))
                break;
            m_screen_pixels[index] = smeta->pixels[x + y*smeta->width];
        }
    }
}

/* private */ void Gpu::load_action
    (GInst & inst, SpriteMeta * smeta, MemorySpace & mem)
{
    if (!smeta) return;
    smeta->pixels.clear();
    UInt32 * data = &mem[inst.arg0];
    int bit_index = 0;
    for (int i = 0; i != smeta->height*smeta->width; ++i) {
        smeta->pixels.push_back((*data >> (31 - bit_index)) & 0x1);
        if (bit_index == 31) {
            ++data;
            bit_index = 0;
        } else {
            ++bit_index;
        }
    }
}

} // end of erfin namespace
