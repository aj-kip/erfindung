/****************************************************************************

    File: ErfiDefs.hpp
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

/** :TODO: I've made the assembler very type unsafe.
 *  I am not regretting this decision. So to refactor, I would like to add
 *  type-safety. I will does this in increments.
 */

/** This document/source file defines Erfindung ISA/interpreter.
 *  Target features:
 *  - RISC + some bells and wistles
 *  - assemble to C++ and dynamically executable byte code
 *
 *  Erfindung is designed to handle two different types of numbers:
 *   - unsigned 32 bit integers (also used for pointers)
 *   - two's complement 32 bit fixed point real numbers
 *
 *  Each instruction is strictly 32bits
 *  There are 8 32bit registers.
 *   -
 *  Erfindung has instructions that can handle fixed point math
 *
 *
 *  Erfindung assmebly language: is a superset of the provided hardware
 *  instructions.
 *  Terms:
 *  - Hardware instruction: an instruction defined by the ISA
 *  - psuedo instruction: an instruction NOT defined by the ISA, but can be
 *    formed from a combination of one or more hardware instructions.
 *
 *  Basic syntax:
 *  NAME: MNEMONIC SUB-MNEMONIC ARGUEMENTS # comment
 *  break down of syntax structure
 *  "NAME" a named constant/label, if stated first it must end with ":"
 *  ":" special operator saying "this is a NAME not a MNEMONIC"
 *  "MNEMONIC" word specifing an operation,
 *  "SUB-MNEMONIC" 2nd word specifing an operation, if necessary
 *  note: a SUB-MNEMONIC must follow if the MNEMONIC has SUB-MNEMONICs however,
 *        if there are no SUB-MNEMONICs for it, then none must follow
 *  "ARGUEMENTS" a mixed set registers, names or immdiates which the
 *               instruction takes as arguements
 *  "# comment" ignored by the assembler
 *  note: blank lines are also ignore by the assemlber
 *
 *  sample code:
 *  set  a     2       # SET INT
 *  set  b     2.2     # SET FP96
 *  set  x     2i      # SET ABS
 *  load a     b     0 # LOAD
 *  save b     a     0 # SAVE
 *  add  a     b     x # ADD
 *  sub  a     b     y # MINUS
 *  cmp  x     y     a # COMPARE
 */
#ifndef MACRO_HEADER_GUARD_ERFIDEFS_HPP
#define MACRO_HEADER_GUARD_ERFIDEFS_HPP

#include <cstdint>
#include <array>
#include <vector>

namespace erfin {

using UInt8  = uint8_t;
using UInt16 = uint16_t;
using UInt64 = uint64_t;
using UInt32 = uint32_t;

// program memory layout
// needs a stack
// need space for program code
//
// +---------------
// |
// |      Program Stack
// |
// +---------------
// |
// |  [Stack growth space]
// |
// +--------------
// | [potentially unfilled]
// +--------------
// |
// |  Program Code
// |
// +-----------------

// 32bit fixed point numbers, shhh-hhhh hhhh-hhhh llll-llll llll-llll
// 3 bits
enum class Reg {
    // GP/'arguement' registers
    X, Y, Z,
    // GP/'answer' registers
    A, B, C,
    // stack (base) pointer [what's so special about it?]
    SP,
    // [special] program counter
    PC,
    // sentinel value [not a valid register]
    COUNT
};

// having a fp flag, can further eliminate unneeded op codes
// reg, reg, reg, reg: fppp xooooo 111 222 3334 44-- ---- ----
// reg, reg, reg     : fppp xooooo 111 222 333- ---- ---- ----
// reg, reg, immd    : fppp xooooo 111 222 iiii-iiii iiii-iiii

/*
# set aside 16bytes*80 for box comps
# set aside 8bytes*16 for pow comps
# set aside 8bytes
struct { fixed x[4]; } boxes[80]; //
struct { fixed pow;  } pows[16] ; //
struct { fixed hp;   } hps[16]  ; //
union { char B; struct { char b0:1; ... }; } BitBytes;
BitBytes box_use[80/8]; // 10 bytes
BitBytes pow_use[16/8]; // 2 bytes
BitBytes hp_use [16/8]; // 2 bytes

find_unused(BitBytes * bb, unsigned len);

 */

// 5 bits (back to)
enum class OpCode : UInt32 {
    // R-type type indifferent split by pf (1-bit) (integer only)
    // PLUS, MINUS, AND, XOR, OR, ROTATE
    PLUS      , // two args, one answer
                // "plus x y a"
                // "plus x immd"
    MINUS     , // two args, one answer
                // "minus x y a"
                // "minus x y immd"
                // "sub x y a"
    AND       , // bitwise and
                // This form applies to all logic operations
                // "and x y a"
                // "and x immd" only LSB
    XOR        , // bitwise exclusive or
    OR         , // bitwise or
    ROTATE     , // negative -> left
                 // positive -> right
                 // "rotate x  4"
                 // "rotate y -6"
                 // "rotate x  a"
    // R-type split by is_fp (1-bit) and pf (1-bit)
    // TIMES, DIVIDE, MODULUS, COMP
    TIMES     , // two args, one answer
                // "times x y a"
                // "times x immd"
    DIVIDE    , // two args, one answer (mod to flags)
                // if divide by zero, flags == denominator
                // "divmod x y a b"
                // "div x y a"
                // "div x immd"
    MODULUS   ,
    COMP      , // do all (>, <, ==, !=) (then use XOR + NON_ZERO)
                // two args, one answer
                // "cmp x y a"
    // M-type (2bits for pf, 0 bits for is_fp) (set types)
    // SET, SAVE, LOAD
    // loading/saving (2 ['heavy'])
    SET       ,
    SAVE      , // two regs, one immd
                // "save x a" saves x to address a
    LOAD      , // two regs, one immd
                // "load x a" loads x from address a
                // "load x immd"
                // "load x a immd
                // "load x" load contents stored at address x, into register x
    // J-type (reducable to 0-bits for "pf")
    // SKIP (make default immd 0b1111 ^ NOT_EQUAL)
    SKIP      , // one arg , conditional, if flags is zero
                // "skip x"
                // "skip x immd" immediate acts as a bit mask here
    // "O"-types (0 bits for "pf") (odd-out types)
    // CALL, NOT
    NOT        , // bitwise complement
                 // "not x a"
    // for SYSTEM_CALL:
    // I can reduce the instruction set size even further by defining, as all
    // ISAs do, function call conventions
    // therefore:
    // answer    registers are: a, b, c
    // parameter registers are: x, y, z
    // saving model: callee saves everything to stack
    // other parameters and answers that cannot fit into the three registers
    // go unto the program stack like so:
    //
    // +--------------------------+
    // | callee's stack frane     |
    // +--------------------------+ <---+--- SP
    // | callee parameter data    |     |
    // +--------------------------+     +--- size is constant
    // | callee answer data       |     |
    // +--------------------------+ <---+
    // | old stack pointer value  |
    // +--------------------------+
    // |                          |
    // | previous function's data |
    // |                          |
    // +--------------------------+
    // registers SP and PC are neither (obviously)
    SYSTEM_CALL,
    COUNT        // sentinal value [not a valid op code]
};

constexpr const int COMP_EQUAL_MASK        = (1 << 0);
constexpr const int COMP_LESS_THAN_MASK    = (1 << 1);
constexpr const int COMP_GREATER_THAN_MASK = (1 << 2);
constexpr const int COMP_NOT_EQUAL_MASK    = (1 << 3);

enum class SystemCallValue {
    // system calls ignore paramform, and will only read the immediate
    // this will provide a space of ~32,000 possible functions
    // this enumeration defines constants that map to thier respective function
    // calls
    // system calls are "calls to the hardware"
    // Erfindung needs a GPU of sorts to display
    // it also needs system calls to read user input
    UPLOAD_SPRITE , // parameters: x: width, y: height, z: address
                    // answers   : a: index for sprite
    UNLOAD_SPRITE , // parameters: x: index for sprite
    DRAW_SPRITE   , // arguments: x: x pos, y: y pox, z: index for sprite
    SCREEN_CLEAR  , // no arguments
    WAIT_FOR_FRAME, // wait until the end of the frame
    READ_INPUT    ,
    SYSTEM_CALL_COUNT // sentinal value
};

enum class ParamForm {
    REG_REG_REG,
    REG_REG_IMMD,
    REG_REG,
    REG_IMMD,
    REG,
    NO_PARAMS,
    INVALID_PARAMS
};

// psuedo instructions:
// any non-hardware instruction that can be formed from a combination of one or
// more hardware instructions.
// PUSH r0
// - SAVE r0 [SP 0]
// - ADD  SP 1 SP
// POP  r0
// - MINUS SP 1 SP
// - LOAD  r0 [SP 0]
// CALL immd
// - PUSH PC
// - JUMP immd
// RET
// - POP PC
// want to load full 32bits?
// we can make another
// Assembly code:
// SET FULL n.m r0
// internal asm:
// SET_IMMD (INT)n r0
// OR       r0     (ABS)m r0
// pseudo instructions:
// any "instruction" which assembles into another real one
// SET_FP r0 240.5
// - SET_IMMD r0 [hex representation of 240.5]
//
// encoding data, uses a "data" directive followed by encoding, then squares
// containing the data (whitespace seperated, each "pack" must be divisible by
// 32bits)
//
// data base64 [ ... ]
//
// data binary [ # binary is quite special
//               # there are many ways to accomplish the encoding
//               # zero characters : 0, O, _
//               # one  characters : 1, X,
//               ... ]
//
// data fpnum [ # series of fixed point numbers
//              ... ]
using RegisterPack = std::array<UInt32, 8>;
using MemorySpace  = std::array<UInt32, 65536/sizeof(UInt32)>;

// high level type alaises
using DebuggerInstToLineMap = std::vector<std::size_t>;

template <typename Base, typename Other>
struct OrCommutative {
    friend Base operator | (const Base & lhs, Other rhs)
        { auto rv(lhs); rv |= rhs; return rv; }
    friend Base operator | (Other lhs, const Base & rhs)
        { auto rv(rhs); rv |= lhs; return rv; }
};

struct ImmdConst;
class Immd;
class RegParamPack;
class FixedPointFlag;

// building instructions has fairly strict semantics
// if these semantics are not enforced, it could result in mysterious bugs that
// take forever to find
// so let's have the compiler help us out here
//
// classes happen to be the only way to enforce them
// this is NOT intended to be OOP
// (as serielize/deserielize will reveal)

class Inst : OrCommutative<Inst, OpCode>, OrCommutative<Inst, Immd>,
             OrCommutative<Inst, RegParamPack>,
             OrCommutative<Inst, FixedPointFlag>
{
public:
    friend Inst deserialize(UInt32);
    friend UInt32 serialize(Inst);
    friend bool decode_is_fixed_point_flag_set(Inst);

    Inst(): v(0) {}
    explicit Inst(OpCode);
    explicit Inst(Immd);
    explicit Inst(RegParamPack);
    explicit Inst(FixedPointFlag);

    Inst & operator |= (OpCode);
    Inst & operator |= (Immd);
    Inst & operator |= (RegParamPack);
    Inst & operator |= (FixedPointFlag);
    Inst & operator |= (Inst);

    bool operator == (const Inst & rhs) const
        { return v == rhs.v; }

    bool operator != (const Inst & rhs) const
        { return v != rhs.v; }

private:
    explicit Inst(UInt32 v_): v(v_) {}
    UInt32 v;
};

inline Inst operator | (Inst lhs, Inst rhs) { return lhs |= rhs; }

class Immd {
public:
    Immd(): v(0) {}
private:
    friend class Inst;
    friend struct ImmdConst;
    friend Immd encode_immd_int(int immd);
    friend Immd encode_immd_fp(double d);

    constexpr explicit Immd(UInt32 v): v(v) {}
    UInt32 v;
};

class RegParamPack {
    friend class Inst;
    explicit RegParamPack(UInt32 bits): v(bits) {}

    RegParamPack(Reg r0): v(UInt32(r0) << 19) {}
    RegParamPack(Reg r0, Reg r1): RegParamPack(r0) { v |= (UInt32(r1) << 16); }
    RegParamPack(Reg r0, Reg r1, Reg r2):
        RegParamPack(r0, r1) { v |= (UInt32(r2) << 13); }

    friend RegParamPack encode_reg(Reg r0);

    friend RegParamPack encode_reg_reg(Reg r0, Reg r1);

    friend RegParamPack encode_reg_reg_reg(Reg r0, Reg r1, Reg r2);

    UInt32 v;
};

class FixedPointFlag {
    friend class Inst;
    friend FixedPointFlag encode_set_is_fixed_point_flag();
    FixedPointFlag(UInt32 v_): v(v_) {}
    UInt32 v;
};

struct ImmdConst { // can only friend "class-likes" AFAIK
    constexpr static const Immd COMP_EQUAL_MASK =
        Immd(::erfin::COMP_EQUAL_MASK);
    constexpr static const Immd COMP_NOT_EQUAL_MASK =
        Immd(::erfin::COMP_NOT_EQUAL_MASK);
    constexpr static const Immd COMP_LESS_THAN_MASK =
        Immd(::erfin::COMP_LESS_THAN_MASK);
    constexpr static const Immd COMP_GREATER_THAN_MASK =
        Immd(::erfin::COMP_GREATER_THAN_MASK);
    constexpr static const Immd COMP_LESS_THAN_OR_EQUAL_MASK =
        Immd(::erfin::COMP_LESS_THAN_MASK | ::erfin::COMP_EQUAL_MASK);
    constexpr static const Immd COMP_GREATER_THAN_OR_EQUAL_MASK =
        Immd(::erfin::COMP_GREATER_THAN_MASK | ::erfin::COMP_EQUAL_MASK);
};

inline Inst deserialize(UInt32 v) { return Inst(v); }

inline UInt32 serialize(Inst i) { return i.v; }

// "wholesale" encoding functions

Inst encode(OpCode op, Reg r0);
Inst encode(OpCode op, Reg r0, Reg r1);
Inst encode(OpCode op, Reg r0, Reg r1, Reg r2);
Inst encode(OpCode op, Reg r0, Immd i);
Inst encode(OpCode op, Reg r0, Reg r1, Immd i);

//Immd operator | (const Immd & lhs, unsigned rhs);
//Immd operator | (unsigned lhs, const Immd & rhs);
/*
Inst encode_op(OpCode op);

Inst encode_param_form(ParamForm pf);
*/
Inst encode_op_with_pf(OpCode op, ParamForm pf);

// for ParamForm: REG
RegParamPack encode_reg(Reg r0);

// for ParamForm: REG_REG
RegParamPack encode_reg_reg(Reg r0, Reg r1);

// for ParamForm: REG_REG_REG
RegParamPack encode_reg_reg_reg(Reg r0, Reg r1, Reg r2);

Immd encode_immd_int(int immd);

Immd encode_immd_fp(double d);

FixedPointFlag encode_set_is_fixed_point_flag();

// helpers for vm/testing the assembler
Reg decode_reg0(Inst inst);
Reg decode_reg1(Inst inst);
Reg decode_reg2(Inst inst);

OpCode decode_op_code(Inst inst);

ParamForm decode_param_form(Inst inst);

int decode_immd_as_int(Inst inst);

UInt32 decode_immd_as_fp(Inst inst);

bool decode_is_fixed_point_flag_set(Inst i);

const char * register_to_string(Reg r);

} // end of erfin namespace

#endif
