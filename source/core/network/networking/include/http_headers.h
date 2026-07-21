//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

    namespace headers {
        constexpr auto userAgent = "User-Agent";
        constexpr auto ocpApimSubscriptionKey = "Ocp-Apim-Subscription-Key";
        constexpr auto apimSubscriptionKey = "apim-subscription-id";
        constexpr auto authorization = "Authorization";
        constexpr auto searchDelegationRPSToken = "X-Search-DelegationRPSToken";
        constexpr auto audioResponseFormat = "X-Output-AudioCodec";
        constexpr auto contentType = "Content-Type";
        constexpr auto contentLength = "Content-Length";
        constexpr auto streamId = "X-StreamId";
        constexpr auto requestId = "X-RequestId";
        constexpr auto region = "Ocp-Apim-Subscription-Region";
        constexpr auto continuationToken = "X-Continuation-Token";
        constexpr auto continuationOffset = "X-Continuation-Audio-Streams-1-Offset";
        constexpr auto connectionId = "X-ConnectionId";
        constexpr auto apimRequestId = "apim-request-id";
        constexpr auto msClientRequestId = "x-ms-client-request-id";
    }

    namespace contentTypes {
        constexpr auto applicationJson = "application/json";
        constexpr auto applicationOctetStream = "application/octet-stream";
    }

} } } }
