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
#include "Debugger.hpp"
#include "StringUtil.hpp"

#include <cstring>

#include "Assembler.hpp"
#include "ErfiConsole.hpp"
#include "ErfiApu.hpp"

namespace {

using Error = std::runtime_error;

void test_string_processing();

void run_numeric_encoding_tests();

void adjust_window_view(sf::RenderWindow & window);

void process_events(erfin::Console & console, sf::Window & window);

void run_console_loop(erfin::Console & console, sf::RenderWindow & window);

} // end of <anonymous> namespace

int main(int argc, char ** argv) {
    using namespace erfin;
#   if 0
    Apu apu;
    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    apu.update(0.1);
    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        // needs to be clearer that it's notes per second
        static constexpr const auto TRI = Channel::TRIANGLE;
        auto chan = Channel::TRIANGLE;

        apu.enqueue(Channel::PULSE_ONE, ApuInstructionType::TEMPO, 4);
        auto push_note =
            [&](int note) { apu.enqueue(chan, ApuInstructionType::NOTE, note); };

        //chan = Channel::PULSE_ONE;
        //for (int i = 0; i != 3*6; ++i) push_note(000);

        for (auto c : { Channel::TRIANGLE, Channel::PULSE_ONE, Channel::TRIANGLE }) {
            chan = Channel::TRIANGLE; (void)c;
            for (int i : { -50, 50, 0 }) {
                apu.enqueue(TRI, ApuInstructionType::TEMPO, 4);
                push_note(500 + i);
                push_note(375 + i);

                push_note(500 + i); // one second passes
                push_note(325 + i);

                push_note(500 + i); // one second passes
                push_note(275 + i);

                apu.enqueue(TRI, ApuInstructionType::TEMPO, 8);
                push_note( 75 + i);
                push_note(125 + i);
                push_note( 75 + i);
                push_note(125 + i);
            }
        }

        // implicit silence -- three seconds
    }
    apu.update(0.);
    //std::this_thread::sleep_for(std::chrono::milliseconds(20000));
#   endif
#   if 0
    for (int i = 0; i != 60; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        apu.update(0.1);
    }
#   endif
    //return 0;
    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    //apu.wait_for_play_thread_then_update();
#   if 0//def MACRO_DEBUG
    try {
        run_encode_decode_tests();
        run_numeric_encoding_tests();
        Assembler::run_tests();
        ErfiCpu::run_tests();
        test_string_processing();
    } catch (std::exception & exp) {
        std::cout << exp.what() << std::endl;
        return ~0;
    }
#   else
    (void)run_encode_decode_tests();
    (void)run_numeric_encoding_tests;
    (void)Assembler::run_tests;
    (void)ErfiCpu::run_tests;
    (void)test_string_processing;
#   endif

    Assembler assembler;
    Console console;
    sf::RenderWindow win;

    const char * filename = "sample.efas";
    if (argc > 1)
        filename = argv[1];

    try {
        assembler.assemble_from_file(filename);
        assembler.print_warnings(std::cout);
        console.load_program(assembler.program_data());
        win.create(sf::VideoMode(unsigned(ErfiGpu::SCREEN_WIDTH *3),
                                 unsigned(ErfiGpu::SCREEN_HEIGHT*3)), " ");
        adjust_window_view(win);
        run_console_loop(console, win);
    } catch (ErfiError & exp) {
        constexpr const char * const NO_LINE_NUM_MSG =
            "<no source line associated with this program location>";
        auto line_num = assembler.translate_to_line_number(exp.program_location());
        std::cerr << "A problem has occured on source line: ";
        if (line_num == Assembler::INVALID_LINE_NUMBER)
            std::cerr << NO_LINE_NUM_MSG;
        else
            std::cerr << line_num;
        std::cerr << "\n(At address " << exp.program_location() << ")";
        std::cerr << std::endl;
        std::cerr << exp.what() << std::endl;
        return ~0;
    } catch (std::exception & exp) {
        std::cout << exp.what() << std::endl;
        return ~0;
    }
    std::cout << std::dec;
    console.print_cpu_registers(std::cout);
    std::cout << "# of op codes " << static_cast<int>(erfin::OpCode::COUNT) << std::endl;
    return 0;
}

namespace {

void test_fp_multiply(double a, double b);

void test_fp_divide(double a, double b);

void test_fixed_point(double value);

void test_string_processing() {
    const char * const input_text =
        "assume integer\n"
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

void run_numeric_encoding_tests() {
    test_fp_multiply(-1.0, 1.0);
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

    std::cout << erfin::fixed_point_to_double(erfin::fp_inverse(erfin::to_fixed_point(0.5))) << std::endl;
    std::cout << erfin::fixed_point_to_double(erfin::fp_inverse(erfin::to_fixed_point(0.25))) << std::endl;
    std::cout << erfin::fixed_point_to_double(erfin::fp_inverse(erfin::to_fixed_point(0.3333333))) << std::endl;
    std::cout << erfin::fixed_point_to_double(erfin::fp_inverse(erfin::to_fixed_point(0.1))) << std::endl;

    test_fp_multiply(2.0, 2.0);
    test_fp_multiply(-1.0, 1.0);
    test_fp_multiply(10.0, 10.0);
    test_fp_multiply(100.0, 100.0);
    test_fp_multiply(0.5, 0.5);
    test_fp_multiply(1.1, 1.1);
    test_fp_multiply(200.0, 0.015625);

    test_fp_divide( 2.0, 1.0);
    test_fp_divide( 2.0, 4.0);
    test_fp_divide(10.0, 3.0);
    test_fp_divide( 2.0, 0.5);
    test_fp_divide( 0.5, 2.0);
    test_fp_divide( 1.1, 1.1);
    //test_fp_divide(-9.0, 1.1);
}

void adjust_window_view(sf::RenderWindow & window) {
    using namespace erfin;
    sf::View view = window.getView();
    view.setCenter(float(ErfiGpu::SCREEN_WIDTH /2),
                   float(ErfiGpu::SCREEN_HEIGHT/2));
    view.setSize(float(ErfiGpu::SCREEN_WIDTH), float(ErfiGpu::SCREEN_HEIGHT));
    window.setVerticalSyncEnabled(true);
    window.setView(view);
}

void process_events(erfin::Console & console, sf::Window & window) {
    sf::Event event;
    while (window.pollEvent(event)) {
        console.process_event(event);
        switch (event.type) {
        case sf::Event::KeyReleased:
            if (event.key.code == sf::Keyboard::Escape)
                window.close();
            if (event.key.code == sf::Keyboard::F5)
                console.press_restart();
            break;
        case sf::Event::Closed:
            window.close();
            break;
        default: break;
        }
    }
}

void run_console_loop(erfin::Console & console, sf::RenderWindow & window) {
    using namespace erfin;
    sf::Clock clk;
    int fps = 0;
    int frame_count = 0;
    //int pc_ticks = 0;
    sf::Texture screen_pixels;
    std::vector<UInt32> pixel_array;
    screen_pixels.create(ErfiGpu::SCREEN_WIDTH, ErfiGpu::SCREEN_HEIGHT);
    pixel_array.resize(ErfiGpu::SCREEN_HEIGHT*ErfiGpu::SCREEN_WIDTH, 0);

    while (window.isOpen()) {
        if (clk.getElapsedTime().asSeconds() >= 1.0) {
            fps = frame_count;
            frame_count = 0;
            clk.restart();
        }
        ++frame_count;

        process_events(console, window);

        window.clear();

        console.run_until_wait([](){
            //++pc_ticks;
            //std::cout << "tick " << pc_ticks << ":\n";
            //console.print_cpu_registers(std::cout);
        });
        if (console.trying_to_shutdown())
            break;

        std::fill(pixel_array.begin(), pixel_array.end(), 0);
        console.draw_pixels([&window, &pixel_array](int x, int y) {
            pixel_array[x + erfin::ErfiGpu::SCREEN_WIDTH*y] = ~0;
        });
        screen_pixels.update(reinterpret_cast<UInt8 *>(&pixel_array.front()),
                             ErfiGpu::SCREEN_WIDTH, ErfiGpu::SCREEN_HEIGHT,
                             0, 0);

        sf::Sprite spt;
        spt.setTexture(screen_pixels);
        spt.setPosition(0.f, 0.f);
        window.draw(spt);

        window.display();
    }
    std::cout << "Last reported fps " << fps << std::endl;
}
using UInt32 = erfin::UInt32;
template <UInt32(*FixedPtFunc)(UInt32, UInt32), double(*DoubleFunc)(double, double)>
void test_fp_operation(double a, double b, char op_char) {
    using namespace erfin;
    OstreamFormatSaver cfs(std::cout); (void)cfs;
    UInt32 fp_a = to_fixed_point(a);
    UInt32 fp_b = to_fixed_point(b);
    UInt32 res = FixedPtFunc(fp_a, fp_b);
    double d = fixed_point_to_double(res);

    const double MAX_ERROR = 0.00002;
    std::cout << "For: " << a << op_char << b << " = " << DoubleFunc(a, b) << std::endl;
    std::cout <<
        "a   value: " << std::hex << std::uppercase << fp_a << "\n"
        "b   value: " << std::hex << std::uppercase << fp_b << "\n"
        "res value: " << std::hex << std::uppercase << res  << std::endl;
    if (mag(d - DoubleFunc(a, b)) > MAX_ERROR) {
        throw std::runtime_error(
            "Stopping test (failed), " + std::to_string(d) + " != " +
            std::to_string(DoubleFunc(a, b)));
    }
}

double mul(double a, double b) { return a*b; }
double div(double a, double b) { return a/b; }

void test_fp_multiply(double a, double b) {
    test_fp_operation<erfin::fp_multiply, mul>(a, b, 'x');
}

void test_fp_divide(double a, double b) {
    test_fp_operation<erfin::fp_divide, div>(a, b, '/');
}

void test_fixed_point(double value) {
    using namespace erfin;
    OstreamFormatSaver cfs(std::cout); (void)cfs;
    UInt32 fp = to_fixed_point(value);
    double val_out = fixed_point_to_double(fp);
    double diff = val_out - value;
    // use an error about 1/(2^16), but "fatten" it up for error
    if (mag(diff) < 0.00002) {
        std::cout <<
            "For: " << value << "\n"
            "Fixed Point value: " << std::hex << std::uppercase << fp << "\n"
            "End value        : " << std::dec << std::nouppercase << val_out << "\n" <<
            std::endl;
        return;
    }
    // not equal!
    std::cout <<
        "Fixed point test failed!\n"
        "Starting         : " << value << "\n"
        "Fixed Point value: " << std::hex << std::uppercase << fp << "\n"
        "End value        : " << std::dec << std::nouppercase << val_out <<
         std::endl;
}

} // end of <anonymous> namespace
