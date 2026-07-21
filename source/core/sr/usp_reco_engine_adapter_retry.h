//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "named_properties.h"
#include "reco_engine_adapter_delegate_impl.h"
#include "reco_engine_adapter_site_delegate_impl.h"
#include "audio_replayer_delegate_impl.h"

#include "site_helpers.h"
#include <object_with_site_init_impl.h>
#include <chrono>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxUspRecoEngineAdapterRetry :
    public ISpxRecoEngineAdapterDelegateImpl<>,
    public ISpxRecoEngineAdapterSiteDelegateToSiteImpl<CSpxUspRecoEngineAdapterRetry>,
    public ISpxAudioReplayerSiteDelegateToSiteImpl<CSpxUspRecoEngineAdapterRetry>,
    public CSpxNamedProperties
{
private:
    using REA_Child = ISpxRecoEngineAdapterDelegateImpl<CSpxDelegateToSharedPtrHelper<ISpxRecoEngineAdapter>>;
    using REA_Site_Parent = ISpxRecoEngineAdapterSiteDelegateToSiteImpl<CSpxUspRecoEngineAdapterRetry>;
    using REA_Site_Audio_Parent = ISpxAudioReplayerSiteDelegateToSiteImpl<CSpxUspRecoEngineAdapterRetry>;

public:

    CSpxUspRecoEngineAdapterRetry() = default;
    ~CSpxUspRecoEngineAdapterRetry() = default;

    SPX_INTERFACE_MAP_BEGIN()
        using namespace USP;
        SPX_INTERFACE_MAP_ENTRY(ISpxRecoEngineAdapter)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecoEngineAdapterSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioProcessor)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioReplayer)
        SPX_INTERFACE_MAP_FUNC(CSpxNamedProperties::QueryInterface)
     SPX_INTERFACE_MAP_END()

    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxAudioReplayer)
        SPX_SERVICE_MAP_ENTRY_FUNC(CSpxNamedProperties::QueryService)
    SPX_SERVICE_MAP_END()

    void Term() override
    {
        ZombieTermAndClearRecoEngineAdapterDelegate();
    }

    void Init() override;

    void ShrinkReplayBuffer(uint64_t newBaseOffset) override;

    void SetFormat(const SPXWAVEFORMATEX* pformat) override;
    void AdapterDisconnected(std::shared_ptr<ISpxErrorInformation> payload) override;
    void Error(ISpxRecoEngineAdapter* adapter, std::shared_ptr<ISpxErrorInformation> payload) override;
    void FireAdapterResult_Intermediate(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result) override;
    void FireAdapterResult_FinalResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result) override;
    void AdapterCompletedSetFormatStop(ISpxRecoEngineAdapter* adapter) override;

    void SetStringValue(const char* name, const char* value) override;
    void SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size) override;

protected:
    void InitDelegatePtr(std::shared_ptr<ISpxRecoEngineAdapter>& ptr) override;

    using REA_Site_Parent::InitDelegatePtr;
    using REA_Site_Audio_Parent::InitDelegatePtr;

private:
    bool ShouldReconnect(const std::shared_ptr<ISpxErrorInformation>& payload);
    void StartReconnect(const std::shared_ptr<ISpxErrorInformation>& payload);
    void CleanupAdapterAndAudio(const std::shared_ptr<ISpxErrorInformation>& payload, bool pendingReconnect);
    std::shared_ptr<ISpxRecognitionResult> DiscardAudioUnderTransportErrors();
    std::shared_ptr<ISpxRecognitionResult> CreateFakeFinalResult(const std::shared_ptr<ISpxRecognitionResult>& intermediate);

    bool m_compressedPassThrough = false;
    SpxWAVEFORMATEX_Type m_currentFormat = nullptr;
    bool m_retryConnectionFailures = true;
    bool m_pendingReconnect = false;
    uint32_t m_numMaxRetries = 4;
    uint32_t m_retriesDone = 0;
    std::chrono::milliseconds m_retryDurationMS{ 250 };
    
    std::shared_ptr<ISpxRecognitionResult> m_mostRecentIntermediateRecoResult;
};

}}}} // Microsoft::CognitiveServices::Speech::Impl
