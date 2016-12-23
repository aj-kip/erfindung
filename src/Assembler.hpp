/****************************************************************************

    File: Assembler.hpp
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

#ifndef MACRO_HEADER_GUARD_ERFI_ASSEMBLER_HPP
#define MACRO_HEADER_GUARD_ERFI_ASSEMBLER_HPP

#include "ErfiDefs.hpp"
#include <vector>
#include <limits>

template <unsigned BASE = 10, typename CharType, typename RealType>
typename std::enable_if<std::is_arithmetic<RealType>::value, bool>::
type /* bool */ string_to_number
    (const CharType * start, const CharType * end, RealType & out);

namespace erfin {

class Assembler {
public:
    enum SuffixAssumption {
        NO_ASSUMPTION = 0,
        MSB_SUFFIX = 1 << 1,
        LSB_SUFFIX = 1 << 2,
        FP_SUFFIX  = 1 << 3,
        INT_SUFFIX = 1 << 4
    };

    using ProgramData = std::vector<Inst>;

    Assembler(): m_assumptions(NO_ASSUMPTION) {}

    void assume(SuffixAssumption assumptions_)
        { m_assumptions = assumptions_; }
    SuffixAssumption assumptions() const
        { return m_assumptions; }
    void assemble_from_file(const char * file);

    void assemble_from_string(const std::string & source);

    /** If non-empty there is an issue with compiling the program
     * @return
     */
    const std::string & error_string() const;

    const ProgramData & program_data() const;

    /**
     *  @note also clears error information
     *  @param other
     */
    void swap_out_program_data(ProgramData & other);

    void swap(Assembler & asmr);

    static void run_tests();

private:
    ProgramData m_program;
    std::string m_error_string;
    SuffixAssumption m_assumptions;
};

// "wholesale" encoding functions

Inst encode(OpCode op, Reg r0);
Inst encode(OpCode op, Reg r0, Reg r1);
Inst encode(OpCode op, Reg r0, Reg r1, Reg r2);
Inst encode(OpCode op, Reg r0, Reg r1, Reg r2, Reg r3);

Inst encode(OpCode op, Reg r0, int i);
Inst encode(OpCode op, Reg r0, Reg r1, int i);
Inst encode(OpCode op, Reg r0, double d);
Inst encode(OpCode op, Reg r0, Reg r1, double d);

inline void load_program_into_memory
    (MemorySpace & memspace, const Assembler::ProgramData & pdata)
{
    if (memspace.size() < pdata.size()) {
        throw std::runtime_error("Program is too large for RAM!");
    }
    UInt32 * beg = &memspace.front();
    for (UInt32 inst : pdata)
        *beg++ = inst;
}

// for ParamForm: REG
inline UInt32 encode_reg(Reg r0)
    { return (UInt32(r0) << 19); }

// for ParamForm: REG_REG
inline UInt32 encode_reg_reg(Reg r0, Reg r1)
    { return encode_reg(r0) | (UInt32(r1) << 16); }

// for ParamForm: REG_REG_REG
inline UInt32 encode_reg_reg_reg(Reg r0, Reg r1, Reg r2)
    { return encode_reg_reg(r0, r1) | (UInt32(r2) << 13); }

// for ParamForm: REG_REG_REG_REG
inline UInt32 encode_reg_reg_reg_reg(Reg r0, Reg r1, Reg r2, Reg r3)
    { return encode_reg_reg_reg(r0, r1, r2) | (UInt32(r3) << 10); }

inline UInt32 encode_op(OpCode op) { return UInt32(op) << 22; }

inline UInt32 encode_param_form(ParamForm pf)
    { return UInt32(pf) << 28; /* at most 3 bits */ }

inline UInt32 encode_immd(int immd)
    { return (immd < 0 ? 0x8000 : 0x0000) | (immd & 0x7FFF); }

UInt32 encode_immd(double d);

// helpers for vm/testing the assembler
inline Reg decode_reg0(Inst inst) { return Reg((inst >> 19) & 0x7); }
inline Reg decode_reg1(Inst inst) { return Reg((inst >> 16) & 0x7); }
inline Reg decode_reg2(Inst inst) { return Reg((inst >> 13) & 0x7); }
inline Reg decode_reg3(Inst inst) { return Reg((inst >> 10) & 0x7); }
inline OpCode decode_op_code(Inst inst) { return OpCode((inst >> 22u) & 0x1F); }

inline ParamForm decode_param_form(Inst inst)
    { return ParamForm((inst >> 28) & 0x7); }

inline int decode_immd_as_int(Inst inst)
    { return int((inst & 0x8000) << 16) | int(inst & 0x7FFF); }

inline UInt32 decode_immd_as_fp(Inst inst) {
    UInt32 rv = UInt32(decode_immd_as_int(inst));
    UInt32 significand = (rv & 0x7FFF) << 10u;
    UInt32 sign_part = (rv & 0x8000) << 16u;
    return sign_part | significand;
}

const char * register_to_string(Reg r);

} // end of erfin namespace

template <unsigned BASE, typename CharType, typename RealType>
typename std::enable_if<std::is_arithmetic<RealType>::value, bool>::
type /* bool */ string_to_number
    (const CharType * start, const CharType * end, RealType & out)
{
    static_assert(BASE <= 16, "Bases for this function can only go as high as 16");
    static_assert(BASE >= 2 , "Smallest available base 2: binary");

    static constexpr bool IS_SIGNED  = std::is_signed<RealType>::value;
    static constexpr bool IS_INTEGER = !std::is_floating_point<RealType>::value;

    const bool IS_NEGATIVE = (*start) == CharType('-');

    // negative numbers cannot be parsed into an unsigned type
    if (!IS_SIGNED && IS_NEGATIVE)
        return false;

    if (IS_NEGATIVE) ++start;

    RealType working = RealType(0), multi = RealType(1);

    // the adder is a one digit number that corresponds to a character
    RealType adder = RealType(0);

    bool found_dot = false;

    // main digit reading loop, iterates characters in the selection in reverse
    do {
        --end;
        switch (*end) {
        case CharType('.'):
            if (found_dot) return false;
            found_dot = true;
            if (IS_INTEGER) {
                if (adder >= RealType(BASE)/RealType(2))
                    working = RealType(1);
                else
                    working = RealType(0);
            } else {
                working /= multi;
            }
            adder = RealType(0);
            multi = RealType(1);
            continue;
        case CharType('0'): case CharType('1'): case CharType('2'):
        case CharType('3'): case CharType('4'): case CharType('5'):
        case CharType('6'): case CharType('7'): case CharType('8'):
        case CharType('9'):
            adder = RealType(*end - CharType('0'));
            break;
        case CharType('a'): case CharType('b'): case CharType('c'):
        case CharType('d'): case CharType('e'): case CharType('f'):
            adder = RealType(*end - 'a' + 10);
            break;
        case CharType('A'): case CharType('B'): case CharType('C'):
        case CharType('D'): case CharType('E'): case CharType('F'):
            adder = RealType(*end - 'A' + 10);
        default: return false;
        }
        if (adder >= RealType(BASE))
            return false;
        // detect overflow
        RealType temp = working + adder*multi;
        if (temp < working) {
            // attempt a recovery... (edge case, min int)
            const RealType MIN_INT = std::numeric_limits<RealType>::min();
            if (IS_NEGATIVE && -working - adder*multi == MIN_INT) {
                out = MIN_INT;
                return true;
            }
            return false;
        }
        multi *= RealType(BASE);
        working = temp;
    }
    while (end != start);

    // we've produced a positive integer, so make the adjustment if needed
    if (IS_NEGATIVE)
        working *= RealType(-1);

    // write to parameter
    out = working;
    return true;
}

#endif
