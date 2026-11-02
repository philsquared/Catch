
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#include <catch2/catch_test_macros.hpp>

#include <optional>

// Deliberately not defined, so that the analysis cannot see through it.
std::optional<int> maybe_value();

TEST_CASE( "CHECKED_IF branches on the condition" ) {
    std::optional<int> opt = maybe_value();
    CHECKED_IF( opt.has_value() ) { CHECK( *opt == 42 ); }
}

TEST_CASE( "CHECKED_ELSE branches on the negated condition" ) {
    std::optional<int> opt = maybe_value();
    CHECKED_ELSE( opt.has_value() ) { CHECK( *opt == 42 ); } // expect-warning: bugprone-unchecked-optional-access
}
