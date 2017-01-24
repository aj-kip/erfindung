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
#include "ErfiError.hpp"

#include <vector>
#include <limits>
#include <string>

namespace erfin {

class Assembler {
public:
    enum SuffixAssumption {
        // -int, -fp must be set explicitly
        NO_ASSUMPTION = 0,
        // only -int must be set explicitly
        USING_FP  = 1 << 1,
        // only -fp must be set explicitly
        USING_INT = 1 << 2
    };

    using ProgramData = std::vector<Inst>;

    static constexpr const std::size_t INVALID_LINE_NUMBER = std::size_t(-1);

    Assembler() {}

    void assemble_from_file(const char * file);

    void assemble_from_string(const std::string & source);

    const ProgramData & program_data() const;

    /**
     *  @note also clears error information
     *  @param other
     */
    void swap_out_program_data(ProgramData & other);

    void swap(Assembler & asmr);

    std::size_t translate_to_line_number
        (std::size_t instruction_address) const noexcept;

    static void run_tests();

private:
    ProgramData m_program;

    // debugging erfi program info
    std::vector<std::size_t> m_inst_to_line_map;
};

// "wholesale" encoding functions

Inst encode(OpCode op, Reg r0);
Inst encode(OpCode op, Reg r0, Reg r1);
Inst encode(OpCode op, Reg r0, Reg r1, Reg r2);
Inst encode(OpCode op, Reg r0, Reg r1, Reg r2, Reg r3);

Inst encode(OpCode op, Reg r0, UInt32 i);
Inst encode(OpCode op, Reg r0, Reg r1, UInt32 i);

namespace with_int {

Inst encode(OpCode op, Reg r0, int i);
Inst encode(OpCode op, Reg r0, Reg r1, int i);

}

namespace with_fp {

Inst encode(OpCode op, Reg r0, double d);
Inst encode(OpCode op, Reg r0, Reg r1, double d);

}

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
    { return (immd < 0 ? 0x8000 | encode_immd(-immd) : (immd & 0x7FFF)); }

UInt32 encode_immd(double d);

inline UInt32 encode_set_is_fixed_point_flag() { return 0x80000000; }

// helpers for vm/testing the assembler
inline Reg decode_reg0(Inst inst) { return Reg((inst >> 19) & 0x7); }
inline Reg decode_reg1(Inst inst) { return Reg((inst >> 16) & 0x7); }
inline Reg decode_reg2(Inst inst) { return Reg((inst >> 13) & 0x7); }
inline Reg decode_reg3(Inst inst) { return Reg((inst >> 10) & 0x7); }
inline OpCode decode_op_code(Inst inst) { return OpCode((inst >> 22u) & 0x1F); }

inline ParamForm decode_param_form(Inst inst)
    { return ParamForm((inst >> 28) & 0x7); }

inline int decode_immd_as_int(Inst inst)
    { return ((inst & 0x8000) ? -1 : 1) * int(inst & 0x7FFF); }

inline UInt32 decode_immd_as_fp(Inst inst) {
    UInt32 rv = UInt32(decode_immd_as_int(inst));
    UInt32 significand = (rv & 0x7FFF) << 10u;
    UInt32 sign_part = (rv & 0x8000) << 16u;
    return sign_part | significand;
}

inline bool decode_is_fixed_point_flag_set(UInt32 i)
    { return (i & 0x80000000) != 0; }

const char * register_to_string(Reg r);

} // end of erfin namespace

#endif
