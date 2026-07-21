//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// usp_reco_engine_adapter.cpp: Implementation definitions for CSpxUspRecoEngineAdapter C++ class
//

#include <vector>
#include <array>
#include "stdafx.h"
#include "usp_reco_engine_adapter.h"
#include "file_utils.h"
#include <inttypes.h>
#include <cstring>
#include <sstream>
#include <chrono>
#include <usp_text_message.h>
#include <usp_binary_message.h>
#include "service_helpers.h"
#include "create_object_helpers.h"
#include "exception.h"
#include "property_id_2_name_map.h"
#include "spx_build_information.h"
#include "platform.h"
#include "guid_utils.h"
#include "error_info.h"
#include "pronunciation_assessment_config.h"
#include "http_utils.h"
#include "reco_engine_adapter_helpers.h"

#include <ajv.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

decltype(CSpxUspRecoEngineAdapter::s_defaultRecognitionLanguage) constexpr CSpxUspRecoEngineAdapter::s_defaultRecognitionLanguage;

CSpxUspRecoEngineAdapter::CSpxUspRecoEngineAdapter() :
    m_startingOffset(0),
    m_resetUspAfterAudioByteCount(0),
    m_uspAudioByteCount(0),
    m_audioState(AudioState::Idle),
    m_uspState(UspState::Idle)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);
}

CSpxUspRecoEngineAdapter::~CSpxUspRecoEngineAdapter()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);
    SPX_DBG_ASSERT(m_uspCallbacks == nullptr);
    SPX_DBG_ASSERT(m_uspConnection == nullptr);
}

void CSpxUspRecoEngineAdapter::Init()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);

    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, GetSite() == nullptr);
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_uspConnection != nullptr);
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_uspCallbacks != nullptr);
    m_message_name_to_type_map =
    {
        { "speech.event", USP::MessageType::SpeechEvent },
        { "event", USP::MessageType::Event },
        { "speech.context", USP::MessageType::Context },
        { "speech.config", USP::MessageType::Config },
        { "speech.agent", USP::MessageType::Agent },
        { "speech.agentcontext", USP::MessageType::AgentContext },
        { "ssml", USP::MessageType::Ssml },
        { "audio", USP::MessageType::Audio }
    };

    SPX_DBG_ASSERT(IsState(AudioState::Idle) && IsState(UspState::Idle));

    m_resetUspAfterAudioSeconds = GetOr<size_t>("SPEECH-ForceUSPReconnectionByAudioSentTimeSeconds", 0);
    m_allowUspResetAfterAudioByteCount = (0 != m_resetUspAfterAudioSeconds);

    m_resetUspAfterTimeSeconds = GetOr<size_t>("SPEECH-ForceUSPReconnectionByTimeConnectedSeconds", 0);
    m_allowUspResetAfterTime = (0 != m_resetUspAfterTimeSeconds);

    // Setup file logging, if requested
    auto dumpAudioToDir = GetStringValue("CARBON-INTERNAL-DumpAudioToDir");
    m_saveToWavEverything.SetFolder(dumpAudioToDir);
    m_saveToWavCurrentTurn.SetFolder(dumpAudioToDir);

    m_useMultiChannelProcessing = GetOr<bool>(PropertyId::Speech_EnableMultiChannelProcessing, false);
}

void CSpxUspRecoEngineAdapter::Term()
{
    SPX_DBG_TRACE_SCOPE("Terminating CSpxUspRecoEngineAdapter...", "Terminating CSpxUspRecoEngineAdapter... Done!");
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);

    if (TryChangeState(UspState::Terminating))
    {
        SPX_DBG_TRACE_VERBOSE("%s: Terminating USP Connection (0x%8p)", __FUNCTION__, (void*)m_uspConnection.get());

        UspTerminate();
        TryChangeState(UspState::Zombie);
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", 
            __FUNCTION__, 
            (void*)this, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::SetAdapterMode(bool singleShot)
{
    SPX_DBG_TRACE_VERBOSE("%s: singleShot=%d", __FUNCTION__, singleShot);
    m_singleShot = singleShot;
    if (IsBadState())
    {
        SPX_THROW_HR(SPXERR_START_RECOGNIZING_INVALID_STATE_TRANSITION);
    }
}

void CSpxUspRecoEngineAdapter::ResolveRecoMode(bool singleShot)
{
    SPX_DBG_TRACE_VERBOSE("%s", __FUNCTION__);
    // Get recognizer type
    uint16_t countSpeech, countTranslation, countDialog, countConversationTranscriber, countConversationTranscriberV2, countMeetingTranscriber, countLanguageId;
    GetSite()->GetScenarioCount(&countSpeech, &countTranslation, &countDialog, &countConversationTranscriber, &countConversationTranscriberV2, &countMeetingTranscriber, &countLanguageId);
    SPX_DBG_ASSERT(countSpeech + countTranslation + countDialog + countConversationTranscriber + countConversationTranscriberV2 + countMeetingTranscriber + countLanguageId == 1); // currently only support one recognizer

    const char* newRecoMode =
        countSpeech == 1
        ? singleShot ? g_recoModeInteractive : g_recoModeConversation
        : countTranslation == 1
        ? singleShot ? g_recoModeInteractive : g_recoModeConversation
        : countDialog == 1 ? g_recoModeInteractive
        : "";

    // Set reco mode.
    if (auto maybeCurrentRecoMode = Get(PropertyId::SpeechServiceConnection_RecoMode))
    {
        // Since the mode is set during connection setup, no mode switch is allowed.
        SPX_THROW_HR_IF(SPXERR_SWITCH_MODE_NOT_ALLOWED,
            maybeCurrentRecoMode.Get() != g_recoModeDictation && maybeCurrentRecoMode.Get() != newRecoMode);
    }
    else
    {
        Set(PropertyId::SpeechServiceConnection_RecoMode, newRecoMode);
        SPX_TRACE_INFO("Reco mode resolved to %s", newRecoMode);
    }
}

void CSpxUspRecoEngineAdapter::OpenConnection(bool singleShot)
{
    SPX_DBG_TRACE_VERBOSE("%s", __FUNCTION__);
    ResolveRecoMode(singleShot);
    // Establish the connection to service.
    EnsureUspInit();
}

void CSpxUspRecoEngineAdapter::CloseConnection()
{
    SPX_DBG_TRACE_VERBOSE("%s: Close connection.", __FUNCTION__);

    // Get recognizer type
    uint16_t countSpeech = 0, countTranslation = 0, countDialog = 0, countConversationTranscriber = 0, countConversationTranscriberV2 = 0, countMeetingTranscriber = 0, countLanguageId = 0;

    auto site = GetSite();
    if (site != nullptr)
    {
        site->GetScenarioCount(&countSpeech, &countTranslation, &countDialog, &countConversationTranscriber, &countConversationTranscriberV2, &countMeetingTranscriber, &countLanguageId);
        SPX_DBG_ASSERT(countSpeech + countTranslation + countDialog + countConversationTranscriber + countConversationTranscriberV2 + countMeetingTranscriber + countLanguageId == 1); // currently only support one recognizer
    }
    SPX_DBG_TRACE_WARNING_IF(site == nullptr, "%s: site == nullptr", __FUNCTION__);

    // Terminate the connection to service.
    UspTerminate();
}

void CSpxUspRecoEngineAdapter::SendSpeechEventMessage(std::string&& message)
{
    // Establish the connection to service.
    EnsureUspInit();
    UspSendMessage("speech.event", message, USP::MessageType::SpeechEvent);
}

USP::MessageType CSpxUspRecoEngineAdapter::GetMessageType(const std::string& path)
{
    auto found = m_message_name_to_type_map.find(path);
    return found == m_message_name_to_type_map.end()
        ? USP::MessageType::Unknown
        : found->second;
}

void CSpxUspRecoEngineAdapter::SendNetworkMessage(const char *path, std::string&& payload, const std::shared_ptr<std::promise<bool>>& pr)
{
    // Establish the connection to service.
    EnsureUspInit();

    // for some reason, no connection is established
    if (m_uspConnection == nullptr || IsState(UspState::Error))
    {
        std::string error_message{ "Invalid state in USP adapter when sending " };
        error_message += path;
        return;
    }

    auto message = std::make_unique<USP::TextMessage>(std::move(payload), path, GetMessageType(path));
    message->SetMessageSentPromise(pr);
    return UspSendMessage(std::move(message));
}

void CSpxUspRecoEngineAdapter::SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload, const std::shared_ptr<std::promise<bool>>& pr)
{
    // Establish the connection to service.
    EnsureUspInit();

    // for some reason, no connection is established
    if (m_uspConnection == nullptr || IsState(UspState::Error))
    {
        ThrowRuntimeError("No usp connection.");
    }
    auto message = std::make_unique<USP::BinaryMessage>(std::move(payload), path, GetMessageType(path));
    message->SetMessageSentPromise(pr);
    return UspSendMessage(std::move(message));
}

void CSpxUspRecoEngineAdapter::SetFormat(const SPXWAVEFORMATEX* pformat)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);

    SPX_DBG_TRACE_VERBOSE_IF(pformat == nullptr, "%s - pformat == nullptr", __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE_IF(pformat != nullptr, "%s\n  wFormatTag:      %s\n  nChannels:       %d\n  nSamplesPerSec:  %d\n  nAvgBytesPerSec: %d\n  nBlockAlign:     %d\n  wBitsPerSample:  %d\n  cbSize:          %d",
        __FUNCTION__,
        pformat->wFormatTag == WAVE_FORMAT_PCM ? "PCM" : std::to_string(pformat->wFormatTag).c_str(),
        pformat->nChannels,
        pformat->nSamplesPerSec,
        pformat->nAvgBytesPerSec,
        pformat->nBlockAlign,
        pformat->wBitsPerSample,
        pformat->cbSize);

    if (IsState(UspState::Zombie))
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d) USP-ZOMBIE", 
            __FUNCTION__, 
            (void*)this,
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState));
    }
    else if (IsBadState() && !IsState(UspState::Terminating))
    {
        // In case of error there still can be some calls to SetFormat in flight.
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d)", 
            __FUNCTION__, 
            (void*)this, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState));
        if (pformat == nullptr)
        {
            /* We still need to do this in order for the session to not go into a non recoverable state */
            InvokeOnSite([this](const SitePtr& p) { p->AdapterCompletedSetFormatStop(this); });
        }
    }
    else if (pformat != nullptr && IsState(UspState::Idle) && TryChangeState(AudioState::Idle, AudioState::Ready))
    {
        // we could call site when errors happen.
        // The m_audioState is Ready at this time. So, if we have two SetFormat calls in a row, the next one won't come in here
        // it goes to the else.
        //
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p)->PrepareFirstAudioReadyState()", __FUNCTION__, (void*)this);
        m_saveToWavEverything.OpenWav("usp-everything-audio-", pformat);
        PrepareFirstAudioReadyState(pformat);
    }
    else if (pformat == nullptr && (TryChangeState(AudioState::Idle) || IsState(UspState::Terminating)))
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) site->AdapterCompletedSetFormatStop()", __FUNCTION__, (void*)this);
        m_saveToWavEverything.CloseWav();
        m_saveToWavCurrentTurn.CloseWav();
        InvokeOnSite([this](const SitePtr& p) { p->AdapterCompletedSetFormatStop(this); });

        m_format.reset();
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", 
            __FUNCTION__, 
            (void*)this, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::ProcessAudio(const DataChunkPtr& audioChunk)
{
    auto size = audioChunk->size;
    if (IsState(UspState::Zombie) && size == 0)
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... size=0 ... (audioState/uspState=%d/%d) USP-ZOMBIE", 
            __FUNCTION__, 
            (void*)this, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState));
    }
    else if (IsBadState())
    {
        // In case of error there still can be some calls to ProcessAudio in flight.
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d)", 
            __FUNCTION__, 
            (void*)this, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState));
    }
    else if (size > 0 && TryChangeState(AudioState::Ready, UspState::Idle, AudioState::Sending, UspState::WaitingForTurnStart))
    {
        SPX_DBG_TRACE_VERBOSE_IF(1, "%s: (0x%8p)->PrepareUspAudioStream() ... size=%d", __FUNCTION__, (void*)this, size);
        PrepareUspAudioStream();
        ProcessAudioChunk(audioChunk);

        SPX_DBG_TRACE_VERBOSE("%s: site->AdapterStartingTurn()", __FUNCTION__);
        InvokeOnSite([this](const SitePtr& p) { p->AdapterStartingTurn(this); });
    }
    else if (audioChunk->size > 0 && IsState(AudioState::Sending))
    {
        SPX_DBG_TRACE_VERBOSE_IF(1, "%s: (0x%8p) Sending Audio ... size=%d", __FUNCTION__, (void*)this, size);
        ProcessAudioChunk(audioChunk);
    }
    else if (size == 0 && IsState(AudioState::Sending))
    {
        SPX_DBG_TRACE_VERBOSE_IF(1, "%s: (0x%8p) Flushing Audio ... size=0 USP-FLUSH", __FUNCTION__, (void*)this);
        FlushAudio(true);
    }
    else if (!IsState(AudioState::Sending))
    {
        SPX_DBG_TRACE_VERBOSE_IF(1, "%s: (0x%8p) Ignoring audio size=%d ... (audioState/uspState=%d/%d)", 
            __FUNCTION__, 
            (void*)this, 
            size, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState));
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", 
            __FUNCTION__, 
            (void*)this, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::SendAgentMessage(const std::string& buffer)
{
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);
    EnsureUspInit();
    UspSendMessage("agent", buffer, USP::MessageType::Agent);
}

void CSpxUspRecoEngineAdapter::EnsureUspInit()
{
    if (m_uspConnection == nullptr)
    {
        UserAgentInit();
        AddDefaultTrafficType();
        UspInitialize();
    }
}

void CSpxUspRecoEngineAdapter::UspInitialize()
{
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_uspConnection != nullptr);
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_uspCallbacks != nullptr);

    // Create the usp clientConfiguration, which we'll configure and use to create the actual connection
    auto uspCallbacks = SpxCreateObjectWithSite<ISpxUspCallbacks>("CSpxUspCallbackWrapper", this);

    // Currently we use a session id as a connection id to correlate logs on the server side with a particular user
    // session, because currently there is a single connection for session at any point in time.
    auto sessionId = GetOr(PropertyId::Speech_SessionId, "");
    USP::ClientConfiguration clientConfiguration(uspCallbacks, USP::EndpointType::Speech, sessionId);

    bool isCustomV1Endpoint = GetOr<bool>(g_isCustomV1Endpoint, false) || GetOr<bool>("SPEECH-ForceV1Endpoint", false);
    clientConfiguration.SetIsCustomV1Endpoint(isCustomV1Endpoint);
    bool isUnifiedEndpoint = GetOr<bool>(g_isUnifiedSpeechEndpoint, false) || GetOr<bool>("SPEECH-ForceUnifiedEndpoint", false);;
    clientConfiguration.SetIsUnifiedEndpoint(isUnifiedEndpoint);

    auto hostUrl = GetOr(PropertyId::SpeechServiceConnection_Host, "");
    if (!hostUrl.empty())
    {
        clientConfiguration.SetIsCustomHost(true);
    }

    // Set up the connection properties, and create the connection
    SetUspEndpoint(clientConfiguration);
    SetUspAuthentication(clientConfiguration);
    SetUserDefinedHttpHeaders(clientConfiguration);
    SetUspProxyInfo(clientConfiguration);
#if SPEECHSDK_USE_OPENSSL
    SetUspCrlProperties(clientConfiguration);
#endif

    // Construct config message payload
    SetSpeechConfigMessage();

    ISpxUspConnection::Ptr uspConnection;

    try
    {
        uspConnection = SpxCreateObjectWithSite<ISpxUspConnection>("CSpxUspConnection", SpxSiteFromThis(this));
        uspConnection->SetConfiguration(clientConfiguration);
        uspConnection->Connect();
    }
    catch (const std::exception& e)
    {
        uspConnection = nullptr;
        SPX_TRACE_ERROR("Error: '%s'", e.what());
        auto error = ErrorInfo::FromExplicitError(CancellationErrorCode::ConnectionFailure, e.what());
        OnError(error);
    }
    catch (...)
    {
        uspConnection = nullptr;
        SPX_TRACE_ERROR("Error: Unexpected exception in UspInitialize");
        auto error = ErrorInfo::FromExplicitError(
            CancellationErrorCode::ConnectionFailure,
            "Error: Unexpected exception in UspInitialize");
        OnError(error);
    }

    // if error occurs in the above clientConfiguration.Connect, return.
    if (uspConnection == nullptr)
    {
        return;
    }
    // Keep track of what time we initialized (so we can reset later)
    m_uspInitTime = std::chrono::system_clock::now();
    m_uspResetTime = m_uspInitTime + std::chrono::seconds(m_resetUspAfterTimeSeconds);

    // We're done!!
    m_uspCallbacks = uspCallbacks;
    m_uspConnection = std::move(uspConnection);

    if (m_uspConnection != nullptr)
    {
        Set(PropertyId::SpeechServiceConnection_Url, m_uspConnection->GetConnectionUrl().c_str());

        UspSendSpeechConfig();
        UspSendAgentConfig();
    }
}

void CSpxUspRecoEngineAdapter::UspTerminate()
{
    // Inform upper layer about disconnection.
    if ((m_uspConnection != nullptr) && m_uspConnection->IsConnected())
    {
        auto disconnectionReason = ErrorInfo::FromWebSocket(WebSocketError::UNKNOWN, WebSocketDisconnectReason::Normal);
        OnDisconnected(disconnectionReason);
    }

    // Term the callbacks first and then reset/release the connection
    SpxTermAndClear(m_uspCallbacks); // After this ... we won't be called back on ISpxUspCallbacks ever again...

    // NOTE: Even if we are in a callback from the USP on another thread, we won't be destroyed until those functions release their shared ptrs
    m_uspConnection.reset();

    // Fix up some of our counters...
    m_uspAudioByteCount = 0;
}

void CSpxUspRecoEngineAdapter::UserAgentInit()
{
    auto existingUserAgent = GetOr("HttpHeader#User-agent", "");
    if (!existingUserAgent.empty())
    {
        return; // User-Agent already set, don't override
    }

    auto lang = GetOr("AZAC-SDK-PROGRAMMING-LANGUAGE", BuildInformation::g_programmingLanguage);
    auto platform = PAL::getOperatingSystem().to_string();

    auto appOverride = GetOr("AZAC-SDK-APPNAME", "");
    const char* app = nullptr;
    if (!appOverride.empty())
    {
        app = appOverride.c_str();
    }
    else
    {
        auto queryParams = GetOr(PropertyId::SpeechServiceConnection_UserDefinedQueryParameters, "");
        app = queryParams.find("traffictype=spx") != std::string::npos ? "spx" : nullptr;
    }

    auto agent = HttpUtils::FormatAzSdkUserAgent(app, BuildInformation::g_SpeechSDKName, lang.c_str(), BuildInformation::g_fullSpeechVersion, platform.c_str());
    SetStringValue("HttpHeader#User-agent", agent.c_str());
}

// The CI build script to set variables defines the variable CARBONSDK_BUILD_TYPE to be "local", "dev", "int" or "prod".
// "prod" means the binary is built by CI pipeline in a release branch. See more details there.
// The root CMakeLists.txt defines the build flag CARBONSDK_BUILD_TYPE_PROD for "prod" builds only.
// We would like Carbon core binary in non-prod builds ("local", "dev", "int") to enforce a trafficType query URL
// so that speech-service usage reports based on service-side telemetry can exclude internal development & test traffic.
// The Carbon core binary built in the release branch ("prod" build) does not need to have trafficType.
void CSpxUspRecoEngineAdapter::AddDefaultTrafficType()
{
#ifdef CARBONSDK_BUILD_TYPE_PROD
    #pragma message (__FILE__ ": Not enforcing trafficType query")
#else
    #pragma message (__FILE__ ": Enforcing trafficType query")
    bool trafficTypeExists = false;
    string queryParams = GetOr(PropertyId::SpeechServiceConnection_UserDefinedQueryParameters, "");
    if (!queryParams.empty())
    {
        string lowerCaseQueryParams = PAL::StringUtils::ToLower(queryParams);
        trafficTypeExists = (string::npos != lowerCaseQueryParams.find("traffictype="));
    }

    if (!trafficTypeExists)
    {
        queryParams += (queryParams.empty() ? "" : "&") + std::string("trafficType=prerelease");
        Set(PropertyId::SpeechServiceConnection_UserDefinedQueryParameters, queryParams.c_str());
    }
#endif
}

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspEndpoint(USP::ClientConfiguration& client)
{
    SPX_DBG_ASSERT(GetSite() != nullptr);

    // How many recognizers of each type do we have?
    uint16_t countSpeech = 0, countTranslation = 0, countDialog = 0, countConversationTranscriber = 0, countConversationTranscriberV2 = 0, countMeetingTranscriber = 0, countLanguageId = 0;
    GetSite()->GetScenarioCount(&countSpeech, &countTranslation, &countDialog, &countConversationTranscriber, &countConversationTranscriberV2, &countMeetingTranscriber, &countLanguageId);
    SPX_DBG_ASSERT(countSpeech + countTranslation + countDialog + countConversationTranscriber + countConversationTranscriberV2 + countMeetingTranscriber + countLanguageId == 1); // currently only support one recognizer

    // set endpoint url if this is provided.
    auto endpoint = GetOr(PropertyId::SpeechServiceConnection_Endpoint, "");

    if (!endpoint.empty())
    {
        SPX_DBG_TRACE_VERBOSE("%s: Using Custom endpoint: %s", __FUNCTION__, endpoint.c_str());
        m_customEndpoint = true;
        client.SetEndpointUrl(endpoint);
    }
    else
    {
        // Set host url if this is provided
        if (auto maybeHost = Get(PropertyId::SpeechServiceConnection_Host))
        {
            SPX_DBG_TRACE_VERBOSE("%s: Using custom host: %s", __FUNCTION__, maybeHost.Get().c_str());
            m_customHost = true;
            client.SetHostUrl(maybeHost.Get());
        }
    }

    // set user defined query parameters if provided.
    if (auto maybeQueryParams = Get(PropertyId::SpeechServiceConnection_UserDefinedQueryParameters))
    {
        SPX_DBG_TRACE_VERBOSE("%s: Using user provided query parameters: %s",
            __FUNCTION__,
            maybeQueryParams.Get().c_str());
        client.SetUserDefinedQueryParameters(maybeQueryParams.Get());
    }

    // set endpoint type.
    if (countTranslation == 1)
    {
        SetUspEndpointTranslation(client);

        // Check to see whether Language id feature property is set or not
        SetUspLanguageIdModeAndPriority(client, false);
    }
    else if (countDialog == 1)
    {
        SetUspEndpointDialog(client);
    }
    else if (countConversationTranscriber == 1)
    {
        SetUspEndpointTranscriber(client, false);
    }
    else if (countConversationTranscriberV2 == 1)
    {
        SetUspEndpointTranscriberV2(client);
        SetUspLanguageIdModeAndPriority(client, false);
    }
    else if (countMeetingTranscriber == 1)
    {
        SetUspEndpointTranscriber(client, false);
    }
    else if (countLanguageId == 1)
    {
        // standalone lid
        SetUspEndpointLanguageId(client);
        SetUspLanguageIdModeAndPriority(client, true);
    }
    else
    {
        SPX_DBG_ASSERT(countSpeech == 1);
        SetUspEndpointDefaultSpeechService(client);
        SetUspLanguageIdModeAndPriority(client, false);
    }

    // No need to set the reco mode for standalone lid
    if (countLanguageId != 1)
    {
        SetUspRecoMode(client);
    }

    constexpr std::uint32_t kDefaultConnectPollingMs = 10;
    constexpr std::uint32_t kDefaultRunPollingMs = 10;
    constexpr std::uint32_t kMinPollingMs = 1;
    constexpr std::uint32_t kMaxPollingMs = 60000;
    constexpr std::uint32_t kNotSet = 0;  // Sentinel value (0 is invalid, min is 1)

    auto tryParsePollingMs = [&](const char* propertyName) -> std::uint32_t
    {
        auto valueStr = GetStringValue(propertyName, "");
        if (valueStr.empty())
        {
            return kNotSet;
        }

        try
        {
            unsigned long parsed = std::stoul(valueStr);
            if (parsed < kMinPollingMs)
            {
                SPX_TRACE_WARNING("%s: %s=%s is below minimum (%u ms). Using %u ms.",
                    __FUNCTION__, propertyName, valueStr.c_str(), kMinPollingMs, kMinPollingMs);
                return kMinPollingMs;
            }
            if (parsed > kMaxPollingMs)
            {
                SPX_TRACE_WARNING("%s: %s=%s is above maximum (%u ms). Using %u ms.",
                    __FUNCTION__, propertyName, valueStr.c_str(), kMaxPollingMs, kMaxPollingMs);
                return kMaxPollingMs;
            }
            return static_cast<std::uint32_t>(parsed);
        }
        catch (const std::logic_error&)
        {
            SPX_TRACE_WARNING("%s: Failed to parse %s=%s as an integer (ms). Ignoring.",
                __FUNCTION__, propertyName, valueStr.c_str());
            return kNotSet;
        }
    };

    // Connect interval precedence:
    //  1) SPEECH-USPConnectPollingInterval (string)
    //  2) SPEECH-USPPollingInterval (string) - retrieved via GetStringValue()
    //  3) SPEECH-USPPollingInterval (typed legacy) - retrieved via GetOr<uint16_t>()
    //  4) Default 10ms
    // Note: SPEECH-USPPollingInterval appears twice because it supports both string-based and typed property APIs for backward compatibility.
    
    std::uint32_t connectPollingMsFromStr = tryParsePollingMs("SPEECH-USPConnectPollingInterval");
    std::uint32_t legacyPollingMsFromStr = tryParsePollingMs("SPEECH-USPPollingInterval");
    std::uint32_t legacyPollingMsTyped = static_cast<std::uint32_t>(
        GetOr<std::uint16_t>("SPEECH-USPPollingInterval", static_cast<std::uint16_t>(kDefaultConnectPollingMs)));

    std::uint32_t connectPollingMs = (connectPollingMsFromStr != kNotSet)
        ? connectPollingMsFromStr
        : ((legacyPollingMsFromStr != kNotSet) ? legacyPollingMsFromStr : legacyPollingMsTyped);

    bool legacyOverrideInEffect = (connectPollingMsFromStr == kNotSet) &&
        ((legacyPollingMsFromStr != kNotSet) || (legacyPollingMsTyped != kDefaultConnectPollingMs));

    SPX_DBG_TRACE_VERBOSE("%s: Setting Websocket Connect Polling interval to %u ms", __FUNCTION__, connectPollingMs);
    client.SetPollingIntervalms(connectPollingMs);

    // Run interval precedence:
    //  1) SPEECH-USPRunPollingInterval (string)
    //  2) If legacy override SPEECH-USPPollingInterval resulted in a non-default connect interval, keep run interval equal to connect.
    //  3) Default 100ms
    std::uint32_t runPollingMsFromStr = tryParsePollingMs("SPEECH-USPRunPollingInterval");
    std::uint32_t runPollingMs = (runPollingMsFromStr != kNotSet)
        ? runPollingMsFromStr
        : (legacyOverrideInEffect ? connectPollingMs : kDefaultRunPollingMs);

    SPX_DBG_TRACE_VERBOSE("%s: Setting Websocket Run Polling interval to %u ms", __FUNCTION__, runPollingMs);
    client.SetRunPollingIntervalms(runPollingMs);

    return client;
}

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspRecoMode(USP::ClientConfiguration& client)
{
    // set reco mode based on recognizer type
    // Reco mode should be already set in properties.
    USP::RecognitionMode mode = GetRecoModeFromProperties();
    m_isInteractiveMode = (mode == USP::RecognitionMode::Interactive);
    client.SetRecognitionMode(mode);
    SPX_DBG_TRACE_VERBOSE("%s: recoMode=%d", __FUNCTION__, static_cast<int>(mode));

    return client;
}

/// <summary>
/// Sets the usp language identifier mode and priority.
/// </summary>
/// <param name="clientConfiguration">The clientConfiguration.</param>
/// <param name="isStandaloneLid">if set to <c>true</c> [is standalone lid].</param>
/// <returns>clientConfiguration address</returns>
USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspLanguageIdModeAndPriority(USP::ClientConfiguration& client, bool /* isStandaloneLid */)
{
    // Default LID properties
    USP::LanguageIdMode languageIdMode = USP::LanguageIdMode::DetectAtAudioStart;
    USP::LanguageIdPriority languageIdPriority = USP::LanguageIdPriority::PrioritizeLatency;

    // Possibly override the above defaults, based on application settings
    UpdateLanguageIdModeAndPriorityFromProperties(languageIdMode, languageIdPriority);

    m_languageIdMode = languageIdMode;
    client.SetLanguageIdMode(languageIdMode);

    m_languageIdPriority = languageIdPriority;
    client.SetLanguageIdPriority(languageIdPriority);

    SPX_DBG_TRACE_VERBOSE("%s: languageIdMode=%d, languageIdPriority=%d", __FUNCTION__, static_cast<int>(languageIdMode), static_cast<int>(languageIdPriority));

    return client;
}

/// <summary>
/// Sets the usp endpoint language identifier.
/// </summary>
/// <param name="clientConfiguration">The clientConfiguration.</param>
/// <returns>usp clientConfiguration</returns>
USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspEndpointLanguageId(USP::ClientConfiguration& client)
{
    SPX_DBG_TRACE_VERBOSE("%s: Endpoint type: StandaloneLanguageId.", __FUNCTION__);
    m_endpointType = USP::EndpointType::StandaloneLanguageId;
    client.SetEndpointType(m_endpointType);

    SetUspRegion(client);

    UpdateDefaultLanguage();

    UpdateOutputFormatOption();

    SetUspQueryParameters(USP::endpoint::standalonelid::queryParameters, client);

    return client;
}

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspEndpointTranscriber(USP::ClientConfiguration& client, bool isDynamic)
{
    SPX_DBG_TRACE_VERBOSE("%s: Endpoint type: ConversationTranscriptionService", __FUNCTION__);

    m_endpointType = isDynamic
        ? USP::EndpointType::DynamicConversationTranscriptionService
        : USP::EndpointType::ConversationTranscriptionService;
    client.SetEndpointType(m_endpointType);

    SetUspRegion(client);
    UpdateDefaultLanguage();
    UpdateOutputFormatOption();
    SetUspQueryParameters(USP::endpoint::conversationTranscriber::queryParameters, client);

    return client;
}

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspEndpointTranscriberV2(USP::ClientConfiguration& client)
{
    SPX_DBG_TRACE_VERBOSE("%s: Endpoint type: TranscriberV2.", __FUNCTION__);
    m_endpointType = USP::EndpointType::ConversationTranscriptionServiceV2;
    client.SetEndpointType(m_endpointType);

    SetUspRegion(client);

    UpdateDefaultLanguage();

    UpdateOutputFormatOption();

    SetUspQueryParameters(USP::endpoint::conversationTranscriberV2::queryParameters, client);

    return client;
}

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspEndpointDialog(USP::ClientConfiguration& client)
{
    SPX_DBG_TRACE_VERBOSE("%s: Endpoint type: Dialog.", __FUNCTION__);
    m_endpointType = USP::EndpointType::Dialog;
    client.SetEndpointType(m_endpointType);

    SetUspRegion(client);

    UpdateDefaultLanguage();

    /* Set conversation id if present */
    m_dialogConversationId = GetOr(PropertyId::Conversation_Conversation_Id, "");
    auto dialogType = GetOr(PropertyId::Conversation_DialogType, "");

    USP::ClientConfiguration::DialogBackend dialogBackend{ USP::ClientConfiguration::DialogBackend::NotSet };
    if (dialogType == g_dialogType_BotFramework)
    {
        dialogBackend = USP::ClientConfiguration::DialogBackend::BotFramework;
        SetUspQueryParameters(USP::endpoint::dialog::botFramework::queryParameters, client);
    }
    else if (dialogType == g_dialogType_CustomCommands)
    {
        dialogBackend = USP::ClientConfiguration::DialogBackend::CustomCommands;
        SetUspQueryParameters(USP::endpoint::dialog::customCommands::queryParameters, client);
    }
    else
    {
        /* We shouldn't be here */
        SPX_THROW_HR(SPXERR_INVALID_ARG);
    }

    client.SetDialogBackend(dialogBackend);

    return client.SetAudioResponseFormat("raw-16khz-16bit-mono-pcm");
}

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspEndpointTranslation(USP::ClientConfiguration& client)
{
    SPX_DBG_TRACE_VERBOSE("%s: Endpoint type: Translation.", __FUNCTION__);

    SetUspRegion(client);

    UpdateOutputFormatOption();

    // Is an endpoint specified?
    if (client.GetIsUnifiedEndpoint() || !HasStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_Endpoint)))
    {
        m_endpointType = USP::EndpointType::Translation;
        SetUspQueryParameters(USP::endpoint::unifiedspeech::queryParameters, client);
    }
    else // Maybe Legacy endpoint
    {
        // Legacy endpionts need to specify a path route.
        auto endpoint = GetOr(PropertyId::SpeechServiceConnection_Endpoint, "");
        auto url = HttpUtils::ParseUrl(endpoint);

        if (url.path.empty() && url.query.empty())
        {
            m_endpointType = USP::EndpointType::Translation;
            SetUspQueryParameters(USP::endpoint::unifiedspeech::queryParameters, client);
        }
        else
        {
            m_endpointType = USP::EndpointType::TranslationV1;
            SetUspQueryParameters(USP::endpoint::translationV1::queryParameters, client);
        }
    }
    
    client.SetEndpointType(m_endpointType);

    return client;
}

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspEndpointDefaultSpeechService
(USP::ClientConfiguration& client)
{
    SPX_DBG_TRACE_VERBOSE("%s: Endpoint type: Speech.", __FUNCTION__);
    m_endpointType = USP::EndpointType::Speech;
    client.SetEndpointType(m_endpointType);

    SetUspRegion(client);

    UpdateDefaultLanguage();

    UpdateOutputFormatOption();

    if (client.GetIsCustomV1Endpoint() || client.GetIsCustomHost())
    {
        SetUspQueryParameters(USP::endpoint::v1speech::queryParameters, client);
        m_handleAsSpeechV1Endpoint = true;
    }
    else
    {
        SetUspQueryParameters(USP::endpoint::unifiedspeech::queryParameters, client);
    }

    return client;
}

template<size_t N>
USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspQueryParameters(
    const std::array<const char*, N>& allowedParameterList,
    USP::ClientConfiguration& client)
{
    for (auto& queryParamName : allowedParameterList)
    {
        SetSingleUspQueryParameter(queryParamName, client);
    }
    return client;
}

void CSpxUspRecoEngineAdapter::SetSingleUspQueryParameter(const char *queryParamName, USP::ClientConfiguration& client) const
{
    enum class PropertyValueType {
        String,
        Unsigned,
        Bool
    };

    struct ParamEntry
    {
        const char* Name;
        PropertyId Id;
        PropertyValueType ValueType;
        // Conditional default assignment:
        //  - Assign a default value to this query parameter even if the property isn't provided
        //  - (Optionally) skip if a non-null existing query string parameter K/V pair isn't already present
        //  - This can make a query string parameter always appear -- use sparingly!
        //  - Note that this means *ordering matters* -- checking for a required K/V has to come after that K/V in the array
        const char* DefaultValue = nullptr;
        const char* DefaultRequiredExistingKey = nullptr;
        const char* DefaultRequiredExistingValue = nullptr;
    };

    constexpr std::array<ParamEntry, 20> QueryParameterEntries {{
        { USP::endpoint::langQueryParam, PropertyId::SpeechServiceConnection_RecoLanguage, PropertyValueType::String },
        { USP::endpoint::deploymentIdQueryParam, PropertyId::SpeechServiceConnection_EndpointId, PropertyValueType::String },
        { USP::endpoint::initialSilenceTimeoutQueryParam, PropertyId::SpeechServiceConnection_InitialSilenceTimeoutMs, PropertyValueType::Unsigned },
        { USP::endpoint::endSilenceTimeoutQueryParam, PropertyId::SpeechServiceConnection_EndSilenceTimeoutMs, PropertyValueType::Unsigned },
        { USP::endpoint::storeAudioQueryParam, PropertyId::SpeechServiceConnection_EnableAudioLogging, PropertyValueType::Bool },
        { USP::endpoint::outputFormatQueryParam, PropertyId::SpeechServiceResponse_OutputFormatOption, PropertyValueType::String },
        // Word-level timestamps and confidence scores are *both* mapped to the RequestWordLevelTimestamps property and
        // *both* will be added by default ("true") if output format is set to detailed
        { USP::endpoint::wordLevelTimestampsQueryParam, PropertyId::SpeechServiceResponse_RequestWordLevelTimestamps, PropertyValueType::Bool,
            "true", USP::endpoint::outputFormatQueryParam, "detailed" },
        { USP::endpoint::wordLevelConfidenceQueryParam, PropertyId::SpeechServiceResponse_RequestWordLevelTimestamps, PropertyValueType::Bool,
            "true", USP::endpoint::outputFormatQueryParam, "detailed" },
        { USP::endpoint::profanityQueryParam, PropertyId::SpeechServiceResponse_ProfanityOption, PropertyValueType::String },
        { USP::endpoint::stableIntermediateThresholdQueryParam, PropertyId::SpeechServiceResponse_StablePartialResultThreshold, PropertyValueType::Unsigned },
        { USP::endpoint::unifiedspeech::postprocessingQueryParam, PropertyId::SpeechServiceResponse_PostProcessingOption, PropertyValueType::String },
        { USP::endpoint::unifiedspeech::lidEnabledQueryParam, PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, PropertyValueType::String },
        { USP::endpoint::translationV1::fromQueryParam, PropertyId::SpeechServiceConnection_RecoLanguage, PropertyValueType::String },
        { USP::endpoint::translationV1::toQueryParam, PropertyId::SpeechServiceConnection_TranslationToLanguages, PropertyValueType::String },
        { USP::endpoint::translationV1::voiceQueryParam, PropertyId::SpeechServiceConnection_TranslationVoice, PropertyValueType::String },
        { USP::endpoint::translationV1::stableTranslationQueryParam, PropertyId::SpeechServiceResponse_TranslationRequestStablePartialResult, PropertyValueType::Bool },
        { USP::endpoint::dialog::customVoiceDeploymentIdsQueryParam, PropertyId::Conversation_Custom_Voice_Deployment_Ids, PropertyValueType::String },
        { USP::endpoint::dialog::botIdQueryParam, PropertyId::Conversation_ApplicationId, PropertyValueType::String },
        { USP::endpoint::dialog::commandsAppIdQueryParam, PropertyId::Conversation_ApplicationId, PropertyValueType::String },
        { USP::endpoint::dialog::botStatusMessageQueryParam, PropertyId::Conversation_Request_Bot_Status_Messages, PropertyValueType::String }
    }};

    auto matchIterator = std::find_if(QueryParameterEntries.begin(), QueryParameterEntries.end(),
        [&](const ParamEntry& entry) { return strcmp(entry.Name, queryParamName) == 0; });
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, matchIterator == std::end(QueryParameterEntries));
    auto entry = *matchIterator;

    auto qualifiesForDefault = [&]()
    {
        return (entry.DefaultValue
            && (!entry.DefaultRequiredExistingKey || client.HasQueryParameter(entry.DefaultRequiredExistingKey))
            && (!entry.DefaultRequiredExistingKey
                || client.GetQueryParameter(entry.DefaultRequiredExistingKey) == entry.DefaultRequiredExistingValue));
    };

    // Find a value to use from an associated property ID, using the first match if multiple exist.
    PropertyId matchingPropertyId = entry.Id;
    std::string matchedPropertyValue{};

    for (const auto& propertyId : GetPropertyIdAliasesForQueryStrings(entry.Id))
    {
        matchingPropertyId = propertyId;
        matchedPropertyValue = GetOr(propertyId, "");
        if (!matchedPropertyValue.empty())
        {
            break;
        }
    }

    // Assign either a discovered value or, if no discovered value exists and the parameter supports a default value,
    // the default value.
    if (!matchedPropertyValue.empty())
    {
        SPX_THROW_HR_IF(
            SPXERR_INVALID_ARG,
            (entry.ValueType == PropertyValueType::Bool && !Get<bool>(matchingPropertyId))
            || (entry.ValueType == PropertyValueType::Unsigned && !Get<uint64_t>(matchingPropertyId)));
        client.SetQueryParameter(entry.Name, matchedPropertyValue);
    }
    else if (qualifiesForDefault())
    {
        client.SetQueryParameter(entry.Name, entry.DefaultValue);
    }
}

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspRegion(USP::ClientConfiguration& client)
{
    auto region = GetOr(PropertyId::SpeechServiceConnection_Region, "");

    if (!m_customEndpoint && !m_customHost)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_REGION, region.empty());
        client.SetRegion(region);
    }
    else
    {
        if (!region.empty())
        {
            SPX_TRACE_ERROR("when using custom endpoint, region should not be specified separately.");
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }
    }
    return client;
}

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspAuthentication(USP::ClientConfiguration& client)
{
    std::array<std::string, static_cast<size_t>(USP::AuthenticationType::SIZE_AUTHENTICATION_TYPE)> authData;

    authData[static_cast<size_t>(USP::AuthenticationType::SubscriptionKey)]
        = GetOr(PropertyId::SpeechServiceConnection_Key, "");
    authData[static_cast<size_t>(USP::AuthenticationType::AuthorizationToken)]
        = GetOr(PropertyId::SpeechServiceAuthorization_Token, "");
    authData[static_cast<size_t>(USP::AuthenticationType::SearchDelegationRPSToken)]
        = GetOr("SPEECH-RpsToken", "");

    return client.SetAuthentication(authData);
}

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUserDefinedHttpHeaders(USP::ClientConfiguration& client)
{
    auto headers = FindPrefix("HttpHeader");
    return client.SetUserDefinedHttpHeaders(headers);
}

void CSpxUspRecoEngineAdapter::UpdateDefaultLanguage()
{
    if (!Get(PropertyId::SpeechServiceConnection_RecoLanguage)
        && !Get(PropertyId::SpeechServiceConnection_EndpointId)
        && GetOr(PropertyId::SpeechServiceConnection_Endpoint, "")
                .find(USP::endpoint::deploymentIdQueryParam) == string::npos)
    {
        Set(PropertyId::SpeechServiceConnection_RecoLanguage, s_defaultRecognitionLanguage);
    }
}

void CSpxUspRecoEngineAdapter::UpdateOutputFormatOption()
{
    bool requestWordLevelTimestamps = false;
    if (auto maybeRequestWordLevelTimestamps = Get<bool>(PropertyId::SpeechServiceResponse_RequestWordLevelTimestamps))
    {
        if (maybeRequestWordLevelTimestamps.Get())
        {
            requestWordLevelTimestamps = true;
        }
    }
    if (requestWordLevelTimestamps)
    {
        // Word level timestamp always use detailed format.
        Set(PropertyId::SpeechServiceResponse_OutputFormatOption, USP::endpoint::outputFormatDetailed);
    }
    else if (auto maybeUseDetailed = Get<bool>(PropertyId::SpeechServiceResponse_RequestDetailedResultTrueFalse))
    {
        // Convert the true/false value for use of detailed results into the string form, but only if the string form
        // isn't already set.
        SetAsDefault(
            PropertyId::SpeechServiceResponse_OutputFormatOption,
            maybeUseDetailed.Get() ? USP::endpoint::outputFormatDetailed : USP::endpoint::outputFormatSimple);
    }
}

#if SPEECHSDK_USE_OPENSSL
USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspCrlProperties(USP::ClientConfiguration& clientConfiguration)
{
    // N.B. the names of the options below have been shared with a customer. Do
    // not change them without consulting with them.
    auto maybeSingleTrustedCert = Get("OPENSSL_SINGLE_TRUSTED_CERT");
    if (maybeSingleTrustedCert)
    {
        clientConfiguration.SetSingleTrustedCert(maybeSingleTrustedCert.Get());
    }

    int max_crl_download_size_in_kb = GetOr<int>("CONFIG_MAX_CRL_SIZE_KB", MAX_CRL_SIZE_DEFAULT);
    clientConfiguration.SetMaxCrlDownloadSizeInKB(max_crl_download_size_in_kb);

    return clientConfiguration
        .SetDisableCrlChecks(
            GetOr<bool>("OPENSSL_DISABLE_CRL_CHECK", true)
            || (maybeSingleTrustedCert && !GetOr<bool>("OPENSSL_SINGLE_TRUSTED_CERT_CRL_CHECK", true)))
        .SetContinueOnCrlDownloadFailure(GetOr<bool>("OPENSSL_CONTINUE_ON_CRL_DOWNLOAD_FAILURE", true));
}
#endif

USP::ClientConfiguration& CSpxUspRecoEngineAdapter::SetUspProxyInfo(USP::ClientConfiguration& client)
{
    // read and parse the list of hosts to bypass the proxy for
    client.SetProxyHostBypass(PAL::StringUtils::Tokenize(GetOr(PropertyId::SpeechServiceConnection_ProxyHostBypass, ""), ","));

    // Get proxy related properties.
    auto maybeHostname = Get(PropertyId::SpeechServiceConnection_ProxyHostName);
    if (!maybeHostname)
    {
        return client;
    }
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, maybeHostname.Get().empty());

    auto maybeProxyPort = Get<int>(PropertyId::SpeechServiceConnection_ProxyPort);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, !maybeProxyPort || maybeProxyPort.Get() <= 0);

    auto maybeUsername = Get(PropertyId::SpeechServiceConnection_ProxyUserName);
    auto maybePassword = Get(PropertyId::SpeechServiceConnection_ProxyPassword);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, maybeUsername ^ maybePassword);

    return client.SetProxyServerInfo(
        maybeHostname.Get().c_str(),
        maybeProxyPort.Get(),
        maybeUsername ? maybeUsername.Get().c_str() : nullptr,
        maybePassword ? maybePassword.Get().c_str() : nullptr);
}

USP::RecognitionMode CSpxUspRecoEngineAdapter::GetRecoModeFromProperties() const
{
    auto TryParseRecoMode = [](const char* input, USP::RecognitionMode& mode) -> bool
    {
        constexpr auto errorValue = (USP::RecognitionMode)-1;
        auto newMode =
            PAL::stricmp(input, g_recoModeInteractive) == 0 ? USP::RecognitionMode::Interactive
            : PAL::stricmp(input, g_recoModeConversation) == 0 ? USP::RecognitionMode::Conversation
            : PAL::stricmp(input, g_recoModeDictation) == 0 ? USP::RecognitionMode::Dictation
            : errorValue;
        mode = newMode == errorValue ? mode : newMode;
        return newMode != errorValue;
    };

    auto propValue = GetOr(PropertyId::SpeechServiceConnection_RecoMode, "");

    USP::RecognitionMode resultMode{ USP::RecognitionMode::Interactive };
    SPX_THROW_HR_IF(SPXERR_NOT_FOUND, propValue.empty());
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, !TryParseRecoMode(propValue.c_str(), resultMode));
    return resultMode;
}

/// <summary>
/// Gets the language identifier mode and priority from properties.
/// </summary>
/// <returns>The USP language id mode</returns>
void CSpxUspRecoEngineAdapter::UpdateLanguageIdModeAndPriorityFromProperties(
    USP::LanguageIdMode& languageIdMode,
    USP::LanguageIdPriority& languageIdPriority) const
{
    auto TryParseLanguageIdModeAndPriority =
        [](const char* propertyLanguageIdMode, USP::LanguageIdMode& mode, USP::LanguageIdPriority& priority) -> bool
    {
        if (*propertyLanguageIdMode == 0)
        {
            return false;
        }
        else if (PAL::stricmp(propertyLanguageIdMode, g_languageIdModeContinuous) == 0)
        {
            mode = USP::LanguageIdMode::DetectContinuous;
            priority = USP::LanguageIdPriority::PrioritizeLatency;
            return true;
        }
        else if (PAL::stricmp(propertyLanguageIdMode, g_languageIdModeAtStart) == 0)
        {
            mode = USP::LanguageIdMode::DetectAtAudioStart;
            priority = USP::LanguageIdPriority::PrioritizeLatency;
            return true;
        }
        else if (PAL::stricmp(propertyLanguageIdMode, g_languageIdModeAtStartHighAccuracy) == 0)
        {
            mode = USP::LanguageIdMode::DetectAtAudioStart;
            priority = USP::LanguageIdPriority::PrioritizeAccuracy;
            return true;
        }
        else
        {
            return false;
        }
    };

    // Property value set by the application
    auto propertyLanguageIdMode = GetOr(PropertyId::SpeechServiceConnection_LanguageIdMode, "");

    if (!TryParseLanguageIdModeAndPriority(
            propertyLanguageIdMode.c_str(),
            languageIdMode,
            languageIdPriority))
    {
        SPX_TRACE_INFO("SpeechServiceConnection_LanguageIdMode not set or has invalid value %s.", propertyLanguageIdMode.c_str());
    }
}

void CSpxUspRecoEngineAdapter::SetSpeechConfigMessage()
{
    ajv::JsonBuilder speechConfig;

    auto context = speechConfig["context"];

    auto system = context["system"];
    system["version"] = BuildInformation::g_fullSpeechVersion;
    system["name"] = BuildInformation::g_SpeechSDKName;
    system["build"] = BuildInformation::g_buildPlatform;
    system["lang"] <<= GetOr("AZAC-SDK-PROGRAMMING-LANGUAGE", "");

    auto os = context["os"];
    auto osInfo = PAL::getOperatingSystem();
    os["name"] = osInfo.name;
    os["version"] = osInfo.version;
    os["platform"] = osInfo.platform;

    // Set the audio configuration data.
    // Todo: Fill audio configuration data with the value via property bags.
    auto audioSource = speechConfig["context"]["audio"]["source"];
    audioSource["type"] = GetOr(PropertyId::AudioConfig_AudioSource, "");
    audioSource["model"] = GetOr("SPEECH-MicrophoneNiceName", "");
    audioSource["samplerate"] = GetOr(PropertyId::AudioConfig_SampleRateForCapture, "");
    audioSource["bitspersample"] = GetOr(PropertyId::AudioConfig_BitsPerSampleForCapture, "");
    audioSource["channelcount"] = GetOr(PropertyId::AudioConfig_NumberOfChannelsForCapture, "");

    if (m_useMultiChannelProcessing)
    {
        audioSource["SeparateChannelProcessing"] = "true";
    }

    for (const auto& item : GetParametersFromRecognizer("speech.config"))
    {
        const auto& name = item.first;
        const auto& value = item.second;
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, value.empty());
        context[name] = ajv::json::Parse(value);
    }

    for (const auto& item : GetParametersFromUser("speech.config"))
    {
        const auto& name = item.first;
        const auto& value = item.second;
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, value.empty());
        context[name] = ajv::json::Parse(value);
    }

    m_speechConfig = speechConfig.AsJson();
}

void CSpxUspRecoEngineAdapter::UspSendSpeechConfig()
{
    constexpr auto messagePath = "speech.config";
    SPX_DBG_TRACE_VERBOSE("%s %s", messagePath, m_speechConfig.c_str());
    UspSendMessage(
        std::make_unique<USP::TextMessage>(
            m_speechConfig, messagePath, "application/json", USP::MessageType::Config));
}

void CSpxUspRecoEngineAdapter::UspSendAgentConfig()
{
    if (m_endpointType != USP::EndpointType::Dialog)
    {
        return;
    }

    ajv::JsonBuilder agentConfigJson;
    agentConfigJson["version"] = 0.2;
    agentConfigJson["ttsOutputFormat"] <<= GetOr(PropertyId::SpeechServiceConnection_SynthOutputFormat, "");

    auto botInfo = agentConfigJson["botInfo"];
    botInfo["commType"] = GetOr("Conversation_Communication_Type", "Default");
    botInfo["conversationId"] <<= m_dialogConversationId;
    botInfo["connectionId"] <<= GetOr(PropertyId::Conversation_Connection_Id, "");
    botInfo["fromId"] <<= GetOr(PropertyId::Conversation_From_Id, "");
    botInfo["commandsCulture"] <<= GetOr(PropertyId::Conversation_DialogType, "") == g_dialogType_CustomCommands
        ? GetOr(PropertyId::SpeechServiceConnection_RecoLanguage, "").c_str()
        : "";

    auto configJson = agentConfigJson.AsJson();

    constexpr auto messagePath = "agent.config";
    SPX_DBG_TRACE_VERBOSE("%s %s", messagePath, configJson.c_str());
    UspSendMessage(messagePath, configJson, USP::MessageType::Config);
}

void CSpxUspRecoEngineAdapter::UspSendSpeechAgentContext()
{
    if (m_endpointType != USP::EndpointType::Dialog)
    {
        return;
    }

    // The Dialog Service Connector is responsible for generating an interaction ID here, so we send it as a
    // speech.agent.context message
    auto site = GetSite();
    auto provider = SpxQueryService<ISpxInteractionIdProvider>(site);
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, provider == nullptr);

    ajv::JsonBuilder contextJson;
    contextJson["version"] = 0.5;
    contextJson["context"]["interactionId"] = provider->GetInteractionId(InteractionIdPurpose::Speech);
    contextJson["channelData"] = "";
    contextJson["messagePayload"] <<= GetOr(PropertyId::Conversation_Speech_Activity_Template, "");

    UspSendMessage("speech.agent.context", contextJson.AsJson(), USP::MessageType::AgentContext);
}

void CSpxUspRecoEngineAdapter::UspSendSpeechEvent()
{
    auto site = GetSite();
    auto provider = SpxQueryService<ISpxSpeechEventPayloadProvider>(site);
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, provider == nullptr);

    // true for starting streaming audio.
    auto payload = provider->GetSpeechEventPayload(true);

    if (!payload.empty())
    {
        UspSendMessage("speech.event", payload, USP::MessageType::SpeechEvent);
    }
}

void CSpxUspRecoEngineAdapter::UspSendSpeechContext()
{
    // create speech context json
    auto speechContext = GetSpeechContextJson();

    if (!speechContext.empty())
    {
        UspSendMessage("speech.context", speechContext, USP::MessageType::Context);
    }
 }

void CSpxUspRecoEngineAdapter::UspSendMessage(const char *messagePath, const std::string &buffer, USP::MessageType messageType)
{
    // Make sure subscription key information is hidden and replaced with (*) in the speech.config json trace string
    auto traceStr = buffer;
    PAL::StringUtils::ReplaceWithSubString(traceStr, R"("key":")", "******************************");
    PAL::StringUtils::ReplaceWithSubString(traceStr, "subscription-key=", "******************************");
    SPX_DBG_TRACE_VERBOSE("%s='%s'", messagePath, traceStr.c_str());
    return UspSendMessage(std::make_unique<USP::TextMessage>(buffer, messagePath, messageType));
}

void CSpxUspRecoEngineAdapter::UspSendMessage(std::unique_ptr<USP::Message> message)
{
    SPX_DBG_ASSERT(m_uspConnection != nullptr || IsState(UspState::Terminating) || IsState(UspState::Zombie));
    if (IsBadState() || m_uspConnection == nullptr)
    {
        /* Notify user there was an error */
        InvokeOnSite([this](const SitePtr& p)
        {
            auto error = ErrorInfo::FromExplicitError(CancellationErrorCode::ConnectionFailure, "Connection is in a bad state.");
            p->Error(this, error);
        });
        SPX_TRACE_ERROR("no connection established or in bad USP state. m_uspConnection %s nullptr, m_uspState:%d.", 
            m_uspConnection == nullptr ? "is" : "is not", 
            static_cast<int>(m_uspState));

        return;
    }
    return m_uspConnection->QueueMessage(std::move(message));
}

//
// This is called for writing the audio file format
//
void CSpxUspRecoEngineAdapter::ProcessAudioFormat(SPXWAVEFORMATEX* pformat)
{
    bool useAudioCompression = m_compressionCodec != nullptr;

    if (!useAudioCompression)
    {
        auto wavHeaderDataPtr = MakeDataChunkForAudioFormat(pformat);

        m_uspAudioByteCount += wavHeaderDataPtr->size;
        if (!useAudioCompression)
        {
            UspWriteActual(wavHeaderDataPtr);
        }
    }
}

void CSpxUspRecoEngineAdapter::ProcessAudioChunk(const DataChunkPtr& audioChunk)
{
    m_uspAudioByteCount += audioChunk->size;

    m_saveToWavEverything.SaveToWav(audioChunk->data.get(), audioChunk->size);
    m_saveToWavCurrentTurn.SaveToWav(audioChunk->data.get(), audioChunk->size);

    if (m_compressionCodec != nullptr)
    {
        m_compressionCodec->Encode(audioChunk->data.get(), audioChunk->size);
    }
    else
    {
        UspWriteActual(audioChunk);
    }

}

void CSpxUspRecoEngineAdapter::UspWriteActual(const DataChunkPtr& audioChunk)
{
    SPX_DBG_TRACE_VERBOSE("%s(..., %d)", __FUNCTION__, audioChunk->size);
    SPX_DBG_ASSERT(m_uspConnection != nullptr || IsState(UspState::Terminating) || IsState(UspState::Zombie));
    if (!IsState(UspState::Terminating) && !IsState(UspState::Zombie) && m_uspConnection != nullptr)
    {
        m_uspConnection->QueueAudioSegment(audioChunk);
    }
    else
    {
        SPX_TRACE_ERROR("%s: unexpected USP connection state:%d. Not sending audio chunk (size=%d).", __FUNCTION__, static_cast<int>(m_uspState), audioChunk->size);
    }
}

void CSpxUspRecoEngineAdapter::FlushAudio(bool flushCodec /*= false*/)
{
    // We should only ever be asked to Flush when we're in a valid state ...
    SPX_DBG_ASSERT(m_uspConnection != nullptr || IsState(UspState::Terminating) || IsState(UspState::Zombie));
    if (!IsState(UspState::Terminating) && !IsState(UspState::Zombie) && m_uspConnection != nullptr)
    {
        // Get the remains of compressed audio out of the codec
        if (m_compressionCodec != nullptr && flushCodec && !m_audioFlushed)
        {
            m_compressionCodec->Flush();
            m_audioFlushed = true;
        }

        m_uspConnection->QueueAudioEnd();
    }
}

void CSpxUspRecoEngineAdapter::WriteTelemetryLatency(uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesis)
{
    if (!m_ignoreTelemetry)
    {
        SPX_DBG_ASSERT(m_uspConnection != nullptr);
        if (m_uspConnection == nullptr)
        {
            SPX_TRACE_ERROR("%s: m_uspConnection is null.", __FUNCTION__);
        }
        else
        {
            m_uspConnection->WriteTelemetryLatency(latencyInTicks, isPhraseLatency, isFirstHypothesis);
        }
    }
    m_ignoreTelemetry = false;
}

void CSpxUspRecoEngineAdapter::FlushTelemetry()
{
    if (m_uspConnection != nullptr)
    {
        m_uspConnection->FlushTelemetry();
    }
}

void CSpxUspRecoEngineAdapter::OnMessageReceived(const USP::RawMsg& m)
{
    // Check the message for offset or tokens.

    InvokeOnSite([&](const SitePtr& p) { p->FireConnectionMessageReceived(m.headers, m.path, m.buffer, m.bufferSize, m.isBufferBinary); });
}

void CSpxUspRecoEngineAdapter::OnSpeechStartDetected(const USP::SpeechStartDetectedMsg& message)
{
    // The USP message for SpeechStartDetected isn't what it might sound like in all "reco modes" ...
    // * In INTERACTIVE mode, it works as it sounds. It indicates the beginning of speech for the "phrase" message that will arrive later
    // * In CONTINUOUS modes, however, it corresponds to the time of the beginning of speech for the FIRST "phrase" message of many inside one turn

    SPX_DBG_TRACE_VERBOSE("Response: Speech.StartDetected message. Speech starts at offset %" PRIu64 " (100ns).\n", message.offset + m_startingOffset);

    if (IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d) %s", 
            __FUNCTION__, 
            (void*)this, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState), 
            IsState(UspState::Terminating) ? "(USP-TERMINATING)" : "********** USP-UNEXPECTED !!!!!!");
    }
    else if (IsState(UspState::WaitingForPhrase))
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) site->AdapterDetectedSpeechStart()", __FUNCTION__, (void*)this);
        InvokeOnSite([this, &message](const SitePtr& p) { p->AdapterDetectedSpeechStart(this, message.offset + m_startingOffset); });
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", __FUNCTION__, (void*)this, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::OnSpeechEndDetected(const USP::SpeechEndDetectedMsg& message)
{
    SPX_DBG_TRACE_VERBOSE("Response: Speech.EndDetected message. Speech ends at offset %" PRIu64 " (100ns)\n", message.offset + m_startingOffset);

    auto requestMute = TryChangeState(AudioState::Sending, AudioState::Mute);

    if (IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d) %s", 
            __FUNCTION__, 
            (void*)this, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState), 
            IsState(UspState::Terminating) ? "(USP-TERMINATING)" : "********** USP-UNEXPECTED !!!!!!");
    }
    else if (IsStateBetweenIncluding(UspState::WaitingForPhrase, UspState::WaitingForTurnEnd) &&
             (IsState(AudioState::Idle) ||
              IsState(AudioState::Mute)))
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) site->AdapterDetectedSpeechEnd()", __FUNCTION__, (void*)this);
        InvokeOnSite([this, &message](const SitePtr& p) { p->AdapterDetectedSpeechEnd(this, message.offset + m_startingOffset); });
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", __FUNCTION__, (void*)this, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
        return;
    }

    SPX_DBG_TRACE_VERBOSE("%s: Flush ... (audioState/uspState=%d/%d)  USP-FLUSH", __FUNCTION__, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    FlushAudio();
    if (requestMute && !IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: site->AdapterRequestingAudioMute(true) ... (audioState/uspState=%d/%d)", __FUNCTION__, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
        InvokeOnSite([this](const SitePtr& p) { p->AdapterRequestingAudioMute(this, true); });
    }
}

void CSpxUspRecoEngineAdapter::OnSpeechHypothesis(const USP::SpeechHypothesisMsg& message)
{
    SPX_DBG_TRACE_VERBOSE("Response: Speech.Hypothesis message. Starts at offset %" PRIu64 ", with duration %" PRIu64 " (100ns). Text: %s\n", 
        message.offset + m_startingOffset, 
        message.duration, 
        message.text.c_str());

    if (IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d) %s", 
            __FUNCTION__, 
            (void*)this, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState), 
            IsState(UspState::Terminating) ? "(USP-TERMINATING)" : "********** USP-UNEXPECTED !!!!!!");
    }
    else if (IsState(UspState::WaitingForPhrase))
    {
        SPX_DBG_TRACE_VERBOSE("%s: site->FireAdapterResult_Intermediate()", __FUNCTION__);

        InvokeOnSite([&](const SitePtr& site)
        {
            auto factory = SpxQueryService<ISpxRecoResultFactory>(site);
            auto result = factory->CreateIntermediateResult(message.text.c_str(), message.offset, message.duration, message.phraseId.c_str());
            auto namedProperties = SpxQueryInterface<ISpxNamedProperties>(result);
            namedProperties->Set(PropertyId::SpeechServiceResponse_JsonResult, message.json.c_str());
            namedProperties->Set(PropertyId::SpeechServiceResponse_RecognitionBackend, "online");

            if (!message.speaker.empty())
            {
                CreateConversationResult(result, message.speaker, message.utteranceId);
            }

            if (!message.language.empty())
            {
                namedProperties->Set(
                    PropertyId::SpeechServiceConnection_AutoDetectSourceLanguageResult,
                    message.language.c_str());
            }

            if (message.isTentativePhrase == true)
            {
                namedProperties->SetStringValue("SpeechServiceResponse_IsTentativePhrase", "true");
            }
            else
            {
                namedProperties->SetStringValue("SpeechServiceResponse_IsTentativePhrase", "false");
            }

            site->FireAdapterResult_Intermediate(message.offset, result);
        });
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)",
             __FUNCTION__, 
             (void*)this, 
             static_cast<int>(m_audioState), 
             static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::OnSpeechKeywordDetected(const USP::SpeechKeywordDetectedMsg& message)
{
    SPX_DBG_TRACE_VERBOSE("Response: Speech.Keyword message. Status: %d, Text: %s, starts at %" PRIu64 ", with duration %" PRIu64 " (100ns).\n", 
        static_cast<int>(message.status), 
        message.text.c_str(), 
        message.offset + m_startingOffset, 
        message.duration);

    if (IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d) %s", __FUNCTION__, (void*)this, 
        static_cast<int>(m_audioState), 
        static_cast<int>(m_uspState), 
        IsState(UspState::Terminating) ? "(USP-TERMINATING)" : "********** USP-UNEXPECTED !!!!!!");
    }
    else if (message.status == USP::KeywordVerificationStatus::Accepted && IsState(UspState::WaitingForPhrase))
    {
        SPX_DBG_TRACE_VERBOSE("%s: site->FireAdapterResult_Intermediate()", __FUNCTION__);

        InvokeOnSite([&](const SitePtr& site)
        {
            auto factory = SpxQueryService<ISpxRecoResultFactory>(site);
            auto result = factory->CreateKeywordResult(1.0, message.offset, message.duration, message.text.c_str(), ResultReason::RecognizedKeyword, nullptr);
            auto namedProperties = SpxQueryInterface<ISpxNamedProperties>(result);
            namedProperties->Set(PropertyId::SpeechServiceResponse_JsonResult, message.json.c_str());
            site->FireAdapterResult_KeywordResult(message.offset, result, true);
        });
    }
    else if (message.status == USP::KeywordVerificationStatus::Rejected && !m_continueOnKeywordReject && TryChangeState(UspState::WaitingForPhrase, UspState::WaitingForTurnEnd))
    {
        SPX_DBG_TRACE_VERBOSE("%s: site->FireAdapterResult_Final()", __FUNCTION__);

        InvokeOnSite([&](const SitePtr& site)
        {
            auto factory = SpxQueryService<ISpxRecoResultFactory>(site);
            auto result = factory->CreateKeywordResult(1.0, message.offset, message.duration, message.text.c_str(), ResultReason::NoMatch, nullptr);
            auto namedProperties = SpxQueryInterface<ISpxNamedProperties>(result);
            namedProperties->Set(PropertyId::SpeechServiceResponse_JsonResult, message.json.c_str());
            site->FireAdapterResult_KeywordResult(message.offset, result, false);
        });
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", __FUNCTION__, (void*)this, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::OnSpeechFragment(const USP::SpeechFragmentMsg& message)
{
    SPX_DBG_TRACE_VERBOSE("Response: Speech.Fragment message. Starts at offset %" PRIu64 ", with duration %" PRIu64 " (100ns). Text: %s\n", message.offset + m_startingOffset, message.duration, message.text.c_str());

    bool sendIntermediate = false;

    if (IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d) %s", __FUNCTION__, (void*)this, static_cast<int>(m_audioState), static_cast<int>(m_uspState), IsState(UspState::Terminating) ? "(USP-TERMINATING)" : "********** USP-UNEXPECTED !!!!!!");
    }
    else if (IsState(UspState::WaitingForPhrase))
    {
        sendIntermediate = true;
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", __FUNCTION__, (void*)this, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }

    if (sendIntermediate)
    {
        SPX_DBG_TRACE_VERBOSE("%s: site->FireAdapterResult_Intermediate()", __FUNCTION__);

        InvokeOnSite([&](const SitePtr& site)
        {
            auto factory = SpxQueryService<ISpxRecoResultFactory>(site);
            auto result = factory->CreateIntermediateResult(message.text.c_str(), message.offset, message.duration, message.phraseId.c_str());

            auto namedProperties = SpxQueryInterface<ISpxNamedProperties>(result);
            namedProperties->Set(PropertyId::SpeechServiceResponse_JsonResult, message.json.c_str());
            if (!message.speaker.empty())
            {
                CreateConversationResult(result, message.speaker, message.utteranceId);
            }
            if (!message.language.empty())
            {
                namedProperties->Set(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguageResult, message.language.c_str());
            }
            site->FireAdapterResult_Intermediate(message.offset, result);
        });
    }
}

void CSpxUspRecoEngineAdapter::OnSpeechPhrase(const USP::SpeechPhraseMsg& message)
{
    SPX_DBG_TRACE_VERBOSE("Response: Speech.Phrase message. Status: %d, Text: %s, starts at %" PRIu64 ", with duration %" PRIu64 " (100ns).\n", 
        static_cast<int>(message.recognitionStatus), 
        message.displayText.c_str(), 
        message.offset + m_startingOffset, 
        message.duration);
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);

    if (IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d) %s", 
            __FUNCTION__, 
            (void*)this, 
            static_cast<int>(m_audioState), 
            static_cast<int>(m_uspState), 
            IsState(UspState::Terminating) ? "(USP-TERMINATING)" : "********** USP-UNEXPECTED !!!!!!");
    }
    else if ((m_isInteractiveMode && TryChangeState(UspState::WaitingForPhrase, UspState::WaitingForTurnEnd)) ||
        (!m_isInteractiveMode && TryChangeState(UspState::WaitingForPhrase, UspState::WaitingForPhrase)))
    {
        if (message.recognitionStatus == RecognitionStatus::EndOfDictation)
        {
            InvokeOnSite([&](const SitePtr& site)
            {
                site->AdapterEndOfDictation(this, message.offset + m_startingOffset, message.duration);
            });
        }
        else
        {
            SPX_DBG_TRACE_VERBOSE("%s: FireFinalResultNow()", __FUNCTION__);
            FireFinalResultNow(message);
        }
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", __FUNCTION__, (void*)this, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::CreateConversationResult(std::shared_ptr<ISpxRecognitionResult>& result, const std::string& userId, const std::string& utteranceId)
{
    auto initConversationResult = SpxQueryInterface<ISpxConversationTranscriptionResultInit>(result);
    if (initConversationResult != nullptr)
    {
        initConversationResult->InitConversationResult(userId.c_str(), utteranceId.c_str());
    }
    else
    {
        auto initMeetingResult = SpxQueryInterface<ISpxMeetingTranscriptionResultInit>(result);
        if (initMeetingResult == nullptr)
        {
            ThrowInvalidArgumentException("Can't get conversation result");
        }
        initMeetingResult->InitMeetingResult(userId.c_str(), utteranceId.c_str());
    }
}

static TranslationStatusCode GetTranslationStatus(USP::TranslationStatus uspStatus)
{
    TranslationStatusCode status = TranslationStatusCode::Error;
    switch (uspStatus)
    {
    case USP::TranslationStatus::Success:
        status = TranslationStatusCode::Success;
        break;
    case USP::TranslationStatus::Error:
        break;
    case USP::TranslationStatus::InvalidMessage:
        // The failureReason contains additional error messages.
        // Todo: have better error handling for different statuses.
        break;
    default:
        SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
        break;
    }
    return status;
}

DataChunkPtr CSpxUspRecoEngineAdapter::MakeDataChunkForAudioFormat(SPXWAVEFORMATEX* pformat)
{
    static const uint16_t cbTag = 4;
    static const uint16_t cbChunkType = 4;
    static const uint16_t cbChunkSize = 4;

    uint32_t cbFormatChunk = (pformat->cbSize == 0) ? sizeof(SPXWAVEFORMAT) : (sizeof(SPXWAVEFORMATEX) + pformat->cbSize);
    uint32_t cbRiffChunk = 0;       // NOTE: This isn't technically accurate for a RIFF/WAV file, but it's fine for the speech service
    uint32_t cbDataChunk = 0;       // NOTE: Similarly, this isn't technically correct for the 'data' chunk, but it's fine for the speech service

    uint32_t cbHeader =
        cbTag + cbChunkSize +       // 'RIFF' #size_of_RIFF#
        cbChunkType +               // 'WAVE'
        cbChunkType + cbChunkSize + // 'fmt ' #size_fmt#
        cbFormatChunk +             // actual format
        cbChunkType + cbChunkSize;  // 'data' #size_of_data#

    // Allocate the buffer, and create a ptr we'll use to advance thru the buffer as we're writing stuff into it
    auto buffer = SpxAllocSharedAudioBuffer(cbHeader);
    auto ptr = buffer.get();

    // The 'RIFF' header (consists of 'RIFF' followed by size of payload that follows)
    ptr = FormatBufferWriteChars(ptr, "RIFF", cbTag);
    ptr = FormatBufferWriteNumber(ptr, cbRiffChunk);

    // The 'WAVE' chunk header
    ptr = FormatBufferWriteChars(ptr, "WAVE", cbChunkType);

    // The 'fmt ' chunk (consists of 'fmt ' followed by the total size of the SPXWAVEFORMAT(EX)(TENSIBLE), followed by the SPXWAVEFORMAT(EX)(TENSIBLE)
    ptr = FormatBufferWriteChars(ptr, "fmt ", cbChunkType);
    ptr = FormatBufferWriteNumber(ptr, cbFormatChunk);
    ptr = FormatBufferWriteBytes(ptr, (uint8_t*)pformat, cbFormatChunk);

    // The 'data' chunk is next
    ptr = FormatBufferWriteChars(ptr, "data", cbChunkType);
    ptr = FormatBufferWriteNumber(ptr, cbDataChunk);

    // Now that we've prepared the header/buffer, send it along to the speech service via UspWrite
    SPX_DBG_ASSERT(cbHeader == uint32_t(ptr - buffer.get()));
    auto wavHeaderDataPtr = std::make_shared<DataChunk>(buffer, cbHeader);
    wavHeaderDataPtr->isWavHeader = true;

    return wavHeaderDataPtr;
}


void CSpxUspRecoEngineAdapter::OnTranslationHypothesis(const USP::TranslationHypothesisMsg& message)
{
    SPX_DBG_TRACE_VERBOSE("Response: Translation.Hypothesis message. RecoText: %s, TranslationStatus: %d, starts at %" PRIu64 ", with duration %" PRIu64 " (100ns).\n",
        message.text.c_str(), 
        static_cast<int>(message.translation.translationStatus), 
        message.offset + m_startingOffset,
        message.duration);
    auto resultMap = message.translation.translations;
#ifdef _DEBUG
    for (const auto& it : resultMap)
    {
        const auto& lang = std::get<0>(it);
        const auto& text = std::get<1>(it);
        SPX_DBG_TRACE_VERBOSE("          Translation in %s: %s,\n", lang.c_str(), text.c_str());
    }
#endif

    if (IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: IGNORING (Err/Terminating/Zombie)... (audioState/uspState=%d/%d)", __FUNCTION__, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }
    else if (IsState(UspState::WaitingForPhrase))
    {
        {
            SPX_DBG_TRACE_SCOPE("Fire intermediate translation result: Creating Result", "FireIntermeidateResult: GetSite()->FireAdapterResult_Intermediate()  complete!");
            InvokeOnSite([&](const SitePtr& site)
            {
                // Create the result
                auto factory = SpxQueryService<ISpxRecoResultFactory>(site);
                auto result = factory->CreateIntermediateResult(message.text.c_str(), message.offset, message.duration, message.phraseId.c_str());

                auto namedProperties = SpxQueryInterface<ISpxNamedProperties>(result);
                namedProperties->Set(PropertyId::SpeechServiceResponse_JsonResult, message.json.c_str());
                if (!message.language.empty())
                {
                    namedProperties->Set(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguageResult, message.language.c_str());
                }
                namedProperties->Set(PropertyId::SpeechServiceResponse_RecognitionBackend, "online");

                // Update our result to be a "TranslationText" result.
                auto initTranslationResult = SpxQueryInterface<ISpxTranslationRecognitionResultInit>(result);

                auto status = GetTranslationStatus(message.translation.translationStatus);
                initTranslationResult->InitTranslationRecognitionResult(status, message.translation.translations, message.translation.failureReason);

                // Fire the result
                site->FireAdapterResult_Intermediate(message.offset, result);
            });
        }
    }
    else
    {
        SPX_TRACE_ERROR("%s: Unexpected USP State transition (audioState/uspState=%d/%d)", __FUNCTION__, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::OnTranslationPhrase(const USP::TranslationPhraseMsg& message)
{
    auto resultMap = message.translation.translations;

    SPX_DBG_TRACE_VERBOSE("Response: Translation.Phrase message. RecoStatus: %d, TranslationStatus: %d, RecoText: %s, starts at %" PRIu64 ", with duration %" PRIu64 " (100ns).\n",
        static_cast<int>(message.recognitionStatus),
        static_cast<int>(message.translation.translationStatus),
        message.text.c_str(),
        message.offset + m_startingOffset,
        message.duration);
        
#ifdef _DEBUG
    if (message.translation.translationStatus != USP::TranslationStatus::Success)
    {
        SPX_DBG_TRACE_VERBOSE(" FailureReason: %ls.", message.translation.failureReason.c_str());
    }
    for (const auto& it : resultMap)
    {
        const auto& lang = std::get<0>(it);
        const auto& text = std::get<1>(it);
        SPX_DBG_TRACE_VERBOSE("          , translated to %s: %s,\n", lang.c_str(), text.c_str());
    }
#endif

    if (IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: IGNORING (Err/Terminating/Zombie)... (audioState/uspState=%d/%d)", __FUNCTION__, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }
    else if ((m_isInteractiveMode && TryChangeState(UspState::WaitingForPhrase, UspState::WaitingForTurnEnd)) ||
             (!m_isInteractiveMode && TryChangeState(UspState::WaitingForPhrase, UspState::WaitingForPhrase)))
    {
        SPX_DBG_TRACE_SCOPE("Fire final translation result: Creating Result", "FireFinalResul: GetSite()->FireAdapterResult_FinalResult()  complete!");
        if (message.recognitionStatus == RecognitionStatus::EndOfDictation)
        {
            InvokeOnSite([&](const SitePtr& site)
            {
                site->AdapterEndOfDictation(this, message.offset + m_startingOffset, message.duration);
            });
        }
        else
        {
            auto cancellationReason = ToCancellationReason(message.recognitionStatus);
            if (cancellationReason != REASON_CANCELED_NONE)
            {
                // The status above should have been treated as error result, so this should not happen here.
                SPX_TRACE_ERROR("Unexpected recognition status %d.", static_cast<int>(message.recognitionStatus));
                SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
            }

            InvokeOnSite([&](const SitePtr& site)
            {
                // Create the result
                auto factory = SpxQueryService<ISpxRecoResultFactory>(site);
                auto result = factory->CreateFinalResult(
                    ToReason(message.recognitionStatus),
                    ToNoMatchReason(message.recognitionStatus),
                    message.text.c_str(),
                    message.offset,
                    message.duration,
                    message.phraseId.c_str());

                auto namedProperties = SpxQueryInterface<ISpxNamedProperties>(result);
                namedProperties->Set(PropertyId::SpeechServiceResponse_JsonResult, message.json.c_str());
                if (!message.language.empty())
                {
                    namedProperties->Set(
                        PropertyId::SpeechServiceConnection_AutoDetectSourceLanguageResult,
                        message.language.c_str());
                }
                namedProperties->Set(PropertyId::SpeechServiceResponse_RecognitionBackend, "online");

                if (!m_currentRequestId.empty())
                {
                    namedProperties->Set(PropertyId::SpeechServiceResponse_RequestId, m_currentRequestId.c_str());
                }

                // Update our result to be an "TranslationText" result.
                auto initTranslationResult = SpxQueryInterface<ISpxTranslationRecognitionResultInit>(result);

                auto status = GetTranslationStatus(message.translation.translationStatus);
                initTranslationResult->InitTranslationRecognitionResult(
                    status, message.translation.translations,
                    message.translation.failureReason);

                // Fire the result
                site->FireAdapterResult_FinalResult(message.offset, result);
            });
        }
    }
    else
    {
        SPX_TRACE_ERROR("%s: Unexpected USP State transition (audioState/uspState=%d/%d)", __FUNCTION__,static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::OnAudioOutputChunk(const USP::AudioOutputChunkMsg& message)
{
    SPX_DBG_TRACE_VERBOSE("Response: Audio output chunk message. Audio data size: %zu\n", message.audioLength);

    if (m_endpointType == USP::EndpointType::Dialog)
    {
        auto it = m_request_session_map.find(message.requestId);
        if (it != m_request_session_map.end())
        {
            auto& machine = it->second;
            machine->AudioReceived(message);
        }
        return;
    }

    InvokeOnSite([&message](const SitePtr &site)
    {
        auto factory = SpxQueryService<ISpxRecoResultFactory>(site);
        auto result = factory->CreateFinalResult(ResultReason::SynthesizingAudio, NO_MATCH_REASON_NONE, "", 0, 0, "");

        // Update our result to be an "TranslationSynthesis" result.
        auto initTranslationResult = SpxQueryInterface<ISpxTranslationSynthesisResultInit>(result);
        initTranslationResult->InitTranslationSynthesisResult(message.audioBuffer, message.audioLength, message.requestId);

        site->FireAdapterResult_TranslationSynthesis(result);
    });
}

void CSpxUspRecoEngineAdapter::OnTurnStart(const USP::TurnStartMsg& message)
{
    SPX_DBG_TRACE_VERBOSE("Response: Turn.Start message. Context.ServiceTag: %s\n", message.contextServiceTag.c_str());
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);

    if (IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d) %s", __FUNCTION__, (void*)this, static_cast<int>(m_audioState), static_cast<int>(m_uspState), IsState(UspState::Terminating) ? "(USP-TERMINATING)" : "********** USP-UNEXPECTED !!!!!!");
    }
    else if (TryChangeState(UspState::WaitingForTurnStart, UspState::WaitingForPhrase))
    {
        // Store the request ID for later use in results
        m_currentRequestId = message.requestId;

        if (!message.serviceManagesOffset)
        {
            if (auto maybeOffset = Get<uint64_t>(g_audioContinuationOffset))
            {
                m_startingOffset = maybeOffset.Get();
                SPX_DBG_TRACE_VERBOSE("%s: set starting offset=%" PRIu64 "", __FUNCTION__, m_startingOffset);
            }
        }

        InvokeOnSite([this, &message](const SitePtr& p) { p->AdapterStartedTurn(this, message.contextServiceTag, message.offset); });

        SetStringValue("SPEECH-UspContinuationServiceTag", message.contextServiceTag.c_str());
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", __FUNCTION__, (void*)this, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::OnTurnEnd(const USP::TurnEndMsg&)
{
    SPX_DBG_TRACE_SCOPE("CSpxUspRecoEngineAdapter::OnTurnEnd ... started... ", "CSpxUspRecoEngineAdapter::OnTurnEnd ... DONE!");
    SPX_DBG_TRACE_VERBOSE("Response: Turn.End message.\n");

    auto adapterTurnStopped = false;

    auto prepareReady =  !m_singleShot &&
        (TryChangeState(AudioState::Sending, AudioState::Ready) ||
         TryChangeState(AudioState::Mute, AudioState::Ready));

    auto requestMute = m_singleShot && TryChangeState(AudioState::Sending, AudioState::Mute);

    if (IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d) %s", __FUNCTION__, (void*)this, static_cast<int>(m_audioState), static_cast<int>(m_uspState), IsState(UspState::Terminating) ? "(USP-TERMINATING)" : "********** USP-UNEXPECTED !!!!!!");
    }
    else if (( m_isInteractiveMode && TryChangeState(UspState::WaitingForTurnEnd, UspState::Idle)) ||
             (!m_isInteractiveMode && TryChangeState(UspState::WaitingForPhrase, UspState::Idle)))
    {
        adapterTurnStopped = true;
    }
    else
    {
        SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", __FUNCTION__, (void*)this, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }

    if (prepareReady && !IsBadState())
    {
        SPX_DBG_TRACE_VERBOSE("%s: PrepareAudioReadyState()", __FUNCTION__);
        PrepareAudioReadyState();

        SPX_DBG_TRACE_VERBOSE("%s: site->AdapterRequestingAudioMute(false) ... (audioState/uspState=%d/%d)", __FUNCTION__, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
        InvokeOnSite([this](const SitePtr& p) { p->AdapterRequestingAudioMute(this, false); });
    }

    if (adapterTurnStopped && ShouldResetAfterTurnStopped())
    {
        ResetAfterTurnStopped();
    }

    auto site = GetSite();
    if (!site)
        return;

    if (adapterTurnStopped)
    {
        SPX_DBG_TRACE_VERBOSE("%s: site->AdapterStoppedTurn()", __FUNCTION__);
        site->AdapterStoppedTurn(this);
    }

    if (requestMute)
    {
        SPX_DBG_TRACE_VERBOSE("%s: FlushAudio()  USP-FLUSH", __FUNCTION__);
        FlushAudio();

        SPX_DBG_TRACE_VERBOSE("%s: site->AdapterRequestingAudioMute(true) ... (audioState/uspState=%d/%d)", __FUNCTION__, static_cast<int>(m_audioState), static_cast<int>(m_uspState));
        site->AdapterRequestingAudioMute(this, true);
    }

    // Disposing of the codec.
    m_compressionCodec.reset();
}


void CSpxUspRecoEngineAdapter::OnMessageStart(const USP::TurnStartMsg& message)
{
    /* For now only handle service generated messages for dialog endpoints */
    if (m_endpointType == USP::EndpointType::Dialog)
    {
        std::weak_ptr<ISpxActivityResultAdapter> weakResultAdapter{ ISpxInterfaceBaseFor<ISpxActivityResultAdapter>::shared_from_this() };
        // start the initial state as SessionStart
        m_request_session_map.emplace(message.requestId, std::make_unique<CSpxActivitySession>(weakResultAdapter));
    }
}

void CSpxUspRecoEngineAdapter::OnMessageEnd(const USP::TurnEndMsg& message)
{
    /* For now only handle service generated messages for dialog endpoints */
    if (m_endpointType == USP::EndpointType::Dialog)
    {
        auto it = m_request_session_map.find(message.requestId);
        if (it != m_request_session_map.end())
        {
            auto& machine = it->second;
            machine->End();
            m_request_session_map.erase(it);
        }
    }
}

void CSpxUspRecoEngineAdapter::OnError(const std::shared_ptr<ISpxErrorInformation>& error)
{
    SPX_TRACE_ERROR("Response: On Error: Code:%d, Message: %s.\n", (int)error->GetCancellationCode(), error->GetDetails().c_str());

    /* If we receive an error, clear all ongoing activity sessions. */
    if (m_endpointType == USP::EndpointType::Dialog)
    {
        m_request_session_map.clear();
    }

    if (IsBadState())
    {
        SPX_TRACE_ERROR("%s: (0x%8p) IGNORING... (audioState/uspState=%d/%d) %s",
             __FUNCTION__, 
             (void*)this, 
             static_cast<int>(m_audioState), 
             static_cast<int>(m_uspState), 
             IsState(UspState::Terminating) ? "(USP-TERMINATING)" : "********** USP-UNEXPECTED !!!!!!");
    }
    else if (TryChangeState(UspState::Error))
    {
        SPX_TRACE_ERROR("%s: site->Error() ... error='%s'", __FUNCTION__, error->GetDetails().c_str());
        InvokeOnSite([this, error](const SitePtr& p) {
            p->Error(this, error);

            auto categoryCode = static_cast<WebSocketError>(error->GetCategoryCode());

            if (error->GetRetryMode() == ISpxErrorInformation::RetryMode::NotAllowed
                && error->GetCancellationCode() == CancellationErrorCode::ConnectionFailure
                && (categoryCode == WebSocketError::CONNECTION_FAILURE || categoryCode == WebSocketError::WEBSOCKET_UPGRADE))
            {
                // we got a permanent failure while establishing the web socket connection, let's call the badly
                // named set format stop method to indicate to the session that the reco engine adapter is done
                p->AdapterCompletedSetFormatStop(this);
            }
        });
    }
    else
    {
       SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", 
        __FUNCTION__, 
        (void*)this, 
        static_cast<int>(m_audioState), 
        static_cast<int>(m_uspState));
    }
}

void CSpxUspRecoEngineAdapter::OnUserMessage(const USP::UserMsg& msg)
{
    SPX_DBG_TRACE_VERBOSE("Response: Usp User Message: %s, content-type=%s", msg.path.c_str(), msg.contentType.c_str());

    if (msg.path == "response")
    {
        if (m_endpointType == USP::EndpointType::Dialog)
        {
            // Dialog uses the response path for both Activity messages and TurnStatus messages
            std::string message{ reinterpret_cast<const char*>(msg.buffer), msg.size };
            SPX_DBG_TRACE_VERBOSE("USP Dialog User Message: response; message='%s'", message.c_str());
            auto responseMessage = ajv::json::Parse(message);

            auto messageType = responseMessage["messageType"].AsString();
            if (messageType.empty())
            {
                SPX_TRACE_ERROR(
                    "Unexpected dialog response message with no messageType; request_id='%s'",
                    msg.requestId.c_str());
            }
            else if (messageType == "Message")
            {
                if (!responseMessage["conversationId"].IsNull())
                {
                    /* Update conversation id */
                    m_dialogConversationId = responseMessage["conversationId"].AsString();
                    InvokeOnServiceIfAvailable<ISpxNamedProperties>(GetSite(), [&](ISpxNamedProperties& properties)
                        {
                            properties.Set(PropertyId::Conversation_Conversation_Id, m_dialogConversationId.c_str());
                        });
                }
                auto it = m_request_session_map.find(msg.requestId);
                if (it != m_request_session_map.end())
                {
                    auto& machine = it->second;
                    machine->ActivityReceived(message);
                    return;
                }
                SPX_TRACE_ERROR("Unexpected message; request_id='%s'", msg.requestId.c_str());
            }
            else if (messageType == "MessageStatus")
            {
                constexpr auto statusCodeKey = "statusCode";
                auto statusCode = HttpStatusCode::NOT_FOUND;

                if (!responseMessage.ValueAt(statusCodeKey).IsOk())
                {
                    SPX_DBG_TRACE_ERROR("Dialog MessageStatus does not contain statusCode");
                }
                else if (!responseMessage[statusCodeKey].IsNumber())
                {
                    SPX_DBG_TRACE_ERROR("Dialog MessageStatus is not a number");
                }
                else
                {
                    statusCode = (HttpStatusCode)responseMessage[statusCodeKey].AsInt();
                }

                if (statusCode == HttpStatusCode::NOT_FOUND)
                {
                    SPX_TRACE_ERROR("Dialog MessageStatus unable to resolve statusCode");
                }

                auto interactionId = responseMessage["interactionId"].AsString();
                auto interactionIdW = PAL::ToWString(interactionId);

                auto conversationId = responseMessage["conversationId"].AsString();
                InvokeOnSite([&](const SitePtr& p)
                    {
                        p->FireAdapterResult_TurnStatusReceived(interactionIdW, conversationId, (int)statusCode);
                    });
            }

            else
            {
                SPX_TRACE_ERROR(
                    "Unexpected dialog messageType '%s'; request_id='%s'",
                    messageType.c_str(),
                    msg.requestId.c_str());
            }
        }
        else
        {
            SPX_TRACE_ERROR("%s: (0x%8p) UNEXPECTED USP State transition ... (audioState/uspState=%d/%d)", 
                __FUNCTION__, 
                (void*)this, 
                static_cast<int>(m_audioState), 
                static_cast<int>(m_uspState));
        }
    }
}

void CSpxUspRecoEngineAdapter::OnConnected(const std::string& url)
{
    InvokeOnSite([&url](const SitePtr& p) { p->AdapterConnected(url); });
}

void CSpxUspRecoEngineAdapter::OnDisconnected(const std::shared_ptr<ISpxErrorInformation>& payload)
{
    InvokeOnSite([&payload](const SitePtr& p) { p->AdapterDisconnected(payload); });
}

uint8_t* CSpxUspRecoEngineAdapter::FormatBufferWriteBytes(uint8_t* buffer, const uint8_t* source, size_t bytes)
{
    std::memcpy(buffer, source, bytes);
    return buffer + bytes;
}

uint8_t* CSpxUspRecoEngineAdapter::FormatBufferWriteNumber(uint8_t* buffer, uint32_t number)
{
    std::memcpy(buffer, &number, sizeof(number));
    return buffer + sizeof(number);
}

uint8_t* CSpxUspRecoEngineAdapter::FormatBufferWriteChars(uint8_t* buffer, const char* psz, size_t cch)
{
    std::memcpy(buffer, psz, cch);
    return buffer + cch;
}

void CSpxUspRecoEngineAdapter::AddDgiJsonToContext(ajv::JsonBuilder& contextJson)
{
    if (GetOr<bool>("CARBON-INTERNAL-USP-NoDGI", false))
    {
        return;
    }

    auto dgiJson = contextJson["dgi"];

    // stream the json into a stream and then parse the string as json.
    std::string phraseListjson;

    for (auto& listenFor : GetSite()->GetListenForList())
    {
        if (listenFor.length() > 3 &&
            listenFor[0] == '{' && listenFor[listenFor.length() - 1] == '}' &&
            listenFor.find(':') != std::string::npos)
        {
            auto grammars = dgiJson["ReferenceGrammars"];
            auto formatted = listenFor.substr(1, listenFor.length() - 2);
            formatted = formatted.replace(formatted.find(':'), 1, "/");
            grammars[grammars.ValueCount()] = formatted;
        }
        else
        {
            auto group = dgiJson["Groups"][0];
            if (!group.IsOk())
            {
                group["Type"] = "Generic";

                if (auto maybeFactor = Get("SPEECH-WordLevelRecognitionFactor"))
                {
                    auto valueAsDouble = std::stod(maybeFactor.Get());
                    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, valueAsDouble < 0);
                    dgiJson["bias"] = valueAsDouble;
                }
                else if (auto maybeWeight = Get(g_phraseListWeightPropertyName))
                {
                    auto valueAsDouble = std::stod(maybeWeight.Get());
                    // Allowed range is already checked in c_api.
                    dgiJson["bias"] = valueAsDouble;
                }
            }

            if (phraseListjson.empty())
            {
                phraseListjson.append(R"({ "Items": [ )");
            }

            ajv::JsonBuilder builder;
            builder["Text"] = listenFor;

            phraseListjson.append(builder.AsJson()).append(",");
        }
    }

    if (!phraseListjson.empty())
    {
        // Remove the trailing ',' since AJV can't handle it.
        phraseListjson = phraseListjson.substr(0, phraseListjson.length() - 1);
        phraseListjson.append("]}");

        auto parsedPhraseList = ajv::json::Parse(phraseListjson);

        auto reader = parsedPhraseList.Reader();
        auto items = reader["Items"];
        
        auto group = dgiJson["Groups"][0];
        group["Items"] = items;
    }
}

// Adds details used for keyword recognition scenarios to an under-construction speech.context JSON document.
// This will only be added if at least one of the following is true:
//   * We have an in-flight on-device keyword that was spotted
//   * Keyword verification has been explicitly enabled for one or more keywords
//   * Keyword verification is implicitly enabled, e.g. by requesting keyword removal from results
void CSpxUspRecoEngineAdapter::AddKeywordDetectionJsonToContext(ajv::JsonBuilder& contextJson)
{
    auto verificationEnabled = GetOr<bool>(KeywordConfig_EnableKeywordVerification, false);
    auto shouldRemoveKeywordFromFinalResult = GetOr<bool>("SPEECH-RemoveKeyword", false);
    auto maybeKeywords = GetList(KEYWORDS_PROPERTY_NAME, ';');
    auto spottedKeywordResult = GetSite()->GetSpottedKeywordResult();

    if (!verificationEnabled && !shouldRemoveKeywordFromFinalResult
            && (!maybeKeywords || maybeKeywords.Get().empty()))
    {
        // No conditions for adding keyword information are met -- nothing to do
        return;
    }

    m_continueOnKeywordReject = !GetOr<bool>("SPEECH-CancelOnKeywordMissing", true);

    // The majority of the information needed is placed into a "keywordDetection" object. In priority order, this is
    // populated either from explicitly-provided keyword information (e.g. for KWV) or from a pending spotted keyword
    // result as raised by on-device KWS.
    ajv::JsonBuilder keywordJsonArray;

    if (maybeKeywords && !maybeKeywords.Get().empty())
    {
        // Provided keywords can optionally specify offsets and/or durations in parallel semicolon-delimited lists.
        auto specifiedKeywordOffsets = GetList("SPEECH-KeywordsToDetect-Offsets", ';');
        auto specifiedKeywordDurations = GetList("SPEECH-KeywordsToDetect-Durations", ';');

        auto intValueFrom = [&](Maybe<std::vector<std::string>>& maybeSource, size_t index)
        {
            if (maybeSource && maybeSource.Get().size() > index)
            {
                try
                {
                    return Maybe<int>(std::stoi(maybeSource.Get()[index]));
                }
                catch (std::invalid_argument&)
                {
                }
            }
            return Maybe<int>{};
        };

        for (auto i = 0; i < (int)maybeKeywords.Get().size(); i++)
        {
            auto element = keywordJsonArray[i];
            element["text"] = maybeKeywords.Get()[i];
            if (auto maybeOffset = intValueFrom(specifiedKeywordOffsets, i))
            {
                element["offset"] = maybeOffset.Get();
            }
            if (auto maybeDuration = intValueFrom(specifiedKeywordDurations, i))
            {
                element["duration"] = maybeDuration.Get();
            }
        }
    }
    else if (spottedKeywordResult)
    {
        auto element = keywordJsonArray[0];
        element["text"] = spottedKeywordResult->GetText();
        element["confidence"] = SpxQueryInterface<ISpxKeywordRecognitionResult>(spottedKeywordResult)->GetConfidence();
        element["startOffset"] = spottedKeywordResult->GetOffset();
        element["duration"] = spottedKeywordResult->GetDuration();
    }

    // If (for whatever reason) we ended up with no actual keyword content, there's no work to be done.
    if (keywordJsonArray.ValueCount() == 0)
    {
        return;
    }

    contextJson["invocationSource"] = "VoiceActivationWithKeyword";

    auto keywordDetection = contextJson["keywordDetection"][0];
    keywordDetection["type"] = "startTrigger";
    keywordDetection["clientDetectedKeywords"] = keywordJsonArray;
    keywordDetection["onReject"]["action"] = m_continueOnKeywordReject ? "Continue" : "EndOfTurn";

    if (shouldRemoveKeywordFromFinalResult)
    {
        contextJson["phraseDetection"]["enrichment"]["stripStartTriggerKeyword"] = shouldRemoveKeywordFromFinalResult;
    }
}

void CSpxUspRecoEngineAdapter::AddLeftRightJsonToContext(ajv::JsonBuilder& contextJson)
{
    auto leftContext = GetOr("DictationInsertionPointLeft", "");
    auto rightContext = GetOr("DictationInsertionPointRight", "");
    if (!leftContext.empty() || !rightContext.empty())
    {
        auto insertionJson = contextJson["dictation"]["insertionPoint"];
        insertionJson["left"] <<= leftContext;
        insertionJson["right"] <<= rightContext;
    }
}

void CSpxUspRecoEngineAdapter::AddTranslationJsonToContext(ajv::JsonBuilder& contextJson)
{
    bool targetLanguagesSpecified = false;

    auto translationLanguages = GetList(PropertyId::SpeechServiceConnection_TranslationToLanguages, ',');
    if (translationLanguages && !translationLanguages.Get().empty())
    {
        contextJson["translation"]["targetLanguages"] = translationLanguages.Get();
        targetLanguagesSpecified = true;
    }
    else // Check the supplied query parameters for translation languages
    {  
        auto translationLanguageQueryName = std::string( g_queryParameterPropertyNamePrefix).append(USP::endpoint::translationV1::toQueryParam);
        auto querySuppliedLanguages = GetOr(translationLanguageQueryName.substr(0, translationLanguageQueryName.length() - 1).c_str(), "");
        if (!querySuppliedLanguages.empty())
        {
            std::vector<std::string> languages;
            std::istringstream iss(querySuppliedLanguages);
            std::string language;
            while (std::getline(iss, language, ',')) {
                languages.push_back(language);
            }
            contextJson["translation"]["targetLanguages"] = languages;
            SPX_TRACE_WARNING("Translation languages were supplied via query parameters. Use top level configuration object properties instead.");
            targetLanguagesSpecified = true;
        }
    }

    if (!targetLanguagesSpecified)
    {
        SPX_TRACE_INFO("No target languages specified for translation.");
        return;
    }

    auto categoryId = GetOr(PropertyId::SpeechServiceConnection_TranslationCategoryId, "");

    if (!categoryId.empty()) {
        contextJson["translation"]["Category"] = categoryId;
        if (translationLanguages && !translationLanguages.Get().empty())
        {
            //for category id scenario, contextJson["translationcontext"] is depreciated, so we used contextJson["translation"]
            contextJson["translation"]["targetLanguages"] = translationLanguages.Get();
        }
    }

    contextJson["translation"]["output"]["includePassThroughResults"] = true;

    auto voice = GetOr(PropertyId::SpeechServiceConnection_TranslationVoice, "");
    if (!voice.empty())
    {
        contextJson["translation"]["onSuccess"]["action"] = "Synthesize";
        contextJson["translation"]["onPassthrough"]["action"] = "Synthesize";

        auto languages = contextJson["translation"]["targetLanguages"][0];
        std::vector<std::string> lang;
        lang.push_back(languages.AsString(""));

        contextJson["synthesis"]["synthesizedLanguages"] = lang;

        std::map<string,string> voices;
        voices.emplace( languages.AsString(""),voice );
        contextJson["synthesis"]["defaultVoices"] = voices;
    }

    auto phraseDetectionJson = contextJson["phraseDetection"];
    if (m_endpointType == USP::EndpointType::Translation)
    {
        auto phraseDetectionAction = "Translate";
        phraseDetectionJson["onSuccess"]["action"] = phraseDetectionAction;
        phraseDetectionJson["onInterim"]["action"] = phraseDetectionAction;

        if (phraseDetectionJson["language"].IsEmpty())
        {
            auto translationLanguageQueryName = std::string(g_queryParameterPropertyNamePrefix).append(USP::endpoint::translationV1::fromQueryParam);
            auto querySuppliedSourceLanguage = GetOr(translationLanguageQueryName.substr(0, translationLanguageQueryName.length() - 1).c_str(), "");
            if (!querySuppliedSourceLanguage.empty())
            {
                phraseDetectionJson["language"] = querySuppliedSourceLanguage;
                SPX_TRACE_WARNING("Translation  supplied via query parameters. Use top level configuration object properties instead.");
            }
        }
    }
}

void CSpxUspRecoEngineAdapter::AddLanguageIdJsonToContext(ajv::JsonBuilder& contextJson)
{
    if (!IsUnifiedEndpoint())
    {
        // Unsupported endpoint type for language id; nothing to do
        return;
    }

    auto maybeSourceLanguages = GetList(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, ',');
    if (!maybeSourceLanguages)
    {
        return;
    }

    bool languageDetectionOnly = m_endpointType == USP::EndpointType::StandaloneLanguageId;
    auto lidJson = contextJson["languageId"];
    lidJson["languages"] = maybeSourceLanguages.Get();
    lidJson["onSuccess"]["action"] = languageDetectionOnly ? "None" : "Recognize";
    lidJson["onUnknown"]["action"] = "None";
    lidJson["mode"] =
        m_languageIdMode == USP::LanguageIdMode::DetectContinuous ? g_languageIdModeDetectContinuous
        : m_languageIdMode == USP::LanguageIdMode::DetectSegments ? g_languageIdModeDetectSegments
        : g_languageIdModeDetectAtAudioStart;
    lidJson["Priority"] =
        m_languageIdPriority == USP::LanguageIdPriority::PrioritizeAccuracy ? g_languageIdPriorityPrioritizeAccuracy
        : g_languageIdPriorityPrioritizeLatency;

    std::vector<std::pair<std::string, std::string>> languageToEndpointIdPairs = GetPerLanguageSetting(maybeSourceLanguages.Get(), PropertyId::SpeechServiceConnection_EndpointId);

    if (languageDetectionOnly)
    {
        // When it is language detection only, we need to set the phraseDetection mode to None
        contextJson["phraseDetection"]["mode"] = "None";
    }
    else
    {
        auto phraseDetectionJson = contextJson["phraseDetection"];
        if (m_endpointType != USP::EndpointType::Translation)
        {
            auto phraseDetectionAction = "None"; // Translation action is already taken care of in AddTranslationJsonToContext
            phraseDetectionJson["onSuccess"]["action"] = phraseDetectionAction;
            phraseDetectionJson["onInterim"]["action"] = phraseDetectionAction;
        }

        if (!languageToEndpointIdPairs.empty())
        {
            int i = 0;
            auto customModels = phraseDetectionJson["customModels"];
            for (const auto& pair : languageToEndpointIdPairs)
            {
                auto model = customModels[i++];
                model["language"] = pair.first;
                model["endpoint"] = pair.second;
            }
        }

        const auto isSpeechEndpoint = m_endpointType == USP::EndpointType::Speech || m_endpointType == USP::EndpointType::ConversationTranscriptionServiceV2;
        auto phraseOutputJson = contextJson["phraseOutput"];
        phraseOutputJson["interimResults"]["resultType"] = isSpeechEndpoint ? "Auto" : "None";
        phraseOutputJson["phraseResults"]["resultType"] = isSpeechEndpoint ? "Always" : "None";

        if (m_endpointType == USP::EndpointType::Translation)
        {
            auto maybeToLanguages = GetList(PropertyId::SpeechServiceConnection_TranslationToLanguages, ',');
            const auto toLanguages = maybeToLanguages.GetOr(std::vector<std::string>{});
            auto voiceNamePairs = GetPerLanguageSetting(toLanguages, PropertyId::SpeechServiceConnection_TranslationVoice);
            auto voice = GetOr(PropertyId::SpeechServiceConnection_TranslationVoice, "");
            auto translationJson = contextJson["translation"];
            translationJson["targetLanguages"] = toLanguages;
            translationJson["output"]["interimResults"]["mode"] = "Always";
            if (!voiceNamePairs.empty()) {
                translationJson["onSuccess"]["action"] = "Synthesize";
                translationJson["onPassthrough"]["action"] = "Synthesize";
                for (const auto& pair : voiceNamePairs)
                {
                    contextJson["synthesis"]["defaultVoices"][pair.first] = pair.second;
                }
            }
            else if (voice.empty()) {
                translationJson["onSuccess"]["action"] = "None";
                translationJson["onPassthrough"]["action"] = "None";
            }
            else {
                translationJson["onSuccess"]["action"] = "Synthesize";
                translationJson["onPassthrough"]["action"] = "Synthesize";
            }
        }
    }
}

void CSpxUspRecoEngineAdapter::AddAudioJsonToContext(ajv::JsonBuilder& contextJson)
{
    contextJson["audio"]["streams"]["1"] = nullptr;

    auto maybeToken = Get("SPEECH-UspContinuationToken");
    auto maybeTag = Get("SPEECH-UspContinuationServiceTag");
    auto maybeOffset = Get(g_audioContinuationOffset);

    if (maybeToken || maybeTag || maybeOffset)
    {
        auto continuationJson = contextJson["continuation"];
        continuationJson["token"] <<= maybeToken.GetOr("");
        continuationJson["previousServiceTag"] <<= maybeTag.GetOr("");

        if (maybeOffset)
        {
            continuationJson["audio"]["streams"]["1"]["offset"] <<= maybeOffset.Get();
        }
    }

}

void CSpxUspRecoEngineAdapter::AddExtraPropertyJsonToContext(ajv::JsonBuilder& contextJson)
{
    for (const auto& it : GetParametersFromUser("speech.context"))
    {
        const auto& name = it.first;
        const auto& value = it.second;
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, name.empty() || value.empty());

        contextJson[name] = ajv::json::Parse(value);
        SPX_DBG_TRACE_VERBOSE("Set '%s' as '%s' in speech.context", name.c_str(), value.c_str());
    }
}

void CSpxUspRecoEngineAdapter::AddPronunciationJsonToContext(ajv::JsonBuilder& contextJson)
{
    auto maybeAssessmentParameters = Get(PropertyId::PronunciationAssessment_Params);
    if (!maybeAssessmentParameters || (m_endpointType != USP::EndpointType::Speech && m_endpointType != USP::EndpointType::ConversationTranscriptionServiceV2))
    {
        return;
    }

    // pronunciation assessment requires detailed format and word timings.
    // TODO: for now, format and word timings are needed to set in speech.context, which is different from current SDK
    // implementation (in connection url). Need to update the logic after service update.
    contextJson["phraseDetection"]["enrichment"]["pronunciationAssessment"]
        = ajv::json::Parse(maybeAssessmentParameters.Get());
    bool requestWordLevelTimestamps = false;
    if (auto maybeRequestWordLevelTimestamps = Get<bool>(PropertyId::SpeechServiceResponse_RequestWordLevelTimestamps))
    {
        if (maybeRequestWordLevelTimestamps.Get())
        {
            requestWordLevelTimestamps = true;
        }
    }
    auto enableWordTimings =
        requestWordLevelTimestamps
        || GetOr(PropertyId::PronunciationAssessment_Granularity, "")
            != PronunciationAssessment::Granularity[static_cast<size_t>(PronunciationAssessmentGranularity::FullText)];

    auto phraseOutputJson = contextJson["phraseOutput"];
    phraseOutputJson["format"] = "Detailed";

    auto optionsJson = phraseOutputJson["detailed"]["options"];

    if (enableWordTimings)
    {
        auto found = false;
        for (auto it = optionsJson.FirstValue(); !it.IsEnd(); it++)
        {
            found = it == "WordTimings";
        }
        if (!found)
        {
            optionsJson[optionsJson.ValueCount()] = "WordTimings";
        }
    }

    optionsJson[optionsJson.ValueCount()] = "PronunciationAssessment";
}

void CSpxUspRecoEngineAdapter::AddProfanityOptionJsonToContext(ajv::JsonBuilder& contextJson)
{
    auto maybeProfanityOption = Get(PropertyId::SpeechServiceResponse_ProfanityOption);
    if (!maybeProfanityOption || !IsUnifiedEndpoint())
    {
        return;
    }
    contextJson["phraseDetection"]["enrichment"]["profanity"] = maybeProfanityOption.Get();
}

void CSpxUspRecoEngineAdapter::AddModeJsonToContext(ajv::JsonBuilder& contextJson)
{
    if (!IsUnifiedEndpoint())
    {
        return;
    }

    if (m_handleAsSpeechV1Endpoint)
    {
        return;
    }

    auto phraseDetectionJson = contextJson["phraseDetection"];
    auto mode = GetRecoModeFromProperties();
    switch (mode)
    {
    case Microsoft::CognitiveServices::Speech::USP::RecognitionMode::Interactive:
        phraseDetectionJson["mode"] = g_recoModeInteractive;
        SPX_TRACE_INFO("Recognition mode INTERACTIVE.");
        break;
    case Microsoft::CognitiveServices::Speech::USP::RecognitionMode::Conversation:
        phraseDetectionJson["mode"] = g_recoModeConversation;
        SPX_TRACE_INFO("Recognition mode CONVERSATION.");
        break;
    case Microsoft::CognitiveServices::Speech::USP::RecognitionMode::Dictation:
        phraseDetectionJson["mode"] = g_recoModeDictation;
        SPX_TRACE_INFO("Recognition mode DICTATION.");
        break;
    default:
        SPX_TRACE_ERROR("Unexpected recognition mode %d.", static_cast<int>(mode));
        SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
        break;
    }
}

bool CSpxUspRecoEngineAdapter::IsUnifiedEndpoint()
{
    // Services that do not use the universal V2 endpoint:
    if (m_endpointType == USP::EndpointType::Dialog ||
        m_endpointType == USP::EndpointType::ConversationTranscriptionService ||
        m_endpointType == USP::EndpointType::DynamicConversationTranscriptionService ||
        m_endpointType == USP::EndpointType::SpeechSynthesis ||
        m_endpointType == USP::EndpointType::CustomVoice ||
        m_endpointType == USP::EndpointType::TranslationV1)
        // If you're adding to this list, consider why there is a new service that is using a different FrontEnd code base
        // that does not accept a "standard" speech.context message with the langauge information in it.
    {
        return false;
    }

    return true;
}

void CSpxUspRecoEngineAdapter::AddLanguageJsonToContext(ajv::JsonBuilder& contextJson)
{
    if(!IsUnifiedEndpoint())
    {
        return;
    }

    auto maybeSourceLanguages = GetList(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguages, ',');
    if (maybeSourceLanguages &&
        maybeSourceLanguages.Get().size() > 0 &&
        maybeSourceLanguages.Get()[0] != g_autoDetectSourceLang_OpenRange)
    {
        // Language detection used, don't add language unless it's an open range detection.
        return;
    }
    auto phraseDetectionJson = contextJson["phraseDetection"];

    // TranslationRecognizer historically did not have a default language. Putting this here for back compat.
    auto defaultLang = m_endpointType == USP::EndpointType::Translation ? "" : s_defaultRecognitionLanguage;

    auto language = GetOr<std::string>(PropertyId::SpeechServiceConnection_RecoLanguage, defaultLang);
    phraseDetectionJson["language"] = language;
}

void CSpxUspRecoEngineAdapter::AddInitialSilenceTimeoutJsonToContext(ajv::JsonBuilder& contextJson)
{
    int initialSilenceTimeout = 0;

    if (auto maybeInitialSilenceTimeoutString = Get<std::string>(PropertyId::SpeechServiceConnection_InitialSilenceTimeoutMs))
    {
        try
        {
            initialSilenceTimeout = std::stoi(maybeInitialSilenceTimeoutString.Get());
        }
        catch (std::invalid_argument&)
        {
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }

        if (initialSilenceTimeout < 0)
        {
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }
        auto phraseDetectionJson = contextJson["phraseDetection"];
        phraseDetectionJson["initialSilenceTimeout"] = initialSilenceTimeout;
    }
}

void CSpxUspRecoEngineAdapter::AddEndSilenceTimeoutJsonToContext(ajv::JsonBuilder& contextJson)
{
    int endSilenceTimeout = 0;

    if (auto maybeEndSilenceTimeoutString = Get<std::string>(PropertyId::SpeechServiceConnection_EndSilenceTimeoutMs))
    {
        try
        {
            endSilenceTimeout = std::stoi(maybeEndSilenceTimeoutString.Get());
        }
        catch (std::invalid_argument&)
        {
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }

        if (endSilenceTimeout < 0)
        {
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }
        auto phraseDetectionJson = contextJson["phraseDetection"];
        phraseDetectionJson["trailingSilenceTimeout"] = endSilenceTimeout;
    }
}

void CSpxUspRecoEngineAdapter::AddStableIntermediateThresholdJsonToContext(ajv::JsonBuilder& contextJson)
{
    int stableIntermediateThreshold = 0;

    if (auto maybeStableIntermediateThresholdString = Get<std::string>(PropertyId::SpeechServiceResponse_StablePartialResultThreshold))
    {
        try
        {
            stableIntermediateThreshold = std::stoi(maybeStableIntermediateThresholdString.Get());
        }
        catch (std::invalid_argument&)
        {
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }

        if (stableIntermediateThreshold < 0)
        {
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }
        auto phraseOutputJson = contextJson["phraseOutput"];
        phraseOutputJson["interimResults"]["stableThreshold"] = stableIntermediateThreshold;
    }
}

void CSpxUspRecoEngineAdapter::AddPostProcessingOptionsJsonToContext(ajv::JsonBuilder& contextJson)
{
    if (auto maybePostProcessingOptionString = Get<std::string>(PropertyId::SpeechServiceResponse_PostProcessingOption))
    {
        auto phraseDetectionJson = contextJson["phraseDetection"];
        if (auto maybeCurrentRecoMode = Get(PropertyId::SpeechServiceConnection_RecoMode))
        {
            string recoMode = "";
            if (maybeCurrentRecoMode.Get() == g_recoModeInteractive)
            {
                recoMode = "interactive";
            }
            else if (maybeCurrentRecoMode.Get() == g_recoModeConversation)
            {
                recoMode = "conversation";
            }
            else if (maybeCurrentRecoMode.Get() == g_recoModeDictation)
            {
                recoMode = "dictation";
            }
            else
            {
                // The mode should always have been resolved to a known value by the time this function is called.
                SPX_THROW_HR(SPXERR_INVALID_STATE);
            }

            string lowerCasePostProcessingOption = PAL::StringUtils::ToLower(maybePostProcessingOptionString.Get());
            if (lowerCasePostProcessingOption == "truetext")
            {
                // Client-side expansion for "truetext" - set specific properties
                phraseDetectionJson["enrichment"][recoMode]["punctuationMode"] = "Implicit";
                phraseDetectionJson["enrichment"][recoMode]["disfluencyMode"] = "Removed";
                phraseDetectionJson["enrichment"][recoMode]["intermediatePunctuationMode"] = "Implicit";
                phraseDetectionJson["enrichment"][recoMode]["intermediatedisfluencymode"] = "Removed";
            }
            else
            {
                // Service-side handling for all other strings
                // Pass the developer-provided value directly to the service.
                // Input validation is handled on the service side.
                phraseDetectionJson["enrichment"][recoMode]["postprocessingoption"] = maybePostProcessingOptionString.Get();
            }
        }
        else
        {
            // The mode should always have been resolved by the time this function is called.
            SPX_THROW_HR(SPXERR_INVALID_STATE);
        }
    }
}

void CSpxUspRecoEngineAdapter::AddSegmentationJsonToContext(ajv::JsonBuilder& contextJson)
{
    if (auto maybeSegmentationStrategyStr = Get<string>(PropertyId::Speech_SegmentationStrategy))
    {
        // Segmentation strategy.
        auto segmentationStrategyStr = maybeSegmentationStrategyStr.Get();
        auto segmentationStrategy = SegmentationStrategy::Default;
        if (EnumHelpers::TryParse<SegmentationStrategy>(segmentationStrategyStr.c_str(), segmentationStrategy)) {
            auto recoMode = SetRecoMode(contextJson);
            auto segmentationJson = contextJson["phraseDetection"][recoMode]["segmentation"];
            switch (segmentationStrategy)
            {
                case SegmentationStrategy::Default:
                    break;
                case SegmentationStrategy::Time:
                    segmentationJson["mode"] = g_segmentationModeCustom;
                    break;
                case SegmentationStrategy::Semantic:
                    segmentationJson["mode"] = g_segmentationModeSemantic;
                    break;
            }
        }
    }

    // If Speech_SegmentationSilenceTimeoutMs or Speech_SegmentationMaximumTimeMs is set while using any strategy other than "Semantic", switch to the "Custom" strategy.
    if (auto maybeSegmentationTimeout = Get<int>(PropertyId::Speech_SegmentationSilenceTimeoutMs))
    {
        auto recoMode = SetRecoMode(contextJson);
        auto segmentationJson = contextJson["phraseDetection"][recoMode]["segmentation"];
        segmentationJson["segmentationSilenceTimeoutMs"] = maybeSegmentationTimeout.Get();
        if (segmentationJson["mode"].IsNull() || segmentationJson["mode"].IsEmpty()) {
            segmentationJson["mode"] = g_segmentationModeCustom;
        }
    }

    if (auto maybeSegmentationMaximumTime = Get<int>(PropertyId::Speech_SegmentationMaximumTimeMs))
    {
        auto recoMode = SetRecoMode(contextJson);
        auto segmentationJson = contextJson["phraseDetection"][recoMode]["segmentation"];
        segmentationJson["segmentationForcedTimeoutMs"] = maybeSegmentationMaximumTime.Get();
        if (segmentationJson["mode"].IsNull() || segmentationJson["mode"].IsEmpty()) {
            segmentationJson["mode"] = g_segmentationModeCustom;
        }
    }
}

void CSpxUspRecoEngineAdapter::AddOutputDetailJsonToContext(ajv::JsonBuilder& contextJson)
{
    bool enableSnr = Get(PropertyId::SpeechServiceResponse_RequestSnr);
    if (enableSnr && m_endpointType == USP::EndpointType::Speech)
    {
        auto phraseOutputJson = contextJson["phraseOutput"];
        auto optionsJson = phraseOutputJson["detailed"]["options"];
        auto found = false;
        for (auto it = optionsJson.FirstValue(); !it.IsEnd(); it++)
        {
            found = it == "SNR";
        }
        if (!found)
        {
            optionsJson[optionsJson.ValueCount()] = "SNR";
        }
    }
}

void CSpxUspRecoEngineAdapter::AddOutputDetailLevelJsonToContext(ajv::JsonBuilder& contextJson)
{
    if (!IsUnifiedEndpoint())
    {
        return;
    }

    bool useDetailedFormat = false;
    if (auto maybeUseDetailed = Get<bool>(PropertyId::SpeechServiceResponse_RequestDetailedResultTrueFalse))
    {
        if (maybeUseDetailed.Get())
        {
            useDetailedFormat = true;
        }
    }
    else if (auto maybeOutputFormat = Get(PropertyId::SpeechServiceResponse_OutputFormatOption))
    {
        if (PAL::StringUtils::ToLower(maybeOutputFormat.Get()) == "detailed")
        {
            useDetailedFormat = true;
        }
    }

    bool requestWordLevelTimestamps = false;
    if (useDetailedFormat)
    {
        // By default, detailed format uses word level timestamps, unless explicitly configured off
        requestWordLevelTimestamps = true;
    }
    if (auto maybeRequestWordLevelTimestamps = Get<bool>(PropertyId::SpeechServiceResponse_RequestWordLevelTimestamps))
    {
        if (maybeRequestWordLevelTimestamps.Get())
        {
            requestWordLevelTimestamps = true;
            useDetailedFormat = true;
        }
        else
        {
            requestWordLevelTimestamps = false;
        }
    }
    
    SPX_TRACE_INFO("DetailedFormat: %d, WordLevelTimestamps: %d", useDetailedFormat, requestWordLevelTimestamps);

    if (useDetailedFormat)
    {
        auto phraseOutputJson = contextJson["phraseOutput"];
        phraseOutputJson["format"] = "Detailed";
        auto optionsJson = phraseOutputJson["detailed"]["options"];
        auto foundWordTimings = false;
        auto foundWordConfidence = false;
        for (auto it = optionsJson.FirstValue(); !it.IsEnd(); it++)
        {
            foundWordTimings = it == "WordTimings";
            foundWordConfidence = it == "WordConfidence";
        }
        if (!foundWordTimings && requestWordLevelTimestamps)
        {
            optionsJson[optionsJson.ValueCount()] = "WordTimings";
        }
        if (!foundWordConfidence && requestWordLevelTimestamps)
        {
            optionsJson[optionsJson.ValueCount()] = "WordConfidence";
        }
    }
}

void CSpxUspRecoEngineAdapter::AddConversationTranscriptionJsonToContext(ajv::JsonBuilder& contextJson)
{
    auto isConversationTranscription = GetOr<bool>(g_isConversationTranscriber_V2, false);
    if (isConversationTranscription)
    {
        contextJson["phraseDetection"]["mode"] = "Conversation";
        auto diarizationJson = contextJson["phraseDetection"]["speakerDiarization"];
        diarizationJson["mode"] = "Anonymous";
        auto sessionId = GetOr(PropertyId::Speech_SessionId, "");
        diarizationJson["audioSessionId"] = sessionId;
        uint64_t audioOffsetInTicks = GetOr<uint64_t>(g_audioContinuationOffset, 0);
        uint64_t audioOffsetInMS = 0;
        if (audioOffsetInTicks > 0)
        {
            audioOffsetInMS = audioOffsetInTicks / 10000;
        }
        diarizationJson["audioOffsetMS"] = audioOffsetInMS;
        diarizationJson["identityProvider"] = nullptr;

        if (GetOr<bool>("SpeechServiceResponse_DiarizeIntermediateResults", false))
        {
            diarizationJson["diarizeIntermediates"] = "True";
        }
    }
}

void CSpxUspRecoEngineAdapter::AddSpeechStartEventSensitivityJsonToContext(ajv::JsonBuilder& contextJson)
{
    auto maybeSpeechStartEventSensitivity = Get(PropertyId::Speech_StartEventSensitivity);
    if (!maybeSpeechStartEventSensitivity)
    {
        return;
    }
    contextJson["phraseDetection"]["voiceOnsetSensitivity"] = maybeSpeechStartEventSensitivity.Get();
}

std::string CSpxUspRecoEngineAdapter::SetRecoMode(ajv::JsonBuilder& contextJson)
{
    auto phraseDetectionJson = contextJson["phraseDetection"];
    // we *must* set a mode to use custom segmentation -- default to interactive if we can't find it
    auto mode = GetOr<std::string>("SPEECH-RecoMode", g_recoModeInteractive);
    phraseDetectionJson["mode"] = mode;
    return mode;
}

std::string CSpxUspRecoEngineAdapter::GetSpeechContextJson()
{
    ajv::JsonBuilder contextJson;

    AddModeJsonToContext(contextJson);
    AddLanguageJsonToContext(contextJson);
    AddDgiJsonToContext(contextJson);
    AddKeywordDetectionJsonToContext(contextJson);
    AddLeftRightJsonToContext(contextJson);
    AddTranslationJsonToContext(contextJson);
    AddLanguageIdJsonToContext(contextJson);
    AddAudioJsonToContext(contextJson);
    AddExtraPropertyJsonToContext(contextJson);
    AddPronunciationJsonToContext(contextJson);
    AddProfanityOptionJsonToContext(contextJson);
    AddInitialSilenceTimeoutJsonToContext(contextJson);
    AddEndSilenceTimeoutJsonToContext(contextJson);
    AddStableIntermediateThresholdJsonToContext(contextJson);
    AddPostProcessingOptionsJsonToContext(contextJson);
    AddSegmentationJsonToContext(contextJson);
    AddOutputDetailJsonToContext(contextJson);
    AddOutputDetailLevelJsonToContext(contextJson);
    AddConversationTranscriptionJsonToContext(contextJson);
    AddSpeechStartEventSensitivityJsonToContext(contextJson);

    return contextJson.AsJson();
}

ResultReason CSpxUspRecoEngineAdapter::ToReason(RecognitionStatus uspRecognitionStatus)
{
    switch (uspRecognitionStatus)
    {
    case RecognitionStatus::Success:
    case RecognitionStatus::EndOfDictation:
        return ResultReason::RecognizedSpeech;

    case RecognitionStatus::NoMatch:
    case RecognitionStatus::InitialSilenceTimeout:
    case RecognitionStatus::InitialBabbleTimeout:
        return ResultReason::NoMatch;

    case RecognitionStatus::Error:
    case RecognitionStatus::TooManyRequests:
    case RecognitionStatus::BadRequest:
    case RecognitionStatus::Forbidden:
    case RecognitionStatus::ServiceUnavailable:
    case RecognitionStatus::InvalidMessage:
        return ResultReason::Canceled;

    default:
        SPX_TRACE_ERROR("Unexpected recognition status %d when converting to ResultReason.", static_cast<int>(uspRecognitionStatus));
        SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
    }
}

CancellationReason CSpxUspRecoEngineAdapter::ToCancellationReason(RecognitionStatus uspRecognitionStatus)
{
    switch (uspRecognitionStatus)
    {
    case RecognitionStatus::Success:
    case RecognitionStatus::NoMatch:
    case RecognitionStatus::InitialSilenceTimeout:
    case RecognitionStatus::InitialBabbleTimeout:
    case RecognitionStatus::EndOfDictation:
        return REASON_CANCELED_NONE;

    case RecognitionStatus::Error:
    case RecognitionStatus::TooManyRequests:
    case RecognitionStatus::BadRequest:
    case RecognitionStatus::Forbidden:
    case RecognitionStatus::ServiceUnavailable:
    case RecognitionStatus::InvalidMessage:
        return CancellationReason::Error;

    default:
        SPX_TRACE_ERROR("Unexpected recognition status %d when converting to CancellationReason.", static_cast<int>(uspRecognitionStatus));
        SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
    }
}

NoMatchReason CSpxUspRecoEngineAdapter::ToNoMatchReason(RecognitionStatus uspRecognitionStatus)
{
    switch (uspRecognitionStatus)
    {
    case RecognitionStatus::Success:
    case RecognitionStatus::Error:
    case RecognitionStatus::TooManyRequests:
    case RecognitionStatus::BadRequest:
    case RecognitionStatus::Forbidden:
    case RecognitionStatus::ServiceUnavailable:
    case RecognitionStatus::InvalidMessage:
    case RecognitionStatus::EndOfDictation:
        return NO_MATCH_REASON_NONE;

    case RecognitionStatus::NoMatch:
        return NoMatchReason::NotRecognized;

    case RecognitionStatus::InitialSilenceTimeout:
        return NoMatchReason::InitialSilenceTimeout;

    case RecognitionStatus::InitialBabbleTimeout:
        return NoMatchReason::InitialBabbleTimeout;

    default:
        SPX_TRACE_ERROR("Unexpected recognition status %d when converting to NoMatchReason.", static_cast<int>(uspRecognitionStatus));
        SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
    }
}

void CSpxUspRecoEngineAdapter::FireActivityResult(std::string activity, std::shared_ptr<ISpxAudioOutput> audio)
{
    SPX_DBG_TRACE_SCOPE("FireActivityAndAudioResult: Creating Result", "FireActivityAndAudioResult: GetSite()->FireAdapterResult_ActivityAudioReceived()  complete!");

    InvokeOnSite([activity{std::move(activity)}, audio](const SitePtr& site)
    {
        site->FireAdapterResult_ActivityReceived(std::move(activity), audio);
    });
}

void CSpxUspRecoEngineAdapter::FireFinalResultLater(const USP::SpeechPhraseMsg& message)
{
    m_finalResultMessageToFireLater = message;
}

void CSpxUspRecoEngineAdapter::FireFinalResultNow(const USP::SpeechPhraseMsg& message)
{
    SPX_DBG_TRACE_SCOPE("FireFinalResultNow: Creating Result", "FireFinalResultNow: GetSite()->FireAdapterResult_FinalResult()  complete!");

    InvokeOnSite([&](const SitePtr& site)
    {
        // Create the result
        auto factory = SpxQueryService<ISpxRecoResultFactory>(site);

        auto cancellationReason = ToCancellationReason(message.recognitionStatus);
        if (cancellationReason != REASON_CANCELED_NONE)
        {
            // The status above should have been treated as error event, so this should not happen here.
            SPX_TRACE_ERROR("Unexpected recognition status %d.", static_cast<int>(message.recognitionStatus));
            SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
        }

        auto result = factory->CreateFinalResult(ToReason(message.recognitionStatus), ToNoMatchReason(message.recognitionStatus), message.displayText.c_str(), message.offset, message.duration, message.phraseId.c_str(), message.speaker.c_str());
        auto namedProperties = SpxQueryInterface<ISpxNamedProperties>(result);
        if (m_ignoreTelemetry)
        {
            auto latencyMS = std::chrono::duration_cast<std::chrono::milliseconds>(m_offlineTimestamp - std::chrono::high_resolution_clock::now());
            result->SetLatency(latencyMS.count());
            namedProperties->SetStringValue("CARBON-INTERNAL-Silence_Telemetry", "true");
        }

        namedProperties->Set(PropertyId::SpeechServiceResponse_JsonResult, message.json.c_str());
        namedProperties->Set(PropertyId::SpeechServiceResponse_RecognitionBackend, "online");

        if (!m_currentRequestId.empty())
        {
            namedProperties->Set(PropertyId::SpeechServiceResponse_RequestId, m_currentRequestId.c_str());
        }

        if (!message.speaker.empty())
        {
            CreateConversationResult(result, message.speaker, message.utteranceId);
        }

        if (!message.language.empty())
        {
            namedProperties->Set(PropertyId::SpeechServiceConnection_AutoDetectSourceLanguageResult, message.language.c_str());
        }

        site->FireAdapterResult_FinalResult(message.offset, result);
    });
}

bool CSpxUspRecoEngineAdapter::TryChangeState(AudioState fromAudioState, UspState fromUspState, AudioState toAudioState, UspState toUspState)
{
    if (fromAudioState == m_audioState &&       // are we in correct audio state, and ...
        fromUspState == m_uspState &&           // are we in correct usp state? ... if so great! but ...
        ((fromUspState != UspState::Error &&        // don't allow transit from Error
          fromUspState != UspState::Zombie &&       // don't allow transit from Zombie
          fromUspState != UspState::Terminating) || // don't allow transit from Terminating ...
         ((fromUspState == toUspState) ||           // unless we're staying in that same usp state
          (fromUspState == UspState::Error &&           // or we're going from Error to Terminating
           toUspState == UspState::Terminating) ||
          (fromUspState == UspState::Terminating &&     // or we're going from Terminating to Zombie
           toUspState == UspState::Zombie))))
    {
        SPX_DBG_TRACE_VERBOSE("%s; audioState/uspState: %d/%d => %d/%d %s%s%s%s%s", 
            __FUNCTION__,
            static_cast<int>(fromAudioState),
            static_cast<int>(fromUspState),
            static_cast<int>(toAudioState), 
            static_cast<int>(toUspState),
            toUspState == UspState::Error ? "USP-ERRORERROR" : "",
            (fromAudioState == AudioState::Idle && fromUspState == UspState::Idle &&
             toAudioState == AudioState::Ready && toUspState == UspState::Idle) ? "USP-START" : "",
            (toAudioState == AudioState::Idle && toUspState == UspState::Idle) ? "USP-DONE" : "",
            toUspState == UspState::Terminating ? "USP-TERMINATING" : "",
            toUspState == UspState::Zombie ? "USP-ZOMBIE" : ""
            );

        m_audioState = toAudioState;
        m_uspState = toUspState;
        return true;
    }

    return false;
}

SPXHR CSpxUspRecoEngineAdapter::PrepareCompressionCodec(
                                    const SPXWAVEFORMATEX* format,
                                    ISpxInternalAudioCodecAdapter::SPXCompressedDataCallback dataCallback)
{
    m_compressionCodec.reset();

    SPX_DBG_TRACE_VERBOSE("%s: Prepare compression codec.", __FUNCTION__);

    std::string codecModule = GetStringValue("SPEECH-Compression-Codec-Module");
    if (codecModule.length() == 0) return SPXERR_NOT_FOUND; // don't spew into log... not fatal...

    // it should come as empty or audio/silk for now
    std::string encodingFormat = GetStringValue("SPEECH-Compression-EncodingFormat");

    auto codecAdapter = SpxCreateObjectWithSite<ISpxInternalAudioCodecAdapter>("CSpxInternalAudioCodecAdapter", this);
    SPX_RETURN_ON_FAIL(codecAdapter->Load(codecModule, encodingFormat, dataCallback));

    codecAdapter->InitCodec(format);
    m_compressionCodec = codecAdapter;

    return SPX_NOERROR;
}

void CSpxUspRecoEngineAdapter::HandleCompressedAudioData(const uint8_t* outData, size_t nBytesOut)
{
    // Handle null / 0 bytes of data on ending the stream
    if (outData == nullptr || nBytesOut == 0)
    {
        // NOOP
        return;
    }

    // Gets the compressed audio data
    auto data = SpxAllocSharedAudioBuffer(nBytesOut);
    memcpy(data.get(), outData, nBytesOut);

    // TODO: Track audio timestamps as well
    auto dataChunkPtr = std::make_shared<DataChunk>(data, (uint32_t)nBytesOut);

    if (!m_audioFormatSent)
    {
        dataChunkPtr->contentType = m_compressionCodec->GetContentType();
        m_audioFormatSent = true;
    }

    UspWriteActual(dataChunkPtr);
}

void CSpxUspRecoEngineAdapter::PrepareFirstAudioReadyState(const SPXWAVEFORMATEX* format)
{
    SPX_DBG_ASSERT(IsState(AudioState::Ready, UspState::Idle));

    auto sizeOfFormat = sizeof(SPXWAVEFORMATEX) + format->cbSize;
    m_format = SpxAllocWAVEFORMATEX(sizeOfFormat);
    memcpy(m_format.get(), format, sizeOfFormat);

    m_resetUspAfterAudioByteCount = m_format->nAvgBytesPerSec * m_resetUspAfterAudioSeconds;
    if (ShouldResetBeforeFirstAudio())
    {
        ResetBeforeFirstAudio();
    }

    auto codecInitResult = PrepareCompressionCodec(format,
        [this](const uint8_t* outData, size_t nBytesOut)
        {
            HandleCompressedAudioData(outData, nBytesOut);
        });

    SPX_DBG_TRACE_VERBOSE_IF(SPX_FAILED(codecInitResult), "%s: (0x%8p)->PrepareCompressionCodec() result: %8lx. Sending the audio uncompressed", __FUNCTION__, (void*)this, (unsigned long)codecInitResult);

    PrepareAudioReadyState();
}

void CSpxUspRecoEngineAdapter::PrepareAudioReadyState()
{
    if (!IsState(AudioState::Ready, UspState::Idle))
    {
        SPX_TRACE_ERROR("wrong state in PrepareAudioReadyState current audio state %d, usp state %d", static_cast<int>(m_audioState), static_cast<int>(m_uspState));
    }

    EnsureUspInit();
}

void CSpxUspRecoEngineAdapter::SendPreAudioMessages()
{
    SPX_DBG_ASSERT(IsState(AudioState::Sending));

    UspSendSpeechContext();
    UspSendSpeechAgentContext();
    UspSendSpeechEvent();
}

void CSpxUspRecoEngineAdapter::PrepareUspAudioStream()
{
    SendPreAudioMessages();
    m_audioFormatSent = false;

    m_saveToWavCurrentTurn.OpenWav("usp-turn-audio-", m_format.get());

    // OPUS audio is handled via "passthrough" and explicit audio information via a format header should not be sent
    // on the wire. For everything else, send that header information so the service processes the audio appropriately.
    auto isPassthroughAudio = m_format.get()->wFormatTag == WAVE_FORMAT_OPUS;
    isPassthroughAudio |= m_format.get()->wFormatTag == WAVE_FORMAT_OGG_OPUS;

    if (!isPassthroughAudio)
    {
        ProcessAudioFormat(m_format.get());
    }
}

bool CSpxUspRecoEngineAdapter::ShouldResetAfterError()
{
    return m_allowUspResetAfterError && IsState(UspState::Idle);
}

void CSpxUspRecoEngineAdapter::ResetAfterError()
{
    SPX_DBG_ASSERT(ShouldResetAfterError());

    // Let's terminate our current usp connection and sever our callbacks
    UspTerminate();

    // Let's get ready for more audio!!!
    PrepareAudioReadyState();
}

bool CSpxUspRecoEngineAdapter::ShouldResetAfterTurnStopped()
{
    return m_allowUspResetAfterAudioByteCount &&
        (m_uspAudioByteCount > m_resetUspAfterAudioByteCount) &&
        (m_format != nullptr) &&
        (m_format.get()->wFormatTag == WAVE_FORMAT_PCM);
}

void CSpxUspRecoEngineAdapter::ResetAfterTurnStopped()
{
    SPX_DBG_ASSERT(ShouldResetAfterTurnStopped());
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p ... USP-RESET", __FUNCTION__, (void*)this);

    // Let's terminate our current usp connection and sever our callbacks
    UspTerminate();

    // If we're in the ready/idle state, be sure and re-initialize the usp
    if (IsState(AudioState::Ready, UspState::Idle))
    {
        UspInitialize();
    }
}

bool CSpxUspRecoEngineAdapter::ShouldResetBeforeFirstAudio()
{
    return m_allowUspResetAfterTime && std::chrono::system_clock::now() > m_uspResetTime;
}

void CSpxUspRecoEngineAdapter::ResetBeforeFirstAudio()
{
    SPX_DBG_ASSERT(ShouldResetBeforeFirstAudio() && IsState(AudioState::Ready, UspState::Idle));
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p ... USP-RESET", __FUNCTION__, (void*)this);

    // Let's terminate our current usp connection and sever our callbacks
    UspTerminate();
}

std::vector<std::pair<std::string, std::string>> CSpxUspRecoEngineAdapter::GetPerLanguageSetting(
    const vector<string>& languages,
    PropertyId propertyId)
{
    auto basePropertyName = GetPropertyName(propertyId);

    std::vector<std::pair<std::string, std::string>> results;

    for (const string& language : languages)
    {
        string perLanguageSettingProperty = language + basePropertyName;
        auto maybePropertyValue = Get(perLanguageSettingProperty.c_str());
        if (maybePropertyValue && !maybePropertyValue.Get().empty())
        {
            results.push_back(std::make_pair(language, maybePropertyValue.Get()));
        }
    }

    return results;
}

CSpxStringMap CSpxUspRecoEngineAdapter::GetParametersFromUser(std::string&& path)
{
    auto getter = SpxQueryService<ISpxGetUspMessageParamsFromUser>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, getter == nullptr);

    return getter->GetParametersFromUser(std::move(path));
}

CSpxStringMap CSpxUspRecoEngineAdapter::GetParametersFromRecognizer(std::string&& path)
{
    auto getter = SpxQueryService<ISpxGetUspMessageParamsFromRecognizer>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, getter == nullptr);

    return getter->GetParametersFromRecognizer(std::move(path));
}

void CSpxUspRecoEngineAdapter::OnToken(const std::string token)
{
    SetStringValue("SPEECH-UspContinuationToken", token.c_str());
}

void CSpxUspRecoEngineAdapter::OnAcknowledgedAudio(uint64_t offset)
{
    SPX_DBG_TRACE_VERBOSE("%s: Before adding starting offset=%" PRIu64 "", __FUNCTION__, offset);

    auto audioContinuationOffset = offset + m_startingOffset;
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p Service acknowledging to offset %" PRIu64 " (100ns).", __FUNCTION__, (void*)this, audioContinuationOffset);

    if (m_useMultiChannelProcessing)
    {
        uint64_t currentAudioContinuationOffset = GetOr<uint64_t>(g_audioContinuationOffset, 0);
        if (currentAudioContinuationOffset >= audioContinuationOffset)
        {
            SPX_DBG_TRACE_VERBOSE(
                "%s: Not updating g_audioContinuationOffset, current %" PRIu64 " >= new %" PRIu64 " (m_useMultiChannelProcessing %d)",
                __FUNCTION__, currentAudioContinuationOffset, audioContinuationOffset, m_useMultiChannelProcessing
            );
            return;
        }
    }

    SetStringValue(g_audioContinuationOffset, std::to_string(audioContinuationOffset).c_str());

    if (auto replayer = SpxQueryService<ISpxAudioReplayer>(GetSite()))
    {
        bool isKeywordAndSpeech = GetOr<bool>(g_keyword_KeywordAndSpeech, false);
        if (isKeywordAndSpeech == false)
        {
            offset = audioContinuationOffset;
        }

        SPX_DBG_TRACE_VERBOSE("%s: ShrinkReplayBuffer=%" PRIu64 "", __FUNCTION__, offset);
        replayer->ShrinkReplayBuffer(offset);
    }
}

std::shared_ptr<ISpxNamedProperties> CSpxUspRecoEngineAdapter::GetParentProperties() const
{
    return SpxQueryService<ISpxNamedProperties>(GetSite());
}

void CSpxUspRecoEngineAdapter::SetStringValue(const char* name, const char* value)
{
    // Set string values in the session as there is a dependency on them being available to the Recognizer object, and that's where it is.
    auto session = SpxQueryService<ISpxSession>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, session == nullptr);

    SpxQueryInterface<ISpxNamedProperties>(session)->SetStringValue(name, value);
}

void CSpxUspRecoEngineAdapter::SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size)
{
    // Set string values in the session as there is a dependency on them being available to the Recognizer object, and that's where it is.
    auto session = SpxQueryService<ISpxSession>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, session == nullptr);

    SpxQueryInterface<ISpxNamedProperties>(session)->SetBinaryValue(name, value, size);
}

// Static helper: use for aliasing when more than one property ID should correspond to a single query string parameter.
// Given an input property ID that's to be reflected in a query string, returns a list of all property IDs that should
// be considered, in order, as a data source for a query string value.
const std::vector<PropertyId> CSpxUspRecoEngineAdapter::GetPropertyIdAliasesForQueryStrings(
    const PropertyId sourcePropertyId)
{
    switch (sourcePropertyId)
    {
        // A duplicate property ID was created for Dialog scenarios (Conversation_Initial_Silence_Timeout) -- treat
        // this as an interchangeable alias to the broader corresponding property.
        case PropertyId::SpeechServiceConnection_InitialSilenceTimeoutMs:
        case PropertyId::Conversation_Initial_Silence_Timeout:
            return
            {
                PropertyId::SpeechServiceConnection_InitialSilenceTimeoutMs,
                PropertyId::Conversation_Initial_Silence_Timeout
            };
        // For any property ID without special treatment, map the ID to a single-element, self-only collection.
        default: return { sourcePropertyId };
    }
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
