// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef HOST_UTILS_H
#define HOST_UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns the length of an IPv6 host without its local zone; other hosts keep their length.
size_t host_without_ipv6_scope_length(const char* host, size_t host_length);

/*
A host is carried internally in its unbracketed form, with a raw "%" separating
an IPv6 address from an optional zone ID, for example "fe80::1%eth0". A URI or
HTTP authority needs the address bracketed, and RFC 6874 section 4 requires the
zone ID to be removed from anything sent to a peer because it only has meaning
on the sending host.

    "example.com"       -> "example.com"
    "192.0.2.1"         -> "192.0.2.1"
    "2001:db8::1"       -> "[2001:db8::1]"
    "fe80::1%eth0"      -> "[fe80::1]"

Used by uws_client.c for the WebSocket Host header, and by http_proxy_io.c for
the CONNECT request-target and its Host header.
*/

/// Length of the formatted authority host, excluding the null terminator.
/// Returns 0 if the host cannot be formatted.
size_t authority_host_length(const char* host);

/// Writes the formatted authority host and a null terminator into output.
/// output_size must be greater than authority_host_length(host).
/// Returns 0 on success.
int format_host_for_authority(const char* host, char* output, size_t output_size);

#ifdef __cplusplus
}
#endif

#endif /* HOST_UTILS_H */
