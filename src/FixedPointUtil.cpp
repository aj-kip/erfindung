/****************************************************************************

    File: FixedPointUtil.cpp
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

#include "FixedPointUtil.hpp"

#include <iostream>

#include <cassert>
#include <cmath>

namespace erfin {

UInt32 reverse_bits(UInt32 num) {

    // this implementation uses a 256 byte table, which may not work well
    // with caching if I'm not calling this often

    // we'll have to measure it later vs. a linear implementation
    static const uint8_t BIT_REVERSE_TABLE[256] = {
#       if defined(R2) || defined(R4) || defined(R6)
#           error "Need R2, R4, R6 for macro values"
#       endif
#       define R2(n)    n,     n + 2*64,     n + 1*64,     n + 3*64
#       define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#       define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
        R6(0), R6(2), R6(1), R6(3)
#       undef R6
#       undef R4
#       undef R2
    };

    UInt32 c; // c will get num reversed

    const uint8_t * p = reinterpret_cast<uint8_t *>(&num);
    uint8_t * q = reinterpret_cast<uint8_t *>(&c);
    q[3] = BIT_REVERSE_TABLE[p[0]];
    q[2] = BIT_REVERSE_TABLE[p[1]];
    q[1] = BIT_REVERSE_TABLE[p[2]];
    q[0] = BIT_REVERSE_TABLE[p[3]];
    return c;
}

UInt32 fp_multiply(UInt32 a, UInt32 b) {
    // Yes! finally found an article that's fully details my problem:
    // http://www.eetimes.com/author.asp?doc_id=1287491
    UInt32 sign = (0x80000000 & a) ^ (0x80000000 & b);
    UInt64 a_bg = UInt64(0x7FFFFFFF & a);
    UInt64 b_bg = UInt64(0x7FFFFFFF & b);
    UInt64 large = (a_bg*b_bg + 0x8000) >> 16;
    return sign | (large & 0xFFFFFFFF);
}

UInt32 fp_inverse(UInt32 a) {
    // could reverse bits and multiply given
    // fp = -1( SUM( b_1^{15} + b_2^{14} + ... + b_{15}^{0} )
    UInt32 sign = 0x80000000 & a;
    a = reverse_bits(a) >> 1;
    return sign | a;
}

UInt32 fp_divide(UInt32 a, UInt32 b) {
    // many thanks to Shawn Stevenson!
    // https://sestevenson.wordpress.com/fixed-point-division-of-two-q15-numbers/
    return fp_multiply(a, fp_inverse(b));
}

UInt32 fp_remainder(UInt32 quot, UInt32 denom, UInt32 num) {
    // n - q*d = r -> n = r + q*d -> n/d = r/d + q
    // reduce quot to int part only
    return num - fp_multiply(quot & 0xFFFF0000, denom);
}

UInt32 fp_compare(UInt32 a, UInt32 b) {
    using namespace erfin::enum_types;
    auto is_neg = [] (UInt32 a) -> bool { return (a & 0xF0000000) != 0; };
    bool neg;
    if ((neg = is_neg(a)) == is_neg(b)) {
        if ((a & 0xFFFFFF00) == (b & 0xFFFFFF00))
            return COMP_EQUAL_MASK;
        UInt32 rv;
        if (a > b) {
            rv = (neg) ? COMP_LESS_THAN_MASK : COMP_GREATER_THAN_MASK;
        } else { // a < b
            rv = (neg) ? COMP_GREATER_THAN_MASK : COMP_LESS_THAN_MASK;
        }
        return rv | COMP_NOT_EQUAL_MASK;
    } else {
        if (is_neg(a))
            return COMP_LESS_THAN_MASK | COMP_NOT_EQUAL_MASK; // a < b
        if (is_neg(b))
            return COMP_GREATER_THAN_MASK | COMP_NOT_EQUAL_MASK; // a > b
    }
    std::cerr << "Wut?" << std::endl;
    std::terminate();
}

UInt32 to_fixed_point(double fp) {
    bool is_neg = fp < 0.0;
    fp = mag(fp);
    UInt32 high = static_cast<UInt32>(std::floor(fp)     );
    UInt32 low  = static_cast<UInt32>(std::round((fp - std::floor(fp))*65535.0));
    assert(low <= 65535);
    UInt32 rv = ((high & 0x7FFF) << 16) | (low);
    return is_neg ? 0x80000000 | rv : rv;
}

double fixed_point_to_double(UInt32 fp) {
    bool is_neg = (fp & 0x80000000) != 0;
    double sign = is_neg ? -1.0 : 1.0;
    UInt32 magi = is_neg ? (fp ^ 0x80000000) : fp;
    return sign*double(magi) / 65536.0;
}

} // end of erfin namespace
