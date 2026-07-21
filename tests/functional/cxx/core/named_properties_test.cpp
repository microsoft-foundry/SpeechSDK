//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"

#include <array>
#include <chrono>
#include <memory>
#include <string>

#include "interfaces/named_properties.h"
#include "named_properties.h"
#include "speechapi_cxx_enums.h"

#include "mocks.h"
#include "test_utils.h"

using namespace std::chrono_literals;

enum class TestEnum
{
    One = 0,
    Two
};

enum class TestEnumWithUnderlying: uint64_t
{
    Three = 0,
    Four
};

template<typename T> struct TestValues {};

template<> struct TestValues<int32_t> { static constexpr std::array<int32_t, 2> Values{ 42, -47 }; };
constexpr std::array<int32_t, 2> TestValues<int32_t>::Values;
template<> struct TestValues<uint32_t> { static constexpr std::array<uint32_t, 2> Values{ 7, 13 }; };
constexpr std::array<uint32_t, 2> TestValues<uint32_t>::Values;
template<> struct TestValues<bool> { static constexpr std::array<bool, 2> Values{ true, false}; };
constexpr std::array<bool, 2> TestValues<bool>::Values;
template<> struct TestValues<double> { static constexpr std::array<double, 2> Values{ 3.14, 2.718 }; };
constexpr std::array<double, 2> TestValues<double>::Values;
template<> struct TestValues<std::string> { static const std::array<std::string, 2> Values; };
const std::array<std::string, 2> TestValues<std::string>::Values = { std::string{"one"}, std::string{"two"} };
template<> struct TestValues<std::chrono::milliseconds> { static constexpr std::array<std::chrono::milliseconds, 2> Values{ 11ms, 23ms }; };
constexpr std::array<std::chrono::milliseconds, 2> TestValues<std::chrono::milliseconds>::Values;
template<> struct TestValues<TestEnum> { static constexpr std::array<TestEnum, 2> Values{ TestEnum::One, TestEnum::Two }; };
constexpr std::array<TestEnum, 2> TestValues<TestEnum>::Values;
template<> struct TestValues<TestEnumWithUnderlying> { static constexpr std::array<TestEnumWithUnderlying, 2> Values{ TestEnumWithUnderlying::Three, TestEnumWithUnderlying::Four }; };
constexpr std::array<TestEnumWithUnderlying, 2> TestValues<TestEnumWithUnderlying>::Values;


SPXTEST_TEMPLATE_CASE_BEGIN("ISpxNamedProperties - Get/Set property", "[core][unit][properties]",
    int32_t, uint32_t, double, std::chrono::milliseconds, bool, std::string, TestEnum, TestEnumWithUnderlying)
{
    using PropertyId = Microsoft::CognitiveServices::Speech::PropertyId;
    auto& Values = TestValues<TestType>::Values;
    constexpr auto propName = "my_prop";
    Carbon::ISpxNamedProperties::Ptr properties = std::make_shared<Carbon::CSpxNamedProperties>();
    properties->Set(propName, Values[0]);
    properties->Set(PropertyId::AudioConfig_AudioProcessingOptions, Values[1]);

    auto prop1 = properties->Get<TestType>(propName);
    auto prop2 = properties->Get<TestType>(PropertyId::AudioConfig_AudioProcessingOptions);

    SPXTEST_REQUIRE(prop1);
    SPXTEST_REQUIRE(prop2);

    SPXTEST_REQUIRE(prop1.Get() == Values[0]);
    SPXTEST_REQUIRE(prop2.Get() == Values[1]);
}
SPXTEST_CASE_END()

