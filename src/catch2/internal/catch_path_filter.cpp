//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#include <catch2/internal/catch_parse_numbers.hpp>
#include <catch2/internal/catch_path_filter.hpp>

namespace Catch {

    Optional<std::vector<GeneratorIndexRange>>
    parseGeneratorIndexRanges( std::string const& spec ) {
        std::vector<GeneratorIndexRange> ranges;
        std::size_t start = 0;
        do {
            auto const comma = spec.find( ',', start );
            auto const token = spec.substr( start, comma - start );
            auto const dash = token.find( '-' );
            auto const first = parseUInt( token.substr( 0, dash ) );
            auto const last = dash == std::string::npos
                                  ? first
                                  : parseUInt( token.substr( dash + 1 ) );
            if ( !first || !last || *first > *last ||
                 ( !ranges.empty() && *first <= ranges.back().last ) ) {
                return {};
            }
            ranges.push_back( { *first, *last } );
            if ( comma == std::string::npos ) { break; }
            start = comma + 1;
        } while ( true );
        return ranges;
    }

} // namespace Catch
