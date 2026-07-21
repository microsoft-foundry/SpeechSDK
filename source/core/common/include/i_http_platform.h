//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <http_platform.h>
#include <functional>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// Interface for the HTTP platform. Only a single instance of this interface will be instantiated
    /// </summary>
    class IHttpPlatform
    {
    public:
        /// <summary>
        /// Initializes the HTTP platform. Will throw exceptions in the case of errors. You should call this once before using
        /// any HTTP/WS 
        /// </summary>
        virtual std::function<void()> Init() const = 0;

        /// <summary>
        /// Tears down the HTTP platform. This should only be called once after you have called Init(). Will throw exceptions
        /// in the case of errors
        /// </summary>
        virtual void Teardown() const = 0;

        /// <summary>
        /// Sets the callback to use for logging from native networking implementation. You should set this once as early as
        /// possible preferable in your main() method or dll load methods. Does not throw exceptions
        /// </summary>
        /// <param name="logFunction">The log function to use</param>
        virtual void SetLoggingFunction(PAL::HttpPlatform::ExternalLogFunction logFunction) const = 0;
    };

}}}}
