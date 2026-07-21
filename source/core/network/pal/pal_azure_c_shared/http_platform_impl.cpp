//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include <string>
#include <stdexcept>
#include <array>
#include <assert.h>
#include <limits>

#include "azure_c_shared_utility_platform_wrapper.h"
#include "azure_c_shared_utility_xlogging_wrapper.h"
#include "azure_c_shared_utility_string_token_wrapper.h"
#include "azure_c_shared_utility_httpapi_wrapper.h"
#include "azure_c_shared_utility_urlencode_wrapper.h"
#include "azure_c_shared_utility_uws_client_wrapper.h"

#include "interfaces/i_http_endpoint_info.h"
#include "interfaces/enum_helpers.h"
#include "http_platform_impl.h"
#include "string_utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    template<>
    const char* EnumHelpers::ToString(UriScheme scheme)
    {
        switch (scheme)
        {
        case UriScheme::HTTPS:  return "https://";
        case UriScheme::WSS:    return "wss://";
        case UriScheme::HTTP:   return "http://";
        case UriScheme::WS:     return "ws://";
        case UriScheme::FILE:   return "file://";
        case UriScheme::RTSP:   return "rtsp://";
        case UriScheme::RTSPS:  return "rtsps://";
        }

        assert(!"You are missing UriScheme to string definition");
        return nullptr; // should never get here
    }

    template<>
    bool EnumHelpers::TryParse(const char* string, UriScheme& value)
    {
        ENUM_N_PARSE(UriScheme::HTTPS, "https://")
        ENUM_N_PARSE(UriScheme::WSS, "wss://")
        ENUM_N_PARSE(UriScheme::HTTP, "http://")
        ENUM_N_PARSE(UriScheme::WS, "ws://")
        ENUM_N_PARSE(UriScheme::FILE, "file://")
        ENUM_N_PARSE(UriScheme::RTSP, "rtsp://");
        ENUM_N_PARSE(UriScheme::RTSPS, "rtsps://");

        assert(!"You are missing string to UriScheme definition");
        return false;
    }

    static PAL::HttpPlatform::ExternalLogFunction _logFunction = nullptr;

    template<typename T>
    using deleted_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

    extern "C" void xlogging_log_function_spx_trace_message_wrapper(LOG_CATEGORY log_category, const char* file, const char* func, int line, unsigned int options, const char* format, ...)
    {
        UNUSED(func);
        UNUSED(options);

        if (!_logFunction)
        {
            return;
        }

        va_list args;
        va_start(args, format);

        try
        {
            switch (log_category)
            {
            case AZ_LOG_INFO:
                _logFunction(__SPX_TRACE_LEVEL_INFO, "SPX_TRACE_INFO: AZ_LOG_INFO: ", file, line, format, args);
                break;

            case AZ_LOG_ERROR:
                _logFunction(__SPX_TRACE_LEVEL_ERROR, "SPX_TRACE_ERROR: AZ_LOG_ERROR: ", file, line, format, args);
                break;

            default:
                break;
            }
        }
        catch (...)
        {
            /* don't care */
        }

        va_end(args);
    }

    IHttpPlatform* HttpPlatformImpl::Instance()
    {
        static HttpPlatformImpl instance;
        return &instance;
    }

    std::function<void()> HttpPlatformImpl::Init() const
    {
        static std::once_flag m_initOnce;
        static std::once_flag m_teardownClaimedOnce;
    
        // Initialize the platform once
        std::call_once(m_initOnce, []()
        {
            int ret = platform_init();
            if (ret)
            {
                ThrowRuntimeError("Failed to initialize platform (azure-c-shared). Error: " + std::to_string(ret));
            }
        });

        // Only the first caller gets the teardown function
        std::function<void()> result = nullptr;
        std::call_once(m_teardownClaimedOnce, [this, &result]()
        {
            result = [this]()
            {
                this->Teardown();
            };
        });

        return result;
    }

    void HttpPlatformImpl::Teardown() const
    {
        SPX_TRACE_FUNCTION();
        platform_deinit();
    }

    void HttpPlatformImpl::SetLoggingFunction(PAL::HttpPlatform::ExternalLogFunction logFunction) const
    {
        _logFunction = logFunction;
        xlogging_set_log_function(xlogging_log_function_spx_trace_message_wrapper);
    }
}}}}
