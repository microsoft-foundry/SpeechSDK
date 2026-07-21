// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AZ_BUFFER_HELPER_H
#define AZ_BUFFER_HELPER_H

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct BUFFER_TAG* BUFFER_HANDLE;

    BUFFER_HANDLE BUFFER_create(const unsigned char* source, size_t size);
    void BUFFER_delete(BUFFER_HANDLE handle);
    unsigned char* BUFFER_u_char(BUFFER_HANDLE handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AZ_BUFFER_HELPER_H */
