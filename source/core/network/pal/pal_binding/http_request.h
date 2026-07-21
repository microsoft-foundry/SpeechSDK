//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "http_response.h"
#include "interfaces/object_with_site.h"
#include "interfaces/generic_site.h"
#include "interface_helpers.h"
#include "../../../c_api/binding_callback_storage.h"
#include "binding_http_event_args.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

// Forward declarations
class CSpxBindingBasedHttpResponse;
using StreamedResponseDataHandler = std::function<void(const uint8_t*, size_t)>;

// This class implements HTTP requests by delegating to the binding layer (C#)
// It uses CallbackStorage instead of recognizer-specific components
class CSpxBindingBasedHttpRequest :
    public ISpxHttpRequest,
    public ISpxObjectWithSite,
    public std::enable_shared_from_this<CSpxBindingBasedHttpRequest>
{
public:
    CSpxBindingBasedHttpRequest() = default;
    virtual ~CSpxBindingBasedHttpRequest() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxHttpRequest)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
    SPX_INTERFACE_MAP_END()

    // Implementation of ISpxObjectWithSite
    virtual void SetSite(std::weak_ptr<ISpxGenericSite> site) override { m_site = site; }

    // Helper method to get the site
    ISpxGenericSite::Ptr GetSite() const { return m_site.lock(); }

    // ISpxHttpRequest implementation
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

private:
    // Helper methods for request/response handling
    void PopulateEventArgs(
        std::shared_ptr<CSpxHttpEventArgs> eventArgs,
        HttpMethod method,
        const IHttpEndpointInfo& endpoint,
        const uint8_t* content,
        size_t contentSize);

    std::unique_ptr<ISpxHttpResponse> PopulateResponse(
        std::shared_ptr<CSpxHttpEventArgs> result);

    // Helper to determine if content is likely text
    bool IsTextContent(const IHttpEndpointInfo& endpoint) const;

    // Site member
    std::weak_ptr<ISpxGenericSite> m_site;
};

}
}
}
}

