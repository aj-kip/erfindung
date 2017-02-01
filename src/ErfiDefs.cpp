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

namespace {

using Error = std::runtime_error;

} // end of <anonymous> namespace

namespace erfin {

constexpr /* static */ const Immd ImmdConst::COMP_EQUAL_MASK;
constexpr /* static */ const Immd ImmdConst::COMP_NOT_EQUAL_MASK;
constexpr /* static */ const Immd ImmdConst::COMP_LESS_THAN_MASK;
constexpr /* static */ const Immd ImmdConst::COMP_GREATER_THAN_MASK;
constexpr /* static */ const Immd ImmdConst::COMP_LESS_THAN_OR_EQUAL_MASK;
constexpr /* static */ const Immd ImmdConst::COMP_GREATER_THAN_OR_EQUAL_MASK;

Inst encode_op(OpCode op) { return deserialize(UInt32(op) << 22); }

Inst encode_param_form(ParamForm pf)
    { return deserialize(UInt32(pf) << 28); /* at most 3 bits */ }

Inst encode_op_with_pf(OpCode op, ParamForm pf) {
    return deserialize((UInt32(pf) << 28) | (UInt32(op) << 22));
}

RegParamPack encode_reg(Reg r0)
    { return RegParamPack(r0); }

RegParamPack encode_reg_reg(Reg r0, Reg r1)
    { return RegParamPack(r0, r1); }

RegParamPack encode_reg_reg_reg(Reg r0, Reg r1, Reg r2)
    { return RegParamPack(r0, r1, r2); }

Immd encode_immd_int(int i)
    { return Immd(i < 0 ? 0x8000u | encode_immd_int(-i).v : i & 0x7FFF); }

Immd encode_immd_fp(double d) {
    UInt32 fullwidth = to_fixed_point(d);
    // we want a 9/6 fixed point number (+ one bit for sign)
    UInt32 sign_part = (fullwidth & 0xF0000000u) >> 16u;
    // full width is a 15/16 fixed point number
    UInt32 partial = (fullwidth >> 10u) & 0x7FFFu;
    // make sure we're not losing any of the integer part
    if ((fullwidth >> 16u) & ~0x1FFu)
        throw Error("Value too large to be encoded in a 9/6 fixed point number.");
    return Immd(sign_part | partial | serialize(Inst(encode_set_is_fixed_point_flag())));
}

FixedPointFlag encode_set_is_fixed_point_flag()
    { return FixedPointFlag(0x80000000); }

// helpers for vm/testing the assembler
Reg decode_reg0(Inst inst) { return Reg((serialize(inst) >> 19) & 0x7); }
Reg decode_reg1(Inst inst) { return Reg((serialize(inst) >> 16) & 0x7); }
Reg decode_reg2(Inst inst) { return Reg((serialize(inst) >> 13) & 0x7); }

OpCode decode_op_code(Inst inst) { return OpCode((serialize(inst) >> 22u) & 0x1F); }

ParamForm decode_param_form(Inst inst)
    { return ParamForm((serialize(inst) >> 28) & 0x7); }

int decode_immd_as_int(Inst inst) {
    return ((serialize(inst) & 0x8000) ? -1 : 1) *
            int(serialize(inst) & 0x7FFF);
}

UInt32 decode_immd_as_fp(Inst inst) {
    UInt32 rv = UInt32(decode_immd_as_int(inst));
    UInt32 significand = (rv & 0x7FFF) << 10u;
    UInt32 sign_part = (rv & 0x8000) << 16u;
    return sign_part | significand;
}

bool decode_is_fixed_point_flag_set(Inst i)
    { return (i.v & 0x80000000) != 0; }

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

/* explicit */ Inst::Inst(OpCode o): v(0) { operator |= (o); }
/* explicit */ Inst::Inst(Immd i): v(0) { operator |= (i); }
/* explicit */ Inst::Inst(RegParamPack rpp) { operator |= (rpp); }
/* explicit */ Inst::Inst(FixedPointFlag fpf) { operator |= (fpf); }

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
    return encode_param_form(erfin::ParamForm::REG) |
           encode_op(op) | encode_reg(r0);
}

Inst encode(OpCode op, Reg r0, Reg r1) {
    return encode_param_form(erfin::ParamForm::REG_REG) |
           encode_op(op) | encode_reg_reg(r0, r1);
}

Inst encode(OpCode op, Reg r0, Reg r1, Reg r2) {
    return encode_param_form(erfin::ParamForm::REG_REG_REG) |
           encode_op(op) | encode_reg_reg_reg(r0, r1, r2);
}

Inst encode(OpCode op, Reg r0, Immd i) {
    return encode_param_form(erfin::ParamForm::REG_IMMD) |
           encode_op(op) | encode_reg(r0) | i;
}

Inst encode(OpCode op, Reg r0, Reg r1, Immd i) {
    return encode_param_form(erfin::ParamForm::REG_REG_IMMD) |
           encode_op(op) | encode_reg_reg(r0, r1) | i;
}

} // end of erfin namespace
