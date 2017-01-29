/****************************************************************************

    File: ErfiCpu.hpp
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

#ifndef MACRO_HEADER_GUARD_ERFINDUNG_CPU_HPP
#define MACRO_HEADER_GUARD_ERFINDUNG_CPU_HPP

#include "ErfiDefs.hpp"
#include "ErfiError.hpp"

#include <iosfwd>

namespace erfin {

class ErfiGpu;
class Debugger;

class ErfiCpu {
public:
    ErfiCpu();

    void reset();

    void run_cycle(MemorySpace & memspace, ErfiGpu * gpu = nullptr);

    void clear_flags() { m_wait_called = false; }

    void print_registers(std::ostream & out) const;
    void update_debugger(Debugger & dbgr) const;
    bool wait_was_called() const { return m_wait_called; }

    static void run_tests();

private:

    UInt32 & reg0(Inst inst);
    UInt32 & reg1(Inst inst);
    UInt32 & reg2(Inst inst);
    UInt32 & reg3(Inst inst);

    void do_rotate(Inst inst);

    void do_syscall(Inst inst, ErfiGpu & gpu);
    void do_basic_arth_inst(Inst inst, UInt32(*func)(UInt32, UInt32));

    ErfiError emit_error(Inst i) const noexcept {
        //               PC increment while the instruction is executing
        //               so it will be one too great if an illegal instruction
        //               is encountered
        return ErfiError(m_registers[enum_types::REG_PC] - 1,
                         std::move(disassemble_instruction(i)));
    }

    std::string disassemble_instruction(Inst i) const;

    RegisterPack m_registers;
    bool m_wait_called;
};

}

#endif
