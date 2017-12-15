/****************************************************************************

    File: LineParsingHelpers.hpp
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

#ifndef MACRO_HEADER_GUARD_LINE_PARSING_HELPERS_HPP
#define MACRO_HEADER_GUARD_LINE_PARSING_HELPERS_HPP

#include "CommonDefinitions.hpp"
#include "../Assembler.hpp"

#include <string>

namespace erfin {

class TextProcessState;
class Assembler;

enum TokenClassification {
    REGISTER,
    IMMEDIATE_INTEGER,
    IMMEDIATE_FIXED_POINT,
    INVALID_CLASS
};

enum NumericClassification {
    INTEGER,
    DECIMAL,
    NOT_NUMERIC
};

enum ExtendedParamForm {
    XPF_4R,
    XPF_3R,
    XPF_2R_INT,
    XPF_2R_FP,
    XPF_2R_LABEL,
    XPF_2R,
    XPF_1R_INT,
    XPF_1R_FP,
    XPF_1R_LABEL,
    XPF_1R,
    XPF_INT,
    XPF_FP,
    XPF_LABEL,
    XPF_INVALID
};

struct NumericParseInfo {
    NumericClassification type;
    union {
        int    integer;
        double floating_point;
    };
};

const char * extended_param_form_to_string(ExtendedParamForm xpf);

NumericParseInfo parse_number(const std::string & str);

TokensConstIterator get_eol(TokensConstIterator beg, TokensConstIterator end);

ExtendedParamForm get_lines_param_form
    (TokensConstIterator beg, TokensConstIterator end,
     NumericParseInfo * npi = nullptr);

Reg string_to_register(const std::string & str);

Reg string_to_register_or_throw
    (TextProcessState & state, const std::string & reg_str);

template <typename T, typename Head, typename ... Types>
bool equal_to_any(T primary, Head head, Types ... args);

template <typename T, typename ... Types>
bool equal_to_any(T, Types...) { return false; }

template <typename T, typename Head, typename ... Types>
bool equal_to_any(T primary, Head head, Types ... args)
    { return primary == head || equal_to_any(primary, args...); }

} // end of erfin namespace

#endif
