
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

TEST_CASE( "REQUIRE_THROWS terminates the test case when nothing is thrown" ) {
    std::optional<int> opt = 42;
    REQUIRE_THROWS( should_throw() ? throw std::runtime_error( "boom" )
                                   : opt.reset() );
    CHECK( *opt == 42 );
}

TEST_CASE( "CHECK_THROWS keeps going when nothing is thrown" ) {
    std::optional<int> opt = 42;
    CHECK_THROWS( should_throw() ? throw std::runtime_error( "boom" )
                                 : opt.reset() );
    CHECK( *opt == 42 ); // expect-warning: bugprone-unchecked-optional-access
}
