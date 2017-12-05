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
#include "Debugger.hpp"
#ifndef MACRO_BUILD_STL_ONLY
#   include <SFML/Window/Event.hpp>
#endif
#include <cassert>

namespace {

using Error = std::runtime_error;

#ifndef MACRO_BUILD_STL_ONLY
erfin::GamePad::Button to_button(sf::Keyboard::Key k);
#endif

erfin::UInt32 do_device_read(erfin::ConsolePack &, erfin::UInt32 address);

void do_device_write(erfin::ConsolePack &, erfin::UInt32 address, erfin::UInt32 data);

constexpr const char * ACCESS_VIOLATION_MESSAGE =
    "Memory access violation (address is too high).";

} // end of <anonymous> namespace

namespace erfin {

UtilityDevices::UtilityDevices():
    m_no_stop(true),
    m_wait(false),
    m_halt_flag(false),
    m_bus_error(false),
    m_rng(std::random_device()()),
    m_prev_time(std::chrono::steady_clock::now())
{}

// read actions
UInt32 UtilityDevices::generate_random_number()
    { return m_distro(m_rng); }

UInt32 UtilityDevices::query_elapsed_time() const { return m_wait_time; }

void UtilityDevices::power(UInt32 p) {
    m_halt_flag = (p != 0);
    update_no_stop_signal();
}

void UtilityDevices::wait (UInt32 w) {
    m_wait = (w != 0);
    update_no_stop_signal();
}

bool UtilityDevices::wait_requested() const { return m_wait; }

bool UtilityDevices::halt_requested() const { return m_halt_flag; }

void UtilityDevices::set_wait_time() {
    using namespace std::chrono;
    auto duration = duration_cast<milliseconds>(steady_clock::now() - m_prev_time);
    m_prev_time = steady_clock::now();
    double et = double(duration.count()) / 1000.0;
    m_wait      = false;
    m_wait_time = to_fixed_point(et);
}

/* private */ void UtilityDevices::update_no_stop_signal()
    { m_no_stop = !m_halt_flag and !m_wait; }

ConsolePack::ConsolePack():
    ram(nullptr),
    cpu(nullptr),
    gpu(nullptr),
    apu(nullptr),
    pad(nullptr),
    dev(nullptr)
{}

void do_write(ConsolePack & con, UInt32 address, UInt32 data) {
    if (address & device_addresses::DEVICE_ADDRESS_MASK) {
        do_device_write(con, address, data);
    } else if (address < con.ram->size()) {
        (*con.ram)[address] = data;
    } else {
        throw Error(ACCESS_VIOLATION_MESSAGE);
    }
}

UInt32 do_read(ConsolePack & con, UInt32 address) {
    if (address & device_addresses::DEVICE_ADDRESS_MASK) {
        return do_device_read(con, address);
    } else if (address < con.ram->size()) {
        return (*con.ram)[address];
    } else {
        throw Error(ACCESS_VIOLATION_MESSAGE);
    }
}

bool address_is_valid(const ConsolePack & con, UInt32 address) {
    using namespace device_addresses;
    if (address & device_addresses::DEVICE_ADDRESS_MASK) {
        return to_string(address) != INVALID_DEVICE_ADDRESS;
    } else {
        assert(con.ram);
        return address < con.ram->size();
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
#   ifndef MACRO_BUILD_STL_ONLY
    switch (event.type) {
    case sf::Event::KeyPressed :
        pack.pad->update(to_button(event.key.code), GamePad::PRESSED);
        break;
    case sf::Event::KeyReleased:
        pack.pad->update(to_button(event.key.code), GamePad::RELEASE);
        break;
    default: break;
    }
#   else
    (void)event;
#   endif
}

void Console::press_restart() {
    pack.cpu->reset();
}

bool Console::trying_to_shutdown() const {
    return pack.dev->halt_requested();
}

void Console::update_with_current_state(Debugger & debugger) const {
    pack.cpu->update_debugger(debugger);
}

void Console::force_wait_state() {
    pack.dev->wait(~0u);
}

const Console::VideoMemory & Console::current_screen() const {
    return pack.gpu->current_screen();
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
#ifndef MACRO_BUILD_STL_ONLY
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
#endif
erfin::UInt32 do_device_read(erfin::ConsolePack & con, erfin::UInt32 address) {
    using namespace erfin;
    using namespace device_addresses;
    con.dev->set_bus_error(false);
    auto bus_error = [&con] () -> UInt32
        { con.dev->set_bus_error(true); return 0; };

    switch (address) {
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

    switch (address) {
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
