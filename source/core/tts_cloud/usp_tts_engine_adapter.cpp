//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// usp_tts_engine_adapter.cpp: Implementation definitions for CSpxUspTtsEngineAdapter C++ class
//

#include "stdafx.h"
#include <ajv.h>
#include <usp_text_message.h>
#include <chrono>
#include <thread>
#include "synthesis_helper.h"
#include "usp_tts_engine_adapter.h"
#include "create_object_helpers.h"
#include "guid_utils.h"
#include "handle_table.h"
#include "service_helpers.h"
#include "property_bag_impl.h"
#include "spx_build_information.h"
#include "time_utils.h"
#include "thread_service.h"
#include "error_info.h"
#include "http_utils.h"
#include "boundary_type_enum_helpers.h"
#include "../synthesizer.h"

#define SPX_DBG_TRACE_USP_TTS 1

#define MAX_RETRY 1

#define WAIT_FOR_FIRST_CHUNK_TIMEOUT 40000  // in milliseconds
#define RECEIVE_ALL_CHUNKS_TIMEOUT 300000   // in milliseconds
#ifdef _DEBUG
#define DEBUG_WAIT_INTERVAL 100         // in milliseconds
#endif

using namespace std;

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template<>
const char* EnumHelpers::ToString<UspState>(UspState state)
{
    switch (state)
    {
    case UspState::Error: return "Error";
    case UspState::Idle: return "Idle";
    case UspState::Connecting: return "Connecting";
    case UspState::Sending: return "Sending";
    case UspState::TurnStarted: return "TurnStarted";
    case UspState::ReceivingData: return "ReceivingData";
    }

    return nullptr;
}

CSpxUspTtsEngineAdapter::CSpxUspTtsEngineAdapter()
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_USP_TTS, __FUNCTION__);
}

CSpxUspTtsEngineAdapter::~CSpxUspTtsEngineAdapter()
{
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);
    SPX_DBG_ASSERT(m_uspCallbacks == nullptr);
    SPX_DBG_ASSERT(m_uspConnection == nullptr);
}

void CSpxUspTtsEngineAdapter::Init()
{
    auto threadService = SpxQueryService<ISpxThreadService>(GetSite());
    SPX_THROW_HR_IF(SPXERR_INVALID_STATE, threadService == nullptr);

    m_threadService = threadService;

    // Get proxy setting
    GetProxySetting();
}

void CSpxUspTtsEngineAdapter::Term()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    Disconnect();
}

std::shared_ptr<ISpxSynthesisResult> CSpxUspTtsEngineAdapter::Speak(const std::string& text, bool isSsml, const std::string& requestId, bool retry)
{
    auto request = SpxCreateObjectWithSite<ISpxSynthesisRequest>("CSpxSynthesisRequest", SpxSiteFromThis(this));
    request->Init(isSsml ? SynthesisRequestInputType::SSML : SynthesisRequestInputType::Text, text, requestId);
    auto requestReader = SpxQueryInterface<ISpxSynthesisRequestReader>(request);
    return Speak(requestReader, retry);
}

std::shared_ptr<ISpxSynthesisResult> CSpxUspTtsEngineAdapter::Speak(std::shared_ptr<ISpxSynthesisRequestReader> request, bool retry)
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_USP_TTS, __FUNCTION__);
    SPX_DBG_ASSERT(UspState::Idle == m_uspState || UspState::Error == m_uspState);

    GetSite()->SetAdapterFormat(this, CSpxSynthesisHelper::GetSpeechSynthesisOutputFormatFromString(m_requestFormatStr));
    m_originalRequestId = request->GetRequestId();
    auto turnRequestId = m_originalRequestId;

    m_shouldStop = false;

    std::shared_ptr<ISpxSynthesisResult> result;
    const auto maxRetry = GetOr<int>("SpeechSynthesis_MaxRetryTimes", MAX_RETRY);
    for (auto tryCount = 0; tryCount <= maxRetry * static_cast<int>(retry); ++tryCount)
    {
        if (tryCount > 0)
        {
            turnRequestId = PAL::ToString(PAL::CreateGuidWithoutDashes());
        }
        SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_USP_TTS,
            "%s: start to send synthesis request, request id : %s, try: %d", __FUNCTION__, turnRequestId.c_str(), tryCount);
        result = SpeakInternal(request, turnRequestId);
        if (ResultReason::SynthesizingAudioCompleted == result->GetReason())
        {
            if (auto site = GetSite())
            {
                site->SetBackendName("online (websocket)");
            }
            break;
        }
        else if (ResultReason::Canceled == result->GetReason() && AudioLengthOfCurrentTurn() > 0)
        {
            SPX_TRACE_ERROR("Synthesis cancelled with partial data received, cannot retry.");
            break;
        }
        else if (request->GetRequestId() != m_currentRequestId && !m_currentRequestId.empty())
        {
            SPX_TRACE_ERROR("request changed, won't retry.");
            break;
        }
        else if (m_currentError->GetRetryMode() != ISpxErrorInformation::RetryMode::Allowed)
        {
            SPX_TRACE_ERROR("Synthesis cancelled by user, won't retry.");
            break;
        }
        else if (request->GetInputType() == SynthesisRequestInputType::TextStream)
        {
            SPX_TRACE_WARNING("We cannot retry for text streaming mode.");
            break;
        }

        SPX_TRACE_ERROR("Synthesis cancelled without data received, retrying.");
    }

    return result;
}

void CSpxUspTtsEngineAdapter::Connect()
{
    std::unique_lock<std::recursive_mutex> lock(m_connectionMutex);
    if (m_uspConnection == nullptr)
    {
        UspInitialize();
    }
    else if (!m_uspConnection->IsConnected())
    {
        // If the connection was closed due to any reason, we re-connect
        Disconnect(true);
        UspInitialize();
    }
    else if (PAL::GetTicks(std::chrono::system_clock::now() - m_lastConnectTime) > static_cast<uint64_t>(9) * 60 * 1000 * 10000)
    {
        const auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
        const auto endpoint = properties->GetOr(PropertyId::SpeechServiceConnection_Endpoint, "");

        // skip re-connect for avatar scenario, where usp re-connect is not applicable.
        if (PAL::StringUtils::ToLower(endpoint).find("enabletalkingavatar=true") == std::string::npos)
        {
            // Per Websocket protocol for TTS
            // The service closes the active connect after 10 mins.
            // We re-connect it after 9 mins in case it breaks an on-going speak.
            Disconnect(true);
            UspInitialize();
        }
    }
}

void CSpxUspTtsEngineAdapter::Disconnect(bool async)
{
    // Inform upper layer about disconnection.
    if ((m_uspConnection != nullptr) && m_uspConnection->IsConnected())
    {
        OnDisconnected(nullptr);
    }

    // Term the callbacks first and then reset/release the connection
    SpxTermAndClear(m_uspCallbacks);

    if (async && m_uspConnection != nullptr && m_uspConnection->IsConnected())
    {
        auto connection = m_uspConnection;
        std::packaged_task<void()> task([connection]()
                                        { connection->Shutdown(); });
        m_threadService->ExecuteAsync(std::move(task));
    }

    m_uspConnection.reset();
}

std::shared_ptr<ISpxSynthesisResult> CSpxUspTtsEngineAdapter::SpeakInternal(std::shared_ptr<ISpxSynthesisRequestReader> request, const std::string& requestId)
{
    std::unique_lock<std::recursive_mutex> connectionLock(m_connectionMutex);
    std::string ssml;
    std::shared_ptr<ISpxErrorInformation> ssmlError;
    if (request->GetInputType() == SynthesisRequestInputType::Text)
    {
        const auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
        std::tie(ssml, ssmlError) = CSpxSynthesisHelper::BuildSsml(request->GetInputContent(), properties);
    }
    else if (request->GetInputType() == SynthesisRequestInputType::SSML)
    {
        ssml = request->GetInputContent();
    }

    auto result = GetSite()->CreateEmptySynthesisResult();
    auto resultInit = SpxQueryInterface<ISpxSynthesisResultInit>(result);

    if (ssmlError != nullptr)
    {
        m_currentError = ssmlError;
        resultInit->InitSynthesisResult(requestId, ResultReason::Canceled, ssmlError);
        return result;
    }

    SPX_DBG_TRACE_VERBOSE("SSML sent to TTS cognitive service: %s", ssml.c_str());

    if (!m_shouldStop)
    {
        m_currentRequestId = requestId;
        if (m_uspState == UspState::Error)
        {
            m_uspState = UspState::Idle;
        }
        Connect();
    }

    if (UspState::Error != m_uspState)
    {
        // Set initial values for current utterance
        m_currentWordOffset = 0;
        m_currentSentenceOffset = 0;
        m_partialVisemeAnimation = string();
        m_currentError = nullptr;

        // Send request
        m_uspState = UspState::Sending;
        UspSendSynthesisContext(request, requestId);
        switch (request->GetInputType())
        {
        case SynthesisRequestInputType::Text:
            m_currentText = PAL::StringToU32String(request->GetInputContent());
            m_currentTextIsSsml = false;
            UspSendSsml(ssml, requestId);
            break;
        case SynthesisRequestInputType::SSML:
            m_currentText = PAL::StringToU32String(ssml);
            m_currentTextIsSsml = true;
            UspSendSsml(ssml, requestId);
            break;
        case SynthesisRequestInputType::TextStream:
        {
            // create a new thread to send pieces in background
            std::packaged_task<void()> task([this, request]()
            {
                UspSendTextPieces(request);
            });

            // Use media affinity.
            // User affinity is used for events while Background affinity is used for websocket messages.
            m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Media);
        }
            break;
        default:
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }
    }

    std::unique_lock<std::mutex> lock(m_mutex);

    // wait to get the first audio chunk
    const auto firstChunkTimeout = GetOr<int>("SpeechSynthesis_FirstChunkTimeoutMs", WAIT_FOR_FIRST_CHUNK_TIMEOUT);
#ifdef _DEBUG
    auto remainingTime = firstChunkTimeout;
    while (!m_cv.wait_for(lock, std::chrono::milliseconds(DEBUG_WAIT_INTERVAL), [&]
    {
        return m_uspState == UspState::ReceivingData || m_uspState == UspState::Idle || m_uspState
            == UspState::Error;
    }) && remainingTime > 0)
    {
        SPX_DBG_TRACE_VERBOSE("%s: waiting for USP to get first audio chunk ...", __FUNCTION__);
        remainingTime -= DEBUG_WAIT_INTERVAL;
    }
#else
    m_cv.wait_for(lock, std::chrono::milliseconds(firstChunkTimeout), [&]
    {
        return m_uspState == UspState::ReceivingData || m_uspState == UspState::Idle || m_uspState ==
            UspState::Error;
    });
#endif

    if (m_currentRequestId != requestId)
    {
        return CreateCancelledResult(requestId);
    }

    if (m_uspState == UspState::Sending || m_uspState == UspState::TurnStarted)
    {
        constexpr auto message = "USP error: timeout waiting for the first audio chunk";
        SPX_TRACE_ERROR(message);
        m_currentError = ErrorInfo::FromExplicitError(CancellationErrorCode::ServiceTimeout, message);
        // no data received here, we are safe to retry.
        m_currentError->SetRetryMode(ISpxErrorInformation::RetryMode::Allowed);
        m_uspState = UspState::Error;
    }

    // wait to receive all audio data
    const auto allChunkTimeout = GetOr<int>("SpeechSynthesis_AllChunkTimeoutMs", RECEIVE_ALL_CHUNKS_TIMEOUT);
#ifdef _DEBUG
    remainingTime = allChunkTimeout;
    while (!m_cv.wait_for(lock, std::chrono::milliseconds(DEBUG_WAIT_INTERVAL), [&]
    {
        return m_uspState == UspState::Idle || m_uspState == UspState::Error;
    }) && remainingTime > 0)
    {
        SPX_DBG_TRACE_VERBOSE("%s: waiting for USP to finish receiving data ...", __FUNCTION__);
        remainingTime -= DEBUG_WAIT_INTERVAL;
    }
#else
    m_cv.wait_for(lock, std::chrono::milliseconds(allChunkTimeout), [&] { return m_uspState == UspState::Idle || m_uspState == UspState::Error; });
#endif

    if (m_currentRequestId != requestId)
    {
        return CreateCancelledResult(requestId);
    }

    if (m_uspState == UspState::ReceivingData)
    {
        constexpr auto messageBase = "USP error: timeout waiting for all remaining audio data.";
        SPX_TRACE_ERROR(messageBase);
        std::stringstream message;
        message << messageBase
            << " Received audio size " << AudioLengthOfCurrentTurn() << " bytes.";
        SPX_TRACE_ERROR("%s: %s", __FUNCTION__, message.str().c_str());
        m_currentError = ErrorInfo::FromExplicitError(CancellationErrorCode::ServiceTimeout, message.str());
        if (AudioLengthOfCurrentTurn() == 0)
        {
            SPX_TRACE_WARNING("%s: no data received, set retry mode to allowed.", __FUNCTION__);
            m_currentError->SetRetryMode(ISpxErrorInformation::RetryMode::Allowed);
        }
        m_uspState = UspState::Error;
        Disconnect();
    }

    if (m_uspState == UspState::Error)
    {
        resultInit->InitSynthesisResult(requestId, ResultReason::Canceled, m_currentError);
        SpxQueryInterface<ISpxNamedProperties>(resultInit)->Set(
            PropertyId::CancellationDetails_ReasonDetailedText,
            m_currentError->GetDetails().c_str());
    }
    else
    {
        SPX_TRACE_INFO("%s: synthesis completed. request id: %s.", __FUNCTION__, requestId.c_str());
        resultInit->InitSynthesisResult(requestId, ResultReason::SynthesizingAudioCompleted, nullptr);
    }

    if (requestId == m_currentRequestId)
    {
        m_currentRequestId = std::string();
    }

    return result;
}

void CSpxUspTtsEngineAdapter::StopSpeaking(const std::shared_ptr<ISpxErrorInformation> &reason)
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_USP_TTS, __FUNCTION__);
    m_shouldStop = true;
    auto error = reason ? reason : ErrorInfo::FromHttpStatus(HttpStatusCode::CLIENT_CLOSED_REQUEST);
    OnError(error);
}

void CSpxUspTtsEngineAdapter::GetProxySetting()
{
    m_proxyHost = ISpxPropertyBagImpl::GetOr(PropertyId::SpeechServiceConnection_ProxyHostName, "");
    m_proxyPort = GetOr<int>(PropertyId::SpeechServiceConnection_ProxyPort, 0);
    if (m_proxyPort < 0)
    {
        ThrowInvalidArgumentException("Invalid proxy port: %d", m_proxyPort);
    }

    m_proxyUsername = ISpxPropertyBagImpl::GetOr(PropertyId::SpeechServiceConnection_ProxyUserName, "");
    m_proxyPassword = ISpxPropertyBagImpl::GetOr(PropertyId::SpeechServiceConnection_ProxyPassword, "");
}

void CSpxUspTtsEngineAdapter::SetSpeechConfigMessage()
{
    ajv::JsonBuilder speechConfig;

    // Set the parameters from the user first so that they can be overridden by the system
    for (const auto& item : GetParametersFromUser("speech.config"))
    {
        const auto& name = item.first;
        const auto& value = item.second;
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, value.empty());
        speechConfig[name] = ajv::json::Parse(value);
    }

    // Set system configuration data.
    speechConfig["context"]["system"]["version"] = BuildInformation::g_fullSpeechVersion;
    speechConfig["context"]["system"]["name"] = BuildInformation::g_SpeechSDKName;
    speechConfig["context"]["system"]["build"] = BuildInformation::g_buildPlatform;

    // Set OS configuration data.
    auto osInfo = PAL::getOperatingSystem();
    speechConfig["context"]["os"]["platform"] = osInfo.platform;
    speechConfig["context"]["os"]["name"] = osInfo.name;
    speechConfig["context"]["os"]["version"] = osInfo.version;

    m_speechConfig = speechConfig.AsJson();
}

void CSpxUspTtsEngineAdapter::UspSendSpeechConfig()
{
    constexpr auto messagePath = "speech.config";
    SPX_DBG_TRACE_VERBOSE("%s %s", messagePath, m_speechConfig.c_str());

    UspSendMessage(std::make_unique<USP::TextMessage>(m_speechConfig, messagePath, USP::MessageType::Config));
}

void CSpxUspTtsEngineAdapter::UspSendSynthesisContext(std::shared_ptr<ISpxSynthesisRequestReader> request, const std::string& requestId)
{
    constexpr auto messagePath = "synthesis.context";

    const auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());

    // Set synthesis context data.
    auto synthesisContext = ajv::json::Build();

    for (const auto& item : GetParametersFromUser(messagePath))
    {
        const auto& name = item.first;
        const auto& value = item.second;
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, value.empty());
        synthesisContext[name] = ajv::json::Parse(value);
    }

    synthesisContext["synthesis"]["audio"]["outputFormat"] = m_requestFormatStr;
    synthesisContext["synthesis"]["audio"]["metadataOptions"]["visemeEnabled"] = GetSite()->GetEventsSite()->VisemeReceived.IsConnected();
    synthesisContext["synthesis"]["audio"]["metadataOptions"]["bookmarkEnabled"] = GetSite()->GetEventsSite()->BookmarkReached.IsConnected();
    synthesisContext["synthesis"]["audio"]["metadataOptions"]["wordBoundaryEnabled"] = WordBoundaryEnabled();
    synthesisContext["synthesis"]["audio"]["metadataOptions"]["punctuationBoundaryEnabled"] =
        WordBoundaryEnabled() && GetOr<bool>(PropertyId::SpeechServiceResponse_RequestPunctuationBoundary, true);
    synthesisContext["synthesis"]["audio"]["metadataOptions"]["sentenceBoundaryEnabled"] =
        GetOr<bool>(PropertyId::SpeechServiceResponse_RequestSentenceBoundary, false);

    synthesisContext["synthesis"]["audio"]["metadataOptions"]["sessionEndEnabled"] = true;

    synthesisContext["synthesis"]["language"]["autoDetection"] = CSpxSynthesisHelper::LanguageAutoDetectionEnabled(properties);

    if (request->GetInputType() == SynthesisRequestInputType::TextStream)
    {
        synthesisContext["synthesis"]["input"]["bidirectionalStreamingMode"] = true;

        std::string voice, modelName, language;
        SynthesisRequestVoiceType voiceType;
        std::tie(voice, voiceType, modelName) = request->GetVoiceName();
        if (voiceType == SynthesisRequestVoiceType::PERSONAL)
        {
            synthesisContext["synthesis"]["input"]["personalVoiceName"] = voice;
            // synthesisContext["synthesis"]["input"]["modelName"] = modelName;
            synthesisContext["synthesis"]["input"]["voiceName"] = modelName;
        }
        else
        {
            std::tie(language, voice) = CSpxSynthesisHelper::GetLanguageAndVoice(properties);
            synthesisContext["synthesis"]["input"]["voiceName"] = voice;
            synthesisContext["synthesis"]["input"]["language"] = language;
        }

        auto requestProperties = SpxQueryInterface<ISpxNamedProperties>(request);
        auto pitch = requestProperties->Get<std::string>(PropertyId::SpeechSynthesisRequest_Pitch);
        if (pitch) {
            synthesisContext["synthesis"]["input"]["pitch"] = pitch.Get();
        }

        auto rate = requestProperties->Get<std::string>(PropertyId::SpeechSynthesisRequest_Rate);
        if (rate) {
            synthesisContext["synthesis"]["input"]["rate"] = rate.Get();
        }

        auto volume = requestProperties->Get<std::string>(PropertyId::SpeechSynthesisRequest_Volume);
        if (volume) {
            synthesisContext["synthesis"]["input"]["volume"] = volume.Get();
        }

        auto style = requestProperties->Get<std::string>(PropertyId::SpeechSynthesisRequest_Style);
        if (style) {
            synthesisContext["synthesis"]["input"]["style"] = style.Get();
        }

        auto temperature = requestProperties->Get<std::string>(PropertyId::SpeechSynthesisRequest_Temperature);
        if (temperature) {
            synthesisContext["synthesis"]["input"]["temperature"] = temperature.Get();
        }

        auto customLexiconUrl = requestProperties->Get<std::string>(PropertyId::SpeechSynthesisRequest_CustomLexiconUrl);
        if (customLexiconUrl) {
            synthesisContext["synthesis"]["input"]["customLexiconUrl"] = customLexiconUrl.Get();
        }

        auto preferLocales = requestProperties->Get<std::string>(PropertyId::SpeechSynthesisRequest_PreferLocales);
        if (preferLocales) {
            synthesisContext["synthesis"]["input"]["preferLocales"] = preferLocales.Get();
        }
    }

    UspSendMessage(
        std::make_unique<USP::TextMessage>(
            synthesisContext.AsJson(), messagePath, USP::MessageType::Context, requestId));
}

void CSpxUspTtsEngineAdapter::UspSendSsml(const std::string& ssml, const std::string& requestId)
{
    constexpr auto messagePath = "ssml";
    constexpr auto contentType = "application/ssml+xml";

    SPX_DBG_TRACE_VERBOSE("%s %s", messagePath, ssml.c_str());

    UspSendMessage(
        std::make_unique<USP::TextMessage>(
            ssml, messagePath, contentType, USP::MessageType::Ssml, requestId));
}

void CSpxUspTtsEngineAdapter::UspSendTextPieces(std::shared_ptr<ISpxSynthesisRequestReader> request)
{
    bool hasMore = true;
    std::string piece;
    while (hasMore) {
        std::tie(hasMore, piece) = request->GetNextTextPiece();
        if (!hasMore) {
            break;
        }

        if (piece.empty()) {
            continue;
        }

        UspSendMessage(std::make_unique<USP::TextMessage>(piece, "text.piece", "text/plain", USP::MessageType::Ssml, m_currentRequestId));
    }

    UspSendMessage(std::make_unique<USP::TextMessage>("", "text.end", "text/plain", USP::MessageType::Ssml, m_currentRequestId));
}

void CSpxUspTtsEngineAdapter::UspSendMessage(std::unique_ptr<USP::TextMessage> message)
{
    if (message == nullptr)
    {
        SPX_TRACE_WARNING("Received a null message to send. Ignoring");
        return;
    }

    SPX_DBG_TRACE_VERBOSE("%s='%s'", message->Path().c_str(), message->Data().c_str());
    std::weak_ptr<ISpxUspConnection> connection(m_uspConnection);

    // NOTE:
    // -----
    // There was an issue in the Microsoft C++ compiler that prevents from creating a packaged_task
    // from a move only lambda: https://github.com/microsoft/STL/issues/321
    // To work around this, we'll need to pass a raw pointer.

#ifdef _MSC_VER
    USP::TextMessage* ptr = nullptr;
    try
    {
        ptr = message.release();

        std::packaged_task<void()> task([connection, ptr]()
        {
            DoSendMessageWork(connection, std::unique_ptr<USP::TextMessage>(ptr));
        });
        m_threadService->ExecuteAsync(move(task));

        // successfully queued so no need to free it
        // NOTE: There is still the potential of a memory leak here if the thread_service gets terminated
        //       before it is able to execute the queued lambda, can't see an easy way to prevent that.
        ptr = nullptr;
    }
    catch (...)
    {
        // Free to prevent memory leaks. This works since message uses the default deleter
        delete ptr;

        throw;
    }

    // Free to prevent memory leaks. This works since message uses the default deleter
    delete ptr;
#else
    std::packaged_task<void()> task([connection, message = std::move(message)]() mutable
    {
        DoSendMessageWork(connection, std::move(message));
    });
    m_threadService->ExecuteAsync(std::move(task));
#endif
}

void CSpxUspTtsEngineAdapter::DoSendMessageWork(std::weak_ptr<ISpxUspConnection> connectionPtr, std::unique_ptr<USP::TextMessage> message)
{
    auto connection = connectionPtr.lock();
    if (connection != nullptr)
    {
        connection->QueueMessage(std::move(message));
    }
    else
    {
        SPX_TRACE_ERROR("usp connection lost when trying to send message.");
    }
}

void CSpxUspTtsEngineAdapter::UspInitialize()
{
    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_uspConnection != nullptr);

    m_uspState = UspState::Connecting;

    // Fill authorization token
    std::array<std::string, static_cast<size_t>(USP::AuthenticationType::SIZE_AUTHENTICATION_TYPE)> authData;

    // Get the named property service...
    auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, properties == nullptr);

    // Get the properties that indicates what endpoint to use...
    auto uspSubscriptionKey = properties->GetOr(PropertyId::SpeechServiceConnection_Key, "");
    auto token = properties->GetOr(PropertyId::SpeechServiceAuthorization_Token, "");

    authData[static_cast<size_t>(USP::AuthenticationType::SubscriptionKey)] = std::move(uspSubscriptionKey);
    authData[static_cast<size_t>(USP::AuthenticationType::AuthorizationToken)] = std::move(token);

    SetAsDefault("HttpHeader#User-agent", ConstructUserAgent().c_str());
    auto headers = FindPrefix("HttpHeader");

    // Create the usp clientConfiguration, which we'll configure and use to create the actual connection
    auto uspCallbacks = SpxCreateObjectWithSite<ISpxUspCallbacks>("CSpxUspCallbackWrapper", this);
    auto clientConfiguration = USP::ClientConfiguration(uspCallbacks, USP::EndpointType::SpeechSynthesis, PAL::CreateGuidWithoutDashesUTF8())
        .SetAuthentication(authData)
        .SetUnderlyingOption(USP::endpoint::tcpNodelayOption, 1)
        .SetUserDefinedHttpHeaders(headers);

    SetUspEndpoint(properties, clientConfiguration);

    // Set proxy
    if (!m_proxyHost.empty() && m_proxyPort > 0)
    {
        clientConfiguration.SetProxyServerInfo(m_proxyHost.data(), m_proxyPort, m_proxyUsername.data(), m_proxyPassword.data());
    }

    // set proxy bypass list
    clientConfiguration.SetProxyHostBypass(
        PAL::StringUtils::Tokenize(
            properties->GetOr(PropertyId::SpeechServiceConnection_ProxyHostBypass, ""),
            ","));

    SetUspCrlProperties(properties, clientConfiguration);

    // Try to connect
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
        constexpr auto message = "Error: Unexpected exception in UspInitialize";
        SPX_TRACE_ERROR(message);
        auto error = ErrorInfo::FromExplicitError(CancellationErrorCode::ConnectionFailure, message);
        OnError(error);
    }

    // if error occurs in the above clientConfiguration.Connect, set state to error and return.
    if (uspConnection == nullptr)
    {
        constexpr auto message = "Error: failed to establish USP connection.";
        SPX_TRACE_ERROR(message);
        m_uspState = UspState::Error;
        m_currentError = ErrorInfo::FromExplicitError(CancellationErrorCode::ConnectionFailure, message);
        return;
    }

    // We're done!!
    m_uspCallbacks = uspCallbacks;
    m_uspConnection = std::move(uspConnection);
    if (m_uspState == UspState::Connecting)
    {
        m_uspState = UspState::Idle;

        m_lastConnectTime = std::chrono::system_clock::now();

        // Send speech config message
        if (m_uspConnection != nullptr)
        {
            properties->Set(PropertyId::SpeechServiceConnection_Url, m_uspConnection->GetConnectionUrl().c_str());

            // Construct config message payload
            SetSpeechConfigMessage();

            // Send speech config
            UspSendSpeechConfig();
        }
    }
}

USP::ClientConfiguration& CSpxUspTtsEngineAdapter::SetUspEndpoint(const std::shared_ptr<ISpxNamedProperties>& properties,
    USP::ClientConfiguration& client) const
{
    SPX_DBG_ASSERT(GetSite() != nullptr);

    const auto endpoint = properties->GetOr(PropertyId::SpeechServiceConnection_Endpoint, "");
    const auto host = properties->GetOr(PropertyId::SpeechServiceConnection_Host, "");
    const auto region = properties->GetOr(PropertyId::SpeechServiceConnection_Region, "");

    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, static_cast<int>(endpoint.empty()) + static_cast<int>(host.empty()) + static_cast<int>(region.empty()) != 2);

    if (!endpoint.empty())
    {
        // set endpoint url if this is provided
        SPX_DBG_TRACE_VERBOSE("%s: Using custom endpoint: %s", __FUNCTION__, endpoint.c_str());
        client.SetEndpointUrl(endpoint);
    }
    else if (!host.empty())
    {
        // set host url if this is provided
        SPX_DBG_TRACE_VERBOSE("%s: Using custom host: %s", __FUNCTION__, host.c_str());
        client.SetHostUrl(host);
    }
    else
    {
        client.SetRegion(region);
    }

    const auto endpointId = properties->GetOr(PropertyId::SpeechServiceConnection_EndpointId, "");
    if (!endpointId.empty())
    {
        client.SetQueryParameter(USP::endpoint::speechSynthesis::deploymentIdQueryParam, endpointId);
    }

    return client;
}

USP::ClientConfiguration& CSpxUspTtsEngineAdapter::SetUspCrlProperties(const std::shared_ptr<ISpxNamedProperties>& properties, USP::ClientConfiguration& clientConfiguration) const
{
#if SPEECHSDK_USE_OPENSSL
    // N.B. the names of the options below have been shared with a customer. Do
    // not change them without consulting with them.
    auto singleTrustedCert = properties->GetStringValue("OPENSSL_SINGLE_TRUSTED_CERT");
    if(!singleTrustedCert.empty())
    {
        clientConfiguration = clientConfiguration.SetSingleTrustedCert(singleTrustedCert);
    }

    bool check_single_crl = properties->GetOr<bool>("OPENSSL_SINGLE_TRUSTED_CERT_CRL_CHECK", true);
    bool disable_crl_check = properties->GetOr<bool>("OPENSSL_DISABLE_CRL_CHECK", true);

    if( (!singleTrustedCert.empty() && !check_single_crl) ||
        disable_crl_check)
    {
        clientConfiguration = clientConfiguration.SetDisableCrlChecks(true);
    }

    bool continue_on_crl_download_failure = properties->GetOr<bool>("OPENSSL_CONTINUE_ON_CRL_DOWNLOAD_FAILURE", true);
    clientConfiguration = clientConfiguration.SetContinueOnCrlDownloadFailure(continue_on_crl_download_failure);

    int max_crl_download_size_in_kb = GetOr<int>("CONFIG_MAX_CRL_SIZE_KB", MAX_CRL_SIZE_DEFAULT);
    clientConfiguration.SetMaxCrlDownloadSizeInKB(max_crl_download_size_in_kb);

#else
    UNUSED(properties);
#endif

    return clientConfiguration;
}

void CSpxUspTtsEngineAdapter::OnTurnStart(const USP::TurnStartMsg& message)
{
    if (message.requestId != m_currentRequestId)
    {
        SPX_TRACE_WARNING("%s: current request (%s) is different from message request id (%s), ignore.",
            __FUNCTION__, m_currentRequestId.c_str(), message.requestId.c_str());
        return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_uspState == UspState::Sending)
    {
        // todo: remove and update this after protocol finalized.
        if (message.json.size() > 100)
        {
            InvokeOnSite([&message](const SitePtr& p) {
                auto properties = SpxQueryService<ISpxNamedProperties>(p);
                properties->Set("SpeechSDKInternal-ExtraTurnStartMessage", message.json.c_str());
            });
        }

        InvokeOnSite([this](const SitePtr& p) { p->FireAdapterResult_TurnStarted(this); });
        m_uspState = UspState::TurnStarted;
    }
    else if (m_uspState != UspState::Error)
    {
        SPX_TRACE_ERROR("turn.start received in invalid state, current state is: %d", static_cast<int>(UspState(m_uspState)));
        SPX_THROW_HR(SPXERR_INVALID_STATE);
    }

    m_cv.notify_all();
}

void CSpxUspTtsEngineAdapter::SetParameter(const char *path, const char *name, const char *value)
{
    if (strlen(value) > MAX_JSON_PAYLOAD_FROM_USER)
    {
        ThrowInvalidArgumentException("The value for SpeechContext exceed 50 MBytes!");
    }

    if (!ajv::json::Parse(value).IsOk())
    {
        std::stringstream ss;
        ss << "The user specified path: ";
        ss << path;
        ss << "  parameter name: ";
        ss << name;
        ss << " parameter value: ";
        ss << value;
        ss << " has invalid json string.";
        ThrowInvalidArgumentException(ss.str());
    }
    std::string path2 = path;

    transform(path2.begin(), path2.end(), path2.begin(), [](unsigned char c) ->char { return (char)::tolower(c); });
    {
        std::unique_lock<std::mutex> lock{ m_uspParameterLock };

        auto existing = m_uspParametersFromUser.find(path);
        if (existing == end(m_uspParametersFromUser))
        {
            m_uspParametersFromUser[path] = { {name, value} };
        }
        else
        {
            existing->second[name] = value;
        }
    }
}

CSpxAsyncOp<bool> CSpxUspTtsEngineAdapter::SendNetworkMessage(const char *path, std::string&& payload)
{
    // for some reason, no connection is established
    if (m_uspConnection == nullptr || m_uspState == UspState::Error)
    {
        ThrowRuntimeError("No usp connection.");
    }

    auto contentType = "application/json";
    if (PAL::StringUtils::ToLower(path) == "ssml")
    {
        contentType = "application/ssml+xml";
    }

    auto message = std::make_unique<USP::TextMessage>(std::move(payload), path, contentType, USP::MessageType::Unknown, m_currentRequestId);
    UspSendMessage(std::move(message));
    return CSpxAsyncOp<bool>::FromResult(true);
}

CSpxAsyncOp<bool> CSpxUspTtsEngineAdapter::SendNetworkMessage(const char * /*path*/, std::vector<uint8_t>&& /*payload*/)
{
    // Not currently supported
    SPX_THROW_HR(SPXERR_NOT_IMPL);
}

CSpxStringMap CSpxUspTtsEngineAdapter::GetParametersFromUser(std::string&& path)
{
    CSpxStringMap result;
    {
        std::unique_lock<std::mutex> lock{ m_uspParameterLock };

        auto search = m_uspParametersFromUser.find(path);
        if (search != end(m_uspParametersFromUser))
        {
            result = search->second;
        }
    }
    return result;
}

void CSpxUspTtsEngineAdapter::OnAudioOutputChunk(const USP::AudioOutputChunkMsg& message)
{
    if (message.requestId != m_currentRequestId)
    {
        SPX_TRACE_WARNING("%s: current request (%s) is different from message request id (%s), ignore.",
            __FUNCTION__, m_currentRequestId.c_str(), message.requestId.c_str());
        return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_uspState == UspState::TurnStarted)
    {
        m_uspState = UspState::ReceivingData;
    }
    else if (m_uspState != UspState::ReceivingData)
    {
        SPX_TRACE_ERROR("Received chunk data in unexpected state, ignore. Current state: %d", static_cast<int>(UspState(m_uspState)));
        return;
    }

    InvokeOnSite([this, message](const SitePtr& p) {
        if (message.audioLength > 0)
        {
            p->Write(this, m_originalRequestId, const_cast<uint8_t *>(message.audioBuffer),
                     static_cast<uint32_t>(message.audioLength), nullptr);
        }
    });

    m_cv.notify_all();
}

void CSpxUspTtsEngineAdapter::OnAudioOutputMetadata(const USP::AudioOutputMetadataMsg& message)
{
    if (message.requestId != m_currentRequestId)
    {
        SPX_TRACE_WARNING("%s: current request (%s) is different from message request id (%s), ignore.",
            __FUNCTION__, m_currentRequestId.c_str(), message.requestId.c_str());
        return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_uspState == UspState::TurnStarted)
    {
        m_uspState = UspState::ReceivingData;
    }
    else if (m_uspState != UspState::ReceivingData)
    {
        SPX_TRACE_ERROR("Received audio metadata data in unexpected state, ignore. Current state: %d", static_cast<int>(UspState(m_uspState)));
        return;
    }

    for (const auto &metadata : message.metadatas)
    {
        if (metadata.type == "TextBoundary")
        {
            auto &wordBoundary = metadata.textBoundary;
            auto boundaryType = SpeechSynthesisBoundaryType::Word;
            if (!EnumHelpers::TryParse(wordBoundary.boundaryType.c_str(), boundaryType)) {
                SPX_TRACE_ERROR("Unknown boundary type [%s]", wordBoundary.boundaryType.c_str());
            }

            // Under no circumstances should the service return a word boundary with an empty text.
            // But unfortunately, the service had an issue where it may return an empty text.
            // So we add following code to avoid infinite loop.
            if (wordBoundary.text.empty())
            {
                SPX_TRACE_ERROR("Word boundary text is empty. This is a bug in the service.");
                continue;
            }

            std::string decodedText = wordBoundary.text;
            if (!m_currentTextIsSsml) {
                decodedText = CSpxSynthesisHelper::XmlDecode(wordBoundary.text);
            }

            int& currentTextOffset = boundaryType == SpeechSynthesisBoundaryType::Sentence ? m_currentSentenceOffset : m_currentWordOffset;
            auto boundaryTextU32 = PAL::StringToU32String(decodedText);
            auto textOffset = m_currentText.find(boundaryTextU32, currentTextOffset);

            if (m_currentTextIsSsml)
            {
                while (textOffset != std::string::npos && !IsXmlTag(wordBoundary.text) && InSsmlTag(textOffset, m_currentText, currentTextOffset))
                {
                    textOffset = m_currentText.find(boundaryTextU32, textOffset + boundaryTextU32.length());
                }
            }

            if (textOffset != std::string::npos)
            {
                currentTextOffset = static_cast<int>(textOffset + boundaryTextU32.length());
            }
            else
            {
                SPX_TRACE_WARNING("Word boundary text [%s] not found.", wordBoundary.text.c_str());
            }

            InvokeOnSite([this, &metadata, &wordBoundary, &textOffset, &boundaryType, &boundaryTextU32, &decodedText](const SitePtr &p)
                         { p->FireAdapterResult_WordBoundary(this, metadata.audioOffset,
                                                             wordBoundary.duration,
                                                             static_cast<uint32_t>(textOffset),
                                                             static_cast<uint32_t>(boundaryTextU32.length()),
                                                             decodedText,
                                                             boundaryType); });
        }
        else if (metadata.type == "Viseme")
        {
            auto &viseme = metadata.viseme;
            if (viseme.isLastAnimation)
            {
                auto animation = m_partialVisemeAnimation + viseme.animationChunk;
                m_partialVisemeAnimation = string();

                InvokeOnSite([this, &viseme, &animation](const SitePtr &p) {
                    p->FireAdapterResult_VisemeReceived(this, viseme.audioOffset, viseme.visemeId, std::move(animation));
                });
            }
            else
            {
                m_partialVisemeAnimation += viseme.animationChunk;
            }
        }
        else if (metadata.type == "Bookmark")
        {
            auto &bookmark = metadata.bookmark;
            InvokeOnSite([this, &bookmark](const SitePtr &p) {
                p->FireAdapterResult_BookmarkReached(this, bookmark.audioOffset, bookmark.text);
            });
        }
        else if (metadata.type == "SessionEnd")
        {
            SPX_TRACE_VERBOSE("Session end received, audio duration %" PRIu64 " ticks", metadata.audioOffset);
            InvokeOnSite([this, &metadata](const SitePtr &p) {
                // metadata.audioOffset is the audio duration of the synthesis request, in ticks (100 nanoseconds).
                // Convert it to milliseconds.
                p->SetSynthesisResultAudioDuration(this, metadata.audioOffset / static_cast<uint64_t>(10000));
            });
        }
    }

    m_cv.notify_all();
}

void CSpxUspTtsEngineAdapter::OnTurnEnd(const USP::TurnEndMsg &message)
{
    if (message.requestId != m_currentRequestId)
    {
        SPX_TRACE_WARNING("%s: current request (%s) is different from message request id (%s), ignore.",
                          __FUNCTION__, m_currentRequestId.c_str(), message.requestId.c_str());
        return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    InvokeOnSite([this](const SitePtr& p) { p->EndOfTurn(this); });
    m_uspState = UspState::Idle;
    m_cv.notify_all();
}

void CSpxUspTtsEngineAdapter::OnError(const std::shared_ptr<ISpxErrorInformation> &error)
{
    SPX_DBG_TRACE_VERBOSE("Response: On Error: Code:%d, Message: %s.\n", static_cast<int>(error->GetCancellationCode()), error->GetDetails().c_str());
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_uspState != UspState::Idle || m_shouldStop) // no need to raise an error if synthesis is already done.
    {

        std::stringstream newMessage;
        newMessage << "USP state: " << EnumHelpers::ToString(m_uspState.load()) << "."
                   << " Received audio size: " << AudioLengthOfCurrentTurn() << " bytes.";
        m_currentError = ErrorInfo::FromErrorWithAppendedDetails(error, newMessage.str());
        m_uspState = UspState::Error;
        m_cv.notify_all();

        if (m_uspState != UspState::Idle && (error->GetCategoryCode() != static_cast<int>(HttpStatusCode::CLIENT_CLOSED_REQUEST)
            || !PAL::ToBool(ISpxPropertyBagImpl::GetStringValue("SpeechSynthesis_KeepConnectionAfterStopping"))))
        {
            // terminate usp connection on error
            Disconnect();
        }
    }
}

void CSpxUspTtsEngineAdapter::OnConnected(const std::string&)
{
    InvokeOnSite([this](const SitePtr& p) { p->FireAdapterResult_ConnectionChanged(this, true); });
}

void CSpxUspTtsEngineAdapter::OnDisconnected(const std::shared_ptr<ISpxErrorInformation>&)
{
    InvokeOnSite([this](const SitePtr& p) { p->FireAdapterResult_ConnectionChanged(this, false); });
}

bool CSpxUspTtsEngineAdapter::WordBoundaryEnabled() const
{
    auto wordBoundaryConnected = GetSite()->GetEventsSite()->WordBoundary.IsConnected();
    return GetOr<bool>(PropertyId::SpeechServiceResponse_RequestWordBoundary, wordBoundaryConnected);
}

bool CSpxUspTtsEngineAdapter::InSsmlTag(size_t currentPos, const std::u32string& ssml, size_t beginningPos)
{
    if (currentPos < beginningPos || currentPos >= ssml.length() || beginningPos >= ssml.length())
    {
        return false;
    }

    auto pos = currentPos;
    while (pos >= beginningPos)
    {
        if (ssml[pos] == '>')
        {
            return false;
        }
        if (ssml[pos] == '<')
        {
            return true;
        }
        pos--;
    }

    return false;
}

bool CSpxUspTtsEngineAdapter::IsXmlTag(const std::string& word)
{
    if (word.length() < 2)
    {
        return false;
    }
    return word[0] == '<' && word[word.length() - 1] == '>';
}

std::shared_ptr<ISpxSynthesisResult> CSpxUspTtsEngineAdapter::CreateCancelledResult(const std::string& requestId) const
{
    auto result = GetSite()->CreateEmptySynthesisResult();
    auto resultInit = SpxQueryInterface<ISpxSynthesisResultInit>(result);
    if (m_uspState == UspState::Error)
    {
        const auto error = ErrorInfo::FromHttpStatus(HttpStatusCode::CLIENT_CLOSED_REQUEST);
        resultInit->InitSynthesisResult(requestId, ResultReason::Canceled, error);
    }

    return result;
}

size_t CSpxUspTtsEngineAdapter::AudioLengthOfCurrentTurn() const
{
    size_t length = 0;
    InvokeOnSite([&length](const SitePtr &p)
                 { length = p->AudioLengthOfCurrentTurn(); });
    return length;
}
} } } } // Microsoft::CognitiveServices::Speech::Impl
