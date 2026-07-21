//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <interfaces/ispx_http_request.h>
#include <interface_helpers.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    class CSpxHttpRequest : public ISpxHttpRequest
    {
    public:
        CSpxHttpRequest() = default;
        virtual ~CSpxHttpRequest() = default;

        SPX_INTERFACE_MAP_BEGIN()
            SPX_INTERFACE_MAP_ENTRY(ISpxHttpRequest)
        SPX_INTERFACE_MAP_END()

        virtual std::unique_ptr<ISpxHttpResponse> SendRequest(
            HttpMethod method,
            const IHttpEndpointInfo& endpoint,
            const uint8_t* content = nullptr,
            size_t contentSize = 0,
            const ISpxHttpErrorHandler::ConstPtr& errorHandler = nullptr) override;

        virtual std::unique_ptr<ISpxHttpResponse> SendRequestStreamResponse(
            HttpMethod method,
            const IHttpEndpointInfo& endpoint,
            StreamedResponseDataHandler&& onDataCallback,
            const uint8_t* content = nullptr,
            size_t contentSize = 0,
            const ISpxHttpErrorHandler::ConstPtr& errorHandler = nullptr) override;
    };

}}}}
