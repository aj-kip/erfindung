/****************************************************************************

    File: StringUtil.hpp
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

/* Microsoft is very seriously disappointing, my install of msvc 2015 seems to
 * be missing a lot string utility features.
 *
 *
 */

#ifndef MACRO_HEADER_GUARD_STRING_UTIL_HPP
#define MACRO_HEADER_GUARD_STRING_UTIL_HPP

#include <type_traits>
#include <stdexcept>
#include <iostream>

// <------------------------- Implementation Detail -------------------------->

template <typename IterType, typename RealType>
using EnableStrToNumIter = typename std::enable_if<
    std::is_base_of<std::forward_iterator_tag,
                    typename std::iterator_traits<IterType>::iterator_category>::value
    && std::is_arithmetic<RealType>::value,
    bool
>;

template <typename IterType, typename RealType>
using EnableStrToNumPtr = typename std::enable_if<
    std::is_pointer<IterType>::value &&
    std::is_arithmetic<RealType>::value,
    bool
>;

template <typename IterType, typename RealType>
using EnableStrToNum = typename std::enable_if <
    (std::is_pointer<IterType>::value ||
     std::is_base_of<std::forward_iterator_tag,
                     typename std::iterator_traits<IterType>::iterator_category>::value)
    && std::is_arithmetic<RealType>::value,
bool
>;

// <---------------------------- Public Interface ---------------------------->

template <typename IterType, typename RealType>
typename EnableStrToNum<IterType, RealType>::type
/* bool */ string_to_number
    (IterType start, IterType end, RealType & out,
     const RealType base_c = 10);

class OstreamFormatSaver {
public:
    OstreamFormatSaver(std::ostream & out):
        old_precision(out.precision()), old_flags(out.flags()), strm(&out)
    {}

    ~OstreamFormatSaver() {
        strm->precision(old_precision);
        strm->flags(old_flags);
    }

private:
    decltype(std::cout.precision()) old_precision;
    std::ios::fmtflags old_flags;
    std::ostream * strm;
};

// <------------------------- Implementation Detail -------------------------->

template <typename IterType, typename RealType>
typename EnableStrToNum<IterType, RealType>::type
/* bool */ string_to_number
    (IterType start, IterType end, RealType & out, const RealType base_c)
{
    using CharType = decltype(*start);
    if (base_c < RealType(2) || base_c > RealType(16)) {
        throw std::runtime_error("bool string_to_number(...): "
                                 "This function supports only bases 2 to 16.");
    }

    static constexpr bool IS_SIGNED  = std::is_signed<RealType>::value;
    static constexpr bool IS_INTEGER = !std::is_floating_point<RealType>::value;
    const bool IS_NEGATIVE = (*start) == CharType('-');

    // negative numbers cannot be parsed into an unsigned type
    if (!IS_SIGNED && IS_NEGATIVE)
        return false;

    if (IS_NEGATIVE) ++start;

    RealType working = RealType(0), multi = RealType(1);

    // the adder is a one digit number that corresponds to a character
    RealType adder = RealType(0);

    bool found_dot = false;

    // main digit reading loop, iterates characters in the selection in reverse
    do {
        switch (*--end) {
        case CharType('.'):
            if (found_dot) return false;
            found_dot = true;
            if (IS_INTEGER) {
                if (adder >= base_c/RealType(2))
                    working = RealType(1);
                else
                    working = RealType(0);
            } else {
                working /= multi;
            }
            adder = RealType(0);
            multi = RealType(1);
            continue;
        case CharType('0'): case CharType('1'): case CharType('2'):
        case CharType('3'): case CharType('4'): case CharType('5'):
        case CharType('6'): case CharType('7'): case CharType('8'):
        case CharType('9'):
            adder = RealType(*end - CharType('0'));
            break;
        case CharType('a'): case CharType('b'): case CharType('c'):
        case CharType('d'): case CharType('e'): case CharType('f'):
            adder = RealType(*end - 'a' + 10);
            break;
        case CharType('A'): case CharType('B'): case CharType('C'):
        case CharType('D'): case CharType('E'): case CharType('F'):
            adder = RealType(*end - 'A' + 10);
        default: return false;
        }
        if (adder >= base_c)
            return false;
        // detect overflow
        RealType temp = working + adder*multi;
        if (temp < working) {
            // attempt a recovery... (edge case, min int)
            const RealType MIN_INT = std::numeric_limits<RealType>::min();
            if (IS_NEGATIVE && -working - adder*multi == MIN_INT) {
                out = MIN_INT;
                return true;
            }
            return false;
        }
        multi *= RealType(base_c);
        working = temp;
    }
    while (end != start);

    // we've produced a positive integer, so make the adjustment if needed
    if (IS_NEGATIVE)
        working *= RealType(-1);

    // write to parameter
    out = working;
    return true;
}

#endif
