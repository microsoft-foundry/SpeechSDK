// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <string.h>

#include "host_utils.h"
#include "azure_c_shared_utility/safe_math.h"

typedef struct AUTHORITY_HOST_INFO_TAG
{
    const char* address;
    size_t address_length;
    size_t formatted_length;
    int is_ipv6;
} AUTHORITY_HOST_INFO;

size_t host_without_ipv6_scope_length(const char* host, size_t host_length)
{
    const char* scope = memchr(host, '%', host_length);
    return (scope != NULL && memchr(host, ':', (size_t)(scope - host)) != NULL)
        ? (size_t)(scope - host)
        : host_length;
}

static int get_authority_host_info(const char* host, AUTHORITY_HOST_INFO* info)
{
    int result;

    if ((host == NULL) || (info == NULL))
    {
        result = __LINE__;
    }
    else
    {
        const char* address = host;
        size_t address_length = strlen(host);
        size_t i;
        int is_ipv6 = 0;

        // Accept an already bracketed literal so a caller cannot produce "[[::1]]".
        if ((address_length >= 2) && (address[0] == '[') && (address[address_length - 1] == ']'))
        {
            address++;
            address_length -= 2;
        }

        for (i = 0; i < address_length; i++)
        {
            if (address[i] == ':')
            {
                // A colon cannot appear in a DNS name or an IPv4 literal.
                is_ipv6 = 1;
                break;
            }
        }

        if (is_ipv6 != 0)
        {
            // RFC 6874 section 4: the zone ID is local to this host, so drop it.
            address_length = host_without_ipv6_scope_length(address, address_length);
        }

        info->address = address;
        info->address_length = address_length;
        info->is_ipv6 = is_ipv6;
        info->formatted_length = (is_ipv6 != 0)
            ? safe_add_size_t(address_length, 2)
            : address_length;

        result = (info->formatted_length == SIZE_MAX) ? __LINE__ : 0;
    }

    return result;
}

size_t authority_host_length(const char* host)
{
    AUTHORITY_HOST_INFO info;
    size_t result;

    if (get_authority_host_info(host, &info) != 0)
    {
        result = 0;
    }
    else
    {
        result = info.formatted_length;
    }

    return result;
}

int format_host_for_authority(const char* host, char* output, size_t output_size)
{
    AUTHORITY_HOST_INFO info;
    int result;

    if ((host == NULL) || (output == NULL))
    {
        result = __LINE__;
    }
    else if (get_authority_host_info(host, &info) != 0)
    {
        result = __LINE__;
    }
    else if (output_size <= info.formatted_length)
    {
        result = __LINE__;
    }
    else
    {
        size_t output_index = 0;

        if (info.is_ipv6 != 0)
        {
            output[output_index++] = '[';
        }

        (void)memcpy(output + output_index, info.address, info.address_length);
        output_index += info.address_length;

        if (info.is_ipv6 != 0)
        {
            output[output_index++] = ']';
        }

        output[output_index] = '\0';
        result = 0;
    }

    return result;
}
