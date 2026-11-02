
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

// Deliberately not defined, so that the analysis cannot see through it.
bool should_throw();

TEST_CASE( "REQUIRE_THROWS_AS terminates the test case on a missing exception" ) {
    std::optional<int> opt = 42;
    REQUIRE_THROWS_AS( should_throw() ? throw std::runtime_error( "boom" )
                                      : opt.reset(),
                       std::runtime_error );
    CHECK( *opt == 42 );
}

TEST_CASE( "CHECK_THROWS_AS keeps going on a missing exception" ) {
    std::optional<int> opt = 42;
    CHECK_THROWS_AS( should_throw() ? throw std::runtime_error( "boom" )
                                    : opt.reset(),
                     std::runtime_error );
    CHECK( *opt == 42 ); // expect-warning: bugprone-unchecked-optional-access
}
