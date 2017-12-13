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
#include <cassert>

#ifndef MACRO_BUILD_STL_ONLY
#   include <SFML/Graphics.hpp>
#endif

#include "ErfiDefs.hpp"
#include "Debugger.hpp"
#include "Assembler.hpp"
#include "ErfiConsole.hpp"

#include "tests.hpp"
#include "parse_program_options.hpp"

#ifdef MACRO_PLATFORM_LINUX
#   include <signal.h>
#endif

namespace {

using Error          = std::runtime_error   ;
using ProgramOptions = erfin::ProgramOptions;
using ProgramData    = erfin::ProgramData   ;

constexpr const char * const WINDOW_TITLE = "Erfindung - Beta";
constexpr const char * const HELP_TEXT =
    "Erfindung command line options:\n\n"
#   ifdef MACRO_BUILD_STL_ONLY
    "NOTE: This is a STL Only / Test build intended for unit testing.\n"
    "this also means that windowed mode will not be available.\n"
#   endif
    "If the entire text does not show, you can always stream the\n"
    "output to a file or use \"less\" on *nix machines.\n\n"
    "-i / --input\n"
    "Specify input file, not compatible with --stream-input option\n"
    "-h / --help\n"
    "Show this help text.\n"
#   ifndef MACRO_BUILD_STL_ONLY
    "-c / --command-line\n"
    "Causes the program to not open a window, the program will "
    "finsih once and only when the halt signal is sent (you "
    "can still cancel the program with ctrl-c as usual)\n"
#   endif
    "-t / --run-tests\n"
    "Run developer tests (for debugging purposes only). If you\n"
    "run this and the program doesn't crash, that means it\n"
    "works!\n"
    "-r / --stream-input\n"
    "The program will accept stdin as a source \"file\" this\n"
    "option is not compatible with -i.\n"
#   ifndef MACRO_BUILD_STL_ONLY
    "-s / --window-scale\n"
    "Scales the window by an integer factor.\n"
    "Each program \"command\" accepts parameters between each \n"
    "of these \"commands\" e.g.\n"
#   endif
    "-b --break-points\n"
    "Prints current frame at the given line numbers to the terminal. "
    "Lists registers and their values, and continues running the "
    "program. Invalid line numbers are ignored.\n"
    "-w -watch\n"
    "Implicitly enabled with breakpoints. Watch mode accepts one numeric\n"
    "argument n, for the number of frames to keep in run history. Run \n"
    "history is printed out if Erfindung runs into a problem with program\n"
    "execution, or is halted."
    "Example:\n"
    "\n\t ./erfindung -i sample.efas -w 3 -c\n"
    "Erfindung is GPLv3 software, refer to COPYING on the terms and \n"
    "conditions for copying.\n"
    "There is a software manual that should be present with your\n"
    "distrobution that you can refer to on how to use the software.";

constexpr const char * const MONOCHROMATIC_ICON_DATA =
    "________________________________"
    "__XX_XXXXXX_________X_XXXXXX____"
    "__X_________________X______XX___"
    "__X_________________X_X_____X___"
    "__X_________________X_X_____X___"
    "__X_________________X_X_____X___"
    "__X_________________X_X_____X___"
    "__X_________________X_X_____X___"
    "__XX_XXXXXX_________X___________"
    "__XX________________X__XX_______"
    "__XX________________X____XX_____"
    "__XX________________X______X____"
    "__XX________________X_______X___"
    "__XX________________X_______X___"
    "__XXXXXXXXX_________X_______X___"
    "________________________________"
    "________________________________"
    "__XX_XXXXXX_________XXXXXXXXXX__"
    "__XX____________________________"
    "__XX____________________XX______"
    "__XX____________________XX______"
    "__XX____________________XX______"
    "__XX____________________XX______"
    "__XX____________________XX______"
    "__XX_XXXX_______________XX______"
    "__X_____________________XX______"
    "__X_____________________XX______"
    "__X_____________________XX______"
    "__X_____________________XX______"
    "__X_____________________________"
    "__X_________________XXXXXXXXXX__"
    "________________________________";

constexpr const unsigned ICON_WIDTH  = 32u;
constexpr const unsigned ICON_HEIGHT = 32u;

class ExecutionHistoryLogger {
public:
    explicit ExecutionHistoryLogger(int frame_limit) noexcept;
    ExecutionHistoryLogger(ExecutionHistoryLogger &&) = delete;
    ExecutionHistoryLogger(const ExecutionHistoryLogger &) = delete;

    ExecutionHistoryLogger & operator = (ExecutionHistoryLogger &&) = delete;
    ExecutionHistoryLogger & operator = (const ExecutionHistoryLogger &) = delete;

    void push_frame(const erfin::Debugger & debugger);
    std::string to_string(const erfin::Debugger & debugger) const;

private:
    int m_frame_limit;
    std::vector<erfin::DebuggerFrame> m_frames;
};

} // end of <anonymous> namespace

int main(int argc, char ** argv) {
    using std::cout;
    using std::cerr;
    using std::endl;
    erfin::Assembler assembler;
    try {
        auto options = erfin::parse_program_options(argc, argv);
        if (options.input_stream_ptr) {
            assembler.assemble_from_stream(*options.input_stream_ptr);
            assembler.print_warnings(cout);
            cout << "Program size: "
                 << assembler.program_data().size()*sizeof(erfin::Inst)
                 << " / " << erfin::MEMORY_CAPACITY << " bytes." << endl;
        }
        options.assembler = &assembler;
        options.mode(options, assembler.program_data());
        return 0;
    } catch (erfin::ErfiCpuError & exp) {
        using namespace erfin;
        constexpr const char * const NO_LINE_NUM_MSG =
            "<no source line associated with this program location>";
        auto line_num = assembler.translate_to_line_number(exp.program_location());
        cerr << "A problem has occured on source line: ";
        if (line_num == Assembler::INVALID_LINE_NUMBER)
            cerr << NO_LINE_NUM_MSG;
        else
            cerr << line_num;
        cerr << "\n(At address " << exp.program_location() << ")\n";
        cerr << exp.what() << endl;
    } catch (std::exception & exp) {
        cerr << exp.what() << endl;
    } catch (...) {
        cerr << "An unknown exception was thrown." << endl;
    }
    return ~0;
}

// ----------------------------------------------------------------------------

namespace {

enum { WINDOWED, TERMINAL };

template <decltype (WINDOWED) UI_TYPE>
void do_watched_mode(const ProgramOptions & opts, const ProgramData & program);

template <decltype (WINDOWED) UI_TYPE>
void do_unwatched_mode(const ProgramOptions & opts, const ProgramData & program);

} // end of <anonymous> namespace

#ifndef MACRO_BUILD_STL_ONLY

void normal_windowed_run
    (const ProgramOptions & opts, const ProgramData & program)
    { do_unwatched_mode<WINDOWED>(opts, program); }

void watched_windowed_run
    (const ProgramOptions & opts, const ProgramData & program)
    { do_watched_mode<WINDOWED>(opts, program); }

#endif // ifndef MACRO_BUILD_STL_ONLY

void cli_run
    (const ProgramOptions & options, const ProgramData & program)
    { do_unwatched_mode<TERMINAL>(options, program); }

void watched_cli_run
    (const ProgramOptions & options, const ProgramData & program)
    { do_watched_mode<TERMINAL>(options, program); }

void print_help(const ProgramOptions &, const ProgramData &)
    { std::cout << HELP_TEXT << std::endl; }

namespace {

ExecutionHistoryLogger::ExecutionHistoryLogger(int frame_limit) noexcept:
    m_frame_limit(frame_limit) {}

void ExecutionHistoryLogger::push_frame(const erfin::Debugger & debugger) {
    if (m_frame_limit == 0) return;
    if (unsigned(m_frame_limit) == m_frames.size()) {
        m_frames.erase(m_frames.begin(), m_frames.begin() + 1);
    }
    m_frames.push_back(debugger.current_frame());
}

std::string ExecutionHistoryLogger::to_string
    (const erfin::Debugger & debugger) const
{
    std::string rv;
    for (const auto & frame : m_frames)
        rv += debugger.print_frame_to_string(frame);
    return rv;
}

} // end of <anonymous> namespace

// ----------------------------------------------------------------------------

namespace {

template <typename Func>
void in_windowed_mode
    (const ProgramOptions & opts, erfin::Console & console,
     Func && do_between_cycles);

template <typename Func>
void in_terminal_mode
    (const ProgramOptions &, erfin::Console & console,
     Func && do_between_cycles);

template <decltype (WINDOWED) UI_TYPE>
void do_watched_mode
    (const ProgramOptions & opts, const erfin::ProgramData & program)
{
    using namespace erfin;
    Console console;
    Debugger debugger;
    ExecutionHistoryLogger exlogger(opts.watched_history_length);
    opts.assembler->setup_debugger(debugger);
    console.load_program(program);
    for (auto bp : opts.break_points) {
        auto actual_line = debugger.add_break_point(bp);
        if (bp != actual_line) {
            std::cout << "Failed to add breakpoint to line: " << bp
                      << " adding breakpoint to proximal line: " << actual_line
                      << std::endl;
        }
    }

    try {
        auto between_cycles = [&]() {
            console.update_with_current_state(debugger);
            exlogger.push_frame(debugger);
            if (debugger.at_break_point()) {
                std::cout << debugger.print_current_frame_to_string() << std::endl;
            }
        };
        if (UI_TYPE == WINDOWED) {
            in_windowed_mode(opts, console, std::move(between_cycles));
        } else {
            in_terminal_mode(opts, console, std::move(between_cycles));
        }
    } catch (std::exception & exp) {
        throw Error(std::string(exp.what()) +
                    "\nAdditionally the prefail frames are as follows:\n" +
                    exlogger.to_string(debugger)                           );
    } catch (...) {
        throw;
    }

    std::cout << "Program finished without simulation errors.\n"
              << exlogger.to_string(debugger);
}

template <decltype (WINDOWED) UI_TYPE>
void do_unwatched_mode
    (const ProgramOptions & opts, const erfin::ProgramData & program)
{
    using namespace erfin;
    Console console;
    console.load_program(program);
    if (UI_TYPE == WINDOWED) {
        in_windowed_mode(opts, console, [](){});
    } else {
        in_terminal_mode(opts, console, [](){});
    }
}

} // end of <anonymous> namespace

// ----------------------------------------------------------------------------

namespace {

#ifndef MACRO_BUILD_STL_ONLY

void setup_window_view(sf::RenderWindow & window, const ProgramOptions &);

void process_events(erfin::Console & console, sf::Window & window);

void map_screen_to_texture
    (const erfin::Console & console, std::vector<erfin::UInt32> & raw_pixels);

#endif

#ifdef MACRO_PLATFORM_LINUX

template <typename Func>
void global_console_pointer_access(Func && f);

#endif

void print_frame(const erfin::Console & console);

template <typename Func>
void in_windowed_mode
    (const ProgramOptions & opts, erfin::Console & console,
     Func && do_between_cycles)
{
#   ifndef MACRO_BUILD_STL_ONLY
    using namespace erfin;
    const auto SCREEN_WIDTH  = unsigned(ErfiGpu::SCREEN_WIDTH );
    const auto SCREEN_HEIGHT = unsigned(ErfiGpu::SCREEN_HEIGHT);
    sf::Clock clk;
    int fps = 0;
    int frame_count = 0;

    sf::Texture screen_pixels;
    screen_pixels.create(SCREEN_WIDTH, SCREEN_HEIGHT);

    std::vector<UInt32> pixel_array(SCREEN_HEIGHT*SCREEN_WIDTH, 0);
    assert(pixel_array.size() == console.current_screen().size());

    sf::Sprite screen_sprite;
    screen_sprite.setTexture(screen_pixels);
    screen_sprite.setPosition(0.f, 0.f);

    sf::RenderWindow window;
    setup_window_view(window, opts);
    while (window.isOpen()) {
        if (double(clk.getElapsedTime().asSeconds()) >= 1.0) {
            fps = frame_count;
            frame_count = 0;
            clk.restart();
        }
        ++frame_count;

        process_events(console, window);

        window.clear();

        console.run_until_wait_with_post_frame(std::move(do_between_cycles));
        if (console.trying_to_shutdown())
            break;

        map_screen_to_texture(console, pixel_array);
        screen_pixels.update(reinterpret_cast<UInt8 *>(&pixel_array.front()),
                             SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0);

        window.draw(screen_sprite);

        window.display();
    }
    std::cout << "Last reported fps " << fps << std::endl;
#   else
    (void)opts;
    (void)console;
    (void)do_between_cycles;
#   endif
}

template <typename Func>
void in_terminal_mode
    (const ProgramOptions &, erfin::Console & console,
     Func && do_between_cycles)
{
    using namespace erfin;
    using MicroSeconds = std::chrono::duration<int, std::micro>;
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

    while (!console.trying_to_shutdown()) {
        console.run_until_wait_with_post_frame(std::move(do_between_cycles));
        std::this_thread::sleep_for(MicroSeconds(16667));
    }
    print_frame(console);

#   ifdef MACRO_PLATFORM_LINUX
    global_console_pointer_access([&console](Console *& c_ptr) {
        c_ptr = nullptr;
    });
#   endif
}

} // end of <anonymous> namespace

// ----------------------------------------------------------------------------

namespace {

#ifndef MACRO_BUILD_STL_ONLY

void setup_window_view(sf::RenderWindow & window, const ProgramOptions & opts) {
    using namespace erfin;
    sf::VideoMode vm;
    vm.width  = unsigned(ErfiGpu::SCREEN_WIDTH *opts.window_scale);
    vm.height = unsigned(ErfiGpu::SCREEN_HEIGHT*opts.window_scale);
    window.create(vm, WINDOW_TITLE);

    // RGBA pixels
    static constexpr const UInt32 black = 0xFF000000;
    static constexpr const UInt32 white = 0xFFFFFFFF;
    std::vector<UInt32> icon_pixel_data;
    icon_pixel_data.reserve(ICON_HEIGHT*ICON_WIDTH);
    for (auto c : std::string(MONOCHROMATIC_ICON_DATA)) {
        switch (c) {
        case '_': icon_pixel_data.push_back(black); break;
        case 'X': icon_pixel_data.push_back(white); break;
        default :
            throw std::runtime_error("Internal icon data is malformed.");
        }
    }
    window.setIcon(ICON_WIDTH, ICON_HEIGHT,
                   reinterpret_cast<const sf::Uint8 *>(icon_pixel_data.data()));

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

void map_screen_to_texture
    (const erfin::Console & console, std::vector<erfin::UInt32> & raw_pixels)
{
    auto scr_itr = console.current_screen().begin();
    for (auto & pix : raw_pixels) {
        assert(scr_itr != console.current_screen().end());
        pix = *scr_itr++ ? ~0u : 0;
    }
}

#endif // ifndef MACRO_BUILD_STL_ONLY

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
