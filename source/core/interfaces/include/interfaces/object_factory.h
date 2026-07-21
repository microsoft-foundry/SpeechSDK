//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <memory>

#include "interfaces/base.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE(ISpxObjectFactory)
{
public:
    template<class I>
    std::shared_ptr<I> CreateObject(const char* className)
    {
        // try to create the object from our interface virtual method...
        auto obj = CreateObject(className, Type<I>::Id);
        if (obj != nullptr)
        {
            auto ptr = reinterpret_cast<I*>(obj);
            return std::shared_ptr<I>(ptr);
        }

        // if we can't, return nullptr to let the caller know
        return nullptr;
    }

    virtual void* CreateObject(const char* className, uint64_t typeId) = 0;
};

} } } }
