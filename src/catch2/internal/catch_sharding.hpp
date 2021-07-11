
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#ifndef CATCH_SHARDING_HPP_INCLUDED
#define CATCH_SHARDING_HPP_INCLUDED

#include <catch2/catch_session.hpp>

#include <cmath>

namespace Catch {

    template<typename CONTAINER>
    CONTAINER createShard(CONTAINER const& container, IConfig const& config) {
        if (config.shardCount() > 1) {
            unsigned int totalTestCount = container.size();

            unsigned int shardCount = (std::min)(config.shardCount(), totalTestCount);
            unsigned int shardIndex = (std::min)(config.shardIndex(), shardCount - 1);

            double shardSize = totalTestCount / static_cast<double>(shardCount);
            double startIndex = shardIndex * shardSize;

            auto startIterator = std::next(container.begin(), std::floor(startIndex));
            auto endIterator = std::next(container.begin(), std::floor(startIndex + shardSize));

            // Since we are calculating the end index with floating point numbers, but flooring 
            // the value, we can't guarantee that the end index of the last shard lines up exactly
            // with the end of input container. If we want the last shard, force the end index to
            // be the end of the input container.
            if (shardIndex == shardCount - 1) {
                endIterator = container.end();
            }

            return CONTAINER(startIterator, endIterator);
        } else {
            return container;
        }
    }

}

#endif // CATCH_SHARDING_HPP_INCLUDED
