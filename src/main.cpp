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
#include <fstream>

#include <SFML/Graphics.hpp>

#include "ErfiDefs.hpp"
#include "FixedPointUtil.hpp"
#include "DrawRectangle.hpp"
#include "Debugger.hpp"
#include "StringUtil.hpp"
#include "Assembler.hpp"
#include "ErfiConsole.hpp"

#include "tests.hpp"
#include "parse_program_options.hpp"

#include <cstring>

#ifdef MACRO_PLATFORM_LINUX
#   include <signal.h>
#endif

namespace {

using Error = std::runtime_error;

class ExecutionHistoryLogger {
public:
    explicit ExecutionHistoryLogger(int frame_limit) noexcept;
    ExecutionHistoryLogger(ExecutionHistoryLogger &&) = delete;
    ExecutionHistoryLogger(const ExecutionHistoryLogger &) = delete;

    ExecutionHistoryLogger & operator = (ExecutionHistoryLogger &&) = delete;
    ExecutionHistoryLogger & operator = (const ExecutionHistoryLogger &) = delete;

    void push_frame(const erfin::Debugger & debugger);
    std::string to_string() const;

private:
    int m_frame_limit;

    std::vector<std::string> m_frames;
};

} // end of <anonymous> namespace

int main(int argc, char ** argv) {

    erfin::Assembler assembler;
    try {
        auto options = erfin::parse_program_options(argc, argv);
        if (options.run_tests) run_tests();
        if (options.input_stream_ptr)
            assembler.assemble_from_stream(*options.input_stream_ptr);
        assembler.print_warnings(std::cout);
        options.assembler = &assembler;
        options.mode(options, assembler.program_data());
        return 0;
    } catch (erfin::ErfiCpuError & exp) {
        using namespace erfin;
        constexpr const char * const NO_LINE_NUM_MSG =
            "<no source line associated with this program location>";
        auto line_num = assembler.translate_to_line_number(exp.program_location());
        std::cerr << "A problem has occured on source line: ";
        if (line_num == Assembler::INVALID_LINE_NUMBER)
            std::cerr << NO_LINE_NUM_MSG;
        else
            std::cerr << line_num;
        std::cerr << "\n(At address " << exp.program_location() << ")\n";
        std::cerr << exp.what() << std::endl;
    } catch (std::exception & exp) {
        std::cerr << exp.what() << std::endl;
    } catch (...) {
        std::cerr << "An unknown exception was thrown." << std::endl;
    }
    return ~0;
}

namespace {

using ProgramOptions = erfin::ProgramOptions;

ExecutionHistoryLogger::ExecutionHistoryLogger(int frame_limit) noexcept:
    m_frame_limit(frame_limit)
{}

void ExecutionHistoryLogger::push_frame(const erfin::Debugger & debugger) {
    if (unsigned(m_frame_limit) == m_frames.size()) {
        m_frames.erase(m_frames.begin(), m_frames.begin() + 1);
    }
    m_frames.push_back(debugger.print_current_frame_to_string());
}

std::string ExecutionHistoryLogger::to_string() const {
    std::string rv;
    for (auto & str : m_frames)
        rv += str;
    return rv;
}

// ----------------------------------------------------------------------------

void setup_window_view(sf::RenderWindow & window, const ProgramOptions &);

void process_events(erfin::Console & console, sf::Window & window);

template <typename Func>
void run_console_loop(erfin::Console & console, sf::RenderWindow & window,
                      Func && do_between_cycles);

} // end of <anonymous> namespace

void normal_windowed_run
    (const ProgramOptions & opts, const erfin::ProgramData & program)
{
    using namespace erfin;
    Console console;
    console.load_program(program);

    sf::RenderWindow win;
    setup_window_view(win, opts);

    run_console_loop(console, win, [](){});
}

void watched_windowed_run
    (const ProgramOptions & opts, const erfin::ProgramData & program)
{
    using namespace erfin;
    Console console;
    Debugger debugger;
    ExecutionHistoryLogger exlogger(opts.watched_history_length);
    opts.assembler->setup_debugger(debugger);
    console.load_program(program);
    for (auto bp : opts.break_points)
        debugger.add_break_point(bp);

    sf::RenderWindow win;
    setup_window_view(win, opts);

    try {
        run_console_loop(console, win, [&]() {
            console.update_with_current_state(debugger);
            exlogger.push_frame(debugger);
            if (debugger.at_break_point()) {
                std::cout << debugger.print_current_frame_to_string() << std::endl;
            }
        });
    } catch (std::exception & exp) {
        throw Error(std::string(exp.what()) +
                    "\nAdditionally the prefail frames are as follows:\n" +
                    exlogger.to_string()                                   );
    } catch (...) {
        throw;
    }

    std::cout << "Program finished without errors.\n" << exlogger.to_string();
}

namespace {

#ifdef MACRO_PLATFORM_LINUX
template <typename Func>
void global_console_pointer_access(Func && f) {
    static std::mutex access_mtx;
    static erfin::Console * g_console_ptr = nullptr;
    access_mtx.lock();
    f(std::ref(g_console_ptr));
    access_mtx.unlock();
}
#endif

void print_frame(const erfin::Console & console) {
    erfin::Debugger debugger;
    console.update_with_current_state(debugger);
    std::cout << debugger.print_current_frame_to_string() << std::endl;
}

} // end of <anonymous> namespace

void cli_run(const ProgramOptions &, const erfin::ProgramData & program) {    
    using namespace erfin;
    Console console;

#   ifdef MACRO_PLATFORM_LINUX
    global_console_pointer_access([&console](Console *& c_ptr) {
        c_ptr = &console;
    });
    signal(SIGPROF, [/* must not capture */](int) {
        global_console_pointer_access([](Console *& c_ptr) {
            print_frame(*c_ptr);
        });
    });
#   endif

    console.load_program(program);
    while (!console.trying_to_shutdown()) {
        console.run_until_wait();
        sf::sleep(sf::microseconds(16667));
    }
    print_frame(console);

#   ifdef MACRO_PLATFORM_LINUX
    global_console_pointer_access([&console](Console *& c_ptr) {
        c_ptr = nullptr;
    });
#   endif
}

void print_help(const ProgramOptions &, const erfin::ProgramData &) {
    std::cout <<
         "Erfindung command line options\n"
         "If the entire text does not show, you can always stream the "
         "output to a file or use \"less\" on *nix machines.\n\n"
         "-i / --input\n"
         "\tSpecify input file, not compatible with --stream-input "
          "option\n"
         "-h / --help\n"
         "\tShow this help text.\n"
         "-c / --command-line\n"
         "\tCauses the program to not open a window, the program will "
          "finsih once and only when the halt signal is sent (you "
          "can still cancel the program with ctrl-c as usual)\n"
         "-t / --run-tests\n"
         "\tRun developer tests (for debugging purposes only). If you "
          "run this and the program doesn't crash, that means it "
          "works!\n"
         "-s / --stream-input\n"
         "\tThe program will accept stdin as a source \"file\" this "
          "option is not compatible with -i.\n"
         "-w / --window-scale\n"
         "\tScales the window by an integer factor.\n"
         "Each program \"command\" accepts parameters between each "
         "of these \"commands\" e.g.\n"
         "\n\t ./erfindung -i sample.efas -w 3 -c\n"
         "Erfindung is GPLv3 software, refer to COPYING on the terms "
         "and conditions for copying.\n"
         "There is a software manual that should be present with "
         "your distrobution that you can refer to on how to use the "
         "software." << std::endl;
}

namespace {

void setup_window_view(sf::RenderWindow & window, const ProgramOptions & opts) {
    using namespace erfin;
    sf::VideoMode vm;
    vm.width  = unsigned(ErfiGpu::SCREEN_WIDTH *opts.window_scale);
    vm.height = unsigned(ErfiGpu::SCREEN_HEIGHT*opts.window_scale);
    window.create(vm, " ");

    sf::View view = window.getView();
    view.setCenter(float(ErfiGpu::SCREEN_WIDTH /2),
                   float(ErfiGpu::SCREEN_HEIGHT/2));
    view.setSize(float(ErfiGpu::SCREEN_WIDTH), float(ErfiGpu::SCREEN_HEIGHT));
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);
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

template <typename Func>
void run_console_loop(erfin::Console & console, sf::RenderWindow & window,
                      Func && do_between_cycles)
{
    using namespace erfin;
    sf::Clock clk;
    int fps = 0;
    int frame_count = 0;

    sf::Texture screen_pixels;
    std::vector<UInt32> pixel_array;
    screen_pixels.create(unsigned(ErfiGpu::SCREEN_WIDTH),
                         unsigned(ErfiGpu::SCREEN_HEIGHT));
    pixel_array.resize(unsigned(ErfiGpu::SCREEN_HEIGHT*ErfiGpu::SCREEN_WIDTH), 0);

    while (window.isOpen()) {
        if (double(clk.getElapsedTime().asSeconds()) >= 1.0) {
            fps = frame_count;
            frame_count = 0;
            clk.restart();
        }
        ++frame_count;

        process_events(console, window);

        window.clear();

        console.run_until_wait_with_post_frame(do_between_cycles);
        if (console.trying_to_shutdown())
            break;

        std::fill(pixel_array.begin(), pixel_array.end(), 0);
        console.draw_pixels([&window, &pixel_array](int x, int y) {
            pixel_array[unsigned(x + erfin::ErfiGpu::SCREEN_WIDTH*y)] = ~0u;
        });
        screen_pixels.update(reinterpret_cast<UInt8 *>(&pixel_array.front()),
                             unsigned(ErfiGpu::SCREEN_WIDTH),
                             unsigned(ErfiGpu::SCREEN_HEIGHT),
                             0, 0);

        sf::Sprite spt;
        spt.setTexture(screen_pixels);
        spt.setPosition(0.f, 0.f);
        window.draw(spt);

        window.display();
    }
    std::cout << "Last reported fps " << fps << std::endl;
}

} // end of <anonymous> namespace
