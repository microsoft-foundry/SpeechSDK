//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once
#include <future>
#include <memory>
#include <spxdebug.h>
#include "interfaces/types.h"
#if defined(EMSCRIPTEN)
#include <emscripten.h>
#endif

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


enum AsyncOpState { AOS_Started, AOS_Completed, AOS_Canceled, AOS_Error };

template<class T>
class CSpxAsyncOp
{
    public:
    template<typename U = T, typename = std::enable_if_t<!std::is_void<U>::value>>
    static CSpxAsyncOp<U> FromResult(U&& result)
    {
        std::promise<U> promise;
        std::shared_future<U> future{ promise.get_future() };
        promise.set_value(std::forward<U>(result));
        return CSpxAsyncOp{ future, AOS_Completed };
    }

    template<typename U = T, typename = std::enable_if_t<std::is_void<U>::value>>
    static CSpxAsyncOp<U> FromResult()
    {
        std::promise<U> promise;
        std::shared_future<U> future{ promise.get_future() };
        promise.set_value();
        return CSpxAsyncOp{ future, AOS_Completed };
    }

    CSpxAsyncOp(CSpxAsyncOp&& other) = default;

    CSpxAsyncOp(std::shared_future<T>& future, AsyncOpState state) :
        Future(future),
        State(state)
    {
    };

    ~CSpxAsyncOp() = default;

    bool WaitFor(uint32_t milliseconds)
    {
        bool completed = false;
#if defined(EMSCRIPTEN)
        const uint32_t millisecondsWaitUnit = 10;   //  > 60fps
        for (uint32_t millisecondsWait = 0; (millisecondsWait < milliseconds) && (!completed); millisecondsWait += millisecondsWaitUnit)
#endif
        {
#if defined(EMSCRIPTEN)
            // TODO account for yielded time using wait_until instead of wait_for
            emscripten_sleep(millisecondsWaitUnit);
            auto status = this->Future.wait_for(std::chrono::milliseconds(0));
#else
            auto status = this->Future.wait_for(std::chrono::milliseconds(milliseconds));
#endif
            switch (status)
            {
            case std::future_status::ready:
                completed = true;
                break;

            case std::future_status::timeout:
                completed = false;
                break;

            default:
            case std::future_status::deferred:
                SPX_THROW_HR(SPXERR_NOT_IMPL);
                break;
            }
        }

        return completed;
    }

    std::shared_future<T> Future;
    AsyncOpState State;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
