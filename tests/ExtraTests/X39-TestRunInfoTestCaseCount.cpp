//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <iostream>

namespace {

    class TestRunInfoListener : public Catch::EventListenerBase {
    public:
        TestRunInfoListener( Catch::IConfig const* config ):
            EventListenerBase( config ) {}

        void testRunStarting( Catch::TestRunInfo const& runInfo ) override {
            std::cout << "X39 - testCaseCount: " << runInfo.testCaseCount << '\n';
        }
    };

} // namespace

CATCH_REGISTER_LISTENER( TestRunInfoListener )

TEST_CASE( "X39 visible A", "[x39a]" ) {
    SUCCEED();
}

TEST_CASE( "X39 visible B", "[x39b]" ) {
    SUCCEED();
}

TEST_CASE( "X39 visible AB", "[x39a][x39b]" ) {
    SUCCEED();
}

TEST_CASE( "X39 hidden", "[.][x39hidden]" ) {
    SUCCEED();
}
