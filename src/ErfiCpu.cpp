/****************************************************************************

    File: ErfiCpu.cpp
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

#include "ErfiCpu.hpp"
#include "Assembler.hpp"

namespace {

using Error = std::runtime_error;
Error make_illegal_inst_error(erfin::Inst inst);

} // end of <anonymous> namespace

namespace erfin {

ErfiCpu::ErfiCpu() {
    for (UInt32 & reg : m_registers)
        reg = 0;
}

void ErfiCpu::run_cycle(MemorySpace & memspace) {
    using namespace erfin;
    using namespace erfin::enum_types;
    Inst inst = memspace[m_registers[REG_PC]];
    auto greg0 = [this](Inst i) -> UInt32 & { return m_registers[decode_reg0(i)]; };
    auto greg1 = [this](Inst i) -> UInt32 & { return m_registers[decode_reg1(i)]; };
    auto greg2 = [this](Inst i) -> UInt32 & { return m_registers[decode_reg2(i)]; };
    auto greg3 = [this](Inst i) -> UInt32 & { return m_registers[decode_reg3(i)]; };
    auto giimd = [this](Inst i) { return int(i & 0x8000) << 16 | int(i & 0x7FFF); };

#   define BARTH(op) \
    switch (decode_param_form(inst)) { \
    case REG_REG_REG: \
        greg2(inst) = greg1(inst) op greg0(inst); \
        break; \
    case REG_IMMD: \
        greg2(inst) = greg2(inst) op UInt32(decode_immd_as_int(inst)); \
        break; \
    default: throw make_illegal_inst_error(inst); \
    }

    switch (decode_op_code(inst)) {
    case PLUS : BARTH(+); break;
    case MINUS: BARTH(-); break;
    case TIMES: BARTH(*); break;
    case DIVIDE_MOD: switch (decode_op_code(inst)) {
        case REG_REG_REG_REG: break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case TIMES_FP:
    case DIV_MOD_FP: break;
    case AND: BARTH(&); break;
    case XOR: BARTH(^); break;
    case OR : BARTH(|); break;
    case NOT: switch (decode_param_form(inst)) {
              case REG: greg0(inst) = ~greg0(inst); break;
              default: throw make_illegal_inst_error(inst);
              }
        break;
    case AND_MSB:
    case XOR_MSB:
    case OR_MSB :
    case COMP: switch (decode_op_code(inst)) {

        }
        break;
    case SKIP: switch (decode_param_form(inst)) {
        case REG: if (greg0(inst))
                ++m_registers[REG_PC];
            break;
        case REG_IMMD: if (greg0(inst) & giimd(inst))
                ++m_registers[REG_PC];
            break;
        default: throw make_illegal_inst_error(inst);
        }
        break;
    case LOAD:
    case SAVE:
    case SET_INT:
    case SET_ABS:
    case SET_FP96:
    case SET_REG: break;
    default: throw make_illegal_inst_error(inst);
    };
#   undef BARTH
    ++m_registers[REG_PC];
}

} // end of erfin namespace

namespace {

Error make_illegal_inst_error(erfin::Inst inst) {

}

} // end of erfin namespace
