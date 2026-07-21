//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "http_platform_impl.h"
#include "http_status_codes.h"
#include <stdexcept>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>

//--INFO ON DEBUGGING MACROS --//

// SPX_TRACE_INFO("HttpBindingPlatformImpl::Init called"); //have more text to add, print variables
// SPX_TRACE_FUNCTION(); //check if in function
// SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__); //enter and exit function -> good for 
// SPX_TRACE_WARNING ( string message);
// SPX_TRACE_ERROR("[0x%p] Web socket close failed. Details: %s", this, ex.what());


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

// Static variables
static PAL::HttpPlatform::ExternalLogFunction g_logFunction = nullptr;

// Singleton instance getter
IHttpPlatform* HttpBindingPlatformImpl::Instance()
{
    static HttpBindingPlatformImpl instance;
    return &instance;
}

// Initialize the HTTP platform - LOGS
std::function<void()> HttpBindingPlatformImpl::Init() const
{
    SPX_TRACE_FUNCTION("HttpBindingPlatformImpl::Init()");
    return nullptr;
}

// Clean up the HTTP platform - LOGS
void HttpBindingPlatformImpl::Teardown() const
{
    SPX_TRACE_FUNCTION("HttpBindingPlatformImpl::Teardown()");
}

// Set the logging function
void HttpBindingPlatformImpl::SetLoggingFunction(PAL::HttpPlatform::ExternalLogFunction logFunction) const
{
    g_logFunction = logFunction;

    if (g_logFunction != nullptr)
    {
        SPX_TRACE_INFO("External logging function has been set.");
    }
    else
    {
        SPX_TRACE_WARNING("Logging function is null. Logging will be disabled.");
    }

}

} // namespace Impl
} // namespace Speech
} // namespace CognitiveServices
} // namespace Microsoft


// C ABI Bridge Functions
extern "C" {

    // Initialize the HTTP platform
    void HTTP_Platform_Init()
    {
        Microsoft::CognitiveServices::Speech::Impl::HttpBindingPlatformImpl::Instance()->Init();
    }

    // Teardown the HTTP platform
    void HTTP_Platform_Teardown()
    {
        Microsoft::CognitiveServices::Speech::Impl::HttpBindingPlatformImpl::Instance()->Teardown();
    }

    // Set logging function
    void HTTP_Platform_SetLoggingFunction(void (*logFunction)(int level, const char* title, const char* file, int line, const char* format, va_list args))
    {
        Microsoft::CognitiveServices::Speech::Impl::HttpBindingPlatformImpl::Instance()->SetLoggingFunction(logFunction);
    }
}
