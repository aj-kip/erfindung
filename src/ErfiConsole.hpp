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

#include "ErfiCpu.hpp"
#include "ErfiGpu.hpp"
#include "ErfiGamePad.hpp"

#include <utility>
#include <iosfwd>

namespace sf {
    class Event;
}

namespace erfin {

struct ConsolePack {
    MemorySpace ram;
    ErfiCpu     cpu;
    ErfiGpu     gpu;
    GamePad     pad;
};

class Console {
public:
    static void load_program_to_memory
        (const ProgramData & program, MemorySpace & memspace);

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
private:
    ConsolePack pack;
};

template <typename Func>
void Console::run_until_wait(Func && f) {
    pack.gpu.wait(pack.ram);
    pack.cpu.clear_flags();
    while (!pack.cpu.wait_was_called()) {
        pack.cpu.run_cycle(&pack);
        f();
    }
}

template <typename Func>
void Console::draw_pixels(Func && f) {
    pack.gpu.draw_pixels(std::move(f));
}

} // end of erfin namespace

#endif
