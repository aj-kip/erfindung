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
#include "FixedPointUtil.hpp"

#include <SFML/Window/Event.hpp>

namespace {

using Error = std::runtime_error;

erfin::GamePad::Button to_button(sf::Keyboard::Key k);

erfin::UInt32 do_device_read(erfin::ConsolePack &, erfin::UInt32 address);
void do_device_write(erfin::ConsolePack &, erfin::UInt32 address, erfin::UInt32 data);

} // end of <anonymous> namespace

namespace erfin {

UtilityDevices::UtilityDevices():
    m_rng(std::random_device()()),
    m_wait(false),
    m_power(true),
    m_bus_error(false)
{}

// read actions
UInt32 UtilityDevices::generate_random_number()
    { return std::uniform_int_distribution<UInt32>()(m_rng); }

UInt32 UtilityDevices::query_elapsed_time() const { return m_wait_time; }

void UtilityDevices::power(UInt32 p) { m_power = (p != 0); }

void UtilityDevices::wait (UInt32 w) { m_wait = (w != 0); }

bool UtilityDevices::wait_requested() const { return m_wait; }

bool UtilityDevices::halt_requested() const { return !m_power; }

void UtilityDevices::set_wait_time() {
    double et = double(m_clock.getElapsedTime().asSeconds());
    m_clock.restart();

    m_wait      = false;
    m_wait_time = to_fixed_point(et);
}

ConsolePack::ConsolePack():
    ram(nullptr),
    cpu(nullptr),
    gpu(nullptr),
    apu(nullptr),
    pad(nullptr),
    dev(nullptr)
{}

void do_write(ConsolePack & con, UInt32 address, UInt32 data) {
    if (int(address) & device_addresses::DEVICE_ADDRESS_MASK) {
        do_device_write(con, address, data);
    } else {
        (*con.ram)[address] = data;
    }
}

UInt32 do_read(ConsolePack & con, UInt32 address) {
    if (int(address) & device_addresses::DEVICE_ADDRESS_MASK) {
        return do_device_read(con, address);
    } else {
        return (*con.ram)[address];
    }
}

Console::Console() {
    pack.apu = &m_apu;
    pack.ram = &m_ram;
    pack.cpu = &m_cpu;
    pack.gpu = &m_gpu;
    pack.apu = &m_apu;
    pack.pad = &m_pad;
    pack.dev = &m_dev;
}

void Console::load_program(const ProgramData & program) {
    load_program_to_memory(program, *pack.ram);
}

void Console::process_event(const sf::Event & event) {
    switch (event.type) {
    case sf::Event::KeyPressed :
        pack.pad->update(to_button(event.key.code), GamePad::PRESSED);
        break;
    case sf::Event::KeyReleased:
        pack.pad->update(to_button(event.key.code), GamePad::RELEASE);
        break;
    default: break;
    }
}

void Console::press_restart() {
    pack.cpu->reset();
}

bool Console::trying_to_shutdown() const {
    return pack.dev->halt_requested();
}

void Console::print_cpu_registers(std::ostream & out) const {
    pack.cpu->print_registers(out);
}

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

erfin::UInt32 do_device_read(erfin::ConsolePack & con, erfin::UInt32 address) {
    using namespace erfin;
    using namespace device_addresses;
    con.dev->set_bus_error(false);
    auto bus_error = [&con] () -> UInt32
        { con.dev->set_bus_error(true); return 0; };

    switch (int(address)) {
    case RESERVED_NULL          :
    case GPU_INPUT_STREAM       : return bus_error();
    case GPU_RESPONSE           : return con.gpu->read();
    case APU_INPUT_STREAM       :
    case TIMER_WAIT_AND_SYNC    : return bus_error();
    case TIMER_QUERY_SYNC_ET    : return con.dev->query_elapsed_time();
    case RANDOM_NUMBER_GENERATOR: return con.dev->generate_random_number();
    case READ_CONTROLLER        : return con.pad->decode();
    case HALT_SIGNAL            :
    case BUS_ERROR              :
    default: return bus_error();
    }
}

void do_device_write
    (erfin::ConsolePack & con, erfin::UInt32 address, erfin::UInt32 data)
{
    using namespace erfin;
    using namespace device_addresses;

    con.dev->set_bus_error(false);
    auto bus_error = [&con] () { con.dev->set_bus_error(true); };

    switch (int(address)) {
    case RESERVED_NULL          : bus_error(); return;
    case GPU_INPUT_STREAM       : con.gpu->io_write(data); return;
    case GPU_RESPONSE           : bus_error(); return;
    case APU_INPUT_STREAM       : con.apu->io_write(data); return;
    case TIMER_WAIT_AND_SYNC    : if (data) con.dev->wait(data); return;
    case TIMER_QUERY_SYNC_ET    :
    case RANDOM_NUMBER_GENERATOR:
    case READ_CONTROLLER        : bus_error(); return;
    case HALT_SIGNAL            : con.dev->power(data); return;
    case BUS_ERROR              :
    default: bus_error(); return;
    }
}

} // end of <anonymous> namespace
