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

namespace erfin {

using UInt8  = uint8_t;
using UInt16 = uint16_t;
using UInt64 = uint64_t;
using UInt32 = uint32_t;
using Inst   = UInt32;

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
namespace enum_types {

// 32bit fixed point numbers, shhh-hhhh hhhh-hhhh llll-llll llll-llll
// 3 bits
enum Reg_e {
    // GP/'arguement' registers
    REG_X ,
    REG_Y ,
    REG_Z ,
    // GP/'answer' registers
    REG_A ,
    REG_B ,
    REG_C ,
    // stack (base) pointer
    REG_BP,
    // [special] program counter
    REG_PC,
    // sentinel value [not a valid register]
    REG_COUNT
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
enum OpCode_e : UInt32 {

    // R-type arthemitic operations (4)
    // I will need variations that can handle immediates
    PLUS      , // two args, one answer
                // "plus x y a"
                // "plus x immd"
    MINUS     , // two args, one answer
                // "minus x y a"
                // "minus x y immd"
                // "sub x y a"
    TIMES     , // two args, one answer
                // "times x y a"
                // "times x immd"
    DIVIDE_MOD, // two args, one answer (mod to flags)
                // if divide by zero, flags == denominator
                // "divmod x y a b"
                // "div x y a"
                // "div x immd"
#   if 0
    TIMES_FP  , // "times.fp x y a"
                // "times.fp x immd"
    DIV_MOD_FP, // "divmod.fp x y a"
                // "divmod.fp x immd"
#   endif
    // R-type LOGIC ops (4)
    AND, // bitwise and
         // This form applies to all logic operations
         // "and x y a"
         // "and x immd" only LSB
    XOR, // bitwise exclusive or
    OR , // bitwise or
    NOT, // bitwise complement
         // "not x a"
    ROTATE , // negative -> left
             // positive -> right
             // "rotate x  4"
             // "rotate y -6"
             // "rotate x  a"

    // J-type operations plus one R-type helper (COMP) (4)
    // do all (>, <, ==, !=) (then use XOR + NON_ZERO)
    COMP    , // two args, one answer
              // "cmp x y a"
    SKIP    , // one arg , conditional, if flags is zero
              // "skip x"
              // "skip x immd" immediate acts as a bit mask here
    // jump is now a psuedo instruction
    // "jump x   " -> "set pc x"
    // "jump immd" -> "set pc immd"

    // loading/saving (2 ['heavy'])
    LOAD, // two regs, one immd
          // "load x a" loads x from address a
          // "load x immd"
          // "load x a immd
          // "load x" load contents stored at address x, into register x
    SAVE, // two regs, one immd
          // "save x a" saves x to address a
    SET,
#   if 0
    // I-types setting values from immds
    SET_INT , // one reg, one IMMD
    SET_FP96, // one reg, one IMMD as a 9/6 bit fixed point (+1 for sign)
    SET_REG , // two regs (r0 = r1)
              // argument deduced "set x 123.3" <- note the decimal
              // "set x a"
#   endif
    // I can reduce the instruction set size even further by defining, as all
    // ISAs do, function call conventions
    // therefore:
    // answer    registers are: a, b, c
    // parameter registers are: x, y, z
    // other parameters and answers that cannot fit into the three registers
    // go unto the program stack like so:
    //
    // +--------------------------+
    // | callee's stack frane     |
    // +--------------------------+ <---+--- BP
    // | callee parameter data    |     |
    // +--------------------------+     +--- size is constant
    // | callee answer data       |     |
    // +--------------------------+ <---+
    // | old base pointer value   |
    // +--------------------------+
    // |                          |
    // | previous function's data |
    // |                          |
    // +--------------------------+
    // registers BP and PC are neither (obviously)
    SYSTEM_CALL,

    OPCODE_COUNT // sentinal value [not a valid op code]
};

enum SystemCallValue_e {
    // system calls ignore paramform, and will only read the immediate
    // this will provide a space of ~32,000 possible functions
    // this enumeration defines constants that map to thier respective function
    // calls
    // system calls are "calls to the hardware"
    // Erfindung needs a GPU of sorts to display
    // it also needs system calls to read user input
    UPLOAD_SPRITE , // arguments: x: width, y: height, z: address
                    // answers  : a: index for sprite
    DRAW_SPRITE   , // arguments: x: x pos, y: y pox, z: index for sprite
    SCREEN_FLUSH  , // no arguments
    WAIT_FOR_FRAME, // wait until the end of the frame
    READ_INPUT
};

enum ParamForm_e {
    REG_REG_REG_REG,
    REG_REG_REG,
    REG_REG_IMMD,
    REG_REG,
    REG_IMMD,
    REG,
    NO_PARAMS,
    INVALID_PARAMS
};

} // end of enum_types

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
using OpCode       = enum_types::OpCode_e   ;
using Reg          = enum_types::Reg_e      ;
using ParamForm    = enum_types::ParamForm_e;
using RegisterPack = std::array<UInt32, 8>;
using MemorySpace  = std::array<UInt32, 65536/sizeof(UInt32)>;

template <typename T>
T mag(T t) { return t < T(0) ? -t : t; }


} // end of erfin namespace

#endif
