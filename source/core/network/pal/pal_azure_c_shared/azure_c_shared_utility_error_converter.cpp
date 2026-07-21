//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// Error converter for azure_c_shared_utility library error codes
// Provides human-readable error messages for network connection failures
//

#include <string>
#include <cstring>
#include <cerrno>
#include "azure_c_shared_utility_error_converter.h"
#include <azure_c_shared_utility/macro_utils.h>
#include <azure_c_shared_utility/uws_client.h>


#ifndef _MSC_VER
#include <netdb.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

// Cross-platform socket error code mappings
#ifdef _MSC_VER
    #define ERR_CONNREFUSED   WSAECONNREFUSED      // 10061
    #define ERR_NETUNREACH    WSAENETUNREACH       // 10051
    #define ERR_HOSTUNREACH   WSAEHOSTUNREACH      // 10065
    #define ERR_TIMEDOUT      WSAETIMEDOUT         // 10060
    #define ERR_CONNRESET     WSAECONNRESET        // 10054
    #define ERR_NOTCONN       WSAENOTCONN          // 10057
    #define ERR_ADDRINUSE     WSAEADDRINUSE        // 10048
    #define ERR_ADDRNOTAVAIL  WSAEADDRNOTAVAIL     // 10049
    #define ERR_NETDOWN       WSAENETDOWN          // 10050
    #define ERR_NOBUFS        WSAENOBUFS           // 10055
    #define ERR_CONNABORTED   WSAECONNABORTED      // 10053
    #define ERR_SHUTDOWN      WSAESHUTDOWN         // 10058
    #define ERR_INPROGRESS    WSAEINPROGRESS       // 10036
    #define ERR_ALREADY       WSAEALREADY          // 10037
    #define ERR_ISCONN        WSAEISCONN           // 10056
    #define ERR_PROVIDER_FAIL WSAEPROVIDERFAILEDINIT // 10106
    #define ERR_PROCLIM       WSAEPROCLIM          // 10067
    #define ERR_AFNOSUPPORT   WSAEAFNOSUPPORT      // 10047
    #define ERR_PFNOSUPPORT   WSAEPFNOSUPPORT      // 10046
    #define ERR_SOCKTNOSUPPORT WSAESOCKTNOSUPPORT  // 10044
    #define ERR_NOPROTOOPT    WSAENOPROTOOPT       // 10042
    #define ERR_PROTONOSUPPORT WSAEPROTONOSUPPORT  // 10043
    #define ERR_DNS_NONAME    WSAHOST_NOT_FOUND    // 11001
    #define ERR_DNS_AGAIN     WSATRY_AGAIN         // 11002
    #define ERR_DNS_FAIL      WSANO_RECOVERY       // 11003
    #define ERR_DNS_NODATA    WSANO_DATA           // 11004
#else
    #define ERR_CONNREFUSED   ECONNREFUSED         // 111
    #define ERR_NETUNREACH    ENETUNREACH          // 101
    #define ERR_HOSTUNREACH   EHOSTUNREACH         // 113
    #define ERR_TIMEDOUT      ETIMEDOUT            // 110
    #define ERR_CONNRESET     ECONNRESET           // 104
    #define ERR_NOTCONN       ENOTCONN             // 107
    #define ERR_ADDRINUSE     EADDRINUSE           // 98
    #define ERR_ADDRNOTAVAIL  EADDRNOTAVAIL        // 99
    #define ERR_NETDOWN       ENETDOWN             // 100
    #define ERR_NOBUFS        ENOBUFS              // 105
    #define ERR_CONNABORTED   ECONNABORTED         // 103
    #define ERR_SHUTDOWN      ESHUTDOWN            // 108
    #define ERR_INPROGRESS    EINPROGRESS          // 115
    #define ERR_ALREADY       EALREADY             // 114
    #define ERR_ISCONN        EISCONN              // 106
    #define ERR_PROVIDER_FAIL -9999 // Not applicable on Linux, use sentinel value
    #define ERR_PROCLIM       -9998 // Not applicable on Linux
    #define ERR_AFNOSUPPORT   EAFNOSUPPORT         // 97
    #define ERR_PFNOSUPPORT   EPFNOSUPPORT         // 96
    #define ERR_SOCKTNOSUPPORT ESOCKTNOSUPPORT     // 94
    #define ERR_NOPROTOOPT    ENOPROTOOPT          // 92
    #define ERR_PROTONOSUPPORT EPROTONOSUPPORT     // 93
    #define ERR_DNS_NONAME    EAI_NONAME           // -2
    #define ERR_DNS_AGAIN     EAI_AGAIN            // -3
    #define ERR_DNS_FAIL      EAI_FAIL             // -4
    #define ERR_DNS_NODATA    EAI_NODATA           // -5 (deprecated but still used)
#endif

enum class NetworkErrorCategory
{
    Unknown,
    DnsResolution,
    ConnectionRefused,
    NetworkUnreachable,
    Timeout,
    SocketError,
    TlsError,
    ProxyError,
    ResourceError
};

std::string GetAzureCSharedErrorMessage(int32_t errorCode)
{
    if (errorCode < 0)
    {
        return ENUM_TO_STRING(WS_OPEN_RESULT, (WS_OPEN_RESULT)(0x7FFFFFFF & errorCode));
    }
    else if (errorCode > 0)
    {
        return ENUM_TO_STRING(HTTPAPI_RESULT, (HTTPAPI_RESULT)errorCode);
    }
    return {};
}

struct SocketErrorInfo
{
    const char* description;
    const char* troubleshootingHint;
    NetworkErrorCategory category;
};

static SocketErrorInfo GetSocketErrorInfo(int errorCode) noexcept
{
    switch (errorCode)
    {
        case ERR_DNS_NONAME:
            return {
                "DNS resolution failed - hostname not found",
                "Check if the hostname is spelled correctly and DNS server is reachable",
                NetworkErrorCategory::DnsResolution
            };
        case ERR_DNS_AGAIN:
            return {
                "DNS temporary failure - server not responding",
                "Retry the connection or check DNS server availability",
                NetworkErrorCategory::DnsResolution
            };
        case ERR_DNS_FAIL:
            return {
                "DNS non-recoverable failure",
                "Check DNS configuration and network connectivity",
                NetworkErrorCategory::DnsResolution
            };
        case ERR_DNS_NODATA:
            return {
                "DNS lookup succeeded but no address records found",
                "Verify the hostname has A/AAAA records configured",
                NetworkErrorCategory::DnsResolution
            };

        case ERR_CONNREFUSED:
            return {
                "Connection refused - no service listening on the target port",
                "Verify the service is running and the port number is correct",
                NetworkErrorCategory::ConnectionRefused
            };
        case ERR_TIMEDOUT:
            return {
                "Connection timed out - server did not respond in time",
                "Check firewall rules, network latency, or if the server is overloaded",
                NetworkErrorCategory::Timeout
            };
        case ERR_CONNRESET:
            return {
                "Connection reset by peer - server closed the connection unexpectedly",
                "The remote server terminated the connection; check server logs",
                NetworkErrorCategory::ConnectionRefused
            };
        case ERR_CONNABORTED:
            return {
                "Connection aborted - local system terminated the connection",
                "Check for local firewall or antivirus interference",
                NetworkErrorCategory::SocketError
            };

        case ERR_NETUNREACH:
            return {
                "Network is unreachable - no route to the destination network",
                "Check network connectivity, VPN status, or routing configuration",
                NetworkErrorCategory::NetworkUnreachable
            };
        case ERR_HOSTUNREACH:
            return {
                "Host is unreachable - cannot reach the specific server",
                "Verify the server is online and accessible from your network",
                NetworkErrorCategory::NetworkUnreachable
            };
        case ERR_NETDOWN:
            return {
                "Network is down - local network interface is unavailable",
                "Check if the network adapter is enabled and connected",
                NetworkErrorCategory::NetworkUnreachable
            };

        case ERR_NOTCONN:
            return {
                "Socket is not connected",
                "Connection was lost or never established; retry the operation",
                NetworkErrorCategory::SocketError
            };
        case ERR_ISCONN:
            return {
                "Socket is already connected",
                "Internal error - connection already established",
                NetworkErrorCategory::SocketError
            };
        case ERR_SHUTDOWN:
            return {
                "Socket has been shut down",
                "Connection was intentionally closed; reconnect if needed",
                NetworkErrorCategory::SocketError
            };

        case ERR_ADDRINUSE:
            return {
                "Address already in use - port is occupied by another process",
                "Wait for the port to be released or use a different port",
                NetworkErrorCategory::SocketError
            };
        case ERR_ADDRNOTAVAIL:
            return {
                "Requested address is not available on this machine",
                "Verify the local IP address configuration",
                NetworkErrorCategory::SocketError
            };

        case ERR_NOBUFS:
            return {
                "No buffer space available - system resources exhausted",
                "Close unused connections or increase system buffer limits",
                NetworkErrorCategory::ResourceError
            };
        case ERR_PROCLIM:
            return {
                "Too many processes or connections",
                "Close unused connections or increase process limits",
                NetworkErrorCategory::ResourceError
            };

        case ERR_AFNOSUPPORT:
            return {
                "Address family not supported",
                "Internal error - protocol configuration issue",
                NetworkErrorCategory::SocketError
            };
        case ERR_PFNOSUPPORT:
            return {
                "Protocol family not supported",
                "Internal error - protocol not available on this system",
                NetworkErrorCategory::SocketError
            };
        case ERR_SOCKTNOSUPPORT:
            return {
                "Socket type not supported",
                "Internal error - socket configuration issue",
                NetworkErrorCategory::SocketError
            };
        case ERR_NOPROTOOPT:
            return {
                "Protocol option not available",
                "Internal error - unsupported socket option",
                NetworkErrorCategory::SocketError
            };
        case ERR_PROTONOSUPPORT:
            return {
                "Protocol not supported",
                "Internal error - protocol not available",
                NetworkErrorCategory::SocketError
            };

        case ERR_INPROGRESS:
            return {
                "Connection in progress - operation would block",
                "Wait for connection to complete",
                NetworkErrorCategory::SocketError
            };
        case ERR_ALREADY:
            return {
                "Connection already in progress",
                "Wait for the pending connection attempt to complete",
                NetworkErrorCategory::SocketError
            };

        case ERR_PROVIDER_FAIL:
            return {
                "Winsock service provider failed to initialize",
                "Restart the application or check Windows networking services",
                NetworkErrorCategory::SocketError
            };

        default:
            return { nullptr, nullptr, NetworkErrorCategory::Unknown };
    }
}

static const char* GetCategoryName(NetworkErrorCategory category) noexcept
{
    switch (category)
    {
        case NetworkErrorCategory::DnsResolution:     return "DNS";
        case NetworkErrorCategory::ConnectionRefused: return "CONNECTION";
        case NetworkErrorCategory::NetworkUnreachable: return "NETWORK";
        case NetworkErrorCategory::Timeout:           return "TIMEOUT";
        case NetworkErrorCategory::SocketError:       return "SOCKET";
        case NetworkErrorCategory::TlsError:          return "TLS";
        case NetworkErrorCategory::ProxyError:        return "PROXY";
        case NetworkErrorCategory::ResourceError:     return "RESOURCE";
        default:                                      return "UNKNOWN";
    }
}

static std::string GetSystemErrorString(int errorCode) noexcept
{
    if (errorCode == 0) return {};

#ifdef _MSC_VER
    LPSTR buffer = nullptr;
    std::string result;

    // Only use FormatMessageA for non-negative error codes (WSA errors, Win32 errors)
    // Negative codes (like DNS EAI_* errors) would produce invalid DWORD values
    DWORD len = 0;
    if (errorCode >= 0)
    {
        len = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, static_cast<DWORD>(errorCode),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
    }

    if (len && buffer)
    {
        result.assign(buffer, len);
        LocalFree(buffer);
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n'))
            result.pop_back();
    }

    // Fallback to strerror_s when FormatMessage yields nothing (some codes are CRT-only)
    if (result.empty())
    {
        char buf[256] = {0};
        if (strerror_s(buf, sizeof(buf), errorCode) == 0 && buf[0] != '\0')
        {
            result = buf;
        }
    }

    return result;
#else
    // On Linux/glibc: EAI_* DNS errors are negative (e.g., EAI_NONAME = -2)
    // errno values are positive. Try both to get meaningful messages.
    if (errorCode < 0)
    {
        // Likely a DNS/getaddrinfo error (EAI_*)
        const char* msg = gai_strerror(errorCode);
        if (msg && msg[0]) return msg;
    }
    
    // For positive codes (errno) or if gai_strerror didn't help
    if (errorCode > 0)
    {
        const char* msg = strerror(errorCode);
        if (msg && msg[0]) return msg;
    }
    return {};
#endif
}

std::string GetConnectionErrorMessage(WS_OPEN_RESULT wsResult, int underlyingCode)
{
    const char* enumStr = ENUM_TO_STRING(WS_OPEN_RESULT, wsResult);
    std::string result = enumStr ? enumStr : "UNKNOWN_WS_RESULT";

    // Only decode for IO failures
    if (wsResult != WS_OPEN_ERROR_UNDERLYING_IO_OPEN_FAILED &&
        wsResult != WS_OPEN_ERROR_UNDERLYING_IO_OPEN_CANCELLED)
    {
        return result;
    }

    if (underlyingCode == 0) return result;

    SocketErrorInfo errorInfo = GetSocketErrorInfo(underlyingCode);
    if (errorInfo.description)
    {
        std::string errorDetail = "[" + std::string(GetCategoryName(errorInfo.category)) + "] ";
        errorDetail += errorInfo.description;
        if (errorInfo.troubleshootingHint && errorInfo.troubleshootingHint[0])
        {
            errorDetail += " - " + std::string(errorInfo.troubleshootingHint);
        }
        
        return result + " (code=" + std::to_string(underlyingCode) + ": " + errorDetail + ")";
    }

    std::string sysErr = GetSystemErrorString(underlyingCode);
    if (!sysErr.empty())
    {
        return result + " (code=" + std::to_string(underlyingCode) + ": " + sysErr + ")";
    }
    
    return result + " (code=" + std::to_string(underlyingCode) + ")";
}
