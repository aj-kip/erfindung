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
#include "AssemblerPrivate/TextProcessState.hpp"
#include "FixedPointUtil.hpp"
#include "StringUtil.hpp"
#include "Debugger.hpp"

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
using TextProcessState = erfin::TextProcessState;

// high level textual processing
std::vector<std::string> seperate_into_lines(const std::string & str);

void remove_comments_from(std::string & str);

std::vector<std::string> tokenize(const std::vector<std::string> & lines);

void convert_to_lower_case(std::string & str);

int get_file_size(const char * filename);

} // end of <anonymous> namespace

namespace erfin {

void Assembler::assemble_from_file(const char * file) {
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

    tpstate.process_tokens(tokens.begin(), tokens.end());
    //process_text(tpstate, tokens.begin(), tokens.end());
    // only when a valid program has been assembled do we swap in the actual
    // instructions, as a throw may occur at any point of the text processing
    tpstate.move_program(m_program, m_inst_to_line_map);
}

const Assembler::ProgramData & Assembler::program_data() const
    { return m_program; }

void Assembler::setup_debugger(Debugger & dbgr) {
    AssemblerDebuggerAttorney::copy_line_inst_map_to_debugger
        (m_inst_to_line_map, dbgr);
}

std::size_t Assembler::translate_to_line_number
    (std::size_t instruction_address) const noexcept
{
    if (instruction_address >= m_inst_to_line_map.size()) {
        return INVALID_LINE_NUMBER;
    }
    return m_inst_to_line_map[instruction_address];
}

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
    case REG_SP: return "bp";
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

namespace with_int {

Inst encode(OpCode op, Reg r0, int i) {
    return ::erfin::encode(op, r0, encode_immd(i));
}

Inst encode(OpCode op, Reg r0, Reg r1, int i) {
    return ::erfin::encode(op, r0, r1, encode_immd(i));
}

} // end of with_int namespace

namespace with_fp {

Inst encode(OpCode op, Reg r0, double d) {
    return ::erfin::encode(op, r0, encode_immd(d));
}

Inst encode(OpCode op, Reg r0, Reg r1, double d) {
    return ::erfin::encode(op, r0, r1, encode_immd(d));
}

} // end of with_fp namespace

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
                case ':': case ' ': case '\t': case ']':
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
                // operators are fickle creatures
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

int get_file_size(const char * filename) {
    struct stat stat_buf;
    int rc = stat(filename, &stat_buf);
    return rc == 0 ? int(stat_buf.st_size) : -1;
}

} // end of anonymous namespace

// we'll have to bring this back later

/* static */ void erfin::Assembler::run_tests() {
    TextProcessState::run_tests();
}
