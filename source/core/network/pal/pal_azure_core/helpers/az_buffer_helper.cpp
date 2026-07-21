// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <memory>
#include <cstring>
#include <cstdlib>
#include "az_buffer_helper.h"

/**
    NOTE:
    This is a slightly modified version of the Azure C Shared code that contains the bare minimum functions we
    need. This is intended to be a stop gap until some code can be re-factored (e.g. audio extension)
*/

typedef struct BUFFER_TAG
{
    unsigned char* buffer;
    size_t size;
} BUFFER;

#define __FAILURE__ 1

#ifdef __cplusplus
extern "C" {
#endif

    static int BUFFER_safemalloc(BUFFER* handleptr, size_t size)
    {
        int result;
        size_t sizetomalloc = size;
        if (size == 0)
        {
            sizetomalloc = 1;
        }
        handleptr->buffer = (unsigned char*)malloc(sizetomalloc);
        if (handleptr->buffer == NULL)
        {
            /*Codes_SRS_BUFFER_02_003: [If allocating memory fails, then BUFFER_create shall return NULL.]*/
            result = __FAILURE__;
        }
        else
        {
            // we still consider the real buffer size is 0
            handleptr->size = size;
            result = 0;
        }
        return result;
    }

    BUFFER_HANDLE BUFFER_create(const unsigned char* source, size_t size)
    {
        BUFFER* result;
        /*Codes_SRS_BUFFER_02_001: [If source is NULL then BUFFER_create shall return NULL.]*/
        if (source == NULL)
        {
            result = NULL;
        }
        else
        {
            /*Codes_SRS_BUFFER_02_002: [Otherwise, BUFFER_create shall allocate memory to hold size bytes and shall copy from source size bytes into the newly allocated memory.] */
            result = (BUFFER*)malloc(sizeof(BUFFER));
            if (result == NULL)
            {
                /*Codes_SRS_BUFFER_02_003: [If allocating memory fails, then BUFFER_create shall return NULL.] */
                /*fallthrough*/
            }
            else
            {
                /* Codes_SRS_BUFFER_02_005: [If size parameter is 0 then 1 byte of memory shall be allocated yet size of the buffer shall be set to 0.]*/
                if (BUFFER_safemalloc(result, size) != 0)
                {
                    free(result);
                    result = NULL;
                }
                else
                {
                    /*Codes_SRS_BUFFER_02_004: [Otherwise, BUFFER_create shall return a non-NULL handle.] */
                    (void)memcpy(result->buffer, source, size);
                }
            }
        }
        return (BUFFER_HANDLE)result;
    }

    void BUFFER_delete(BUFFER_HANDLE handle)
    {
        /* Codes_SRS_BUFFER_07_004: [BUFFER_delete shall not delete any BUFFER_HANDLE that is NULL.] */
        if (handle != NULL)
        {
            BUFFER* b = (BUFFER*)handle;
            if (b->buffer != NULL)
            {
                /* Codes_SRS_BUFFER_07_003: [BUFFER_delete shall delete the data associated with the BUFFER_HANDLE along with the Buffer.] */
                free(b->buffer);
            }
            free(b);
        }
    }

    unsigned char* BUFFER_u_char(BUFFER_HANDLE handle)
    {
        BUFFER* handleData = (BUFFER*)handle;
        unsigned char* result;
        if (handle == NULL || handleData->size == 0)
        {
            /* Codes_SRS_BUFFER_07_026: [BUFFER_u_char shall return NULL for any error that is encountered.] */
            /* Codes_SRS_BUFFER_07_029: [BUFFER_u_char shall return NULL if underlying buffer size is zero.] */
            result = NULL;
        }
        else
        {
            result = handleData->buffer;
        }
        return result;
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */
