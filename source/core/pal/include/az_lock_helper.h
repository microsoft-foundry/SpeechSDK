// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AZ_LOCK_HELPER_H
#define AZ_LOCK_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef void* LOCK_HANDLE;

    enum LOCK_RESULT
    {
        LOCK_OK,
        LOCK_ERROR
    };

    /**
     * @brief   This API creates and returns a valid lock handle.
     *
     * @return  A valid @c LOCK_HANDLE when successful or @c NULL otherwise.
     */
    LOCK_HANDLE Lock_Init();

    /**
     * @brief   The lock instance is destroyed.
     *
     * @param   handle      A valid handle to the lock.
     *
     * @return  Returns @c LOCK_OK when the lock object has been
     *          destroyed and @c LOCK_ERROR when an error occurs.
     */
    LOCK_RESULT Lock_Deinit(LOCK_HANDLE handle);

    /**
     * @brief   Acquires a lock on the given lock handle. Uses platform
     *          specific mutex primitives in its implementation.
     *
     * @param   handle      A valid handle to the lock.
     *
     * @return  Returns @c LOCK_OK when a lock has been acquired and
     *          @c LOCK_ERROR when an error occurs.
     */
    LOCK_RESULT Lock(LOCK_HANDLE handle);

    /**
     * @brief   Releases the lock on the given lock handle. Uses platform
     *          specific mutex primitives in its implementation.
     *
     * @param   handle      A valid handle to the lock.
     *
     * @return  Returns @c LOCK_OK when the lock has been released and
     *          @c LOCK_ERROR when an error occurs.
     */
    LOCK_RESULT Unlock(LOCK_HANDLE handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AZ_LOCK_HELPER_H */
