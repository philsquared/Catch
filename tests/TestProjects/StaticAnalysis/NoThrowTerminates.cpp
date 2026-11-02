
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

TEST_CASE( "REQUIRE_NOTHROW terminates the test case when the expression throws" ) {
    std::optional<int> opt;
    REQUIRE_NOTHROW( opt ? 0 : throw std::runtime_error("empty") );
    CHECK( *opt == 42 );
}

TEST_CASE( "CHECK_NOTHROW keeps going when the expression throws" ) {
    std::optional<int> opt;
    CHECK_NOTHROW( opt ? 0 : throw std::runtime_error("empty") );
    CHECK( *opt == 42 ); // expect-warning: bugprone-unchecked-optional-access
}
