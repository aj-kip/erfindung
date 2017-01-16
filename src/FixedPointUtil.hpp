/****************************************************************************

    File: FixedPointUtil.hpp
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

#ifndef MACRO_HEADER_GUARD_FIXED_POINT_UTIL_HPP
#define MACRO_HEADER_GUARD_FIXED_POINT_UTIL_HPP

#include "ErfiDefs.hpp"

//#include <boost/operators.hpp>

namespace erfin {

UInt32 reverse_bits(UInt32 num);

UInt32 fp_multiply(UInt32 a, UInt32 b);

UInt32 fp_inverse(UInt32 a);

UInt32 fp_divide(UInt32 a, UInt32 b);

UInt32 fp_remainder(UInt32 quot, UInt32 denom, UInt32 num);

UInt32 fp_compare(UInt32 a, UInt32 b);

UInt32 to_fixed_point(double fp);

double fixed_point_to_double(UInt32 fp);
#if 0
class FixedPoint : public boost::dividable<FixedPoint> {
public:

private:
    UInt32 m_rep;
};
#endif
} // end of erfin namespace

#endif
