//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class HttpBindingPlatformImpl : public IHttpPlatform
{
public:
    static IHttpPlatform* Instance();

    virtual ~HttpBindingPlatformImpl() = default;

    virtual std::function<void()> Init() const override;
    virtual void Teardown() const override;
    virtual void SetLoggingFunction(PAL::HttpPlatform::ExternalLogFunction logFunction) const override;

private:
    HttpBindingPlatformImpl() = default;
};
}}}}
