//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <memory>

#include "interfaces/base.h"
#include "interfaces/types.h"
#include "interfaces/named_properties.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE(ISpxHttpEventArgs)
{
    using Ptr = std::shared_ptr<ISpxHttpEventArgs>;
};

}
}
}
}
