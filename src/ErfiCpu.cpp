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
UInt32 rotate(UInt32 x, UInt32 y);

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
    Inst inst = deserialize(memspace[m_registers[std::size_t(Reg::PC)]++]);
    run_cycle(inst, memspace, gpu);
}

void ErfiCpu::run_cycle(Inst inst, MemorySpace & memspace, ErfiGpu * gpu) {
    // 2+3+2 | r0 | r1 | r2
    //       | r0 | r1 | immd
    //       | r0 |    | immd
    // R-type type indifferent split by pf (1-bit) (integer only)
    // PLUS, MINUS, AND, XOR, OR, ROTATE
    //
    // R-type split by is_fp (1-bit) and pf (1-bit)
    // TIMES, DIVIDE, MODULUS, COMP
    //
    // M-type (2bits for pf, 0 bits for is_fp) (set types)
    // SET, SAVE, LOAD
    //
    // J-type (reducable to 0-bits for "pf")
    // SKIP (make default immd 0b1111 ^ NOT_EQUAL)
    //
    // "O"-types (0 bits for "pf") (odd-out types)
    // CALL, NOT
    using O  = OpCode;
    switch (decode_op_code(inst)) {
    case O::PLUS   : do_basic_arth    <plus                 >(inst);   return;
    case O::MINUS  : do_basic_arth    <minus                >(inst);   return;
    case O::AND    : do_basic_arth    <andi                 >(inst);   return;
    case O::XOR    : do_basic_arth    <xori                 >(inst);   return;
    case O::OR     : do_basic_arth    <ori                  >(inst);   return;
    case O::ROTATE : do_basic_arth    <rotate               >(inst);   return;
    case O::TIMES  : do_arth_branch_fp<fp_multiply, times   >(inst);   return;
    case O::DIVIDE : do_arth_branch_fp<div_fp     , div_int >(inst);   return;
    case O::MODULUS: do_arth_branch_fp<mod_fp     , mod_int >(inst);   return;
    case O::COMP   : do_arth_branch_fp<comp_fp    , comp_int>(inst);   return;
    case O::SET    : do_set(inst);                                     return;
    case O::SAVE   : memspace[get_move_op_address(inst)] = reg0(inst); return;
    case O::LOAD   : reg0(inst) = memspace[get_move_op_address(inst)]; return;
    case O::SKIP   : do_skip(inst);                                    return;
    case O::NOT    : do_not (inst);                                    return;
    case O::SYSTEM_CALL: if (gpu) do_syscall(inst, *gpu);              return;
    default: emit_error(inst);
    };
}

void ErfiCpu::clear_flags() {
    m_wait_called = false;
}

void ErfiCpu::print_registers(std::ostream & out) const {
    auto old_pre = out.precision();
    out.precision(3);
    for (int i = 0; i != int(Reg::COUNT); ++i) {
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

bool ErfiCpu::wait_was_called() const { return m_wait_called; }

void try_program(const char * source_code, const int inst_limit_c);

/* static */ void ErfiCpu::run_tests() {
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

/* private */ void ErfiCpu::do_syscall(Inst inst, ErfiGpu & gpu) {
    using Sys = SystemCallValue;
    auto get_reg_ref = [this] (Reg r) -> UInt32 &
        { return std::ref(m_registers[std::size_t(r)]); };
    auto & x = get_reg_ref(Reg::X), & y = get_reg_ref(Reg::Y),
         & z = get_reg_ref(Reg::Z);
    auto & a = get_reg_ref(Reg::A);
    static_assert(std::is_same<decltype(x), decltype(y)>::value, "");
    switch (Sys(decode_immd_as_int(inst))) {
    case Sys::UPLOAD_SPRITE : a = gpu.upload_sprite(x, y, z); break;
    case Sys::UNLOAD_SPRITE : gpu.unload_sprite(x); break;
    case Sys::DRAW_SPRITE   : gpu.draw_sprite(x, y, z); break;
    case Sys::SCREEN_CLEAR  : gpu.screen_clear(); break;
    case Sys::WAIT_FOR_FRAME:
        m_wait_called = true;
        break;
    case Sys::READ_INPUT    : break;
    default: emit_error(inst);
    }
}

/* private */ void ErfiCpu::do_set(Inst inst) {
    using Pf = ParamForm;
    switch (decode_param_form(inst)) {
    case Pf::REG_REG_IMMD:
#       ifdef MACRO_DEBUG
        if (decode_reg0(inst) == Reg::PC && decode_is_fixed_point_flag_set(inst))
            emit_error(inst);
#       endif
        reg0(inst) = reg1(inst) + ::decode_immd_selectively(inst);
        return;
    case Pf::REG_REG : reg0(inst) = reg1(inst); return;
    case Pf::REG_IMMD: reg0(inst) = ::decode_immd_selectively(inst); return;
    default: emit_error(inst);
    }
}

/* private */ void ErfiCpu::do_skip(Inst inst) {
    switch (decode_param_form(inst)) {
    case ParamForm::REG: if (reg0(inst))
            ++m_registers[std::size_t(Reg::PC)];
        return;
    case ParamForm::REG_IMMD: if (reg0(inst) & UInt32(decode_immd_as_int(inst)))
            ++m_registers[std::size_t(Reg::PC)];
        return;
    default: emit_error(inst);
    }
}

/* private */ void ErfiCpu::do_not(Inst inst) {
    if (decode_param_form(inst) != ParamForm::REG)
        emit_error(inst);
    (void)~reg0(inst);
}

/* private static */ UInt32 ErfiCpu::decode_immd_selectively(Inst inst) {
    return ::decode_immd_selectively(inst);
}

/* private */ std::size_t ErfiCpu::get_move_op_address(Inst inst) {
    using Pf = ParamForm;
    std::size_t address;
    static constexpr const auto giimd = decode_immd_as_int;
    switch (decode_param_form(inst)) {
    case Pf::REG_IMMD    : address = std::size_t(giimd(inst)); break;
    case Pf::REG_REG     : address = reg1(inst); break;
    case Pf::REG_REG_IMMD:
        address = std::size_t(int(reg1(inst)) + giimd(inst));
        break;
    default: emit_error(inst);
    }
#   if 0 //def MACRO_DEBUG
    switch (decode_op_code(inst)) {
    case OpCode::SAVE:
        std::cout << "saved to address: " << address
                  << " from "
                  << register_to_string(decode_reg0(inst)) << " with value "
                  << reg0(inst) << std::endl;
        break;
    case OpCode::LOAD:
        std::cout << "loaded from address: " << address << " to "
                  << register_to_string(decode_reg0(inst)) << std::endl;
        break;
    default: assert(false); break;
    }
#   if 0
    if (address > memspace.size()) {
        throw Error("Cannot save to address outside of memory space.");
    }
#   endif
#   endif
    return address;
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

UInt32 rotate(UInt32 x, UInt32 y) {
    int rint = int(y);
    if (rint < 0) { // left
        rint = (rint*-1) % 32;
        return (x << rint) | (x >> (32 - rint));
    } else if (rint > 0) { // right
        rint %= 32;
        return (x >> rint) | (x << (32 - rint));
    }
    return x;
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
    if (res == 0) temp |= COMP_EQUAL_MASK;
    if (res <  0) temp |= COMP_LESS_THAN_MASK;
    if (res >  0) temp |= COMP_GREATER_THAN_MASK;
    if (res != 0) temp |= COMP_NOT_EQUAL_MASK;
    return temp;
}

UInt32 comp_int(UInt32 x, UInt32 y) {
    using namespace erfin;
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
