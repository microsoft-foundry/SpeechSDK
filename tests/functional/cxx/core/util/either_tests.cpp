//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "test_utils.h"

#include "util/either.h"
#include "util/result.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXTEST_CASE_BEGIN("Either<T, U>: Basic operations", "[util][either]")
{
    using E = Either<std::string, int32_t>;

    constexpr const char * valueForLeft { "fourty two" };
    constexpr int32_t valueForRight { 42 };

    SPXTEST_SECTION("Create left")
    {
        E either{ valueForLeft };
        SPXTEST_REQUIRE(either.Has<std::string>());
        SPXTEST_REQUIRE_FALSE(either.Has<int32_t>());
        SPXTEST_REQUIRE_FALSE(either.Has<uint32_t>());

        auto& storedValue = either.Get<std::string>();
        SPXTEST_REQUIRE(valueForLeft == storedValue);

        SPXTEST_SECTION("Copy assignment")
        {
            E other(valueForRight);
            either = other;
            SPXTEST_REQUIRE_FALSE(either.Has<std::string>());
            SPXTEST_REQUIRE(either.Has<int32_t>());
            SPXTEST_REQUIRE_FALSE(either.Has<uint32_t>());

            auto& newValue = either.Get<int32_t>();
            SPXTEST_REQUIRE(valueForRight == newValue);
        }

        SPXTEST_SECTION("Move assignment")
        {
            E other(valueForRight);
            either = std::move(other);
            SPXTEST_REQUIRE_FALSE(either.Has<std::string>());
            SPXTEST_REQUIRE(either.Has<int32_t>());
            SPXTEST_REQUIRE_FALSE(either.Has<uint32_t>());

            auto& newValue = either.Get<int32_t>();
            SPXTEST_REQUIRE(valueForRight == newValue);
        }
    }

    SPXTEST_SECTION("Create right")
    {
        E either{ 42 };
        SPXTEST_REQUIRE_FALSE(either.Has<std::string>());
        SPXTEST_REQUIRE(either.Has<int32_t>());
        SPXTEST_REQUIRE_FALSE(either.Has<uint32_t>());
    }
}
SPXTEST_CASE_END()
