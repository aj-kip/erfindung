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

/******************************************************************************

This document defines Erfindung ISA/interpreter and acts as the manual.

Target features:
 - RISC + some bells and wistles
 - assemble to C++ and dynamically executable byte code -> selved
 - parts of the virtual machine can operate independantly from each other
 - You can run programs using only the memory space and CPU
 - You can upload instructions to the APU or GPU and expect the objects
  to produce proper output and be well behaved.

Erfindung Assembly Language
---------------------------

### Introduction
Erfindung is designed to handle two different types of numbers:
 - unsigned 32 bit integers (also used for pointers)
 - two's complement 32 bit fixed point real numbers

Each instruction is strictly 32bits
There are 8 32bit registers.


Erfindung has instructions that can handle fixed point math

Erfindung assmebly language: is a superset of the provided hardware
instructions.
Terms:
 - Hardware instruction: an instruction defined by the ISA
 - psuedo instruction: an instruction NOT defined by the ISA, but can be
formed from a combination of one or more hardware instructions.

Complete Memory Map
-------------------

+============================+============================+
| Region                     | Address                    |
+============================+============================+
| Device I/O                 | 0x0000 0000 - 0x0000 0010  |
+----------------------------+----------------------------+
| Program Code               | 0x1??? ????                |
+----------------------------+----------------------------+
| Free Memory for Stack/Heap | 0x1??? ???? + program size |
+----------------------------+----------------------------+

Memory Mapped I/O Devices
-------------------------

### Stop Space/Reserved null (Address 0x0000 0000)
+----------+
|0       31|
+----------+
|          |
+----------+

### GPU (Addresses 0x0000 0001 - 0x0000 0002)
+---------------------------+--------------------+
|0                        31|32                63|
+---------------------------+--------------------+
| write-only command stream | command output ROM |
+---------------------------+--------------------+

Command Indentities
Commands are uploaded to the GPU, first with the command indentity
(a special ISA defined number), followed by its parameters (assume each are
32bit integers, unless specified otherwise).

UPLOAD_SPRITE
 - parameters: width, height, address
 - answers   : (in output ROM, available after write) index for sprite

UNLOAD_SPRITE
 - parameters: index for sprite

DRAW_SPRITE
 - parameters: x pos, y pox, index for sprite

SCREEN_CLEAR
 - takes no parameters

### APU (Addresses 0x0000 0003 - 0x0000 0004)
+---------------------------+--------------+
|0                        31|32          63|
+---------------------------+--------------+
| write-only command stream | reserved ROM |
+---------------------------+--------------+

### Timer (Addresses 0x0000 0005 - 0x0000 0006)
+-----+--------------------------+-------------------------+
|0  30|31                      31|32                     63|
+-----+--------------------------+-------------------------+
| ... | wait & sync (write-only) | query duration (fp/ROM) |
+-----+--------------------------+-------------------------+

### RNG (Address 0x0000 0007)
+---------------------+
|0                  31|
+---------------------+
| random number (ROM) |
+---------------------+

### Controller (Address 0x0000 0008)
All are ROM
+----+------+------+-------+---+---+-------+-----+
|0  0|1    1|2    2|3     3|4 4|5 5|6     6|7  31|
+----+------+------+-------+---+---+-------+-----+
| UP | DOWN | LEFT | RIGHT | A | B | START | ... |
+----+------+------+-------+---+---+-------+-----+

### Power (Address 0x0000 0009)
+-----+------------+
|0  30|31        31|
+-----+------------+
| ... | is_powered |
+-----+------------+

### Flip to zero to HALT the machine

Bus Error Data (Address 0x0000 0x000A)
+----------------+
|0             31|
+----------------+
| Bus Error Code |
+----------------+

Bus Errors:
 - no error
 - write on ROM
 - read on write-only
 - queue overflow
 - invalid address access

### Addresses (0x0000 000B - 0x0000 0010)
Reserved

******************************************************************************/

#ifndef MACRO_HEADER_GUARD_ERFIDEFS_HPP
#define MACRO_HEADER_GUARD_ERFIDEFS_HPP

#include <cstdint>
#include <array>
#include <vector>

namespace erfin {

using UInt8  = uint8_t ;
using UInt16 = uint16_t;
using UInt64 = uint64_t;
using  Int32 =  int32_t;
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

// instruction bit break down
// OpCode
// ooooo--- -------- -------- --------
// OpCode determines instruction type
// and further layouts depend on this type
// R-type (both indifferent and split)
// ooooo-fp 000-111- 222----- --------
// ooooo-fp 000-111- iiiiiiii iiiiiiii
//
// M-type (2 bits for parameter form, 3 are valid, 4 for set)
// ooooo-pp 000-111- iiiiiiii iiiiiiii; set: (1 for fixed point, 1 for integer)
//                                      load/save: immds are integer offsets
// ooooo-pp 000-111- -------- --------
// ooooo-pp 000----- iiiiiiii iiiiiiii
//
// "Set" type (a seperate handler for set ops, 2 bits for pf)
// ooooo-fp 000-111- -------- -------- // "2" here
// ooooo-fp 000----- iiiiiiii iiiiiiii // 2 here
//
// J-types
// ooooo--p 000----- -------- -------- // the fp bit doesn't change anything
// ooooo--p 000----- iiiiiiii iiiiiiii
//
// F-types
// ooooo--- 000----- iiiiiiii iiiiiiii (integer immd only)
//

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
enum class OpCode {
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
    CALL      , // special instruction using sp
    // F-types (0 bits for "pf") (odd-out types)
    // CALL, NOT
    NOT        , // bitwise complement
                 // "not x a"
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
    // | callee's stack frame     |
    // +--------------------------+
    // | old stack pointer value  | <---+--- SP
    // +--------------------------+     |
    // | callee parameter data    |     |
    // +--------------------------+     +--- size is "constant"
    // | callee answer data       |     |
    // +--------------------------+     |
    // |                          | <---+
    // | previous function's data |
    // |                          |
    // +--------------------------+
    // registers SP and PC are neither (obviously)
    COUNT        // sentinal value [not a valid op code]
};

constexpr const int COMP_EQUAL_MASK        = (1 << 0);
constexpr const int COMP_LESS_THAN_MASK    = (1 << 1);
constexpr const int COMP_GREATER_THAN_MASK = (1 << 2);
constexpr const int COMP_NOT_EQUAL_MASK    = (1 << 3);

namespace device_addresses {
    // register data is stored internally as UInt32, bit masking signed
    // integers, I'm not sure if that's problematic
    // perhaps it would be safe to make them unsigned

    // these are all conviently negative n.-
    constexpr const UInt32 RESERVED_NULL           = 0x80000000;
    constexpr const UInt32 GPU_INPUT_STREAM        = 0x80000001;
    constexpr const UInt32 GPU_RESPONSE            = 0x80000002;
    constexpr const UInt32 APU_INPUT_STREAM        = 0x80000003;
    constexpr const UInt32 TIMER_WAIT_AND_SYNC     = 0x80000004;
    constexpr const UInt32 TIMER_QUERY_SYNC_ET     = 0x80000005;
    constexpr const UInt32 RANDOM_NUMBER_GENERATOR = 0x80000006;
    constexpr const UInt32 READ_CONTROLLER         = 0x80000007;
    constexpr const UInt32 HALT_SIGNAL             = 0x80000008;
    constexpr const UInt32 BUS_ERROR               = 0x80000009;
    constexpr const UInt32 DEVICE_ADDRESS_MASK     = 0x80000000;

    //! @return returns INVALID_DEVICE_ADDRESS pointer if the address is invalid
    const char * to_string(UInt32);
    bool is_device_address(UInt32);

} // end of device_addresses namespace

enum class ParamForm {
    REG_REG_REG,
    REG_REG_IMMD,
    REG_REG,
    REG_IMMD,
    REG,
    IMMD,
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
// data binary [ # binary is quite special
//               # there are many ways to accomplish the encoding
//               # zero characters : 0, O, _
//               # one  characters : 1, X,
//               ... ]
//
//
// data numbers [ # series of numbers either integer or fixed point
//                # bases 10 or 16
//                # numbers seperated by whitespace
//                ... ]
using RegisterPack = std::array<UInt32, 8>;
constexpr const unsigned MEMORY_CAPACITY = 65536;
using MemorySpace  = std::array<UInt32, MEMORY_CAPACITY / sizeof(UInt32)>;

// high level type alaises
using DebuggerInstToLineMap = std::vector<std::size_t>;

template <typename Base, typename Other>
struct OrCommutative {
    friend Base operator | (Base lhs, Other rhs)
        { auto rv(lhs); rv |= rhs; return rv; }
    friend Base operator | (Other lhs, Base rhs)
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

class Inst :
    OrCommutative<Inst, OpCode>,
    OrCommutative<Inst, Immd>,
    OrCommutative<Inst, RegParamPack>,
    OrCommutative<Inst, FixedPointFlag>
{
public:
    friend class InstAttorney;
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

using ProgramData = std::vector<Inst>;

class Immd {
public:
    Immd(): v(0) {}
    bool operator == (const Immd & rhs) const
        { return v == rhs.v; }
    bool operator != (const Immd & rhs) const
        { return v != rhs.v; }
private:
    friend class Inst;
    friend struct ImmdConst;
    friend Immd encode_immd_int(int immd);
    friend Immd encode_immd_fp(double d);
    friend Immd encode_immd_addr(UInt32 addr);

    constexpr explicit Immd(UInt32 v): v(v) {}
    UInt32 v;
};

class RegParamPack {
public:
    RegParamPack(): v(0) {}

private:
    constexpr static const int REG0_POS = 22;
    constexpr static const int REG1_POS = 18;
    constexpr static const int REG2_POS = 14;

    friend class Inst;
    explicit RegParamPack(UInt32 bits): v(bits) {}

    explicit RegParamPack(Reg r0): v(UInt32(r0) << REG0_POS) {}
    RegParamPack(Reg r0, Reg r1): RegParamPack(r0)
        { v |= (UInt32(r1) << REG1_POS); }
    RegParamPack(Reg r0, Reg r1, Reg r2):
        RegParamPack(r0, r1) { v |= (UInt32(r2) << REG2_POS); }

    friend RegParamPack encode_reg(Reg r0);

    friend RegParamPack encode_reg_reg(Reg r0, Reg r1);

    friend RegParamPack encode_reg_reg_reg(Reg r0, Reg r1, Reg r2);

    friend Reg decode_reg0(Inst inst);
    friend Reg decode_reg1(Inst inst);
    friend Reg decode_reg2(Inst inst);

    UInt32 v;
};

class FixedPointFlag {
    friend class Inst;
    friend FixedPointFlag encode_set_is_fixed_point_flag();
    FixedPointFlag(UInt32 v_): v(v_) {}
    UInt32 v;
};

enum class RTypeParamForm {
    _3R_INT     ,
    _2R_IMMD_INT,
    _3R_FP      , // Note: Fixed point must be the msb!
    _2R_IMMD_FP
};

enum class MTypeParamForm {
    _2R_INT , // integers are offsets
    _2R     ,
    _1R_INT ,
    _INVALID
};

enum class SetTypeParamForm {
    _2R_INTVER,
    _1R_INT   ,
    _2R_FPVER , // Note: Fixed point must be the msb!
    _1R_FP
};

// problem is, I can't have two constants with the same value using enums
enum class JTypeParamForm {
    // style: all or none?
    _1R     = 0,
    _1R_INT_FOR_JUMP = 1,
    _IMMD_FOR_CALL   = _1R_INT_FOR_JUMP
};

inline Inst operator | (Inst lhs, Inst rhs) { return lhs |= rhs; }

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

Inst encode_op_with_pf(OpCode op, ParamForm pf);

// for ParamForm: REG
RegParamPack encode_reg(Reg r0);

// for ParamForm: REG_REG
RegParamPack encode_reg_reg(Reg r0, Reg r1);

// for ParamForm: REG_REG_REG
RegParamPack encode_reg_reg_reg(Reg r0, Reg r1, Reg r2);

Immd encode_immd_int(int immd);

Immd encode_immd_addr(UInt32 addr);

Immd encode_immd_fp(double d);

FixedPointFlag encode_set_is_fixed_point_flag();

// helpers for vm/testing the assembler
Reg decode_reg0(Inst inst);
Reg decode_reg1(Inst inst);
Reg decode_reg2(Inst inst);

OpCode decode_op_code(Inst inst);

RTypeParamForm   decode_r_type_pf(Inst i);
MTypeParamForm   decode_m_type_pf(Inst i);
SetTypeParamForm decode_s_type_pf(Inst i);
JTypeParamForm   decode_j_type_pf(Inst i);

Int32 decode_immd_as_int(Inst inst);

UInt32 decode_immd_as_addr(Inst inst);

UInt32 decode_immd_as_fp(Inst inst);

bool decode_is_fixed_point_flag_set(Inst i);

const char * register_to_string(Reg r);

void run_encode_decode_tests();

struct ConsolePack;

// ---------------------- APU Constants/utility functions ---------------------

enum class Channel {
    TRIANGLE ,
    PULSE_ONE,
    PULSE_TWO,
    NOISE    ,
    COUNT
};

enum class ApuInstructionType {
    NOTE ,
    TEMPO,
    DUTY_CYCLE_WINDOW
};

enum class DutyCycleOption {
    FULL_WAVE,
    ONE_HALF,
    ONE_THIRD,
    ONE_QUARTER
};

bool is_valid_value(const ApuInstructionType it);

bool is_valid_value(Channel c);

bool is_valid_value(DutyCycleOption it);

// ---------------------- GPU Constants/utility functions ---------------------

namespace gpu_enum_types {

enum GpuOpCode_e {
    UPLOAD,
    DRAW  ,
    CLEAR ,
};

} // end of gpu_enum_types namespace

// smallest possible sprite
constexpr const int MINI_SPRITE_BIT_COUNT = 64; // 8x8

using GpuOpCode = gpu_enum_types::GpuOpCode_e;

bool is_valid_gpu_op_code(GpuOpCode) noexcept;

inline bool is_valid_gpu_op_code(UInt32 code) noexcept
    { return is_valid_gpu_op_code(static_cast<GpuOpCode>(code)); }

int parameters_per_instruction(GpuOpCode code);

} // end of erfin namespace

#endif
