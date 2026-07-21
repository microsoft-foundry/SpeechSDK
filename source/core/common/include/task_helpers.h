//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <future>
#include <memory>
#include <chrono>

#include "interfaces/thread_service.h"
#include "interface_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template<typename F>
void RunSyncOnThreadService(ISpxThreadService::Ptr threadService, F fn, ISpxThreadService::Affinity affinity = ISpxThreadService::Affinity::Background)
{
    if (threadService->IsOnThread(affinity))
    {
        fn();
    }
    else
    {
        threadService->ExecuteSync(std::packaged_task<void()>{ [&]()
        {
            fn();
        } }, affinity);
    }
}

template<typename F>
void RunAsync(ISpxThreadService::Ptr threadService, F fn, ISpxThreadService::Affinity affinity = ISpxThreadService::Affinity::Background)
{
    threadService->ExecuteAsync(std::packaged_task<void()>{ [fn = std::move(fn)]()
    {
        fn();
    } }, affinity);
}

template<typename F>
void RunAsyncWithDelay(ISpxThreadService::Ptr threadService, F fn, std::chrono::milliseconds delay, ISpxThreadService::Affinity affinity = ISpxThreadService::Affinity::Background)
{
    threadService->ExecuteAsync(std::packaged_task<void()>{ [fn = std::move(fn)]()
    {
        fn();
    } }, delay, affinity);
}

template<typename F, typename T>
void RunAsyncWithKeepAlive(ISpxThreadService::Ptr threadService, std::shared_ptr<T> keepAlive, F fn, ISpxThreadService::Affinity affinity = ISpxThreadService::Affinity::Background)
{
    threadService->ExecuteAsync(std::packaged_task<void()>{ [fn = std::move(fn), keepAlive]()
    {
        fn();
    } }, affinity);
}

// Change to use std::latch after upgrade to C++20
// https://en.cppreference.com/w/cpp/thread/latch
class CountDownLatch {
public:
    explicit CountDownLatch(const unsigned int count): m_count(count) { }
    virtual ~CountDownLatch() = default;

    void Await(void) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_count > 0) {
            m_cv.wait(lock, [this](){ return m_count == 0; });
        }
    }

    template <class Rep, class Period>
    bool Await(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        bool result = true;
        if (m_count > 0) {
            result = m_cv.wait_for(lock, timeout, [this](){ return m_count == 0; });
        }

        return result;
    }

    void CountDown(void) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_count > 0) {
            m_count--;
            m_cv.notify_all();
        }
    }

    unsigned int GetCount(void) {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_count;
    }

protected:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    unsigned int m_count = 0;
};

} } } }
