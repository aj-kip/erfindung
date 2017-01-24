/****************************************************************************

    File: CommonDefinitions.hpp
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

#ifndef MACRO_HEADER_GUARD_ASSEMBLER_PRIVATE_COMMON_DEFINITIONS_HPP
#define MACRO_HEADER_GUARD_ASSEMBLER_PRIVATE_COMMON_DEFINITIONS_HPP

#include <vector>
#include <string>

namespace erfin {

using TokensContainer = std::vector<std::string>;
using TokensConstIterator = TokensContainer::const_iterator;

} // end of erfin namespace

#endif
