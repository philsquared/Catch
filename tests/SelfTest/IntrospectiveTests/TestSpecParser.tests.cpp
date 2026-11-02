
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/internal/catch_tag_alias_registry.hpp>
#include <catch2/internal/catch_test_spec_parser.hpp>

namespace {
    static constexpr Catch::SourceLineInfo dummySourceLineInfo = CATCH_INTERNAL_LINEINFO;

    static Catch::TestSpec parseAndCreateSpec(std::string const& str) {
        Catch::TagAliasRegistry registry;
        Catch::TestSpecParser parser( registry );

        parser.parse( str );
        auto spec = parser.testSpec();
        REQUIRE( spec.hasFilters() );
        REQUIRE( spec.getInvalidSpecs().empty());

        return spec;
    }

}

TEST_CASE( "Parsing tags with non-alphabetical characters is pass-through",
           "[test-spec][test-spec-parser]" ) {
    auto const& tagString = GENERATE( as<std::string>{},
                                      "[tag with spaces]",
                                      "[I said \"good day\" sir!]" );
    CAPTURE(tagString);

    auto spec = parseAndCreateSpec( tagString );

    Catch::TestCaseInfo testCase(
        "", { "fake test name", tagString }, dummySourceLineInfo );

    REQUIRE( spec.matches( testCase ) );
}

TEST_CASE("Parsed tags are matched case insensitive",
    "[test-spec][test-spec-parser]") {
    auto spec = parseAndCreateSpec( "[CASED tag]" );

    Catch::TestCaseInfo testCase(
        "", { "fake test name", "[cased TAG]" }, dummySourceLineInfo );

    REQUIRE( spec.matches( testCase ) );
}

TEST_CASE( "Empty tag filters are invalid test specifications",
           "[test-spec][test-spec-parser][approvals]" ) {
    auto const& input = GENERATE( as<std::string>{},
                                  "[]",
                                  "~[]",
                                  "exclude:[]",
                                  "[][valid]",
                                  "[valid][]",
                                  "[],[valid]",
                                  "[valid],[]",
                                  "[][]" );
    CAPTURE( input );
    Catch::TagAliasRegistry registry;
    auto spec = Catch::TestSpecParser( registry ).parse( input ).testSpec();
    REQUIRE( spec.getInvalidSpecs() == std::vector<std::string>{ input } );
}

TEST_CASE( "Invalid empty tags retain the original argument across aliases",
           "[test-spec][test-spec-parser][approvals]" ) {
    Catch::TagAliasRegistry registry;
    registry.add( "[@empty]", "[]", dummySourceLineInfo );
    Catch::TestSpecParser parser( registry );
    parser.parse( "[@empty]" ).parse( "[valid]" ).parse( "~[]" );
    auto spec = parser.testSpec();
    REQUIRE( spec.getInvalidSpecs() ==
             std::vector<std::string>{ "[@empty]", "~[]" } );
}

TEST_CASE( "Empty names and literal brackets are not empty tag filters",
           "[test-spec][test-spec-parser][approvals]" ) {
    auto const& input = GENERATE( as<std::string>{},
                                  "",
                                  "\"\"",
                                  "\"[]\"",
                                  "\\[\\]",
                                  "[ ]",
                                  "[.]",
                                  "[valid]" );
    CAPTURE( input );
    Catch::TagAliasRegistry registry;
    auto spec = Catch::TestSpecParser( registry ).parse( input ).testSpec();
    REQUIRE( spec.getInvalidSpecs().empty() );
}

TEST_CASE(
    "Empty tag filters do not inherit pattern text from earlier arguments",
    "[test-spec][test-spec-parser][approvals]" ) {
    auto const& previous =
        GENERATE( as<std::string>{}, "\\[\\]", "foo\\", "[foo\\" );
    auto const& input = GENERATE(
        as<std::string>{}, "[]", "~[]", "[][valid]", "[],[valid]", "[]*" );
    CAPTURE( previous, input );
    Catch::TagAliasRegistry registry;
    Catch::TestSpecParser parser( registry );
    parser.parse( previous ).parse( input );
    auto spec = parser.testSpec();
    REQUIRE( spec.getInvalidSpecs() == std::vector<std::string>{ input } );
}

TEST_CASE( "Literal bracket names can be combined with tag filters",
           "[test-spec][test-spec-parser][approvals]" ) {
    Catch::TagAliasRegistry registry;
    Catch::TestSpecParser parser( registry );
    SECTION( "Name before tag" ) { parser.parse( "\"[]\"" ).parse( "[valid]" ); }
    SECTION( "Tag before name" ) { parser.parse( "[valid]" ).parse( "\"[]\"" ); }
    SECTION( "Name and tag in one argument" ) { parser.parse( "\"[]\"[valid]" ); }
    auto spec = parser.testSpec();
    REQUIRE( spec.getInvalidSpecs().empty() );
    Catch::TestCaseInfo bracketName(
        "", { "[]", "[valid]" }, dummySourceLineInfo );
    Catch::TestCaseInfo ordinaryName(
        "", { "ordinary", "[valid]" }, dummySourceLineInfo );
    Catch::TestCaseInfo otherTag(
        "", { "[]", "[other]" }, dummySourceLineInfo );
    REQUIRE( spec.matches( bracketName ) );
    REQUIRE_FALSE( spec.matches( ordinaryName ) );
    REQUIRE_FALSE( spec.matches( otherTag ) );
}
