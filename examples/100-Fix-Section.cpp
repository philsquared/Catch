// 100-Fix-Section.cpp

// Catch has two ways to express fixtures:
// - Sections (this file)
// - Traditional class-based fixtures

// Let Catch provide main():
#define CATCH_CONFIG_MAIN

#include "catch.hpp"

TEST_CASE( "vectors can be sized and resized", "[vector]" ) {

    // For each section, vector v is anew:

    std::vector<int> v( 5 );

    REQUIRE( v.size() == 5 );
    REQUIRE( v.capacity() >= 5 );

    SECTION( "resizing bigger changes size and capacity" ) {
        v.resize( 10 );

        REQUIRE( v.size() == 10 );
        REQUIRE( v.capacity() >= 10 );
    }
    SECTION( "resizing smaller changes size but not capacity" ) {
        v.resize( 0 );

        REQUIRE( v.size() == 0 );
        REQUIRE( v.capacity() >= 5 );
    }
    SECTION( "reserving bigger changes capacity but not size" ) {
        v.reserve( 10 );

        REQUIRE( v.size() == 5 );
        REQUIRE( v.capacity() >= 10 );
    }
    SECTION( "reserving smaller does not change size or capacity" ) {
        v.reserve( 0 );

        REQUIRE( v.size() == 5 );
        REQUIRE( v.capacity() >= 5 );
    }
}

// Compile & run:
// - g++ -std=c++11 -Wall -I$(CATCH_SINGLE_INCLUDE) -o 100-Fix-Section 100-Fix-Section.cpp && 100-Fix-Section --success
// - cl -EHsc -I%CATCH_SINGLE_INCLUDE% 100-Fix-Section.cpp && 100-Fix-Section --success

// Expected compact output (all assertions):
//
// prompt> 100-Fix-Section.exe --reporter compact --success
// 100-Fix-Section.cpp:16: passed: v.size() == 5 for: 5 == 5
// 100-Fix-Section.cpp:17: passed: v.capacity() >= 5 for: 5 >= 5
// 100-Fix-Section.cpp:22: passed: v.size() == 10 for: 10 == 10
// 100-Fix-Section.cpp:23: passed: v.capacity() >= 10 for: 10 >= 10
// 100-Fix-Section.cpp:16: passed: v.size() == 5 for: 5 == 5
// 100-Fix-Section.cpp:17: passed: v.capacity() >= 5 for: 5 >= 5
// 100-Fix-Section.cpp:28: passed: v.size() == 0 for: 0 == 0
// 100-Fix-Section.cpp:29: passed: v.capacity() >= 5 for: 5 >= 5
// 100-Fix-Section.cpp:16: passed: v.size() == 5 for: 5 == 5
// 100-Fix-Section.cpp:17: passed: v.capacity() >= 5 for: 5 >= 5
// 100-Fix-Section.cpp:34: passed: v.size() == 5 for: 5 == 5
// 100-Fix-Section.cpp:35: passed: v.capacity() >= 10 for: 10 >= 10
// 100-Fix-Section.cpp:16: passed: v.size() == 5 for: 5 == 5
// 100-Fix-Section.cpp:17: passed: v.capacity() >= 5 for: 5 >= 5
// 100-Fix-Section.cpp:40: passed: v.size() == 5 for: 5 == 5
// 100-Fix-Section.cpp:41: passed: v.capacity() >= 5 for: 5 >= 5
// Passed 1 test case with 16 assertions.