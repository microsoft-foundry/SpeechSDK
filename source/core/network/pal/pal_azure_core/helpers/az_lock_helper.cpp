// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "../stdafx.h"
#include <stdlib.h>
#include <mutex>
#include "az_lock_helper.h"

/**
    NOTE:
    This is a bare minimum re-implementation of the Azure C Shared version of this code that contains the
    minimum drop in replacement functionality that is currently used. This is intended to be a stop gap
    until some code can be re-factored (e.g. audio extension)
*/

struct LockHelper
{
    std::mutex m_mutex;
};

#ifdef __cplusplus
extern "C" {
#endif

    LOCK_HANDLE Lock_Init()
    {
        try
        {
            return new LockHelper();
        }
        catch (...)
        {
            // ignore
        }

        return nullptr;
    }

    LOCK_RESULT Lock_Deinit(LOCK_HANDLE handle)
    {
        try
        {
            if (handle == nullptr)
            {
                return LOCK_RESULT::LOCK_ERROR;
            }
            else
            {
                auto ptr = static_cast<LockHelper*>(handle);
                delete ptr;
                return LOCK_RESULT::LOCK_OK;
            }
        }
        catch (...)
        {
            return LOCK_RESULT::LOCK_ERROR;
        }
    }

    LOCK_RESULT Lock(LOCK_HANDLE handle)
    {
        try
        {
            if (handle)
            {
                auto ptr = static_cast<LockHelper*>(handle);
                ptr->m_mutex.lock();
                return LOCK_RESULT::LOCK_OK;
            }
        }
        catch (const std::exception& ex)
        {
            SPX_TRACE_ERROR("Failed to acquire the lock. Error: %s", ex.what());
        }
        catch (...)
        {
            SPX_TRACE_ERROR("Failed to acquire the lock");
        }

        return LOCK_RESULT::LOCK_ERROR;
    }

    LOCK_RESULT Unlock(LOCK_HANDLE handle)
    {
        try
        {
            if (handle)
            {
                auto ptr = static_cast<LockHelper*>(handle);
                ptr->m_mutex.unlock();
                return LOCK_RESULT::LOCK_OK;
            }
        }
        catch (const std::exception& ex)
        {
            SPX_TRACE_ERROR("Failed to release the lock. Error: %s", ex.what());
        }
        catch (...)
        {
            SPX_TRACE_ERROR("Failed to release the lock");
        }

        return LOCK_RESULT::LOCK_ERROR;
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */
