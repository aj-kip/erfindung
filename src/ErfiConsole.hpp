/****************************************************************************

    File: ErfiConsole.hpp
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

#ifndef MACRO_HEADER_GUARD_ERFINDUNG_CONSOLE_HPP
#define MACRO_HEADER_GUARD_ERFINDUNG_CONSOLE_HPP

#include "ErfiDefs.hpp"

#include "ErfiApu.hpp"
#include "ErfiCpu.hpp"
#include "ErfiGpu.hpp"
#include "ErfiGamePad.hpp"

#include <chrono>
#include <utility>
#include <iosfwd>

namespace sf { class Event; }

namespace erfin {

class UtilityDevices {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    UtilityDevices();

    // read actions
    UInt32 generate_random_number(); // modifies rng state
    UInt32 query_elapsed_time    () const;

    // write actions
    void power(UInt32);
    void wait (UInt32);

    // interface for program
    bool wait_requested() const;

    bool halt_requested() const;

    bool no_stop_signal() const { return m_no_stop; }

    void set_wait_time();

    void set_bus_error(bool v) { m_bus_error = v; }

    bool bus_error_present() const { return m_bus_error; }

private:
    void update_no_stop_signal();

    bool m_no_stop;
    bool m_wait ;
    bool m_halt_flag;
    bool m_bus_error;
    std::mt19937 m_rng;
    std::uniform_int_distribution<UInt32> m_distro;
    TimePoint m_prev_time;
    UInt32 m_wait_time;
};

struct ConsolePack {
    ConsolePack();
    MemorySpace    * ram;
    ErfiCpu        * cpu;
    ErfiGpu        * gpu;
    Apu            * apu;
    GamePad        * pad;
    UtilityDevices * dev;
};

void do_write(ConsolePack &, UInt32 address, UInt32 data);
UInt32 do_read(ConsolePack &, UInt32 address);
bool address_is_valid(const ConsolePack &, UInt32 address);

/** @brief User level view of the entire "Erfindung console"
 *  The console has all the machine components built into it.
 */
class Console {
public:
    using VideoMemory = ErfiGpu::VideoMemory;

    Console();

    void load_program(const ProgramData & program);

    void process_event(const sf::Event & event);

    void press_restart();

    bool trying_to_shutdown() const;

    template <typename Func>
    void run_until_wait_with_post_frame(Func && f);

    void run_until_wait() { run_until_wait_with_post_frame([](){}); }

    void update_with_current_state(Debugger &) const;

    void force_wait_state();

    const VideoMemory & current_screen() const;

    static void load_program_to_memory
        (const ProgramData & program, MemorySpace & memspace);

private:
    ConsolePack pack;

    MemorySpace    m_ram;
    ErfiCpu        m_cpu;
    ErfiGpu        m_gpu;
    Apu            m_apu;
    GamePad        m_pad;
    UtilityDevices m_dev;
};

template <typename Func>
void Console::run_until_wait_with_post_frame(Func && f) {
    pack.gpu->wait(*pack.ram);
    pack.apu->update();
    pack.dev->set_wait_time();
    while (pack.dev->no_stop_signal()) {
        pack.cpu->run_cycle(pack);
        f();
    }
}

} // end of erfin namespace

#endif
