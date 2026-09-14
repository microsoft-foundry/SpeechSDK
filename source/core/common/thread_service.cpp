//
// Copyright (c) Microsoft. All rights reserved.
// See LICENSE.md file in the project root for full license information.
//
#include <atomic>

#include "stdafx.h"
#include "thread_service.h"
#include "try_catch_helpers.h"
#include "speechapi_cxx_utils.h"

using namespace std::chrono_literals;

constexpr auto stopTimeoutInMilliseconds = 1000ms;

namespace Microsoft { namespace CognitiveServices { namespace Speech { namespace Impl {

    // Debug variable for tracking the size of the ThreadService BACKGROUND THREAD task queue
    // NOTE: This can be misleading when there is more than one thread service active (e.g. you have
    //       more than one recognizer running at once)
    std::atomic<int> gTaskQueueSize{ 0 };


    void CSpxThreadService::Init()
    {
    }

    CSpxThreadService::~CSpxThreadService()
    {
        std::string error;
        SPXAPI_TRY()
        {
            Term();
        }
        SPXAPI_CATCH_ONLY()
    }

    void CSpxThreadService::Term()
    {
        {
#if !defined(EMSCRIPTEN)
            std::unique_lock<std::mutex> lock(m_mutex);
#endif
            m_terminated = true;
            SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
            if (m_threads.empty())
            {
                return;
            }
        }

        for (auto& entry : m_threads)
        {
            const auto& affinity = entry.first;
            auto& thread = entry.second;
            if (thread)
            {
                // Currently, stopping the user thread as detached.
                thread->Stop(affinity == Affinity::User);
            }
            else
            {
                // Handle the case where the thread entry is invalid
                // (thread object is null or not initialized correctly)
                SPX_TRACE_WARNING("Thread entry is invalid when terminating the thread service.");
            }
        }
    }

    constexpr const char * AffinityName(ISpxThreadService::Affinity affinity)
    {
        switch (affinity)
        {
            case ISpxThreadService::Affinity::Background: return "Background";
            case ISpxThreadService::Affinity::User: return "User";
            case ISpxThreadService::Affinity::Media: return "Media";
            default: return nullptr;
        }
    }

    CSpxThreadService::ThreadPtr CSpxThreadService::EnsureThreadInitialized(Affinity affinity)
    {
#if !defined(EMSCRIPTEN)
        std::unique_lock<std::mutex> lock(m_mutex);
#endif
        if (m_terminated) return nullptr;
        auto it = m_threads.find(affinity);
        if (it == m_threads.end())
        {
            auto newThread = std::make_shared<Thread>();
            /* Not found, let's create the thread */
            m_threads.emplace(affinity, newThread);
            /* TODO: Do we need an extra call? */
            newThread->Start();
            auto threadHash = std::hash<std::thread::id>{}(newThread->Id()) % 1000000;
            SPX_TRACE_INFO("Started thread %s with ID [%lu]", AffinityName(affinity), (unsigned long)threadHash);
            return newThread;
        }
        else
        {
            return it->second;
        }
    }

    CSpxThreadService::TaskId CSpxThreadService::ExecuteAsync(std::packaged_task<void()>&& task, Affinity affinity, std::promise<bool>&& executed)
    {
        auto thread = EnsureThreadInitialized(affinity);

        auto taskId = m_nextTaskId++;
        if (thread)
        {
           auto innerTask = std::make_shared<Task>(taskId, std::move(task));
           thread->Queue(innerTask, std::move(executed));
        }
        return taskId;
    }

    CSpxThreadService::TaskId CSpxThreadService::ExecuteAsync(std::packaged_task<void()>&& task, std::chrono::milliseconds delay, Affinity affinity, std::promise<bool>&& executed)
    {
        auto thread = EnsureThreadInitialized(affinity);

        // If 0 or negative delay, treat like normal
        if (delay <= 0ms)
        {
            return ExecuteAsync(std::move(task), affinity, std::move(executed));
        }

        auto taskId = m_nextTaskId++;
        if (thread)
        {
            auto innerTask = std::make_shared<DelayTask>(taskId, std::move(task), delay);
            thread->Queue(innerTask, std::move(executed));
        }
        return taskId;
    }

    bool CSpxThreadService::IsOnServiceThread() const
    {
        // see if current thread is managed by thread pool
        auto id = std::this_thread::get_id();

        for (const auto& t : m_threads) {
            if (id == t.second->Id())
                return true;
        }

        return false;
    }

    bool CSpxThreadService::IsOnThread(Affinity affinity) const
    {
        auto it = m_threads.find(affinity);
        if (it != m_threads.end())
        {
            const auto id = std::this_thread::get_id();
            return it->second->Id() == id;
        }
        return false;
    }

    void CSpxThreadService::ExecuteSync(std::packaged_task<void()>&& task, Affinity affinity)
    {
        // are we trying to schedule synchronous work on our own thread from the same thread?
        if (IsOnThread(affinity))
        {
            SPX_TRACE_ERROR("Task cannot be executed synchronously on the thread"
                " from the thread service in order to avoid potential deadlocks.");
            SPX_THROW_HR(SPXERR_ABORT);
        }

        auto future = task.get_future();
        std::promise<bool> executed;
        auto executedFuture = executed.get_future();
        ExecuteAsync(std::move(task), affinity, std::move(executed));

        if (executedFuture.get())
        {
            future.get();
        }
    }

    bool CSpxThreadService::Cancel(CSpxThreadService::TaskId id)
    {
        for (auto& t : m_threads)
        {
            if (t.second->Cancel(id))
            {
                return true;
            }
        }
        return false;
    }

    void CSpxThreadService::CancelAllTasks()
    {
        for (auto& t : m_threads)
        {
            t.second->CancelAllTasks();
        }
    }

    void CSpxThreadService::Task::Run()
    {
        m_state = State::Running;
        m_task();
        m_state = State::Finished;
    }

    void CSpxThreadService::Thread::Start()
    {
        if (m_started.exchange(true))
        {
            SPX_TRACE_ERROR("Thread has already been started");
            SPX_THROW_HR(SPXERR_INVALID_STATE);
        }

        m_thread = std::thread(CSpxThreadService::Thread::WorkerLoop, shared_from_this());
    }

    void CSpxThreadService::Thread::Stop(bool forceDetach)
    {
        // If we never started, or, we're stopped, or stopping, we're done
        if (!m_started || m_stopped || m_shouldStop)
        {
            return;
        }

        // Cancel all pending tasks
        auto guard = Utils::MakeScopeGuard([&] { CancelAllTasks(); });

        // This happens when the object that OWNS this thread service is destroyed from within a
        // task the worker is currently running -- i.e. the owner's last reference is released on
        // this worker thread, so its destructor (and therefore this Stop() call) execute here.
        // Any thread service owner can hit this; a TTS synthesizer destroyed inside one of its own
        // event callbacks is just the case we first observed.
        //
        // We cannot join() here: a thread cannot wait for itself to finish (self-join is undefined
        // behavior). We also cannot throw: Stop() is reached from a noexcept destructor, so any
        // exception would terminate the process.
        //
        // So we detach instead. Detaching is safe because the worker was started holding a
        // shared_ptr to itself (shared_from_this(), see Start()), which keeps the Thread object
        // alive until the loop finishes on its own. The worker loop never accesses the owner, so
        // the owner can be torn down as soon as this call returns.
        auto isThisThread = m_thread.get_id() == std::this_thread::get_id();
        if (isThisThread && !forceDetach)
        {
            SPX_TRACE_WARNING("Thread is being stopped from its own task; detaching to avoid self-join.");
            forceDetach = true;
        }

        // Notify the WorkerLoop to stop
        if (m_shouldStop.exchange(true)) // atomically handle race
        {
            return;
        }

        m_cv.notify_all();

        // Wait for the WorkerLoop to finish
        if (!isThisThread)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, stopTimeoutInMilliseconds, [&] { return m_stopped.load(); });
        }

        // Detach or join
        if (forceDetach)
        {
            m_thread.detach();
        }
        else if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    void CSpxThreadService::Thread::Queue(TaskPtr task, std::promise<bool>&& executed)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_failed)
        {
            SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
        }

        // Make sure we do not schedule new tasks
        // if the thread is being stopped.
        if (m_shouldStop)
        {
            task->MarkCanceled();
            return;
        }

        m_tasks.push_back({ task, std::move(executed) });
        m_cv.notify_all();
    }

    void CSpxThreadService::Thread::Queue(DelayTaskPtr task, std::promise<bool>&& executed)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_failed)
            SPX_THROW_HR(SPXERR_RUNTIME_ERROR);

        // Make sure we do not schedule new tasks
        // if the thread is being stopped.
        if (m_shouldStop)
        {
            task->MarkCanceled();
            return;
        }

        AddDelayTaskAtProperPlace(task, std::move(executed));
        m_cv.notify_all();
    }

    bool CSpxThreadService::Thread::Cancel(TaskId id)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return CancelTask(m_tasks, id) ||
               CancelTask(m_timerTasks, id);
    }

    void CSpxThreadService::Thread::CancelAllTasks()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        // Mark all tasks as canceled.
        MarkAllTasksCancelled(m_tasks);
        MarkAllTasksCancelled(m_timerTasks);

        // Remove all.
        RemoveAllTasks();
    }

    void CSpxThreadService::Thread::MarkFailed(const std::exception_ptr& e)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_failed = true;
        MarkAllTasksFailed(m_tasks, e);
        MarkAllTasksFailed(m_timerTasks, e);
        RemoveAllTasks();
    }

    void CSpxThreadService::Thread::RemoveAllTasks()
    {
        m_tasks.clear();
        m_timerTasks.clear();
    }

    void CSpxThreadService::Thread::AddDelayTaskAtProperPlace(DelayTaskPtr task, std::promise<bool>&& executed)
    {
        if (task->CurrentState() == Task::State::Failed)
        {
            // We cannot reschedule a failed task.
            return;
        }

        task->UpdateWhen();
        DelayTaskAndPromise value{ task, std::move(executed) };
        auto place = upper_bound(m_timerTasks.begin(), m_timerTasks.end(), value,
            [](const DelayTaskAndPromise& a, const DelayTaskAndPromise& b) { return (a.first)->When() < (b.first)->When(); });

        if (place == m_timerTasks.end())
        {
            m_timerTasks.push_back({ task, std::move(value.second) });
        }
        else
        {
            m_timerTasks.insert(place, { task, std::move(value.second) });
        }
    }


    void CSpxThreadService::Thread::WorkerLoop(std::shared_ptr<Thread> self)
    {
        auto guard = Utils::MakeScopeGuard([&] {
            self->m_stopped = true;
            self->m_cv.notify_all();
        });

        try
        {
            const int MaxSlice = 10;
            auto waitForTasksTimeout = 1000ms;
            std::unique_lock<std::mutex> lock(self->m_mutex);

            while (!self->m_shouldStop)
            {
                // Execute all tasks that have been scheduled,
                // but not more than slice tasks to avoid timer tasks starvation.
                int sliceCounter = 0;
                while (!self->m_tasks.empty() && sliceCounter++ < MaxSlice)
                {
                    self->RunTask(lock, self->m_tasks);

                    if (self->m_shouldStop)
                        return;

                    lock.lock();
                }

                // Execute timer tasks if required,
                // but not more than slice tasks to avoid immediate tasks starvation.
                sliceCounter = 0;
                while (!self->m_timerTasks.empty() &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(self->m_timerTasks.front().first->When() - std::chrono::system_clock::now()).count() <= 0 &&
                    sliceCounter++ < MaxSlice)
                {
                    self->RunTask(lock, self->m_timerTasks);

                    if (self->m_shouldStop)
                        return;

                    lock.lock();
                }

                waitForTasksTimeout = 200ms;
                if (!self->m_timerTasks.empty())
                {
                    waitForTasksTimeout = std::min(std::chrono::duration_cast<std::chrono::milliseconds>(
                        self->m_timerTasks.front().first->When() - std::chrono::system_clock::now()), waitForTasksTimeout);
                }

                // Continue if there is some delay task that should be executed.
                if (waitForTasksTimeout.count() <= 0)
                    continue;

                // Sleeping for the delay till there is some task or we are asked to stop.
                if(self->m_tasks.empty() && !self->m_shouldStop)
                {
                    self->m_cv.wait_for(lock, waitForTasksTimeout, [&]()
                    {
                        return self->m_shouldStop.load() || !self->m_tasks.empty();
                    });
                }
            }
        }
        // If we are in the exception handler, something really bad happened.
        // Not the task, but some of our structures misbehaved and we are in unknown state.
        // We cannot really recover, because this can lead to live locks.
        // We abort all outstanding promises and set the whole thread to invalid state,
        // so that the next access to the thread object will result in an error.
        catch (const std::exception& e)
        {
            SPX_TRACE_ERROR("Exception caused termination of the thread service: %s", e.what());
            self->MarkFailed(std::current_exception());
        }
#ifdef THREAD_SERVICE_HANDLE_FORCED_UNWIND
        // Currently Python forcibly kills the thread by throwing __forced_unwind,
        // taking care we propagate this exception further.
        catch (abi::__forced_unwind&)
        {
            SPX_TRACE_ERROR("Caught forced unwind in a thread service thread, rethrowing");
            self->MarkFailed(std::make_exception_ptr(std::runtime_error("Forced unwind")));
            self->CancelAllTasks();
            throw;
        }
#endif
        catch (...)
        {
            SPX_TRACE_ERROR("Unknown exception happened during task execution");
            self->MarkFailed(std::current_exception());
        }

        self->CancelAllTasks();
    }

}}}} // Microsoft::CognitiveServices::Speech::Impl
