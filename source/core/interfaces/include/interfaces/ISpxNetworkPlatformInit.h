//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <chrono>
#include <functional>
#include "interfaces/base.h"
#include "interfaces/ispx_http_error_handler.h"
#include "ispxinterfaces.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

/// <summary>
/// The interface that web socket implementations should implement
/// </summary>
SPX_INTERFACE(ISpxNetworkPlatformInit)
{
public:
    /// <summary>
    /// Initializes the network platform implementation
    /// </summary>
    virtual std::function<void()> Init() = 0;
};

}}}}
