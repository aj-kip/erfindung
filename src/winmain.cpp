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

int main();

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
#   ifdef MACRO_DEBUG
    AllocConsole();
    FILE * so = nullptr;
    FILE * se = nullptr;
    freopen_s(&so, "CONOUT$", "w", stdout);
    freopen_s(&se, "CONOUT$", "w", stderr);
    assert(so);
    assert(se);
#   endif
	int rv = main();
#   ifdef MACRO_DEBUG
    fclose(so);
    fclose(se);

    std::cin.get();
#   endif
	return rv;
}