/****************************************************************************

    File: Debugger.hpp
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

#ifndef MACRO_HEADER_GUARD_ERFINDUNG_DEBUGGER_HPP
#define MACRO_HEADER_GUARD_ERFINDUNG_DEBUGGER_HPP

#include "ErfiDefs.hpp"

#include <set>

namespace erfin {

class Assembler;
class AssemblerDebuggerAttorney;

/** The Debugger is a special, programmer's device that you can hook up to
 *  your console.
 *  Features:
 *  - Break points
 *  - Convert machine's state into a human friendly report
 */
class Debugger {
public:
    friend class AssemblerDebuggerAttorney;
    using InstToLineMap = DebuggerInstToLineMap;
    using BreakPointsContainer = std::set<std::size_t>;

    enum Interpretation {
        AS_FP,
        AS_INT
    };

    static constexpr const std::size_t NO_LINE = std::size_t(-1);

    Debugger(): m_at_break_point(false) {}

    bool at_break_point() const noexcept;

    bool is_outside_program() const noexcept;

    std::size_t add_break_point(std::size_t line_number);

    bool remove_break_point(std::size_t line_number);

    void update_internals(const RegisterPack &);

    const std::string & interpret_register(Reg, Interpretation);

    const BreakPointsContainer & break_points() const noexcept
        { return m_break_points; }

    std::string print_current_frame_to_string() const;

private:

    const std::string & interpret_register
        (Reg, Interpretation, const MemorySpace *);

    InstToLineMap m_inst_to_line_map;
    BreakPointsContainer m_break_points;
    RegisterPack m_regs;

    std::string m_reg_int_cache;
    bool m_at_break_point;
};

class AssemblerDebuggerAttorney {
    friend class Assembler;
    using InstToLineMap = DebuggerInstToLineMap;

    static void copy_line_inst_map_to_debugger
        (const InstToLineMap & map_, Debugger & debugger)
    { debugger.m_inst_to_line_map = map_; }
};

} // end of erfin namespace

#endif
