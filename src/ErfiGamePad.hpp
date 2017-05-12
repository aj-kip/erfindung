/****************************************************************************

    File: ErfiGamePad.hpp
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

#ifndef MACRO_HEADER_GUARD_ERFINDUNG_GAME_PAD_HPP
#define MACRO_HEADER_GUARD_ERFINDUNG_GAME_PAD_HPP

#include <bitset>

#include "ErfiDefs.hpp"

namespace erfin {

class GamePad {
public:
    enum Event {
        PRESSED,
        RELEASE
    };

    enum Button {
        UP,
        DOWN,
        LEFT,
        RIGHT,
        A,
        B,
        START,
        BUTTON_COUNT
    };

    void update(Button b, Event e) {
        if (b == BUTTON_COUNT) return;
        m_state.set(b, e == PRESSED ? 1 : 0);
    }

    UInt32 decode() const { return UInt32(m_state.to_ulong()); }

private:

    static_assert(BUTTON_COUNT <= 32, "Controller can have at most 32 buttons.");
    std::bitset<BUTTON_COUNT> m_state;
};

} // end of erfin namespace

#endif
