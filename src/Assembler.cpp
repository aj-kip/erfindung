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

namespace {

using StringCIter = std::vector<std::string>::const_iterator;
using UInt32 = erfin::UInt32;
using Error = std::runtime_error;
using SuffixAssumption = erfin::Assembler::SuffixAssumption;

struct TextProcessState {
    TextProcessState(): in_square_brackets(false), in_data_directive(false),
                        current_source_line(1), assumptions(nullptr) {}
    bool in_square_brackets;
    bool in_data_directive;
    std::size_t current_source_line;

    std::vector<UInt32> data;
    struct LabelPair { std::size_t program_location, source_line; };
    std::map<std::string, LabelPair> labels;
    std::vector<erfin::Inst> program_data;
    std::vector<LabelPair> unfulfilled_labels;
    const SuffixAssumption * assumptions;
};

// high level textual processing
std::vector<std::string> seperate_into_lines(const std::string & str);

void remove_comments_from(std::string & str);

std::vector<std::string> tokenize(const std::vector<std::string> & lines);

void remove_blank_strings(std::vector<std::string> & strings);

void convert_to_lower_case(std::string & str);

void process_text(TextProcessState & state, StringCIter beg, StringCIter end);

int get_file_size(const char * filename) {
    struct stat stat_buf;
    int rc = stat(filename, &stat_buf);
    return rc == 0 ? int(stat_buf.st_size) : -1;
}

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
    // only when a valid program has been assembled do we swap in the actual
    // instructions, as a throw may occur at any point of the text processing
    m_program.swap(tpstate.program_data);
}

UInt32 encode_immd(double d) { return to_fixed_point(d); }

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
        process_text(state, func(state, beg, end), end);
    } else if (*beg == "data") {
        process_text(state, process_data(state, beg, end), end);
    } else if (*beg == ":") {
        process_text(state, process_label(state, beg, end), end);
    }
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
    return beg;
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

StringCIter make_and_msb(TextProcessState &, StringCIter, StringCIter);

StringCIter make_or_msb(TextProcessState &, StringCIter, StringCIter);

StringCIter make_xor_msb(TextProcessState &, StringCIter, StringCIter);

StringCIter make_not(TextProcessState &, StringCIter, StringCIter);

// <--------------------- flow control operations ---------------------------->

StringCIter make_cmp(TextProcessState &, StringCIter, StringCIter);

StringCIter make_skip(TextProcessState &, StringCIter, StringCIter);

// <------------------------- move operations -------------------------------->

StringCIter make_set(TextProcessState &, StringCIter, StringCIter);

StringCIter make_load(TextProcessState &, StringCIter, StringCIter);

StringCIter make_save(TextProcessState &, StringCIter, StringCIter);

LineToInstFunc get_line_processing_func(SuffixAssumption assumptions, const std::string & fname) {
    static bool is_initialized = false;
    static std::map<std::string, LineToInstFunc> fmap;
    if (is_initialized) {
        auto itr = fmap.find(fname);
        return itr->second;
    }

    fmap["and"] = fmap["&"] = make_and;
    fmap["or" ] = fmap["|"] = make_or ;
    fmap["xor"] = fmap["^"] = make_xor;

    // suffixes
    fmap["and.lsb"] = fmap["&.lsb"] = make_and;
    fmap["or.lsb" ] = fmap["|.lsb"] = make_or ;
    fmap["xor.lsb"] = fmap["^.lsb"] = make_xor;
    fmap["and.msb"] = fmap["&.msb"] = make_and_msb;
    fmap["or.msb" ] = fmap["|.msb"] = make_or_msb ;
    fmap["xor.msb"] = fmap["^.msb"] = make_xor_msb;

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

    fmap["cmp"] = fmap["<>="] = make_cmp;
    fmap["skip"] = fmap["?"] = make_skip;

    // for short hand, memory is to be thought of as on the left of the
    // instruction.
    fmap["save"] = fmap["sav"] = fmap["<<"] = make_save;
    fmap["load"] = fmap["ld" ] = fmap[">>"] = make_load;

    fmap["set"] = fmap["="] = make_set;
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
                // v fall through v
            case '_': case 'o': case '0': case '.':
                bit_pos = (bit_pos + 1) % 32;
                break;
            // characters we should never see
            case '\n':
                if (beg->size() == 1) break;
                // v fall through v
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
    NON_NEGATIVE_INTEGER,
    INTEGER,
    DECIMAL,
    NOT_NUMERIC
};

TokenClassification classify_token(const std::string & str);

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
UInt32 encode_integer_immd_from_iter(StringCIter iter);
UInt32 encode_fpoint_immd_from_iter(StringCIter iter);

erfin::ParamForm get_lines_param_form(StringCIter beg, StringCIter end);

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

StringCIter make_and_msb
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::AND_MSB, state, beg, end); }

StringCIter make_or_msb
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::OR_MSB, state, beg, end); }

StringCIter make_xor_msb
    (TextProcessState & state, StringCIter beg, StringCIter end)
{ return make_generic_logic(erfin::OpCode::XOR_MSB, state, beg, end); }

StringCIter make_not
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;
    ++beg;
    auto eol = get_eol(beg, end);
    switch (get_lines_param_form(beg, eol)) {
    case REG:
        state.program_data.push_back(encode_op(NOT) |
                                     encode_reg(string_to_register(*beg)));
        break;
    default:
        throw make_error(state, ": exactly one argument permitted for logical "
                                "complement (not).");
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
    UInt32 immd = 0;
    switch (get_lines_param_form(beg, eol)) {
    case REG_IMMD:
        switch (classify_token(*(beg + 1))) {
        case IMMEDIATE_FIXED_POINT:
            immd = encode_fpoint_immd_from_iter(beg + 1);
            break;
        case IMMEDIATE_INTEGER:
            immd = encode_integer_immd_from_iter(beg + 1);
            break;
        default: assert(false); break;
        }
        // v fall through v
    case REG:
        r1 = string_to_register(*beg);
        break;
    default: throw make_error(state, ": unsupported parameters.");
    }
    state.program_data.push_back(encode_op(erfin::OpCode::SKIP) |
                                 encode_reg(r1) | immd);
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

    if (line_end - beg != 2) {
        throw make_error(state, ": set instruction may only have exactly "
                                "two arguments.");
    }
    auto r1id = string_to_register(*beg);
    if (r1id == REG_COUNT) {
        throw make_error(state, ": in all forms the first argument must be a "
                                "register.");
    }
    auto a2t  = classify_token(*(beg + 1));
    switch (a2t) {
    case REGISTER:
        assert(string_to_register(*(beg + 1)) != REG_COUNT);
        state.program_data.push_back(encode_op(SET_REG) |
            erfin::encode_reg_reg(r1id, string_to_register(*(beg + 1)))
        );
        break;
    case IMMEDIATE_FIXED_POINT:
        state.program_data.push_back(encode_op(SET_FP96) |
                                     erfin::encode_reg(r1id) |
                                     encode_fpoint_immd_from_iter(beg + 1));
        break;
    case IMMEDIATE_INTEGER:
        state.program_data.push_back(encode_op(SET_INT) |
                                     erfin::encode_reg(r1id) |
                                     encode_integer_immd_from_iter(beg + 1));
        break;
    case INVALID_CLASS:
        throw make_error(state, ": labels may not be used for set operations, "
                                "perhaps a \"load\" was intented?");
    }
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

erfin::ParamForm get_lines_param_form(StringCIter beg, StringCIter end) {
    using namespace erfin;
    using namespace erfin::enum_types;
    // this had better not be the begging of the line
    assert(!get_line_processing_func(erfin::Assembler::NO_ASSUMPTION, *beg));

    // naturally anyone can read this (oh god it's shit)
    switch (int(end - beg)) {
    case 4:
        for (int i = 0; i != 4; ++i)
            if (classify_token(*(beg + i)) != REGISTER)
                return INVALID_PARAMS;
        return REG_REG_REG_REG;
    case 3: case 2:
        for (int i = 0; i != int(end - beg) - 1; ++i)
            if (classify_token(*(beg + i)) != REGISTER)
                return INVALID_PARAMS;
        switch (classify_token(*(beg + int(end - beg) - 1))) {
        case IMMEDIATE_FIXED_POINT: case IMMEDIATE_INTEGER: case INVALID_CLASS:
            return (int(end - beg) == 3) ? REG_REG_IMMD : REG_IMMD;
        case REGISTER:
            return (int(end - beg) == 3) ? REG_REG_REG : REG_REG;
        }
    case 1: // reg only
        return (classify_token(*beg) == REGISTER) ? REG : INVALID_PARAMS;
    default: return INVALID_PARAMS;
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
    ++beg;
    auto eol = get_eol(beg, end);

    using namespace erfin;
    using namespace erfin::enum_types;
    Reg a1, a2, ans;
    UInt32 immd;
    bool supports_fpoints  = op_code_supports_fpoint_immd (op_code);
    bool supports_integers = op_code_supports_integer_immd(op_code);

    static constexpr const char * const fp_unsupported_msg =
        ": instruction does not support fixed point immediates.";
    static constexpr const char * const int_unsupported_msg =
        ": instruction does not support integer immediates.";
    switch (get_lines_param_form(beg, eol)) {
    case REG_REG: // psuedo instruction
        a1 = string_to_register(*(beg));
        a2 = ans = string_to_register(*(beg + 1));
        break;
    case REG_REG_REG:
        a1  = string_to_register(*(beg));
        a2  = string_to_register(*(beg + 1));
        ans = string_to_register(*(beg + 2));
        break;
    case REG_IMMD:
        // we're leaving!
        a1 = string_to_register(*(beg));
        // both fp and integer immediates supported (maybe)
        switch (classify_token(*(beg + 1))) {
        case IMMEDIATE_FIXED_POINT:
            if (!supports_fpoints)
                throw make_error(state, fp_unsupported_msg);
            immd = encode_fpoint_immd_from_iter(beg + 1);
            break;
        case IMMEDIATE_INTEGER:
            if (!supports_integers)
                throw make_error(state, int_unsupported_msg);
            immd = encode_integer_immd_from_iter(beg + 1);
            break;
        case REGISTER: case INVALID_CLASS: assert(false); break;
        }
        state.program_data.push_back(encode_op(op_code) | encode_reg(a1) | immd);
        ++state.current_source_line;
        return eol;
    case REG_REG_IMMD: case REG_REG_REG_REG: case REG: case NO_PARAMS:
        throw make_error(state, ": unsupported parameters.");
    default: assert(false); break;
    }
    state.program_data.push_back(encode_op(op_code) |
                                 encode_reg_reg_reg(a1, a2, ans));
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
    case REG_REG_REG_REG:
        n = string_to_register(*(beg));
        d = string_to_register(*(beg + 1));
        q = string_to_register(*(beg + 2));
        r = string_to_register(*(beg + 3));
        break;
    case REG_REG_REG:
        n = string_to_register(*(beg));
        d = string_to_register(*(beg + 1));
        r = q = string_to_register(*(beg + 2));
        break;
    case REG_REG:
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
    auto eol = get_eol(++beg, end);
    Reg reg;
    Reg addr_reg;
    UInt32 immd = 0;

    // too lazy to seperate function
    const auto int_immd_only =
    [] (TextProcessState & state, StringCIter itr) -> UInt32 {
        switch (classify_token(*itr)) {
        case IMMEDIATE_FIXED_POINT:
            throw make_error(state, ": fixed points are not valid offsets.");
        case IMMEDIATE_INTEGER:
            return encode_integer_immd_from_iter(itr);
        default: assert(false); break;
        }
    };

    switch (get_lines_param_form(beg, eol)) {
    case REG:
        if (op_code == SAVE)
            throw make_error(state, ": deference psuedo instruction only available for loading.");
        reg = addr_reg = string_to_register(*beg);
        break;
    case REG_IMMD:
        // loading/saving from/to a particular address
        reg = string_to_register(*beg);
        immd = int_immd_only(state, beg + 1);
        state.program_data.push_back(encode_op(op_code) | encode_reg(reg) | immd);
        ++state.current_source_line;
        return eol;
    case REG_REG_IMMD:
        immd = int_immd_only(state, beg + 2);
        // v fall through v
    case REG_REG:
        reg = string_to_register(*beg);
        addr_reg = string_to_register(*(beg + 1));
        break;
    default:
        throw make_error(state, ": unsupported parameters.");
    }
    state.program_data.push_back(encode_op(op_code) |
                                 encode_reg_reg(reg, addr_reg) | immd);
    ++state.current_source_line;
    return eol;
}

bool op_code_supports_fpoint_immd(erfin::Inst inst) {
    using namespace erfin::enum_types;
    switch (inst) {
    case PLUS: case MINUS: case TIMES_FP: case DIV_MOD_FP:
        return true;
    case TIMES: case AND: case XOR: case OR: case AND_MSB: case XOR_MSB:
    case OR_MSB: case DIVIDE_MOD: case COMP:
        return false;
    default: assert(false); break;
    }
}

bool op_code_supports_integer_immd(erfin::Inst inst) {
    using namespace erfin::enum_types;
    switch (inst) {
    case PLUS: case MINUS: case TIMES: case AND: case XOR: case OR:
    case AND_MSB: case XOR_MSB: case OR_MSB: case DIVIDE_MOD: case COMP:
        return true;
    case TIMES_FP: case DIV_MOD_FP:
        return false;
    default: assert(false); break;
    }
}

UInt32 encode_integer_immd_from_iter(StringCIter iter) {
    int i;
    bool b = string_to_number<10>(&*iter->begin(), &*iter->end(), i);
    (void)b; assert(b);
    return erfin::encode_immd(i);
}

UInt32 encode_fpoint_immd_from_iter(StringCIter iter) {
    double d;
    bool b = string_to_number<10>(&*iter->begin(), &*iter->end(), d);
    (void)b; assert(b);
    return erfin::encode_immd(d);
}

TokenClassification classify_token(const std::string & str) {
    using namespace erfin::enum_types;
    if (string_to_register(str) != REG_COUNT) return REGISTER;
    double d; int i;
    bool at_least_int = string_to_number<10>(&str[0], &*str.end(), i);
    bool at_least_dbl = string_to_number<10>(&str[0], &*str.end(), d);

    if (at_least_dbl && at_least_int) {
        if (d != double(i))
            return IMMEDIATE_FIXED_POINT;
    }
    if (at_least_int)
        return IMMEDIATE_INTEGER;
    return INVALID_CLASS;
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
    case 'c': rv = REG_FLAGS; break;
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

} // end of anonymous namespace

void erfin::Assembler::run_tests() {
    using namespace erfin;
    // with such a deep call tree, unit tests on each function on each level
    // becomes quite useful
    TextProcessState state;
    // data encodings
    {
    const std::vector<std::string> sample_binary = {
        "____xxxx", "____x_xxx___x__x", "xx__x_x_",
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
    auto supposed_top = encode_op(OpCode::SET_FP96) | encode_reg(Reg::REG_X) | encode_immd(12.34);
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
    const auto supposed_top = encode_op(OpCode::MINUS) | encode_reg_reg(Reg::REG_X, Reg::REG_X) | encode_immd(123);
    assert(supposed_top == state.program_data.back());
    }
    {
    const std::vector<std::string> sample_code = {
        ">>", "x", "9384", "\n",
        "<<", "y", "a", "4",
    };
    auto beg = sample_code.begin(), end = sample_code.end();
    beg = make_load(state, beg, end);
    beg = make_save(state, ++beg, end);
    const auto supposed_top = encode_op(OpCode::SAVE) | encode_reg_reg(Reg::REG_Y, Reg::REG_A) | encode_immd(4);
    assert(supposed_top == state.program_data.back());
    }
}

namespace {
    const LineToInstFunc init_f = get_line_processing_func
                                  (erfin::Assembler::NO_ASSUMPTION, "");
}  // end of anonymous namespace
