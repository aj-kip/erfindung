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
#include "ErfiConsole.hpp"
#include "Debugger.hpp"
#include "StringUtil.hpp"

#include <iostream>
#include <iomanip>
#include <functional>

#include <cassert>

#ifdef MACRO_COMPILER_GCC
#   define MACRO_FALLTHROUGH
#elif defined(MACRO_COMPILER_MSVC)
#   define MACRO_FALLTHROUGH
#elif defined(MACRO_COMPILER_CLANG)
#   define MACRO_FALLTHROUGH [[clang::fallthrough]]
#else
#   error "no compiler defined"
#endif

namespace {

using Error = std::runtime_error;
using UInt32 = erfin::UInt32;

const char * op_code_to_string(erfin::Inst i);
const char * param_form_to_string(erfin::Inst i);

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
UInt32 comp_int(UInt32 x, UInt32 y);

} // end of <anonymous> namespace

namespace erfin {

ErfiCpu::ErfiCpu()
    { reset(); }

void ErfiCpu::reset() {
    // std::array does not initialize values
    // note: g++ on ARM64 -Ofast, produces broken executable
    // attempting to find issue
    // failed guess -> std::fill here fails to write zeros
    std::fill(m_registers.begin(), m_registers.end(), 0);
}

void ErfiCpu::run_cycle(ConsolePack & console) {
    auto & pc_reg = m_registers[std::size_t(Reg::PC)];
    if (pc_reg >= console.ram->size()) {
        throw ErfiCpuError(pc_reg,
            "Failed to decode instruction at invalid address. Note that the "
            "PC cannot load instructions from devices. (Perhaps a bad SET pc "
            "instruction?)");
    }

    run_cycle(deserialize((*console.ram)[pc_reg++]), console);
}

void ErfiCpu::run_cycle(Inst inst, ConsolePack & console) {
    // ------------------- This is inside a HOT LOOP --------------------------
    using O = OpCode;
    switch (decode_op_code(inst)) {
    // R-type type indifferent split by pf (1-bit) (integer only)
    // PLUS, MINUS, AND, XOR, OR, ROTATE
    case O::PLUS   : do_arth<plus       , plus    >(inst); return;
    case O::MINUS  : do_arth<minus      , minus   >(inst); return;
    case O::AND    : do_arth<andi       , andi    >(inst); return;
    case O::XOR    : do_arth<xori       , xori    >(inst); return;
    case O::OR     : do_arth<ori        , ori     >(inst); return;
    case O::ROTATE : do_arth<rotate     , rotate  >(inst); return;
    // R-type split by is_fp (1-bit) and pf (1-bit)
    // TIMES, DIVIDE, MODULUS, COMP
    case O::TIMES  : do_arth<fp_multiply, times   >(inst); return;
    case O::DIVIDE : do_arth<div_fp     , div_int >(inst); return;
    case O::MODULUS: do_arth<mod_fp     , mod_int >(inst); return;
    case O::COMP   : do_arth<fp_compare , comp_int>(inst); return;
    // M-type (2bits for pf, 0 bits for is_fp) (set types)
    // SET, SAVE, LOAD
    case O::SET : do_set(inst); return;
    case O::SAVE: do_write(console, get_move_op_address(inst), reg0(inst)); return;
    case O::LOAD: reg0(inst) = do_read(console, get_move_op_address(inst)); return;
    // J-type (reducable to 0-bits for "pf")
    // SKIP (make default immd 0b1111 ^ NOT_EQUAL)
    case O::SKIP: do_skip(inst         ); return;
    case O::CALL: do_call(inst, console); return;
    // "O"-types (0 bits for "pf") (odd-out types)
    // NOT
    case O::NOT: do_not(inst); return;
    default: throw_error(inst); // throws
    };
}

void ErfiCpu::update_debugger(Debugger & dbgr) const {
    dbgr.update_internals(m_registers);
}

void try_program(const char * source_code, const int inst_limit_c);

/* static */ void ErfiCpu::run_tests() {
    try_program(
        "     assume integer \n"
        "     set  x -10\n"
        "     set  y  10\n"
        ":inc add  x   5\n"
        "     comp a x y\n"
        "     skip a >= \n"
        "     jump   inc\n"
        ":safety-loop set pc safety-loop", 20);
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
    assert(mod_int(UInt32(-1), UInt32(-1)) == 0);
    assert(mod_int( 3,  2) == 1);
    assert(mod_int( 7,  4) == 7 % 4);
    assert(int(mod_int(UInt32(-7), UInt32( 4))) == -(7 % 4));
    assert(int(mod_int(UInt32( 7), UInt32(-4))) == -(7 % 4));
}

void try_program(const char * source_code, const int inst_limit_c) {
    Assembler asmr;
    MemorySpace mem;
    ErfiCpu cpu;
    ConsolePack con; con.cpu = &cpu; con.ram = &mem;

    try {
        asmr.assemble_from_string(source_code);
        for (UInt32 & i : *con.ram) i = 0;
        Console::load_program_to_memory(asmr.program_data(), *con.ram);
        for (int i = 0; i != inst_limit_c; ++i)
            con.cpu->run_cycle(con);
    } catch (ErfiCpuError & err) {
        auto sline = asmr.translate_to_line_number(err.program_location());
        std::cerr << "Illegal instruction occured!\n";
        std::cerr << "See line " << sline << " in source\n";
        std::cerr << "Details: " << err.what() << std::endl;
    } catch (std::exception & exp) {
        std::cerr << "General exception: " << exp.what() << std::endl;
    }
}

/* private */ void ErfiCpu::do_set(Inst inst) {
    using Pf = SetTypeParamForm;
    UInt32 & r0 = reg0(inst);
    switch (decode_s_type_pf(inst)) {
    case Pf::_2R_INTVER: r0 =        reg1              (inst) ; return;
    case Pf::_1R_INT   : r0 = UInt32(decode_immd_as_int(inst)); return;
    case Pf::_2R_FPVER : r0 =        reg1              (inst) ; return;
    case Pf::_1R_FP    : r0 =        decode_immd_as_fp (inst) ; return;
    }
}

/* private */ void ErfiCpu::do_skip(Inst inst) {
    using Pf = JTypeParamForm;
    UInt32 & pc = m_registers[std::size_t(Reg::PC)];
    switch (decode_j_type_pf(inst)) {
    case Pf::_1R             : if (reg0(inst)) ++pc; return;
    case Pf::_1R_INT_FOR_JUMP:
        if (reg0(inst) & UInt32(decode_immd_as_int(inst))) ++pc;
        return;
    }
}

/* private */ void ErfiCpu::do_call(Inst inst, ConsolePack & pack) {
    using Pf = JTypeParamForm;
    UInt32 & pc = m_registers[std::size_t(Reg::PC)];
    do_write(pack, ++m_registers[std::size_t(Reg::SP)], pc);
    switch (decode_j_type_pf(inst)) {
    case Pf::_1R           : pc = reg0(inst); return;
    case Pf::_IMMD_FOR_CALL: pc = UInt32(decode_immd_as_int(inst)); return;
    }
}

/* private */ void ErfiCpu::do_not(Inst inst) {
    // prevent from accidently mutating register one
    UInt32 t   = reg1(inst);
    reg0(inst) = ~t;
}

/* private */ UInt32 ErfiCpu::get_move_op_address(Inst inst) {
    using Pf = MTypeParamForm;
    switch (decode_m_type_pf(inst)) {
    case Pf::_2R_INT : return plus(UInt32(decode_immd_as_int(inst)), reg1(inst));
    case Pf::_2R     : return reg1(inst);
    case Pf::_1R_INT : return decode_immd_as_addr(inst);
    case Pf::_INVALID: return 0;
    }
    std::terminate();
}

/* private */ [[noreturn]] void ErfiCpu::throw_error(Inst i) const {
    //               PC increment while the instruction is executing
    //               so it will be one too great if an illegal instruction
    //               is encountered -> what about jumps?
    throw ErfiCpuError(m_registers[std::size_t(Reg::PC)] - 1,
                       std::move(disassemble_instruction(i)));
}

/* private */ std::string ErfiCpu::disassemble_instruction(Inst i) const {
    return std::string("Unsupport instruction \"") +
           op_code_to_string(i) + "\" with parameter form of: " +
           param_form_to_string(i);
}

} // end of erfin namespace

namespace {

UInt32 rotate(UInt32 x, UInt32 y) {
    auto rint = erfin::Int32(y);
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
    static const auto FP_ZERO_ = erfin::to_fixed_point(0.0);
    if (y == FP_ZERO_)
        throw Error("Attempted to divide by zero.");
    return erfin::fp_divide(x, y);
}

UInt32 div_int (UInt32 x, UInt32 y) {
    if (y == 0u) throw Error("Attempted to divide by zero.");
    return UInt32(erfin::Int32(x) / erfin::Int32(y));
}

UInt32 mod_fp  (UInt32 x, UInt32 y) {
    return erfin::fp_remainder(div_fp(x, y), y, x);
}

UInt32 mod_int (UInt32 x, UInt32 y) {
    if (y == 0) throw Error("Attempted to divide by zero.");
    static const auto sign = [](UInt32 x) { return x & 0x80000000; };
    static const auto mag  = [](UInt32 x) { return sign(x) ? ~(x - 1) : x; };
    // the C++ standard does not specify how negatives are moded
    // using two's complement
    return (sign(x) ^ sign(y)) ? ~((mag(x) % mag(y)) - 1) : (mag(x) % mag(y));
}

UInt32 comp_int(UInt32 x, UInt32 y) {
    using namespace erfin;
    UInt32 temp = 0;
    auto x_ = Int32(x), y_ = Int32(y);
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
    case O::PLUS   : return "plus"   ;
    case O::MINUS  : return "minus"  ;
    case O::TIMES  : return "times"  ;
    case O::DIVIDE : return "div"    ;
    case O::MODULUS: return "mod"    ;
    case O::AND    : return "and"    ;
    case O::XOR    : return "xor"    ;
    case O::OR     : return "or"     ;
    case O::NOT    : return "not"    ;
    case O::ROTATE : return "rotate" ;
    case O::COMP   : return "compare";
    case O::SKIP   : return "skip"   ;
    case O::LOAD   : return "load"   ;
    case O::SAVE   : return "save"   ;
    case O::SET    : return "set"    ;
    default: return "<NOT ANY OPTCODE>";
    }
}

const char * param_form_to_string(erfin::Inst i) {
    (void)i;
    return "<INVALID PARAMETER FORM>";
}

} // end of erfin namespace
