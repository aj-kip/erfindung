/****************************************************************************

    File: winmain.cpp
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

#include <Windows.h>

#include <iostream>

#include <cassert>
#include <cstdio>

#include "parse_program_options.hpp"

int main(int argc, char ** argv);

namespace {

void display_exception(const std::string &);

class CommandLineHandlerRaii {
public:
    CommandLineHandlerRaii();
    ~CommandLineHandlerRaii();
private:
    FILE * sin, * sout, * serr;
};

}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    CommandLineHandlerRaii clhr; (void)clhr;
    try {
        auto arguments = erfin::InitListArgs(GetCommandLine());
        return main(arguments.argc(), arguments.args());
    } catch (std::exception & exp) {
        display_exception(exp.what());
    } catch (...) {
        display_exception("Unexpected exception occured (bad type).");
    }
    return ~0;
}

namespace {

CommandLineHandlerRaii::CommandLineHandlerRaii():
    sin(nullptr), sout(nullptr), serr(nullptr)
{
    if (GetConsoleWindow()) return;
    AllocConsole();
    freopen_s(&sin , "CONOUT$", "r", stdin );
    freopen_s(&sout, "CONOUT$", "w", stdout);
    freopen_s(&serr, "CONOUT$", "w", stderr);
    assert(sout);
    assert(serr);
    assert(sin );
}
CommandLineHandlerRaii::~CommandLineHandlerRaii() {
    if (!sin) {
        assert(!sout && !serr);
        return;
    }
    fclose(sin );
    fclose(sout);
    fclose(serr);
}

void display_exception(const std::string & error_text) {
    static constexpr const char * const PREFACE =
        "An exception occured in an especially unusual place of the "
        "program. This is indication that this problem needs to be fixed on "
        "a source-code level.\nDetails:\n\n";
    (void)MessageBox(nullptr, (PREFACE + error_text).c_str(),
                     "Fatal Runtime Error", MB_OK           );
}

} // end of <anonymous> namespace