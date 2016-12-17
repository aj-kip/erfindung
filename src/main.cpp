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
//#include "DrawRectangle.hpp"
#include "ErfiDefs.hpp"
#include "FixedPointUtil.hpp"
//#include <SFML/Graphics.hpp>
#include <cstring>
#include "Assembler.hpp"

#include "ErfiGpu.hpp"

namespace erfin {

inline OpCode get_op_code(Inst inst) {
    inst >>= 22;
    return static_cast<OpCode>(inst & 0x3F);
}

inline int get_r0_index(Inst inst) { return (inst & 0x380000) >> (16+3); }
inline int get_r1_index(Inst inst) { return (inst &  0x70000) >> (16  ); }
inline int get_r2_index(Inst inst) { return (inst &   0xE000) >> (16-3); }
inline int get_r3_index(Inst inst) { return (inst &   0x1C00) >> (16-6); }



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
// for op: SKIP    , // one arg , conditional, if flags is zero
// for op: JUMP    , // one IMMD, one answer (to PC)
inline Inst make_jump(UInt32 immd) {
    return encode_op(enum_types::JUMP) | immd;
}

// for op: JUMP_REG,
// for op: LOAD, // two regs, one immd
// for op: SAVE, // two regs, one immd
// for op: SET_INT , // one reg, one IMMD
inline Inst make_set_int(Reg r0, UInt32 immd) {
    return encode_op(enum_types::SET_INT) | encode_reg(r0) | immd;
}

// for op: SET_ABS , // one reg, one IMMD
inline Inst make_set_abs(Reg r0, UInt32 immd) {
    return encode_op(enum_types::SET_ABS) | encode_reg(r0) | immd;
}
// for op: SET_FP96, // one reg, one IMMD

// for op: SET_REG , // two regs
inline Inst make_set_reg(Reg r0, Reg r1) {
    return encode_op(enum_types::SET_REG) | encode_reg_reg(r0, r1);
}

inline Inst make_gload_addr(Reg r0, UInt32 immd) {
    return encode_op(enum_types::GLOAD_ADDR) | encode_reg(r0) | immd;
}

inline Inst make_gload_reg(Reg r0) {
    return encode_op(enum_types::GLOAD_REG) | encode_reg(r0);
}

inline Inst make_gsize(Reg r0, Reg r1) {
    return encode_op(enum_types::GSIZE) | encode_reg_reg(r0, r1);
}

inline Inst make_gset_index(Reg r0) {
    return encode_op(enum_types::GSET_INDEX) | encode_reg(r0);
}

inline Inst make_draw(Reg r0, Reg r1) {
    return encode_op(enum_types::DRAW) | encode_reg_reg(r0, r1);
}

inline Inst make_draw_flush() {
    return encode_op(enum_types::DRAW_FLUSH);
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

// <-------------------------- erfin implementations ------------------------->

namespace erfin {

ParamForm get_op_param(OpCode op) {
    using namespace enum_types;
    switch (op) {
    case PLUS: case MINUS: case TIMES:
        return REG_REG_REG;
    case DIVIDE_MOD:
        return REG_REG_REG_REG;
    //case PLUS_IMMD: case MINUS_IMMD:
        //return REG_REG_IMMD;
    case AND: case NOT: case XOR: case OR:
        return REG_REG_REG;

        //return REG_REG_IMMD;
    case COMP    : return REG_REG_REG;
    case SKIP    : return REG;
    case JUMP    : return REG_IMMD;
    case JUMP_REG: return REG;
    case LOAD: case SAVE:
        return REG_REG_IMMD;
    case SET_REG : return REG_REG;
    case GLOAD_ADDR: return REG_IMMD;
    case GLOAD_REG : return REG;
    case GSIZE     : return REG_REG;
    case GSET_INDEX: return REG;
    case DRAW      : return REG_REG;
    case DRAW_FLUSH: return NO_PARAMS;
    // gets elapsed time for draw flush and input device
    case SYSTEM_READ: return REG_IMMD;
    default: return INVALID_PARAMS;
    }
}

void do_cycle(RegisterPack & regs, MemorySpace & mem, Gpu & gpu, Inst inst) {
    using namespace enum_types;
    // "decode"
    UInt32 * r0 = nullptr, * r1 = nullptr, * r2 = nullptr, * r3 = nullptr;
    UInt32 immd = 0;
    (void)r3;
    switch (get_op_param(get_op_code(inst))) {
    case REG_REG_REG_REG:
        r3 = &regs[get_r3_index(inst)];
    case REG_REG_REG:
        r2 = &regs[get_r2_index(inst)];
    case REG_REG_IMMD: case REG_REG:
        r1 = &regs[get_r1_index(inst)];
    case REG_IMMD: case REG:
        r0 = &regs[get_r0_index(inst)];
    default: break;
    }

    switch (get_op_param(get_op_code(inst))) {
    case REG_REG_IMMD: case REG_IMMD:
        //immd = get_immd(inst);
    default: break;
    }

    // execute
    switch (get_op_code(inst)) {
    // fundemental computations
    case PLUS : *r2 = *r0 + *r1; break;
    case MINUS: *r2 = *r0 - *r1; break;
    case TIMES: *r2 = fp_multiply(*r0, *r1); break;
    case DIVIDE_MOD: break;
    //case PLUS_IMMD: *r1 = *r0 + immd; break;

    //case MINUS_IMMD: *r1 = *r0 - immd; break;

    case AND     : *r2 = *r0 & *r1; break;
    case NOT     : *r1 = ~*r0     ; break;
    case XOR     : *r2 = *r0 ^ *r1; break;
    case OR      : *r2 = *r0 | *r1; break;

    // flow control
    case COMP    :
        *r2 =   (*r0 < *r1) | ((*r0 > *r1) << 1) | ((*r0 == *r1) << 2)
              | ((*r0 != *r1) << 3);
        break;
    case SKIP    : if (*r0 == 0) regs[REG_PC] += sizeof(Inst); break;
    case JUMP    : regs[REG_PC] = immd; break;
    case JUMP_REG: regs[REG_PC] =  *r0; break; // what's special about this vs SET
    // loading/saving
    case LOAD: *r1 = mem[*r0 + immd]; break;
    case SAVE: mem[*r1 + immd] = *r0; break;
    // setting values
    case SET_INT : *r0 = immd; break;
    case SET_ABS : *r0 = immd; break;
    case SET_FP96: *r0 = immd; break;
    case SET_REG : *r0 = *r1; break;
    // graphics operations
    case GLOAD_ADDR: gpu.load(mem[*r0 + immd]); break;
    case GLOAD_REG : gpu.load(*r0); break;
    case GSIZE     : gpu.size(*r0, *r1); break;
    case GSET_INDEX: gpu.set_index(*r0); break;
    case DRAW      : gpu.draw(*r0, *r1); break;
    case DRAW_FLUSH: gpu.draw_flush(mem); break;
    // device read instruction
    case SYSTEM_READ: break;
    default: break;
    }
}

void do_cycle(RegisterPack & regs, MemorySpace & mem, Gpu & gpu) {
    // fetch
    do_cycle(regs, mem, gpu, mem[regs[enum_types::REG_PC]]);
    ++regs[enum_types::REG_PC];
}

} // end of erfin namespace

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

void test_instructions() {
    using namespace erfin;
    using namespace erfin::enum_types;
    using std::cout;
    using std::endl;
    // first register and immdetiates in instructions
    Gpu gpu;
    MemorySpace mem;
    // second all instructions (except GPU/system_read) and their results
    // PLUS
    {
    RegisterPack regs;
    regs[REG_A] = to_fixed_point(10.0);
    regs[REG_B] = to_fixed_point(12.0);
    regs[REG_X] = to_fixed_point(0.0);
    Inst i = make_plus(REG_A, REG_B, REG_X);
    do_cycle(regs, mem, gpu, i);
    assert(regs[REG_X] == to_fixed_point(22.0));
    }
    // MINUS
    {
    RegisterPack regs;
    regs[REG_A] = to_fixed_point(10.0);
    regs[REG_B] = to_fixed_point(12.0);
    regs[REG_X] = to_fixed_point(0.0);
    Inst i = make_minus(REG_A, REG_B, REG_X);
    do_cycle(regs, mem, gpu, i);
    assert(regs[REG_X] == to_fixed_point(-2.0));
    }
    // TIMES
    {
    RegisterPack regs;
    regs[REG_A] = to_fixed_point( 3.25);
    regs[REG_B] = to_fixed_point( 4.5 );
    regs[REG_X] = to_fixed_point( 0.0 );
    Inst i = make_times(REG_A, REG_B, REG_X);
    do_cycle(regs, mem, gpu, i);
    //UInt32 fp = to_fixed_point( 14.625);
    //assert(regs[REG_X] == fp);
    }
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

    return app_mem;
}

int main() {
    std::cout << erfin::enum_types::OPCODE_COUNT << std::endl;
    test_string_processing();
    return 0;

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
    (void)make_demo_app();

    Gpu gpu;
#   if 0
    sf::RenderWindow window;
    window.create(sf::VideoMode(Gpu::SCREEN_WIDTH*3, Gpu::SCREEN_HEIGHT*3), " ");

    {
    sf::View v;
    v.setSize(Gpu::SCREEN_WIDTH, Gpu::SCREEN_HEIGHT);
    v.setCenter(Gpu::SCREEN_WIDTH/2, Gpu::SCREEN_HEIGHT/2);
    window.setView(v);
    window.setFramerateLimit(30);
    }
#   endif

    MemorySpace ram;

    gpu.set_index(0);
    gpu.size(8, 8);
    ram[0] = 0x0103070F;
    ram[1] = 0x103070F0;
    gpu.load(0);
#   if 1
    gpu.set_index(1);
    gpu.size(12, 15); // 180 bits
    ram[2] = 0x12345678;
    ram[3] = 0x9ABCDEF0;
    ram[4] = 0x12345678;
    ram[5] = 0x9ABCDEF0;
    ram[6] = 0x12345678;
    ram[7] = 0x9ABCDEF0;
    gpu.load(2);
#   endif
    gpu.draw(0, 0);
    gpu.set_index(0);
    gpu.draw(120, 120);
    gpu.set_index(1);
    gpu.draw(Gpu::SCREEN_WIDTH - 4, Gpu::SCREEN_HEIGHT - 4);
    gpu.draw_flush(ram);
#   if 0
    DrawRectangle drect;
    drect.set_size(1.f, 1.f);
    drect.set_color(sf::Color::White);
    while (window.isOpen()) {
        {
        sf::Event e;
        while (window.pollEvent(e)) {
            switch (e.type) {
            case sf::Event::Closed:
                window.close();
                break;
            default: break;
            }
        }
        }
        window.clear(sf::Color::Black);
        gpu.draw_pixels([&window, &drect](int x, int y, bool p) {
            if (!p) return;
            drect.set_position(x, y);
            window.draw(drect);
        });
        window.display();
    }
#   endif
    return 0;
}
