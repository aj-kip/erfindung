/****************************************************************************

    File: ErfiCpu.hpp
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

#ifndef MACRO_HEADER_GUARD_ERFINDUNG_CPU_HPP
#define MACRO_HEADER_GUARD_ERFINDUNG_CPU_HPP

#include "ErfiDefs.hpp"

#include <iosfwd>

namespace erfin {

class ErfiCpu {
public:
    ErfiCpu();

    void run_cycle(MemorySpace & memspace);
    void print_registers(std::ostream & out) const;
    static void run_tests();

private:

    UInt32 & reg0(Inst inst);
    UInt32 & reg1(Inst inst);
    UInt32 & reg2(Inst inst);
    UInt32 & reg3(Inst inst);
    void do_rotate(Inst inst);
    void do_divmod(Inst inst);
    void do_basic_arth_inst(Inst inst, UInt32(*func)(UInt32, UInt32));

    RegisterPack m_registers;
};

}

#endif
