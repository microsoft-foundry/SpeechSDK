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

        virtual void Init() const override;
        virtual void Teardown() const override;
        virtual void SetLoggingFunction(PAL::HttpPlatform::ExternalLogFunction logFunction) const override;
        virtual void SetProxy(const char* host, uint16_t port, const char* username = nullptr, const char* password = nullptr) const override;
        virtual bool GetProxy(std::string& host, uint16_t& port, std::string& username, std::string& password) const override;
        virtual std::string GetErrorMessage(int32_t errorCode, int32_t additionalErrorCode = 0) const override;

    private:
        HttpPlatformImpl() = default;
    };

}}}}
