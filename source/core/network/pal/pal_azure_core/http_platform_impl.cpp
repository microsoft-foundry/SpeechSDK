//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "http_platform_impl.h"
#include "azure/core/diagnostics/logger.hpp"
#include "azure/core/url.hpp"
#include "http_utils.h"
#include "string_utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    using namespace Azure::Core;
    using namespace Azure::Core::Diagnostics;

    IHttpPlatform* HttpPlatformImpl::Instance()
    {
        static HttpPlatformImpl instance;
        return &instance;
    }

    std::function<void()> HttpPlatformImpl::Init() const
    {
        // Nothing to see here, move along
        return nullptr;
    }

    void HttpPlatformImpl::Teardown() const
    {
        // Nothing to see here, move along
    }

    extern "C" void InternalLogFunction(PAL::HttpPlatform::ExternalLogFunction logFunction, int level, const char* title, const char* format, ...)
    {
        va_list args;
        va_start(args, format);

        try
        {
            logFunction(level, title, "", 0, format, args);
        }
        catch (...) {}

        va_end(args);
    }

    void HttpPlatformImpl::SetLoggingFunction(PAL::HttpPlatform::ExternalLogFunction logFunction) const
    {
        Logger::SetListener([logFunction](Logger::Level logLevel, const std::string& msg)
        {
            const char* title = "";
            int level = 0;

            switch (logLevel)
            {
            case Logger::Level::Error:
                level = __AZAC_TRACE_LEVEL_ERROR;
                title = "AZ_CORE_LOG_ERROR : ";
                break;

            case Logger::Level::Informational:
                level = __AZAC_TRACE_LEVEL_INFO;
                title = "AZ_CORE_LOG_INFO : ";
                break;

            case Logger::Level::Warning:
                level = __AZAC_TRACE_LEVEL_WARNING;
                title = "AZ_CORE_LOG_WARNING : ";
                break;


            case Logger::Level::Verbose:
#if _DEBUG
                level = __AZAC_TRACE_LEVEL_VERBOSE;
                title = "AZ_CORE_LOG_VERBOSE : ";
                break;
#else
                return;
#endif
            }

            InternalLogFunction(logFunction, level, title, "%s", msg.c_str());
        });

        Logger::SetLevel(Logger::Level::Verbose);
    }
}}}}
