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
#include "Debugger.hpp"

#include <iostream>

#include <cassert>

namespace {

using Error = std::runtime_error;
using UInt32 = erfin::UInt32;

const char * op_code_to_string(erfin::Inst i);
const char * param_form_to_string(erfin::Inst i);

UInt32 decode_immd_selectively(erfin::Inst inst);

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
    // std::array does not initialize values
    reset();
}

void ErfiCpu::reset() {
    for (UInt32 & reg : m_registers)
        reg = 0;
}

void ErfiCpu::run_cycle(MemorySpace & memspace, ErfiGpu * gpu) {
    using namespace erfin;
    using namespace erfin::enum_types;
    using O = OpCode;
    using Pf = ParamForm;

    Inst inst = deserialize(memspace[m_registers[std::size_t(Reg::REG_PC)]++]);
    const auto giimd = decode_immd_as_int;
    static constexpr const auto is_fp_inst = decode_is_fixed_point_flag_set;

    // R-type split by is_fp (1-bit) and pf (1-bit)
    // PLUS, MINUS, TIMES, DIVIDE, MODULUS, AND, XOR, OR, COMP
    // "S"-type (2bits for pf, 0 bits for is_fp) (set types)
    // SET,SAVE,LOAD
    // J-type (reducable to 0-bits for "pf")
    // SKIP
    // "O"-types (0 bits for "pf") (odd-out types)
    // CALL, NOT
    //
    // ROTATE (1 bit for pf)
    switch (decode_op_code(inst)) {
    case O::PLUS : do_basic_arth_inst(inst, plus ); break;
    case O::MINUS: do_basic_arth_inst(inst, minus); break;
    case O::TIMES:
        do_basic_arth_inst(inst, is_fp_inst(inst) ? fp_multiply : times);
        break;
    case O::DIVIDE:
        do_basic_arth_inst(inst, is_fp_inst(inst) ? div_fp : div_int);
        break;
    case O::MODULUS:
        do_basic_arth_inst(inst, is_fp_inst(inst) ? mod_fp : mod_int);
        break;
    case O::AND: do_basic_arth_inst(inst, andi); break;
    case O::XOR: do_basic_arth_inst(inst, xori); break;
    case O::OR : do_basic_arth_inst(inst, ori ); break;
    case O::NOT:
        if (decode_param_form(inst) != Pf::REG)
            throw emit_error(inst);
        (void)~reg0(inst);
        break;
    case O::ROTATE: do_rotate(inst); break;
    case O::COMP:
        do_basic_arth_inst(inst, is_fp_inst(inst) ? comp_fp : comp_int);
        break;
    case O::SKIP: switch (decode_param_form(inst)) {
        case Pf::REG: if (reg0(inst))
                ++m_registers[std::size_t(Reg::REG_PC)];
            break;
        case Pf::REG_IMMD: if (reg0(inst) & UInt32(giimd(inst)))
                ++m_registers[std::size_t(Reg::REG_PC)];
            break;
        default: throw emit_error(inst);
        }
        break;
    case O::LOAD: switch (decode_param_form(inst)) {
        case Pf::REG_IMMD: reg0(inst) = memspace[std::size_t(giimd(inst))]; break;
        case Pf::REG_REG : reg0(inst) = memspace[reg1(inst)]; break;
        case Pf::REG_REG_IMMD: {
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
    case O::SAVE: switch (decode_param_form(inst)) {
        case Pf::REG_IMMD: memspace[std::size_t(giimd(inst))] = reg0(inst); break;
        case Pf::REG_REG:
            memspace[reg1(inst)] = reg0(inst);
            break;
        case Pf::REG_REG_IMMD:
            memspace[std::size_t(int(reg1(inst)) + giimd(inst))] = reg0(inst);
            std::cout << "saved to address: " << (std::size_t(int(reg1(inst)) + giimd(inst))) << std::endl;
            break;
        default: throw emit_error(inst);
        }
        break;
    case O::SET: switch (decode_param_form(inst)) {
        case Pf::REG_REG_IMMD:
#           ifdef MACRO_DEBUG
            if (decode_reg0(inst) == Reg::REG_PC && is_fp_inst(inst))
                throw emit_error(inst);
#           endif
            reg0(inst) = reg1(inst) + decode_immd_selectively(inst);
            break;
        case Pf::REG_REG : reg0(inst) = reg1(inst); break;
        case Pf::REG_IMMD: reg0(inst) = decode_immd_selectively(inst); break;
        default: throw emit_error(inst);
        }
        break;
    case O::SYSTEM_CALL: if (gpu) do_syscall(inst, *gpu); break;
    default: throw emit_error(inst);
    };
}

void ErfiCpu::print_registers(std::ostream & out) const {
    auto old_pre = out.precision();
    out.precision(3);
    for (int i = 0; i != int(Reg::REG_COUNT); ++i) {
        const char * reg_name = register_to_string(Reg(i));
        out << reg_name;
        if (reg_name[1] == 0)
            out << " ";
        out << " | " << int(m_registers[std::size_t(i)]);
        out << " | " << fixed_point_to_double(m_registers[std::size_t(i)]);
        out << "\n";
    }
    out.precision(old_pre);
    out << std::flush;
}

void ErfiCpu::update_debugger(Debugger & dbgr) const {
    dbgr.update_internals(m_registers);
}

void try_program(const char * source_code, const int inst_limit_c);

/* static */ void ErfiCpu::run_tests() {
    //assert(decode_immd_as_int(encode_immd(-1)) == -1);
#   if 0
    try_program(
        "     assume integer \n"
        "     set  x -10\n"
        "     set  y  10\n"
        ":inc add  x   5\n"
        "     comp a x y\n"
        "     skip a >= \n"
        "     jump   inc\n"
        ":safety-loop set pc safety-loop", 20);
#   endif
    try_program(
        "set  sp safety-loop\n"
        "set  a 1\n"
        "push a b c x y z \n"
        "push pc \n"
        "set  a 0\n"
        "pop  z y x c b a z \n"
        "set  a 1\n"
        "push a  \n"
        "set  a 0\n"
        "pop  a  \n"
        ":safety-loop set pc safety-loop", 25);
    try_program(
        "     set sp stack-start\n"
        "     set x 1\n"
        "     set y 2\n"
        "     set z 3\n"
        "     set a 4\n"
        "     set b 5\n"
        "     set c 6\n"
        "     push a b c x y z\n"
        "     set x 0\n"
        "     set y 0\n"
        "     set z 0\n"
        "     set a 0\n"
        "     set b 0\n"
        "     set c 0\n"
        "     pop a b c x y z\n"
        ":safety-loop set pc safety-loop\n"
        ":stack-start data [________ ________ ________ ________]", 30);
}

void try_program(const char * source_code, const int inst_limit_c) {
    Assembler asmr;
    ErfiCpu cpu;
    MemorySpace mem;

    try {
        asmr.assemble_from_string(source_code);
        for (UInt32 & i : mem) i = 0;
        load_program_into_memory(mem, asmr.program_data());
        for (int i = 0; i != inst_limit_c; ++i)
            cpu.run_cycle(mem);
    } catch (ErfiError & err) {
        auto sline = asmr.translate_to_line_number(err.program_location());
        std::cerr << "Illegal instruction occured!\n";
        std::cerr << "See line " << sline << " in source\n";
        std::cerr << "Details: " << err.what() << std::endl;
    } catch (std::exception & exp) {
        std::cerr << "General exception: " << exp.what() << std::endl;
    }
}

/* private */ UInt32 & ErfiCpu::reg0(Inst inst)
    { return m_registers[std::size_t(decode_reg0(inst))]; }
/* private */ UInt32 & ErfiCpu::reg1(Inst inst)
    { return m_registers[std::size_t(decode_reg1(inst))]; }
/* private */ UInt32 & ErfiCpu::reg2(Inst inst)
    { return m_registers[std::size_t(decode_reg2(inst))]; }

/* private */ void ErfiCpu::do_rotate(Inst inst) {
    using namespace enum_types;
    using Pf = ParamForm;
    int rint;
    UInt32 r0;
    switch (decode_param_form(inst)) {
    case Pf::REG_REG : rint = int(reg1(inst))         ; break;
    case Pf::REG_IMMD: rint = decode_immd_as_int(inst); break;
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
    using Sys = SystemCallValue_e;
    auto get_reg_ref = [this] (Reg r) -> UInt32 &
        { return std::ref(m_registers[std::size_t(r)]); };
    auto & x = get_reg_ref(Reg::REG_X), & y = get_reg_ref(Reg::REG_Y),
         & z = get_reg_ref(Reg::REG_Z);
    auto & a = get_reg_ref(Reg::REG_A);
    static_assert(std::is_same<decltype(x), decltype(y)>::value, "");
    switch (Sys(decode_immd_as_int(inst))) {
    case Sys::UPLOAD_SPRITE : a = gpu.upload_sprite(x, y, z); break;
    case Sys::UNLOAD_SPRITE : gpu.unload_sprite(x); break;
    case Sys::DRAW_SPRITE   : gpu.draw_sprite(x, y, z); break;
    case Sys::SCREEN_CLEAR  : gpu.screen_clear(); break;
    case Sys::WAIT_FOR_FRAME: m_wait_called = true; break;
    case Sys::READ_INPUT    : break;
    default:
        throw emit_error(inst);
    }
}

/* private */ void ErfiCpu::do_basic_arth_inst
    (Inst inst, UInt32(*func)(UInt32, UInt32))
{
    using Pf = ParamForm;
    UInt32 & r0 = reg0(inst), & r1 = reg1(inst);
    switch (decode_param_form(inst)) {
    case Pf::REG_REG_REG : r0 = func(r1, reg2(inst)); break;
    case Pf::REG_REG_IMMD: r0 = func(r1, decode_immd_selectively(inst)); break;
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

UInt32 decode_immd_selectively(erfin::Inst inst) {
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
    using O = OpCode;
    switch (decode_op_code(i)) {
    case O::PLUS       : return "plus";
    case O::MINUS      : return "minus";
    case O::TIMES      : return "times";
    case O::DIVIDE     : return "div";
    case O::MODULUS    : return "mod";
    case O::AND        : return "and";
    case O::XOR        : return "xor";
    case O::OR         : return "or";
    case O::NOT        : return "not";
    case O::ROTATE     : return "rotate";
    case O::COMP       : return "compare";
    case O::SKIP       : return "skip";
    case O::LOAD       : return "load";
    case O::SAVE       : return "save";
    case O::SET        : return "set";
    case O::SYSTEM_CALL: return "call";
    default: return "<NOT ANY OPTCODE>";
    }
}

const char * param_form_to_string(erfin::Inst i) {
    using namespace erfin;
    using Pf = ParamForm;
    switch (decode_param_form(i)) {
    case Pf::REG_REG_REG : return "3 registers";
    case Pf::REG_REG_IMMD: return "2 registers and an immediate";
    case Pf::REG_REG     : return "2 registers";
    case Pf::REG_IMMD    : return "1 register and an immediate";
    case Pf::REG         : return "1 register";
    case Pf::NO_PARAMS   : return "no parameters";
    default              : return "<INVALID PARAMETER FORM>";
    }
}

} // end of erfin namespace
