//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <future>
#include <chrono>

#include "interfaces/base.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE(ISpxThreadService)
{
    public:
    using TaskId = int;

    enum class Affinity
    {
        User = 0,
        Background = 1,
        Media = 2
    };

    // Asynchronously execute a task on a thread. All tasks scheduled with the same affinity using this function
    // are executed in FIFO order.
    //
    // Optional 'executed' promise can be used to be notified about task execution:
    //    true: if the task has been successfully executed
    //    false: if the task has been cancelled
    //    exception: if there was an exception during scheduling
    virtual TaskId ExecuteAsync(std::packaged_task<void()>&& task,
        Affinity affinity = Affinity::Background,
        std::promise<bool>&& executed = std::promise<bool>()) = 0;

    // Asynchronously execute a task on a thread with a delay 'count' number of times.
    //
    // Optional 'executed' promise can be used to be notified about task execution:
    //    true: if the task has been successfully executed
    //    false: if the task has been cancelled
    //    exception: if there was an exception during scheduling
    virtual TaskId ExecuteAsync(std::packaged_task<void()>&& task,
        std::chrono::milliseconds delay,
        Affinity affinity = Affinity::Background,
        std::promise<bool>&& executed = std::promise<bool>()) = 0;

    // Execute a task on a thread synchronously blocking the caller.
    virtual void ExecuteSync(std::packaged_task<void()>&& task,
        Affinity affinity = Affinity::Background) = 0;

    // Cancels the task. If the task is canceled,
    // the corresponding 'canceled' promise is fulfilled.
    virtual bool Cancel(TaskId id) = 0;

    // Cancels all tasks.
    virtual void CancelAllTasks() = 0;

    virtual bool IsOnThread(Affinity affinity) const = 0;

    virtual bool IsOnServiceThread() const = 0;
};

} } } }
