/****************************************************************************

    File: tests.cpp
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

#include "tests.hpp"

#include "Assembler.hpp"
#include "ErfiCpu.hpp"

#include "StringUtil.hpp"

#include <cstring>
#include <cassert>

namespace {

void run_numeric_encoding_tests();
void test_string_processing();
void test_string_to_number();

}

void run_tests() {
    using namespace erfin;

    OstreamFormatSaver osfs(std::cout); (void)osfs;

    test_string_to_number();
    run_encode_decode_tests();
    run_numeric_encoding_tests();
    Assembler::run_tests();
    ErfiCpu::run_tests();
    test_string_processing();

    std::cout << "All Internal Tests passed sucessfully." << std::endl;
}

namespace {

// ----------------------------------------------------------------------------

void test_fp_multiply(double a, double b);

void test_fp_divide(double a, double b);

void test_fixed_point(double value);

void test_string_processing() {
    const char * const input_text =
        "assume integer\n"
        "jump b\n"
        "jump 0\n\r"
        "cmp  a b\n\n\n"
        "jump b\n"
        "load b a 10 # comment 3\n"
        "\n"
        "save a b   0\n"
        "save a b -10\n"
        "load a b  -1\r"
        "load a b\n"
        "\n\n";
    erfin::Assembler asmr;
    asmr.assemble_from_string(input_text);
}

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
#   if 0
    std::cout << erfin::fixed_point_to_double(erfin::fp_inverse(erfin::to_fixed_point(0.5))) << std::endl;
    std::cout << erfin::fixed_point_to_double(erfin::fp_inverse(erfin::to_fixed_point(0.25))) << std::endl;
    std::cout << erfin::fixed_point_to_double(erfin::fp_inverse(erfin::to_fixed_point(0.3333333))) << std::endl;
    std::cout << erfin::fixed_point_to_double(erfin::fp_inverse(erfin::to_fixed_point(0.1))) << std::endl;
#   endif
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

template <typename T>
void test_on(const char * const str, T num, int base) {
    auto mag = [](T x) { return x > T(0) ? x : -x; };
    T res = -1;
    assert(string_to_number(str, str + ::strlen(str), res, T(base)));
    double res_diff = double(mag(num - res));
    assert(res_diff < 0.0005);
}

void test_string_to_number() {
    // integers
    // "normal" inputs test
    using Int32 = std::int32_t;
    auto string_to_number_ = [](const char * const str, Int32 & out) -> bool
        { return string_to_number(str, str + ::strlen(str), out, 10); };
    {
    test_on<int>("0", 0, 10);
    test_on<int>("-1586", -1586, 10);
    test_on<int>("1234", 1234, 10);
    }
    // min-max value edge cases
    // these types must strictly be 32bits
    {
    const Int32 max_c = std::numeric_limits<Int32>::max();
    const Int32 min_c = std::numeric_limits<Int32>::min();
    const char * const max_dec_str =  "2147483647";
    const char * const min_dec_str = "-2147483648";
    Int32 res = -1;
    assert(string_to_number_(max_dec_str, res));
    assert(res == max_c);
    assert(string_to_number_(min_dec_str, res));
    assert(res == min_c);
    }
    // hexidecimal
    {
    test_on<int>("92AB" ,  0x92AB, 16);
    test_on<int>("-D98E", -0xD98E, 16);
    }
    // binary
    {
    test_on<int>("1001110",  78, 2);
    test_on<int>("-111011", -59, 2);
    }
    // octal
    {
    test_on<int>( "273",  187, 8);
    test_on<int>("-713", -459, 8);
    }
    // base 13
    {
    test_on<int>( "B86", 1969, 13);
    test_on<int>("-13A", -218, 13);
    }
    // floating-points
    {
    test_on<double>("132.987" , 132.987 , 10);
    test_on<double>("-762.168", -762.168, 10);
    test_on<double>("A.A",10.0 + 10.0/12.0, 12);
    test_on<double>(".1", .1, 10);
    test_on<double>("1.", 1., 10);
    }
    // rounding
    {
    test_on<int>("-573.5", -574, 10);
    test_on<int>("-573.4", -573, 10);
    test_on<int>("342.6" ,  343, 10);
    test_on<int>("342.2" ,  342, 10);
    }
    // negative tests, cases where functionality is supposed to "fail"
    {
    std::int32_t res = -1;
    // edge failure case
    assert(!string_to_number_("2147483648", res));
    // regular overflow
    assert(!string_to_number_("2147483649", res));
    assert(!string_to_number_("-8-12" , res));
    assert(!string_to_number_("1.21.2", res));
    }
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
    OstreamFormatSaver cfs(std::cout); (void)cfs;
    UInt32 fp = to_fixed_point(value);
    double val_out = fixed_point_to_double(fp);
    double diff = val_out - value;
    // use an error about 1/(2^16), but "fatten" it up for error
    if (mag(diff) < 0.00002) {
#       if 0
        std::cout <<
            "For: " << value << "\n"
            "Fixed Point value: " << std::hex << std::uppercase << fp << "\n"
            "End value        : " << std::dec << std::nouppercase << val_out << "\n" <<
            std::endl;
#       endif
        return;
    }
    // not equal!
    std::cout <<
        "Fixed point test failed!\n"
        "Starting         : " << value << "\n"
        "Fixed Point value: " << std::hex << std::uppercase << fp << "\n"
        "End value        : " << std::dec << std::nouppercase << val_out <<
         std::endl;
}

// ----------------------------------------------------------------------------

template <UInt32(*FixedPtFunc)(UInt32, UInt32), double(*DoubleFunc)(double, double)>
void test_fp_operation(double a, double b, char) {
    using namespace erfin;
    OstreamFormatSaver cfs(std::cout); (void)cfs;
    UInt32 fp_a = to_fixed_point(a);
    UInt32 fp_b = to_fixed_point(b);
    UInt32 res = FixedPtFunc(fp_a, fp_b);
    double d = fixed_point_to_double(res);

    const double MAX_ERROR = 0.00002;
#   if 0
    std::cout << "For: " << a << op_char << b << " = " << DoubleFunc(a, b) << std::endl;
    std::cout <<
        "a   value: " << std::hex << std::uppercase << fp_a << "\n"
        "b   value: " << std::hex << std::uppercase << fp_b << "\n"
        "res value: " << std::hex << std::uppercase << res  << std::endl;
#   endif
    if (mag(d - DoubleFunc(a, b)) > MAX_ERROR) {
        throw std::runtime_error(
            "Stopping test (failed), " + std::to_string(d) + " != " +
            std::to_string(DoubleFunc(a, b)));
    }
}

} // end of <anonymous> namespace
