//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "i_http_platform.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    class HttpPlatformImpl : public IHttpPlatform
    {
    public:
        static IHttpPlatform* Instance();

        virtual ~HttpPlatformImpl() = default;

        virtual std::function<void()> Init() const override;
        virtual void Teardown() const override;
        virtual void SetLoggingFunction(PAL::HttpPlatform::ExternalLogFunction logFunction) const override;

    private:
        HttpPlatformImpl() = default;
    };

}}}}
