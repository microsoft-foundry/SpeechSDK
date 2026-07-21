//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include <sstream>

#include "usp_connection.h"
#include "usp_client_configuration.h"
#include "uspimpl.h"
#include "endpoint_utils.h"
#include "error_info.h"
#include "guid_utils.h"
#include "http_endpoint_info.h"
#include "http_utils.h"
#include "interfaces/ispx_telemetry_store.h"
#include "create_object_helpers.h"

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

// Note: these forward-decl array dimensions must match the definitions in usp_endpoint.h
// This explicit match won't be needed if/when we move to c++17
constexpr std::array<const char*, 12> endpoint::v1speech::queryParameters;
constexpr std::array<const char*, 2> endpoint::unifiedspeech::queryParameters;
constexpr std::array<const char*, 4> endpoint::standalonelid::queryParameters;
constexpr std::array<const char*, 13> endpoint::translationV1::queryParameters;
constexpr std::array<const char*, 7> endpoint::dialog::customCommands::queryParameters;
constexpr std::array<const char*, 8> endpoint::dialog::botFramework::queryParameters;
constexpr std::array<const char*, 5> endpoint::conversationTranscriber::queryParameters;
constexpr std::array<const char*, 7> endpoint::conversationTranscriberV2::queryParameters;
constexpr std::array<const char*, 1> endpoint::speechSynthesis::queryParameters;

decltype(endpoint::outputFormatSimple) constexpr endpoint::outputFormatSimple;
decltype(endpoint::outputFormatDetailed) constexpr endpoint::outputFormatDetailed;

const char* path::speechHypothesis = "speech.hypothesis";
const char* path::speechTentativePhrase = "speech.tentative.phrase";
const char* path::speechPhrase = "speech.phrase";
const char* path::speechFragment = "speech.fragment";
const char* path::speechKeyword = "speech.keyword";
const char* path::turnStart = "turn.start";
const char* path::turnEnd = "turn.end";
const char* path::speechStartDetected = "speech.startDetected";
const char* path::speechEndDetected = "speech.endDetected";
const char* path::translationHypothesis = "translation.hypothesis";
const char* path::translationPhrase = "translation.phrase";
const char* path::translationSynthesis = "translation.synthesis";
const char* path::translationSynthesisEnd = "translation.synthesis.end";
const char* path::translationResponse = "translation.response";
const char* path::audio = "audio";
const char* path::audioMetaData = "audio.metadata";
const char* path::audioStart = "audio.start";
const char* path::audioEnd = "audio.end";

const char* json_properties::offset = "Offset";
const char* json_properties::duration = "Duration";
const char* json_properties::status = "Status";
const char* json_properties::text = "Text";
const char* json_properties::recoStatus = "RecognitionStatus";
const char* json_properties::displayText = "DisplayText";
const char* json_properties::context = "context";
const char* json_properties::tag = "serviceTag";
const char* json_properties::speaker = "SpeakerId";
const char* json_properties::utteranceId = "UtteranceId";
const char* json_properties::phraseId = "Id";
const char* json_properties::nbest = "NBest";
const char* json_properties::confidence = "Confidence";
const char* json_properties::display = "Display";

const char* json_properties::translation = "Translation";
const char* json_properties::translationStatus = "TranslationStatus";
const char* json_properties::failureReason = "FailureReason";
const char* json_properties::translations = "Translations";
const char* json_properties::synthesisStatus = "SynthesisStatus";
const char* json_properties::lang = "Language";
const char* json_properties::translationLanguage = "TranslationLanguage";

const char* json_properties::metadata = "Metadata";
const char* json_properties::type = "Type";
const char* json_properties::data = "Data";
const char* json_properties::textBoundary = "TextBoundary";
const char* json_properties::wordBoundary = "WordBoundary";
const char* json_properties::sentenceBoundary = "SentenceBoundary";
const char* json_properties::boundaryType = "BoundaryType";
const char* json_properties::viseme = "Viseme";
const char* json_properties::bookmark = "Bookmark";
const char* json_properties::sessionEnd = "SessionEnd";
const char* json_properties::lowerText = "text";
const char* json_properties::visemeId = "VisemeId";
const char* json_properties::animationChunk = "AnimationChunk";
const char* json_properties::isLastAnimation = "IsLastAnimation";

const char* json_properties::primaryLanguage = "PrimaryLanguage";
const char* json_properties::speechHypothesis = "SpeechHypothesis";
const char* json_properties::speechPhrase = "SpeechPhrase";

// This is called from telemetry_flush, invoked on a worker thread in turn-end.
void CSpxUspConnection::OnTelemetryData(std::string&& data, const std::string& requestId)
{
    bool sendTelemetryData = true;

    auto site = GetSite();
    if (site)
    {
        auto props = site->QueryInterface<ISpxNamedProperties>();
        if (props)
        {
            sendTelemetryData = props->GetOr<bool>("SPEECH-TelemetryDataEnabled", true);
        }
    }

    if (m_transport != nullptr && sendTelemetryData)
    {
        m_transport->SendTelemetryData(std::move(data), requestId);
    }
}


CSpxUspConnection::CSpxUspConnection()
    : m_valid(false),
    m_connected(false),
    m_speechContextMessageAllowed(true),
    m_audioOffset(0),
    m_creationTime(telemetry_gettime()),
    m_turnUsingHeaders(false)
{}

void CSpxUspConnection::SetConfiguration(const ClientConfiguration& config)
{
    m_config = std::make_shared<ClientConfiguration>(config);
    
    SPX_DBG_TRACE_VERBOSE("CSpxUspConnection::SetConfiguration Set UspConnection Configuration with SessionId: %s", config.m_connectionId.c_str());
}

CSpxUspConnection::~CSpxUspConnection()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    Shutdown();
}

uint64_t CSpxUspConnection::getTimestamp()
{
    return telemetry_gettime() - m_creationTime;
}

void CSpxUspConnection::Init()
{
    auto threadService = SpxQueryService<ISpxThreadService>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, threadService == nullptr);

    m_threadService = threadService;
}

void CSpxUspConnection::Invoke(std::function<void(CallbacksPtr)> callback)
{
    auto callbacks = m_config->m_callbacks;
    if (callbacks == nullptr || !m_valid)
    {
        return;
    }

    callback(callbacks);
}

void CSpxUspConnection::Shutdown()
{
    if (m_config)
    {
        m_config->m_callbacks = nullptr;
    }

    bool wasConnected = m_connected;

    m_connected = false;
    m_valid = false;

    if (m_transport != nullptr && wasConnected)
    {
        m_transport->Disconnect();
    }
}

bool CSpxUspConnection::IsConnected()
{
    return m_connected;
}

std::string CSpxUspConnection::GetConnectionUrl()
{
    return m_connectionUrl;
}

std::string CSpxUspConnection::ConstructConnectionUrl() const
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    auto recoMode = static_cast<std::underlying_type_t<RecognitionMode>>(m_config->m_recoMode);
    auto region = m_config->m_region;
    std::ostringstream oss;
    bool customEndpoint = false;
    bool customHost = false;
    bool hostSet = false;

    // Using customized endpoint if it is defined.
    if (!m_config->m_customEndpointUrl.empty())
    {
        SPX_TRACE_INFO("Endpoint specified.");
        // Check for invalid use of auth token service endpoint

        bool isTokenServiceEndpoint = false;
        std::string endpointRegion;

        std::tie(isTokenServiceEndpoint, endpointRegion) =
            EndpointUtils::IsTokenServiceEndpoint(m_config->m_customEndpointUrl);

        if (isTokenServiceEndpoint)
        {
            SPX_TRACE_INFO("Detected TokenService Endpoint");
            if (!endpointRegion.empty())
            {
                // Use only the region, defaults for the rest
                region = endpointRegion;
            }
        }
        else
        {
            auto url = HttpUtils::ParseUrl(m_config->m_customEndpointUrl);

            SPX_DBG_TRACE_INFO("Path Specified: %s", url.path.c_str());
            SPX_DBG_TRACE_INFO("Query Parameters Specified: %s", url.query.c_str());
            
            if (!url.path.empty() || !url.query.empty())
            {
                customEndpoint = true;
            }
            else
            {
                SPX_TRACE_INFO("Building endpoint from host");
                oss << HttpUtils::SchemePrefix(url.scheme) << url.host << ':' << url.port;
                hostSet = true;
            }
        }
    }

    if (customEndpoint)
    {
        SPX_TRACE_INFO("Using Custom Endpoint '%s'", m_config->m_customEndpointUrl.c_str());
        oss << m_config->m_customEndpointUrl;
    }
    else if (!m_config->m_customHostUrl.empty())
    {
        // Parse the custom host address
        Url url = HttpUtils::ParseUrl(m_config->m_customHostUrl);

        if (!url.path.empty())
        {
            ThrowInvalidArgumentException("Resource path is not allowed in the host URI.");
        }

        if (!url.query.empty())
        {
            ThrowInvalidArgumentException("Query parameters are not allowed in the host URI.");
        }

        SPX_TRACE_INFO("Building URL with host %s", m_config->m_customHostUrl.c_str());
        oss << HttpUtils::SchemePrefix(url.scheme) << url.host << ':' << url.port;
        customHost = true;
    }
    else if(!hostSet)
    {
        SPX_TRACE_INFO("Building host from region name");
        oss << endpoint::protocol;
    }

    if (!customEndpoint) // standard endpoint, or custom host with standard path
    {
        switch (m_config->m_endpointType)
        {
        case EndpointType::Speech:
            if (!customHost)
            {
                if (!hostSet)
                {
                    oss << GetHostNameSuffix(region, EndpointType::Speech);
                }
                oss << endpoint::unifiedspeech::path;
            }
            else
            {
                oss << endpoint::v1speech::pathPrefix
                    << g_recoModeStrings[recoMode]
                    << endpoint::v1speech::pathSuffix;
            }
            break;

        case EndpointType::StandaloneLanguageId:
            if (!customHost && !hostSet)
            {
                oss << GetHostNameSuffix(region, EndpointType::StandaloneLanguageId);
            }
            if (customHost)
            {
                oss << endpoint::unifiedspeech::unifiedPath; // Containers don't yet support /stt/ in the path
            }
            else
            {
                oss << endpoint::standalonelid::path;
            }
            break;

        case EndpointType::Translation:
            if (!customHost && !hostSet)
            {
                oss << GetHostNameSuffix(region, EndpointType::Translation);
            }
            oss << endpoint::unifiedspeech::path;
            break;

        case EndpointType::Dialog:
            /* This will generate an url of the form wss://<region>.convai.speech.microsoft.com/<backend>/api/<version> */
            if (!customHost)
            {
                oss << region
                    << endpoint::dialog::hostnameSuffix;
            }
            { // must be enclosed in a block to avoid compiler errors about jumping past a declaration with an initializer
                const char* resource = nullptr;
                const char* version = nullptr;
                if (m_config->m_dialogBackend == ClientConfiguration::DialogBackend::BotFramework)
                {
                    resource = endpoint::dialog::resourcePath::botFramework;
                    version = endpoint::dialog::version::botFramework;
                }
                else if (m_config->m_dialogBackend == ClientConfiguration::DialogBackend::CustomCommands)
                {
                    resource = endpoint::dialog::resourcePath::customCommands;
                    version = endpoint::dialog::version::customCommands;
                }
                else
                {
                    ThrowInvalidArgumentException("Invalid dialog backend.");
                }

                oss << resource
                    << endpoint::dialog::suffix
                    << version;
            }
            break;

        case EndpointType::ConversationTranscriptionService:
        case EndpointType::DynamicConversationTranscriptionService:
            if (!customHost)
            {
                oss << endpoint::conversationTranscriber::hostnamePrefix
                    << region
                    << endpoint::conversationTranscriber::hostnameSuffix;
            }

            oss << endpoint::conversationTranscriber::pathPrefix
                << (m_config->m_endpointType == EndpointType::ConversationTranscriptionService
                    ? endpoint::conversationTranscriber::pathSuffixMultiAudio
                    : endpoint::conversationTranscriber::pathSuffixDynamic);
            break;

        case EndpointType::ConversationTranscriptionServiceV2:
            if (!customHost && !hostSet)
            {
                oss << region << endpoint::conversationTranscriberV2::hostnameSuffix;
            }
            if (customHost)
            {
                oss << endpoint::unifiedspeech::unifiedPath; // Containers don't yet support /stt/ in the path
            }
            else
            {
                oss << endpoint::conversationTranscriberV2::path;
            }
            break;

        case EndpointType::SpeechSynthesis:
            if (!customHost && !hostSet)
            {
                if (m_config->m_queryParameters.find(endpoint::speechSynthesis::deploymentIdQueryParam) != m_config->m_queryParameters.end())
                {
                    oss << GetHostNameSuffix(region, EndpointType::CustomVoice);
                }
                else
                {
                    oss << GetHostNameSuffix(region, EndpointType::SpeechSynthesis);
                }
            }
            if (customHost)
            {
                oss << endpoint::speechSynthesis::hostPath;
            }
            else
            {
                oss << endpoint::speechSynthesis::path;
            }
            break;

        default:
            ThrowInvalidArgumentException("Unknown endpoint type.");
        }
    }

    // Appends user defined query parameters first.
    if (!m_config->m_userDefinedQueryParameters.empty())
    {
        oss << queryParameterDelim << m_config->m_userDefinedQueryParameters;
        customEndpoint = true;
    }
    // Note the use of customEndpoint in the code below for building query params
    // - customEndpoint true:  endpoint is specified by FromEndpoint, or m_config->m_userDefinedQueryParameters is non-empty
    // - customEndpoint false: endpoint is not specified by FromEndpoint and m_config->m_userDefinedQueryParameters is empty

    switch (m_config->m_endpointType)
    {
    case EndpointType::ConversationTranscriptionServiceV2:
        BuildQueryParameters(endpoint::conversationTranscriberV2::queryParameters, m_config->m_queryParameters, customEndpoint, oss);
        break;

    case EndpointType::ConversationTranscriptionService:
    case EndpointType::DynamicConversationTranscriptionService:
        BuildQueryParameters(endpoint::conversationTranscriber::queryParameters, m_config->m_queryParameters, customEndpoint, oss);
        break;

    case EndpointType::Speech:
    case EndpointType::StandaloneLanguageId:
        if (m_config->GetIsCustomV1Endpoint() || m_config->GetIsCustomHost())
        {
            BuildQueryParameters(endpoint::v1speech::queryParameters, m_config->m_queryParameters, customEndpoint, oss);
        }
        else
        {
            BuildQueryParameters(endpoint::unifiedspeech::queryParameters, m_config->m_queryParameters, customEndpoint, oss);
        }
        break;
    case EndpointType::Translation:
        BuildQueryParameters(endpoint::unifiedspeech::queryParameters, m_config->m_queryParameters, customEndpoint, oss);
        break;
    case EndpointType::TranslationV1:
            for (auto queryParameterName : endpoint::translationV1::queryParameters)
            {
                if (!customEndpoint || !contains(oss.str(), queryParameterName))
                {
                    auto entry = m_config->m_queryParameters.find(queryParameterName);
                    if (entry != m_config->m_queryParameters.end() && !entry->second.empty())
                    {
                        // Need to use separate parameter for each target language.
                        if (strcmp(queryParameterName, endpoint::translationV1::toQueryParam) == 0)
                        {
                            auto langVector = PAL::split(entry->second, CommaDelim);
                            for (auto item : langVector)
                            {
                                oss << queryParameterDelim << endpoint::translationV1::toQueryParam << HttpUtils::UrlEscape(item);
                            }
                        }
                        // Voice need 2 query parameters.
                        else if (strcmp(queryParameterName, endpoint::translationV1::voiceQueryParam) == 0)
                        {
                            oss << queryParameterDelim << endpoint::translationV1::featuresQueryParam << endpoint::translationV1::requireVoice;
                            oss << queryParameterDelim << endpoint::translationV1::voiceQueryParam << HttpUtils::UrlEscape(entry->second);
                        }
                        else
                        {
                            oss << queryParameterDelim << queryParameterName << entry->second;
                        }
                    }
                }
            }
        break;

    case EndpointType::Dialog:
        if (m_config->m_dialogBackend == ClientConfiguration::DialogBackend::BotFramework)
        {
            BuildQueryParameters(endpoint::dialog::botFramework::queryParameters, m_config->m_queryParameters, customEndpoint, oss);
        }
        else if (m_config->m_dialogBackend == ClientConfiguration::DialogBackend::CustomCommands)
        {
            BuildQueryParameters(endpoint::dialog::customCommands::queryParameters, m_config->m_queryParameters, customEndpoint, oss);
        }
        break;

    case EndpointType::SpeechSynthesis:
    case EndpointType::CustomVoice:
        BuildQueryParameters(endpoint::speechSynthesis::queryParameters, m_config->m_queryParameters, customEndpoint, oss);
        break;
    }

    auto urlStr = oss.str();
    auto firstQueryDelim = urlStr.find_first_of("?&");
    if (firstQueryDelim != std::string::npos)
    {
        urlStr[firstQueryDelim] = '?';
    }

    return urlStr;
}

void CSpxUspConnection::Connect()
{
    SPX_TRACE_INFO("%s: entering...", __FUNCTION__);
    if (m_transport != nullptr || m_valid)
    {
        ThrowLogicError("USP connection already created.");
    }

    HttpEndpointInfo endpoint;

    if (!m_config->m_audioResponseFormat.empty())
    {
        endpoint.SetHeader(headers::audioResponseFormat, m_config->m_audioResponseFormat);
    }

    // Set authentication headers.
    auto& authStr = m_config->m_authData[static_cast<size_t>(AuthenticationType::SubscriptionKey)];
    if (!authStr.empty())
    {
        SPX_TRACE_INFO("Adding subscription key headers");
        endpoint.SetHeader(headers::ocpApimSubscriptionKey, authStr);
    }
    authStr = m_config->m_authData[static_cast<size_t>(AuthenticationType::AuthorizationToken)];
    if (!authStr.empty())
    {
        SPX_TRACE_INFO("Adding authorization token headers");
        auto token = "Bearer " + authStr;
        endpoint.SetHeader(headers::authorization, token);
    }
    authStr = m_config->m_authData[static_cast<size_t>(AuthenticationType::SearchDelegationRPSToken)];
    if (!authStr.empty())
    {
        SPX_TRACE_INFO("Adding search delegation RPS token.");
        endpoint.SetHeader(headers::searchDelegationRPSToken, authStr);
    }

    if (m_config->m_endpointType == EndpointType::Dialog)
    {
        auto& region = m_config->m_region;
        if (!region.empty())
        {
            SPX_TRACE_INFO("Adding region header");
            endpoint.SetHeader(headers::region, region);
        }
    }

    for (const auto& header : m_config->m_userDefinedHttpHeaders)
    {
        SPX_TRACE_INFO("Set a user defined HTTP header '%s':'%s'", header.first.c_str(), header.second.c_str());
        endpoint.SetHeader(header.first, header.second);
    }

    for (const auto& option : m_config->m_underlyingOptions)
    {
        SPX_TRACE_INFO("Set an underlying io option '%s'", option.first.c_str());
        endpoint.SetUnderlyingOption(option.first, option.second);
    }

    // TODO: should probably update the ConstructConnectionUrl to set the scheme, host, path and query parameters
    //       directly on the endpoint instance
    m_connectionUrl = ConstructConnectionUrl();
    endpoint.EndpointUrl(m_connectionUrl);
    if (endpoint.Scheme() != UriScheme::WS && endpoint.Scheme() != UriScheme::WSS)
    {
        if (endpoint.IsSecure())
        {
            SPX_TRACE_INFO("Secure connection requested, but the scheme is not WSS. Changing to WSS.");
            endpoint.Scheme(UriScheme::WSS);
        }
        else
        {
            SPX_TRACE_INFO("Insecure connection requested, but the scheme is not WS. Changing to WS.");
            endpoint.Scheme(UriScheme::WS);
        }
    }

    m_connectionUrl = endpoint.EndpointUrl();
    SPX_TRACE_INFO("connectionUrl=%s", m_connectionUrl.c_str());

    // Set the proxy configuration
    endpoint.Proxy(m_config->m_proxyServerInfo.get());
    endpoint.BypassProxyFor(m_config->m_proxyBypass);

    EnsureTelemetry();

    std::string connectionId = m_config->m_connectionId;

    // Log the device uuid
    MetricsDeviceStartup(m_telemetry, connectionId, PAL::DeviceUuid());

#ifdef SPEECHSDK_USE_OPENSSL
    if (!m_config->m_trustedCert.empty())
    {
        endpoint
            .DisableDefaultVerifyPaths(true)
            .SingleTrustedCertificate(m_config->m_trustedCert);
    }
    if (m_config->m_disable_crl_check)
    {
        endpoint.DisableCrlChecks(true);
    }
    if (m_config->m_continue_on_crl_download_failure)
    {
        endpoint.ContinueOnCrlDownloadFailure(true);
    }

    endpoint.MaxCRLDownloadSizeOption(m_config->m_max_crl_download_size_in_kb);
#endif

    m_transport = UspWebSocket::Create(
        m_threadService,
        ISpxThreadService::Affinity::Background,
        std::chrono::milliseconds{ m_config->m_pollingIntervalms },
        m_telemetry,
        GetSite());

    auto shared = ISpxInterfaceBaseFor<CSpxUspConnection>::shared_from_this();

    m_transport->OnConnected.Add(shared, &CSpxUspConnection::OnTransportOpened);
    m_transport->OnDisconnected.Add(shared, &CSpxUspConnection::OnTransportClosed);
    m_transport->OnError.Add(shared, &CSpxUspConnection::OnTransportError);
    m_transport->OnUspBinaryData.Add(shared, &CSpxUspConnection::OnTransportBinaryData);
    m_transport->OnUspTextData.Add(shared, &CSpxUspConnection::OnTransportTextData);
    m_transport->OnEstimatedUploadRateKBPerSec.Add(shared, &CSpxUspConnection::OnTransportEstimatedUploadRate);

    // set the connection ID header and protocol
    endpoint
        .SetHeader("X-ConnectionId", connectionId)
        .AddWebSocketProtocol("USP");

    // open the web socket connection
    m_valid = true;
    m_transport->Connect(endpoint, connectionId);
}

void CSpxUspConnection::EnsureTelemetry()
{
    auto telemetryStore = SpxQueryService<ISpxTelemetryStore>(GetSite());
    SPX_DBG_ASSERT(telemetryStore);

    auto storedTelemetry = telemetryStore->FetchTelemetry();

    if (storedTelemetry == nullptr)
    {
        storedTelemetry = SpxCreateObject<ISpxTelemetryBase>("CSpxTelemetry", SpxGetRootSite());
        telemetryStore->StoreTelemetry(storedTelemetry);
    }

    m_telemetry = storedTelemetry->QueryInterface<ISpxWebSocketTelemetry>();

    if (m_telemetry == nullptr)
    {
        ThrowRuntimeError("Failed to create telemetry instance.");
    }
}

std::string CSpxUspConnection::CreateRequestId()
{
    auto requestId = PAL::CreateGuidWithoutDashesUTF8();

    SPX_TRACE_INFO("RequestId: '%s'", requestId.c_str());
    RegisterRequestId(requestId);

    return requestId;
}

void CSpxUspConnection::RegisterRequestId(const std::string& requestId)
{
    MetricsTransportRequestId(m_telemetry.get(), requestId.c_str());
    m_activeRequestIds.insert(requestId);
}

void CSpxUspConnection::QueueMessage(std::unique_ptr<Message> message)
{
    if (message->Path().empty())
    {
        ThrowInvalidArgumentException("The path is null or empty.");
    }

    if (!m_valid || m_transport == nullptr)
    {
        return;
    }

    // According to USP protocol, speech.context must be sent before any audio in a turn, and
    // only one speech.context message is allowed in the same turn.
    if (message->MessageType() == MessageType::Context)
    {
        // QueueMessage() is serialized by ThreadService, no lock is needed.
        if (!m_speechContextMessageAllowed && m_config->m_endpointType != EndpointType::SpeechSynthesis)
        {
            ThrowLogicError("Error trying to send a context message while in the middle of a speech turn.");
        }
        else
        {
            // Only one speech.context in the same turn.
            m_speechContextMessageAllowed = false;
        }
    }

    std::string usedRequestId = message->RequestId();
    if (usedRequestId.empty())
    {
        usedRequestId = UpdateRequestId(message->MessageType(), message->IsBinary());
    }
    else if (message->Path() == "synthesis.context")
    {
        // TODO: This logic should be cleaned up. If we want to
        //       set the request ID we should be explicit about it e.g. call a
        //       SetRequestId(...) method
        m_speechRequestId = usedRequestId;
    }

    message->RequestId(usedRequestId);

    if (m_transport)
    {
        m_transport->SendData(std::move(message));
    }
}

std::string CSpxUspConnection::UpdateRequestId(const MessageType messageType, bool binary)
{
    // The config message does not require a X-RequestId header, because this message is not associated with a particular request.
    // Other messages, such as speech.event, speech.context etc need a request id.
    std::string requestId;

    switch (messageType)
    {
    case MessageType::Config:
        break;

    case MessageType::AgentContext:
        requestId = CreateRequestId();
        m_speechRequestId = requestId;
        break;

    case MessageType::Context:
        if (m_config->m_endpointType == EndpointType::ConversationTranscriptionService)
        {
            // For ConversationTranscription, speech.event might be sent before speech.context, so m_speechRequestId might already be set.
            // If this is fixed, the following code should be removed, i.e. instead of creating requestId, a LogicError should be thrown,
            if (m_speechRequestId.empty())
            {
                m_speechRequestId = CreateRequestId();
            }
        }
        else
        {
            // For other services, speech.event must be associated to an ongoing speech turn. Only speech.context or audio can kick-off a new turn.
            // And speech.context must be sent before audio, so m_speechRequestId must be empty at this time.
            if (!m_speechRequestId.empty())
            {
                ThrowLogicError("Speech.Context must be the first message in a turn, and m_speechRequestId must be empty.");
            }
            m_speechRequestId = CreateRequestId();
        }
        requestId = m_speechRequestId;
        break;

    case MessageType::SpeechEvent:
        // According to USP, SpeechEvent is associated with the current speech turn. So m_speechRequestId must be non-empty.
        // However, the current conversation transcriber will send speech.event either before a turn (before audio/speech.context)
        // or outside a turn (after turn.end).
        if (m_config->m_endpointType == EndpointType::ConversationTranscriptionService)
        {
            if (m_speechRequestId.empty())
            {
                m_speechRequestId = CreateRequestId();
            }
        }
        if (m_speechRequestId.empty())
        {
            ThrowLogicError("Speech.event must be associated to the current speech turn, so m_speechRequestId must be non-empty.");
        }
        requestId = m_speechRequestId;
        break;

    case MessageType::Event:
        requestId = CreateRequestId();
        break;

    case MessageType::Agent:
        requestId = CreateRequestId();
        break;

    case MessageType::Ssml:
        if (m_speechRequestId.empty())
        {
            ThrowLogicError("Request ID is required for speech.synthesis request, so m_speechRequestId must be non-empty when sending SSML request.");
        }

        requestId = m_speechRequestId;
        break;

    default:
        // Unknown messages that are binary will get an existing request ID if one is available.
        if (binary)
        {
            if (m_speechRequestId.empty())
            {
                m_speechRequestId = CreateRequestId();
            }

            requestId = m_speechRequestId;
        }
        else
        {
            requestId = CreateRequestId();
        }
        break;
    }

    SPX_TRACE_INFO("Create requestId %s for messageType %d", requestId.c_str(), static_cast<int>(messageType));
    return requestId;
}

void CSpxUspConnection::QueueAudioSegment(const DataChunkPtr& audioChunk)
{
    auto size = audioChunk->size;
    if (size == 0)
    {
        QueueAudioEnd();
        return;
    }

    SPX_TRACE_INFO("TS:%" PRIu64 ", Write %" PRIu32 " bytes audio data.", getTimestamp(), size);

    throw_if_null(audioChunk->data.get(), "data");

    if (!m_valid)
    {
        return;
    }

    // after sending audio message, no speech.context is allowed in the same turn.
    if (m_speechContextMessageAllowed)
    {
        m_speechContextMessageAllowed = false;
    }

    MetricsAudioStreamData(size);

    bool newTurn = m_audioOffset == 0;
    if (newTurn)
    {
        // The service uses the first audio message that contains a unique request identifier to signal the start of a new request/response cycle or turn.
        // After receiving an audio message with a new request identifier, the service discards any queued or unsent messages
        // that are associated with any previous turn.
        m_speechRequestId = m_speechRequestId.empty() ? CreateRequestId() : m_speechRequestId;
        SPX_TRACE_INFO("The current speech request id is %s", m_speechRequestId.c_str());

        MetricsAudioStreamInit();
        MetricsAudioStart(m_telemetry, m_speechRequestId);
    }

    if (m_transport)
    {
        m_transport->SendAudioData(path::audio, audioChunk, m_speechRequestId, newTurn);
    }

    m_audioOffset += size;
}

void CSpxUspConnection::QueueAudioEnd()
{
    SPX_TRACE_INFO("TS:%" PRIu64 ", Flush audio buffer.", getTimestamp());

    if (!m_valid || m_audioOffset == 0)
    {
        return;
    }

    // no speech.context is allowed after audio.end
    if (m_speechContextMessageAllowed)
    {
        m_speechContextMessageAllowed = false;
    }

    std::exception_ptr ex;
    try
    {
        if (m_transport)
        {
            m_transport->SendAudioData(path::audio, std::make_shared<DataChunk>(nullptr, 0), m_speechRequestId);
        }
    }
    catch (const std::exception& innerEx)
    {
        ex = std::make_exception_ptr(ExceptionWithCallStack{ std::string("QueueAudioEnd failed with an exception: ") + innerEx.what(), SPXERR_RUNTIME_ERROR });
    }
    catch (...)
    {
        ex = std::make_exception_ptr(ExceptionWithCallStack{ "QueueAudioEnd failed with an unknown throwable", SPXERR_RUNTIME_ERROR });
    }

    m_audioOffset = 0;
    MetricsAudioStreamFlush();
    MetricsAudioEnd(m_telemetry, m_speechRequestId);

    if (ex)
    {
        std::rethrow_exception(ex);
    }
}

void CSpxUspConnection::WriteTelemetryLatency(uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency)
{
    if (m_valid)
    {
        MetricsResultLatency(m_telemetry, m_speechRequestId, latencyInTicks, isPhraseLatency, isFirstHypothesisLatency);
    }
    else
    {
        SPX_TRACE_ERROR("%s: m_valid is false.", __FUNCTION__);
    }
}

void CSpxUspConnection::FlushTelemetry()
{
    SPX_TRACE_FUNCTION();

    if (IsConnected() && m_telemetry != nullptr)
    {
        auto shared = ISpxInterfaceBaseFor<CSpxUspConnection>::shared_from_this();

        std::string requestId = m_speechRequestId.empty() ? CreateRequestId() : m_speechRequestId;
        SPX_DBG_TRACE_VERBOSE("%s: Using requestId: %s", __FUNCTION__, requestId.c_str());

        m_telemetry->Flush(requestId, [shared](std::string&& data, const std::string& reqId)
            {
                shared->OnTelemetryData(std::move(data), reqId);
            });
    }
}

// Callback for transport opened
void CSpxUspConnection::OnTransportOpened(const std::string &url)
{
    if (m_connected)
    {
        SPX_TRACE_ERROR("TS:%" PRIu64 ", connection:0x%p is already connected!!!", getTimestamp(), (void*)this);
    }

    //assert(m_connected == false);
    m_connected = true;

    // Switch to the run-time polling interval now that connection is established.
    // Only switch if the run interval differs from the connect interval.
    if (m_config->HasRunPollingInterval() &&
        m_config->m_runPollingIntervalms != m_config->m_pollingIntervalms &&
        m_transport != nullptr)
    {
        SPX_TRACE_INFO("TS:%" PRIu64 ", connection:0x%p switching polling interval: %u ms -> %u ms",
            getTimestamp(), (void *)this, m_config->m_pollingIntervalms, m_config->m_runPollingIntervalms);
        m_transport->SetPollingInterval(std::chrono::milliseconds{ m_config->m_runPollingIntervalms });
    }

    SPX_TRACE_INFO("TS:%" PRIu64 ", OnConnected: connection:0x%p", getTimestamp(), (void *)this);
    Invoke([&](auto callbacks) {
        callbacks->OnConnected(url);
        });
}

// Callback for transport closed
void CSpxUspConnection::OnTransportClosed(WebSocketDisconnectReason reason, const std::string& details, bool serverRequested)
{
    if (m_connected)
    {
        m_connected = false;
        SPX_TRACE_INFO("TS:%" PRIu64 ", OnDisconnected: connection:0x%p, Reason: %d, Server Requested: %d, Details: %s",
            getTimestamp(), (void*)this, static_cast<int>(reason), serverRequested, details.c_str());

        auto error = ErrorInfo::FromWebSocket(serverRequested ? WebSocketError::REMOTE_CLOSED : WebSocketError::UNKNOWN, reason, details);
        auto callbacks = m_config->m_callbacks;

        Invoke([&error](auto callbacks) {
            callbacks->OnDisconnected(error);
            });
    }
}

// Callback for transport errors
void CSpxUspConnection::OnTransportError(const std::shared_ptr<ISpxErrorInformation>& error)
{
    const auto errorCode = error != nullptr ? error->GetCancellationCode() : CancellationErrorCode::NoError;
    const auto& errorMessage = error != nullptr ? error->GetDetails() : "";

    SPX_TRACE_INFO("TS:%" PRIu64 ", TransportError: connection:0x%p, code=%d, string=%s",
        getTimestamp(), (void*)this, static_cast<int>(errorCode), errorMessage.c_str());

    if (m_connected)
    {
        m_connected = false;
        SPX_TRACE_INFO("TS:%" PRIu64 ", OnDisconnected: connection:0x%p", getTimestamp(), (void*)this);
        Invoke([&error](auto callbacks) {
            callbacks->OnDisconnected(error);
            });
    }

    Invoke([&error](auto callbacks) {
        callbacks->OnError(error);
        });

    m_valid = false;
}

// Callback for data available on transport
void CSpxUspConnection::OnTransportData(bool isBinary, const UspHeaders& headers, const unsigned char* buffer, size_t bufferSize)
{
    if (buffer == nullptr)
    {
        return;
    }

    bool messageHadContinuationHeader = false;
    std::ostringstream msgTrace;

    auto path = TryGet(headers, HEADER_PATH);
    if (path.empty())
    {
        PROTOCOL_VIOLATION("response missing '%s' header", HEADER_PATH);
        return;
    }
    msgTrace << "UspResponseMsg " << HEADER_PATH << ": " << path;

    try
    {
        // TODO: Should we update the message received to take the parsed unordered map of headers instead?
        Invoke([&](auto callbacks)
            {
                callbacks->OnMessageReceived({ GetHeadersAsString(headers), path, buffer, (uint32_t)bufferSize, isBinary });
            });
    }
    catch (const std::exception& ex)
    {
        // shouldn't fail because of a bad callback
        SPX_TRACE_ERROR("OnMessageReceived callback failed with exception: '%s'", ex.what());
    }

    std::string requestId = TryGet(headers, headers::requestId);
    if (!m_activeRequestIds.count(requestId))
    {
        if (requestId.empty() || path != path::turnStart)
        {
            PROTOCOL_VIOLATION("Unexpected request id '%s', Path: %s", requestId.c_str(), path.c_str());
            MetricsUnexpectedRequestId(requestId);
            return;
        }
        else
        {
            SPX_TRACE_INFO("Service originated request received with requestId: %s", requestId.c_str());
            RegisterRequestId(requestId);
        }
    }

    std::string contentType;
    if (bufferSize != 0)
    {
        contentType = TryGet(headers, headers::contentType);
        if (contentType.empty())
        {
            SPX_TRACE_INFO("response '%s' contains body with no content-type", path.c_str());
        }
        else
        {
            SPX_TRACE_INFO("Response Message: content type: %s.", contentType.c_str());
        }
    }

    MetricsReceivedMessage(m_telemetry, requestId, path);

    auto headerOffset = TryGet(headers, headers::continuationOffset);
    OffsetType offsetFromHeader = 0;

    if (!headerOffset.empty())
    {
        msgTrace << " | " << headers::continuationOffset << ": " << headerOffset;
        offsetFromHeader = stoull(headerOffset);
        Invoke([&](auto callbacks) { callbacks->OnAcknowledgedAudio(offsetFromHeader); });
        messageHadContinuationHeader = true;
    }

    auto token = TryGet(headers, headers::continuationToken);
    if (!token.empty())
    {
        msgTrace << " | " << headers::continuationToken << ": " << token;
        Invoke([&](auto callbacks) { callbacks->OnToken(token); });
        messageHadContinuationHeader = true;
    }

    SPX_TRACE_INFO("TS:%" PRIu64 " Response Message: path: %s, size: %zu.", getTimestamp(), path.c_str(), bufferSize);

    if (isBinary)
    {
        if (path == path::translationSynthesis || path == path::audio)
        {
            // streamId is optional
            auto streamId = TryGet(headers, headers::streamId);

            AudioOutputChunkMsg msg;
            if (path == path::audio && m_streamIdLangMap.size() > 0)
            {
                SPX_DBG_TRACE_VERBOSE("m_streamIdLangMap has data, will FillLanguageForAudioOutputChunkMsg");
                FillLanguageForAudioOutputChunkMsg(streamId, path, msg);
            }
            msg.requestId = requestId;
            msg.audioBuffer = (uint8_t*)buffer;
            msg.audioLength = bufferSize;

            Invoke([&](auto callbacks) { callbacks->OnAudioOutputChunk(msg); });
        }
        else
        {
            PROTOCOL_VIOLATION("Binary frame received with unexpected path: %s", path.c_str());
        }
    }
    else
    {
        auto json = ajv::json::Parse((char*)buffer, bufferSize);
        auto jsonStr = json.AsJson();
        jsonStr.erase(std::remove(jsonStr.begin(), jsonStr.end(), '\n'), jsonStr.end());
        msgTrace << " | msgBuffer: " << jsonStr;

        if (path == path::speechStartDetected || path == path::speechEndDetected)
        {
            SPX_DBG_TRACE_VERBOSE("%s", msgTrace.str().c_str());

            auto offsetObj = json[json_properties::offset];
            // For whatever reason, offset is sometimes missing on the end detected message.
            auto offset = offsetObj.AsUint<OffsetType>();

            if (path == path::speechStartDetected)
            {
                Invoke([&](auto callbacks) { callbacks->OnSpeechStartDetected({ json.AsJson(), offset }); });
            }
            else
            {
                // Not sure this doesn't belong here....
                // connection->Invoke([&] { callbacks->OnAcknowledgedAudio(offset); });
                Invoke([&](auto callbacks) { callbacks->OnSpeechEndDetected({ json.AsJson(), offset }); });
            }
        }
        else if (path == path::turnStart)
        {
            SPX_DBG_TRACE_VERBOSE("%s", msgTrace.str().c_str());

            auto tag = json[json_properties::context][json_properties::tag].AsString();

            m_turnUsingHeaders = messageHadContinuationHeader;

            if (requestId == m_speechRequestId)
            {
                /* We know this request id is related to a speech turn */
                Invoke([&](auto callbacks) { callbacks->OnTurnStart({ json.AsJson(), tag, requestId, m_turnUsingHeaders, offsetFromHeader }); });
            }
            else
            {
                /* We know this request id is a server initiated request */
                Invoke([&](auto callbacks) { callbacks->OnMessageStart({ json.AsJson(), tag, requestId, m_turnUsingHeaders, offsetFromHeader }); });
            }
        }
        else if (path == path::turnEnd)
        {
            SPX_DBG_TRACE_VERBOSE("%s", msgTrace.str().c_str());

            {
                m_activeRequestIds.erase(requestId);
                SPX_DBG_TRACE_VERBOSE("Got turn end, clear m_streamIdLangMap.");
                m_streamIdLangMap.clear();
            }

            auto shared = ISpxInterfaceBaseFor<CSpxUspConnection>::shared_from_this();

            // flush the telemetry before invoking the onTurnEnd callback.
            m_telemetry->Flush(requestId, [shared](std::string&& data, const std::string& requestId)
                {
                    shared->OnTelemetryData(std::move(data), requestId);
                });

            if (requestId == m_speechRequestId)
            {
                m_speechRequestId.clear();
                m_speechContextMessageAllowed = true;
                Invoke([&](auto callbacks) { callbacks->OnTurnEnd({ requestId }); });
            }
            else
            {
                Invoke([&](auto callbacks) { callbacks->OnMessageEnd({ requestId }); });
            }
        }
        else if (path == path::speechKeyword)
        {
            auto status = ToKeywordVerificationStatus(json[json_properties::status].AsString());
            auto offsetObj = json[json_properties::offset];
            auto offset = offsetObj.IsNull() ? 0 : offsetObj.AsUint<OffsetType>();
            auto durationObj = json[json_properties::duration];
            auto duration = durationObj.IsNull() ? 0 : durationObj.AsUint<uint64_t>();
            auto textObj = json[json_properties::text];
            auto text = textObj.IsNull() ? "" : textObj.AsString();

            Invoke([&](auto callbacks) {
                callbacks->OnSpeechKeywordDetected({ json.AsJson(),
                                                    offset,
                                                    duration,
                                                    status,
                                                    std::move(text) });
                });
        }
        else if (path == path::speechHypothesis
            || path == path::speechFragment
            || path == path::speechTentativePhrase)
        {
            auto offset = json[json_properties::offset].AsUint<OffsetType>();
            auto duration = json[json_properties::duration].AsUint<uint64_t>();
            std::string text;
            bool isTentative;
            // If the EPR is set through the service params and if we receive a tentative phrase
            // we will still process that as hypothesis with a property in the result as
            // "SpeechServiceResponse_IsTentativePhrase" equals true.
            // To set this client needs to set the service query params like the following
            // config.SetServiceProperty("SpeechContext-phraseOutput.tentativePhraseResults", "Always", ServicePropertyChannel.UriQueryParameter);
            if (path == path::speechTentativePhrase)
            {
                text = json[json_properties::displayText].AsString();
                isTentative = true;
            }
            else
            {
                text = json[json_properties::text].AsString();
                isTentative = false;
            }

            auto speaker = json[json_properties::speaker].AsString();
            auto utteranceId = json[json_properties::utteranceId].AsString();
            auto phraseId = json[json_properties::phraseId].AsString();

            auto language = RetrievePrimaryLanguage(json, path);
            if (path == path::speechHypothesis ||
                path == path::speechTentativePhrase)
            {
                Invoke([&](auto callbacks) {
                    callbacks->OnSpeechHypothesis({ json.AsJson(),
                                                   offset,
                                                   duration,
                                                   std::move(text),
                                                   std::move(speaker),
                                                   std::move(utteranceId),
                                                   std::move(language),
                                                   std::move(phraseId),
                                                   isTentative });
                    });
            }
            else
            {
                Invoke([&](auto callbacks) {
                    callbacks->OnSpeechFragment({ json.AsJson(),
                                                 offset,
                                                 duration,
                                                 std::move(text),
                                                 std::move(speaker),
                                                 std::move(utteranceId),
                                                 std::move(language),
                                                 std::move(phraseId) });
                    });
            }
        }
        else if (path == path::speechPhrase)
        {
            SPX_DBG_TRACE_VERBOSE("%s", msgTrace.str().c_str());

            SpeechPhraseMsg result = RetrieveSpeechPhraseResult(json.Reader());
            if (isErrorRecognitionStatus(result.recognitionStatus))
            {
                InvokeRecognitionErrorCallback(result.recognitionStatus, json.AsJson());
            }
            else
            {
                Invoke([&](auto callbacks) { callbacks->OnSpeechPhrase(result); });
            }

            if (!m_turnUsingHeaders)
            {
                // Tell our site we're done with the audio.
                Invoke([&](auto callbacks) { callbacks->OnAcknowledgedAudio(result.offset + result.duration); });
            }
        }
        else if (path == path::translationHypothesis)
        {
            auto speechResult = RetrieveSpeechResult(json.Reader());
            auto translationResult = RetrieveTranslationResult(json[json_properties::translation], false);
            // TranslationStatus is always success for translation.hypothesis
            translationResult.translationStatus = TranslationStatus::Success;

            Invoke([&](auto callbacks) {
                callbacks->OnTranslationHypothesis({ std::move(speechResult.json),
                                                    speechResult.offset,
                                                    speechResult.duration,
                                                    std::move(speechResult.text),
                                                    std::move(translationResult) });
                });
        }
        else if (path == path::translationPhrase)
        {
            auto status = ToRecognitionStatus(json[json_properties::recoStatus].AsString());
            if (isErrorRecognitionStatus(status))
            {
                // There is an error in speech recognition, fire an error event.
                InvokeRecognitionErrorCallback(status, json.AsJson());
            }
            else
            {
                auto speechResult = RetrieveSpeechResult(json.Reader());
                TranslationResult translationResult;
                if (status == RecognitionStatus::Success)
                {
                    translationResult = RetrieveTranslationResult(json[json_properties::translation], true);
                }
                else
                {
                    translationResult.translationStatus = TranslationStatus::Success;
                }
                // There is no speech recognition error, we fire a translation phrase event.
                Invoke([&](auto callbacks) {
                    callbacks->OnTranslationPhrase({ std::move(speechResult.json),
                                                    speechResult.offset,
                                                    speechResult.duration,
                                                    std::move(speechResult.text),
                                                    std::move(translationResult),
                                                    status });
                    });

                if (!m_turnUsingHeaders)
                {
                    // Tell our site we're done with the audio.
                    Invoke([&](auto callbacks) { callbacks->OnAcknowledgedAudio(speechResult.offset + speechResult.duration); });
                }
            }
        }
        else if (path == path::translationSynthesisEnd)
        {
            std::string failureReason;
            bool synthesisSuccess = false;

            auto statusHandle = json[json_properties::synthesisStatus];
            if (statusHandle.IsOk())
            {
                auto synthesisStatus = statusHandle.AsString();
                if (synthesisStatus != "Success" && synthesisStatus != "Error")
                {
                    PROTOCOL_VIOLATION("Invalid synthesis status in synthesis.end message. Json=%s", json.AsJson().c_str());
                    failureReason = "Invalid synthesis status in synthesis.end message.";
                }
                synthesisSuccess = synthesisStatus == "Success";
            }
            else
            {
                PROTOCOL_VIOLATION("No synthesis status in synthesis.end message. Json=%s", json.AsJson().c_str());
                failureReason = "No synthesis status in synthesis.end message.";
            }

            auto failureHandle = json[json_properties::failureReason];
            if (failureHandle.IsOk())
            {
                if (synthesisSuccess)
                {
                    PROTOCOL_VIOLATION("FailureReason should be empty if SynthesisStatus is success. Json=%s", json.AsJson().c_str());
                }
                failureReason = failureReason + failureHandle.AsString();
            }

            if (synthesisSuccess)
            {
                AudioOutputChunkMsg msg;
                msg.audioBuffer = NULL;
                msg.audioLength = 0;
                msg.requestId = requestId;
                Invoke([&](auto callbacks) { callbacks->OnAudioOutputChunk(msg); });
            }
            else
            {
                auto error = ErrorInfo::FromExplicitError(CancellationErrorCode::ServiceError, failureReason);
                Invoke([&](auto callbacks) { callbacks->OnError(error); });
            }
        }
        else if (path == path::translationResponse)
        {
            if (json[json_properties::speechHypothesis].IsOk())
            {
                SPX_DBG_TRACE_INFO("Got translation response for %s.", json_properties::speechHypothesis);
                auto speechHypothesisJson = json[json_properties::speechHypothesis];
                auto speechHypothesisMsg = RetrieveSpeechResult(speechHypothesisJson);
                auto translationResult = RetrieveTranslationResult(json.Reader(), true);
                if (translationResult.translationStatus != TranslationStatus::Success)
                {
                    PROTOCOL_VIOLATION("Translation result for hypothesis should always have success status, otherwise, service shouldn't have sent it. Json=%s", json.AsJson().c_str());
                }
                else
                {
                    Invoke([&](auto callbacks) {
                        callbacks->OnTranslationHypothesis({ json.AsJson(),
                                                            speechHypothesisMsg.offset,
                                                            speechHypothesisMsg.duration,
                                                            std::move(speechHypothesisMsg.text),
                                                            std::move(translationResult),
                                                            std::move(speechHypothesisMsg.language) });
                        });
                }
            }
            else if (json[json_properties::speechPhrase].IsOk())
            {
                SPX_DBG_TRACE_INFO("Got translation response for %s.", json_properties::speechPhrase);
                auto speechPhraseJson = json[json_properties::speechPhrase];
                auto status = ToRecognitionStatus(speechPhraseJson[json_properties::recoStatus].AsString());
                if (isErrorRecognitionStatus(status))
                {
                    // There is an error in speech recognition, fire an error event.
                    InvokeRecognitionErrorCallback(status, json.AsJson());
                }
                else
                {
                    auto speechPhraseMsg = RetrieveSpeechPhraseResult(speechPhraseJson);
                    TranslationResult translationResult;
                    if (status == RecognitionStatus::Success)
                    {
                        translationResult = RetrieveTranslationResult(json.Reader(), true);
                    }
                    else
                    {
                        translationResult.translationStatus = TranslationStatus::Success;
                    }
                    // There is no speech recognition error, we fire a translation phrase event.
                    Invoke([&](auto callbacks) {
                        callbacks->OnTranslationPhrase({ json.AsJson(),
                                                        speechPhraseMsg.offset,
                                                        speechPhraseMsg.duration,
                                                        std::move(speechPhraseMsg.displayText),
                                                        std::move(translationResult),
                                                        status,
                                                        std::move(speechPhraseMsg.language),
                                                        speechPhraseMsg.languageDetectionConfidence });
                        });


                    if (!m_turnUsingHeaders)
                    {
                        // Tell our site we're done with the audio.
                        Invoke([&](auto callbacks) { callbacks->OnAcknowledgedAudio(speechPhraseMsg.offset + speechPhraseMsg.duration); });
                    }
                }
            }
            else
            {
                PROTOCOL_VIOLATION("Invalid translation response, no %s or %s data. Json=%s",
                    json_properties::speechPhrase,
                    json_properties::speechHypothesis,
                    json.AsJson().c_str());
            }
        }
        else if (path == path::audioMetaData)
        {
            AudioOutputMetadataMsg msg;
            msg.requestId = requestId;
            msg.size = bufferSize;

            auto metadatas = json[json_properties::metadata];
            if (metadatas.IsArray())
            {
                auto metadata = metadatas.FirstValue();
                while (metadata.IsOk())
                {
                    auto metadataType = metadata[json_properties::type].AsString();
                    AudioOutputMetadata audioOutputMetadata;
                    if (metadataType == json_properties::wordBoundary || metadataType == json_properties::sentenceBoundary)
                    {
                        audioOutputMetadata.type = json_properties::textBoundary;

                        auto metadataData = metadata[json_properties::data];
                        if (metadataData.IsContainer())
                        {
                            auto offset = metadataData[json_properties::offset];
                            audioOutputMetadata.audioOffset = offset.AsUint<OffsetType>();

                            auto duration = metadataData[json_properties::duration];
                            audioOutputMetadata.textBoundary.duration = duration.AsUint<OffsetType>();

                            auto text = metadataData[json_properties::lowerText];
                            if (text.IsContainer())
                            {
                                audioOutputMetadata.textBoundary.text = text[json_properties::text].AsString();
                                audioOutputMetadata.textBoundary.boundaryType = text[json_properties::boundaryType].AsString();
                            }
                        }
                    }
                    else if (metadataType == json_properties::viseme)
                    {
                        audioOutputMetadata.type = json_properties::viseme;

                        auto metadataData = metadata[json_properties::data];
                        if (metadataData.IsContainer())
                        {
                            auto offset = metadataData[json_properties::offset];
                            audioOutputMetadata.viseme.audioOffset = offset.AsUint<OffsetType>();
                            audioOutputMetadata.viseme.visemeId = metadataData[json_properties::visemeId].AsUint<uint32_t>();
                            audioOutputMetadata.viseme.animationChunk = metadataData[json_properties::animationChunk].AsString();
                            audioOutputMetadata.viseme.isLastAnimation = metadataData[json_properties::isLastAnimation].AsBool(true);
                        }
                    }
                    else if (metadataType == json_properties::bookmark)
                    {
                        audioOutputMetadata.type = json_properties::bookmark;

                        auto metadataData = metadata[json_properties::data];
                        if (metadataData.IsObject())
                        {
                            auto offset = metadataData[json_properties::offset];
                            audioOutputMetadata.bookmark.audioOffset = offset.AsUint<OffsetType>();
                            audioOutputMetadata.bookmark.text = metadataData[json_properties::bookmark].AsString();
                        }
                    }
                    else if (metadataType == json_properties::sessionEnd)
                    {
                        audioOutputMetadata.type = json_properties::sessionEnd;
                        auto metadataData = metadata[json_properties::data];
                        if (metadataData.IsObject())
                        {
                            auto offset = metadataData[json_properties::offset];
                            audioOutputMetadata.audioOffset = offset.AsUint<OffsetType>();
                        }
                    }
                    else
                    {
                        SPX_DBG_TRACE_WARNING("Not supported metadata type %s", metadataType.c_str());
                    }

                    msg.metadatas.emplace_back(audioOutputMetadata);
                    ++metadata;
                }
            }

            Invoke([&](auto callbacks) { callbacks->OnAudioOutputMetadata(msg); });
        }
        else if (path == path::audioStart)
        {
            auto streamId = TryGet(headers, headers::streamId);
            if (streamId.empty())
            {
                PROTOCOL_VIOLATION("No stream id in %s header", path.c_str());
            }
            else if (json[json_properties::translationLanguage].IsEnd())
            {
                // TODO: in future, audio.start may be used in other scenario, this validation logic need to update accordingly.
                PROTOCOL_VIOLATION("Cannot find TranslationLanguage in audio.start message. Json=%s", json.AsJson().c_str());
            }
            else
            {
                auto language = json[json_properties::translationLanguage].AsString();
                SPX_TRACE_INFO("Got streamId %s to language %s map.  current m_streamIdLangMap size = %zu", streamId.c_str(), language.c_str(), m_streamIdLangMap.size());
                // This is a protection logic to avoid memory usage increases due to service error.
                // So far we haven't any scenario that needs more than 10 synthesising languages
                // TODO: remove this protection code after the new service endpoint is stable
                const int MaxLanguages = 10;
                if (m_streamIdLangMap.size() > MaxLanguages)
                {
                    PROTOCOL_VIOLATION("We have got more than %d audio.start messages for different languages, service is sending too many such messages.", MaxLanguages);
                }
                else
                {
                    m_streamIdLangMap[streamId] = language;
                }
            }
        }
        else if (path == path::audioEnd)
        {
            std::string failureReason;
            auto status = json[json_properties::status].AsString();
            if (status != "Success" && status != "Error")
            {
                failureReason = "Invalid status in audio.end message.";
                PROTOCOL_VIOLATION("%s Json=%s", failureReason.c_str(), json.AsJson().c_str());
            }
            bool isSuccess = status == "Success";
            if (isSuccess)
            {
                AudioOutputChunkMsg msg;
                msg.audioBuffer = NULL;
                msg.audioLength = 0;
                msg.requestId = requestId;
                auto streamId = TryGet(headers, headers::streamId);
                FillLanguageForAudioOutputChunkMsg(streamId, path, msg);
                Invoke([&](auto callbacks) { callbacks->OnAudioOutputChunk(msg); });
            }
            else
            {
                failureReason = failureReason + json[json_properties::failureReason].AsString();
                auto error = ErrorInfo::FromExplicitError(CancellationErrorCode::ServiceError, failureReason);
                Invoke([&](auto callbacks) { callbacks->OnError(error); });
            }
        }
        else
        {
            Invoke([&](auto callbacks) {
                callbacks->OnUserMessage({ path,
                                          contentType,
                                          requestId,
                                          buffer,
                                          bufferSize });
                });
        }
    }
}

void CSpxUspConnection::OnTransportEstimatedUploadRate(const float uploadRateKBPerSecond)
{
    auto site = GetSite();
    if (site)
    {
        auto props = site->QueryInterface<ISpxNamedProperties>();
        if (props)
        {
            props->Set("SPEECH-EstimatedWebSocketUploadRate-KBps", uploadRateKBPerSecond);
        }
    }
}

void CSpxUspConnection::InvokeRecognitionErrorCallback(RecognitionStatus status, const std::string& response)
{
    auto callbacks = m_config->m_callbacks;
    auto error = ErrorInfo::FromRecognitionStatus(status, response);

    this->Invoke([&](auto callbacks) { callbacks->OnError(error); });
}

SpeechPhraseMsg CSpxUspConnection::RetrieveSpeechPhraseResult(const ajv::JsonReader& json)
{
    SpeechPhraseMsg result;
    result.json = json.AsJson();
    result.offset = json[json_properties::offset].AsUint<OffsetType>();
    result.duration = json[json_properties::duration].AsUint<uint64_t>();
    result.recognitionStatus = ToRecognitionStatus(json[json_properties::recoStatus].AsString());
    result.speaker = json[json_properties::speaker].AsString();
    result.utteranceId = json[json_properties::utteranceId].AsString();
    result.phraseId = json[json_properties::phraseId].AsString();
    if (result.recognitionStatus == RecognitionStatus::Success)
    {
        // The DisplayText field will be present only if the RecognitionStatus field has the value Success.
        // and the format output is simple
        auto displayText = json[json_properties::displayText].AsString();

        // For Detailed format output...
        // The service returns sorted n-best results and the first result is the best one.
        // Use the first one as the default text, irrespective of the confidence value
        if (displayText.empty())
        {
            displayText = json[json_properties::nbest][0][json_properties::display].AsString();
        }

        result.displayText = displayText;
    }

    auto primaryLanguageJson = json[json_properties::primaryLanguage];
    if (primaryLanguageJson.IsObject())
    {
        result.language = primaryLanguageJson[json_properties::lang].AsString();
        auto confidence = primaryLanguageJson[json_properties::confidence].AsString();
        if (result.language.empty() || confidence.empty())
        {
            PROTOCOL_VIOLATION("Invalid language detection response. language = %s and confidence = %s should both have values. Json = %s",
                result.language.c_str(),
                confidence.c_str(),
                primaryLanguageJson.AsJson().c_str());
        }
        else
        {
            SPX_DBG_TRACE_VERBOSE("Got language %s and confidence %s from speech phrase message.", result.language.c_str(), confidence.c_str());
            result.languageDetectionConfidence = ToConfidenceLevel(confidence);
        }
    }
    return result;
}

bool CSpxUspConnection::isErrorRecognitionStatus(RecognitionStatus status)
{
    switch (status)
    {
    case RecognitionStatus::Success:
    case RecognitionStatus::InitialSilenceTimeout:
    case RecognitionStatus::InitialBabbleTimeout:
    case RecognitionStatus::NoMatch:
    case RecognitionStatus::EndOfDictation:
        return false;
    default:
        return true;
    }
}

void CSpxUspConnection::FillLanguageForAudioOutputChunkMsg(const std::string& streamId, const std::string& messagePath, AudioOutputChunkMsg& msg)
{
    if (streamId.empty())
    {
        PROTOCOL_VIOLATION("%s message is received but doesn't have streamId in header.", messagePath.c_str());
    }
    else if (m_streamIdLangMap.count(streamId) == 0)
    {
        PROTOCOL_VIOLATION(
            "%s message is received but cannot find streamId %s from streamId to language map, may be caused by audio.start message not being received before this message.",
            messagePath.c_str(),
            streamId.c_str());
    }
    else
    {
        msg.language = m_streamIdLangMap.at(streamId);
        if (messagePath == path::audioEnd)
        {
            SPX_DBG_TRACE_VERBOSE("Got audio end, remove %s from m_streamIdLangMap.", streamId.c_str());
            m_streamIdLangMap.erase(streamId);
        }
    }
}

}}}}
