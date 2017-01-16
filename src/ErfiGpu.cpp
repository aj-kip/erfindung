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

namespace {

using Error = std::runtime_error;

template <typename Key, typename Value>
Value & query(std::map<Key, Value> & map, const Key & k);

template <typename T>
T front_and_pop(std::queue<T> & queue);

void upload_sprite(erfin::GpuContext & ctx, const erfin::UInt32 * memory);
void unload_sprite(erfin::GpuContext & ctx);
void draw_sprite  (erfin::GpuContext & ctx);
void clear_screen (erfin::GpuContext & ctx);

void push_bits_to (std::vector<bool> & v, erfin::UInt32 bits);

} // end of <anonymous> namespace

namespace erfin {

struct GpuContext {
    using SpriteMeta  = ErfiGpu::SpriteMeta ;
    using VideoMemory = ErfiGpu::VideoMemory;
    std::queue<UInt32>       command_buffer;
    std::queue<SpriteMeta *> sprite_data   ;
    VideoMemory              pixels        ;
};

/* static */ const int ErfiGpu::SCREEN_WIDTH  = 320;
/* static */ const int ErfiGpu::SCREEN_HEIGHT = 240;

ErfiGpu::ErfiGpu():
    m_screen_width(SCREEN_WIDTH),
    m_index_pos(0),
    m_cold(new GpuContext()),
    m_hot(new GpuContext())
{
    m_cold->pixels.resize(SCREEN_WIDTH*SCREEN_HEIGHT, false);
}

ErfiGpu::~ErfiGpu() {
    if (m_gfx_thread.joinable())
        m_gfx_thread.join();
}

void ErfiGpu::wait(MemorySpace & memory) {
    if (m_gfx_thread.joinable())
        m_gfx_thread.join();

    for (auto itr = m_sprite_map.begin(); itr != m_sprite_map.end(); ++itr) {
        if (itr->second.delete_flag) {
            itr = m_sprite_map.erase(itr);
            continue;
        }
    }

    m_cold.swap(m_hot);

    std::thread t1(do_gpu_tasks, std::ref(*m_hot), &memory[0]);
    m_gfx_thread.swap(t1);
}

UInt32 ErfiGpu::upload_sprite(UInt32 width, UInt32 height, UInt32 address) {
    auto & sprite = m_sprite_map[m_index_pos] = SpriteMeta { width, height };
    m_cold->sprite_data.push(&sprite);
    upload_sprite(m_index_pos, width, height, address);
    return m_index_pos++;
}

void ErfiGpu::unload_sprite(UInt32 index) {
    m_cold->command_buffer.push(gpu_enum_types::UNLOAD);
    m_cold->sprite_data.push(&query(m_sprite_map, index));
}

void ErfiGpu::draw_sprite(UInt32 x, UInt32 y, UInt32 index) {
    m_cold->command_buffer.push(gpu_enum_types::DRAW);
    m_cold->command_buffer.push(x);
    m_cold->command_buffer.push(y);
    m_cold->sprite_data.push(&query(m_sprite_map, index));
}

void ErfiGpu::screen_clear() {
    m_cold->command_buffer.push(gpu_enum_types::CLEAR);
}

/* private */ void ErfiGpu::upload_sprite
    (UInt32 index, UInt32 width, UInt32 height, UInt32 address)
{
    m_cold->command_buffer.push(gpu_enum_types::UPLOAD);
    //m_cold->command_buffer.push(index  );
    (void)index;
    m_cold->command_buffer.push(width  );
    m_cold->command_buffer.push(height );
    m_cold->command_buffer.push(address);
}

/* private static */ void ErfiGpu::do_gpu_tasks
    (GpuContext & context, const UInt32 * memory)
{
    using namespace gpu_enum_types;

    while (!context.command_buffer.empty()) {
        switch (front_and_pop(context.command_buffer)) {
        case UPLOAD: ::upload_sprite(context, memory); break;
        case UNLOAD: ::unload_sprite(context); break;
        case DRAW  : ::draw_sprite  (context); break;
        case CLEAR : ::clear_screen (context); break;
        }
    }
}

} // end of erfin namespace

namespace {

std::size_t coord_to_index(erfin::UInt32 x, erfin::UInt32 y);
void push_bits_to(std::vector<bool> & v, erfin::UInt8 bits);

template <typename Key, typename Value>
Value & query(std::map<Key, Value> & map, const Key & k) {
    auto itr = map.find(k);
    if (itr == map.end())
        throw Error("Failed to find sprite, index not found.");
    return itr->second;
}

template <typename T>
T front_and_pop(std::queue<T> & queue) {
    T rv = queue.front();
    queue.pop();
    return rv;
}

void upload_sprite(erfin::GpuContext & ctx, const erfin::UInt32 * memory) {
    // possible mucking about on another thread's work :/
    using namespace erfin;
    using SpriteMeta = GpuContext::SpriteMeta;
    auto & queue = ctx.command_buffer;
    UInt32 width = front_and_pop(queue), height = front_and_pop(queue),
           address = front_and_pop(queue);
    SpriteMeta * sprite = front_and_pop(ctx.sprite_data);
    const UInt32 * start = &memory[address];
    const UInt32 * end   = start + width*height;
    sprite->width  = width;
    sprite->height = height;
    for (; start != end; ++start) {
        push_bits_to(sprite->pixels, *start);
    }
}

void unload_sprite(erfin::GpuContext & ctx) {
    auto sprite = front_and_pop(ctx.sprite_data);
    sprite->delete_flag = false;
}

void draw_sprite(erfin::GpuContext & ctx) {
    using namespace erfin;
    using SpriteMeta = GpuContext::SpriteMeta;

    SpriteMeta * sprite = front_and_pop(ctx.sprite_data);
    UInt32 x_ = front_and_pop(ctx.command_buffer),
           y_ = front_and_pop(ctx.command_buffer);
    UInt32 i = 0;
    for (UInt32 y = y_; y != sprite->height; ++y) {
    for (UInt32 x = x_; x != sprite->width ; ++x) {
        ctx.pixels[coord_to_index(x, y)] = sprite->pixels[++i];
    }}
}

void clear_screen(erfin::GpuContext & ctx) {
    using namespace erfin;
    for (UInt32 y = 0; y != UInt32(ErfiGpu::SCREEN_HEIGHT); ++y) {
    for (UInt32 x = 0; x != UInt32(ErfiGpu::SCREEN_WIDTH ); ++x) {
        ctx.pixels[coord_to_index(x, y)] = false;
    }}
}

void push_bits_to(std::vector<bool> & v, erfin::UInt32 bits) {
    using namespace erfin;
    push_bits_to(v, UInt8((bits & 0xFF000000) >> 24));
    push_bits_to(v, UInt8((bits & 0x00FF0000) >> 16));
    push_bits_to(v, UInt8((bits & 0x0000FF00) >>  8));
    push_bits_to(v, UInt8((bits & 0x000000FF)      ));
}

std::size_t coord_to_index(erfin::UInt32 x, erfin::UInt32 y) {
    return std::size_t(x + y*erfin::ErfiGpu::SCREEN_WIDTH);
}

void push_bits_to(std::vector<bool> & v, erfin::UInt8 bits) {
    v.push_back(bits & 0x80);
    v.push_back(bits & 0x40);
    v.push_back(bits & 0x20);
    v.push_back(bits & 0x10);
    v.push_back(bits & 0x08);
    v.push_back(bits & 0x04);
    v.push_back(bits & 0x02);
    v.push_back(bits & 0x01);
}

} // end of <anonymous> namespace
