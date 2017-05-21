/****************************************************************************

    File: ErfiDefs.cpp
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

#include "ErfiDefs.hpp"
#include "FixedPointUtil.hpp"

#include <stdexcept>
#include <limits>

#include <cassert>

namespace {

using Error = std::runtime_error;

constexpr const int OP_CODE_POS = 27;
constexpr const int R_TYPE_PF_POS = 25;
constexpr const int SET_TYPE_PF_POS = R_TYPE_PF_POS;
constexpr const int M_TYPE_PF_POS   = R_TYPE_PF_POS;
constexpr const int J_TYPE_PF_POS   = R_TYPE_PF_POS;

constexpr const unsigned IS_FIXED_POINT_MASK = 0x4000000u;

} // end of <anonymous> namespace

namespace erfin {

constexpr /* static */ const Immd ImmdConst::COMP_EQUAL_MASK;
constexpr /* static */ const Immd ImmdConst::COMP_NOT_EQUAL_MASK;
constexpr /* static */ const Immd ImmdConst::COMP_LESS_THAN_MASK;
constexpr /* static */ const Immd ImmdConst::COMP_GREATER_THAN_MASK;
constexpr /* static */ const Immd ImmdConst::COMP_LESS_THAN_OR_EQUAL_MASK;
constexpr /* static */ const Immd ImmdConst::COMP_GREATER_THAN_OR_EQUAL_MASK;

const char * device_addresses::to_string(int address) {
    switch (address) {
    case RESERVED_NULL          : return "RESERVED_NULL"          ;
    case GPU_INPUT_STREAM       : return "GPU_INPUT_STREAM"       ;
    case GPU_RESPONSE           : return "GPU_RESPONSE"           ;
    case APU_INPUT_STREAM       : return "APU_INPUT_STREAM"       ;
    case TIMER_WAIT_AND_SYNC    : return "TIMER_WAIT_AND_SYNC"    ;
    case TIMER_QUERY_SYNC_ET    : return "TIMER_QUERY_SYNC_ET"    ;
    case RANDOM_NUMBER_GENERATOR: return "RANDOM_NUMBER_GENERATOR";
    case READ_CONTROLLER        : return "READ_CONTROLLER"        ;
    case HALT_SIGNAL            : return "HALT_SIGNAL"            ;
    case BUS_ERROR              : return "BUS_ERROR"              ;
    default: return "<INVALID ADDRESS>";
    }
}


Inst encode_op_with_pf(OpCode op, ParamForm pf) {
    using O  = OpCode;
    using Pf = ParamForm;
    UInt32 rv = UInt32(op) << OP_CODE_POS;
    switch (op) {
    case O::PLUS  : case O::MINUS : case O::AND    :
    case O::XOR   : case O::OR    :
    case O::ROTATE: // R type indifferent
    case O::TIMES : case O::DIVIDE: case O::MODULUS:
    case O::COMP  : // end of R types
        switch (pf) {
        case Pf::REG_REG_REG : return deserialize(rv);
        case Pf::REG_REG_IMMD:
            return deserialize(rv | (1 << R_TYPE_PF_POS));
        default: throw Error("Parameter form invalid for R-type");
        }
    case O::SET: // "set" type, very special
        switch (pf) {
        case Pf::REG_REG : return deserialize(rv);
        case Pf::REG_IMMD: return deserialize(rv | (1 << SET_TYPE_PF_POS));
        default:
            throw Error("Parameter form invalid for \"Set\" type");
        }
    case O::SAVE:
    case O::LOAD: // M-Types
        switch (pf) {
        case Pf::REG_REG_IMMD: return deserialize(rv);
        case Pf::REG_REG:
            return deserialize(rv | (1 << M_TYPE_PF_POS));
        case Pf::REG_IMMD:
            return deserialize(rv | (2 << M_TYPE_PF_POS));
        default: throw Error("Parameter form invalid for M type");
        }
    case O::SKIP:
        switch (pf) {
        case Pf::REG     : return deserialize(rv);
        case Pf::REG_IMMD: return deserialize(rv | (1 << J_TYPE_PF_POS));
        default: throw Error("Parameter form invalid for Skip");
        }
    case O::CALL:
        switch (pf) {
        case Pf::REG : return deserialize(rv);
        case Pf::IMMD: return deserialize(rv | (1 << J_TYPE_PF_POS));
        default: throw Error("Parameter form invalid for Call");
        }
    case O::NOT:
        switch (pf) {
        case Pf::REG_REG: return deserialize(rv);
        default:
            throw Error("Parameter form invalid for Not");
        }
    default: break;
    }
    throw Error("Cannot encode op code pf pair, perhaps invalid values were "
                "given?");
}

RegParamPack encode_reg(Reg r0)
    { return RegParamPack(r0); }

RegParamPack encode_reg_reg(Reg r0, Reg r1)
    { return RegParamPack(r0, r1); }

RegParamPack encode_reg_reg_reg(Reg r0, Reg r1, Reg r2)
    { return RegParamPack(r0, r1, r2); }

Immd encode_immd_int(int i) {
    if (i > std::numeric_limits<int16_t>::max() ||
        i < std::numeric_limits<int16_t>::min())
    {
        throw Error("Cannot store number \"" + std::to_string(i) +
                    "\" in an immediate.");
    }
    if (i < 0) return Immd(0x8000u | encode_immd_int(-(i + 1)).v);
    return Immd(i & 0x7FFF);
}

Immd encode_immd_fp(double d) {
    UInt32 fullwidth = to_fixed_point(d);
    // we want a 9/6 fixed point number (+ one bit for sign)
    UInt32 sign_part = (fullwidth & 0xF0000000u) >> 16u;
    // full width is a 15/16 fixed point number
    UInt32 partial = (fullwidth >> 10u) & 0x7FFFu;
    // make sure we're not losing any of the integer part
    if ((fullwidth >> 16u) & ~0x1FFu)
        throw Error("Value too large to be encoded in a 9/6 fixed point number.");
    auto rv = sign_part | partial;
    if ((0x7FFFu & rv) == 0)
        throw Error("Value too small to be encoded in a 9/6 fixed point number.");
    auto  o = serialize(Inst(encode_set_is_fixed_point_flag()));
    return Immd(rv | o);
}

FixedPointFlag encode_set_is_fixed_point_flag()
    { return FixedPointFlag(IS_FIXED_POINT_MASK); }

// helpers for vm/testing the assembler
Reg decode_reg0(Inst inst)
    { return Reg((serialize(inst) >> RegParamPack::REG0_POS) & 0x7); }
Reg decode_reg1(Inst inst)
    { return Reg((serialize(inst) >> RegParamPack::REG1_POS) & 0x7); }
Reg decode_reg2(Inst inst)
    { return Reg((serialize(inst) >> RegParamPack::REG2_POS) & 0x7); }

OpCode decode_op_code(Inst inst)
    { return OpCode((serialize(inst) >> OP_CODE_POS) & 0x1F); }

RTypeParamForm decode_r_type_pf(Inst i) {
    return RTypeParamForm((serialize(i) >> R_TYPE_PF_POS) & 0x3);
}

MTypeParamForm decode_m_type_pf(Inst i) {
    return MTypeParamForm((serialize(i) >> M_TYPE_PF_POS) & 0x3);
}

SetTypeParamForm decode_s_type_pf(Inst i) {
    return SetTypeParamForm((serialize(i) >> SET_TYPE_PF_POS) & 0x3);
}

JTypeParamForm decode_j_type_pf(Inst i) {
    return JTypeParamForm((serialize(i) >> J_TYPE_PF_POS) & 0x1);
}

int decode_immd_as_int(Inst inst) {
    auto bits = serialize(inst) & 0xFFFF;
    auto is_neg = bits & 0x8000;
    return (is_neg ? -1 : 1)*int(bits & 0x7FFF) + (is_neg ? -1 : 0);
}

UInt32 decode_immd_as_fp(Inst inst) {
    UInt32 rv = UInt32(decode_immd_as_int(inst));
    UInt32 significand = (rv & 0x7FFF) << 10u;
    UInt32 sign_part = (rv & 0x8000) << 16u;
    return sign_part | significand;
}

bool decode_is_fixed_point_flag_set(Inst i)
    { return (i.v & IS_FIXED_POINT_MASK) != 0; }

const char * register_to_string(Reg r) {
    switch (r) {
    case Reg::X : return "x" ;
    case Reg::Y : return "y" ;
    case Reg::Z : return "z" ;
    case Reg::A : return "a" ;
    case Reg::B : return "b" ;
    case Reg::C : return "c" ;
    case Reg::SP: return "bp";
    case Reg::PC: return "pc";
    default: throw Error("Invalid register, cannot convert to a string.");
    }
}

void run_encode_decode_tests() {
    using namespace device_addresses;
    for (int i :{ RESERVED_NULL, GPU_INPUT_STREAM, GPU_RESPONSE,
                  APU_INPUT_STREAM, TIMER_WAIT_AND_SYNC, TIMER_QUERY_SYNC_ET,
                  RANDOM_NUMBER_GENERATOR, READ_CONTROLLER, HALT_SIGNAL,
                  BUS_ERROR                                                   })
    {
        int product = decode_immd_as_int(Inst(encode_immd_int(i)));
        if (i == product) continue;
        throw Error(std::string("Failed to encode \"") + to_string(i)   +
                    "\" expected: " + std::to_string(i) + " produced: " +
                    std::to_string(product)                              );
    }
}

/* explicit */ Inst::Inst(OpCode           o): v(0) { operator |= (o  ); }
/* explicit */ Inst::Inst(Immd             i): v(0) { operator |= (i  ); }
/* explicit */ Inst::Inst(RegParamPack   rpp): v(0) { operator |= (rpp); }
/* explicit */ Inst::Inst(FixedPointFlag fpf): v(0) { operator |= (fpf); }

Inst & Inst::operator |= (OpCode o) {
    v |= UInt32(o);
    return *this;
}

Inst & Inst::operator |= (Immd i) {
    v |= i.v;
    return *this;
}

Inst & Inst::operator |= (RegParamPack rpp) {
    v |= rpp.v;
    return *this;
}

Inst & Inst::operator |= (FixedPointFlag fpf) {
    v |= fpf.v;
    return *this;
}

Inst & Inst::operator |= (Inst i) {
    v |= i.v;
    return *this;
}

Inst encode(OpCode op, Reg r0) {
    return encode_op_with_pf(op, erfin::ParamForm::REG) |
           encode_reg(r0);
}

Inst encode(OpCode op, Reg r0, Reg r1) {
    return encode_op_with_pf(op, erfin::ParamForm::REG_REG) |
           encode_reg_reg(r0, r1);
}

Inst encode(OpCode op, Reg r0, Reg r1, Reg r2) {
    return encode_op_with_pf(op, erfin::ParamForm::REG_REG_REG) |
           encode_reg_reg_reg(r0, r1, r2);
}

Inst encode(OpCode op, Reg r0, Immd i) {
    return encode_op_with_pf(op, erfin::ParamForm::REG_IMMD) |
           encode_reg(r0) | i;
}

Inst encode(OpCode op, Reg r0, Reg r1, Immd i) {
    return encode_op_with_pf(op, erfin::ParamForm::REG_REG_IMMD) |
           encode_reg_reg(r0, r1) | i;
}

// ---------------------- APU Constants/utility functions ---------------------

bool is_valid_value(const ApuInstructionType it) {
    using Ait = ApuInstructionType;
    switch (it) {
    case Ait::NOTE: case Ait::TEMPO: case Ait::DUTY_CYCLE_WINDOW: return true;
    default: return false;
    }
}

bool is_valid_value(Channel c) {
    using Ch = Channel;
    switch (c) {
    case Ch::TRIANGLE: case Ch::PULSE_ONE: case Ch::PULSE_TWO: case Ch::NOISE:
        return true;
    default: return false;
    }
}

bool is_valid_value(DutyCycleOption it) {
    using Dco = DutyCycleOption;
    switch (it) {
    case Dco::ONE_HALF: case Dco::ONE_THIRD: case Dco::ONE_QUARTER:
    case Dco::ONE_FIFTH: return true;
    default: return false;
    }
}

// ---------------------- GPU Constants/utility functions ---------------------

int parameters_per_instruction(GpuOpCode code) {
    using namespace gpu_enum_types;
    switch (code) {
    case UPLOAD: return 3;
    case UNLOAD: return 1;
    case DRAW  : return 3;
    case CLEAR : return 0;
    }
    std::terminate();
}

} // end of erfin namespace
