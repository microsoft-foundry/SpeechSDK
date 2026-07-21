//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "http_platform_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    IHttpPlatform* HttpPlatformImpl::Instance()
    {
        static HttpPlatformImpl instance;
        return &instance;
    }

    void HttpPlatformImpl::Init() const
    {
    }

    std::function<void()> HttpPlatformImpl::Teardown() const
    {
        return nullptr;
    }

    void HttpPlatformImpl::SetLoggingFunction(PAL::HttpPlatform::ExternalLogFunction logFunction) const
    {
        (void)logFunction;
    }

    void HttpPlatformImpl::SetProxy(const char* host, uint16_t port, const char* username, const char* password) const
    {
        (void)host;
        (void)port;
        (void)username;
        (void)password;
    }

    bool HttpPlatformImpl::GetProxy(std::string& host, uint16_t& port, std::string& username, std::string& password) const
    {
        (void)host;
        (void)port;
        (void)username;
        (void)password;
        return false;
    }

    std::string HttpPlatformImpl::GetErrorMessage(int32_t errorCode, int32_t additionalErrorCode) const
    {
        (void)errorCode;
        (void)additionalErrorCode;

        return {};
    }

}}}}
