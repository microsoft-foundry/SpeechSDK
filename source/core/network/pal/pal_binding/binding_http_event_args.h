//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// binding_http_event_args.h: HTTP event arguments class
//

#pragma once

#include "stdafx.h"
#include "interfaces/http_event_args.h"
#include "property_bag_impl.h"
#include <map>
#include <string>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

// Event args class for HTTP events
class CSpxHttpEventArgs : public ISpxHttpEventArgs, public ISpxPropertyBagImpl
{
public:
    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()

    CSpxHttpEventArgs() = default;
    virtual ~CSpxHttpEventArgs() = default;
};

// Define Type<EventArgs> to fix the "Id is not a member of Type<T>" error
_SPX_DEFINE_TYPE(CSpxHttpEventArgs)

} // namespace Impl
} // namespace Speech
} // namespace CognitiveServices
} // namespace Microsoft
