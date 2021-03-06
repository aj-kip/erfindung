/****************************************************************************

    File: LineParsingHelpers.cpp
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

#include "LineParsingHelpers.hpp"
#include "TextProcessState.hpp"
#include "../StringUtil.hpp"
#include "GetLineProcessingFunction.hpp"

#include <iostream>

#include <cassert>

namespace erfin {

const char * extended_param_form_to_string(ExtendedParamForm xpf) {
    switch (xpf) {
    case XPF_4R      : return "4 registers"                         ;
    case XPF_3R      : return "3 registers"                         ;
    case XPF_2R_INT  : return "2 registers and an integer"          ;
    case XPF_2R_FP   : return "2 registers and a fixed point number";
    case XPF_2R_LABEL: return "2 registers and a label"             ;
    case XPF_2R      : return "2 registers"                         ;
    case XPF_1R_INT  : return "a register and an integer"           ;
    case XPF_1R_FP   : return "a register and a fixed point number" ;
    case XPF_1R_LABEL: return "a register and a label"              ;
    case XPF_1R      : return "a register"                          ;
    case XPF_INT     : return "an integer"                          ;
    case XPF_FP      : return "a fixed point number"                ;
    case XPF_LABEL   : return "a label"                             ;
    case XPF_INVALID : return "an invalid parameter form"           ;
    default: std::cerr << "This should be unreachable!" << std::endl;
             std::terminate();
    }
}

NumericParseInfo parse_number(const std::string & str) {
    NumericParseInfo rv;
    // first try to find a prefex
    auto str_has_at = [&str] (char c, std::size_t idx) -> bool {
        if (idx >= str.size()) return false;
        return tolower(str[idx]) == c;
    };
    bool neg = str_has_at('-', 0);
    bool zero_prefixed = str_has_at('0', neg ? 1 : 0);
    int  base;
    auto beg = str.begin();
    if (zero_prefixed && str_has_at('x', neg ? 2 : 1)) {
        beg += (neg ? 3 : 2);
        base = 16;
    } else if (zero_prefixed && str_has_at('b', neg ? 2 : 1)) {
        beg += (neg ? 3 : 2);
        base = 2;
    } else {
        // reg decimal
        beg += (neg ? 1 : 0);
        base = 10;
    }

    bool has_dot = false;
    for (char c : str) has_dot = (c == '.') ? true : has_dot; // n.-
    if (has_dot) {
        if (string_to_number(beg, str.end(), rv.floating_point, double(base))) {
            if (neg) rv.floating_point *= -1.0;
            rv.type = DECIMAL;
            return rv;
        }
    } else {
        if (string_to_number(beg, str.end(), rv.integer, base)) {
            if (neg) rv.integer *= -1;
            rv.type = INTEGER;
            return rv;
        }
    }
    rv.type = NOT_NUMERIC;
    return rv;
}

erfin::Reg string_to_register(const std::string & str) {
    using namespace erfin;
    static_assert(sizeof(char)*2 == sizeof(UInt16)               ,
                  "string_to_register Implementation must change");
    auto first2eq = [](const char * lhs, const char * rhs) {
        return *reinterpret_cast<const UInt16 *>(lhs) ==
               *reinterpret_cast<const UInt16 *>(rhs);
    };

    switch (str.size()) {
    case 1: switch (str[0]) {
        case 'x': return Reg::X    ;
        case 'y': return Reg::Y    ;
        case 'z': return Reg::Z    ;
        case 'a': return Reg::A    ;
        case 'b': return Reg::B    ;
        case 'c': return Reg::C    ;
        default : return Reg::COUNT;
        }
    case 2: if (first2eq(str.c_str(), "pc")) return Reg::PC;
            if (first2eq(str.c_str(), "sp")) return Reg::SP;
            return Reg::COUNT;
    default: return Reg::COUNT;
    }
}

Reg string_to_register_or_throw
    (TextProcessState & state, const std::string & reg_str)
{
    auto rv = string_to_register(reg_str);
    if (rv == Reg::COUNT)
        throw state.make_error(": \"" + reg_str + "\" is not a valid register.");
    return rv;
}

TokensConstIterator get_eol(TokensConstIterator beg, TokensConstIterator end) {
    while (*beg != "\n") {
        ++beg;
        if (beg == end) break;
    }
    return beg;
}

ExtendedParamForm get_lines_param_form
    (TokensConstIterator beg, TokensConstIterator end, NumericParseInfo * npi)
{
    using namespace erfin;

    // this had better not be the begging of the line
    assert(!get_line_processing_function(erfin::Assembler::NO_ASSUMPTIONS, *beg));

    // naturally anyone can read this (oh god it's shit)
    const int arg_count = int(end - beg);
    NumericParseInfo local_npi;
    if (!npi) npi = &local_npi;
    switch (arg_count) {
    case 4:
        for (int i = 0; i != 4; ++i)
            if (string_to_register(*(beg + i)) == Reg::COUNT)
                return XPF_INVALID;
        return XPF_4R;
    case 3: case 2:
        for (int i = 0; i != arg_count - 1; ++i)
            if (string_to_register(*(beg + i)) == Reg::COUNT)
                return XPF_INVALID;
        if (string_to_register(*(beg + arg_count - 1)) != Reg::COUNT) {
            return arg_count == 2 ? XPF_2R : XPF_3R;
        }
        *npi = parse_number(*(beg + arg_count - 1));
        switch (npi->type) {
        case INTEGER: return arg_count == 2 ? XPF_1R_INT : XPF_2R_INT;
        case DECIMAL: return arg_count == 2 ? XPF_1R_FP  : XPF_2R_FP ;
        default: return arg_count == 2 ? XPF_1R_LABEL : XPF_2R_LABEL ;
        }
    case 1: // reg only
        if (string_to_register(*beg) != Reg::COUNT) return XPF_1R;
        *npi = parse_number(*beg);
        switch (npi->type) {
        case INTEGER: return XPF_INT;
        case DECIMAL: return XPF_FP;
        default     : return XPF_LABEL;
        }
    default: return XPF_INVALID;
    }
}

} // end of erfin namespace
