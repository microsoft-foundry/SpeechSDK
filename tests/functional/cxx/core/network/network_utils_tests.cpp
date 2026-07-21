//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "network/usp/header_builder.h"
#include "network/usp/header_view.h"
#include "network/usp/header_utils.h"
#include "network/usp/message_builder.h"
#include "network/usp/message_utils.h"

#include "mocks.h"
#include "test_utils.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;
using namespace Microsoft::CognitiveServices::Speech::Impl::USP;

template<typename TVal>
bool HasValueAndEquals(Maybe<TVal> const& maybe, const TVal& value)
{
    return maybe && maybe.Get() == value;
}

bool HasValueAndEquals(Maybe<std::string> const& maybe, const char* value)
{
    return maybe && maybe.Get() == std::string{ value };
}

SPXTEST_CASE_BEGIN("Header Serialization/Deserialization", "[network][utils]")
{
    auto ts = std::chrono::system_clock::now();
    SPXTEST_CAPTURE(PAL::GetTicks(ts));
    auto expectedUtcTimestamp = PAL::GetTimeInString(ts, 3);
    SPXTEST_CAPTURE(expectedUtcTimestamp);

    // we lose some precision for the USP timestamp so convert back from UTC format
    std::chrono::system_clock::time_point expectedTs;
    SPXTEST_REQUIRE(PAL::TryParseUtcTimestamp(expectedUtcTimestamp, expectedTs));

    auto headers = USPHeaderBuilder{ "config" }
        .ContentType("application/json")
        .RequestId("42")
        .AddHeader("X-Extra-Header", "ExtraValue")
        .Timestamp(ts)
        .Build();

    auto predictedSize = USPHeaderUtils::Serialize(headers, nullptr, 0);
    auto buffer = SpxAllocUniqueBuffer<uint8_t>(predictedSize + 1);
    auto actualSize = USPHeaderUtils::Serialize(headers, buffer.get(), predictedSize + 1);
    SPXTEST_REQUIRE(predictedSize == actualSize);
    auto deserialized = USPHeaderUtils::Deserialize(buffer.get(), actualSize);
    SPXTEST_REQUIRE(deserialized.size() == 5);

    const USPHeaderView headerView(deserialized);
    SPXTEST_REQUIRE(HasValueAndEquals(headerView.Path(), "config"));
    SPXTEST_REQUIRE(HasValueAndEquals(headerView.ContentType(), "application/json"));
    SPXTEST_REQUIRE(HasValueAndEquals(headerView.RequestId(), "42"));
    SPXTEST_REQUIRE(HasValueAndEquals(headerView.GetHeader("X-Extra-Header"), "ExtraValue"));
    SPXTEST_REQUIRE(HasValueAndEquals(headerView.UtcTimestampString(), expectedUtcTimestamp));
    SPXTEST_REQUIRE(HasValueAndEquals(headerView.Timestamp(), expectedTs));

    // On most systems, we should not differ by more than 9999 ticks. However in some cases
    // we have lower precision than this so allow for a larger delta of 1,000,000 ticks
    // 1 tick = 100ns
    const std::chrono::nanoseconds maxAllowedDelta{ 1000000 * 100 };
    auto delta = ts - headerView.Timestamp().Get();
    auto actualDeltaSecs = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
    SPXTEST_CAPTURE(delta.count());
    SPXTEST_REQUIRE(actualDeltaSecs >= 0s);
    SPXTEST_REQUIRE(actualDeltaSecs <= maxAllowedDelta);
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Message Serialization/Deserialization", "[network][utils]")
{
    auto headers = USPHeaderBuilder{ "config" }
        .ContentType("application/json")
        .RequestId("42")
        .AddHeader("X-Extra-Header", "ExtraValue")
        .Build();

    SPXTEST_SECTION("Text message")
    {
        std::string payload{ "this is a message" };
        auto message = USPMessageBuilder{}
            .Headers(std::move(headers))
            .Payload(std::move(payload))
            .Build();
        auto predictedSize = USPMessageUtils::Serialize(message, nullptr, 0);
        auto buffer = SpxAllocSharedBuffer<uint8_t>(predictedSize + 1);
        auto actualSize = USPMessageUtils::Serialize(message, buffer.get(), predictedSize + 1);
        SPXTEST_REQUIRE(predictedSize == actualSize);
        auto deserialized = USPMessageUtils::Deserialize(buffer.get(), actualSize, false);
        SPXTEST_REQUIRE(deserialized.Headers.size() == 5);

        const USPHeaderView headerView(deserialized.Headers);
        SPXTEST_REQUIRE(HasValueAndEquals(headerView.Path(), "config"));
        SPXTEST_REQUIRE(HasValueAndEquals(headerView.ContentType(), "application/json"));
        SPXTEST_REQUIRE(HasValueAndEquals(headerView.RequestId(), "42"));
        SPXTEST_REQUIRE(HasValueAndEquals(headerView.GetHeader("X-Extra-Header"), "ExtraValue"));

        SPXTEST_REQUIRE(deserialized.Data.Has<std::string>());
        SPXTEST_REQUIRE(deserialized.Data.Get<std::string>() == "this is a message");
    }

    SPXTEST_SECTION("Binary message")
    {
        SharedBufferView<uint8_t> payload{ SpxAllocSharedBuffer<uint8_t>(10), 10 };
        payload[0] =  2; payload[1] =  3; payload[2] =  5; payload[3] =  7; payload[4] = 11;
        payload[5] = 13; payload[6] = 17; payload[7] = 18; payload[8] = 23; payload[9] = 29;
        auto message = USPMessageBuilder{}
            .Headers(std::move(headers))
            .Payload(payload)
            .Build();
        auto predictedSize = USPMessageUtils::Serialize(message, nullptr, 0);
        auto buffer = SpxAllocSharedBuffer<uint8_t>(predictedSize + 1);
        auto actualSize = USPMessageUtils::Serialize(message, buffer.get(), predictedSize + 1);
        SPXTEST_REQUIRE(predictedSize == actualSize);
        auto deserialized = USPMessageUtils::Deserialize(buffer.get(), actualSize, true);
        SPXTEST_REQUIRE(deserialized.Headers.size() == 5);

        const USPHeaderView headerView(deserialized.Headers);
        SPXTEST_REQUIRE(HasValueAndEquals(headerView.Path(), "config"));
        SPXTEST_REQUIRE(HasValueAndEquals(headerView.ContentType(), "application/json"));
        SPXTEST_REQUIRE(HasValueAndEquals(headerView.RequestId(), "42"));
        SPXTEST_REQUIRE(HasValueAndEquals(headerView.GetHeader("X-Extra-Header"), "ExtraValue"));

        SPXTEST_REQUIRE(deserialized.Data.Has<SharedBufferView<uint8_t>>());
        auto deserializedPayload = deserialized.Data.Get<SharedBufferView<uint8_t>>();
        SPXTEST_REQUIRE(deserializedPayload.Size() == payload.Size());
        for (size_t i{ 0 }; i < payload.Size(); i++)
        {
            SPXTEST_REQUIRE(deserializedPayload[i] == payload[i]);
        }
    }
}
SPXTEST_CASE_END()
