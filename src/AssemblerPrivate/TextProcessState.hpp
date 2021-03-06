/****************************************************************************

    File: TextProcessState.hpp
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

#ifndef MACRO_HEADER_GUARD_TEXT_PROCESS_STATE_HPP
#define MACRO_HEADER_GUARD_TEXT_PROCESS_STATE_HPP

#include "../Assembler.hpp"
#include "CommonDefinitions.hpp"

#include <vector>
#include <string>
#include <map>

#include <stdexcept>

namespace erfin {

class AssumptionRAII {
public:
    using Assumption = Assembler::Assumption;
    explicit AssumptionRAII(Assumption *);

    AssumptionRAII(const AssumptionRAII &) = delete;
    AssumptionRAII & operator = (const AssumptionRAII &) = delete;

    AssumptionRAII(AssumptionRAII &&);
    AssumptionRAII & operator = (AssumptionRAII &&);

    ~AssumptionRAII();
private:
    Assumption * m_ptr;
    Assumption m_old;
};

class TextProcessState {
public:
    using StringCIter = TokensConstIterator;
    using Assumption = Assembler::Assumption;

    friend class StrWrapTextStateAttorney;
    friend void run_get_line_processing_function_tests();

    TextProcessState();

    Assumption assumptions() const { return m_assumptions; }
    void include_assumption(Assumption);
    void exclude_assumption(Assumption);

    AssumptionRAII get_scoped_assumption_restorer();

    // having some encapsulation, helps me to change the state in predictable
    // ways rather than having to micro-manage everything >.>

    void add_instruction(erfin::Inst inst, const std::string * label = nullptr);

    // regular text processing does not need this
    void move_program
        (ProgramData & prog, std::vector<std::size_t> & inst_to_line);

    // regular text processing does not need this
    void resolve_unfulfilled_labels();

    StringCIter process_label(StringCIter beg, StringCIter end);

    void handle_newlines(StringCIter * itr, StringCIter end_of_text);

    void process_tokens(StringCIter beg, StringCIter end);

    void push_warning(const std::string &);

    // regular text processing does not need this
    void retrieve_warnings(std::vector<std::string> &);

    std::runtime_error make_error(const std::string & str) const noexcept;

    std::size_t current_source_line() const;

    bool last_instruction_was(OpCode) const;

    static void run_tests();

private:
    struct UnfilledLabelPair {
        UnfilledLabelPair(std::size_t i_, const std::string & s_):
            program_location(i_),
            label(s_)
        {}
        std::size_t program_location;
        std::string label;
    };

    struct LabelPair { std::size_t program_location, source_line; };

    Assumption m_assumptions;
    std::size_t m_current_source_line;
    ProgramData m_program_data;
    std::vector<std::size_t> m_inst_to_source_line;
    std::vector<UnfilledLabelPair> m_unfulfilled_labels;
    std::map<std::string, LabelPair> m_labels;
    std::vector<std::string> m_warnings;
};

} // end of erfin namespace

#endif
