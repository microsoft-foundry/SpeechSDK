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

AZAC_EXTERN_C void* Microsoft::CognitiveServices::Speech::Impl::Pal_CreateModuleObject(const char* className, uint64_t interfaceTypeId)
{
    using namespace Microsoft::CognitiveServices::Speech::Impl;
    SPX_FACTORY_MAP_BEGIN()
        SPX_FACTORY_MAP_ENTRY(CSpxHttpRequest, ISpxHttpRequest)
        SPX_FACTORY_MAP_ENTRY(CSpxWebSocket, ISpxWebSocket)
    SPX_FACTORY_MAP_END()
}

PAL_API pal_get_platform(void** value)
{
    using namespace Microsoft::CognitiveServices::Speech::Impl;
    try
    {
        *value = HttpPlatformImpl::Instance();
        return AZAC_ERR_NONE;
    }
    catch (...)
    {
        return AZAC_ERR_RUNTIME_ERROR;
    }
}
