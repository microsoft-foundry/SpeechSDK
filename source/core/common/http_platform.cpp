//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <mutex>
#include <stdexcept>

#include "http_platform.h"
#include "i_http_platform.h"
#include <azac_api_c_common.h>
#include <functional>

#if defined(PAL_CONFIG_EXPORTAPIS)
#define PAL_API_EXPORT         AZAC_DLL_EXPORT
#else
#define PAL_API_EXPORT         AZAC_DLL_IMPORT
#endif

#define PAL_API AZAC_EXTERN_C PAL_API_EXPORT AZAC_API_RESULTTYPE AZAC_API_NOTHROW AZAC_API_CALLTYPE

/// <summary>
/// Gets the network platform from the correct platform library.
/// </summary>
/// <param name="pValue">Pointer to the value to set</param>
/// <returns>AZAC_ERR_NONE on success, AZAC_ERR_NOT_FOUND if the name specified is unknown,
/// AZAC_ERR_INVALID_ARG if either argument passed is null, or AZAC_ERR_RUNTIME_ERROR
/// in the case of errors</returns>
PAL_API pal_get_platform(void** ppValue);

PAL::HttpPlatform::ExternalLogFunction g_logFunction = nullptr;

namespace
{
using namespace Microsoft::CognitiveServices::Speech::Impl;

const IHttpPlatform* GetSingletonInstance()
{
    static std::once_flag m_initOnce;
    static IHttpPlatform* m_httpPlatform = nullptr;

    std::call_once(m_initOnce, []()
        {
            AZACHR res = pal_get_platform((void**)&m_httpPlatform);
            if (res != AZAC_ERR_NONE)
            {
                throw std::runtime_error("Failed to get HTTP platform singleton instance. Error: " + std::to_string(res));
            }
        });

    return m_httpPlatform;
}
}

namespace PAL
{
std::function<void()> HttpPlatform::Init()
{
    return GetSingletonInstance()->Init();
}

void HttpPlatform::Teardown()
{
    GetSingletonInstance()->Teardown();
}

void HttpPlatform::SetLoggingFunction(ExternalLogFunction logFunction)
{
    GetSingletonInstance()->SetLoggingFunction(logFunction);
}
}
