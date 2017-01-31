/****************************************************************************

    File: Debugger.cpp
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

#include "Debugger.hpp"
#include "Assembler.hpp"
#include "FixedPointUtil.hpp"

namespace {

using LineNumIter = erfin::DebuggerInstToLineMap::const_iterator;
using Error = std::runtime_error;

LineNumIter find_closest_value
    (std::size_t v, const erfin::DebuggerInstToLineMap & cont) noexcept;

} // end of <anonymous> namespace

namespace erfin {

bool Debugger::at_break_point() const noexcept
    { return m_at_break_point; }

bool Debugger::is_outside_program() const noexcept {
    using namespace erfin::enum_types;
    return m_regs[std::size_t(Reg::REG_PC)] > m_inst_to_line_map.size();
}

std::size_t Debugger::add_break_point(std::size_t line_number) {
    auto closest_itr = find_closest_value(line_number, m_inst_to_line_map);
    if (closest_itr == m_inst_to_line_map.end())
        return NO_LINE;
    auto resp = m_break_points.insert(*closest_itr);
    if (!resp.second) {
        return m_break_points.find(*closest_itr) != m_break_points.end();
    }
    return *closest_itr;
}

bool Debugger::remove_break_point(std::size_t line_number) {
    auto itr = m_break_points.find(line_number);
    if (itr == m_break_points.end())
        return false;
    m_break_points.erase(itr);
    return true;
}

void Debugger::update_internals(const RegisterPack & cpu_regs) {
    using namespace enum_types;
    if (is_outside_program())
        return;
    constexpr auto pc_idx = std::size_t(Reg::REG_PC);
    auto itr = m_break_points.find(m_inst_to_line_map[m_regs[pc_idx]]);
    m_at_break_point = (itr != m_break_points.end());
    m_regs = cpu_regs;
}

const std::string & Debugger::interpret_register(Reg r, Interpretation intr)
    { return interpret_register(r, intr, nullptr); }

const std::string & Debugger::interpret_register
    (Reg r, Interpretation intr, const MemorySpace & memory)
{ return interpret_register(r, intr, &memory); }

/* private */ const std::string & Debugger::interpret_register
    (Reg r, Interpretation intr, const MemorySpace * memory)
{
    m_reg_int_cache = register_to_string(r);
    m_reg_int_cache += ": ";
    const UInt32 * source = nullptr;
    auto reg_idx = std::size_t(r);
    static_assert(std::is_same<const UInt32 &, decltype((*memory)[0])>::value, "");
    if (memory && m_regs[reg_idx] <= memory->size()) {
        source = &(*memory)[0] + m_regs[reg_idx];
    } else {
        source = &m_regs[reg_idx];
    }
    if (intr == AS_FP) {
        m_reg_int_cache += std::to_string(fixed_point_to_double(*source));
    } else if (intr == AS_INT) {
        m_reg_int_cache += std::to_string(int(*source));
    } else {
        throw Error("Bad value provided for interpretation.");
    }
    return m_reg_int_cache;
}

} // end of erfin namespace

namespace {

LineNumIter find_closest_value
    (std::size_t v, LineNumIter beg, LineNumIter end) noexcept;

LineNumIter find_closest_value
    (std::size_t v, const erfin::DebuggerInstToLineMap & cont) noexcept
{ return find_closest_value(v, cont.begin(), cont.end()); }

LineNumIter find_closest_value
    (std::size_t v, LineNumIter beg, LineNumIter end) noexcept
{
    using std::size_t;

    const auto diff = [](size_t x, size_t y)
                        { return (x > y) ? x - y : y - x; };
    auto mid = beg + ((end - beg) / 2);
    switch (end - beg) {
    case 2 : return (diff(v, *beg) < diff(v, *(beg + 1))) ? beg : (beg + 1);
    case 1 : return beg;
    case 0 : return end;
    default: if (v < *mid)
                 return find_closest_value(v, beg, mid);
             else if (v > *mid)
                 return find_closest_value(v, mid, end);
             else
                 return mid;
    }
}

} // end of <anonymous> namespace
