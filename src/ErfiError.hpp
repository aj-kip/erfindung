/****************************************************************************

    File: ErfiError.hpp
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

#ifndef MACRO_HEADER_GUARD_ERFINDUNG_ERFIERROR_HPP
#define MACRO_HEADER_GUARD_ERFINDUNG_ERFIERROR_HPP

#include <stdexcept>

namespace erfin {

class ErfiError : public std::exception {
public:
    ErfiError(std::size_t program_location_, const std::string && msg);
    ErfiError(std::size_t program_location_, const char * msg);

    const char * what() const noexcept override { return m_message.c_str(); }

    std::size_t program_location() const noexcept { return m_program_location; }

private:
    std::size_t m_program_location;
    std::string m_message;
};

inline ErfiError::ErfiError
    (std::size_t program_location_, const std::string && msg):
    m_program_location(program_location_),
    m_message(std::move(msg))
{}

inline ErfiError::ErfiError(std::size_t program_location_, const char * msg):
    m_program_location(program_location_),
    m_message(msg)
{}

} // end of erfin namespace

#endif
