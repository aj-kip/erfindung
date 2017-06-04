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
#include <random>

namespace erfin {

class Debugger;

class ErfiCpu {
public:
    ErfiCpu();

    void reset();

    void do_nothing(MemorySpace &, ConsolePack *){}
    void run_cycle(ConsolePack & console);
    void run_cycle(Inst inst, ConsolePack & console);

    void print_registers(std::ostream & out) const;
    void update_debugger(Debugger & dbgr) const;

    static void run_tests();

private:

    UInt32 & reg0(Inst inst);
    UInt32 & reg1(Inst inst);
    UInt32 & reg2(Inst inst);

    template <UInt32(*FuncFp)(UInt32, UInt32), UInt32(*FuncInt)(UInt32, UInt32)>
    void do_arth(Inst inst);
    void do_set(Inst inst);

    void do_skip(Inst inst);
    void do_call(Inst inst, ConsolePack & pack);

    void do_not(Inst inst);

    UInt32 get_move_op_address(Inst inst);

    [[noreturn]] void throw_error(Inst i) const;

    std::string disassemble_instruction(Inst i) const;

    RegisterPack m_registers;
};

template <UInt32(*FuncFp)(UInt32, UInt32), UInt32(*FuncInt)(UInt32, UInt32)>
/* private */ void ErfiCpu::do_arth(Inst inst) {
    using Pf = RTypeParamForm;
    UInt32 & r0 = reg0(inst), & r1 = reg1(inst);
    switch (decode_r_type_pf(inst)) {
    case Pf::_3R_INT     : r0 = FuncInt(r1, reg2(inst)              ); return;
    case Pf::_2R_IMMD_INT: r0 = FuncInt(r1, decode_immd_as_int(inst)); return;
    case Pf::_3R_FP      : r0 = FuncFp (r1, reg2(inst)              ); return;
    case Pf::_2R_IMMD_FP : r0 = FuncFp (r1, decode_immd_as_fp (inst)); return;
    }
}

} // end of erfin namespace

#endif
