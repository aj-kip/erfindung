/****************************************************************************

    File: make_generic_instructions.cpp
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

#include "make_generic_instructions.hpp"
#include "LineParsingHelpers.hpp"
#include "GetLineProcessingFunction.hpp"

#include "../StringUtil.hpp"

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

using StringCIter = erfin::TokensConstIterator;
using TextProcessState = erfin::TextProcessState;

erfin::Immd deal_with_int_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state);

erfin::Immd deal_with_fp_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state);

void warn_if_rotate_and_assuming_fp
    (TextProcessState & state, erfin::ExtendedParamForm pf, erfin::OpCode op);

bool numeric_assumption_matters_for(erfin::OpCode op_code);

} // end of <anonymous> namespace

namespace erfin {

StringCIter make_generic_logic
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end)
{
    return make_generic_arithemetic(op_code, state, beg, end);
}

StringCIter make_generic_arithemetic
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end)
{
    using namespace erfin;

    static constexpr const char * const fp_int_ambig_msg =
        ": cannot deduce whether a fixed point or integer operation was "
        "meant; the assembler doesn't know which instruction to construct.";

    bool assumption_matters = numeric_assumption_matters_for(op_code);

    Reg ans; // the first register is ALWAYS the answer
    ++beg;
    auto eol = get_eol(beg, end);
    const auto pf = get_lines_param_form(beg, eol);

    warn_if_rotate_and_assuming_fp(state, pf, op_code);
    switch (pf) {
    case XPF_3R   : case XPF_2R: {
        // perfect chance to check if assumptions are ok!
        auto assumpt = state.assumptions();
        if (assumption_matters && !(   assumpt & Assembler::USING_FP
                                    || assumpt & Assembler::USING_INT))
        {
            throw state.make_error(fp_int_ambig_msg);
        }
        }
        MACRO_FALLTHROUGH;
    case XPF_2R_FP: case XPF_2R_INT: case XPF_2R_LABEL:
    case XPF_1R_FP: case XPF_1R_INT: case XPF_1R_LABEL:
        ans = string_to_register(*beg);
        break;
    default:
        throw state.make_error(": unsupported parameters.");
    }

    // explicit immediates should silence this warning!

    Reg a1, a2;
    Inst inst;
    Immd (*handle_immd)(StringCIter, OpCode, TextProcessState &) = nullptr;
    const std::string * label = nullptr;
    auto do_nothing_for_label =
        [](StringCIter, OpCode, TextProcessState &) { return Immd(); };
    static constexpr const int IS_FP = 0, IS_INT = 1, INDETERMINATE = -1;
    int type_indentity = INDETERMINATE;
    switch (pf) {
    case XPF_2R_FP: case XPF_1R_FP:
        handle_immd = deal_with_fp_immd;
        type_indentity = IS_FP;
        break;
    case XPF_2R_INT: case XPF_1R_INT:
        handle_immd = deal_with_int_immd;
        type_indentity = IS_INT;
        break;
    case XPF_2R_LABEL: case XPF_1R_LABEL:
        handle_immd = do_nothing_for_label;
        label = &*(eol - 1);
        type_indentity = IS_INT;
        break;
    default: break;
    }
    if (type_indentity == INDETERMINATE) {
        if (state.assumptions() & Assembler::USING_FP)
            type_indentity = IS_FP;
        else if (state.assumptions() & Assembler::USING_INT)
            type_indentity = IS_INT;
    }

    switch (pf) {
    case XPF_2R: // psuedo instruction
        a1 = ans;
        a2 = string_to_register(*(beg + 1));
        inst = encode(op_code, ans, a1, a2);
        break;
    case XPF_3R:
        a1 = string_to_register(*(beg + 1));
        a2 = string_to_register(*(beg + 2));
        inst = encode(op_code, ans, a1, a2);
        break;
    case XPF_2R_FP: case XPF_2R_INT: case XPF_2R_LABEL:
        a2 = string_to_register(*(beg + 1));
        inst = encode(op_code, ans, a2, handle_immd(eol, op_code, state));
        break;
    case XPF_1R_FP: case XPF_1R_INT: case XPF_1R_LABEL:
        inst = encode(op_code, ans, ans, handle_immd(eol, op_code, state));
        break;
    default: assert(false); break;
    }
    if (type_indentity == IS_FP) {
        inst |= encode_set_is_fixed_point_flag();
    }

    state.add_instruction(inst, label);
    return eol;
}

StringCIter make_generic_memory_access
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end)
{
    using namespace erfin;

    assert(op_code == OpCode::LOAD || op_code == OpCode::SAVE);

    const auto eol = get_eol(++beg, end);
    assert(!get_line_processing_function(Assembler::NO_ASSUMPTIONS, *beg));

    const std::string * label = nullptr;
    Inst inst;

    NumericParseInfo npi;
    const auto pf = get_lines_param_form(beg, eol, &npi);

    if (pf == XPF_1R && op_code == OpCode::SAVE) {
        throw state.make_error(": deference psuedo instruction only available "
                               "for loading.");
    }
    // save one reg and immd argument -> write to address directly

    switch (pf) {
    case XPF_1R: case XPF_2R:
        inst |= encode_op_with_pf(op_code, ParamForm::REG_REG);
        break;
    case XPF_2R_LABEL: case XPF_2R_INT:
        inst |= encode_op_with_pf(op_code, ParamForm::REG_REG_IMMD);
        break;
    case XPF_1R_INT: case XPF_1R_LABEL:
        if (op_code == OpCode::LOAD)
            inst |= encode_op_with_pf(op_code, ParamForm::REG_REG_IMMD);
        else
            inst |= encode_op_with_pf(op_code, ParamForm::REG_IMMD);
        break;
    default:
        throw state.make_error(std::string(": <op_code> does not support ") +
                               extended_param_form_to_string(pf) +
                               " for parameters.");
    }

    if (equal_to_any(pf, XPF_2R_INT, XPF_1R_INT))
        inst |= encode_immd_int(npi.integer);

    if (equal_to_any(pf, XPF_1R_LABEL, XPF_2R_LABEL))
        label = &*(eol - 1);

    Reg reg = string_to_register(*beg);
    if (equal_to_any(pf, XPF_3R, XPF_2R, XPF_2R_INT, XPF_2R_LABEL)) {
        inst |= encode_reg_reg(reg, string_to_register(*(beg + 1)));
    }

    if (equal_to_any(pf, XPF_1R, XPF_1R_INT, XPF_1R_LABEL)) {
        if (op_code == OpCode::LOAD) {
            inst |= encode_reg_reg(reg, reg);
        } else {
            inst |= encode_reg(reg);
        }
    }

    state.add_instruction(inst, label);
    return eol;
}

} // end of erfin namespace

namespace {

bool op_code_supports_fpoint_immd(erfin::OpCode op_code);

bool op_code_supports_integer_immd(erfin::OpCode op_code);

erfin::Immd deal_with_int_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state)
{
    static constexpr const char * const int_unsupported_msg =
        ": instruction does not support integer immediates.";
    if (!op_code_supports_integer_immd(op_code))
        throw state.make_error(int_unsupported_msg);
    int i;
    string_to_number(&(eol - 1)->front(), &(eol - 1)->back() + 1, i);
    return erfin::encode_immd_int(i);
}

erfin::Immd deal_with_fp_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state)
{
    static constexpr const char * const fp_unsupported_msg =
        ": instruction does not support fixed point immediates.";
    if (!op_code_supports_fpoint_immd(op_code))
        throw state.make_error(fp_unsupported_msg);
    double d;
    string_to_number(&(eol - 1)->front(), &(eol - 1)->back() + 1, d);
    return erfin::encode_immd_fp(d);
}

void warn_if_rotate_and_assuming_fp
    (TextProcessState & state, erfin::ExtendedParamForm pf, erfin::OpCode op) {
    using namespace erfin;
    if (op != OpCode::ROTATE) return;
    switch (pf) {
    case XPF_2R_FP: case XPF_2R_INT: case XPF_2R_LABEL:
    case XPF_1R_FP: case XPF_1R_INT: case XPF_1R_LABEL:
        return;
    default: break;
    }
    if (state.assumptions() & Assembler::USING_FP) {
        state.push_warning(": rotate is being used while the fixed point "
                           "assumption is active.");
    }
}

bool numeric_assumption_matters_for(erfin::OpCode op_code) {
    using O = erfin::OpCode;
    switch (op_code) {
    // this does require some impl knowledge...
    case O::PLUS: case O::MINUS: case O::AND: case O::OR: case O::XOR:
    case O::NOT: case O::ROTATE:
        return false;
    default: return true;
    }
}

// ----------------------------------------------------------------------------

bool op_code_supports_fpoint_immd(erfin::OpCode op_code) {
    using O = erfin::OpCode;
    switch (op_code) {
    case O::COMP: case O::DIVIDE: case O::TIMES: case O::PLUS: case O::MINUS:
        return true;
    case O::AND: case O::XOR: case O::OR:
        return false;
    default: assert(false); break;
    }
    std::terminate();
}

bool op_code_supports_integer_immd(erfin::OpCode op_code) {
    using O = erfin::OpCode;
    switch (op_code) {
    case O::PLUS: case O::MINUS: case O::TIMES: case O::AND: case O::XOR:
    case O::OR  : case O::DIVIDE: case O::COMP: case O::MODULUS:
    case O::ROTATE:
        return true;
    default: return false;
    }
}

} // end of <anonymous> namespace
