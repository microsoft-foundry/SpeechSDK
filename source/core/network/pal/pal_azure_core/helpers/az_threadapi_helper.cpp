// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <thread>
#include <exception>
#include "az_threadapi_helper.h"

/**
    NOTE:
    This is a bare minimum re-implementation of the Azure C Shared version of this code that contains the
    minimum drop in replacement functionality that is currently used. This is intended to be a stop gap
    until some code can be re-factored (e.g. audio extension)
*/

    struct ThreadWrapper
    {
        ThreadWrapper(std::thread&& thread) : m_thread(std::move(thread)) {}
        ~ThreadWrapper() = default;
        ThreadWrapper(const ThreadWrapper&) = delete;
        ThreadWrapper(ThreadWrapper&&) = delete;

        std::thread m_thread;
    };

#ifdef __cplusplus
extern "C" {
#endif

    THREADAPI_RESULT ThreadAPI_Create(THREAD_HANDLE* threadHandle, THREAD_START_FUNC func, void* arg)
    {
        if (threadHandle == nullptr || func == nullptr)
        {
            return THREADAPI_RESULT::THREADAPI_INVALID_ARG;
        }

        try
        {
            *threadHandle = new ThreadWrapper(std::thread(func, arg));
            return THREADAPI_RESULT::THREADAPI_OK;
        }
        catch (...)
        {
            return THREADAPI_RESULT::THREADAPI_ERROR;
        }
    }

    THREADAPI_RESULT ThreadAPI_Join(THREAD_HANDLE threadHandle, int*)
    {
        THREADAPI_RESULT ret;
        ThreadWrapper* wrapper = nullptr;

        try
        {
            wrapper = reinterpret_cast<ThreadWrapper*>(threadHandle);

            if (wrapper == nullptr)
            {
                ret = THREADAPI_RESULT::THREADAPI_INVALID_ARG;
            }
            else if (!wrapper->m_thread.joinable())
            {
                ret = THREADAPI_RESULT::THREADAPI_ERROR;
            }
            else
            {
                wrapper->m_thread.join();
                ret = THREADAPI_RESULT::THREADAPI_OK;
            }
        }
        catch (...)
        {
            ret = THREADAPI_RESULT::THREADAPI_ERROR;
        }

        // finally free the memory since we don't have an explicit free/release method
        if (wrapper)
        {
            delete wrapper;
        }

        return ret;
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */
