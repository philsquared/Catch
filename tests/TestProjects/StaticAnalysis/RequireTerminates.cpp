
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#include <catch2/catch_test_macros.hpp>

#include <optional>

// Deliberately not defined, so that the analysis cannot see through it.
std::optional<int> maybe_value();

TEST_CASE( "REQUIRE terminates the test case on failure" ) {
    std::optional<int> opt = maybe_value();
    REQUIRE( opt.has_value() );
    CHECK( *opt == 42 );
}

TEST_CASE( "REQUIRE_FALSE terminates the test case on failure" ) {
    std::optional<int> opt = maybe_value();
    REQUIRE_FALSE( !opt.has_value() );
    CHECK( *opt == 42 );
}

TEST_CASE( "CHECK does not terminate the test case on failure" ) {
    std::optional<int> opt = maybe_value();
    CHECK( opt.has_value() );
    CHECK( *opt == 42 ); // expect-warning: bugprone-unchecked-optional-access
}
