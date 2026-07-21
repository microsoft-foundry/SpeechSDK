//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include "base.h"
#include "ispx_http_request.h"
#include "ispx_http_response.h"
#include "web_socket.h"
#include "named_properties.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

/// <summary>
/// The interface for sending HTTP requests
/// </summary>
SPX_INTERFACE(ISpxHttpTransportFactory)
{
public:
    /// <summary>
    /// Creates an HTTP Request according to ether the default policies, or the settings in the property collection
    /// </summary>
    /// <param name="properties">An optional properties collection that contains information to construct the HttpRequest with.</param>
    /// <returns>An HTTP Request object that can be used to issue an HTTP request.</returns>
    virtual ISpxHttpRequest::Ptr CreateHttpRequest(ISpxNamedProperties::Ptr properties = nullptr, ISpxGenericSite::Ptr site = nullptr) = 0;

    /// <summary>
    /// Creates a WebSocket according to ether the default policies, or the settings in the property collection
    /// </summary>
    /// <param name="properties"An optional properties collection that contains information to construct the WebSocket with.</param>></param>
    /// <returns>A WebSocket implementations</returns>
    /// <remarks>
    /// The WebSocket may still need to be initialized after construction. see: ISpxWebSocketInit.
    /// </remarks>
    virtual ISpxWebSocket::Ptr CreateWebSocket(ISpxNamedProperties::Ptr properties = nullptr, ISpxGenericSite::Ptr site = nullptr) = 0;
};

}}}}
