//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <type_traits>
#include <utility>

#include "util/traits.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template<typename... Args, typename I, typename R>
auto resolveOverload(R(I::* member)(Args...)) -> decltype(member)
{
    return member;
}

template<typename I, typename R>
auto resolveOverload(R(I::* member)(void)) -> decltype(member)
{
    return member;
}

template<typename F, typename... Args, typename R = InvokeResult<F, Args...>, std::enable_if_t<!std::is_void<R>::value, int> = 0>
auto CallHandlerIfPresent(F handler, Args&&... args) -> R
{
    using ReturnType = decltype(std::declval<F>()(args...));
    if (handler)
    {
        return handler(std::forward<Args>(args)...);
    }
    return ReturnType{};
}

template<typename F, typename... Args, typename R = InvokeResult<F, Args...>, std::enable_if_t<std::is_void<R>::value, int> = 0>
void CallHandlerIfPresent(F handler, Args&&... args)
{
    if (handler)
    {
        handler(std::forward<Args>(args)...);
    }
}

template<typename T, size_t N>
T FindTupleInArrayOr(const std::array<std::tuple<const char *, T>, N>& array, const std::string& key, T defaultValue)
{
    auto found = std::find_if(array.begin(), array.end(), [&](const auto& entry)
    {
        const auto& k = std::get<0>(entry);
        return key == k;
    });

    if (found != array.end())
    {
        return std::get<T>(*found);
    }

    return defaultValue;
}

} } } }
