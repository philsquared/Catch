
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#define CATCH_CONFIG_ENABLE_COMPLEX_STRINGMAKER
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <functional>
#include <limits>
#include <sstream>

namespace {
    struct ScopedPrecision {
        int& precision;
        int original;

        ScopedPrecision( int& target, int replacement ):
            precision( target ), original( target ) {
            precision = replacement;
        }
        ~ScopedPrecision() { precision = original; }
        ScopedPrecision( ScopedPrecision const& ) = delete;
        ScopedPrecision& operator=( ScopedPrecision const& ) = delete;
    };

    struct StreamedNumber {
        double value;
    };

    std::ostream& operator<<( std::ostream& stream, StreamedNumber value ) {
        return stream << value.value;
    }
} // namespace

TEMPLATE_TEST_CASE( "Complex default precision preserves both components",
                    "[toString][complex][approvals]",
                    float,
                    double,
                    long double ) {
    using Complex = std::complex<TestType>;
    using Maker = Catch::StringMaker<Complex>;
    CHECK( Maker::precision == std::numeric_limits<TestType>::max_digits10 );

    const Complex value{ std::nextafter( TestType( 1 ), TestType( 2 ) ),
                         std::nextafter( TestType( -1 ), TestType( -2 ) ) };
    std::istringstream stream( Catch::Detail::stringify( value ) );
    Complex parsed;
    stream >> parsed;
    REQUIRE( stream );
    // Round-trip preservation requires exact comparison, including long double.
    CHECK( std::equal_to<TestType>{}( parsed.real(), value.real() ) );
    CHECK( std::equal_to<TestType>{}( parsed.imag(), value.imag() ) );
    CHECK( stream.peek() == std::char_traits<char>::eof() );
}

TEMPLATE_TEST_CASE( "Complex precision can be changed at runtime",
                    "[toString][complex][approvals]",
                    float,
                    double,
                    long double ) {
    using Complex = std::complex<TestType>;
    using Maker = Catch::StringMaker<Complex>;
    ScopedPrecision restore( Maker::precision, 4 );
    const Complex value{ TestType( 1.234567L ), TestType( -9.876543L ) };
    CHECK( Catch::Detail::stringify( value ) == "(1.235,-9.877)" );
    Maker::precision = 2;
    CHECK( Catch::Detail::stringify( value ) == "(1.2,-9.9)" );
}

TEMPLATE_TEST_CASE( "Complex special values retain stream formatting",
                    "[toString][complex][approvals]",
                    float,
                    double,
                    long double ) {
    using Complex = std::complex<TestType>;
    using Maker = Catch::StringMaker<Complex>;
    ScopedPrecision restore( Maker::precision, 12 );
    const auto infinity = std::numeric_limits<TestType>::infinity();
    const auto nan = std::numeric_limits<TestType>::quiet_NaN();
    const Complex values[]{ { TestType( 0 ), -TestType( 0 ) },
                            { infinity, -infinity },
                            { nan, TestType( 1 ) } };
    for ( auto const& value : values ) {
        std::ostringstream expected;
        expected.precision( Maker::precision );
        expected << value;
        CHECK( Catch::Detail::stringify( value ) == expected.str() );
    }
}

TEST_CASE( "Complex precision settings are independent",
           "[toString][complex][approvals]" ) {
    ScopedPrecision floatPrecision(
        Catch::StringMaker<std::complex<float>>::precision, 2 );
    ScopedPrecision doublePrecision(
        Catch::StringMaker<std::complex<double>>::precision, 4 );
    ScopedPrecision longDoublePrecision(
        Catch::StringMaker<std::complex<long double>>::precision, 6 );
    ScopedPrecision scalarFloatPrecision( Catch::StringMaker<float>::precision,
                                          7 );
    ScopedPrecision scalarDoublePrecision(
        Catch::StringMaker<double>::precision, 8 );

    CHECK( Catch::Detail::stringify(
               std::complex<float>{ 1.234567f, -9.876543f } ) == "(1.2,-9.9)" );
    CHECK( Catch::Detail::stringify( std::complex<double>{
               1.234567, -9.876543 } ) == "(1.235,-9.877)" );
    CHECK( Catch::Detail::stringify( std::complex<long double>{
               1.234567L, -9.876543L } ) == "(1.23457,-9.87654)" );
    CHECK( Catch::StringMaker<float>::precision == 7 );
    CHECK( Catch::StringMaker<double>::precision == 8 );
}

TEST_CASE( "Complex precision does not leak into reused streams",
           "[toString][complex][approvals]" ) {
    ScopedPrecision restore(
        Catch::StringMaker<std::complex<double>>::precision, 2 );
    CHECK( Catch::Detail::stringify(
               std::complex<double>{ 1.234567, -9.876543 } ) == "(1.2,-9.9)" );
    CHECK( Catch::Detail::stringify( StreamedNumber{ 1.23456789 } ) ==
           "1.23457" );
}
