//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// recognizer.cpp: Implementation definitions for CSpxRecognizer C++ class
//

#include "stdafx.h"
#include "recognizer.h"
#include <future>
#include <sstream>
#include "site_helpers.h"
#include "service_helpers.h"
#include "create_object_helpers.h"
#include "property_id_2_name_map.h"
#include "log_helpers.h"
#include <ajv.h>

#include "iostream"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxRecognizer::CSpxRecognizer() :
    ISpxRecognizerEvents()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxRecognizer::~CSpxRecognizer()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    TermDefaultSession();
}

void CSpxRecognizer::Init()
{
    SPX_DBG_TRACE_FUNCTION();
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, GetSite() == nullptr);
    EnsureDefaultSession();
    CheckLogFilename();
}

void CSpxRecognizer::Term()
{
    SPX_DBG_TRACE_FUNCTION();
}

void CSpxRecognizer::SetStringValue(const char* name, const char* value)
{
    auto namedProperties = GetParentProperties();
    if (namedProperties)
    {
        namedProperties->SetStringValue(name, value);
    }
    else
    {
        // Fallback when the recognizer has no assigned site (and therefore no parent properties).
        // Write directly to the local property bag, which Match() scans before parent properties.
        ISpxPropertyBagImpl::SetStringValue(name, value);
    }
}

void CSpxRecognizer::SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size)
{
    auto namedProperties = GetParentProperties();
    if (namedProperties)
    {
        namedProperties->SetBinaryValue(name, value, size);
    }
    else
    {
        // Fallback when the recognizer has no assigned site (and therefore no parent properties).
        // Write directly to the local property bag, which Match() scans before parent properties.
        ISpxPropertyBagImpl::SetBinaryValue(name, value, size);
    }
}

std::shared_ptr<ISpxGrammar> CSpxRecognizer::GetPhraseListGrammar(const wchar_t* name)
{
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, name != nullptr && name[0] != L'\0'); // only support one phrase list at this point
    return SpxQueryInterface<ISpxGrammar>(EnsureDefaultPhraseListGrammar());
}

void CSpxRecognizer::AddGrammar(std::shared_ptr<ISpxGrammar> grammar)
{
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, grammar == nullptr);

    m_grammarlist.push_back(grammar);
}

void CSpxRecognizer::SetRecognitionFactor(double factor)
{
    // For now we only support one factor.
    SetStringValue("SPEECH-WordLevelRecognitionFactor", std::to_string(factor).c_str());
}

std::list<std::string> CSpxRecognizer::GetListenForList()
{
    auto retVal = std::list<std::string>();

    for (auto& grammar : m_grammarlist)
    {
        auto grammarListenFor = grammar->GetListenForList();
        retVal.insert(retVal.end(), grammarListenFor.begin(), grammarListenFor.end());
    }

    // Check for phrase list weight. Only a single phrase list and weight is supported.
    if (!m_grammarlist.empty())
    {
        auto phraseList = SpxQueryInterface<ISpxPhraseList>(m_grammarlist.front());
        if (phraseList)
        {
            double weight = phraseList->GetWeight();
            SetStringValue(g_phraseListWeightPropertyName, std::to_string(weight).c_str());
        }
        else
        {
            SetStringValue(g_phraseListWeightPropertyName, "");
        }
    }

    return retVal;
}

void CSpxRecognizer::SetDisposing()
{
    if (m_defaultSession)
    {
        m_defaultSession->SetDisposing();
    }
}

void CSpxRecognizer::OpenConnection(bool forContinuousRecognition)
{
    EnsureDefaultSession();
    m_defaultSession->OpenConnection(forContinuousRecognition);
}

void CSpxRecognizer::CloseConnection()
{
    EnsureDefaultSession();
    m_defaultSession->CloseConnection();
}

CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> CSpxRecognizer::RecognizeAsync()
{
    const char* recoModePropertyName = GetPropertyName(PropertyId::SpeechServiceConnection_RecoMode);
    auto currentRecoMode = GetStringValue(recoModePropertyName, "");

    // with VAD on, set reco mode and invoke vad single shot function.
    if (GetOr<bool>(g_Detection_VadModeOn, false))
    {
        // currently, vad uses recoModeInteractive as default.
        if (currentRecoMode.empty())
        {
            SetStringValue(recoModePropertyName, g_recoModeInteractive);
        }
        return m_defaultSession->RecognizeAsyncWithVAD();
    }

    auto recoModeToSet = g_recoModeInteractive;

    if (currentRecoMode.empty())
    {
        SetStringValue(recoModePropertyName, recoModeToSet);
    }
    else
    {
        // If the reco mode is set to dictation (which can only be set before starting any recognition), just use it.
        // But switching between interactive and conversation after connection setup is not allowed.
        SPX_THROW_HR_IF(SPXERR_SWITCH_MODE_NOT_ALLOWED, (currentRecoMode.compare(g_recoModeDictation) != 0 && currentRecoMode.compare(recoModeToSet)) != 0);
    }

    return m_defaultSession->RecognizeAsync();
}

CSpxAsyncOp<void> CSpxRecognizer::StartContinuousRecognitionAsync()
{
    const char* recoModePropertyName = GetPropertyName(PropertyId::SpeechServiceConnection_RecoMode);
    auto currentRecoMode = GetStringValue(recoModePropertyName, "");

    // with VAD on, set reco mode and invoke vad continous recognition function.
    if (GetOr<bool>(g_Detection_VadModeOn, false))
    {
        // VAD uses conversation mode by default with USP engine.
        if (currentRecoMode.empty())
        {
            SetStringValue(recoModePropertyName, g_recoModeConversation);
        }
        return m_defaultSession->StartContinuousRecognitionAsyncWithVAD();
    }

    auto recoModeToSet = g_recoModeConversation;
    if (currentRecoMode.empty())
    {
        SetStringValue(recoModePropertyName, recoModeToSet);
    }
    else
    {
        // If the reco mode is set to dictation (which can only be set before starting any recognition), just use it.
        // But switching between interactive and conversation after connection setup is not allowed.
        SPX_THROW_HR_IF(SPXERR_SWITCH_MODE_NOT_ALLOWED, (currentRecoMode.compare(g_recoModeDictation) != 0 && currentRecoMode.compare(recoModeToSet) != 0));
    }
    return m_defaultSession->StartContinuousRecognitionAsync();
}

CSpxAsyncOp<void> CSpxRecognizer::StopContinuousRecognitionAsync()
{
    if (m_defaultSession && GetOr<bool>(g_Detection_VadModeOn, false))
    {
        return m_defaultSession->StopContinuousRecognitionAsyncWithVAD();
    }
    else if(m_defaultSession)
    {
        return m_defaultSession->StopContinuousRecognitionAsync();
    }
    else
    {
        // in some error cases, m_defaultSession is empty. Return AOS_Error.
        std::promise<void> void_promise;
        void_promise.set_exception(std::make_exception_ptr(std::runtime_error("The default session is a nullptr.")));
        std::shared_future<void> void_future = void_promise.get_future().share();
        return CSpxAsyncOp<void>(void_future, AOS_Error);
    }
}

CSpxAsyncOp<void> CSpxRecognizer::StartKeywordRecognitionAsync(std::shared_ptr<ISpxKwsModel> model)
{
    const char* recoModePropertyName = GetPropertyName(PropertyId::SpeechServiceConnection_RecoMode);
    auto currentRecoMode = GetStringValue(recoModePropertyName, "");

    // currently, kws uses recoModeInteractive as default, but takes the passed mode, if configured
    if (currentRecoMode.empty())
    {
        SetStringValue(recoModePropertyName, g_recoModeInteractive);
    }
    return m_defaultSession->StartKeywordRecognitionAsync(model);
}

CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> CSpxRecognizer::RecognizeAsync(std::shared_ptr<ISpxKwsModel> model)
{
    constexpr auto recoModePropertyName = GetPropertyName(PropertyId::SpeechServiceConnection_RecoMode);
    auto currentRecoMode = GetStringValue(recoModePropertyName, "");
    if (currentRecoMode.empty())
    {
        SetStringValue(recoModePropertyName, g_recoModeInteractive);
    }
    return m_defaultSession->RecognizeAsync(model);
}

CSpxAsyncOp<void> CSpxRecognizer::StopKeywordRecognitionAsync()
{
    return m_defaultSession->StopKeywordRecognitionAsync();
}

std::shared_ptr<ISpxSession> CSpxRecognizer::GetDefaultSession()
{
    EnsureDefaultSession();
    return m_defaultSession;
}

std::shared_ptr<ISpxEventArgsFactory> CSpxRecognizer::GetEventArgsFactory()
{
    SPX_DBG_ASSERT(GetSite());
    return SpxQueryService<ISpxEventArgsFactory>(GetSite());
}

std::shared_ptr<ISpxSessionEventArgs> CSpxRecognizer::CreateSessionEventArgs(const std::wstring& sessionId)
{
    auto factory = GetEventArgsFactory();
    return factory->CreateSessionEventArgs(sessionId);
}

std::shared_ptr<ISpxConnectionEventArgs> CSpxRecognizer::CreateConnectionEventArgs(const std::wstring& sessionId)
{
    auto factory = GetEventArgsFactory();
    return factory->CreateConnectionEventArgs(sessionId);
}

std::shared_ptr<ISpxSessionEventArgs> CSpxRecognizer::CreateTokenRequestEventArgs(const std::wstring& sessionId)
{
    auto factory = GetEventArgsFactory();
    return factory->CreateTokenRequestEventArgs(sessionId);
}

void CSpxRecognizer::FireSessionStarted(const std::wstring& sessionId)
{
    auto sessionEvent = CreateSessionEventArgs(sessionId);
    SessionStarted.Signal(sessionEvent);
}

void CSpxRecognizer::FireSessionStopped(const std::wstring& sessionId)
{
    auto sessionEvent = CreateSessionEventArgs(sessionId);
    SessionStopped.Signal(sessionEvent);
}

void CSpxRecognizer::FireConnected(const std::wstring& sessionId)
{
    auto connectionEvent = CreateConnectionEventArgs(sessionId);
    Connected.Signal(connectionEvent);
}

void CSpxRecognizer::FireDisconnected(const std::wstring& sessionId)
{
    auto connectionEvent = CreateConnectionEventArgs(sessionId);
    Disconnected.Signal(connectionEvent);
}

void CSpxRecognizer::FireConnectionMessageReceived(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary)
{
    if (ConnectionMessageReceived.IsConnected())
    {
        auto factory = GetEventArgsFactory();
        auto connectionMessageEvent = factory->CreateConnectionMessageEventArgs(headers, path, buffer, bufferSize, isBufferBinary);
        ConnectionMessageReceived.Signal(connectionMessageEvent);
    }
}

void CSpxRecognizer::FireSpeechStartDetected(const std::wstring& sessionId, uint64_t offset)
{
    FireRecoEvent(&SpeechStartDetected, sessionId, nullptr, offset);
}

void CSpxRecognizer::FireSpeechEndDetected(const std::wstring& sessionId, uint64_t offset)
{
    FireRecoEvent(&SpeechEndDetected, sessionId, nullptr, offset);
}

void CSpxRecognizer::FireTokenRequest(const std::wstring& sessionId)
{
    auto sessionEvent = CreateTokenRequestEventArgs(sessionId);
    TokenRequested.Signal(sessionEvent);
}

void CSpxRecognizer::FireResultEvent(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    ISpxRecognizerEvents::RecoEvent_Type* pevent = nullptr;
    auto reason = result->GetReason();
    switch (reason)
    {
    case ResultReason::Canceled:
        pevent = &Canceled;
        break;

    case ResultReason::NoMatch:
    case ResultReason::RecognizedSpeech:
    case ResultReason::RecognizedKeyword:
    case ResultReason::TranslatedSpeech:
        pevent = &FinalResult;
        SPX_DBG_TRACE_VERBOSE_IF(!pevent->IsConnected(), "%s: No FinalResult event signal connected!! nobody listening...", __FUNCTION__);
        break;

    case ResultReason::RecognizingSpeech:
    case ResultReason::RecognizingKeyword:
    case ResultReason::TranslatingSpeech:
        pevent = &IntermediateResult;
        break;

    case ResultReason::SynthesizingAudio:
    case ResultReason::SynthesizingAudioCompleted:
        pevent = &TranslationSynthesisResult;
        break;

    default:
        // TODO: This should be changed to throw exception. But currently it causes problem in lock.
        SPX_DBG_ASSERT_WITH_MESSAGE(false, "The reason found in the result was unexpected.");
        break;
    }

    FireRecoEvent(pevent, sessionId, result);
}

void CSpxRecognizer::FireRecoEvent(ISpxRecognizerEvents::RecoEvent_Type* pevent, const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result, uint64_t offset)
{
    if (pevent != nullptr )
    {
        if (pevent->IsConnected())
        {
            auto factory = GetEventArgsFactory();
            auto recoEvent = (result != nullptr)
                ? factory->CreateRecognitionEventArgs(sessionId, result)
                : factory->CreateRecognitionEventArgs(sessionId, offset);
            pevent->Signal(recoEvent);
        }
        else
        {
            SPX_DBG_TRACE_VERBOSE("No listener connected to event");
        }
    }
}

void CSpxRecognizer::EnsureDefaultSession()
{
    if (m_defaultSession == nullptr)
    {
        SPX_DBG_ASSERT(GetSite());
        m_defaultSession = GetSite()->GetDefaultSession();
    }
}

void CSpxRecognizer::TermDefaultSession()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_defaultSession != nullptr)
    {
        m_defaultSession->RemoveRecognizer(this);
    }
    SpxTermAndClear(m_defaultSession);
}

std::shared_ptr<ISpxPhraseList> CSpxRecognizer::EnsureDefaultPhraseListGrammar()
{
    if (m_phraselist == nullptr)
    {
        auto phraselist = SpxCreateObjectWithSite<ISpxPhraseList>("CSpxPhraseListGrammar", this);
        phraselist->InitPhraseList(L"");

        m_phraselist = phraselist;

        AddGrammar(SpxQueryInterface<ISpxGrammar>(m_phraselist));
    }

    return m_phraselist;
}

void CSpxRecognizer::OnIsEnabledChanged()
{
    // no op currently
}

void CSpxRecognizer::CheckLogFilename()
{
    auto namedProperties = SpxQueryService<ISpxNamedProperties>(m_defaultSession);
    SpxDiagLogSetProperties(namedProperties);
}

std::shared_ptr<ISpxNamedProperties> CSpxRecognizer::GetParentProperties() const
{
    return SpxQueryService<ISpxNamedProperties>(GetSite());
}

std::shared_ptr<ISpxConnection> CSpxRecognizer::GetConnection()
{
    auto recognizerAsSite = SpxSiteFromThis(this);
    auto connection = SpxCreateObjectWithSite<ISpxConnection>("CSpxConnection", recognizerAsSite);

    auto initConnection = SpxQueryInterface<ISpxConnectionInit>(connection);
    initConnection->Init(SpxSharedPtrFromThis<ISpxRecognizer>(this), SpxSharedPtrFromThis<ISpxMessageParamFromUser>(this));

    return connection;
}

void CSpxRecognizer::SetParameter(const char *path, const char *name, const char *value)
{
    SetParameterInternal(path, name, value, m_uspParametersFromUser);
}

void CSpxRecognizer::SetRecognizerParameter(const char* path, const char* name, const char* value)
{
    SetParameterInternal(path, name, value, m_uspParametersFromRecognizer);
}

void CSpxRecognizer::SetParameterInternal(const char* path, const char* name, const char* value, std::map<std::string, CSpxStringMap>& uspParameters)
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

        auto existing = uspParameters.find(path);
        if (existing == end(uspParameters))
        {
            uspParameters[path] = { {name, value} };
        }
        else
        {
            existing->second[name] = value;
        }
    }
}

CSpxStringMap CSpxRecognizer::GetParametersFromUser(std::string&& path)
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

CSpxStringMap CSpxRecognizer::GetParametersFromRecognizer(std::string&& path)
{
    CSpxStringMap result;
    {
        std::unique_lock<std::mutex> lock{ m_uspParameterLock };

        auto search = m_uspParametersFromRecognizer.find(path);
        if (search != end(m_uspParametersFromRecognizer))
        {
            result = search->second;
        }
    }
    return result;
}

CSpxAsyncOp<bool> CSpxRecognizer::SendNetworkMessage(const char *path, std::string&& payload)
{
    if (payload.length() > MAX_JSON_PAYLOAD_FROM_USER)
    {
        ThrowInvalidArgumentException("The value for SpeechEvent exceed 50 MBytes!");
    }

    if (!ajv::json::Parse(payload).IsOk())
    {
        std::string message = "The payload of speech event is invalid";
        ThrowInvalidArgumentException(message);
    }

    SPX_DBG_TRACE_INFO("CSpxRecognizer::SendNetworkMessage path=%s, payload=%s", path, payload.c_str());
    auto defaultSession = GetDefaultSession();
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, defaultSession == nullptr);
    return defaultSession->SendNetworkMessage(path, std::move(payload));
}

CSpxAsyncOp<bool> CSpxRecognizer::SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload)
{
    auto defaultSession = GetDefaultSession();
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, defaultSession == nullptr);
    SPX_DBG_TRACE_INFO("CSpxRecognizer::SendNetworkMessage path=%s binary payload", path);
    return defaultSession->SendNetworkMessage(path, std::move(payload));
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
