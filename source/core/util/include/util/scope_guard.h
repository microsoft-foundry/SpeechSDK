//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <functional>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class ScopeGuard
{
public:
    template<typename F>
    ScopeGuard(F fn) noexcept: m_fn{ fn }
    {}

    ~ScopeGuard()
    {
        m_fn();
    }

private:
    std::function<void()> m_fn;
};

} } } }
