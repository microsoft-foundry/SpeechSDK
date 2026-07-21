//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// recognizer.h: Implementation declarations for CSpxRecognizer C++ class
//

#pragma once
#include <atomic>
#include <future>
#include <memory>
#include "asyncop.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"
#include "service_helpers.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxRecognizer :
    public ISpxRecognizer,
    public ISpxRecognizerEvents,
    public ISpxServiceProvider,
    public ISpxSessionFromRecognizer,
    public ISpxObjectWithSiteInitImpl<ISpxRecognizerSite>,
    public ISpxPropertyBagImpl,
    public ISpxGrammarList,
    public ISpxGrammar,
    public ISpxMessageParamFromUser,
    public ISpxGetUspMessageParamsFromUser,
    public ISpxGetUspMessageParamsFromRecognizer,
    public ISpxConnectionFromRecognizer,
    public ISpxGenericSite
{
public:

    CSpxRecognizer();
    virtual ~CSpxRecognizer();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxSessionFromRecognizer)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecognizerEvents)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecognizer)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
        SPX_INTERFACE_MAP_ENTRY(ISpxGrammarList)
        SPX_INTERFACE_MAP_ENTRY(ISpxGrammar)
        SPX_INTERFACE_MAP_ENTRY(ISpxMessageParamFromUser)
        SPX_INTERFACE_MAP_ENTRY(ISpxGetUspMessageParamsFromUser)
        SPX_INTERFACE_MAP_ENTRY(ISpxGetUspMessageParamsFromRecognizer)
        SPX_INTERFACE_MAP_ENTRY(ISpxConnectionFromRecognizer)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
    SPX_INTERFACE_MAP_END()

    // --- ISpxObjectWithSiteInit
    void Init() override;
    void Term() override;

    // --- ISpxNamedProperties (overrides)
    void SetStringValue(const char* name, const char* value) override;
    void SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size) override;

    // --- ISpxPropertyBagImpl (overrides)
    std::shared_ptr<ISpxNamedProperties> GetParentProperties() const override;

    // --- ISpxGrammarList
    std::shared_ptr<ISpxGrammar> GetPhraseListGrammar(const wchar_t* name) override;
    void AddGrammar(std::shared_ptr<ISpxGrammar> grammar) override;
    void SetRecognitionFactor(double factor) override;

    // --- ISpxGrammar
    std::list<std::string> GetListenForList() override;

    // --- ISpxRecognizer
    void SetDisposing() override;

    CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> RecognizeAsync() override;
    CSpxAsyncOp<void> StartContinuousRecognitionAsync() override;
    CSpxAsyncOp<void> StopContinuousRecognitionAsync() override;

    CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> RecognizeAsync(std::shared_ptr<ISpxKwsModel> model) final;
    CSpxAsyncOp<void> StartKeywordRecognitionAsync(std::shared_ptr<ISpxKwsModel> model) override;
    CSpxAsyncOp<void> StopKeywordRecognitionAsync() override;

    void OpenConnection(bool forContinuousRecognition) override;
    void CloseConnection() override;

    // --- ISpxSessionFromRecognizer
    std::shared_ptr<ISpxSession> GetDefaultSession() override;

    // --- ISpxRecognizerEvents
    void FireSessionStarted(const std::wstring& sessionId) override;
    void FireSessionStopped(const std::wstring& sessionId) override;

    void FireConnected(const std::wstring& sessionId) override;
    void FireDisconnected(const std::wstring& sessionId) override;
    void FireConnectionMessageReceived(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary) override;

    void FireSpeechStartDetected(const std::wstring& sessionId, uint64_t offset) override;
    void FireSpeechEndDetected(const std::wstring& sessionId, uint64_t offset) override;

    void FireResultEvent(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result) override;
    void FireTokenRequest(const std::wstring& sessionId) override;

    // --- IServiceProvider
    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxNamedProperties)
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()

    // --- ISpxConnectionFromRecognizer
    std::shared_ptr<ISpxConnection> GetConnection() override;

    // --- ISpxSetUspMessageParamFromUser
    void SetParameter(const char *path, const char *name, const char *value) override;
    CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::string&& payload) override;
    CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload) override;

    // --- ISpxGetUspMessageParamsFromUser
    CSpxStringMap GetParametersFromUser(std::string&& path) override;

    // --- ISpxGetUspMessageParamsFromRecognizer
    CSpxStringMap GetParametersFromRecognizer(std::string&& path) override;

protected:

    void EnsureDefaultSession();
    void TermDefaultSession();

    std::shared_ptr<ISpxPhraseList> EnsureDefaultPhraseListGrammar();

    void OnIsEnabledChanged();

    void CheckLogFilename();

    void SetRecognizerParameter(const char* path, const char* name, const char* value);
    void SetParameterInternal(const char* path, const char* name, const char* value, std::map<std::string, CSpxStringMap>& uspParameters);

protected:
    std::shared_ptr<ISpxSession> m_defaultSession;

private:

    CSpxRecognizer(const CSpxRecognizer&) = delete;
    CSpxRecognizer(const CSpxRecognizer&&) = delete;

    CSpxRecognizer& operator=(const CSpxRecognizer&) = delete;

    // Retain a separate pointer to the phrase list for now since it's
    // a singleton wrt the recognizer.
    std::shared_ptr<ISpxPhraseList> m_phraselist;

    std::list<std::shared_ptr<ISpxGrammar>> m_grammarlist;

    void SetStringValueInProperties(const char* name, const char* value);

    void FireRecoEvent(ISpxRecognizerEvents::RecoEvent_Type* pevent, const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result, uint64_t offset = 0);
    std::shared_ptr<ISpxEventArgsFactory> GetEventArgsFactory();
    std::shared_ptr<ISpxSessionEventArgs> CreateSessionEventArgs(const std::wstring& sessionId);
    std::shared_ptr<ISpxConnectionEventArgs> CreateConnectionEventArgs(const std::wstring& sessionId);
    std::shared_ptr<ISpxSessionEventArgs> CreateTokenRequestEventArgs(const std::wstring& sessionId);

    std::map<std::string, CSpxStringMap> m_uspParametersFromUser;
    std::map<std::string, CSpxStringMap> m_uspParametersFromRecognizer;
    std::mutex m_uspParameterLock;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
