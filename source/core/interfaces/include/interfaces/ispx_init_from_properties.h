//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include "interfaces/base.h"
#include "interfaces/types.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class ISpxNamedProperties;

SPX_INTERFACE(ISpxInitFromProperties)
{
    public:

    virtual void InitFromProperties(const char* optionName, const char* optionValue, const std::shared_ptr<ISpxNamedProperties>& moreOptions, const char* moreNamespace = nullptr) = 0;
};

} } } }
