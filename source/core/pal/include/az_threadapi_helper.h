// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AZ_THREADAPI_HELPER_H
#define AZ_THREADAPI_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef int(*THREAD_START_FUNC)(void*);

    /** @brief Enumeration specifying the possible return values for the APIs in
     *         this module.
     */
    enum THREADAPI_RESULT
    {
        THREADAPI_OK,
        THREADAPI_INVALID_ARG,
        THREADAPI_NO_MEMORY,
        THREADAPI_ERROR
    };

    typedef void* THREAD_HANDLE;

    /**
     * @brief   Creates a thread with the entry point specified by the @p func
     *          argument.
     *
     * @param   threadHandle    The handle to the new thread is returned in this
     *                          pointer.
     * @param   func            A function pointer that indicates the entry point
     *                          to the new thread.
     * @param   arg             A void pointer that must be passed to the function
     *                          pointed to by @p func.
     *
     * @return  @c THREADAPI_OK if the API call is successful or an error
     *          code in case it fails.
     */
    THREADAPI_RESULT ThreadAPI_Create(THREAD_HANDLE* threadHandle, THREAD_START_FUNC func, void* arg);

    /**
     * @brief   Blocks the calling thread by waiting on the thread identified by
     *          the @p threadHandle argument to complete.
     *
     * @param   threadHandle    The handle of the thread to wait for completion.
     *          reserved        This parameter must be set to nullptr
     *
     *          When the @p threadHandle thread completes, all resources associated
     *          with the thread must be released and the thread handle will no
     *          longer be valid.
     *
     * @return  @c THREADAPI_OK if the API call is successful or an error
     *          code in case it fails.
     */
    THREADAPI_RESULT ThreadAPI_Join(THREAD_HANDLE threadHandle, int* reserved);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AZ_THREADAPI_HELPER_H */
