//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// usp_tts_engine_adapter.h: Implementation declarations for CSpxUspTtsEngineAdapter C++ class
//

#pragma once
#include <memory>
#include <queue>
#include "asyncop.h"
#include "spxcore_common.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"
#include "usp_text_message.h"
#include "usp.h"
#include "service_helpers.h"
#include <object_with_site_init_impl.h>
#include "cloud_tts_engine_adapter.h"
#include "interfaces/enum_helpers.h"
#include "interfaces/ispx_usp_connection.h"
#include "ispx_telemetry_store_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

enum class UspState {
    Error = -1,
    Idle = 0,
    Connecting = 1,     // from trying to connect to request sent
    Sending = 2,
    TurnStarted = 3,    // from turn.start received to first audio message received
    ReceivingData = 4   // from first audio message received to turn.end received
};

/// <summary>
/// Converts an enum value to its string representation
/// </summary>
/// <param name="value">The value to convert</param>
/// <returns>The corresponding string for that value</returns>
template<>
const char* EnumHelpers::ToString<UspState>(UspState state);

class CSpxUspTtsEngineAdapter :
    public ISpxGenericSite,
    public ISpxServiceProvider,
    public USP::ISpxUspCallbacks,
    public CSpxCloudTtsEngineAdapter,
    public ISpxTelemetryStoreImpl,
    public ISpxMessageParamFromUser,
    public ISpxGetUspMessageParamsFromUser
{
public:

    CSpxUspTtsEngineAdapter();
    virtual ~CSpxUspTtsEngineAdapter();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxTelemetryStore)
        using namespace USP;
        SPX_INTERFACE_MAP_ENTRY(ISpxUspCallbacks)
        SPX_INTERFACE_MAP_ENTRY(ISpxTtsEngineAdapter)
        SPX_INTERFACE_MAP_ENTRY(ISpxMessageParamFromUser)
        SPX_INTERFACE_MAP_ENTRY(ISpxGetUspMessageParamsFromUser)
    SPX_INTERFACE_MAP_END()

    // --- ISpxObjectInit ---
    void Init() override;
    void Term() override;

    // --- ISpxTtsEngineAdapter ---
    std::shared_ptr<ISpxSynthesisResult> Speak(const std::string& text, bool isSsml, const std::string& requestId, bool retry) override;
    std::shared_ptr<ISpxSynthesisResult> Speak(std::shared_ptr<ISpxSynthesisRequestReader> request, bool retry) override;
    void StopSpeaking(const std::shared_ptr<ISpxErrorInformation> &reason = nullptr) override;
    void Connect() override;
    void Disconnect(bool async = false) override;

    // --- ISpxUspMessageParamFromUser ---
    void SetParameter(const char *path, const char *name, const char *value) override;
    CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::string&& payload) override;
    CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload) override;

    // --- ISpxGetUspMessageParamsFromUser
    CSpxStringMap GetParametersFromUser(std::string&& path) override;

    // --- IServiceProvider ---
    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxTelemetryStore)
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()


private:
    using SitePtr = std::shared_ptr<ISpxTtsEngineAdapterSite>;

    CSpxUspTtsEngineAdapter(const CSpxUspTtsEngineAdapter&) = delete;
    CSpxUspTtsEngineAdapter(const CSpxUspTtsEngineAdapter&&) = delete;

    CSpxUspTtsEngineAdapter& operator=(const CSpxUspTtsEngineAdapter&) = delete;

    void GetProxySetting();

    std::shared_ptr<ISpxSynthesisResult> SpeakInternal(std::shared_ptr<ISpxSynthesisRequestReader> request, const std::string& requestId);

    void SetSpeechConfigMessage();
    void UspSendSpeechConfig();
    void UspSendSynthesisContext(std::shared_ptr<ISpxSynthesisRequestReader> request, const std::string& requestId);
    void UspSendSsml(const std::string& ssml, const std::string& requestId);
    void UspSendTextPieces(std::shared_ptr<ISpxSynthesisRequestReader> request);
    void UspSendMessage(std::unique_ptr<USP::TextMessage> message);
    static void DoSendMessageWork(ISpxUspConnection::WkPtr connectionPtr, std::unique_ptr<USP::TextMessage> message);

    void UspInitialize();

    USP::ClientConfiguration& SetUspEndpoint(const std::shared_ptr<ISpxNamedProperties>& properties, USP::ClientConfiguration& client) const;
    USP::ClientConfiguration& SetUspCrlProperties(const std::shared_ptr<ISpxNamedProperties>& properties, USP::ClientConfiguration& client) const;

    void OnTurnStart(const USP::TurnStartMsg& message) override;
    void OnAudioOutputChunk(const USP::AudioOutputChunkMsg& message) override;
    void OnAudioOutputMetadata(const USP::AudioOutputMetadataMsg& message) override;
    void OnTurnEnd(const USP::TurnEndMsg& message) override;
    void OnError(const std::shared_ptr<ISpxErrorInformation>& error) override;
    void OnConnected(const std::string& url) override;
    void OnDisconnected(const std::shared_ptr<ISpxErrorInformation>&) override;

    bool WordBoundaryEnabled() const;
    static bool InSsmlTag(size_t currentPos, const std::u32string& ssml, size_t beginningPos);
    static bool IsXmlTag(const std::string& word);

    std::shared_ptr<ISpxSynthesisResult> CreateCancelledResult(const std::string& requestId) const;
    size_t AudioLengthOfCurrentTurn() const;

private:

    std::string m_proxyHost;
    int m_proxyPort { 0 };
    std::string m_proxyUsername;
    std::string m_proxyPassword;

    std::shared_ptr<ISpxThreadService> m_threadService;
    std::shared_ptr<ISpxUspCallbacks> m_uspCallbacks;
    std::shared_ptr<ISpxUspConnection> m_uspConnection;
    std::chrono::system_clock::time_point m_lastConnectTime;

    std::string m_speechConfig;

    std::atomic<UspState> m_uspState { UspState::Idle };
    std::atomic<bool> m_shouldStop{ false };

    std::string m_originalRequestId;
    std::string m_currentRequestId;
    std::u32string m_currentText;
    bool m_currentTextIsSsml{};
    int m_currentWordOffset{};
    int m_currentSentenceOffset{};
    std::string m_partialVisemeAnimation{};

    std::shared_ptr<ISpxErrorInformation> m_currentError;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::recursive_mutex m_connectionMutex;

    std::map<std::string, CSpxStringMap> m_uspParametersFromUser;
    std::mutex m_uspParameterLock;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
