//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <memory>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

//
// Replace with reinterpret_pointer_cast when we move every compiler to C++17
//
template <class _Ty1, class _Ty2>
std::shared_ptr<_Ty1> SpxReinterpretPointerCast(const std::shared_ptr<_Ty2>& _Other) noexcept
{
    // reinterpret_cast for shared_ptr that properly respects the reference count control block
    const auto _Ptr = reinterpret_cast<typename std::shared_ptr<_Ty1>::element_type*>(_Other.get());
    return std::shared_ptr<_Ty1>(_Other, _Ptr);
}

} } } }
