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

#include <SFML/Graphics.hpp>

#include "ErfiDefs.hpp"
#include "FixedPointUtil.hpp"
#include "DrawRectangle.hpp"

#include <cstring>

#include "Assembler.hpp"
#include "ErfiCpu.hpp"
#include "ErfiGpu.hpp"

namespace {
    using Error = std::runtime_error;
}

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
        "jump b\n"
        "jump 0\n\r"
        "cmp  a b\n\n\n"
        "jump b\n"
        "load b a 10 # comment 3\n"
        "\n"
        "save a b   0\n"
        "save a b -10\n"
        "load a b  -1\r"
        "load a b\n"
        "\n\n";
    erfin::Assembler asmr;
    asmr.assemble_from_string(input_text);
}

int main() {
    using namespace erfin;
#   if 0
    std::cout << enum_types::OPCODE_COUNT << std::endl;
    ErfiCpu::run_tests();
    test_string_processing();
#   endif
    Assembler asmr;
    ErfiCpu cpu;
    ErfiGpu gpu;
    MemorySpace memory;

    try {
        asmr.assemble_from_file("sample.efas");
    } catch (std::exception & exp) {
        std::cout << exp.what() << std::endl;
        return ~0;
    }

    load_program_into_memory(memory, asmr.program_data());

    sf::RenderWindow win;
    win.create(sf::VideoMode(unsigned(ErfiGpu::SCREEN_WIDTH*3),
                             unsigned(ErfiGpu::SCREEN_HEIGHT*3)), " ");
    {
    sf::View view = win.getView();
    //view.zoom(0.999f);
    view.setCenter(ErfiGpu::SCREEN_WIDTH/2, ErfiGpu::SCREEN_HEIGHT/2);
    view.setSize(ErfiGpu::SCREEN_WIDTH, ErfiGpu::SCREEN_HEIGHT);
    win.setView(view);
    }
    while (win.isOpen()) {
        {
        sf::Event e;
        while (win.pollEvent(e)) {
            switch (e.type) {
            case sf::Event::KeyReleased:
                if (e.key.code == sf::Keyboard::Escape)
                    win.close();
                break;
            default: break;
            }
        }
        }
        win.clear();

        cpu.clear_flags();

        try {
            while (!cpu.wait_was_called())
                cpu.run_cycle(memory, &gpu);
        } catch (ErfiError & exp) {
            std::cerr << "A problem has occured on source line: "
                      << asmr.translate_to_line_number(exp.program_location())
                      << std::endl;
            std::cerr << exp.what() << std::endl;
            return ~0;
        }

        DrawRectangle brush;
        brush.set_size(1.f, 1.f);
        gpu.draw_pixels([&win, &brush](int x, int y) {
            brush.set_position(float(x), float(y));
            win.draw(brush);
        });
        gpu.wait(memory);

        win.display();
    }

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

    std::cout << "# of op codes " << erfin::enum_types::OPCODE_COUNT << std::endl;
    return 0;
}
