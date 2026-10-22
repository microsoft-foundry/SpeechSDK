// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stddef.h>
#include <string.h>

#include "testrunnerswitcher.h"
#include "../../src/host_utils.h"

#define ASSERT_EQUAL(type, expected, actual) CTEST_ASSERT_ARE_EQUAL_WITH_MSG(type, expected, actual, "")
#define ASSERT_NOT_EQUAL(type, expected, actual) CTEST_ASSERT_ARE_NOT_EQUAL_WITH_MSG(type, expected, actual, "")

/* Checks that both entry points agree: the length reported for a host matches
   what formatting it actually produces. */
#define ASSERT_FORMATS_TO(host, expected)                                                   \
    do                                                                                      \
    {                                                                                       \
        char output[128];                                                                   \
        ASSERT_EQUAL(size_t, sizeof(expected) - 1, authority_host_length(host));            \
        ASSERT_EQUAL(int, 0, format_host_for_authority(host, output, sizeof(output)));      \
        ASSERT_EQUAL(char_ptr, expected, output);                                           \
    } while (0)

BEGIN_TEST_SUITE(host_utils_ut)

TEST_FUNCTION(a_dns_host_is_left_unchanged)
{
    ASSERT_FORMATS_TO("example.azure-devices.net", "example.azure-devices.net");
}

TEST_FUNCTION(an_ipv4_host_is_left_unchanged)
{
    ASSERT_FORMATS_TO("192.168.0.1", "192.168.0.1");
}

TEST_FUNCTION(a_global_ipv6_host_is_bracketed)
{
    ASSERT_FORMATS_TO("2001:db8::1", "[2001:db8::1]");
}

TEST_FUNCTION(a_loopback_ipv6_host_is_bracketed)
{
    ASSERT_FORMATS_TO("::1", "[::1]");
}

/* RFC 6874 section 4: an intermediary must remove the zone ID from an outgoing
   URI, because it only has meaning on the sending host. */
TEST_FUNCTION(a_named_ipv6_zone_is_removed)
{
    ASSERT_FORMATS_TO("fe80::1%eth0", "[fe80::1]");
}

TEST_FUNCTION(a_numeric_ipv6_zone_is_removed)
{
    ASSERT_FORMATS_TO("fe80::1%12", "[fe80::1]");
}

TEST_FUNCTION(an_ipv6_zone_containing_reserved_bytes_is_removed)
{
    ASSERT_FORMATS_TO("fe80::1%Ethernet 2", "[fe80::1]");
}

/* A percent sign only starts a zone ID on an IPv6 literal, so it must survive
   anywhere else. */
TEST_FUNCTION(a_percent_in_a_dns_host_is_not_treated_as_a_zone)
{
    ASSERT_FORMATS_TO("weird%name.example.com", "weird%name.example.com");
}

/* Guards against a caller that has already bracketed the literal producing
   "[[::1]]". */
TEST_FUNCTION(an_already_bracketed_host_is_not_bracketed_again)
{
    ASSERT_FORMATS_TO("[2001:db8::1]", "[2001:db8::1]");
}

TEST_FUNCTION(an_already_bracketed_scoped_host_still_loses_its_zone)
{
    ASSERT_FORMATS_TO("[fe80::1%eth0]", "[fe80::1]");
}

TEST_FUNCTION(an_empty_host_stays_empty)
{
    ASSERT_FORMATS_TO("", "");
}

TEST_FUNCTION(a_scoped_ipv6_host_excludes_its_zone_from_peer_identity)
{
    const char* host = "fe80::1%Ethernet 2";
    ASSERT_EQUAL(size_t, sizeof("fe80::1") - 1, host_without_ipv6_scope_length(host, strlen(host)));
}

TEST_FUNCTION(a_percent_in_a_dns_host_remains_part_of_peer_identity)
{
    const char* host = "weird%name.example.com";
    ASSERT_EQUAL(size_t, strlen(host), host_without_ipv6_scope_length(host, strlen(host)));
}

TEST_FUNCTION(a_colon_after_a_percent_does_not_make_a_scope)
{
    const char* host = "weird%name:example";
    ASSERT_EQUAL(size_t, strlen(host), host_without_ipv6_scope_length(host, strlen(host)));
}

TEST_FUNCTION(an_unscoped_ipv6_host_remains_the_peer_identity)
{
    const char* host = "2001:db8::1";
    ASSERT_EQUAL(size_t, strlen(host), host_without_ipv6_scope_length(host, strlen(host)));
}

TEST_FUNCTION(a_bracketed_ipv6_host_excludes_its_zone_without_its_brackets)
{
    const char* host = "[fe80::1%eth0]";
    ASSERT_EQUAL(size_t, sizeof("fe80::1") - 1, host_without_ipv6_scope_length(host + 1, strlen(host) - 2));
}

TEST_FUNCTION(format_host_for_authority_with_exact_size_buffer_succeeds)
{
    // arrange
    const char* host = "2001:db8::1";
    char output[sizeof("[2001:db8::1]")];

    // act
    int result = format_host_for_authority(host, output, sizeof(output));

    // assert
    ASSERT_EQUAL(int, 0, result);
    ASSERT_EQUAL(char_ptr, "[2001:db8::1]", output);
}

TEST_FUNCTION(format_host_for_authority_with_too_small_buffer_fails)
{
    // arrange
    const char* host = "2001:db8::1";
    char output[sizeof("[2001:db8::1]")];

    // act
    int result = format_host_for_authority(host, output, sizeof(output) - 1);

    // assert
    ASSERT_NOT_EQUAL(int, 0, result);
}

TEST_FUNCTION(null_arguments_fail)
{
    // arrange
    char output[16];

    // act, assert
    ASSERT_EQUAL(size_t, 0, authority_host_length(NULL));
    ASSERT_NOT_EQUAL(int, 0, format_host_for_authority(NULL, output, sizeof(output)));
    ASSERT_NOT_EQUAL(int, 0, format_host_for_authority("a", NULL, sizeof(output)));
}

END_TEST_SUITE(host_utils_ut)
