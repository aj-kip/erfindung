/****************************************************************************

    File: Assembler.hpp
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

#ifndef MACRO_HEADER_GUARD_ERFI_ASSEMBLER_HPP
#define MACRO_HEADER_GUARD_ERFI_ASSEMBLER_HPP

#include "ErfiDefs.hpp"
#include "ErfiError.hpp"

#include <vector>
#include <limits>
#include <string>
#include <set>

namespace erfin {

class Debugger;

class Assembler {
public:

    enum Assumption {
        // -int, -fp must be set explicitly
        NO_ASSUMPTIONS = 0,
        // only -int must be set explicitly
        USING_FP  = 1 << 1,
        // only -fp must be set explicitly
        USING_INT = 1 << 2,
        INVALID_FP_INT_ASSUMPTION = 0x3,
        NUMERIC_ASSUMPTION_BIT_MASK = 0x3,
        SAVE_AND_RESTORE_REGISTERS = 1 << 3
    };

    static constexpr const std::size_t INVALID_LINE_NUMBER = std::size_t(-1);

    Assembler() {}

    void assemble_from_file(const char * file);

    void assemble_from_string(const std::string & source);

    void print_warnings(std::ostream &) const;

    const ProgramData & program_data() const;

    void setup_debugger(Debugger & dbgr);

    /**
     *  @note also clears error information
     *  @param other
     */
    void swap_out_program_data(ProgramData & other);

    void swap(Assembler & asmr);

    std::size_t translate_to_line_number
        (std::size_t instruction_address) const noexcept;

    static void run_tests();

private:

    ProgramData m_program;

    // debugging erfi program info
    DebuggerInstToLineMap m_inst_to_line_map;

    std::vector<std::string> m_warnings;
};

} // end of erfin namespace

#endif
