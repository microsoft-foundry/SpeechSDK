//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <cstdint>
#include <string>

#include "interfaces/base.h"
#include "interfaces/utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE(ISpxReadWriteBufferInit)
{
    public:
    enum class OverflowBehavior
    {
        DoNotAllow = 0,
        AllowWithTraces = 1,
        AllowWithoutTraces = 2,
        // Block the writer until the reader frees enough room, instead of throwing or dropping data.
        // Intended for single-producer/single-consumer streaming buffers that need write back-pressure.
        BlockUntilSpace = 3
    };

    virtual size_t SetSize(size_t size) = 0;
    virtual void SetInitPos(uint64_t pos) = 0;
    virtual void AllowOverflow(OverflowBehavior allow) = 0;

    virtual void SetName(std::string name) = 0;

    virtual void Term() = 0;
};

} } } }
