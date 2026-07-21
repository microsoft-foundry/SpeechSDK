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

SPX_INTERFACE(ISpxServiceProvider)
{
public:
    virtual std::shared_ptr<ISpxInterfaceBase> QueryService(uint64_t serviceTypeId) = 0;
};

} } } }
