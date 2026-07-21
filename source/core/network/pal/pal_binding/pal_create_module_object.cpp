//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "factory_helpers.h"
#include "http_request.h"
#include "http_response.h"
#include "pal_create_module_object.h"
#include "http_platform_impl.h"
#include "../../../interfaces/include/interfaces/http_event_args.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_EXTERN_C void* Pal_CreateModuleObject_Binding(const char* className, uint64_t interfaceTypeId)
{
    using namespace Microsoft::CognitiveServices::Speech::Impl;
    SPX_FACTORY_MAP_BEGIN();
        SPX_FACTORY_MAP_ENTRY_REPLACE(CSpxHttpRequest, ISpxHttpRequest, CSpxBindingBasedHttpRequest);
        SPX_FACTORY_MAP_ENTRY_REPLACE(CSpxHttpResponse, ISpxHttpResponse, CSpxBindingBasedHttpResponse);
        SPX_FACTORY_MAP_ENTRY(CSpxHttpEventArgs, ISpxHttpEventArgs);
    SPX_FACTORY_MAP_END();
}

}}}} // Microsoft::CognitiveServices::Speech::Impl

// Platform binding function implementation
PAL_API pal_get_platform_binding(void** value)
{
    using namespace Microsoft::CognitiveServices::Speech::Impl;
    AZACHR ret = AZAC_ERR_NONE;

    try
    {
        *value = HttpBindingPlatformImpl::Instance();
    }
    catch (const std::exception& ex)
    {
        SPX_TRACE_ERROR("Failed to get platform binding: %s", ex.what());
        ret = AZAC_ERR_RUNTIME_ERROR;
    }
    catch (...)
    {
        SPX_TRACE_ERROR("Failed to get platform binding");
        ret = AZAC_ERR_RUNTIME_ERROR;
    }

    return ret;
}
