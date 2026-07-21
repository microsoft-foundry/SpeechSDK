//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "create_object_helpers.h"
#include "interfaces/ispx_http_transport_factory.h"
#include "interfaces/ISpxNetworkPlatformInit.h"
#include "site_helpers.h"
#include "exception.h"
#include <mutex>
#include <vector>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxResourceManager_HttpTransportFactory : public ISpxHttpTransportFactory
{
private:
    template <class T>
    std::shared_ptr<T> CreateNetworkObject(ISpxNamedProperties::Ptr properties, ISpxGenericSite::Ptr site, const char* classicPalClassName, const char* bindingPalClassName)
    {
        properties = properties == nullptr ? SpxQueryInterface<ISpxNamedProperties>(SpxGetRootSite()) : properties;
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, nullptr == properties);

        site = site == nullptr ? SpxGetRootSite() : site;
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, nullptr == site);

        auto transportType = properties->GetStringValue("service.transport.type", "core");

        const char* className;
        if (0 == PAL::stricmp("core", transportType.c_str()))
        {
            className = classicPalClassName;
        }
        else if (0 == PAL::stricmp("binding", transportType.c_str()))
        {
            className = bindingPalClassName;
        }
        else
        {
            SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
        }

        auto ret = SpxCreateObjectWithSite<T>(className, site);
        SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, ret == nullptr);

        auto platInit = SpxQueryInterface<ISpxNetworkPlatformInit>(ret);
        if (nullptr != platInit)
        {
            auto teardownFunction = platInit->Init();
            if (teardownFunction)
            {
                SPX_TRACE_INFO("Storing HTTP platform teardown function");
                std::lock_guard<std::mutex> lock(m_platformInitMutex);
                m_platformTeardownFunctions.push_back(teardownFunction);
            }
        }

        return ret;
    }

protected:
    std::mutex m_platformInitMutex;
    std::vector<std::function<void()>> m_platformTeardownFunctions;

public:
    ISpxHttpRequest::Ptr CreateHttpRequest(ISpxNamedProperties::Ptr properties, ISpxGenericSite::Ptr site)
    {
        return CreateNetworkObject<ISpxHttpRequest>(properties, site, "CSpxHttpRequest", "CSpxBindingBasedHttpRequest");
    }

    ISpxWebSocket::Ptr CreateWebSocket(ISpxNamedProperties::Ptr properties, ISpxGenericSite::Ptr site)
    {
        return CreateNetworkObject<ISpxWebSocket>(properties, site, "CSpxWebSocket", "CSpxBindingBasedWebSocket");
    }
}; // Microsoft::CognitiveServices::Speech::Impl
}}}}
#pragma once
