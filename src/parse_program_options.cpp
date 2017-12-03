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

#include <cassert>

namespace {

struct TempOptions;
using Error       = std::runtime_error;
using OptionsPair = erfin::OptionsPair;
using ProcessOptionFunc = void (*)(TempOptions &, char **, char **);

constexpr const bool should_window_default =
#ifdef MACRO_BUILD_STL_ONLY
    false;
#else
    true;
#endif

struct TempOptions final : erfin::ProgramOptions {
    TempOptions():
        should_watch(false), should_window(should_window_default),
        should_help(false), should_test(false)
    {}
    void swap(OptionsPair &);
    bool should_watch;
    bool should_window;
    bool should_help;
    bool should_test;
};

constexpr const char * const ONLY_ONE_INPUT_MSG =
    "Only one input option permitted.";

bool str_eq(const char * a, const char * b);

unsigned str_len(const char * beg);

template <typename T>
typename std::enable_if<std::is_integral<T>::value, bool>::
type to_dec_number(const char * str, T & out) {
    return string_to_number<const char *, T>(str, str + str_len(str), out, 10);
}

void select_input(TempOptions &, char ** beg, char ** end);

void select_cli(TempOptions &, char **, char **);

void select_help(TempOptions &, char **, char **);

void select_watched(TempOptions &, char ** beg, char ** end);

void add_break_points(TempOptions &, char ** beg, char ** end);

void select_tests(TempOptions &, char**, char **);

void select_stream_input(TempOptions &, char**, char **);

void select_window_scale(TempOptions &, char ** beg, char ** end);

OptionsPair to_options_pair(TempOptions & topts);

static const struct {
    char identity;
    const char * longform;
    ProcessOptionFunc process;
} options_table_c[] = {
    { 'b', "break-points" , add_break_points    },
    { 'c', "command-line" , select_cli          },
    { 'h', "help"         , select_help         },
    { 'i', "input"        , select_input        },
    { 'r', "stream-input" , select_stream_input },
    { 's', "window-scale" , select_window_scale },
    { 't', "run-tests"    , select_tests        },
    { 'w', "watch"        , select_watched      }

};

} // end of <anonymous> namespace

namespace erfin {

ProgramOptions::ProgramOptions():
    window_scale(3),
    watched_history_length(DEFAULT_FRAME_LIMIT),
    input_stream_ptr(nullptr)
{}

ProgramOptions::ProgramOptions(ProgramOptions && lhs):
    ProgramOptions()
    { swap(lhs); }

ProgramOptions::~ProgramOptions() {
    if (!input_stream_ptr or input_stream_ptr == &std::cin) return;
    delete input_stream_ptr;
}

void ProgramOptions::swap(ProgramOptions & lhs) {
    std::swap(window_scale          , lhs.window_scale          );
    std::swap(watched_history_length, lhs.watched_history_length);
    std::swap(input_stream_ptr      , lhs.input_stream_ptr      );
}

OptionsPair::OptionsPair():
    mode(nullptr)
{}

OptionsPair parse_program_options(int argc, char ** argv) {
    assert(argc >= 1);
    TempOptions opts;
    int last = 1;
    ProcessOptionFunc process_options = select_input;

    auto process_step = [&](int end_index, const char * opt) {
        if (process_options)
            process_options(opts, argv + last, argv + end_index);
        else
            std::cout << "Unrecognized option: \"" << opt << "\"." << std::endl;
    };

    if (argc == 1) {
        opts.should_help = true;
        return to_options_pair(opts);
    }

    for (int i = 1; i != argc; ++i) {
        if (argv[i][0] != '-') continue;
        // edge case, implicit file select ignored
        if (i != 1) {
            process_step(i, argv[last - 1]);
        }
        // switch options
        last = i + 1;
        if (argv[i][1] == '-') {
            process_options = nullptr;
            for (const auto & entry : options_table_c) {
                if (str_eq(argv[i] + 2, entry.longform)) {
                    process_options = entry.process;
                    break;
                }
            }
        } else {
            // shortform (or series of shortforms)
            decltype (process_options) process = nullptr;
            for (char * c = argv[i] + 1; *c; ++c) {
                for (const auto & entry : options_table_c) {
                    if (entry.identity != *c) continue;
                    if (process)
                        process(opts, nullptr, nullptr);
                    process = entry.process;
                    break;
                }
            }
            process_options = process ? process : select_input;
        }
    }
    // if dereference happens here: default option is wrongly being
    // 'unrecognized'
    process_step(argc, last == 1 ? nullptr : argv[last - 1]);

    return to_options_pair(opts);
}

} // end of erfin namespace

// ----------------------------------------------------------------------------

namespace {

void TempOptions::swap(OptionsPair & lhs) {
    ProgramOptions::swap(lhs);
    if (should_help) {
        lhs.mode = print_help;
    } else if (should_test) {
        lhs.mode = run_tests;
    } else if (should_window) {
#       ifndef MACRO_BUILD_STL_ONLY
        if (should_watch) {
            lhs.mode = watched_windowed_run;
        } else {
            lhs.mode = normal_windowed_run;
        }
#       else
            throw Error("Windowed mode is not available in this build.");
#       endif
    } else {
        assert(!should_window);
        if (should_watch) {
            lhs.mode = watched_cli_run;
        } else {
            lhs.mode = cli_run;
        }
    }
}

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

void select_input(TempOptions &opts, char ** beg, char ** end) {
    if (beg == end) {
        throw Error("Select input option requires an argument (source file).");
    }
    if (end - beg > 1) throw Error("Only one file can be loaded.");
    if (opts.input_stream_ptr) throw Error(ONLY_ONE_INPUT_MSG);
    opts.input_stream_ptr = new std::ifstream(*beg, std::ifstream::binary);
    opts.input_stream_ptr->unsetf(std::ios_base::skipws);
}

void select_cli(TempOptions & opts, char **, char **)
    { opts.should_window = false; }

void select_help(TempOptions & opts, char **, char **)
    { opts.should_help = true; }

void select_watched(TempOptions & opts, char ** beg, char ** end) {
    if (end - beg == 0) return;
    if (end - beg > 1)
        throw Error("Watch option expects at most one argument");
    if (!to_dec_number(*beg, opts.watched_history_length)) {
        opts.should_watch = true;
        std::cout << "Warning: history length is not a valid decimal "
                     "number." << std::endl;
    }
}

void add_break_points(TempOptions & opts, char ** beg, char ** end) {
    std::size_t out = erfin::Assembler::INVALID_LINE_NUMBER;
    if (beg == end) {
        std::cout << "Warning: no break points specified, ignoring." << std::endl;
    }
    for (auto itr = beg; itr != end; ++itr) {
        if (to_dec_number(*itr, out)) {
            opts.break_points.push_back(out);
            opts.should_watch = true;
        } else {
            std::cout << "Warning: break point is not a valid decimal number." << std::endl;
        }
    }
}

void select_tests(TempOptions & opts, char**, char **)
    { opts.should_test = true; }

void select_stream_input(TempOptions & opts, char**, char **) {
    if (opts.input_stream_ptr) throw Error(ONLY_ONE_INPUT_MSG);
    opts.input_stream_ptr = &std::cin;
    std::cin.unsetf(std::ios_base::skipws);
}

void select_window_scale(TempOptions & opts, char ** beg, char ** end) {
    if (end - beg != 1)
        throw Error("Window scale expects exactly one argument.");
    const char * s_end = *beg;
    const char * s_beg = *beg;
    while (*s_end) ++s_end;
    static constexpr const char * const NUM_ERR_MSG =
        "Window scale expects only base 10 positive integers.";
    if (!string_to_number(s_beg, s_end, opts.window_scale, 10))
        throw Error(NUM_ERR_MSG);
    if (opts.window_scale <= 0)
        throw Error(NUM_ERR_MSG);
    opts.should_window = true;
}

OptionsPair to_options_pair(TempOptions & topts) {
    OptionsPair rv;
    topts.swap(rv);
    assert(rv.mode);
    return rv;
}

} // end of <anonymous> namespace
