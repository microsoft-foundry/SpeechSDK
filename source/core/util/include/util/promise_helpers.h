//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <exception>
#include <future>
#include <atomic>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// Helpers to make it easier to work with promises
    /// </summary>
    class PromiseHelpers
    {
    public:
        /// <summary>
        /// Gets a future that has already completed
        /// </summary>
        /// <typeparam name="TRet">The value type of the future</typeparam>
        /// <param name="value">The value to set on the promise</param>
        /// <returns>The future that is immediately completed with the specified value</returns>
        template<typename TRet, typename = std::enable_if_t<!std::is_void<TRet>::value>>
        static std::future<TRet> CompletedFuture(TRet&& value)
        {
            std::promise<TRet> promise;
            promise.set_value(std::forward<TRet>(value));
            return promise.get_future();
        }

        /// <summary>
        /// Gets a future that has already completed
        /// </summary>
        /// <returns>The future that is immediately completed with the specified value</returns>
        template<typename TRet = void, typename = std::enable_if_t<std::is_void<TRet>::value>>
        static std::future<TRet> CompletedFuture()
        {
            std::promise<TRet> promise;
            promise.set_value();
            return promise.get_future();
        }

        /// <summary>
        /// Gets a future that has already failed
        /// </summary>
        /// <typeparam name="TRet">The value type of the future</typeparam>
        /// <param name="ex">The exception to fail the promise with</param>
        /// <returns>A future that is immediately failed with the specified exception</returns>
        template<typename TRet = void>
        static std::future<TRet> FailedFuture(const std::exception& ex)
        {
            std::promise<TRet> promise;
            auto pEx = std::make_exception_ptr(ex);
            promise.set_exception(pEx);
            return promise.get_future();
        }

        /// <summary>
        /// Gets a future that has already failed
        /// </summary>
        /// <typeparam name="TRet">The value type of the future</typeparam>
        /// <param name="pEx">The exception pointer to fail the promise with</param>
        /// <returns>A future that is immediately failed with the specified exception</returns>
        template<typename TRet = void>
        static std::future<TRet> FailedFuture(std::exception_ptr pEx)
        {
            std::promise<TRet> promise;
            promise.set_exception(pEx);
            return promise.get_future();
        }
    };

}}}}
