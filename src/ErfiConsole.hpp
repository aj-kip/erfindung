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

#include <SFML/System/Clock.hpp>

#include <utility>
#include <iosfwd>

namespace sf { class Event; }

namespace erfin {

class UtilityDevices {
public:
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
    void set_wait_time();

    void set_bus_error(bool v) { m_bus_error = v; }
    bool bus_error_present() const { return m_bus_error; }
private:
    std::default_random_engine m_rng;
    sf::Clock m_clock;
    bool m_wait ;
    bool m_power;
    UInt32 m_wait_time;
    bool m_bus_error;
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

/** @brief User level view of the entire "Erfindung console"
 *  The console has all the machine components built into it.
 */
class Console {
public:
    Console();

    void load_program(const ProgramData & program);

    void process_event(const sf::Event & event);

    void press_restart();

    bool trying_to_shutdown() const;

    void print_cpu_registers(std::ostream & out) const;

    template <typename Func>
    void run_until_wait(Func && f);

    void run_until_wait() { run_until_wait([](){}); }

    template <typename Func>
    void draw_pixels(Func && f);

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
void Console::run_until_wait(Func && f) {
    pack.gpu->wait(*pack.ram);
    pack.apu->update();
    pack.dev->set_wait_time();
    while (!pack.dev->wait_requested() && !pack.dev->halt_requested()) {
        pack.cpu->run_cycle(pack);
        f();
    }
}

template <typename Func>
void Console::draw_pixels(Func && f) {
    pack.gpu->draw_pixels(std::move(f));
}

} // end of erfin namespace

#endif
