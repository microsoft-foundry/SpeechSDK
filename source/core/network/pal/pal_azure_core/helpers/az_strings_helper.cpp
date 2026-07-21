// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <memory>
#include <cstring>
#include <cstdlib>
#include "az_strings_helper.h"

/**
    NOTE:
    This is a slightly modified version of the Azure C Shared code that contains the bare minimum functions we
    need. This is intended to be a stop gap until some code can be re-factored (e.g. audio extension)
*/

#ifdef __cplusplus
extern "C" {
#endif

#define __FAILURE__ 1

    typedef struct STRING_TAG
    {
        char* s;
    } STRING;

    STRING_HANDLE STRING_new(void)
    {
        STRING* result;
        if ((result = (STRING*)malloc(sizeof(STRING))) != NULL)
        {
            if ((result->s = (char*)malloc(1)) != NULL)
            {
                result->s[0] = '\0';
            }
            else
            {
                free(result);
                result = NULL;
            }
        }
        return (STRING_HANDLE)result;
    }

    void STRING_delete(STRING_HANDLE handle)
    {
        if (handle != NULL)
        {
            STRING* value = (STRING*)handle;
            free(value->s);
            value->s = NULL;
            free(value);
        }
    }

    /* Codes_SRS_STRING_07_003: [STRING_construct shall allocate a new string with the value of the specified const char*.] */
    STRING_HANDLE STRING_construct(const char* psz)
    {
        STRING_HANDLE result;
        if (psz == NULL)
        {
            /* Codes_SRS_STRING_07_005: [If the supplied const char* is NULL STRING_construct shall return a NULL value.] */
            result = NULL;
        }
        else
        {
            STRING* str;
            if ((str = (STRING*)malloc(sizeof(STRING))) != NULL)
            {
                size_t nLen = strlen(psz) + 1;
                if ((str->s = (char*)malloc(nLen)) != NULL)
                {
                    (void)memcpy(str->s, psz, nLen);
                    result = (STRING_HANDLE)str;
                }
                /* Codes_SRS_STRING_07_032: [STRING_construct encounters any error it shall return a NULL value.] */
                else
                {
                    free(str);
                    result = NULL;
                }
            }
            else
            {
                /* Codes_SRS_STRING_07_032: [STRING_construct encounters any error it shall return a NULL value.] */
                result = NULL;
            }
        }
        return result;
    }

    /*Codes_SRS_STRING_02_001: [STRING_clone shall produce a new string having the same content as the handle string.*/
    STRING_HANDLE STRING_clone(STRING_HANDLE handle)
    {
        STRING* result;
        /*Codes_SRS_STRING_02_002: [If parameter handle is NULL then STRING_clone shall return NULL.]*/
        if (handle == NULL)
        {
            result = NULL;
        }
        else
        {
            /*Codes_SRS_STRING_02_003: [If STRING_clone fails for any reason, it shall return NULL.] */
            if ((result = (STRING*)malloc(sizeof(STRING))) != NULL)
            {
                STRING* source = (STRING*)handle;
                /*Codes_SRS_STRING_02_003: [If STRING_clone fails for any reason, it shall return NULL.] */
                size_t sourceLen = strlen(source->s);
                if ((result->s = (char*)malloc(sourceLen + 1)) == NULL)
                {
                    free(result);
                    result = NULL;
                }
                else
                {
                    (void)memcpy(result->s, source->s, sourceLen + 1);
                }
            }
            else
            {
                /*not much to do, result is NULL from malloc*/
            }
        }
        return (STRING_HANDLE)result;
    }

    /*this function will copy the string from s2 to s1*/
    /*returns 0 if success*/
    /*any other error code is failure*/
    /* Codes_SRS_STRING_07_016: [STRING_copy shall copy the const char* into the supplied STRING_HANDLE.] */
    int STRING_copy(STRING_HANDLE handle, const char* s2)
    {
        int result;
        if ((handle == NULL) || (s2 == NULL))
        {
            /* Codes_SRS_STRING_07_017: [STRING_copy shall return a nonzero value if any of the supplied parameters are NULL.] */
            result = __FAILURE__;
        }
        else
        {
            STRING* s1 = (STRING*)handle;
            /* Codes_SRS_STRING_07_026: [If the underlying char* refered to by s1 handle is equal to char* s2 than STRING_copy shall be a noop and return 0.] */
            if (s1->s != s2)
            {
                size_t s2Length = strlen(s2);
                char* temp = (char*)realloc(s1->s, s2Length + 1);
                if (temp == NULL)
                {
                    /* Codes_SRS_STRING_07_027: [STRING_copy shall return a nonzero value if any error is encountered.] */
                    result = __FAILURE__;
                }
                else
                {
                    s1->s = temp;
                    memmove(s1->s, s2, s2Length + 1);
                    result = 0;
                }
            }
            else
            {
                /* Codes_SRS_STRING_07_033: [If overlapping pointer address is given to STRING_copy the behavior is undefined.] */
                result = 0;
            }
        }
        return result;
    }

    /* Codes_SRS_STRING_07_020: [STRING_c_str shall return the const char* associated with the given STRING_HANDLE.] */
    const char* STRING_c_str(STRING_HANDLE handle)
    {
        const char* result;
        if (handle != NULL)
        {
            result = ((STRING*)handle)->s;
        }
        else
        {
            /* Codes_SRS_STRING_07_021: [STRING_c_str shall return NULL if the STRING_HANDLE is NULL.] */
            result = NULL;
        }
        return result;
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */
