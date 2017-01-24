/****************************************************************************

    File: GetLineProcessingFunction.cpp
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

#include "CommonDefinitions.hpp"
#include "GetLineProcessingFunction.hpp"
#include "TextProcessState.hpp"
#include "LineParsingHelpers.hpp"
#include "../StringUtil.hpp"
#include "../Assembler.hpp" // for tests

#include <iostream>

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
using AssumptionResetRAII = erfin::AssumptionResetRAII;
using UInt32 = erfin::UInt32;

StringCIter make_plus(TextProcessState &, StringCIter, StringCIter);

StringCIter make_minus(TextProcessState &, StringCIter, StringCIter);

StringCIter make_multiply(TextProcessState &, StringCIter, StringCIter);

StringCIter make_multiply_int(TextProcessState &, StringCIter, StringCIter);

StringCIter make_multiply_fp(TextProcessState &, StringCIter, StringCIter);

StringCIter make_divide(TextProcessState &, StringCIter, StringCIter);

StringCIter make_divide_int(TextProcessState &, StringCIter, StringCIter);

StringCIter make_divide_fp(TextProcessState &, StringCIter, StringCIter);

StringCIter make_modulus(TextProcessState &, StringCIter, StringCIter);

StringCIter make_modulus_int(TextProcessState &, StringCIter, StringCIter);

StringCIter make_modulus_fp(TextProcessState &, StringCIter, StringCIter);

// <------------------------- Logic operations ------------------------------->

StringCIter make_and(TextProcessState &, StringCIter, StringCIter);

StringCIter make_or(TextProcessState &, StringCIter, StringCIter);

StringCIter make_xor(TextProcessState &, StringCIter, StringCIter);

StringCIter make_not(TextProcessState &, StringCIter, StringCIter);

StringCIter make_rotate(TextProcessState &, StringCIter, StringCIter);

StringCIter make_syscall(TextProcessState &, StringCIter, StringCIter);

// <--------------------- flow control operations ---------------------------->

StringCIter make_cmp(TextProcessState &, StringCIter, StringCIter); // "no hint"

StringCIter make_cmp_fp (TextProcessState &, StringCIter, StringCIter);

StringCIter make_cmp_int(TextProcessState &, StringCIter, StringCIter);

StringCIter make_skip(TextProcessState &, StringCIter, StringCIter);

// <------------------------- move operations -------------------------------->

StringCIter make_set(TextProcessState &, StringCIter, StringCIter);

StringCIter make_load(TextProcessState &, StringCIter, StringCIter);

StringCIter make_save(TextProcessState &, StringCIter, StringCIter);

// <----------------------- psuedo instructions ------------------------------>

StringCIter make_jump(TextProcessState &, StringCIter, StringCIter);

StringCIter assume_directive(TextProcessState &, StringCIter, StringCIter);

} // end of <anonymous> namespace

namespace erfin {

LineToInstFunc get_line_processing_function
    (Assembler::SuffixAssumption assumptions, const std::string & fname)
{
    using LineFuncMap = std::map<std::string, LineToInstFunc>;
    static bool is_initialized = false;
    static LineFuncMap fmap;
    if (is_initialized) {
        auto itr = fmap.find(fname);
        return (itr == fmap.end()) ? nullptr : itr->second;
    }

    fmap["and"] = fmap["&"] = make_and;
    fmap["or" ] = fmap["|"] = make_or ;
    fmap["xor"] = fmap["^"] = make_xor;

    fmap["not"] = fmap["!"] = fmap["~"] = make_not;

    fmap["plus" ] = fmap["add"] = fmap["+"] = make_plus;
    fmap["minus"] = fmap["sub"] = fmap["-"] = make_minus;
    fmap["skip"] = fmap["?"] = make_skip;

    // for short hand, memory is to be thought of as on the left of the
    // instruction.
    fmap["save"] = fmap["sav"] = fmap["<<"] = make_save;
    fmap["load"] = fmap["ld" ] = fmap[">>"] = make_load;

    fmap["set"] = fmap["="] = make_set;
    fmap["rotate"] = fmap["rot"] = fmap["@"] = make_rotate;

    fmap["call"] = fmap["()"] = make_syscall;

    fmap["jump"] = make_jump;

    // suffixes
    fmap["times-int"] = fmap["mul-int"] = fmap["multiply-int"] = fmap["*-int"]
        = make_multiply_int;
    fmap["times-fp" ] = fmap["mul-fp" ] = fmap["multiply-fp" ] = fmap["*-fp" ]
        = make_multiply_fp;
    fmap["times"] = fmap["mul"] = fmap["multiply"] = fmap["*"]
        = make_multiply;

    fmap["div-int"] = fmap["divide-int"  ] = fmap["/-int"] = make_divide_int;
    fmap["div-fp" ] = fmap["divide-fp"   ] = fmap["/-fp" ] = make_divide_fp;
    fmap["div"] = fmap["divmod"] = fmap["/"] = make_divide;

    fmap["comp-int"] = fmap["compare-int"] = fmap["cmp-int"]
        = fmap["<>=-int"] = make_cmp_int;
    fmap["comp-fp"] = fmap["compare-fp"] = fmap["cmp-fp"] = fmap["<>=-fp"]
        = make_cmp_fp;
    fmap["comp"] = fmap["compare"] = fmap["cmp"] = fmap["<>="] = make_cmp;

    fmap["mod"] = fmap["modulus"] = fmap["%"] = make_modulus;
    fmap["mod-int"] = fmap["modulus-int"] = fmap["%-int"] = make_modulus_int;
    fmap["mod-fp"] = fmap["modulus-fp"] = fmap["%-fp"] = make_modulus_fp;

    fmap["assume"] = assume_directive;

    is_initialized = true;
    return get_line_processing_function(assumptions, fname);
}

} // end of erfin namespace

namespace {

// <-------------------------------------------------------------------------->

bool op_code_supports_fpoint_immd(erfin::Inst inst);

bool op_code_supports_integer_immd(erfin::Inst inst);

template <typename T, typename ... Types>
bool equal_to_any(T, Types...);

StringCIter make_generic_logic
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end);

StringCIter make_generic_arithemetic
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end);

StringCIter make_generic_memory_access
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end);

StringCIter make_plus
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_arithemetic(erfin::OpCode::PLUS, state, beg, end);
}

StringCIter make_minus
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_arithemetic(erfin::OpCode::MINUS, state, beg, end);
}

StringCIter make_multiply
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_arithemetic(erfin::OpCode::TIMES, state, beg, end);
}

StringCIter make_multiply_int
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    AssumptionResetRAII arr(state, erfin::Assembler::USING_INT); (void)arr;
    return make_generic_arithemetic(erfin::OpCode::TIMES, state, beg, end);
}

StringCIter make_multiply_fp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    AssumptionResetRAII arr(state, erfin::Assembler::USING_FP); (void)arr;
    return make_generic_arithemetic(erfin::OpCode::TIMES, state, beg, end);
}

StringCIter make_divide
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_arithemetic(erfin::OpCode::DIVIDE, state, beg, end);
}

StringCIter make_divide_int
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    AssumptionResetRAII arr(state, erfin::Assembler::USING_INT); (void)arr;
    return make_generic_arithemetic(erfin::OpCode::DIVIDE, state, beg, end);
}

StringCIter make_divide_fp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    AssumptionResetRAII arr(state, erfin::Assembler::USING_FP); (void)arr;
    return make_generic_arithemetic(erfin::OpCode::DIVIDE, state, beg, end);
}

StringCIter make_modulus
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_arithemetic(erfin::OpCode::MODULUS, state, beg, end);
}

StringCIter make_modulus_int
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    AssumptionResetRAII arr(state, erfin::Assembler::USING_FP); (void)arr;
    return make_generic_arithemetic(erfin::OpCode::DIVIDE, state, beg, end);
}

StringCIter make_modulus_fp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    AssumptionResetRAII arr(state, erfin::Assembler::USING_FP); (void)arr;
    return make_generic_arithemetic(erfin::OpCode::DIVIDE, state, beg, end);
}

StringCIter make_and
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::AND, state, beg, end); }

StringCIter make_or
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::OR, state, beg, end); }

StringCIter make_xor
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::XOR, state, beg, end); }

StringCIter make_not
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;
    ++beg;
    auto eol = get_eol(beg, end);
    switch (get_lines_param_form(beg, eol)) {
    case XPF_1R:
        state.add_instruction(encode(NOT ,string_to_register(*beg)));
        return eol;
    default:
        throw state.make_error(": exactly one argument permitted for logical "
                               "complement (not).");
    }
}

StringCIter make_rotate
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;
    static constexpr const char * const NON_INT_IMMD_MSG =
        ": Rotate may only take an integer for bit rotation.";
    static constexpr const char * const INVALID_PARAMS_MSG =
        ": Rotate may only take a register target and another register or "
        "integer for number of places to rotate the data.";
    auto eol = get_eol(++beg, end);

    NumericParseInfo npi;
    switch (get_lines_param_form(beg, eol, &npi)) {
    case XPF_1R_INT:
        switch (npi.type) {
        case INTEGER: break;
        default:
            throw state.make_error(NON_INT_IMMD_MSG);
        }
        state.add_instruction(with_int::encode(
            ROTATE, string_to_register(*beg), npi.integer));
        break;
    case XPF_2R:
        state.add_instruction(encode(ROTATE, string_to_register(*beg),
                                     string_to_register(*(beg + 1))   ));
        break;
    default: throw state.make_error(INVALID_PARAMS_MSG);
    }
    return eol;
}

StringCIter make_syscall
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;

    auto eol = get_eol(++beg, end);
    NumericParseInfo npi = parse_number(*beg);

    switch (npi.type) {
    case INTEGER: break;
    case NOT_NUMERIC:
        if (*beg == "upload")
            npi.integer = UPLOAD_SPRITE;
        else if (*beg == "unload")
            npi.integer = UNLOAD_SPRITE;
        else if (*beg == "clear")
            npi.integer = SCREEN_CLEAR;
        else if (*beg == "wait")
            npi.integer = WAIT_FOR_FRAME;
        else if (*beg == "read")
            npi.integer = READ_INPUT;
        else if (*beg == "draw")
            npi.integer = DRAW_SPRITE;
        else
            throw state.make_error(": \"" + *beg + "\" is not a system call.");
        break;
    default:
        throw state.make_error(": fixed points are not system call names.");
    }

    state.add_instruction(encode_op(SYSTEM_CALL) | encode_immd(npi.integer));
    return eol;
}

StringCIter make_cmp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_logic(erfin::OpCode::COMP, state, beg, end);
}

StringCIter make_cmp_fp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    AssumptionResetRAII arr(state, erfin::Assembler::USING_FP); (void)arr;
    return make_cmp(state, beg, end);
}

StringCIter make_cmp_int
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    AssumptionResetRAII arr(state, erfin::Assembler::USING_INT); (void)arr;
    return make_cmp(state, beg, end);
}

StringCIter make_skip
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;
    ++beg;
    auto eol = get_eol(beg, end);

    auto string_to_int_immd = [] (StringCIter itr) -> UInt32 {
        int i;
        bool b = string_to_number(itr->begin(),itr->end(), i);
        assert(b); (void)b;
        return encode_immd(i);
    };

    UInt32 temp;

    switch (get_lines_param_form(beg, eol)) {
    case XPF_1R_LABEL:{
        const auto & label = *(beg + 1);
        if (label == "==") {
            temp = COMP_EQUAL_MASK;
        } else if (label == "<") {
            temp = COMP_LESS_THAN_MASK;
        } else if (label == ">") {
            temp = COMP_GREATER_THAN_MASK;
        } else if (label == "<=") {
            temp = COMP_EQUAL_MASK | COMP_LESS_THAN_MASK;
        } else if (label == ">=") {
            temp = COMP_EQUAL_MASK | COMP_GREATER_THAN_MASK;
        } else if (label == "!=") {
            temp = COMP_NOT_EQUAL_MASK;
        } else {
            throw state.make_error(": labels are not support with skip "
                                   "instructions.");
        }
        }
        state.add_instruction(encode(SKIP, string_to_register(*beg), temp));
        break;
    case XPF_1R_INT:
        temp = string_to_int_immd(beg + 1);
        state.add_instruction(encode(SKIP, string_to_register(*beg), temp));
        break;
    case XPF_1R_FP:
        throw state.make_error(": a fixed point is not an appropiate mask.");
    case XPF_1R:
        state.add_instruction(encode(SKIP, string_to_register(*beg)));
        break;
    default: throw state.make_error(": unsupported parameters.");
    }

    return eol;
}

StringCIter make_set
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;
    assert(*beg == "set" || *beg == "=");
    // supports fp 9/6, integer immediates and registers
    ++beg; // skip "set"
    // set instruction has exactly two arguments
    StringCIter line_end = get_eol(beg, end);
    auto itr2reg = [](StringCIter itr) { return string_to_register(*itr); };
    Inst inst;
    NumericParseInfo npi;
    const std::string * label = nullptr;
    switch (get_lines_param_form(beg, line_end, &npi)) {
    case XPF_2R:
        inst = encode(SET, itr2reg(beg), itr2reg(beg + 1));
        break;
    case XPF_1R_INT:
        inst = with_int::encode(SET, itr2reg(beg), npi.integer);
        break;
    case XPF_1R_FP:
        inst = with_fp::encode(SET, itr2reg(beg), npi.floating_point);
        break;
    case XPF_1R_LABEL:
        label = &*(beg + 1);
        inst = encode_op(SET) | encode_param_form(REG_IMMD) |
               encode_reg(itr2reg(beg));
        break;
    default:
        throw state.make_error(": set instruction may only have exactly "
                                "two arguments.");
    }
    state.add_instruction(inst, label);
    return line_end;
}

StringCIter make_load
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_memory_access(erfin::OpCode::LOAD, state, beg, end);
}

StringCIter make_save
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_memory_access(erfin::OpCode::SAVE, state, beg, end);
}

StringCIter make_jump
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    auto eol = get_eol(++beg, end);
    static constexpr const auto PC = enum_types::REG_PC;
    Inst inst = encode_op(enum_types::SET);
    NumericParseInfo npi;
    const std::string * label = nullptr;
    switch (get_lines_param_form(beg, eol, &npi)) {
    case XPF_1R:
        inst |= encode_param_form(enum_types::REG_REG) |
                encode_reg_reg(PC, string_to_register(*beg));
        break;
    case XPF_INT:
        inst |= encode_param_form(enum_types::REG_IMMD) |
                encode_reg(PC) | encode_immd(npi.integer);
        break;
    case XPF_LABEL:
        // not quite sure... will need to insert the offset...? maybe not
        label = &*beg;
        inst |= encode_reg(PC);
        break;
    default:
        throw state.make_error(": jump only excepts one argument, the "
                               "destination.");
    }
    state.add_instruction(inst, label);
    return eol;
}

StringCIter assume_directive
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    auto eol = get_eol(++beg, end);
    if ((eol - beg) == 1) {
        if (equal_to_any(*beg, "fp", "fixed-point")) {
            state.assumptions = Assembler::NO_ASSUMPTION;
        } else if (*beg == "int" || *beg == "integer") {
            state.assumptions = Assembler::USING_INT;
        } else if (equal_to_any(*beg, "none", "nothing")) {
            state.assumptions = Assembler::USING_FP;
        } else {
            state.make_error(": \"" + *beg + "\" is not a valid assumption.");
        }
    } else {
        throw state.make_error(": too many assumptions/arguments.");
    }
    return eol;
}

// <-------------------------------------------------------------------------->

UInt32 deal_with_int_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state);

UInt32 deal_with_fp_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state);

bool op_code_supports_fpoint_immd(erfin::Inst inst) {
    using namespace erfin::enum_types;
    switch (inst) {
    case PLUS: case MINUS: return true;
    case TIMES: case AND: case XOR: case OR: case DIVIDE: case COMP:
        return false;
    default: assert(false); break;
    }
    std::terminate();
}

bool op_code_supports_integer_immd(erfin::Inst inst) {
    using namespace erfin::enum_types;
    switch (inst) {
    case PLUS: case MINUS: case TIMES: case AND: case XOR: case OR:
    case DIVIDE: case COMP:
        return true;
    default: assert(false); break;
    }
    std::terminate();
}

template <typename T, typename ... Types>
bool equal_to_any(T, Types...) { return false; }

template <typename T, typename Head, typename ... Types>
bool equal_to_any(T primary, Head head, Types ... args)
    { return primary == head || equal_to_any(primary, args...); }

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
    using namespace erfin::enum_types;

    static constexpr const char * const fp_int_ambig_msg =
        ": cannot deduce whether a fixed point or integer operation was "
        "meant; the assembler doesn't know which instruction to construct.";

    bool assumption_matters;
    switch (op_code) {
    // this does require some impl knowledge...
    case PLUS: case MINUS:
    case AND: case OR: case XOR: case NOT:
        assumption_matters = false;
        break;
    default: assumption_matters = true; break;
    }

    Reg a1;
    ++beg;
    auto eol = get_eol(beg, end);
    const auto pf = get_lines_param_form(beg, eol);

    switch (pf) {
    case XPF_3R   : case XPF_2R:
        // perfect chance to check if assumptions are ok!
        if (assumption_matters && state.assumptions == Assembler::NO_ASSUMPTION) {
            throw state.make_error(fp_int_ambig_msg);
        }
        MACRO_FALLTHROUGH;
    case XPF_2R_FP: case XPF_2R_INT: case XPF_2R_LABEL:
    case XPF_1R_FP: case XPF_1R_INT: case XPF_1R_LABEL:
        a1 = string_to_register(*beg);
        break;
    default:
        throw state.make_error(": unsupported parameters.");
    }

    Reg a2, ans;
    Inst inst = 0;
    UInt32 (*handle_immd)(StringCIter, OpCode, TextProcessState &) = nullptr;
    const std::string * label = nullptr;
    auto do_nothing_for_label =
        [](StringCIter, OpCode, TextProcessState &) -> UInt32 { return 0; };
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
        if (state.assumptions == Assembler::USING_FP)
            type_indentity = IS_FP;
        else if (state.assumptions == Assembler::USING_INT)
            type_indentity = IS_INT;
    }
    if (type_indentity == IS_FP) {
        inst |= encode_set_is_fixed_point_flag();
    }

    switch (pf) {
    case XPF_2R: // psuedo instruction
        a2 = ans = string_to_register(*(beg + 1));
        inst = encode(op_code, a1, a2, ans);
        break;
    case XPF_3R:
        a2  = string_to_register(*(beg + 1));
        ans = string_to_register(*(beg + 2));
        inst = encode(op_code, a1, a2, ans);
        break;
    case XPF_2R_FP: case XPF_2R_INT: case XPF_2R_LABEL:
        ans = string_to_register(*(beg + 1));
        inst = encode(op_code, a1, ans, handle_immd(eol, op_code, state));
        break;
    case XPF_1R_FP: case XPF_1R_INT: case XPF_1R_LABEL:
        inst = encode(op_code, a1, a1, handle_immd(eol, op_code, state));
        break;
    default: assert(false); break;
    }

    state.add_instruction(inst, label);
    return eol;
}

StringCIter make_generic_memory_access
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;
    assert(op_code == LOAD || op_code == SAVE);

    const auto eol = get_eol(++beg, end);
    assert(!get_line_processing_function(Assembler::NO_ASSUMPTION, *beg));

    const std::string * label = nullptr;
    Inst inst = encode_op(op_code);
    NumericParseInfo npi;
    const auto pf = get_lines_param_form(beg, eol, &npi);

    if (equal_to_any(pf, XPF_1R, XPF_1R_INT, XPF_1R_LABEL) && op_code == SAVE)
        throw state.make_error(": deference psuedo instruction only available for loading.");

    switch (pf) {
    case XPF_1R: case XPF_2R:
        inst |= encode_param_form(REG_REG);
        break;
    case XPF_2R_LABEL: case XPF_2R_INT:
        inst |= encode_param_form(REG_REG_IMMD);
        break;
    case XPF_1R_INT: case XPF_1R_LABEL:
        inst |= encode_param_form(REG_REG_IMMD);
        break;
    default:
        throw state.make_error(std::string(": <op_code> does not support ") +
                               extended_param_form_to_string(pf) +
                               " for parameters.");
    }

    if (equal_to_any(pf, XPF_2R_INT, XPF_1R_INT))
        inst |= npi.integer;

    if (equal_to_any(pf, XPF_1R_LABEL, XPF_2R_LABEL))
        label = &*(eol - 1);

    Reg reg = string_to_register(*beg);
    if (equal_to_any(pf, XPF_3R, XPF_2R, XPF_2R_INT, XPF_2R_LABEL)) {
        inst |= encode_reg_reg(reg, string_to_register(*(beg + 1)));
    }
    if (equal_to_any(pf, XPF_1R, XPF_1R_INT, XPF_1R_LABEL)) {
        inst |= encode_reg_reg(reg, reg);
    }

    state.add_instruction(inst, label);
    return eol;
}

// <-------------------------------------------------------------------------->

UInt32 deal_with_int_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state)
{
    static constexpr const char * const int_unsupported_msg =
        ": instruction does not support integer immediates.";
    if (!op_code_supports_integer_immd(op_code))
        throw state.make_error(int_unsupported_msg);
    int i;
    string_to_number(&*(eol - 1)->begin(), &*(eol - 1)->end(), i);
    return erfin::encode_immd(i);
}

UInt32 deal_with_fp_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state)
{
    static constexpr const char * const fp_unsupported_msg =
        ": instruction does not support fixed point immediates.";
    if (!op_code_supports_fpoint_immd(op_code))
        throw state.make_error(fp_unsupported_msg);
    double d;
    string_to_number(&*(eol - 1)->begin(), &*(eol - 1)->end(), d);
    return erfin::encode_immd(d);
}

// <-------------------------------------------------------------------------->

// tests include helpers, therefore this is at the bottom of the source code

} // end of <anonymous> namespace

namespace erfin {

void run_get_line_processing_function_tests() {
    TextProcessState state;
    // test basic instructions
    {
    const std::vector<std::string> sample_code = {
        "="  , "x", "y"    , "\n",
        "set", "x", "1234" , "\n",
        "="  , "x", "12.34"
    };
    auto beg = sample_code.begin();
    auto end = sample_code.end();
    beg = make_set(state, beg, end);
    beg = make_set(state, ++beg, end);
    beg = make_set(state, ++beg, end);
    auto supposed_top = with_fp::encode(OpCode::SET, Reg::REG_X, 12.34);
    assert(state.m_program_data.back() == supposed_top);
    (void)supposed_top;
    }
    // test that "generic arthemetic" instruction maker functions
    {
    const std::vector<std::string> sample_code = {
        "add", "x", "y", "\n",
        "and", "x", "y", "a", "\n",
        "-"  , "x", "123"
    };
    auto beg = sample_code.begin(), end = sample_code.end();
    beg = make_plus (state, beg, end);
    beg = make_and  (state, ++beg, end);
    beg = make_minus(state, ++beg, end);
    const auto supposed_top = with_int::encode(OpCode::MINUS, Reg::REG_X, Reg::REG_X, 123);
    assert(supposed_top == state.m_program_data.back());
    (void)supposed_top;
    }
    {
    const std::vector<std::string> sample_code = {
        ">>", "x", "9384", "\n",
        ">>", "z", "\n",
        "<<", "y", "a", "\n",
        "<<", "y", "a", "4"
    };
    auto beg = sample_code.begin(), end = sample_code.end();
    beg = make_load(state,   beg, end);
    beg = make_load(state, ++beg, end);
    beg = make_save(state, ++beg, end);
    beg = make_save(state, ++beg, end);
    const auto supposed_top = with_int::encode(OpCode::SAVE, Reg::REG_Y, Reg::REG_A, 4);
    assert(supposed_top == state.m_program_data.back());
    (void)supposed_top;
    }
    {
    const std::vector<std::string> sample_code = {
        "assume", "integer", "\n",
        "<>=", "x", "y", "\n",
        "?", "x", "1"
    };
    auto beg = sample_code.begin(), end = sample_code.end();
    beg = assume_directive(state,   beg, end);
    beg = make_cmp        (state, ++beg, end);
    beg = make_skip       (state, ++beg, end);
    const auto supposed_top = with_int::encode(OpCode::SKIP, Reg::REG_X, 1);
    assert(supposed_top == state.m_program_data.back());
    (void)supposed_top;
    }
    // test unfulfilled labels!
    {
    state = TextProcessState();
    const std::vector<std::string> sample_code = {
        "=" , "pc", "label1", "\n",
        ">>", "x", "label2", "\n",
        ":", "label1", ":", "label2", "+", "x", "y", "\n",
        "-", "x", "a"
    };
    auto beg = sample_code.begin(), end = sample_code.end();
    beg = make_set     (state,   beg, end);
    beg = make_load    (state, ++beg, end);
    beg = state.process_label(++beg, end);
    beg = state.process_label(  beg, end);
    beg = make_plus    (state,   beg, end);
    beg = make_minus   (state, ++beg, end);
    state.resolve_unfulfilled_labels();
    assert(state.m_program_data[0] == with_int::encode(OpCode::SET , Reg::REG_PC, 2));
    // remember load reg immd is a psuedo instruction
    assert(state.m_program_data[1] == with_int::encode(OpCode::LOAD, Reg::REG_X, Reg::REG_X , 2));
    }
    {
    constexpr const char * const sample_code =
        "     = x 1.0 # hello there ;-)\n"
        "     = y 1.44\n"
        ":inc + x y x\n"
        "     = pc inc";
    Assembler asmr;
    asmr.assemble_from_string(sample_code);
    const auto & pdata = asmr.program_data();
    assert(pdata[0] == with_fp::encode(OpCode::SET , Reg::REG_X, 1.0 ));
    assert(pdata[1] == with_fp::encode(OpCode::SET , Reg::REG_Y, 1.44));
    assert(pdata[2] == encode(OpCode::PLUS, Reg::REG_X, Reg::REG_Y, Reg::REG_X));
    assert(pdata[3] == with_int::encode(OpCode::SET , Reg::REG_PC, 2));
    (void)pdata;
    }
}

} // end of erfin namespace
