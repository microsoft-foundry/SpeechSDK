//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include <atomic>
#include "spxcore_common.h"
#include "interface_delegate_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    extern std::atomic<int> gTaskQueueSize;

template <typename DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxThreadService>>
class CSpxThreadServiceDelegateHelper : public DelegateToHelperT
{
private:

    using I = ISpxThreadService;
    using C = CSpxThreadServiceDelegateHelper<DelegateToHelperT>;

public:

    SPX_DELEGATE_ACCESSORS(ThreadService, DelegateToHelperT, ISpxThreadService)

    I::TaskId DelegateExecuteAsync(std::packaged_task<void()>&& task, I::Affinity affinity = I::Affinity::Background, std::promise<bool>&& executed = std::promise<bool>())
    {
        I::TaskId (I::*pfn)(std::packaged_task<void()>&&, I::Affinity, std::promise<bool>&&) = &I::ExecuteAsync;
        return InvokeOnDelegateR(C::GetDelegate(), pfn, -1, std::move(task), affinity, std::move(executed));
    }

    I::TaskId DelegateExecuteAsync(std::packaged_task<void()>&& task, std::chrono::milliseconds delay, I::Affinity affinity = I::Affinity::Background, std::promise<bool>&& executed = std::promise<bool>())
    {
        I::TaskId (I::*pfn)(std::packaged_task<void()>&&, std::chrono::milliseconds, I::Affinity, std::promise<bool>&&) = &I::ExecuteAsync;
        return InvokeOnDelegateR(C::GetDelegate(), pfn, -1, std::move(task), delay, affinity, std::move(executed));
    }

    void DelegateExecuteSync(std::packaged_task<void()>&& task, I::Affinity affinity = I::Affinity::Background)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::ExecuteSync, std::move(task), affinity);
    }

    bool DelegateCancel(I::TaskId id)
    {
        return InvokeOnDelegateR(C::GetDelegate(), &I::Cancel, false, id);
    }

    void DelegateCancelAllTasks()
    {
        InvokeOnDelegate(C::GetDelegate(), &I::CancelAllTasks);
    }

    I::TaskId DelegateExecuteAsync(std::function<void()> func, I::Affinity affinity = I::Affinity::Background)
    {
        auto task = std::packaged_task<void()>(func);
        return DelegateExecuteAsync(std::move(task), affinity);
    }

    I::TaskId DelegateExecuteAsync(std::function<void()> func, std::chrono::milliseconds delay, I::Affinity affinity = I::Affinity::Background)
    {
        auto task = std::packaged_task<void()>(func);
        return DelegateExecuteAsync(std::move(task), delay, affinity);
    }

    void DelegateExecuteSync(std::function<void()> func, I::Affinity affinity = I::Affinity::Background)
    {
        auto task = std::packaged_task<void()>(func);
        DelegateExecuteSync(std::move(task), affinity);
    }

    bool DelegateIsOnThread(I::Affinity affinity) const
    {
        return InvokeOnDelegateR(C::GetConstDelegate(), &I::IsOnThread, false, affinity);
    }

    bool DelegateIsOnServiceThread() const
    {
        return InvokeOnDelegateR(C::GetConstDelegate(), &I::IsOnServiceThread, false);
    }

};

#define SPX_THREAD_SERVICE_DELEGATE_TASK_BEGIN(TKeepAlive, affinity, pszFuncName, lineNum)  \
{                                                                                           \
    std::string funcName(pszFuncName);                                                      \
    SPX_DBG_TRACE_VERBOSE("[%p][Enqueue] ThreadService %s. Size: %d, Source: %s (%d)", (void*)static_cast<TKeepAlive*>(this), affinity, ++gTaskQueueSize, funcName.c_str(), lineNum); \
    std::weak_ptr<TKeepAlive> weak = SpxSharedPtrFromThis<TKeepAlive>(this);                \
    DelegateExecuteAsync([=](){                                                             \
        SPX_DBG_TRACE_SCOPE(funcName.c_str(), funcName.c_str());                            \
        auto keepAlive = weak.lock();                                                       \
        SPX_DBG_TRACE_VERBOSE("[%p][Dequeue] ThreadService %s. Size: %d, Source: %s (%d)", (void*)keepAlive.get(), affinity, --gTaskQueueSize, funcName.c_str(), lineNum);\
        if (keepAlive) {                                                                    \

#define SPX_THREAD_SERVICE_DELEGATE_BACKGROUND_TASK_BEGIN(x)                                \
    SPX_THREAD_SERVICE_DELEGATE_TASK_BEGIN(x, "Background", __FUNCTION__, __LINE__)

#define SPX_THREAD_SERVICE_DELEGATE_BACKGROUND_TASK_END()                                   \
    }}, ISpxThreadService::Affinity::Background); }

#define SPX_THREAD_SERVICE_DELEGATE_USER_TASK_BEGIN(x)                                      \
    SPX_THREAD_SERVICE_DELEGATE_TASK_BEGIN(x, "User", __FUNCTION__, __LINE__)

#define SPX_THREAD_SERVICE_DELEGATE_USER_TASK_END()                                         \
    }}, ISpxThreadService::Affinity::User); }

#define SPX_THREAD_SERVICE_DELEGATE_MEDIA_TASK_BEGIN(x)                                \
    SPX_THREAD_SERVICE_DELEGATE_TASK_BEGIN(x, "Media", __FUNCTION__, __LINE__)

#define SPX_THREAD_SERVICE_DELEGATE_MEDIA_TASK_END()                                   \
    }}, ISpxThreadService::Affinity::Media); }

} } } } // Microsoft::CognitiveServices::Speech::Impl
