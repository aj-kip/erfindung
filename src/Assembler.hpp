#ifndef MACRO_HEADER_GUARD_ERFI_ASSEMBLER_HPP
#define MACRO_HEADER_GUARD_ERFI_ASSEMBLER_HPP

#include "ErfiDefs.hpp"
#include <vector>

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

// for ParamForm: REG
inline UInt32 encode_reg(Reg r0) {
    return (UInt32(r0) << 19);
}
// for ParamForm: REG_REG
inline UInt32 encode_reg_reg(Reg r0, Reg r1) {
    return encode_reg(r0) | (UInt32(r1) << 16);
}
// for ParamForm: REG_REG_REG
inline UInt32 encode_reg_reg_reg(Reg r0, Reg r1, Reg r2) {
    return encode_reg_reg(r0, r1) | (UInt32(r2) << 13);
}
// for ParamForm: REG_REG_REG_REG
inline UInt32 encode_reg_reg_reg_reg(Reg r0, Reg r1, Reg r2, Reg r3) {
    return encode_reg_reg_reg(r0, r1, r2) | (UInt32(r3) << 10);
}

inline UInt32 encode_op(OpCode op) { return UInt32(op) << 22; }

} // end of erfin namespace

#endif
