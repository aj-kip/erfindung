/****************************************************************************

    File: main.cpp
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

#include <iostream>
#include <cstdint>
#include <array>
#include <map>
#include <cmath>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <queue>
#include <algorithm>

#include "ErfiDefs.hpp"
#include "FixedPointUtil.hpp"

#include <cstring>
#include "Assembler.hpp"
#include "ErfiCpu.hpp"

#include "ErfiGpu.hpp"

namespace {
using Error = std::runtime_error;
}

namespace erfin {

inline OpCode get_op_code(Inst inst) {
    inst >>= 22;
    return static_cast<OpCode>(inst & 0x3F);
}

// for ParamForm: REG_REG_IMMD

// for ParamForm: REG_IMMD

// for op: PLUS
inline Inst make_plus(Reg r0, Reg r1, Reg r2) {
    return encode_op(enum_types::PLUS) | encode_reg_reg_reg(r0, r1, r2);
}
// for op: MINUS
inline Inst make_minus(Reg r0, Reg r1, Reg r2) {
    return encode_op(enum_types::MINUS) | encode_reg_reg_reg(r0, r1, r2);
}
// for op: TIMES
inline Inst make_times(Reg r0, Reg r1, Reg r2) {
    return encode_op(enum_types::TIMES) | encode_reg_reg_reg(r0, r1, r2);
}
// for op: DIVIDE_MOD
// for op: PLUS_IMMD,
// for op: TIMES_IMMD,
// for op: MINUS_IMMD, // if immediates can be negative
// for op: DIV_REG_IMMD,
// for op: DIV_IMMD_REG,
// for op: AND
inline Inst make_and(Reg r0, Reg r1, Reg r2) {
    return encode_op(enum_types::AND) | encode_reg_reg_reg(r0, r1, r2);
}
// for op: NOT
inline Inst make_not(Reg r0, Reg r1) {
    return encode_op(enum_types::NOT) | encode_reg_reg(r0, r1);
}
// for op: XOR
inline Inst make_xor(Reg r0, Reg r1, Reg r2) {
    return encode_op(enum_types::XOR) | encode_reg_reg_reg(r0, r1, r2);
}
// for op: OR
inline Inst make_or(Reg r0, Reg r1, Reg r2) {
    return encode_op(enum_types::TIMES) | encode_reg_reg_reg(r0, r1, r2);
}
// for op: AND_IMMD, // two args, one answer
// for op: XOR_IMMD, // two args, one answer
// for op: OR_IMMD , // two args, one answer
// for op: COMP    , // two args, one answer
inline Inst make_comp(Reg r0, Reg r1, Reg r2) {
    return encode_op(enum_types::COMP) | encode_reg_reg_reg(r0, r1, r2);
}

// for op: JUMP_REG,
// for op: LOAD, // two regs, one immd
// for op: SAVE, // two regs, one immd
// for op: SET_INT , // one reg, one IMMD
inline Inst make_set_int(Reg r0, UInt32 immd) {
    return encode_op(enum_types::SET_INT) | encode_reg(r0) | immd;
}
// for op: SET_FP96, // one reg, one IMMD

// for op: SET_REG , // two regs
inline Inst make_set_reg(Reg r0, Reg r1) {
    return encode_op(enum_types::SET_REG) | encode_reg_reg(r0, r1);
}
// for op: SYSTEM_READ // one reg, one immd


ParamForm get_op_param(OpCode op);

class Cpu {
public:
    void do_cycle(MemorySpace & mem);
private:
    RegisterPack m_regs;
};

void do_cycle(RegisterPack & regs, MemorySpace & mem, Gpu & gpu);

} // end of erfin

class CoutFormatSaver {
public:
    CoutFormatSaver():
        old_precision(std::cout.precision()), old_flags(std::cout.flags())
    {}

    ~CoutFormatSaver() {
        std::cout.precision(old_precision);
        std::cout.flags(old_flags);
    }

private:
    int old_precision;
    std::ios::fmtflags old_flags;
};


void test_fixed_point(double value) {
    using namespace erfin;
    UInt32 fp = to_fixed_point(value);
    double val_out = fixed_point_to_double(fp);
    double diff = val_out - value;
    // use an error about 1/(2^16), but "fatten" it up for error
    if (mag(diff) < 0.00002) {
        CoutFormatSaver cfs;
        (void)cfs;
        std::cout <<
            "For: " << value << "\n"
            "Fixed Point value: " << std::hex << std::uppercase << fp << "\n"
            "End value        : " << std::dec << std::nouppercase << val_out << "\n" <<
            std::endl;
        return;
    }
    // not equal!
    std::stringstream ss;
    ss <<
        "Fixed point test failed!\n"
        "Starting         : " << value << "\n"
        "Fixed Point value: " << std::hex << std::uppercase << fp << "\n"
        "End value        : " << std::dec << std::nouppercase << val_out << "\n";
    std::cout << ss.str() << std::endl;
    //throw std::runtime_error(ss.str());
}

void test_fp_multiply(double a, double b) {
    using namespace erfin;
    UInt32 fp_a = to_fixed_point(a);
    UInt32 fp_b = to_fixed_point(b);
    UInt32 res = fp_multiply(fp_a, fp_b);
    double d = fixed_point_to_double(res);
    CoutFormatSaver cfs;
    (void)cfs;
    const double MAX_ERROR = 0.00002;
    std::cout << "For: " << a << "x" << b << " = " << (a*b) << std::endl;
    std::cout <<
        "a   value: " << std::hex << std::uppercase << fp_a << "\n"
        "b   value: " << std::hex << std::uppercase << fp_b << "\n"
        "res value: " << std::hex << std::uppercase << res  << std::endl;
    if (mag(d - (a*b)) > MAX_ERROR) {
        throw std::runtime_error(
            "Stopping test (failed), " + std::to_string(d) + " != " +
            std::to_string(a*b));
    }
}

void test_string_processing() {
    const char * const input_text =
        "jump b 0\n"
        "jump 0\n\r"
        "cmp  a b\n\n\n"
        "jump b\n"
        "load b a 10 # comment 3\n"
        "\n"
        "save a b   0\n"
        "save a b -10\n"
        "load a b  -1\r"
        "load a b\n"
        "test\n\n";
    erfin::Assembler asmr;
    asmr.assemble_from_string(input_text);
}

class StringToBitmapper {
public:
    using UInt32 = erfin::UInt32;
    StringToBitmapper(const char * str);
    UInt32 next_32b();
    bool is_done() const;
private:
    const char * m_str;
};

StringToBitmapper::StringToBitmapper(const char * str): m_str(str) {}

erfin::UInt32 StringToBitmapper::next_32b() {
    UInt32 rv = 0;
    for (int i = 0; i != 32 && m_str[i]; ++i) {
        if (m_str[i] != ' ')
            rv |= (1 << i);
        ++m_str;
    }
    return rv;
}

bool StringToBitmapper::is_done() const { return !(*m_str); }

std::vector<erfin::UInt32> make_demo_app() {
    using namespace erfin;
    const char * const ERFINDUNG_STRING =
    "  XX XXXXXX  X XXXXXX   XX XXXXXX  XXXXXXXXXX               X                               "
    "  X          X      XX  XX                                  X                               "
    "  X          X X     X  XX             XX                   X                               "
    "  X          X X     X  XX             XX                   X                               "
    "  X          X X     X  XX             XX                   X                               "
    "  X          X X     X  XX             XX                   X                               "
    "  X          X X     X  XX             XX                   X                               "
    "  XX XXXXXX  X          XX XXXX        XX                   X                               "
    "  XX         X  XX      X              XX      X XXXX  XXXX X  X    X  X XXXX  XXXXXX       "
    "  XX         X    XX    X              XX      X    X  X    X  X    X  X    X  X    X       "
    "  XX         X      X   X              XX      X    X  X    X  X    X  X    X  X    X       "
    "  XX         X       X  X              XX      X    X  X    X  X    X  X    X  X    X       "
    "  XX         X       X  X                      X    X  X    X  X    X  X    X  X    X       "
    "  XXXXXXXXX  X       X  X          XXXXXXXXXX  X    X  XXXXXX  XXXX X  X    X  XXXX X       "
    "                                                                                    X       "
    "                                                                                    X       "
    "                                                                                    X       "
    "                                                                                    X       "
    "X X X X X X X X X X X X X X X XX  XX  XX  XX  XX  XXXXXXXXXXXXXXXXXXXXXXXX     XXXXXX       ";
    const char * const WIDTH_STR =
    "                                                                                            ";

    (void)WIDTH_STR;
    using namespace erfin::enum_types;
    // jump past text storage
    std::vector<erfin::Inst> app_mem;
    app_mem.push_back(0); // reserved for jump-over
    StringToBitmapper stbm(ERFINDUNG_STRING);
    while (!stbm.is_done()) {
        app_mem.push_back(stbm.next_32b());
    }
#   if 0
    //app_mem[0] = make_jump(make_immd(app_mem.size(), IMMD_ABS_INT));
    // load it!
    app_mem.push_back(make_gset_index(REG_A));

    /*app_mem.push_back(make_set_immd(REG_X, strlen(WIDTH_STR)));
    app_mem.push_back(make_set_immd(REG_Y,
        (strlen(ERFINDUNG_STRING)/strlen(WIDTH_STR), IMMD_ABS_INT)));*/
    app_mem.push_back(make_gsize(REG_X, REG_Y));

    app_mem.push_back(make_gload_addr(REG_A, 1));

    // draw it!
    app_mem.push_back(make_draw(REG_A, REG_B));
    app_mem.push_back(make_draw_flush());
#   endif
    return app_mem;
}

int main() {
    std::cout << erfin::enum_types::OPCODE_COUNT << std::endl;
    erfin::ErfiCpu::run_tests();
    test_string_processing();

    using namespace erfin;

    try {
        test_fixed_point(  2.0);
        test_fixed_point( -1.0);
        test_fixed_point( 10.0);
        test_fixed_point(  0.1);
        test_fixed_point(-10.0);
        test_fixed_point( -0.1);

        test_fixed_point( 32767.0);
        test_fixed_point(-32767.0);

        // minumum value
        test_fixed_point( 0.00001525878);
        test_fixed_point(-0.00001525878);
        // maximum value
        /* (1/2)+(1/4)+(1/8)+(1/16)+(1/32)+(1/64)+(1/128)+(1/256)
           +(1/512)+(1/1024)+(1/2048)+(1/4096)
           +(1/8192)+(1/16384)+(1/32768)+(1/65536) */
        test_fixed_point( 32767.9999923706);
        test_fixed_point(-32767.9999923706);

        test_fp_multiply(2.0, 2.0);
        test_fp_multiply(-1.0, 1.0);
        test_fp_multiply(10.0, 10.0);
        test_fp_multiply(100.0, 100.0);
        test_fp_multiply(0.5, 0.5);
        test_fp_multiply(1.1, 1.1);
        test_fp_multiply(200.0, 0.015625);

        //test_instructions();
    } catch (std::exception & ex) {
        std::cout << ex.what() << std::endl;
        return 1;
    }

    std::cout << erfin::enum_types::OPCODE_COUNT << std::endl;
    return 0;
}
