/****************************************************************************

    File: parse_program_options.cpp
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

#include "parse_program_options.hpp"

#include "Assembler.hpp"
#include "StringUtil.hpp"

#include <iostream>
#include <fstream>

namespace {

using Error       = std::runtime_error;
using OptionsPair = erfin::OptionsPair;

constexpr const auto default_run_selection =
#ifdef MACRO_BUILD_STL_ONLY
    cli_run;
#else
    normal_windowed_run;
#endif

constexpr const char * const ONLY_ONE_INPUT_MSG =
    "Only one input option permitted.";

bool str_eq(const char * a, const char * b);

unsigned str_len(const char * beg);

template <typename T>
typename std::enable_if<std::is_integral<T>::value, bool>::
type to_dec_number(const char * str, T & out) {
    return string_to_number<const char *, T>(str, str + str_len(str), out, 10);
}

void select_input(OptionsPair &, char ** beg, char ** end);

#ifndef MACRO_BUILD_STL_ONLY
void select_cli(OptionsPair &, char **, char **);
#endif

void select_help(OptionsPair &, char **, char **);

#ifndef MACRO_BUILD_STL_ONLY
void select_watched_window(OptionsPair &, char ** beg, char ** end);
#endif

void add_break_points(OptionsPair &, char ** beg, char ** end);

void select_tests(OptionsPair &, char**, char **);

void select_stream_input(OptionsPair &, char**, char **);

#ifndef MACRO_BUILD_STL_ONLY
void select_window_scale(OptionsPair &, char ** beg, char ** end);
#endif

} // end of <anonymous> namespace

namespace erfin {

ProgramOptions::ProgramOptions():
    run_tests(false),
    window_scale(3),
    watched_history_length(DEFAULT_FRAME_LIMIT),
    input_stream_ptr(nullptr)
{}

ProgramOptions::ProgramOptions(ProgramOptions && lhs):
    ProgramOptions()
    { swap(lhs); }

ProgramOptions::~ProgramOptions() {
    if (!input_stream_ptr || input_stream_ptr == &std::cin) return;
    delete input_stream_ptr;
}

void ProgramOptions::swap(ProgramOptions & lhs) {
    std::swap(run_tests             , lhs.run_tests             );
    std::swap(window_scale          , lhs.window_scale          );
    std::swap(watched_history_length, lhs.watched_history_length);
    std::swap(input_stream_ptr      , lhs.input_stream_ptr      );
}

OptionsPair::OptionsPair():
    mode(default_run_selection)
{}

OptionsPair parse_program_options(int argc, char ** argv) {
    OptionsPair opts;

    static const struct {
        char identity;
        const char * longform;
        void (*process)(OptionsPair &, char **, char **);
    } options_table[] = {
        { 'i', "input"        , select_input          },
        { 'h', "help"         , select_help           },
#       ifndef MACRO_BUILD_STL_ONLY
        { 'c', "command-line" , select_cli            },
#       endif
        { 't', "run-tests"    , select_tests          },
        { 's', "stream-input" , select_stream_input   },
#       ifndef MACRO_BUILD_STL_ONLY
        { 'w', "window-scale" , select_window_scale   },
        { 'p', "prefail-watch", select_watched_window },
#       endif
        { 'b', "break-points" , add_break_points      }
    };

    auto unrecogized_option = [](const char * opt) {
        std::cout << "Unrecognized option: \"" << opt << "\"." << std::endl;
    };

    int last = 1;
    void (*process_options)(OptionsPair &, char **, char **) = select_input;
    auto process_step = [&](int end_index, const char * opt) {
        if (process_options)
            process_options(opts, argv + last, argv + end_index);
        else
            unrecogized_option(opt);
    };

    for (int i = 1; i != argc; ++i) {
        if (argv[i][0] != '-') continue;
        // switch options
        process_step(i, argv[last - 1]);
        last = i + 1;
        if (argv[i][1] == '-') {
            process_options = nullptr;
            for (const auto & entry : options_table) {
                if (str_eq(argv[i] + 2, entry.longform)) {
                    process_options = entry.process;
                    break;
                }
            }
        } else {
            // shortform (or series of shortforms)
            decltype (process_options) process = nullptr;
            for (char * c = argv[i] + 1; *c; ++c) {
                for (const auto & entry : options_table) {
                    if (entry.identity != *c) continue;
                    if (process)
                        process(opts, nullptr, nullptr);
                    process = entry.process;
                }
            }
            process_options = process ? process : select_input;
        }
    }
    // if dereference happens here: default option is wrongly being
    // 'unrecognized'
    process_step(argc, last == 1 ? nullptr : argv[last - 1]);
    return opts;
}

} // end of erfin namespace

// ----------------------------------------------------------------------------

namespace {

bool str_eq(const char * a, const char * b) {
    while (*a == *b && *a && *b) {
        ++a; ++b;
    }
    return *a == *b;
}

unsigned str_len(const char * beg) {
    auto end = beg;
    while (*end) ++end;
    return unsigned(end - beg);
}

void select_input(OptionsPair & opts, char ** beg, char ** end) {
    if (beg == end) {
        opts.mode = print_help;
        return;
    }
    if (end - beg > 1) throw Error("Only one file can be loaded.");
    if (opts.input_stream_ptr) throw Error(ONLY_ONE_INPUT_MSG);
    opts.input_stream_ptr = new std::ifstream(*beg, std::ifstream::binary);
    opts.input_stream_ptr->unsetf(std::ios_base::skipws);
}

#ifndef MACRO_BUILD_STL_ONLY
void select_cli(OptionsPair & opts, char **, char **)
    { opts.mode = cli_run; }
#endif

void select_help(OptionsPair & opts, char **, char **)
    { opts.mode = print_help; }

#ifndef MACRO_BUILD_STL_ONLY
void select_watched_window(OptionsPair & opts, char ** beg, char ** end) {
    opts.mode = watched_windowed_run;
    if (end - beg == 0) return;
    if (end - beg > 1)
        throw Error("Watch option expects at most one argument");
    if (!to_dec_number(*beg, opts.watched_history_length)) {
        std::cout << "Warning: history length is not a valid decimal "
                     "number." << std::endl;
    }
}
#endif

void add_break_points(OptionsPair & opts, char ** beg, char ** end) {
    std::size_t out = erfin::Assembler::INVALID_LINE_NUMBER;
    if (beg == end) {
        std::cout << "Warning: no break points specified, ignoring." << std::endl;
    }
    for (auto itr = beg; itr != end; ++itr) {
        if (to_dec_number(*itr, out)) {
            opts.break_points.push_back(out);
        } else {
            std::cout << "Warning: break point is not a valid decimal number." << std::endl;
        }
    }
}

void select_tests(OptionsPair & opts, char**, char **)
    { opts.run_tests = true; }

void select_stream_input(OptionsPair & opts, char**, char **) {
    if (opts.input_stream_ptr) throw Error(ONLY_ONE_INPUT_MSG);
    opts.input_stream_ptr = &std::cin;
    std::cin.unsetf(std::ios_base::skipws);
}
#ifndef MACRO_BUILD_STL_ONLY
void select_window_scale(OptionsPair & opts, char ** beg, char ** end) {
    if (end - beg != 1)
        throw Error("Window scale expects exactly one argument.");
    const char * s_end = *beg;
    const char * s_beg = *beg;
    while (*s_end) ++s_end;
    static constexpr const char * const NUM_ERR_MSG =
        "Window scale expects only base 10 positive integers.";
    if (!string_to_number(s_beg, s_end, opts.window_scale, 10))
        throw Error(NUM_ERR_MSG);
    if (opts.window_scale <= 0) throw Error(NUM_ERR_MSG);
}
#endif
} // end of <anonymous> namespace
