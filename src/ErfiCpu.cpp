/****************************************************************************

    File: ErfiCpu.cpp
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

#include "ErfiCpu.hpp"
#include "Assembler.hpp"
#include "FixedPointUtil.hpp"

#include <iostream>

namespace {

using Error = std::runtime_error;
using UInt32 = erfin::UInt32;
Error make_illegal_inst_error(erfin::Inst inst);
UInt32 plus (UInt32 x, UInt32 y) { return x + y; }
UInt32 minus(UInt32 x, UInt32 y) { return x - y; }
UInt32 times(UInt32 x, UInt32 y) { return x * y; }
UInt32 andi (UInt32 x, UInt32 y) { return x & y; }
UInt32 ori  (UInt32 x, UInt32 y) { return x | y; }
UInt32 xori (UInt32 x, UInt32 y) { return x ^ y; }


} // end of <anonymous> namespace

namespace erfin {

ErfiCpu::ErfiCpu() {
    for (UInt32 & reg : m_registers)
        reg = 0;
}

void ErfiCpu::run_cycle(MemorySpace & memspace) {
    using namespace erfin;
    using namespace erfin::enum_types;
    Inst inst = memspace[m_registers[REG_PC]];
    UInt32 temp, r0t, r1t;
    int tempi;
    const auto giimd = decode_immd_as_int;

    switch (decode_op_code(inst)) {
    case PLUS : do_basic_arth_inst(inst, plus ); break;
    case MINUS: do_basic_arth_inst(inst, minus); break;
    case TIMES: do_basic_arth_inst(inst, times); break;
    case DIVIDE_MOD: switch (decode_op_code(inst)) {
        case REG_REG_REG_REG:
            // n, d, q, r; quot written last
            tempi      = int(reg0(inst)) / int(reg1(inst));
            reg3(inst) = (reg0(inst) & 0x7FFFFFFF) % (reg1(inst) & 0x7FFFFFFF);
            reg2(inst) = UInt32(tempi);
            break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case TIMES_FP: switch (decode_param_form(inst)) {
        case REG_REG_REG:
            reg0(inst) = fp_multiply(reg1(inst), reg2(inst));
            break;
        case REG_IMMD:
            reg0(inst) = fp_multiply(reg0(inst), decode_immd_as_fp(inst));
            break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case DIV_MOD_FP: switch (decode_param_form(inst)) {
        case REG_REG_REG_REG:
            temp       = fp_divide(reg0(inst), reg1(inst));
            reg3(inst) = fp_remainder(temp, reg0(inst), reg1(inst));
            reg2(inst) = temp;
            break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case AND: do_basic_arth_inst(inst, andi); break;
    case XOR: do_basic_arth_inst(inst, xori); break;
    case OR : do_basic_arth_inst(inst, ori) ; break;
    case NOT: switch (decode_param_form(inst)) {
              case REG: reg0(inst) = ~reg0(inst); break;
              default: throw make_illegal_inst_error(inst);
              }
        break;
    case ROTATE: do_rotate(inst); break;
    case COMP: switch (decode_op_code(inst)) {
        case REG_REG_REG:
            temp = 0; r0t = reg0(inst); r1t = reg1(inst);
            // <=, >=, >, <, ==, !=
            if (r0t <  r1t) temp |= 1;
            if (r0t >  r1t) temp |= 2;
            if (r0t == r1t) temp |= 3;
            reg3(inst) = temp;
            break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case SKIP: switch (decode_param_form(inst)) {
        case REG: if (reg0(inst))
                ++m_registers[REG_PC];
            break;
        case REG_IMMD: if (reg0(inst) & UInt32(giimd(inst)))
                ++m_registers[REG_PC];
            break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case LOAD: switch (decode_param_form(inst)) {
        case REG    : reg0(inst) = memspace[reg0(inst)]; break;
        case REG_REG: reg0(inst) = memspace[reg1(inst)]; break;
        case REG_REG_IMMD:
            reg0(inst) = memspace[std::size_t(int(reg1(inst)) + giimd(inst))];
            break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case SAVE: switch (decode_param_form(inst)) {
        case REG_REG:
            memspace[reg1(inst)] = reg0(inst);
            break;
        case REG_REG_IMMD:
            memspace[std::size_t(int(reg1(inst)) + giimd(inst))] = reg0(inst);
            break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case SET_INT: switch (decode_param_form(inst)) {
        case REG_IMMD: reg0(inst) = UInt32(decode_immd_as_int(inst)); break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case SET_FP96: switch (decode_param_form(inst)) {
        case REG_IMMD: reg0(inst) = decode_immd_as_fp(inst); break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case SET_REG: switch (decode_param_form(inst)) {
        case REG_REG: reg0(inst) = reg1(inst); break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case SYSTEM_CALL: /* does not handle system calls */ break;
    default: throw make_illegal_inst_error(inst);
    };

    ++m_registers[REG_PC];
}

void ErfiCpu::print_registers(std::ostream & out) const {
    auto old_pre = out.precision();
    out.precision(3);
    for (int i = 0; i != enum_types::REG_COUNT; ++i) {
        const char * reg_name = register_to_string(Reg(i));
        out << reg_name;
        if (reg_name[1] == 0)
            out << " ";
        out << " | " << int(m_registers[std::size_t(i)]);
        out << " | " << fixed_point_to_double(m_registers[std::size_t(i)]);
    }
    out.precision(old_pre);
}

/* static */ void ErfiCpu::run_tests() {
    Assembler::run_tests();

    Assembler asmr;
    asmr.assemble_from_string(
        "     set  x -10\n"
        "     set  y  10\n"
        ":inc add  x 5\n"
        "     comp x y a\n"
        "     skip a 2 # x > y\n"
        "     set pc inc\n"
        ":safety-loop set pc safety-loop"
    );
    ErfiCpu cpu;
    MemorySpace mem;
    for (UInt32 & i : mem) i = 0;
    load_program_into_memory(mem, asmr.program_data());
    for (int i = 0; i != 10; ++i)
        cpu.run_cycle(mem);
    int j = 0;
    ++j;
}

/* private */ UInt32 & ErfiCpu::reg0(Inst inst)
    { return m_registers[decode_reg0(inst)]; }
/* private */ UInt32 & ErfiCpu::reg1(Inst inst)
    { return m_registers[decode_reg1(inst)]; }
/* private */ UInt32 & ErfiCpu::reg2(Inst inst)
    { return m_registers[decode_reg2(inst)]; }
/* private */ UInt32 & ErfiCpu::reg3(Inst inst)
    { return m_registers[decode_reg3(inst)]; }

/* private */ void ErfiCpu::do_rotate(Inst inst) {
    using namespace enum_types;
    int rint;
    UInt32 r0;
    switch (decode_param_form(inst)) {
    case REG_REG : rint = int(reg1(inst))   ; break;
    case REG_IMMD: rint = decode_immd_as_int(inst); break;
    default: throw make_illegal_inst_error(inst);
    }
    r0 = reg0(inst);
    if (rint < 0) { // left
        rint = (rint*-1) % 32;
        reg0(inst) = (r0 << rint) | (r0 >> (32 - rint));
    } else if (rint > 0) { // right
        rint %= 32;
        reg0(inst) = (r0 >> rint) | (r0 << (32 - rint));
    }
}

/* private */ void ErfiCpu::do_basic_arth_inst
    (Inst inst, UInt32(*func)(UInt32, UInt32))
{
    using namespace erfin::enum_types;
    int r1 = int(reg1(inst)), r0 = int(reg0(inst));
    (void)r1; (void)r0;

    switch (decode_param_form(inst)) {
    case REG_REG_REG:
        reg2(inst) = func(reg1(inst), reg0(inst));
        break;
    case REG_REG_IMMD:
        reg0(inst) = func(reg1(inst), UInt32(decode_immd_as_int(inst)));
        break;
    default: throw make_illegal_inst_error(inst);
    }
}

} // end of erfin namespace

namespace {

Error make_illegal_inst_error(erfin::Inst) {
    return Error("Unsupport instruction.");
}

} // end of erfin namespace
