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

void GpuCommandBuffer::upload_sprite
    (UInt32 index, UInt32 width, UInt32 height, UInt32 address)
{
    m_queue.push(gpu_enum_types::UPLOAD);
    m_queue.push(index  );
    m_queue.push(width  );
    m_queue.push(height );
    m_queue.push(address);
}

void GpuCommandBuffer::unload_sprite(UInt32 index) {
    m_queue.push(gpu_enum_types::UNLOAD);
    m_queue.push(index);
}

void GpuCommandBuffer::draw_sprite(UInt32 x, UInt32 y, UInt32 index) {
    m_queue.push(gpu_enum_types::DRAW);
    m_queue.push(x);
    m_queue.push(y);
    m_queue.push(index);
}

void GpuCommandBuffer::screen_clear() {
    m_queue.push(gpu_enum_types::CLEAR);
}

UInt32 GpuCommandBuffer::pop() {
    UInt32 top = m_queue.front();
    m_queue.pop();
    return top;
}

int ErfiGpu::do_gpu_tasks(GpuCommandBuffer & commands, VideoMemory & video_mem) {
    using namespace gpu_enum_types;
    while (!GpuAttorney::is_empty(commands)) {
        switch (GpuAttorney::pop(commands)) {
        case UPLOAD:
        case UNLOAD:
        case DRAW  :
        case CLEAR : break;
        }
    }
    (void)video_mem;
    return 0;
}

ErfiGpu::ErfiGpu():
    m_screen_width(SCREEN_WIDTH),
    m_index_pos(0)
{
    m_screen_pixels.resize(SCREEN_WIDTH*SCREEN_HEIGHT, false);

    m_hot.gpu_task = GpuUpdateTask(do_gpu_tasks);
}

void ErfiGpu::wait(MemorySpace &) {
    if (m_hot.signal.valid()) {
        if (m_gfx_thread.joinable())
            m_gfx_thread.join();
        (void)m_hot.signal.get();
        //m_hot.gpu_task.reset();
    }
    swap(m_hot.command_buffer);
    m_screen_pixels.swap(m_hot.working_pixels);
    m_hot.signal = m_hot.gpu_task.get_future();
    std::thread t1(std::move(m_hot.gpu_task),
                   std::ref(m_hot.command_buffer),
                   std::ref(m_hot.working_pixels));
    m_gfx_thread.swap(t1);
}

UInt32 ErfiGpu::upload_sprite(UInt32 width, UInt32 height, UInt32 address) {
    m_sprite_map[m_index_pos] = { int(width), int(height), std::vector<bool>() };
    GpuCommandBuffer::upload_sprite(m_index_pos, width, height, address);
    return m_index_pos++;
}

} // end of erfin namespace
