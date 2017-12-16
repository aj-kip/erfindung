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

#include "Debugger.hpp"

#include <algorithm>
#include <iterator>
#include <iostream>
#include <fstream>
#include <map>

#include <sys/stat.h>
#include <cassert>

namespace {

using StringCIter = std::vector<std::string>::const_iterator;
using UInt32 = erfin::UInt32;
using Error = std::runtime_error;
using SuffixAssumption = erfin::Assembler::Assumption;
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
    // imposes a file size limit of ~2.1GB (no reasonable source file should
    //                                      ever be this large!)
    int fsize = get_file_size(file);
    if (fsize < 0) {
        throw Error("Could not read file contents (file too large/not "
                    "there?).");
    }
    std::fstream file_stream(file);
    assemble_from_stream(file_stream);
}

void Assembler::assemble_from_stream(std::istream & in) {
    std::string temp { std::istream_iterator<char>(in),
                       std::istream_iterator<char>()   };
    assemble_from_work_string(temp);
}

void Assembler::assemble_from_string(const std::string & source) {
    std::string temp = source;
    assemble_from_work_string(temp);
}

void Assembler::print_warnings(std::ostream & out) const {
    for (const auto & line : m_warnings) {
        out << line << std::endl;
    }
}

const ProgramData & Assembler::program_data() const
    { return m_program; }

void Assembler::setup_debugger(Debugger & dbgr) const {
    AssemblerDebuggerAttorney::copy_line_inst_map_to_debugger
        (m_inst_to_line_map, dbgr);
}

/* private */ void Assembler::assemble_from_work_string(std::string & source) {
    m_program.clear();
    convert_to_lower_case(source);
    std::vector<std::string> line_list = seperate_into_lines(source);
    for (std::string & line : line_list)
        remove_comments_from(line);
    // I need some means to sync line numbers with the source code
    std::vector<std::string> tokens = tokenize(line_list);

    TextProcessState tpstate;

    tpstate.process_tokens(tokens.begin(), tokens.end());
    tpstate.retrieve_warnings(m_warnings);
    // only when a valid program has been assembled do we swap in the actual
    // instructions, as a throw may occur at any point of the text processing
    tpstate.move_program(m_program, m_inst_to_line_map);
}

std::size_t Assembler::translate_to_line_number
    (std::size_t instruction_address) const noexcept
{
    if (instruction_address >= m_inst_to_line_map.size()) {
        return INVALID_LINE_NUMBER;
    }
    return m_inst_to_line_map[instruction_address];
}

/* static */ void Assembler::run_tests() {
    TextProcessState::run_tests();
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
                std::cerr << "tokenize: FATAL: Program logic error: Should not "
                             "see newline chars at this stage." << std::endl;
                std::terminate();
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
    static constexpr char TO_LOWER_DIFF = char('a' - 'A');
    for (char & c : str) {
        if (c >= 'A' && c <= 'Z')
            c = char(c + TO_LOWER_DIFF);
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
