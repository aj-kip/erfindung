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
#include "FixedPointUtil.hpp"

#include <cstring>
#include <cassert>

namespace {

void test_string_processing();
void test_string_to_number();

}

void run_tests(const erfin::ProgramOptions &, const erfin::ProgramData &) {
    using namespace erfin;

    OstreamFormatSaver osfs(std::cout); (void)osfs;

    test_string_to_number();
    run_encode_decode_tests();
    run_fixed_point_tests();
    Assembler::run_tests();
    ErfiCpu::run_tests();
    test_string_processing();

    std::cout << "All Internal Tests passed sucessfully." << std::endl;
}

namespace {

// ----------------------------------------------------------------------------


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



std::runtime_error make_failed_string_to_number() {
    return std::runtime_error("test_string_to_number: tests fail.");
}

template <typename T>
void test_on(const char * const str, T num, int base) {
    auto mag = [](T x) { return x > T(0) ? x : -x; };
    T res = -1;
    if (!string_to_number(str, str + ::strlen(str), res, T(base)))
        throw make_failed_string_to_number();
    double res_diff = double(mag(num - res));
    if (res_diff >= 0.0005)
        throw make_failed_string_to_number();
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
    if (!string_to_number_(max_dec_str, res))
        throw make_failed_string_to_number();
    if (res != max_c)
        throw make_failed_string_to_number();
    if (!string_to_number_(min_dec_str, res))
        throw make_failed_string_to_number();
    if (res != min_c)
        throw make_failed_string_to_number();
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
    if (string_to_number_("2147483648", res))
        throw make_failed_string_to_number();
    // regular overflow
    if (string_to_number_("2147483649", res))
        throw make_failed_string_to_number();
    if (string_to_number_("-8-12" , res))
        throw make_failed_string_to_number();
    if (string_to_number_("1.21.2", res))
        throw make_failed_string_to_number();
    }
    // test on unsigneds
    {
    const auto max = std::numeric_limits<std::uint32_t>::max();
    const char * const max_str = "4294967295";
    std::uint32_t res = 0;
    if (!string_to_number(max_str, max_str + ::strlen(max_str), res, 10u))
        throw make_failed_string_to_number();
    if (max != res)
        throw make_failed_string_to_number();
    }
}

} // end of <anonymous> namespace
