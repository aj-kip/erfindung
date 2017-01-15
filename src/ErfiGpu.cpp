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

/* static */ const int ErfiGpu::SCREEN_WIDTH  = 320;
/* static */ const int ErfiGpu::SCREEN_HEIGHT = 240;

ErfiGpu::ErfiGpu():
    m_screen_width(SCREEN_WIDTH),
    m_index_pos(0)
{
    m_cold.pixels.resize(SCREEN_WIDTH*SCREEN_HEIGHT, false);
}

ErfiGpu::~ErfiGpu() {
    if (m_gfx_thread.joinable())
        m_gfx_thread.join();
}

void ErfiGpu::wait(MemorySpace &) {
    if (m_gfx_thread.joinable())
        m_gfx_thread.join();

    m_cold.swap(m_hot);

    std::thread t1(do_gpu_tasks,
                   std::ref(m_hot.command_buffer),
                   std::ref(m_hot.pixels));

    m_gfx_thread.swap(t1);
}

UInt32 ErfiGpu::upload_sprite(UInt32 width, UInt32 height, UInt32 address) {
    m_sprite_map[m_index_pos] = { int(width), int(height), std::vector<bool>() };
    upload_sprite(m_index_pos, width, height, address);
    return m_index_pos++;
}

void ErfiGpu::unload_sprite(UInt32 index) {
    m_cold.command_buffer.push(gpu_enum_types::UNLOAD);
    m_cold.command_buffer.push(index);
}

void ErfiGpu::draw_sprite(UInt32 x, UInt32 y, UInt32 index) {
    m_cold.command_buffer.push(gpu_enum_types::DRAW);
    m_cold.command_buffer.push(x);
    m_cold.command_buffer.push(y);
    m_cold.command_buffer.push(index);
}

void ErfiGpu::screen_clear() {
    m_cold.command_buffer.push(gpu_enum_types::CLEAR);
}

/* private */ void ErfiGpu::upload_sprite
    (UInt32 index, UInt32 width, UInt32 height, UInt32 address)
{
    m_cold.command_buffer.push(gpu_enum_types::UPLOAD);
    m_cold.command_buffer.push(index  );
    m_cold.command_buffer.push(width  );
    m_cold.command_buffer.push(height );
    m_cold.command_buffer.push(address);
}

/* private static */ void ErfiGpu::do_gpu_tasks
    (std::queue<UInt32> & commands, VideoMemory & video_mem)
{
    using namespace gpu_enum_types;
    using Queue = std::queue<UInt32>;
    Queue queue_;
    auto pop = [](Queue & q) { auto rv = q.front(); q.pop(); return rv; };
    queue_.swap(commands);
    while (!queue_.empty()) {
        switch (pop(queue_)) {
        case UPLOAD:
        case UNLOAD:
        case DRAW  :
        case CLEAR : break;
        }
    }
    (void)video_mem;
    queue_.swap(commands);
}

} // end of erfin namespace
