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
#include "ProcessIoLine.hpp"
#include "make_generic_instructions.hpp"

#include "../StringUtil.hpp"
#include "../Assembler.hpp" // for tests

#include <iostream>
#include <bitset>

#include <cassert>



namespace {

using StringCIter = erfin::TokensConstIterator;
using TextProcessState = erfin::TextProcessState;
using Assembler = erfin::Assembler;
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

// <--------------------- flow control operations ---------------------------->

StringCIter make_cmp(TextProcessState &, StringCIter, StringCIter); // "no hint"

StringCIter make_cmp_fp (TextProcessState &, StringCIter, StringCIter);

StringCIter make_cmp_int(TextProcessState &, StringCIter, StringCIter);

StringCIter make_skip(TextProcessState &, StringCIter, StringCIter);
// v moved v
StringCIter make_call(TextProcessState &, StringCIter, StringCIter);

// <------------------------- move operations -------------------------------->

StringCIter make_set(TextProcessState &, StringCIter, StringCIter);

StringCIter make_load(TextProcessState &, StringCIter, StringCIter);

StringCIter make_save(TextProcessState &, StringCIter, StringCIter);

// <----------------------- psuedo instructions ------------------------------>

StringCIter make_jump(TextProcessState &, StringCIter, StringCIter);

StringCIter assume_directive(TextProcessState &, StringCIter, StringCIter);

StringCIter make_push(TextProcessState &, StringCIter, StringCIter);

StringCIter make_pop(TextProcessState &, StringCIter, StringCIter);

} // end of <anonymous> namespace

namespace erfin {

LineToInstFunc get_line_processing_function
    (Assembler::Assumption assumptions, const std::string & fname)
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

    fmap["io"] = make_sysio;

    fmap["call"] = make_call;

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
    fmap["comp"] = fmap["compare"] = fmap["cmp"] = fmap["<=>"] = make_cmp;

    fmap["mod"] = fmap["modulus"] = fmap["%"] = make_modulus;
    fmap["mod-int"] = fmap["modulus-int"] = fmap["%-int"] = make_modulus_int;
    fmap["mod-fp"] = fmap["modulus-fp"] = fmap["%-fp"] = make_modulus_fp;

    fmap["assume"] = assume_directive;
    fmap["push"] = make_push;
    fmap["pop"] = make_pop;

    is_initialized = true;
    return get_line_processing_function(assumptions, fname);
}

} // end of erfin namespace

// <-------------------------------------------------------------------------->

namespace {

StringCIter make_stack_op
    (TextProcessState & state, StringCIter beg, StringCIter end,
     erfin::OpCode val_op);

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
    auto arr = state.get_scoped_assumption_restorer(); (void)arr;
    state.include_assumption(Assembler::USING_INT);
    return make_generic_arithemetic(erfin::OpCode::TIMES, state, beg, end);
}

StringCIter make_multiply_fp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    auto arr = state.get_scoped_assumption_restorer(); (void)arr;
    state.include_assumption(Assembler::USING_FP);
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
    auto arr = state.get_scoped_assumption_restorer(); (void)arr;
    state.include_assumption(Assembler::USING_INT);
    return make_generic_arithemetic(erfin::OpCode::DIVIDE, state, beg, end);
}

StringCIter make_divide_fp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    auto arr = state.get_scoped_assumption_restorer(); (void)arr;
    state.include_assumption(Assembler::USING_FP);
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
    auto arr = state.get_scoped_assumption_restorer(); (void)arr;
    state.include_assumption(Assembler::USING_INT);
    return make_generic_arithemetic(erfin::OpCode::MODULUS, state, beg, end);
}

StringCIter make_modulus_fp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    auto arr = state.get_scoped_assumption_restorer(); (void)arr;
    state.include_assumption(Assembler::USING_FP);
    return make_generic_arithemetic(erfin::OpCode::MODULUS, state, beg, end);
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

    ++beg;
    auto eol = get_eol(beg, end);
    switch (get_lines_param_form(beg, eol)) {
    case XPF_1R:
        state.add_instruction(encode(OpCode::NOT ,string_to_register(*beg)));
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
    return make_generic_logic(OpCode::ROTATE, state, beg, end);
}

StringCIter make_call
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;

    using Pf = ParamForm;
    constexpr const auto CALL = OpCode::CALL;
    auto eol = get_eol(++beg, end);
    NumericParseInfo npi = parse_number(*beg);
    Inst inst;
    const std::remove_reference<decltype(*beg)>::type * label = nullptr;
    switch (get_lines_param_form(beg, eol, &npi)) {
    case XPF_1R   :
        inst |= encode_op_with_pf(CALL, Pf::REG) |
                encode_reg(string_to_register(*beg));
        break;
    case XPF_INT  :
        inst |= encode_op_with_pf(CALL, Pf::IMMD) | encode_immd_int(npi.integer);
        break;
    case XPF_LABEL:
        inst |= encode_op_with_pf(CALL, Pf::IMMD);
        label = &*beg;
        break;
    default:
        throw state.make_error(": requires exactly one argument, and "
                               "immediate or register.");
    }
    state.add_instruction(inst, label);
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
    auto arr = state.get_scoped_assumption_restorer(); (void)arr;
    state.include_assumption(Assembler::USING_FP);
    return make_cmp(state, beg, end);
}

StringCIter make_cmp_int
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    auto arr = state.get_scoped_assumption_restorer(); (void)arr;
    state.include_assumption(Assembler::USING_INT);
    return make_cmp(state, beg, end);
}

StringCIter make_skip
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;

    ++beg;
    auto eol = get_eol(beg, end);

    auto string_to_int_immd = [] (StringCIter itr) -> Immd {
        int i;
        bool b = string_to_number(itr->begin(),itr->end(), i);
        assert(b); (void)b;
        return encode_immd_int(i);
    };

    Immd temp;

    switch (get_lines_param_form(beg, eol)) {
    case XPF_1R_LABEL: {
        const auto & label = *(beg + 1);
        if (label == "==") {
            temp = ImmdConst::COMP_EQUAL_MASK;
        } else if (label == "<") {
            temp = ImmdConst::COMP_LESS_THAN_MASK;
        } else if (label == ">") {
            temp = ImmdConst::COMP_GREATER_THAN_MASK;
        } else if (label == "<=") {
            temp = ImmdConst::COMP_LESS_THAN_OR_EQUAL_MASK;
        } else if (label == ">=") {
            temp = ImmdConst::COMP_GREATER_THAN_OR_EQUAL_MASK;
        } else if (label == "!=") {
            temp = ImmdConst::COMP_NOT_EQUAL_MASK;
        } else {
            throw state.make_error(": labels are not support with skip "
                                   "instructions.");
        }
        }
        state.add_instruction(encode(OpCode::SKIP, string_to_register(*beg), temp));
        break;
    case XPF_1R_INT:
        temp = string_to_int_immd(beg + 1);
        state.add_instruction(encode(OpCode::SKIP, string_to_register(*beg), temp));
        break;
    case XPF_1R_FP:
        throw state.make_error(": a fixed point is not an appropiate mask.");
    case XPF_1R:
        state.add_instruction(encode(OpCode::SKIP, string_to_register(*beg)));
        break;
    default: throw state.make_error(": unsupported parameters.");
    }

    return eol;
}

StringCIter make_set
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;

    assert(*beg == "set" || *beg == "=");
    // supports fp 9/6, integer immediates and registers
    ++beg; // skip "set"
    // set instruction has exactly two arguments
    StringCIter line_end = get_eol(beg, end);
    auto itr2reg = [](StringCIter itr) -> Reg { return string_to_register(*itr); };
    Inst inst;
    NumericParseInfo npi;
    const std::string * label = nullptr;
    switch (get_lines_param_form(beg, line_end, &npi)) {
    case XPF_2R:
        inst = encode(OpCode::SET, itr2reg(beg), itr2reg(beg + 1));
        break;
    case XPF_1R_INT:
        inst = encode(OpCode::SET, itr2reg(beg), encode_immd_int(npi.integer));
        break;
    case XPF_1R_FP:
        inst = encode(OpCode::SET, itr2reg(beg), encode_immd_fp(npi.floating_point));
        break;
    case XPF_1R_LABEL:
        label = &*(beg + 1);
        inst = encode_op_with_pf(OpCode::SET, ParamForm::REG_IMMD) |
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
    Inst inst;
    ParamForm pf;
    NumericParseInfo npi;
    const std::string * label = nullptr;

    switch (get_lines_param_form(beg, eol, &npi)) {
    case XPF_1R:
        pf = ParamForm::REG_REG;
        inst |= encode_reg_reg(Reg::PC, string_to_register(*beg));
        break;
    case XPF_INT:
        pf = ParamForm::REG_IMMD;
        inst |= Inst() | encode_reg(Reg::PC) | encode_immd_int(npi.integer);
        break;
    case XPF_LABEL:
        // not quite sure... will need to insert the offset...? maybe not
        pf = ParamForm::REG_IMMD;
        label = &*beg;
        inst |= encode_reg(Reg::PC);
        break;
    default:
        throw state.make_error(": jump only excepts one argument, the "
                               "destination.");
    }
    inst |= encode_op_with_pf(OpCode::SET, pf);
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
            state.include_assumption(Assembler::USING_FP);
        } else if (equal_to_any(*beg, "int", "integer")) {
            state.include_assumption(Assembler::USING_INT);
        } else if (equal_to_any(*beg, "none", "nothing")) {
            state.include_assumption(Assembler::NO_ASSUMPTIONS);
        } else if (equal_to_any(*beg, "io-throw-away-registers",
                                      "io-throw-away"          ))
        {
            state.exclude_assumption(Assembler::SAVE_AND_RESTORE_REGISTERS);
        } else if (equal_to_any(*beg, "io-save-and-restore-registers",
                                      "io-save-and-restore"          ))
        {
            state.include_assumption(Assembler::SAVE_AND_RESTORE_REGISTERS);
        } else {
            throw state.make_error(": \"" + *beg + "\" is not a valid assumption.");
        }
    } else {
        throw state.make_error(": too many assumptions/arguments.");
    }
    return eol;
}

StringCIter make_push
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    return make_stack_op(state, beg, end, OpCode::SAVE);
}

StringCIter make_pop
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    return make_stack_op(state, beg, end, OpCode::LOAD);
}

// <-------------------------------------------------------------------------->

template <int ARGUMENT_COUNT, int COMMAND_IDENTITY>
StringCIter make_gpu_io_instruction
    (TextProcessState &, StringCIter beg, StringCIter end,
     const char * argument_count_mismatch_error           );

StringCIter make_stack_op
    (TextProcessState & state, StringCIter beg, StringCIter end,
     erfin::OpCode val_op)
{
    using namespace erfin;

    assert(val_op == OpCode::SAVE || val_op == OpCode::LOAD);

    // "compile time" cosntant
    const constexpr auto SP = Reg::SP;
    // runtime constant
    const auto eol_c = get_eol(beg, end);
    if (eol_c - (beg + 1) > std::numeric_limits<int>::max())
        throw state.make_error(": You know what you did...");

    const int num_of_args_c = int(eol_c - (beg + 1));

    if (num_of_args_c == 0) return eol_c;

    Inst change_sp = encode(
        val_op == OpCode::LOAD ? OpCode::MINUS : OpCode::PLUS,
        SP, SP, encode_immd_int(num_of_args_c));

    // I want pop to sub first
    // Rationale, it will allow me to "pop pc" to do a return
    if (val_op == OpCode::LOAD) state.add_instruction(change_sp);

    // variable
    int stack_offset = val_op == OpCode::LOAD ? num_of_args_c : 1;
    while (++beg != eol_c) {
        auto reg = string_to_register_or_throw(state, *beg);
        state.add_instruction
            (encode(val_op, reg, SP, encode_immd_int(stack_offset)));
        stack_offset += val_op == OpCode::LOAD ? -1 : 1;
    }

    if (val_op == OpCode::SAVE) state.add_instruction(change_sp);

    assert(beg == eol_c);
    return beg;
}

// <-------------------------------------------------------------------------->

template <int ARGUMENT_COUNT, int COMMAND_IDENTITY>
StringCIter make_gpu_io_instruction
    (TextProcessState & state, StringCIter beg, StringCIter end,
     const char * argument_count_mismatch_error                 )
{
    // three arguments
    using namespace erfin;
    static constexpr const int REGISTERS_USED =
        ARGUMENT_COUNT == 0 ? 1 : ARGUMENT_COUNT;
    auto eol = get_eol(++beg, end);
    if (eol - beg != REGISTERS_USED) {
        state.make_error(argument_count_mismatch_error);
    }
    std::array<Reg, REGISTERS_USED> args;
    for (Reg & arg : args) {
        arg = string_to_register_or_throw(state, *beg++);
    }
    assert(beg == eol);
    // what does push do to the SP?
    make_gpu_io_command_send(state, COMMAND_IDENTITY, args[0]);
    static constexpr const auto GPU_INPUT_STREAM = device_addresses::GPU_INPUT_STREAM;
    for (const Reg & arg : args) {
        state.add_instruction(encode(OpCode::SAVE,                arg,
                                     encode_immd_addr(GPU_INPUT_STREAM)) );
    }
    return eol;
}

// <-------------------------------------------------------------------------->

// tests include helpers, therefore this is at the bottom of the source code

} // end of <anonymous> namespace

namespace erfin {

void run_get_line_processing_function_tests() {
    {
    auto a = encode_op_with_pf(OpCode::SET, ParamForm::REG_IMMD);
    auto b = encode(OpCode::SET, Reg(0), encode_immd_int(0));
    (void)a; (void)b;
    assert(string_to_register("x") == Reg::X);

    NumericParseInfo npi;
    npi = parse_number("12.34");
    Immd i = encode_immd_fp(12.34);
    (void)npi;
    (void)i;
    assert(i == encode_immd_fp(npi.floating_point));

    auto c = encode(OpCode::SET, Reg::X, encode_immd_fp(12.34));
    auto d = encode(OpCode::SET, string_to_register("x"), encode_immd_fp(npi.floating_point));
    (void)c; (void)d;
    assert(c == d);
    }
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

    auto supposed_top = encode(OpCode::SET, Reg::X, encode_immd_fp(12.34));
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
    const auto supposed_top = encode(OpCode::MINUS, Reg::X, Reg::X,
                                     encode_immd_int(123));
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
    const auto supposed_top = encode(OpCode::SAVE, Reg::Y, Reg::A, encode_immd_int(4));
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
    const auto supposed_top = encode(OpCode::SKIP, Reg::X, encode_immd_int(1));
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
    assert(state.m_program_data[0] == encode(OpCode::SET , Reg::PC, encode_immd_int(2)));
    // remember load reg immd is a psuedo instruction
    assert(state.m_program_data[1] == encode(OpCode::LOAD, Reg::X, Reg::X,
                                             encode_immd_int(2))          );
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
    assert(pdata[0] == encode(OpCode::SET , Reg::X, encode_immd_fp(1.0)));
    assert(pdata[1] == encode(OpCode::SET , Reg::Y, encode_immd_fp(1.44)));
    assert(pdata[2] == encode(OpCode::PLUS, Reg::X, Reg::Y, Reg::X));
    assert(pdata[3] == encode(OpCode::SET , Reg::PC, encode_immd_int(2)));
    (void)pdata;
    }
    {
    constexpr const char * const sample_code =
        "io upload x y z a";
    Assembler asmr;
    asmr.assemble_from_string(sample_code);
    const auto & pdata = asmr.program_data();
    (void)pdata;
    }
    run_make_sysio_tests();
}

} // end of erfin namespace
