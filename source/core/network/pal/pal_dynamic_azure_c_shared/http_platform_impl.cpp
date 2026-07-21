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

    std::function<void()> HttpPlatformImpl::Init() const
    {
        return nullptr;
    }

    void HttpPlatformImpl::Teardown() const
    {
    }

    void HttpPlatformImpl::SetLoggingFunction(PAL::HttpPlatform::ExternalLogFunction logFunction) const
    {
        (void)logFunction;
    }
}}}}
