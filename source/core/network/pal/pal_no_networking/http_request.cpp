//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <stdexcept>
#include "http_request.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    std::unique_ptr<ISpxHttpResponse> CSpxHttpRequest::SendRequest(
        HttpMethod method, const IHttpEndpointInfo& endpoint, const uint8_t* content, size_t contentSize, const ISpxHttpErrorHandler::ConstPtr& errorHandler)
    {
        (void)method;
        (void)endpoint;
        (void)content;
        (void)contentSize;
        (void)errorHandler;

        throw std::runtime_error("Native networking is disabled");
    }

    std::unique_ptr<ISpxHttpResponse> CSpxHttpRequest::SendRequestStreamResponse(
        HttpMethod method, const IHttpEndpointInfo& endpoint, StreamedResponseDataHandler&& onDataCallback, const uint8_t* content, size_t contentSize, const ISpxHttpErrorHandler::ConstPtr& errorHandler)
    {
        (void)method;
        (void)endpoint;
        (void)onDataCallback;
        (void)content;
        (void)contentSize;
        (void)errorHandler;

        throw std::runtime_error("Native networking is disabled");
    }

} } } }
