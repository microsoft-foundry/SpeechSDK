//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <array>

#include "interfaces/proxy_server_info.h"
#include "usp_callbacks.h"
#include "usp_enums.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

class CSpxUspConnection;

class ClientConfiguration
{
public:
    /**
     * Creates a USP client.
     * @param callbacks The struct defines callback functions that will be invoked when various USP events occur.
     * @param endpointType The speech service to be used, Speech, Translation, etc.
     * @param connectionId Connection id, that will be passed to the service in the X-ConnectionId header and can be used for diagnostics.
     */
    ClientConfiguration(CallbacksPtr callbacks, EndpointType endpointType, const std::string& connectionId) :
        m_callbacks(callbacks),
        m_endpointType(endpointType),
        m_recoMode(RecognitionMode::Interactive),
        m_languageIdMode(LanguageIdMode::DetectAtAudioStart),
        m_languageIdPriority(LanguageIdPriority::PrioritizeLatency),
        m_disable_crl_check(false),
        m_continue_on_crl_download_failure(false),
        m_authData{},
        m_userDefinedHttpHeaders{},
        m_connectionId(connectionId),
        m_proxyBypass(),
        m_underlyingOptions{},
        m_isCustomV1Endpoint(false),
        m_isCustomHost(false)
    {
    }

    enum class DialogBackend
    {
        NotSet = 0,
        BotFramework,
        CustomCommands
    };

    /**
     * Sets the audio response format that will be passed to the service in the X-Output-AudioCodec header.
     * More info can be found here: https://docs.microsoft.com/azure/cognitive-services/speech-service/rest-apis
     */
    ClientConfiguration& SetAudioResponseFormat(const std::string& format)
    {
        m_audioResponseFormat = format;
        return *this;
    }

    /**
     * Sets the region of the service endpoint.
     */
    ClientConfiguration& SetRegion(const std::string& region)
    {
        m_region = region;
        return *this;
    }

    /**
     * Sets the URL of the service endpoint. It should contain the host name, resource path and all query parameters needed.
     */
    ClientConfiguration& SetEndpointUrl(const std::string& endpointUrl)
    {
        m_customEndpointUrl = endpointUrl;
        return *this;
    }

    /**
     * Sets the URL of the service host. It should contain "protocol://host:port" where ":port" is optional.
     */
    ClientConfiguration& SetHostUrl(const std::string& hostUrl)
    {
        m_customHostUrl = hostUrl;
        return *this;
    }

    /**
     * Sets the query parameters provided by users.
     */
    ClientConfiguration& SetUserDefinedQueryParameters(const std::string& queryParameters)
    {
        m_userDefinedQueryParameters = queryParameters;
        return *this;
    }

    /**
     * When using OpenSSL only: sets a single trusted cert, optionally w/o CRL checks.
     * This is meant to be used in a firewall setting with potential lack of
     * CRLs (particularly on the leaf).
     * @param trustedCert the certificate to trust (PEM format)
     * @return Client reference
     */
    ClientConfiguration& SetSingleTrustedCert(const std::string& trustedCert)
    {
        m_trustedCert = trustedCert;
        return *this;
    }

    /**
     * Enables CRL checks to be disabled.
     */
    ClientConfiguration& SetDisableCrlChecks(bool disable_crl_check)
    {
        m_disable_crl_check = disable_crl_check;
        return *this;
    }

    /**
     * Enables CRL checks to be skipped on CRL download failure.
     */
    ClientConfiguration& SetContinueOnCrlDownloadFailure(bool continue_on_crl_download_failure)
    {
        m_continue_on_crl_download_failure = continue_on_crl_download_failure;
        return *this;
    }

    /**
     * Sets the speech service type.
     */
    ClientConfiguration& SetEndpointType(EndpointType type)
    {
        m_endpointType = type;
        return *this;
    }

    /**
     * Get the endpoint type.
     */
    const EndpointType& GetEndpointType()
    {
        return m_endpointType;
    }

    /**
     * Sets the property that indicates whether the custom endpoint that is being used is v1 or not.
     */
    ClientConfiguration& SetIsCustomV1Endpoint(bool isCustomV1Endpoint)
    {
        m_isCustomV1Endpoint = isCustomV1Endpoint;
        return *this;
    }

    /**
     * Gets the property that indicates whether the custom endpoint that is being used is v1 or not.
     */
    bool GetIsCustomV1Endpoint()
    {
        return m_isCustomV1Endpoint;
    }

    /**
     * Sets the property that indicates whether the custom endpoint that is being used is the unified endpoint or not.
     */
    ClientConfiguration& SetIsUnifiedEndpoint(bool isUnifiedEndpoint)
    {
        m_isUnifiedEndpoint = isUnifiedEndpoint;
        return *this;
    }

    /**
     * Gets the property that indicates whether the custom endpoint that is being used is unified endpoint or not.
     */
    bool GetIsUnifiedEndpoint()
    {
        return m_isUnifiedEndpoint;
    }

    /**
     * Sets the property that indicates whether the custom host is being used.
     */
    ClientConfiguration& SetIsCustomHost(bool isCustomHost)
    {
        m_isCustomHost = isCustomHost;
        return *this;
    }

    /**
     * Gets the property that indicates whether the custom host is being used.
     */
    bool GetIsCustomHost()
    {
        return m_isCustomHost;
    }

    /**
     * Sets the recognition mode, e.g. Interactive, Conversation, Dictation.
     */
    ClientConfiguration& SetRecognitionMode(RecognitionMode mode)
    {
        m_recoMode = mode;
        return *this;
    }

    /**
     * Sets the language id mode, e.g. DetectAtAudioStart, DetectContinuous, DetectSegments
     */
    void SetLanguageIdMode(LanguageIdMode languageIdMode)
    {
        m_languageIdMode = languageIdMode;
    }

    /**
     * Gets the language id mode, e.g. DetectAtAudioStart, DetectContinuous, DetectSegments
     */
    const LanguageIdMode& GetLanguageIdMode()
    {
        return m_languageIdMode;
    }

    /**
     * Sets the language id priority, e.g. PrioritizeLatency, PrioritizeAccuracy
     */
    void SetLanguageIdPriority(LanguageIdPriority languageIdPriority)
    {
        m_languageIdPriority = languageIdPriority;
    }

    /**
     * Gets the language id priority, e.g. PrioritizeLatency, PrioritizeAccuracy
     */
    const LanguageIdPriority& GetLanguageIdPriority()
    {
        return m_languageIdPriority;
    }

    /**
     * Sets authentication parameters.
     * @param authType The type of authentication to be used.
     * @param authData The authentication data for the specified authentication type.
     */
    ClientConfiguration& SetAuthentication(const std::array<std::string, static_cast<size_t>(AuthenticationType::SIZE_AUTHENTICATION_TYPE)>& authData)
    {
        std::copy_n(authData.cbegin(), authData.size(), m_authData.begin());
        return *this;
    }

    /**
     * Sets HTTP header values.
     * @param headers header values passed in by end users.
     */
    ClientConfiguration& SetUserDefinedHttpHeaders(const std::map<std::string, std::string>& headers)
    {
        // In case, a new key has a duplicate in the existing keys, overwrites the existing one.
        for (const auto& newHeader : headers)
        {
            m_userDefinedHttpHeaders[newHeader.first] = newHeader.second;
        }

        return *this;
    }

    /**
     * Sets the polling interval (ms) the client will use during connection establishment.
     * Configurable via property: SPEECH-USPConnectPollingInterval (string)
     * Legacy property: SPEECH-USPPollingInterval (applies to both connect and run)
     */
    ClientConfiguration& SetPollingIntervalms(const std::uint32_t pollingInterval)
    {
        m_pollingIntervalms = pollingInterval;
        return *this;
    }

    /**
     * Sets the polling interval (ms) the client will use after connection is established.
     * Configurable via property: SPEECH-USPRunPollingInterval (string)
     * Note: If SPEECH-USPPollingInterval (legacy) is set, both intervals match for backward compatibility.
     */
    ClientConfiguration& SetRunPollingIntervalms(const std::uint32_t pollingInterval)
    {
        m_runPollingIntervalms = pollingInterval;
        m_hasRunPollingInterval = true;
        return *this;
    }

    /**
     * Returns true if a separate run polling interval has been configured.
     */
    bool HasRunPollingInterval() const
    {
        return m_hasRunPollingInterval;
    }

    /**
    * Sets the max crl download size in KB.
    */
    ClientConfiguration& SetMaxCrlDownloadSizeInKB(const std::uint32_t maxCRLDownloadSizeInKb)
    {
        m_max_crl_download_size_in_kb = maxCRLDownloadSizeInKb;
        return *this;
    }

    /**
     * Sets the dialog backend to connect to.
     */
    ClientConfiguration& SetDialogBackend(DialogBackend dialogBackend)
    {
        m_dialogBackend = dialogBackend;
        return *this;
    }

    /**
     * Sets the underlying io option, e.g. TCP_NODELAY
     */
    ClientConfiguration& SetUnderlyingOption(const std::string name, int value)
    {
        m_underlyingOptions[name] = value;
        return *this;
    }

    /**
     * Sets the proxy server information, which is used to configure the connection to go through a proxy server.
     */
    ClientConfiguration& SetProxyServerInfo(const char* proxyHost, int proxyPort, const char* proxyUsername, const char* proxyPassword)
    {
        m_proxyServerInfo = std::make_shared<ProxyServerInfo>();
        if (proxyHost)
        {
            m_proxyServerInfo->host = proxyHost;
        }
        m_proxyServerInfo->port = proxyPort;
        if (proxyUsername)
        {
            m_proxyServerInfo->username = proxyUsername;
        }
        if (proxyPassword)
        {
            m_proxyServerInfo->password = proxyPassword;
        }
        return *this;
    }

    /**
     * Sets the list of hosts that we don't want to use the proxies for. This will supersede all other settings.
     * The host match is done in a case insensitive manner. No wild cards are supported
     */
    ClientConfiguration& SetProxyHostBypass(std::vector<std::string>&& bypass)
    {
        m_proxyBypass = std::move(bypass);
        return *this;
    }

    /**
     * Sets query parameters
     */
    ClientConfiguration& SetQueryParameter(const std::string& name, const std::string& value)
    {
        m_queryParameters[name] = value;
        return *this;
    }

    /**
     * Checks for the presence of a query parameter by name
     */
    bool HasQueryParameter(const std::string& name) const
    {
        return (m_queryParameters.find(name) != m_queryParameters.end());
    }

    /**
     * Retrieves the existing value of a keyed query parameter
     * Should only be called if the key is present (use HasQueryParameter)
     */
    const std::string GetQueryParameter(const std::string& name)
    {
        return m_queryParameters[name];
    }

    ClientConfiguration(const ClientConfiguration&) = default;
    ~ClientConfiguration() = default;

private:

    friend class CSpxUspConnection;

    CallbacksPtr m_callbacks;

    EndpointType m_endpointType;
    RecognitionMode m_recoMode;
    LanguageIdMode m_languageIdMode;
    LanguageIdPriority m_languageIdPriority;
    std::string m_customEndpointUrl;
    std::string m_customHostUrl;
    std::string m_region;
    std::string m_userDefinedQueryParameters;
    std::map<std::string, std::string> m_queryParameters;

    std::shared_ptr<ProxyServerInfo> m_proxyServerInfo;

    std::string m_trustedCert;
    bool m_disable_crl_check;
    bool m_continue_on_crl_download_failure;
    int m_max_crl_download_size_in_kb = -1;

    std::array<std::string, static_cast<size_t>(AuthenticationType::SIZE_AUTHENTICATION_TYPE)> m_authData;

    std::map<std::string, std::string> m_userDefinedHttpHeaders;
    std::string m_connectionId;

    std::string m_audioResponseFormat;

    // Polling intervals for the WebSocket work loop (in milliseconds).
    // - m_pollingIntervalms: Used during connection establishment (default: 10ms for fast response)
    // - m_runPollingIntervalms: Used after connection is established (default: 100ms for efficiency)
    // The switch from connect to run interval happens automatically in OnTransportOpened.
    std::uint32_t m_pollingIntervalms = 10;
    std::uint32_t m_runPollingIntervalms = 10;
    bool m_hasRunPollingInterval = false;

    DialogBackend m_dialogBackend = DialogBackend::NotSet;

    std::vector<std::string> m_proxyBypass;

    std::map<std::string, int> m_underlyingOptions;

    bool m_isCustomV1Endpoint;
    bool m_isUnifiedEndpoint;

    bool m_isCustomHost;
};

}}}}
