/****************************************************************************

    File: ErfiConsole.cpp
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

#include "ErfiConsole.hpp"

#include <SFML/Window/Event.hpp>

namespace {

erfin::GamePad::Button to_button(sf::Keyboard::Key k);

} // end of <anonymous> namespace

namespace erfin {

/* static */ void Console::load_program_to_memory
    (const ProgramData & program, MemorySpace & memspace)
{
    if (memspace.size() < program.size()) {
        throw std::runtime_error("Program is too large for RAM!");
    }
    UInt32 * beg = &memspace.front();
    for (Inst inst : program)
        *beg++ = serialize(inst);
}

void Console::load_program(const ProgramData & program) {
    load_program_to_memory(program, pack.ram);
}

void Console::process_event(const sf::Event & event) {

    switch (event.type) {
    case sf::Event::KeyPressed :
        pack.pad.update(to_button(event.key.code), GamePad::PRESSED);
        break;
    case sf::Event::KeyReleased:
        pack.pad.update(to_button(event.key.code), GamePad::RELEASE);
        break;
    default: break;
    }
}

void Console::press_restart() {
    pack.cpu.reset();
}

bool Console::trying_to_shutdown() const {
    return pack.cpu.requesting_halt();
}

void Console::print_cpu_registers(std::ostream & out) const {
    pack.cpu.print_registers(out);
}

} // end of erfin namespace

namespace {

erfin::GamePad::Button to_button(sf::Keyboard::Key k) {
    using Key = sf::Keyboard::Key;
    using Gp  = erfin::GamePad;
    switch (k) {
    case Key::Down  : return Gp::DOWN ;
    case Key::Up    : return Gp::UP   ;
    case Key::Left  : return Gp::LEFT ;
    case Key::Right : return Gp::RIGHT;
    case Key::A     : return Gp::A    ;
    case Key::S     : return Gp::B    ;
    case Key::Return: return Gp::START;
    default: return Gp::BUTTON_COUNT;
    }
}

} // end of <anonymous> namespace
