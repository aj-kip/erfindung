/****************************************************************************

    File: Assembler.cpp
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

#include "Assembler.hpp"
#include "FixedPointUtil.hpp"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>

#include <sys/stat.h>
#include <cassert>

#ifdef MACRO_COMPILER_GCC
#   define MACRO_FALLTHROUGH
#elif defined(MACRO_COMPILER_MSVC)
#   define MACRO_FALLTHROUGH
#elif defined(MACRO_COMPILER_CLANG)
#   define MACRO_FALLTHROUGH [[clang::fallthrough]]
#else
#   define MACRO_FALLTHROUGH [[clang::fallthrough]]
#endif

namespace {

using StringCIter = std::vector<std::string>::const_iterator;
using UInt32 = erfin::UInt32;
using Error = std::runtime_error;
using SuffixAssumption = erfin::Assembler::SuffixAssumption;

struct TextProcessState {
    TextProcessState(): in_square_brackets(false), in_data_directive(false),
                        current_source_line(1), assumptions(nullptr) {}
    struct LabelPair { std::size_t program_location, source_line; };
    struct UnfilledLabelPair {
        UnfilledLabelPair(std::size_t i_, const std::string & s_):
            program_location(i_),
            label(s_)
        {}
        std::size_t program_location;
        std::string label;
    };

    bool in_square_brackets;
    bool in_data_directive;
    std::size_t current_source_line;
    std::vector<UInt32> data;
    std::map<std::string, LabelPair> labels;
    std::vector<erfin::Inst> program_data;
    std::vector<UnfilledLabelPair> unfulfilled_labels;
    const SuffixAssumption * assumptions;
};

// high level textual processing
std::vector<std::string> seperate_into_lines(const std::string & str);

void remove_comments_from(std::string & str);

std::vector<std::string> tokenize(const std::vector<std::string> & lines);

void remove_blank_strings(std::vector<std::string> & strings);

void convert_to_lower_case(std::string & str);

void process_text(TextProcessState & state, StringCIter beg, StringCIter end);

int get_file_size(const char * filename);

void resolve_unfulfilled_labels(TextProcessState & state);

} // end of <anonymous> namespace

namespace erfin {

void Assembler::assemble_from_file(const char * file) {
    m_error_string = "";
    std::string file_contents;
    int fsize = get_file_size(file);
    file_contents.reserve(std::size_t(fsize));
    if (fsize < 0) {
        m_error_string = "Could not read file contents (file too large/not "
                         "there?).";
        return;
    }
    std::fstream file_stream(file);
    for (int i = 0; i != fsize; ++i) {
        file_contents.push_back(' ');
        file_stream.read(&file_contents.back(), 1);
    }
    assemble_from_string(file_contents);
}

void Assembler::assemble_from_string(const std::string & source) {
    m_error_string = "";
    m_program.clear();
    std::string source_copy = source;
    convert_to_lower_case(source_copy);
    std::vector<std::string> line_list = seperate_into_lines(source_copy);
    for (std::string & str : line_list)
        remove_comments_from(str);
    remove_blank_strings(line_list);
    std::vector<std::string> tokens = tokenize(line_list);

    TextProcessState tpstate;
    tpstate.assumptions = &m_assumptions;
    process_text(tpstate, tokens.begin(), tokens.end());
    resolve_unfulfilled_labels(tpstate);
    // only when a valid program has been assembled do we swap in the actual
    // instructions, as a throw may occur at any point of the text processing
    m_program.swap(tpstate.program_data);
}

const Assembler::ProgramData & Assembler::program_data() const
    { return m_program; }

UInt32 encode_immd(double d) {
    UInt32 fullwidth = to_fixed_point(d);
    // we want a 9/6 fixed point number (+ one bit for sign)
    UInt32 sign_part = (fullwidth & 0xF0000000u) >> 16u;
    // full width is a 15/16 fixed point number
    UInt32 partial = (fullwidth >> 10u) & 0x7FFFu;
    // make sure we're not losing any of the integer part
    if ((fullwidth >> 16u) & ~0x1FFu)
        throw Error("Value too large to be encoded in a 9/6 fixed point number.");
    return sign_part | partial;
}

const char * register_to_string(Reg r) {
    using namespace enum_types;
    switch (r) {
    case REG_X : return "x" ;
    case REG_Y : return "y" ;
    case REG_Z : return "z" ;
    case REG_A : return "a" ;
    case REG_B : return "b" ;
    case REG_C : return "c" ;
    case REG_BP: return "bp";
    case REG_PC: return "pc";
    default: throw Error("Invalid register, cannot convert to a string.");
    }
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

Inst encode(OpCode op, Reg r0, Reg r1, Reg r2, Reg r3) {
    return encode_param_form(erfin::ParamForm::REG_REG_REG_REG) |
           encode_op(op) | encode_reg_reg_reg_reg(r0, r1, r2, r3);
}

Inst encode(OpCode op, Reg r0, int i) {
    return encode_param_form(erfin::ParamForm::REG_IMMD) |
           encode_op(op) | encode_reg(r0) | encode_immd(i);
}

Inst encode(OpCode op, Reg r0, Reg r1, int i) {
    return encode_param_form(erfin::ParamForm::REG_REG_IMMD) |
           encode_op(op) | encode_reg_reg(r0, r1) | encode_immd(i);
}

Inst encode(OpCode op, Reg r0, double d) {
    return encode_param_form(erfin::ParamForm::REG_IMMD) |
           encode_op(op) | encode_reg(r0) | encode_immd(d);
}

Inst encode(OpCode op, Reg r0, Reg r1, double d) {
    return encode_param_form(erfin::ParamForm::REG_REG_IMMD) |
           encode_op(op) | encode_reg_reg(r0, r1) | encode_immd(d);
}

} // end of erfin namespace

// <-------------------------- Level 1 helpers ------------------------------->

namespace {

bool is_line_blank(const std::string & line);

std::vector<std::string> seperate_into_lines(const std::string & str) {
    std::vector<std::string> rv;
    auto old_pos = str.begin();
    auto itr = str.begin();
    for (; itr != str.end(); ++itr) {
        switch (*itr) {
        case '\n': case '\r':
            if (old_pos != str.end()) {
                rv.push_back(std::string());
                rv.back().insert(rv.back().begin(), old_pos, itr);
                old_pos = str.end();
            }
            break;
        default:
            if (old_pos == str.end())
                old_pos = itr;
            break;
        }
    }
    if (itr != old_pos && old_pos != str.end()) {
        rv.push_back(std::string());
        rv.back().insert(rv.back().begin(), old_pos, itr);
    }
    return rv;
}

void remove_comments_from(std::string & str) {
    bool overwrite_until_end = false;
    for (char & c : str) {
        switch (c) {
        case '#':
            overwrite_until_end = true;
            break;
        case '\n': case '\r':
            overwrite_until_end = false;
            break;
        default:
            if (overwrite_until_end)
                c = '#';
            break;
        }
    }
    str.erase(std::remove(str.begin(), str.end(), '#'), str.end());
}

std::vector<std::string> tokenize(const std::vector<std::string> & lines) {
    for (const std::string & str : lines) {
        if (str.empty())
            assert(!"Lines must not be blank.");
    }
    std::vector<std::string> tokens;
    for (const std::string & str : lines) {
        auto old_pos = str.end();
        auto itr = str.begin();
        for (; itr != str.end(); ++itr) {
            // check for end of words
            if (old_pos != str.end()) {
                switch (*itr) {
                case ':': case ' ': case '\t':
                    tokens.push_back(std::string { old_pos, itr });
                    old_pos = str.end();
                    break;
                default: break;
                }
            }
            switch (*itr) {
            // label declarations
            // "operators" :[]
            case ':': case '[': case ']':
                // end of word processed by
                tokens.push_back(std::string { *itr });
                break;
            case '\n': case '\r':
                assert(!"Should not see newline chars at this stage.");
                break;
            case ' ': case '\t': break;
            default:
                if (old_pos == str.end())
                    old_pos = itr;
                break;
            }
        }
        if (old_pos != str.end()) {
            tokens.push_back(std::string { old_pos, itr });
        }
        tokens.push_back("\n");
    }
    return tokens;
}

void remove_blank_strings(std::vector<std::string> & strings) {
    auto itr = std::remove_if
        (strings.begin(), strings.end(), is_line_blank);
    strings.erase(itr, strings.end());
}

void convert_to_lower_case(std::string & str) {
    static_assert(int('a') > int('A'), "");
    for (char & c : str) {
        if (c >= 'A' && c <= 'Z')
            c -= ('a' - 'A');
    }
}

// <---------------------------- level 2 helpers ----------------------------->

using LineToInstFunc = StringCIter(*)(TextProcessState & state,
                                      StringCIter beg, StringCIter end);

// each of the line to inst functions should have uniform requirements
LineToInstFunc get_line_processing_func
    (SuffixAssumption assumptions, const std::string & fname);

StringCIter process_data
    (TextProcessState & state, StringCIter beg, StringCIter end);

StringCIter process_label
    (TextProcessState & state, StringCIter beg, StringCIter end);

void process_text(TextProcessState & state, StringCIter beg, StringCIter end) {
    if (beg == end) return;
    auto func = get_line_processing_func(*state.assumptions, *beg);
    if (func) {
        process_text(state, ++func(state, beg, end), end);
    } else if (*beg == "data") {
        process_text(state, process_data(state, beg, end), end);
    } else if (*beg == ":") {
        process_text(state, process_label(state, beg, end), end);
    }
}

int get_file_size(const char * filename) {
    struct stat stat_buf;
    int rc = stat(filename, &stat_buf);
    return rc == 0 ? int(stat_buf.st_size) : -1;
}

void resolve_unfulfilled_labels(TextProcessState & state) {
#   if 1//def MACRO_DEBUG
    if (state.unfulfilled_labels.size() > 1) {
        for (std::size_t i = 0; i != state.unfulfilled_labels.size() - 1; ++i) {
            const auto & cont = state.unfulfilled_labels;
            assert(cont[i].program_location < cont[i + 1].program_location);
        }
    }
#   endif
    for (auto & unfl_pair : state.unfulfilled_labels) {
        auto itr = state.labels.find(unfl_pair.label);
        if (itr == state.labels.end()) {
            throw Error("Label on line: ??, \"" + unfl_pair.label +
                        "\" not found anywhere in source code.");
        }
        state.program_data[unfl_pair.program_location] |=
            itr->second.program_location;
    }
    state.unfulfilled_labels.clear();
}

bool is_line_blank(const std::string & line) {
    for (char c : line) {
        switch (c) {
        case ' ': case '\t': case '\r': case '\n': break;
        default : return false;
        }
    }
    return true;
}

// <--------------------------- level 3 helpers ------------------------------>

StringCIter process_binary
    (TextProcessState & state, StringCIter beg, StringCIter end);
void skip_new_lines(TextProcessState & state, StringCIter * itr);
erfin::Reg string_to_register(const std::string & str);
Error make_error(TextProcessState & state, const std::string & str);

StringCIter process_data
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    if (beg == end) {
        throw make_error(state, ": stray data directive found at the end of "
                                "the source code.");
    }
    auto process_func = process_binary;
    if (*beg != "[") {
        if (*++beg == "binary") {
            // default
        } else {
            throw make_error(state, ": encoding scheme \"" + *beg + "\" not "
                                    "recognized.");
        }
        ++beg;
    }
    skip_new_lines(state, &beg);
    if (*beg != "[") {
        throw make_error(state, ": expected square bracket to indicate the "
                                "start of data.");
    }
    ++beg;
    return process_func(state, beg, end);
}

StringCIter process_label
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    assert(*beg == ":");
    ++beg;
    skip_new_lines(state, &beg);
    if (beg == end) {
        throw make_error(state, ": Code ends before a label was given for "
                                "the label directive.");
    }
    skip_new_lines(state, &beg);
    if (string_to_register(*beg) != erfin::enum_types::REG_COUNT) {
        throw make_error(state, ": register cannot be used as a label.");
    }
    auto itr = state.labels.find(*beg);
    if (itr == state.labels.end()) {
        state.labels[*beg] = TextProcessState::LabelPair {
            state.program_data.size(),
            state.current_source_line
        };
    } else {
        throw make_error(state, ": dupelicate label, previously defined "
                         "on line: " + std::to_string(itr->second.source_line));
    }
    return ++beg;
}

void skip_new_lines(TextProcessState & state, StringCIter * itr) {
    while (**itr == "\n") {
        ++(*itr);
        ++state.current_source_line;
    }
}

// <--------------------------- level 4 helpers ------------------------------>

// <----------------------- Arithmetic operations ---------------------------->

StringCIter make_plus(TextProcessState &, StringCIter, StringCIter);

StringCIter make_minus(TextProcessState &, StringCIter, StringCIter);

StringCIter make_multiply(TextProcessState &, StringCIter, StringCIter);

StringCIter make_divmod(TextProcessState &, StringCIter, StringCIter);

StringCIter make_multiply_fp(TextProcessState &, StringCIter, StringCIter);

StringCIter make_divmod_fp(TextProcessState &, StringCIter, StringCIter);

// <------------------------- Logic operations ------------------------------->

StringCIter make_and(TextProcessState &, StringCIter, StringCIter);

StringCIter make_or(TextProcessState &, StringCIter, StringCIter);

StringCIter make_xor(TextProcessState &, StringCIter, StringCIter);
#if 0
StringCIter make_and_msb(TextProcessState &, StringCIter, StringCIter);

StringCIter make_or_msb(TextProcessState &, StringCIter, StringCIter);

StringCIter make_xor_msb(TextProcessState &, StringCIter, StringCIter);
#endif
StringCIter make_not(TextProcessState &, StringCIter, StringCIter);

StringCIter make_rotate(TextProcessState &, StringCIter, StringCIter);

// <--------------------- flow control operations ---------------------------->

StringCIter make_cmp(TextProcessState &, StringCIter, StringCIter);

StringCIter make_skip(TextProcessState &, StringCIter, StringCIter);

// <------------------------- move operations -------------------------------->

StringCIter make_set(TextProcessState &, StringCIter, StringCIter);

StringCIter make_load(TextProcessState &, StringCIter, StringCIter);

StringCIter make_save(TextProcessState &, StringCIter, StringCIter);

LineToInstFunc get_line_processing_func
    (SuffixAssumption assumptions, const std::string & fname)
{
    static bool is_initialized = false;
    static std::map<std::string, LineToInstFunc> fmap;
    if (is_initialized) {
        auto itr = fmap.find(fname);
        return (itr == fmap.end()) ? nullptr : itr->second;
    }

    fmap["and"] = fmap["&"] = make_and;
    fmap["or" ] = fmap["|"] = make_or ;
    fmap["xor"] = fmap["^"] = make_xor;

    // suffixes
#   if 0
    fmap["and.lsb"] = fmap["&.lsb"] = make_and;
    fmap["or.lsb" ] = fmap["|.lsb"] = make_or ;
    fmap["xor.lsb"] = fmap["^.lsb"] = make_xor;
    fmap["and.msb"] = fmap["&.msb"] = make_and_msb;
    fmap["or.msb" ] = fmap["|.msb"] = make_or_msb ;
    fmap["xor.msb"] = fmap["^.msb"] = make_xor_msb;
#   endif
    fmap["not"] = fmap["!"] = fmap["~"] = make_not;

    fmap["plus" ] = fmap["add"] = fmap["+"] = make_plus;
    fmap["minus"] = fmap["sub"] = fmap["-"] = make_minus;

    fmap["times"] = fmap["mul"] = fmap["multiply"] = fmap["*"] = make_multiply_fp;
    fmap["div"] = fmap["divmod"  ] = fmap["/"] = make_divmod_fp;

    // suffixes
    fmap["times.int"] = fmap["mul.int"] = fmap["multiply.int"] = fmap["*.int"]
        = make_multiply;
    fmap["times.fp" ] = fmap["mul.fp" ] = fmap["multiply.fp" ] = fmap["*.fp" ]
        = make_multiply_fp;
    fmap["div.int"] = fmap["divmod.int"  ] = fmap["/.int"] = make_divmod;
    fmap["div.fp" ] = fmap["divmod.fp"   ] = fmap["/.fp" ] = make_divmod_fp;

    fmap["comp"] = fmap["compare"] = fmap["cmp"] = fmap["<>="] = make_cmp;
    fmap["skip"] = fmap["?"] = make_skip;

    // for short hand, memory is to be thought of as on the left of the
    // instruction.
    fmap["save"] = fmap["sav"] = fmap["<<"] = make_save;
    fmap["load"] = fmap["ld" ] = fmap[">>"] = make_load;

    fmap["set"] = fmap["="] = make_set;
    fmap["rotate"] = fmap["rot"] = fmap["@"] = make_rotate;
    is_initialized = true;
    return get_line_processing_func(assumptions, fname);
}

Error make_error(TextProcessState & state, const std::string & str) {
    return Error("On line " + std::to_string(state.current_source_line) + str);
}

StringCIter process_binary
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    static constexpr const char * const BAD_CHAR_MSG =
        ": binary encodings only handle five characters '1','x' for one and "
        "'_', 'o', '0' for zero.";
    static constexpr const char * const SOURCE_ENDED_TOO_SOON_MSG =
        ": source code ended without ending the current data sequence, is must "
        "be closed with a \"]\"";
    int bit_pos = 0;
    assert(state.data.empty());
    while (*beg != "]") {
        for (char c : *beg) {
            switch (c) {
            case '_': case 'o': case '0': case '.': case '1': case 'x':
                if (bit_pos == 0)
                    state.data.push_back(0);
                break;
            default: break;
            }
            switch (c) {
            case '1': case 'x':
                state.data.back() |= (1 << (31 - bit_pos));
                MACRO_FALLTHROUGH;
            case '_': case 'o': case '0': case '.':
                bit_pos = (bit_pos + 1) % 32;
                break;
            // characters we should never see
            case '\n':
                if (beg->size() == 1) break;
                MACRO_FALLTHROUGH;
            case '[': case ']': assert(false); break;
            default: throw make_error(state, BAD_CHAR_MSG);
            }
        }
        ++beg;
        if (beg == end) {
            throw make_error(state, SOURCE_ENDED_TOO_SOON_MSG);
        }
    }
    if (bit_pos != 0) {
        throw make_error(state, "All data sequences must be divisible by 32 "
                         "bits, this data sequence is off by " +
                         std::to_string(32 - bit_pos) + "bits.");
    }
    for (UInt32 datum : state.data) {
        state.program_data.push_back(erfin::Inst(datum));
    }
    state.data.clear();
    return ++beg;
}

// <--------------------------- level 5 helpers ------------------------------>

enum TokenClassification {
    REGISTER,
    IMMEDIATE_INTEGER,
    IMMEDIATE_FIXED_POINT,
    INVALID_CLASS
};

enum NumericClassification {
    INTEGER,
    DECIMAL,
    NOT_NUMERIC
};

enum ExtendedParamForm {
    XPF_4R,
    XPF_3R,
    XPF_2R_INT,
    XPF_2R_FP,
    XPF_2R_LABEL,
    XPF_2R,
    XPF_1R_INT,
    XPF_1R_FP,
    XPF_1R_LABEL,
    XPF_1R,
    XPF_INVALID
};

StringCIter make_generic_logic
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end);

StringCIter make_generic_arithemetic
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end);

StringCIter make_generic_divmod
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end);

StringCIter make_generic_memory_access
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end);

bool op_code_supports_fpoint_immd(erfin::Inst inst);

bool op_code_supports_integer_immd(erfin::Inst inst);

NumericClassification convert_to_number_strict
    (StringCIter itr, double & d, int & i);

ExtendedParamForm get_lines_param_form(StringCIter beg, StringCIter end);

// <----------------------- Arithmetic operations ---------------------------->

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

StringCIter make_divmod
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_divmod(erfin::OpCode::DIVIDE_MOD, state, beg, end);
}

StringCIter make_multiply_fp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_arithemetic(erfin::OpCode::TIMES_FP, state, beg, end);
}

StringCIter make_divmod_fp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_divmod(erfin::OpCode::DIV_MOD_FP, state, beg, end);
}

// <------------------------- Logic operations ------------------------------->

StringCIter get_eol(StringCIter beg, StringCIter end);

StringCIter make_and
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::AND, state, beg, end); }

StringCIter make_or
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::OR, state, beg, end); }

StringCIter make_xor
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::XOR, state, beg, end); }
#if 0
StringCIter make_and_msb
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::AND_MSB, state, beg, end); }

StringCIter make_or_msb
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::OR_MSB, state, beg, end); }

StringCIter make_xor_msb
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::XOR_MSB, state, beg, end); }
#endif
StringCIter make_not
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;
    ++beg;
    auto eol = get_eol(beg, end);
    switch (get_lines_param_form(beg, eol)) {
    case XPF_1R:
        state.program_data.push_back(encode(NOT ,string_to_register(*beg)));
        ++state.current_source_line;
        return eol;
    default:
        throw make_error(state, ": exactly one argument permitted for logical "
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
    union {
        int i;
        double d;
    } u;
    switch (get_lines_param_form(beg, eol)) {
    case XPF_1R_INT:
        switch (convert_to_number_strict(beg + 1, u.d, u.i)) {
        case INTEGER: break;
        default:
            throw make_error(state, NON_INT_IMMD_MSG);
        }
        state.program_data.push_back(encode(ROTATE, string_to_register(*beg),
                                            u.i                             ));
        break;
    case XPF_2R:
        state.program_data.push_back(encode(ROTATE, string_to_register(*beg),
                                            string_to_register(*(beg + 1))));
        break;
    default: throw make_error(state, INVALID_PARAMS_MSG);
    }
    ++state.current_source_line;
    return eol;
}

StringCIter make_cmp
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    return make_generic_logic(erfin::OpCode::COMP, state, beg, end);
}

StringCIter make_skip
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;
    ++beg;
    auto eol = get_eol(beg, end);
    Reg r1;
    int i = 0;
    double d;
    NumericClassification numclass = INTEGER;

    switch (get_lines_param_form(beg, eol)) {
    case XPF_1R_LABEL:
        state.unfulfilled_labels.emplace_back
            (state.program_data.size(), *(beg + 1));
        state.program_data.push_back
            (encode_op(SKIP) | encode_param_form(REG_IMMD) |
             encode_reg(string_to_register(*beg)));
        ++state.current_source_line;
        return eol;
    case XPF_1R_FP: case XPF_1R_INT:
        numclass = convert_to_number_strict(beg + 1, d, i);
        MACRO_FALLTHROUGH;
    case XPF_1R:
        r1 = string_to_register(*beg);
        break;
    default: throw make_error(state, ": unsupported parameters.");
    }

    if (numclass == INTEGER) {
        state.program_data.push_back(encode(SKIP, r1, i));
    } else if (numclass == NOT_NUMERIC) {
        state.unfulfilled_labels.emplace_back
            (state.program_data.size(), *(beg + 1));
        state.program_data.push_back
            (encode_param_form(ParamForm::REG_IMMD) |
             encode_op(erfin::OpCode::SKIP) | encode_reg(r1));
    } else {
        throw make_error(state, ": a fixed point is not an appropiate offset.");
    }

    ++state.current_source_line;
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
    int i;
    double d;
    switch (get_lines_param_form(beg, line_end)) {
    case XPF_2R:
        inst = encode(SET_REG, itr2reg(beg), itr2reg(beg + 1));
        break;
    case XPF_1R_INT:
        convert_to_number_strict(beg + 1, d, i);
        inst = encode(SET_INT, itr2reg(beg), i);
        break;
    case XPF_1R_FP:
        convert_to_number_strict(beg + 1, d, i);
        inst = encode(SET_FP96, itr2reg(beg), d);
        break;
    case XPF_1R_LABEL:
        state.unfulfilled_labels.emplace_back(state.program_data.size(), *(beg + 1));
        inst = encode_op(SET_INT) | encode_param_form(REG_IMMD) |
               encode_reg(itr2reg(beg));
        break;
    default:
        throw make_error(state, ": set instruction may only have exactly "
                                 "two arguments.");
    }
    state.program_data.push_back(inst);
    ++state.current_source_line;
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

// <--------------------------- level 6 helpers ------------------------------>

erfin::ParamForm narrow_param_form(ExtendedParamForm xpf);

NumericClassification convert_to_number_strict
    (StringCIter itr, double & d, int & i)
{
    bool has_dec = false;
    for (char c : *itr) {
        if (c == '.') {
            has_dec = true;
            break;
        }
    }
    if (has_dec) {
        if (string_to_number<10>(&*itr->begin(), &*itr->end(), d))
            return DECIMAL;
    } else {
        if (string_to_number<10>(&*itr->begin(), &*itr->end(), i))
            return INTEGER;
    }
    return NOT_NUMERIC;
}

ExtendedParamForm get_lines_param_form(StringCIter beg, StringCIter end) {
    using namespace erfin;
    using namespace erfin::enum_types;
    // this had better not be the begging of the line
    assert(!get_line_processing_func(erfin::Assembler::NO_ASSUMPTION, *beg));

    // naturally anyone can read this (oh god it's shit)
    const int arg_count = int(end - beg);
    union {
        int i;
        double d;
    } u;
    switch (arg_count) {
    case 4:
        for (int i = 0; i != 4; ++i)
            if (string_to_register(*(beg + i)) == REG_COUNT)
                return XPF_INVALID;
        return XPF_4R;
    case 3: case 2:
        for (int i = 0; i != arg_count - 1; ++i)
            if (string_to_register(*(beg + i)) == REG_COUNT)
                return XPF_INVALID;
        if (string_to_register(*(beg + arg_count - 1)) != REG_COUNT) {
            return arg_count == 2 ? XPF_2R : XPF_3R;
        }
        switch (convert_to_number_strict(beg + arg_count - 1, u.d, u.i)) {
        case INTEGER: return arg_count == 2 ? XPF_1R_INT : XPF_2R_INT;
        case DECIMAL: return arg_count == 2 ? XPF_1R_FP  : XPF_2R_FP ;
        default: return arg_count == 2 ? XPF_1R_LABEL : XPF_2R_LABEL ;
        }
    case 1: // reg only
        return (string_to_register(*beg) != REG_COUNT) ? XPF_1R : XPF_INVALID;
    default: return XPF_INVALID;
    }
}

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
    static constexpr const char * const fp_unsupported_msg =
        ": instruction does not support fixed point immediates.";
    static constexpr const char * const int_unsupported_msg =
        ": instruction does not support integer immediates.";

    Reg a1;
    ++beg;
    auto eol = get_eol(beg, end);
    const auto pf = get_lines_param_form(beg, eol);

    switch (pf) {
    case XPF_2R: case XPF_3R: case XPF_1R_FP: case XPF_1R_INT:
    case XPF_1R_LABEL:
        a1 = string_to_register(*beg);
        break;
    default:
        throw make_error(state, ": unsupported parameters.");
    }

    Reg a2, ans;
    int i;
    double d;
    bool supports_fpoints  = op_code_supports_fpoint_immd (op_code);
    bool supports_integers = op_code_supports_integer_immd(op_code);

    switch (pf) {
    case XPF_2R: // psuedo instruction
        a2 = ans = string_to_register(*(beg + 1));
        state.program_data.push_back(encode(op_code, a1, a2, ans));
        break;
    case XPF_3R:
        a2  = string_to_register(*(beg + 1));
        ans = string_to_register(*(beg + 2));
        state.program_data.push_back(encode(op_code, a1, a2, ans));
        break;
    case XPF_1R_FP:
        if (!supports_fpoints)
            throw make_error(state, fp_unsupported_msg);
        string_to_number<10>(&*(beg + 1)->begin(), &*(beg + 1)->end(), d);
        state.program_data.push_back(encode(op_code, a1, a1, d));
        break;
    case XPF_1R_INT:
        if (!supports_integers)
            throw make_error(state, int_unsupported_msg);
        string_to_number<10>(&*(beg + 1)->begin(), &*(beg + 1)->end(), i);
        state.program_data.push_back(encode(op_code, a1, a1, i));
        break;
    case XPF_1R_LABEL:
        if (!supports_integers)
            throw make_error(state, int_unsupported_msg);
        state.unfulfilled_labels.emplace_back
            (state.program_data.size(), *(beg + 1));
        state.program_data.push_back
            (encode_op(op_code) | encode_param_form(REG_IMMD) | encode_reg(a1));
        break;
    default: assert(false); break;
    }    
    ++state.current_source_line;
    return eol;
}

StringCIter make_generic_divmod
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;
    assert(op_code == DIV_MOD_FP || op_code == DIVIDE_MOD);
    auto eol = get_eol(beg, end);
    ++beg;
    Reg n, d, q, r;
    switch (get_lines_param_form(beg, eol)) {
    case XPF_4R:
        n = string_to_register(*(beg));
        d = string_to_register(*(beg + 1));
        q = string_to_register(*(beg + 2));
        r = string_to_register(*(beg + 3));
        break;
    case XPF_3R:
        n = string_to_register(*(beg));
        d = string_to_register(*(beg + 1));
        r = q = string_to_register(*(beg + 2));
        break;
    case XPF_2R:
        // q := n / d also a := a / b
        // so q == n
        // quotent will have to be written last
        r = q = n = string_to_register(*(beg));
        d = string_to_register(*(beg + 1));
        break;
    default: throw make_error(state, ": unsupported parameters.");
    }
    state.program_data.push_back(encode_op(op_code) |
                                 encode_reg_reg_reg_reg(n, d, q, r));
    ++state.current_source_line;
    return eol;
}

StringCIter make_generic_memory_access
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;
    assert(op_code == LOAD || op_code == SAVE);

    // too lazy to seperate function
    const auto int_immd_only =
    [] (TextProcessState & state, StringCIter itr) -> UInt32 {
        int i;
        double d;
        switch (convert_to_number_strict(itr, d, i)) {
        case DECIMAL:
            throw make_error(state, ": fixed points are not valid offsets.");
        case INTEGER: return encode_immd(i);
        default: assert(false); break;
        }
    };

    const auto eol = get_eol(++beg, end);
    assert(!get_line_processing_func(Assembler::NO_ASSUMPTION, *beg));
    Reg reg;
    Reg addr_reg = REG_COUNT;
    int arg_count = 2;
    const auto pf = get_lines_param_form(beg, eol);
    switch (pf) {
    case XPF_2R: case XPF_2R_INT: case XPF_2R_LABEL:
        ++arg_count;
        addr_reg = string_to_register(*(beg + 1));
        MACRO_FALLTHROUGH;
    case XPF_1R: case XPF_1R_INT: case XPF_1R_LABEL:
        reg = string_to_register(*beg);
        break;
    default:
        throw make_error(state, ": unsupported parameters.");
    }

    UInt32 immd = 0;
    switch (pf) {
    case XPF_1R:
        if (op_code == SAVE)
            throw make_error(state, ": deference psuedo instruction only available for loading.");
        addr_reg = reg;
        break;
    case XPF_1R_LABEL: case XPF_2R_LABEL:
        state.unfulfilled_labels.emplace_back
            (state.program_data.size(),*(beg + arg_count - 1));
        break;
    case XPF_1R_INT: case XPF_2R_INT:
        immd = int_immd_only(state, beg + arg_count - 1);
        break;
    case XPF_2R: break;
    default: assert(false); break;
    }

    Inst inst = encode_param_form(narrow_param_form(pf)) | encode_op(op_code) | immd;
    if (arg_count == 2) {
        inst |= encode_reg(reg);
    } else if (arg_count == 3) {
        assert(addr_reg != REG_COUNT);
        inst |= encode_reg_reg(reg, addr_reg);
    } else {
        assert(false);
    }
    state.program_data.push_back(inst);
    ++state.current_source_line;
    return eol;
}

bool op_code_supports_fpoint_immd(erfin::Inst inst) {
    using namespace erfin::enum_types;
    switch (inst) {
    case PLUS: case MINUS: case TIMES_FP: case DIV_MOD_FP:
        return true;
    case TIMES: case AND: case XOR: case OR: case DIVIDE_MOD: case COMP:
        return false;
    default: assert(false); break;
    }
}

bool op_code_supports_integer_immd(erfin::Inst inst) {
    using namespace erfin::enum_types;
    switch (inst) {
    case PLUS: case MINUS: case TIMES: case AND: case XOR: case OR:
    case DIVIDE_MOD: case COMP:
        return true;
    case TIMES_FP: case DIV_MOD_FP:
        return false;
    default: assert(false); break;
    }
}

erfin::Reg string_to_register(const std::string & str) {
    using namespace erfin::enum_types;
    if (str.empty()) return REG_COUNT;
    auto rv = REG_COUNT;
    switch (str[0]) {
    case 'x': rv = REG_X    ; break;
    case 'y': rv = REG_Y    ; break;
    case 'z': rv = REG_Z    ; break;
    case 'a': rv = REG_A    ; break;
    case 'b': rv = REG_B    ; break;
    case 'c': rv = REG_C; break;
    default: break;
    }
    if (rv != REG_COUNT)
        return str.size() == 1 ? rv : REG_COUNT;
    if (str == "sp") {
        return REG_BP;
    } else if (str == "pc") {
        return REG_PC;
    } else {
        return REG_COUNT;
    }
}

StringCIter get_eol(StringCIter beg, StringCIter end) {
    while (*beg != "\n") {
        ++beg;
        if (beg == end) break;
    }
    return beg;
}

// <--------------------------- level 7 helpers ------------------------------>

erfin::ParamForm narrow_param_form(ExtendedParamForm xpf) {
    using namespace erfin::enum_types;
    switch (xpf) {
    case XPF_4R: return REG_REG_REG_REG;
    case XPF_3R: return REG_REG_REG;
    case XPF_2R_INT: case XPF_2R_FP: case XPF_2R_LABEL:
        return REG_REG_IMMD;
    case XPF_2R: return REG_REG;
    case XPF_1R_INT: case XPF_1R_FP: case XPF_1R_LABEL:
        return REG_IMMD;
    case XPF_1R: return REG;
    case XPF_INVALID: return INVALID_PARAMS;
    }
    assert(false);
    throw Error("Impossible branch?!");
}

} // end of anonymous namespace

void erfin::Assembler::run_tests() {
    using namespace erfin;
    // with such a deep call tree, unit tests on each function on each level
    // becomes quite useful
    TextProcessState state;
    // data encodings
    {
    const std::vector<std::string> sample_binary = {
        "____xxxx", "____x_xxx___x__x", "xx__x_x_", "\n",
        "]"
    };
    (void)process_binary(state, sample_binary.begin(), sample_binary.end());
    assert(state.program_data.back() == 252414410);
    }
    {
    const std::vector<std::string> sample_data = {
        "data", "binary", "[", "\n",
            "____xxxxxx__x_x_", "\n", // 4042
            "___x_xxx____x__x", "\n", // 5897
        "]"
    };
    (void)process_data(state, sample_data.begin(), sample_data.end());
    assert(state.program_data.back() == 264902409);
    }
    {
    const std::vector<std::string> sample_label = {
        ":", "hello", "and", "x", "y", "\n"
                    , "jump", "hello"
    };
    (void)process_label(state, sample_label.begin(), sample_label.end());
    assert(state.labels.find("hello") != state.labels.end());
    }
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
    auto supposed_top = encode(OpCode::SET_FP96, Reg::REG_X, 12.34);
    assert(state.program_data.back() == supposed_top);
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
    const auto supposed_top = encode(OpCode::MINUS, Reg::REG_X, Reg::REG_X, 123);
    assert(supposed_top == state.program_data.back());
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
    const auto supposed_top = encode(OpCode::SAVE, Reg::REG_Y, Reg::REG_A, 4);
    assert(supposed_top == state.program_data.back());
    }
    {
    const std::vector<std::string> sample_code = {
        "<>=", "x", "y", "\n",
        "?", "x", "1"
    };
    auto beg = sample_code.begin(), end = sample_code.end();
    beg = make_cmp (state,   beg, end);
    beg = make_skip(state, ++beg, end);
    const auto supposed_top = encode(OpCode::SKIP, Reg::REG_X, 1);
    assert(supposed_top == state.program_data.back());
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
    beg = process_label(state, ++beg, end);
    beg = process_label(state,   beg, end);
    beg = make_plus    (state,   beg, end);
    beg = make_minus   (state, ++beg, end);
    resolve_unfulfilled_labels(state);
    assert(state.program_data[0] == encode(OpCode::SET_INT, Reg::REG_PC, 2));
    assert(state.program_data[1] == encode(OpCode::LOAD   , Reg::REG_X , 2));
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
    assert(pdata[0] == encode(OpCode::SET_FP96, Reg::REG_X, 1.0 ));
    assert(pdata[1] == encode(OpCode::SET_FP96, Reg::REG_Y, 1.44));
    assert(pdata[2] == encode(OpCode::PLUS    , Reg::REG_X, Reg::REG_Y, Reg::REG_X));
    assert(pdata[3] == encode(OpCode::SET_INT , Reg::REG_PC, 2));
    }
}

namespace {
    const LineToInstFunc init_f = get_line_processing_func
                                  (erfin::Assembler::NO_ASSUMPTION, "");
}  // end of anonymous namespace
