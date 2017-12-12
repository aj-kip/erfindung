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
#include <sstream>
#include <stdexcept>

#include <cassert>
#include <cmath>

namespace {

using Error = std::runtime_error;

const constexpr erfin::UInt32 SIGN_BIT_MASK = 0x80000000;

template <typename T>
typename std::enable_if<std::is_signed<T>::value, T>::type
    mag(T t) { return t < T(0) ? -t : t; }

void run_numeric_encoding_tests();

} // end of <anonymous> namespace

namespace erfin {

void run_fixed_point_tests() {
    run_numeric_encoding_tests();
}

UInt32 fp_multiply(UInt32 a, UInt32 b) {
    // Yes! finally found an article that's fully details my problem:
    // http://www.eetimes.com/author.asp?doc_id=1287491
    UInt32 sign = (SIGN_BIT_MASK & a) ^ (SIGN_BIT_MASK & b);
    UInt64 a_bg = UInt64(~SIGN_BIT_MASK & a);
    UInt64 b_bg = UInt64(~SIGN_BIT_MASK & b);
    UInt64 large = (a_bg*b_bg + 0x8000) >> 16; // mul, bias, shift
    return sign | UInt32(large & UInt64(~SIGN_BIT_MASK));
}

UInt32 fp_inverse(UInt32 a) {
    // could reverse bits and multiply given
    // fp = -1( \Sigma( b_1^{15} + b_2^{14} + ... + b_{31}^{-16} ) )
    // the pivot of this reversal is the ones position
    // that is (in binary) 0.1 -> 10.0
    // reverse_bits(0.1) = 1.0
    // shift_up(reverse_bits(0.1)) = 10.0
    //UInt32 sign = 0x80000000 & a;
    //a =           0x7FFFFFFF & reverse_bits(a);
    //return sign | a;
    return fp_divide(0x00010000, a);
}

UInt32 fp_divide(UInt32 a, UInt32 b) {
    UInt32 sign = (SIGN_BIT_MASK & a) ^ (SIGN_BIT_MASK & b);
    UInt64 a_bg = UInt64(~SIGN_BIT_MASK & a);
    UInt64 b_bg = UInt64(~SIGN_BIT_MASK & b);
    UInt64 temp = ((a_bg << 16) - 0x8000) / b_bg; // shift, bias, div
    return sign | (UInt32(temp) & ~SIGN_BIT_MASK);
}

UInt32 fp_remainder(UInt32 quot, UInt32 denom, UInt32 num) {
    // n - q*d = r -> n = r + q*d -> n/d = r/d + q
    // reduce quot to int part only
    return num - fp_multiply(quot & 0xFFFF0000, denom);
}

UInt32 fp_compare(UInt32 a, UInt32 b) {
    auto is_neg = [] (UInt32 a) -> bool { return (a & SIGN_BIT_MASK) != 0; };
    bool neg;
    if ((neg = is_neg(a)) == is_neg(b)) {
        if ((a & 0x7FFFFF00) == (b & 0x7FFFFF00))
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
    std::cerr << "fp_compare: \"Impossible\" branch taken." << std::endl;
    std::terminate();
}

UInt32 to_fixed_point(double fp) {
    bool is_neg = fp < 0.0;
    fp = mag(fp);
    UInt32 high = static_cast<UInt32>(std::floor(fp)     );
    UInt32 low  = static_cast<UInt32>(std::round((fp - std::floor(fp))*65535.0));
    assert(low <= 65535);
    UInt32 rv = ((high & 0x7FFF) << 16) | (low);
    return is_neg ? SIGN_BIT_MASK | rv : rv;
}

double fixed_point_to_double(UInt32 fp) {
    bool is_neg = (fp & SIGN_BIT_MASK) != 0;
    double sign = is_neg ? -1.0 : 1.0;
    UInt32 magi = is_neg ? (fp ^ SIGN_BIT_MASK) : fp;
    return sign*double(magi) / 65536.0;
}

} // end of erfin namespace

namespace {

void test_fp_multiply(double a, double b);

void test_fp_divide(double a, double b);

void test_fixed_point(double value);


void run_numeric_encoding_tests() {
    test_fp_multiply(-1.0, 1.0);
    test_fixed_point(  2.0);
    test_fixed_point( -1.0);
    test_fixed_point( 10.0);
    test_fixed_point(  0.1);
    test_fixed_point(-10.0);
    test_fixed_point( -0.1);

    test_fixed_point( 32767.0);
    test_fixed_point(-32767.0);

    // minumum value
    test_fixed_point( 0.00001525878);
    test_fixed_point(-0.00001525878);
    // maximum value
    /* (1/2)+(1/4)+(1/8)+(1/16)+(1/32)+(1/64)+(1/128)+(1/256)
       +(1/512)+(1/1024)+(1/2048)+(1/4096)
       +(1/8192)+(1/16384)+(1/32768)+(1/65536) */
    test_fixed_point( 32767.9999923706);
    test_fixed_point(-32767.9999923706);

    test_fp_multiply(2.0, 2.0);
    test_fp_multiply(-1.0, 1.0);
    test_fp_multiply(10.0, 10.0);
    test_fp_multiply(100.0, 100.0);
    test_fp_multiply(0.5, 0.5);
    test_fp_multiply(1.1, 1.1);
    test_fp_multiply(200.0, 0.015625);

    test_fp_divide( 2.0, 1.0);
    test_fp_divide( 2.0, 4.0);
    test_fp_divide(10.0, 3.0);
    test_fp_divide( 2.0, 0.5);
    test_fp_divide( 0.5, 2.0);
    test_fp_divide( 1.1, 1.1);
}

// ----------------------------------------------------------------------------

double mul(double a, double b) { return a*b; }
double div(double a, double b) { return a/b; }

using UInt32 = erfin::UInt32;

template <UInt32(*FixedPtFunc)(UInt32, UInt32), double(*DoubleFunc)(double, double)>
void test_fp_operation(double a, double b, char op_char);

void test_fp_multiply(double a, double b) {
    test_fp_operation<erfin::fp_multiply, mul>(a, b, 'x');
}

void test_fp_divide(double a, double b) {
    test_fp_operation<erfin::fp_divide, div>(a, b, '/');
}

void test_fixed_point(double value) {
    using namespace erfin;
    //OstreamFormatSaver cfs(std::cout); (void)cfs;
    std::stringstream ssout;
    UInt32 fp = to_fixed_point(value);
    double val_out = fixed_point_to_double(fp);
    double diff = val_out - value;
    // use an error about 1/(2^16), but "fatten" it up for error
    if (mag(diff) < 0.00002) {
        return;
    }
    // not equal!
    ssout <<
        "Fixed point test failed!\n"
        "Starting         : " << value << "\n"
        "Fixed Point value: " << std::hex << std::uppercase << fp << "\n"
        "End value        : " << std::dec << std::nouppercase << val_out <<
         std::endl;
    throw Error(ssout.str().c_str());
}

// ----------------------------------------------------------------------------

template <UInt32(*FixedPtFunc)(UInt32, UInt32), double(*DoubleFunc)(double, double)>
void test_fp_operation(double a, double b, char) {
    using namespace erfin;
    UInt32 fp_a = to_fixed_point(a);
    UInt32 fp_b = to_fixed_point(b);
    UInt32 res = FixedPtFunc(fp_a, fp_b);
    double d = fixed_point_to_double(res);

    const double MAX_ERROR = 0.00002;
    if (mag(d - DoubleFunc(a, b)) > MAX_ERROR) {
        throw std::runtime_error(
            "Stopping test (failed), " + std::to_string(d) + " != " +
            std::to_string(DoubleFunc(a, b)));
    }
}

} // end of <anonymous> namespace
