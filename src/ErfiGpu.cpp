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

#include <iostream>

#include <cassert>

namespace {

using Error = std::runtime_error;
using UInt32 = erfin::UInt32;

static constexpr const UInt32 SIZE_BITS_MASK = 0x7 << 10;

template <typename Key, typename Value>
Value & query(std::map<Key, Value> & map, const Key & k);

template <typename T>
T front_and_pop(std::queue<T> & queue);

void upload_sprite(erfin::GpuContext & ctx, const erfin::UInt32 * memory);

void draw_sprite  (erfin::GpuContext & ctx);
void clear_screen (erfin::GpuContext & ctx);

bool queue_has_enough_for_top_instruction(const erfin::GpuContext *);

UInt32 compute_size_of_sprite(UInt32 index);
std::size_t convert_index_to_offset(UInt32 index);

} // end of <anonymous> namespace

namespace erfin {

struct GpuContext {
    using VideoMemory = ErfiGpu::VideoMemory;

    std::queue<UInt32> command_buffer;
    VideoMemory        pixels        ;
    std::vector<bool>  sprite_memory ;
};

/* static */ const int ErfiGpu::SCREEN_WIDTH  = 320;
/* static */ const int ErfiGpu::SCREEN_HEIGHT = 240;

ErfiGpu::ErfiGpu():
    m_index_pos(0),
    m_cold(new GpuContext()),
    m_hot (new GpuContext())
{
    m_cold->pixels.resize(SCREEN_WIDTH*SCREEN_HEIGHT, false);
    m_hot ->pixels.resize(SCREEN_WIDTH*SCREEN_HEIGHT, false);
    m_hot ->sprite_memory.resize(128*128*4, false);
}

ErfiGpu::~ErfiGpu() {
    {
    std::unique_lock<std::mutex> lock(m_thread_control.mtx);
    auto & hot = m_hot;
    m_thread_control.gpu_ready.wait(lock, [&hot]()
        { return hot->command_buffer.empty(); });
    m_thread_control.finish = true;
    m_thread_control.buffer_ready.notify_one();
    }
    if (m_gfx_thread.joinable())
        m_gfx_thread.join();
}

void ErfiGpu::wait(MemorySpace & memory) {
#   if 0
    if (m_gfx_thread.joinable()) {
        m_gpus_lock.lock();
        m_thread_control.command_buffer_swaped = false;

        bool & gpu_ready = m_thread_control.gpu_thread_ready;
        m_thread_control.gpu_ready.wait(m_gpus_lock, [&gpu_ready]()
            { return gpu_ready; });

        m_cold.swap(m_hot);
        // make sure sprite memory stays with hot
        m_cold->sprite_memory.swap(m_hot->sprite_memory);
        assert(!m_hot->sprite_memory.empty());
        m_thread_control.command_buffer_swaped = true;

        m_gpus_lock.unlock();
        m_thread_control.buffer_ready.notify_one();
    } else {
        std::unique_lock<std::mutex> lock(m_thread_control.mtx);
        m_gpus_lock.swap(lock);
        m_gpus_lock.unlock();

        std::thread t1(do_gpu_tasks, std::ref(m_hot), &memory[0], std::ref(m_thread_control));
        m_gfx_thread.swap(t1);
    }
#   endif
    m_cold.swap(m_hot);
    // make sure sprite memory stays with hot
    m_cold->sprite_memory.swap(m_hot->sprite_memory);
    do_gpu_tasks(m_hot, &memory[0], m_thread_control);
}

void ErfiGpu::upload_sprite
    (UInt32 address, UInt32 width, UInt32 height, UInt32 index)
{
    if (!is_valid_sprite_index(index)) {
        throw Error("Sprite index is invalid (improperly encoded).");
    }
    using namespace gpu_enum_types;
    auto push_instr = [this] (UInt32 i) { m_cold->command_buffer.push(i); };
    push_instr(UPLOAD );
    push_instr(address);
    push_instr(width  );
    push_instr(height );
    push_instr(index  );
}

void ErfiGpu::draw_sprite(UInt32 x, UInt32 y, UInt32 index) {
    m_cold->command_buffer.push(gpu_enum_types::DRAW);
    m_cold->command_buffer.push(x);
    m_cold->command_buffer.push(y);
    m_cold->command_buffer.push(index);
}

void ErfiGpu::screen_clear() {
    m_cold->command_buffer.push(gpu_enum_types::CLEAR);
}

void ErfiGpu::io_write(UInt32 data) {
    m_cold->command_buffer.push(data);
}

UInt32 ErfiGpu::read() const {
    // mmm...
    return 0;
}

/* static */ bool ErfiGpu::is_valid_sprite_index(UInt32 idx) {

    switch (SIZE_BITS_MASK & idx) {
    case 0 << 10: case 1 << 10: case 2 << 10: case 3 << 10: case 4 << 10:
        break;
    default: return false;
    }
    auto active_quadrant_indicies = (SIZE_BITS_MASK & idx) >> 10;
    UInt32 temp = idx;
    for (UInt32 i = 0; i != 5; ++i) {
        if (i > active_quadrant_indicies) {
            if ((temp & 0x3) != 0) return false;
        }
        temp >>= 2;
    }
    return (idx >> 13) == 0;
}

/* private */ std::size_t ErfiGpu::next_set_pixel(std::size_t i) {
    while (i != m_cold->pixels.size()) {
        if (m_cold->pixels[i++])
            return i;
    }
    return std::size_t(-1);
}

/* private static */ void ErfiGpu::do_gpu_tasks
    (std::unique_ptr<GpuContext> & context, const UInt32 * memory, ThreadControl & tc)
{
    (void)tc;
    using namespace gpu_enum_types;

    while (true) {
#       if 0
        std::unique_lock<std::mutex> lock(tc.mtx);
        tc.gpu_thread_ready = true;
        tc.gpu_ready.notify_one();
        tc.buffer_ready.wait(lock, [&tc]()
            { return tc.command_buffer_swaped || tc.finish; });
        tc.gpu_thread_ready = false;
#       endif
        while (!context->command_buffer.empty()) {
            // stop instruction process if there isn't enough for the next
            // instruction
            if (!queue_has_enough_for_top_instruction(context.get())) break;

            switch (front_and_pop(context->command_buffer)) {
            case UPLOAD: ::upload_sprite(*context, memory); break;
            case DRAW  : ::draw_sprite  (*context); break;
            case CLEAR : ::clear_screen (*context); break;
            }
        }
#       if 0
        if (tc.finish) return;
#       endif
        return;
    }
}

} // end of erfin namespace

namespace {

std::size_t coord_to_index(erfin::UInt32 x, erfin::UInt32 y);

template <typename Key, typename Value>
Value & query(std::map<Key, Value> & map, const Key & k) {
    auto itr = map.find(k);
    if (itr == map.end())
        throw Error("Failed to find sprite, index not found.");
    return itr->second;
}

template <typename T>
T front_and_pop(std::queue<T> & queue) {
    assert(!queue.empty());
    T rv = queue.front();
    queue.pop();
    return rv;
}

void upload_sprite(erfin::GpuContext & ctx, const erfin::UInt32 * memory) {
    using namespace erfin;
    auto & queue = ctx.command_buffer;

    UInt32 width   = front_and_pop(queue);
    UInt32 height  = front_and_pop(queue);
    UInt32 address = front_and_pop(queue);
    UInt32 index   = front_and_pop(queue);

    UInt32 dest_size = compute_size_of_sprite(index);

    if (width > dest_size || height > dest_size) {
        throw Error("Width and/or height exceed sprite cell size.");
    }

    const UInt32 * start = &memory[address];
    const UInt32 * end   = start + (width*height)/32 +
                           ( (width*height % 32) ? 1 : 0 );
    (void)end; // needed in debug mode -> make sure we don't overrun the buffer
    // lets just one bit at a time, we ALWAYS have time to micro-optimize
    // later
    auto dest_offset = convert_index_to_offset(index);
    std::bitset<32> line32 = *start;
    UInt32 bit_pos = 0;
    for (UInt32 y = 0; y != height; ++y) {
        for (UInt32 x = 0; x != width ; ++x) {
            ctx.sprite_memory[dest_offset + x] = line32[31 - (bit_pos % 32)];
            ++bit_pos;
            if (bit_pos % 32 == 0) {
                line32 = *++start;
                assert(start != end || bit_pos == width*height);
            }
        }
        dest_offset += dest_size;
    }
}

template <typename BoolRef>
void xor_set(BoolRef dest, BoolRef arg) {
    dest = dest ^ arg;
}

void draw_sprite(erfin::GpuContext & ctx) {
    using namespace erfin;

    UInt32 x_         = front_and_pop(ctx.command_buffer);
    UInt32 y_         = front_and_pop(ctx.command_buffer);
    UInt32 text_index = front_and_pop(ctx.command_buffer);

    const UInt32 SCREEN_WIDTH  = UInt32(ErfiGpu::SCREEN_WIDTH );
    const UInt32 SCREEN_HEIGHT = UInt32(ErfiGpu::SCREEN_HEIGHT);

    auto text_size   = compute_size_of_sprite (text_index);
    auto text_offset = convert_index_to_offset(text_index);

    // grab the bits and render (using xor)
    for (UInt32 y = y_; (y - y_) != text_size && y < SCREEN_HEIGHT; ++y) {
    for (UInt32 x = x_; (x - x_) != text_size && x < SCREEN_WIDTH ; ++x) {
        //                                                       v as width
        std::size_t from_index = std::size_t((x - x_) + (y - y_)*text_size);
        assert(coord_to_index(x, y) < ctx.pixels.size());
        assert(text_offset + from_index < ctx.sprite_memory.size());
        xor_set(ctx.pixels       [coord_to_index(x, y)    ],
                ctx.sprite_memory[text_offset + from_index]);
    }}
}

void clear_screen(erfin::GpuContext & ctx) {
    std::fill(ctx.pixels.begin(), ctx.pixels.end(), false);
}

bool queue_has_enough_for_top_instruction(const erfin::GpuContext * context) {
    using namespace erfin;
    auto code = static_cast<GpuOpCode>(context->command_buffer.front());
    int left_after_code = int(context->command_buffer.size() - 1);
    return left_after_code >= parameters_per_instruction(code);
}

UInt32 compute_size_of_sprite(UInt32 index) {
    // size mask, number of bits used to encode which "sprite" to use
    UInt32 bits_used = (SIZE_BITS_MASK & index) >> 10;
    if (bits_used > 4)
        throw Error("Invalid sprite index: invalid size bits...");
    return 128 >> bits_used;
}

std::size_t convert_index_to_offset(UInt32 index) {
    std::size_t rv = 0;
    std::size_t current_quad_size = 128*128; // 14
    for (UInt32 i = ((index >> 10) & 0x7) + 1; i != 0; --i) {
        // position in increasing level of specifity
        rv += current_quad_size*(index & 0x3);

        current_quad_size /= 4; // -2
        index             >>= 2;
    }
    return rv;
}

std::size_t coord_to_index(erfin::UInt32 x, erfin::UInt32 y) {
    return std::size_t(x + y*erfin::ErfiGpu::SCREEN_WIDTH);
}

} // end of <anonymous> namespace
