// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AZ_STRINGS_HELPER_H
#define AZ_STRINGS_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct STRING_TAG* STRING_HANDLE;

    STRING_HANDLE STRING_new();
    void STRING_delete(STRING_HANDLE handle);

    STRING_HANDLE STRING_construct(const char* psz);
    STRING_HANDLE STRING_clone(STRING_HANDLE handle);
    int STRING_copy(STRING_HANDLE s1, const char* s2);

    const char* STRING_c_str(STRING_HANDLE handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AZ_STRINGS_HELPER_H */
