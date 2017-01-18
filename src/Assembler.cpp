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
#   error "no compiler defined"
#endif

namespace {

using StringCIter = std::vector<std::string>::const_iterator;
using UInt32 = erfin::UInt32;
using Error = std::runtime_error;
using SuffixAssumption = erfin::Assembler::SuffixAssumption;

struct TextProcessState {
    friend void erfin::Assembler::run_tests();
    using ProgramData = erfin::Assembler::ProgramData;
    TextProcessState(): in_square_brackets(false), in_data_directive(false),
                        current_source_line(1),
                        assumptions(erfin::Assembler::NO_ASSUMPTION) {}
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

    std::vector<UInt32> data;
    std::size_t current_source_line;
    SuffixAssumption assumptions;

    // having some encapsulation, helps me to change the state in predictable
    // ways rather than having to micro-manage everything >.>

    void add_instruction(erfin::Inst inst, const std::string * label = nullptr);

    void swap_program
        (ProgramData & prog, std::vector<std::size_t> & inst_to_line);

    void resolve_unfulfilled_labels();

    StringCIter process_label(StringCIter beg, StringCIter end);

private:
    ProgramData program_data;
    std::vector<std::size_t> m_inst_to_source_line;
    std::vector<UnfilledLabelPair> unfulfilled_labels;
    std::map<std::string, LabelPair> labels;
};

// high level textual processing
std::vector<std::string> seperate_into_lines(const std::string & str);

void remove_comments_from(std::string & str);

std::vector<std::string> tokenize(const std::vector<std::string> & lines);

void convert_to_lower_case(std::string & str);

void process_text(TextProcessState & state, StringCIter beg, StringCIter end);

int get_file_size(const char * filename);

} // end of <anonymous> namespace

namespace erfin {

void Assembler::assemble_from_file(const char * file) {
    m_error_string = "";
    std::string file_contents;
    int fsize = get_file_size(file);
    if (fsize < 0) {
        throw Error("Could not read file contents (file too large/not "
                    "there?).");
    }
    file_contents.reserve(std::size_t(fsize));
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
    // I need some means to sync line numbers with the source code
    //remove_blank_strings(line_list);
    std::vector<std::string> tokens = tokenize(line_list);

    TextProcessState tpstate;

    process_text(tpstate, tokens.begin(), tokens.end());
    tpstate.resolve_unfulfilled_labels();
    // only when a valid program has been assembled do we swap in the actual
    // instructions, as a throw may occur at any point of the text processing
    tpstate.swap_program(m_program, m_inst_to_line_map);
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
    return sign_part | partial | encode_set_is_fixed_point_flag();
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

Inst encode(OpCode op, Reg r0, UInt32 i) {
    return encode_param_form(erfin::ParamForm::REG_IMMD) |
           encode_op(op) | encode_reg(r0) | i;
}

Inst encode(OpCode op, Reg r0, Reg r1, UInt32 i) {
    return encode_param_form(erfin::ParamForm::REG_REG_IMMD) |
           encode_op(op) | encode_reg_reg(r0, r1) | i;
}

Inst encode(OpCode op, Reg r0, int i) {
    return encode(op, r0, encode_immd(i));
}

Inst encode(OpCode op, Reg r0, Reg r1, int i) {
    return encode(op, r0, r1, encode_immd(i));
}

Inst encode(OpCode op, Reg r0, double d) {
    return encode(op, r0, encode_immd(d));
}

Inst encode(OpCode op, Reg r0, Reg r1, double d) {
    return encode(op, r0, r1, encode_immd(d));
}

} // end of erfin namespace

// <-------------------------- Level 1 helpers ------------------------------->

namespace {

std::vector<std::string> seperate_into_lines(const std::string & str) {
    std::vector<std::string> rv;
    auto old_pos = str.begin();
    auto itr = str.begin();
    for (; itr != str.end(); ++itr) {
        switch (*itr) {
        case '\n': case '\r':
            rv.push_back(std::string());
            rv.back().insert(rv.back().begin(), old_pos, itr);
            old_pos = itr + 1;
            break;
        default: break;
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

void convert_to_lower_case(std::string & str) {
    static_assert(int('a') > int('A'), "");
    for (char & c : str) {
        if (c >= 'A' && c <= 'Z')
            c += ('a' - 'A');
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

Error make_error(TextProcessState & state, const std::string & str);

void skip_new_lines(TextProcessState & state, StringCIter * itr, StringCIter end);

void process_text(TextProcessState & state, StringCIter beg, StringCIter end) {
    if (beg == end) return;
    skip_new_lines(state, &beg, end);
    auto func = get_line_processing_func(state.assumptions, *beg);
    if (func) {
        process_text(state, ++func(state, beg, end), end);
    } else if (*beg == "data") {
        process_text(state, process_data(state, beg, end), end);
    } else if (*beg == ":") {
        process_text(state, state.process_label(beg, end), end);
    } else {
        throw make_error(state,
                         " first token \"" + *beg +
                         "\" is neither directive, label, or instruction.");
    }
}

int get_file_size(const char * filename) {
    struct stat stat_buf;
    int rc = stat(filename, &stat_buf);
    return rc == 0 ? int(stat_buf.st_size) : -1;
}

void TextProcessState::add_instruction
    (erfin::Inst inst, const std::string * label)
{
    m_inst_to_source_line.push_back(current_source_line);
    if (label) {
        // if you have a label, there must be space to insert the immd
        // this is marked by leaving the 16 lsb equal to 0.
        assert((inst & 0xFFFF) == 0);
        unfulfilled_labels.emplace_back(program_data.size(), *label);
    }
    program_data.push_back(inst);
}

void TextProcessState::swap_program
    (ProgramData & prog, std::vector<std::size_t> & inst_to_line)
{
    prog.swap(program_data);
    inst_to_line.swap(m_inst_to_source_line);
}

void TextProcessState::resolve_unfulfilled_labels() {
#   if 0//def MACRO_DEBUG
    if (unfulfilled_labels.size() > 1) {
        for (std::size_t i = 0; i != unfulfilled_labels.size() - 1; ++i) {
            const auto & cont = unfulfilled_labels;
            assert(cont[i].program_location < cont[i + 1].program_location);
            (void)cont;
        }
    }
#   endif
    for (UnfilledLabelPair & unfl_pair : unfulfilled_labels) {
        auto itr = labels.find(unfl_pair.label);
        if (itr == labels.end()) {
            throw Error("Label on line: ??, \"" + unfl_pair.label +
                        "\" not found anywhere in source code.");
        }
        const LabelPair & lbl_pair = itr->second;
        std::cout << "resolving label \"" << unfl_pair.label << "\" on "
                  << lbl_pair.source_line << " to integer value: "
                  << lbl_pair.program_location << std::endl;

        assert((program_data[unfl_pair.program_location] & 0xFFFF) == 0);
        program_data[unfl_pair.program_location] |=
            erfin::encode_immd(int(lbl_pair.program_location));
        int i = erfin::decode_immd_as_int(program_data[unfl_pair.program_location]);
        assert(i == int(lbl_pair.program_location));
        int j = 0;
        ++j;
    }
    unfulfilled_labels.clear();
}

// <--------------------------- level 3 helpers ------------------------------>

StringCIter process_binary
    (TextProcessState & state, StringCIter beg, StringCIter end);

erfin::Reg string_to_register(const std::string & str);

StringCIter process_data
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    if (beg == end) {
        throw make_error(state, ": stray data directive found at the end of "
                                "the source code.");
    }
    ++beg; // set over "data"
    auto process_func = process_binary;
    if (*beg != "[") {
        if (*beg == "binary") {
            // default
        } else {
            throw make_error(state, ": encoding scheme \"" + *beg + "\" not "
                                    "recognized.");
        }
        ++beg;
    }
    skip_new_lines(state, &beg, end);
    if (*beg != "[") {
        throw make_error(state, ": expected square bracket to indicate the "
                                "start of data.");
    }
    ++beg;
    beg = process_func(state, beg, end);
    skip_new_lines(state, &beg, end);
    return beg;
}

StringCIter TextProcessState::process_label(StringCIter beg, StringCIter end) {
    // forgive me I did not have any kind of access control enforcement
    // (encapsulation) on this function before, therefore the odd "this"
    assert(*beg == ":");
    using LabelPair = TextProcessState::LabelPair;
    ++beg;
    skip_new_lines(*this, &beg, end);
    if (beg == end) {
        throw make_error(*this, ": Code ends before a label was given for "
                                "the label directive.");
    }
    skip_new_lines(*this, &beg, end);
    if (string_to_register(*beg) != erfin::enum_types::REG_COUNT) {
        throw make_error(*this, ": register cannot be used as a label.");
    }
    auto itr = labels.find(*beg);
    if (itr == labels.end()) {
        labels[*beg] = LabelPair { program_data.size(), current_source_line };
    } else {
        throw make_error(*this, ": dupelicate label, previously defined "
                         "on line: " + std::to_string(itr->second.source_line));
    }
    return ++beg;
}

void skip_new_lines(TextProcessState & state, StringCIter * itr, StringCIter end) {
    if (*itr == end) return;
    while (**itr == "\n") {
        ++(*itr);
        ++state.current_source_line;
        if (*itr == end) return;
    }
}

Error make_error(TextProcessState & state, const std::string & str) {
    return Error("On line " + std::to_string(state.current_source_line) + str);
}

// <--------------------------- level 4 helpers ------------------------------>

// <----------------------- Arithmetic operations ---------------------------->

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

LineToInstFunc get_line_processing_func
    (SuffixAssumption assumptions, const std::string & fname)
{
    static bool is_initialized = false;
    using LineFuncMap = std::map<std::string, LineToInstFunc>;
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
    return get_line_processing_func(assumptions, fname);
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
                ++state.current_source_line;
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
        throw make_error(state, ": all data sequences must be divisible by 32 "
                         "bits, this data sequence is off by " +
                         std::to_string(32 - bit_pos) + " bits.");
    }
    for (UInt32 datum : state.data) {
        state.add_instruction(erfin::Inst(datum));
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
    XPF_INT,
    XPF_FP,
    XPF_LABEL,
    XPF_INVALID
};

class AssumptionResetRAII {
public:
    using SuffixAssumption = erfin::Assembler::SuffixAssumption;

    AssumptionResetRAII(TextProcessState & state, SuffixAssumption new_assumpt):
        m_state(&state),
        m_old_assumpt(state.assumptions)
    { state.assumptions = new_assumpt; }

    ~AssumptionResetRAII() { m_state->assumptions = m_old_assumpt; }

private:
    TextProcessState * m_state;
    const SuffixAssumption m_old_assumpt;
};

StringCIter make_generic_logic
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end);

StringCIter make_generic_arithemetic
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
        state.add_instruction(encode(ROTATE, string_to_register(*beg), u.i));

        break;
    case XPF_2R:
        state.add_instruction(encode(ROTATE, string_to_register(*beg),
                                     string_to_register(*(beg + 1))   ));
        break;
    default: throw make_error(state, INVALID_PARAMS_MSG);
    }
    ++state.current_source_line;
    return eol;
}

StringCIter make_syscall
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    using namespace erfin::enum_types;

    auto eol = get_eol(++beg, end);
    union { int i; double d; } u;

    switch (convert_to_number_strict(beg, u.d, u.i)) {
    case INTEGER: break;
    case NOT_NUMERIC:
        if (*beg == "upload")
            u.i = UPLOAD_SPRITE;
        else if (*beg == "unload")
            u.i = UNLOAD_SPRITE;
        else if (*beg == "clear")
            u.i = SCREEN_CLEAR;
        else if (*beg == "wait")
            u.i = WAIT_FOR_FRAME;
        else if (*beg == "read")
            u.i = READ_INPUT;
        else if (*beg == "draw")
            u.i = DRAW_SPRITE;
        else
            throw make_error(state, ": \"" + *beg + "\" is not a system call.");
        break;
    default:
        throw make_error(state, ": fixed points are not system call names.");
    }

    state.add_instruction(encode_op(SYSTEM_CALL) | encode_immd(u.i));
    ++state.current_source_line;
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
        bool b = string_to_number<10>(&*itr->begin(), &*itr->end(), i);
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
            throw make_error(state, ": labels are not support with skip "
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
        throw make_error(state, ": a fixed point is not an appropiate mask.");
    case XPF_1R:
        state.add_instruction(encode(SKIP, string_to_register(*beg)));
        break;
    default: throw make_error(state, ": unsupported parameters.");
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
    const std::string * label = nullptr;
    switch (get_lines_param_form(beg, line_end)) {
    case XPF_2R:
        inst = encode(SET, itr2reg(beg), itr2reg(beg + 1));
        break;
    case XPF_1R_INT:
        convert_to_number_strict(beg + 1, d, i);
        inst = encode(SET, itr2reg(beg), i);
        break;
    case XPF_1R_FP:
        convert_to_number_strict(beg + 1, d, i);
        inst = encode(SET, itr2reg(beg), d);
        break;
    case XPF_1R_LABEL:
        label = &*(beg + 1);
        inst = encode_op(SET) | encode_param_form(REG_IMMD) |
               encode_reg(itr2reg(beg));
        break;
    default:
        throw make_error(state, ": set instruction may only have exactly "
                                 "two arguments.");
    }
    state.add_instruction(inst, label);
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

StringCIter make_jump
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    auto eol = get_eol(++beg, end);
    static constexpr const auto PC = enum_types::REG_PC;
    Inst inst = encode_op(enum_types::SET);
    union { int i; double d; } u;
    (void)convert_to_number_strict(beg, u.d, u.i);
    const std::string * label = nullptr;
    switch (get_lines_param_form(beg, eol)) {
    case XPF_1R:
        inst |= encode_param_form(enum_types::REG_REG) |
                encode_reg_reg(PC, string_to_register(*beg));
        break;
    case XPF_INT:
        inst |= encode_param_form(enum_types::REG_IMMD) |
                encode_reg(PC) | encode_immd(u.i);
        break;
    case XPF_LABEL:
        // not quite sure... will need to insert the offset...? maybe not
        label = &*beg;
        inst |= encode_reg(PC);
        break;
    default:
        throw make_error(state, ": jump only excepts one argument, the "
                                "destination.");
    }
    state.add_instruction(inst, label);
    ++state.current_source_line;
    return eol;
}

StringCIter assume_directive
    (TextProcessState & state, StringCIter beg, StringCIter end)
{
    using namespace erfin;
    auto eol = get_eol(++beg, end);
    if ((eol - beg) == 1) {
        if (*beg == "fp" || *beg == "fixed-point") {
            state.assumptions = Assembler::NO_ASSUMPTION;
        } else if (*beg == "int" || *beg == "integer") {
            state.assumptions = Assembler::USING_INT;
        } else if (*beg == "none" || *beg == "nothing") {
            state.assumptions = Assembler::USING_FP;
        } else {
            make_error(state, ": \"" + *beg + "\" is not a valid assumption.");
        }
    } else {
        throw make_error(state, ": too many assumptions/arguments.");
    }
    ++state.current_source_line;
    return eol;
}

// <--------------------------- level 6 helpers ------------------------------>

erfin::ParamForm narrow_param_form(ExtendedParamForm xpf);

UInt32 deal_with_int_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state);

UInt32 deal_with_fp_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state);

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
        if (string_to_register(*beg) != REG_COUNT) return XPF_1R;
        switch (convert_to_number_strict(beg, u.d, u.i)) {
        case INTEGER: return XPF_INT;
        case DECIMAL: return XPF_FP;
        default     : return XPF_LABEL;
        }
    default: return XPF_INVALID;
    }
}

StringCIter make_generic_logic
    (erfin::OpCode op_code, TextProcessState & state,
     StringCIter beg, StringCIter end)
{
    return make_generic_arithemetic(op_code, state, beg, end);
}

UInt32 deal_with_int_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state)
{
    static constexpr const char * const int_unsupported_msg =
        ": instruction does not support integer immediates.";
    if (!op_code_supports_integer_immd(op_code))
        throw make_error(state, int_unsupported_msg);
    int i;
    string_to_number<10>(&*(eol - 1)->begin(), &*(eol - 1)->end(), i);
    return erfin::encode_immd(i);
}

UInt32 deal_with_fp_immd
    (StringCIter eol, erfin::OpCode op_code, TextProcessState & state)
{
    static constexpr const char * const fp_unsupported_msg =
        ": instruction does not support fixed point immediates.";
    if (!op_code_supports_fpoint_immd(op_code))
        throw make_error(state, fp_unsupported_msg);
    double d;
    string_to_number<10>(&*(eol - 1)->begin(), &*(eol - 1)->end(), d);
    return erfin::encode_immd(d);
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
            throw make_error(state, fp_int_ambig_msg);
        }
        MACRO_FALLTHROUGH;
    case XPF_2R_FP: case XPF_2R_INT: case XPF_2R_LABEL:
    case XPF_1R_FP: case XPF_1R_INT: case XPF_1R_LABEL:
        a1 = string_to_register(*beg);
        break;
    default:
        throw make_error(state, ": unsupported parameters.");
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
        throw Error("Impossible branch reached!");
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
    const std::string * label = nullptr;
    switch (pf) {
    case XPF_1R:
        if (op_code == SAVE)
            throw make_error(state, ": deference psuedo instruction only available for loading.");
        addr_reg = reg;
        break;
    case XPF_1R_LABEL: case XPF_2R_LABEL:
        label = &*(beg + arg_count - 1);
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
    state.add_instruction(inst, label);
    ++state.current_source_line;
    return eol;
}

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
    default:
        throw Error("Attempted to convert a parameter form that is only "
                    "available for psuedo-instructions.");
    }
    assert(false);
    throw Error("Impossible branch?!");
}

} // end of anonymous namespace

namespace {
    const LineToInstFunc init_f = get_line_processing_func
                                  (erfin::Assembler::NO_ASSUMPTION, "");
}  // end of anonymous namespace

/* static */ void erfin::Assembler::run_tests() {
    using namespace erfin;
    (void)init_f;
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
    (void)state.process_label(sample_label.begin(), sample_label.end());
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
    auto supposed_top = encode(OpCode::SET, Reg::REG_X, 12.34);
    assert(state.program_data.back() == supposed_top);
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
    const auto supposed_top = encode(OpCode::MINUS, Reg::REG_X, Reg::REG_X, 123);
    assert(supposed_top == state.program_data.back());
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
    const auto supposed_top = encode(OpCode::SAVE, Reg::REG_Y, Reg::REG_A, 4);
    assert(supposed_top == state.program_data.back());
    (void)supposed_top;
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
    assert(state.program_data[0] == encode(OpCode::SET , Reg::REG_PC, 2));
    assert(state.program_data[1] == encode(OpCode::LOAD, Reg::REG_X , 2));
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
    assert(pdata[0] == encode(OpCode::SET , Reg::REG_X, 1.0 ));
    assert(pdata[1] == encode(OpCode::SET , Reg::REG_Y, 1.44));
    assert(pdata[2] == encode(OpCode::PLUS, Reg::REG_X, Reg::REG_Y, Reg::REG_X));
    assert(pdata[3] == encode(OpCode::SET , Reg::REG_PC, 2));
    (void)pdata;
    }
}
