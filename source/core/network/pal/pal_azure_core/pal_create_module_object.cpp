//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "factory_helpers.h"
#include "http_request.h"
#include "web_socket.h"
#include "pal_create_module_object.h"
#include "http_platform_impl.h"

SPX_EXTERN_C void* Microsoft::CognitiveServices::Speech::Impl::Pal_CreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    using namespace Microsoft::CognitiveServices::Speech::Impl;
    SPX_FACTORY_MAP_BEGIN();
        SPX_FACTORY_MAP_ENTRY(CSpxHttpRequest, ISpxHttpRequest);
        SPX_FACTORY_MAP_ENTRY(CSpxWebSocket, ISpxWebSocket);

        // In case the developer wants to intercept the HTTP or web socket implementations used but
        // still forward to the native built-in Azure IoT based one, they can create an instance
        // using _AzureIoT postfix
        SPX_FACTORY_MAP_ENTRY_REPLACE(CSpxHttpRequest_AzureCore, ISpxHttpRequest, CSpxHttpRequest)
        SPX_FACTORY_MAP_ENTRY_REPLACE(CSpxWebSocket_AzureCore, ISpxWebSocket, CSpxWebSocket)
    SPX_FACTORY_MAP_END();
}

PAL_API pal_get_platform(void** value)
{
    using namespace Microsoft::CognitiveServices::Speech::Impl;
    AZACHR ret = AZAC_ERR_NONE;

    try
    {
        *value = HttpPlatformImpl::Instance();
    }
    catch (const std::exception& ex)
    {
        SPX_TRACE_ERROR("Failed to get value: %s", ex.what());
        ret = AZAC_ERR_RUNTIME_ERROR;
    }
    catch (...)
    {
        SPX_TRACE_ERROR("Failed to get value");
        ret = AZAC_ERR_RUNTIME_ERROR;
    }

    return ret;
}
