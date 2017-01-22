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
#include "ErfiGpu.hpp"

#include <iostream>

#include <cassert>

namespace {

using Error = std::runtime_error;
using UInt32 = erfin::UInt32;
#if 0
Error make_illegal_inst_error(erfin::Inst inst);
#endif

const char * op_code_to_string(erfin::Inst i);
const char * param_form_to_string(erfin::Inst i);

UInt32 decode_immd_selectively(UInt32 inst);

UInt32 plus (UInt32 x, UInt32 y) { return x + y; }
UInt32 minus(UInt32 x, UInt32 y) { return x - y; }
UInt32 times(UInt32 x, UInt32 y) { return x * y; }
UInt32 andi (UInt32 x, UInt32 y) { return x & y; }
UInt32 ori  (UInt32 x, UInt32 y) { return x | y; }
UInt32 xori (UInt32 x, UInt32 y) { return x ^ y; }

UInt32 div_fp  (UInt32 x, UInt32 y);
UInt32 div_int (UInt32 x, UInt32 y);
UInt32 mod_fp  (UInt32 x, UInt32 y);
UInt32 mod_int (UInt32 x, UInt32 y);
UInt32 comp_fp (UInt32 x, UInt32 y);
UInt32 comp_int(UInt32 x, UInt32 y);

} // end of <anonymous> namespace

namespace erfin {

ErfiCpu::ErfiCpu():
    m_wait_called(false)
{
    for (UInt32 & reg : m_registers)
        reg = 0;
}

void ErfiCpu::run_cycle(MemorySpace & memspace, ErfiGpu * gpu) {
    using namespace erfin;
    using namespace erfin::enum_types;

    Inst inst = memspace[m_registers[REG_PC]++];
    const auto giimd = decode_immd_as_int;
    static constexpr const auto is_fp_inst = decode_is_fixed_point_flag_set;

    switch (decode_op_code(inst)) {
    case PLUS : do_basic_arth_inst(inst, plus ); break;
    case MINUS: do_basic_arth_inst(inst, minus); break;
    case TIMES:
        do_basic_arth_inst(inst, is_fp_inst(inst) ? fp_multiply : times);
        break;
    case DIVIDE:
        do_basic_arth_inst(inst, is_fp_inst(inst) ? div_fp : div_int);
        break;
    case MODULUS:
        do_basic_arth_inst(inst, is_fp_inst(inst) ? mod_fp : mod_int);
        break;
    case AND: do_basic_arth_inst(inst, andi); break;
    case XOR: do_basic_arth_inst(inst, xori); break;
    case OR : do_basic_arth_inst(inst, ori ); break;
    case NOT:
        if (decode_param_form(inst) != REG)
            throw emit_error(inst);
        (void)~reg0(inst);
        break;
    case ROTATE: do_rotate(inst); break;
    case COMP:
        do_basic_arth_inst(inst, is_fp_inst(inst) ? comp_fp : comp_int);
        break;
    case SKIP: switch (decode_param_form(inst)) {
        case REG: if (reg0(inst))
                ++m_registers[REG_PC];
            break;
        case REG_IMMD: if (reg0(inst) & UInt32(giimd(inst)))
                ++m_registers[REG_PC];
            break;
        default: throw emit_error(inst);
        }
        break;
    case LOAD: switch (decode_param_form(inst)) {
        case REG_IMMD: reg0(inst) = memspace[std::size_t(giimd(inst))]; break;
        case REG_REG : reg0(inst) = memspace[reg1(inst)]; break;
        case REG_REG_IMMD: {
            std::size_t address = std::size_t(int(reg1(inst)) + giimd(inst));
            if (address > memspace.size()) {
                throw Error("Cannot load from address outside of memory space.");
            }
            std::cout << "loaded from address: " << address << std::endl;
            reg0(inst) = memspace[address];
            break;
        }
        default: throw emit_error(inst);
        }
        break;
    case SAVE: switch (decode_param_form(inst)) {
        case REG_IMMD: memspace[std::size_t(giimd(inst))] = reg0(inst); break;
        case REG_REG:
            memspace[reg1(inst)] = reg0(inst);
            break;
        case REG_REG_IMMD:
            memspace[std::size_t(int(reg1(inst)) + giimd(inst))] = reg0(inst);
            break;
        default: throw emit_error(inst);
        }
        break;
    case SET: switch (decode_param_form(inst)) {
        case REG_REG_IMMD:
#           ifdef MACRO_DEBUG
            if (decode_reg0(inst) == REG_PC && is_fp_inst(inst))
                throw emit_error(inst);
#           endif
            reg0(inst) = reg1(inst) + decode_immd_selectively(inst);
            break;
        case REG_REG : reg0(inst) = reg1(inst); break;
        case REG_IMMD: reg0(inst) = decode_immd_selectively(inst); break;
        default: throw emit_error(inst);
        }
        break;
    case SYSTEM_CALL: if (gpu) do_syscall(inst, *gpu); break;
    default: throw emit_error(inst);
    };
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
    assert(decode_immd_as_int(encode_immd(-1)) == -1);
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
    default: throw emit_error(inst);
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

/* private */ void ErfiCpu::do_syscall(Inst inst, ErfiGpu & gpu) {
    using namespace erfin::enum_types;

    auto & x = m_registers[REG_X], & y = m_registers[REG_Y], & z = m_registers[REG_Z];
    auto & a = m_registers[REG_A];
    static_assert(std::is_same<decltype(x), decltype(y)>::value, "");
    switch (decode_immd_as_int(inst)) {
    case UPLOAD_SPRITE : a = gpu.upload_sprite(x, y, z); break;
    case UNLOAD_SPRITE : gpu.unload_sprite(x); break;
    case DRAW_SPRITE   : gpu.draw_sprite(x, y, z); break;
    case SCREEN_CLEAR  : gpu.screen_clear(); break;
    case WAIT_FOR_FRAME: m_wait_called = true; break;
    case READ_INPUT    : break;
    default:
        throw emit_error(inst);
    }
}

/* private */ void ErfiCpu::do_basic_arth_inst
    (Inst inst, UInt32(*func)(UInt32, UInt32))
{
    using namespace erfin::enum_types;
    switch (decode_param_form(inst)) {
    case REG_REG_REG:
        reg0(inst) = func(reg1(inst), reg2(inst));
        break;
    case REG_REG_IMMD:
        reg0(inst) = func(reg1(inst), decode_immd_selectively(inst));
        break;
    default: throw emit_error(inst);
    }
}

/* private */ std::string ErfiCpu::disassemble_instruction(Inst i) const {
    return std::string("Unsupport instruction \"") +
           op_code_to_string(i) + "\" with parameter form of: " +
           param_form_to_string(i);
}

} // end of erfin namespace

namespace {

#if 0
Error make_illegal_inst_error(erfin::Inst i) {
    return Error(std::string("Unsupport instruction \"") +
                 op_code_to_string(i) + "\" with parameter form of: " +
                 param_form_to_string(i));
}
#endif
UInt32 decode_immd_selectively(UInt32 inst) {
    if (erfin::decode_is_fixed_point_flag_set(inst))
        return erfin::decode_immd_as_fp(inst);
    else
        return UInt32(erfin::decode_immd_as_int(inst));
}

UInt32 div_fp  (UInt32 x, UInt32 y) {
    if (y == erfin::to_fixed_point(0.0))
        throw Error("Attempted to divide by zero.");
    return erfin::fp_divide(x, y);
}

UInt32 div_int (UInt32 x, UInt32 y) {
    if (y == 0) throw Error("Attempted to divide by zero.");
    return UInt32(int(x) / int(y));
}

UInt32 mod_fp  (UInt32 x, UInt32 y) {
    return erfin::fp_remainder(div_fp(x, y), y, x);
}

UInt32 mod_int (UInt32 x, UInt32 y) {
    if (y == 0) throw Error("Attempted to divide by zero.");
    auto sign = [](int x) { return x < 0 ? -1 : 1; };
    auto mag  = [](int x) { return x < 0 ? -x : x; };
    // the C++ standard does not specify how negatives are moded
    int rv = mag(int(x)) % mag(int(y));
    return UInt32(sign(x)*sign(y)*rv);
}

UInt32 comp_fp (UInt32 x, UInt32 y) {
    using namespace erfin;
    UInt32 temp = 0;
    int res = erfin::fp_compare(x, y);
    if (res == 0) temp |= enum_types::COMP_EQUAL_MASK;
    if (res <  0) temp |= enum_types::COMP_LESS_THAN_MASK;
    if (res >  0) temp |= enum_types::COMP_GREATER_THAN_MASK;
    if (res != 0) temp |= enum_types::COMP_NOT_EQUAL_MASK;
    return temp;
}

UInt32 comp_int(UInt32 x, UInt32 y) {
    using namespace erfin::enum_types;
    UInt32 temp = 0;
    int x_ = int(x), y_ = int(y);
    if (x_ <  y_) temp |= COMP_LESS_THAN_MASK   ;
    if (x_ >  y_) temp |= COMP_GREATER_THAN_MASK;
    if (x_ == y_) temp |= COMP_EQUAL_MASK       ;
    if (x_ != y_) temp |= COMP_NOT_EQUAL_MASK   ;
    return temp;
}

const char * op_code_to_string(erfin::Inst i) {
    using namespace erfin;
    using namespace erfin::enum_types;
    switch (decode_op_code(i)) {
    case PLUS       : return "plus";
    case MINUS      : return "minus";
    case TIMES      : return "times";
    case DIVIDE : return "divmod";
    case AND        : return "and";
    case XOR        : return "xor";
    case OR         : return "or";
    case NOT        : return "not";
    case ROTATE     : return "rotate";
    case COMP       : return "compare";
    case SKIP       : return "skip";
    case LOAD       : return "load";
    case SAVE       : return "save";
    case SET        : return "set";
    case SYSTEM_CALL: return "call";
    default: return "<NOT ANY OPTCODE>";
    }
}

const char * param_form_to_string(erfin::Inst i) {
    using namespace erfin;
    using namespace erfin::enum_types;
    switch (decode_param_form(i)) {
    case REG_REG_REG_REG: return "4 registers";
    case REG_REG_REG    : return "3 registers";
    case REG_REG_IMMD   : return "2 registers and an immediate";
    case REG_REG        : return "2 registers";
    case REG_IMMD       : return "1 register and an immediate";
    case REG            : return "1 register";
    case NO_PARAMS      : return "no parameters";
    default             : return "<INVALID PARAMETER FORM>";
    }
}

} // end of erfin namespace
