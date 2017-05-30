/****************************************************************************

    File: TextProcessState.cpp
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

#include "TextProcessState.hpp"
#include "GetLineProcessingFunction.hpp"
#include "LineParsingHelpers.hpp"
#include "../FixedPointUtil.hpp"

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

using Error = std::runtime_error;
using StringCIter = erfin::TextProcessState::StringCIter;
using TextProcessState = erfin::TextProcessState;
using UInt32 = erfin::UInt32;

void process_text(TextProcessState & state, StringCIter beg, StringCIter end);

} // end of <anonymous> namespace

namespace erfin {

/* explicit */ AssumptionRAII::AssumptionRAII(Assumption * ptr):
    m_ptr(ptr),
    m_old(*ptr)
{}

AssumptionRAII::AssumptionRAII(AssumptionRAII && rhs):
    m_ptr(rhs.m_ptr),
    m_old(rhs.m_old)
{
    rhs.m_ptr = nullptr;
}

AssumptionRAII & AssumptionRAII::operator = (AssumptionRAII && rhs) {
    if (this != &rhs) {
        m_ptr = rhs.m_ptr;
        m_old = rhs.m_old;
        rhs.m_ptr = nullptr;
    }
    return *this;
}

AssumptionRAII::~AssumptionRAII() {
    if (!m_ptr) return;
    *m_ptr = m_old;
}

void TextProcessState::include_assumption(Assumption assume) {
    using Asr = Assembler;
    switch (assume) {
    case Asr::NO_ASSUMPTIONS: m_assumptions = assume; return;
    case Asr::USING_FP: case Asr::USING_INT:
        m_assumptions = static_cast<Assembler::Assumption>
                        ((m_assumptions & ~0x3) | assume);
        return;
    case Asr::SAVE_AND_RESTORE_REGISTERS:
        m_assumptions = static_cast<Assembler::Assumption>
                        (m_assumptions | assume);
        return;
    default:
        throw Error("Invalid assumption to include.");
    }
}

void TextProcessState::exclude_assumption(Assumption assume) {
    using Asr = Assembler;
    switch (assume) {
    case Asr::NO_ASSUMPTIONS: return;
    case Asr::USING_FP: case Asr::USING_INT:
        m_assumptions = static_cast<Assembler::Assumption>
                        (m_assumptions & ~0x3);
        return;
    case Asr::SAVE_AND_RESTORE_REGISTERS:
        m_assumptions = static_cast<Assembler::Assumption>
                        (m_assumptions & ~0x4);
        return;
    default:
        throw Error("Invalid assumption to exclude.");
    }
}

AssumptionRAII TextProcessState::get_scoped_assumption_restorer() {
    return AssumptionRAII(&m_assumptions);
}

void TextProcessState::add_instruction
    (erfin::Inst inst, const std::string * label)
{
    m_inst_to_source_line.push_back(m_current_source_line);
    if (label) {
        // if you have a label, there must be space to insert the immd
        // this is marked by leaving the 16 lsb equal to 0.
        assert((serialize(inst) & 0xFFFF) == 0);
        m_unfulfilled_labels.emplace_back(m_program_data.size(), *label);
    }
    m_program_data.push_back(inst);
}

void TextProcessState::move_program
    (ProgramData & prog, std::vector<std::size_t> & inst_to_line)
{
    resolve_unfulfilled_labels();
    prog.swap(m_program_data);
    inst_to_line.swap(m_inst_to_source_line);

    m_current_source_line = 1;
    m_program_data.clear();
    m_inst_to_source_line.clear();
    m_unfulfilled_labels.clear();
    m_labels.clear();
}

void TextProcessState::resolve_unfulfilled_labels() {
    for (UnfilledLabelPair & unfl_pair : m_unfulfilled_labels) {
        auto itr = m_labels.find(unfl_pair.label);
        if (itr == m_labels.end()) {
            auto line_num = m_inst_to_source_line[unfl_pair.program_location];
            throw Error("Label on line: " + std::to_string(line_num) +
                        ", \"" + unfl_pair.label +
                        "\" not found anywhere in source code.");
        }
        const LabelPair & lbl_pair = itr->second;

        assert((serialize(m_program_data[unfl_pair.program_location]) & 0xFFFF) == 0);
        if (lbl_pair.program_location > std::numeric_limits<int16_t>::max()) {
            throw Error("Label resolves to a location that is too large for "
                        "this assembler to handle.");
        }
        m_program_data[unfl_pair.program_location] |=
            erfin::encode_immd_int(int(lbl_pair.program_location));
        int i = erfin::decode_immd_as_int(m_program_data[unfl_pair.program_location]);
        (void)i;
        assert(i == int(lbl_pair.program_location));
    }
    m_unfulfilled_labels.clear();
}

StringCIter TextProcessState::process_label(StringCIter beg, StringCIter end) {
    // forgive me I did not have any kind of access control enforcement
    // (encapsulationerefore th) on this function before, the odd "this"
    assert(*beg == ":");
    using LabelPair = TextProcessState::LabelPair;
    ++beg;
    handle_newlines(&beg, end);
    if (beg == end) {
        throw make_error(": Code ends before a label was given for the label "
                         "directive.");
    }
    handle_newlines(&beg, end);
    if (string_to_register(*beg) != erfin::Reg::COUNT) {
        throw make_error(": register cannot be used as a label.");
    }
    auto itr = m_labels.find(*beg);
    if (itr == m_labels.end()) {
        m_labels[*beg] = LabelPair { m_program_data.size(), m_current_source_line };
    } else {
        throw make_error(": dupelicate label, previously defined on line: " +
                         std::to_string(itr->second.source_line));
    }
    return ++beg;
}

void TextProcessState::handle_newlines(StringCIter * itr, StringCIter end_of_text) {
    if (*itr == end_of_text) return;
    while (**itr == "\n") {
        ++(*itr);
        ++m_current_source_line;
        if (*itr == end_of_text) return;
    }
}

void TextProcessState::process_tokens(StringCIter beg, StringCIter end) {
    process_text(*this, beg, end);
}

void TextProcessState::push_warning(const std::string & warning_string) {
    std::string warn_str = "Warning on line " +
        std::to_string(m_current_source_line) +
        warning_string                        ;
    m_warnings.emplace_back(std::move(warn_str));
}

void TextProcessState::retrieve_warnings(std::vector<std::string> & target) {
    m_warnings.swap(target);
}

std::runtime_error TextProcessState::make_error(const std::string & str) const noexcept {
    return std::runtime_error("On line " + std::to_string(m_current_source_line) + str);
}

std::size_t TextProcessState::current_source_line() const
    { return m_current_source_line; }

bool TextProcessState::last_instruction_was(OpCode op) const {
    if (m_program_data.empty()) return false;
    return decode_op_code(m_program_data.back()) == op;
}

} // end of erfin namespace

namespace {

StringCIter process_data
    (TextProcessState & state, StringCIter beg, StringCIter end,
     std::vector<UInt32> * cached_cont = nullptr);

void process_text(TextProcessState & state, StringCIter beg, StringCIter end) {
    std::vector<UInt32> data_cache;

    if (beg == end) return;

    while (true) {
        state.handle_newlines(&beg, end);
        if (beg == end) return;
        // each function should expect to start after a newline
        // and a newline character is what is expected to be returned, expect
        // for the special functions which are not gotten from
        // get_line_processing_func
        auto func = erfin::get_line_processing_function(state.assumptions(), *beg);
        if (func) {
            auto new_beg = func(state, beg, end);
            for (auto itr = beg; itr != new_beg; ++itr)
                assert(*itr != "\n");
            beg = new_beg;
        } else if (*beg == "data") {
            beg = process_data(state, beg, end, &data_cache);
        } else if (*beg == ":") {
            beg = state.process_label(beg, end);
        } else {
            throw state.make_error(
                             " first token \"" + *beg +
                             "\" is neither directive, label, or instruction.");
        }
    }
}

// <-------------------------------------------------------------------------->

StringCIter process_binary
    (TextProcessState & state, std::vector<UInt32> & data,
     StringCIter beg, StringCIter end);

StringCIter process_numbers
    (TextProcessState & state, std::vector<UInt32> & data,
     StringCIter beg, StringCIter end);

StringCIter process_data
    (TextProcessState & state, StringCIter beg, StringCIter end,
     std::vector<UInt32> * cached_cont)
{
    std::vector<UInt32> non_cached_cont;
    std::vector<UInt32> * data = cached_cont ? cached_cont : &non_cached_cont;

    if (beg == end) {
        throw state.make_error(": stray data directive found at the end of "
                               "the source code.");
    }
    ++beg; // set over "data"
    auto process_func = process_binary;
    if (*beg != "[") {
        if (*beg == "binary") {
            // default
        } else if (*beg == "numbers") {
            process_func = process_numbers;
        } else {
            throw state.make_error(": encoding scheme \"" + *beg + "\" not "
                                   "recognized.");
        }
        ++beg;
    }
    state.handle_newlines(&beg, end);
    if (*beg != "[") {
        throw state.make_error(": expected square bracket to indicate the "
                               "start of data.");
    }
    ++beg;
    assert(data);
    data->clear();
    beg = process_func(state, *data, beg, end);
    state.handle_newlines(&beg, end);
    return beg;
}

// <-------------------------------------------------------------------------->

StringCIter process_binary
    (TextProcessState & state, std::vector<UInt32> & data,
     StringCIter beg, StringCIter end)
{
    static constexpr const char * const BAD_CHAR_MSG =
        ": binary encodings only handle five characters '1','x' for one and "
        "'_', 'o', '0' for zero.";
    static constexpr const char * const SOURCE_ENDED_TOO_SOON_MSG =
        ": source code ended without ending the current data sequence, is must "
        "be closed with a \"]\"";

    int bit_pos = 0;
    assert(data.empty());
    while (*beg != "]") {
        for (char c : *beg) {
            switch (c) {
            case '_': case 'o': case '0': case '.': case '1': case 'x':
                if (bit_pos == 0)
                    data.push_back(0);
                break;
            default: break;
            }
            switch (c) {
            case '1': case 'x':
                data.back() |= (1 << (31 - bit_pos));
                MACRO_FALLTHROUGH;
            case '_': case 'o': case '0': case '.':
                bit_pos = (bit_pos + 1) % 32;
                break;
            // characters we should never see
            case '\n':
                // only valid newline token has one character
                if (beg->size() == 1) {
                    state.handle_newlines(&beg, end);
                    // beg will start at the newline, however this will mean
                    // that we'll skip the first token!
                    // to fix this error AND make sure that multiple newlines
                    // are passed like normal, I use a crude "off-by-one" fix
                    --beg;
                    break;
                }
                MACRO_FALLTHROUGH;
            case '[': case ']': assert(false); break;
            default: throw state.make_error(BAD_CHAR_MSG);
            }
        }
        ++beg;
        if (beg == end) {
            throw state.make_error(SOURCE_ENDED_TOO_SOON_MSG);
        }
    }
    if (bit_pos != 0) {
        throw state.make_error(": all data sequences must be divisible by 32 "
                               "bits, this data sequence is off by " +
                               std::to_string(32 - bit_pos) + " bits.");
    }
    for (UInt32 datum : data) {
        state.add_instruction(erfin::deserialize(datum));
    }

    return ++beg;
}


StringCIter process_numbers
    (TextProcessState & state, std::vector<UInt32> & data,
     StringCIter beg, StringCIter end)
{
    assert(data.empty());
    using namespace erfin;
    for (; *beg != "]"; ++beg) {
        if (*beg == "\n") {
            state.handle_newlines(&beg, end);
            --beg;
            continue;
        }
        NumericParseInfo npi;
        npi = parse_number(*beg);
        switch (npi.type) {
        case INTEGER: data.push_back(npi.integer); break;
        case DECIMAL: data.push_back(to_fixed_point(npi.floating_point)); break;
        case NOT_NUMERIC:
            throw state.make_error(": all entries in the data sequence must be"
                                   "  numeric");
            break;
        default: assert(false); break;
        }
    }
    for (UInt32 datum : data) {
        state.add_instruction(erfin::deserialize(datum));
    }
    return ++beg;
}

} // end of <anonymous> namespace

namespace erfin {

// tests include helpers, therefore this is at the bottom of the source code

/* static */ void TextProcessState::run_tests() {
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
    std::vector<UInt32> data;
    (void)process_binary(state, data, sample_binary.begin(), sample_binary.end());
    assert(serialize(state.m_program_data.back()) == 252414410);
    }
    {
    const std::vector<std::string> sample_data = {
        "data", "binary", "[", "\n",
            "____xxxxxx__x_x_", "\n", // 4042
            "___x_xxx____x__x", "\n", // 5897
        "]"
    };
    (void)process_data(state, sample_data.begin(), sample_data.end());
    assert(serialize(state.m_program_data.back()) == 264902409);
    }
    {
    const std::vector<std::string> sample_label = {
        ":", "hello", "and", "x", "y", "\n"
                    , "jump", "hello"
    };
    (void)state.process_label(sample_label.begin(), sample_label.end());
    assert(state.m_labels.find("hello") != state.m_labels.end());
    }
    run_get_line_processing_function_tests();
}

} // end of erfin namespace
