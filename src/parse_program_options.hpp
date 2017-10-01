/****************************************************************************

    File: parse_program_options.hpp
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

#ifndef MACRO_HEADER_GUARD_PARSE_PROGRAM_OPTIONS_HPP
#define MACRO_HEADER_GUARD_PARSE_PROGRAM_OPTIONS_HPP

#include <vector>
#include <iosfwd>

#include "ErfiDefs.hpp"

namespace erfin {
struct ProgramOptions;
}

// ----------- Program Driver Functions - implemented in main.cpp -------------

void normal_windowed_run (const erfin::ProgramOptions &, const erfin::ProgramData &);
void watched_windowed_run(const erfin::ProgramOptions &, const erfin::ProgramData &);
void cli_run             (const erfin::ProgramOptions &, const erfin::ProgramData &);
void print_help          (const erfin::ProgramOptions &, const erfin::ProgramData &);

// ----------- Options Parsing - implemented in respective source -------------

namespace erfin {

class Assembler;

struct ProgramOptions {

    static constexpr const std::size_t DEFAULT_FRAME_LIMIT = 3;

    ProgramOptions();
    ProgramOptions(const ProgramOptions &) = delete;
    ProgramOptions(ProgramOptions &&);
    ~ProgramOptions();

    ProgramOptions & operator = (const ProgramOptions &) = delete;

    void swap(ProgramOptions &);

    bool run_tests;
    int window_scale;
    int watched_history_length;
    std::vector<std::size_t> break_points;
    Assembler * assembler;
    std::istream * input_stream_ptr;
};

struct OptionsPair : ProgramOptions {
    OptionsPair();
    void (*mode)(const ProgramOptions &, const ProgramData &);
};

OptionsPair parse_program_options(int argc, char ** argv);

} // end of erfin namespace

#endif
