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

class CSpxHybridRecoEngineAdapter :
    public ISpxRecoEngineAdapterDelegateImpl<>,
    public ISpxRecoEngineAdapterSiteDelegateToSiteImpl<CSpxHybridRecoEngineAdapter>,
    public ISpxAudioReplayerSiteDelegateToSiteImpl<CSpxHybridRecoEngineAdapter>,
    public CSpxNamedProperties
{
private:
    using REA_Child = ISpxRecoEngineAdapterDelegateImpl<CSpxDelegateToSharedPtrHelper<ISpxRecoEngineAdapter>>;
    using REA_Site_Parent = ISpxRecoEngineAdapterSiteDelegateToSiteImpl<CSpxHybridRecoEngineAdapter>;
    using REA_Site_Audio_Parent = ISpxAudioReplayerSiteDelegateToSiteImpl<CSpxHybridRecoEngineAdapter>;

public:

    CSpxHybridRecoEngineAdapter();
    ~CSpxHybridRecoEngineAdapter();

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
    void GetCurrentAudioBufferOffset(uint64_t* offsetInTicks, uint64_t* offsetInBytes) override;
    void GetCurrentAudioContinuationOffset(uint64_t* offsetInTicks) override;
    void GetMultiChannelProcessingMode(bool* useMultiChannelProcessing) override;

    void SetAdapterMode(bool singleShot) override;
    void SetFormat(const SPXWAVEFORMATEX* pformat) override;
    void AdapterConnected(const std::string& url) override;
    void AdapterDisconnected(std::shared_ptr<ISpxErrorInformation> payload) override;
    void Error(ISpxRecoEngineAdapter* adapter, std::shared_ptr<ISpxErrorInformation> payload) override;
    void FireAdapterResult_Intermediate(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result) override;
    void FireAdapterResult_FinalResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result) override;
    void AdapterCompletedSetFormatStop(ISpxRecoEngineAdapter* adapter) override;
    void SetStringValue(const char* name, const char* value) override;

protected:
    void InitDelegatePtr(std::shared_ptr<ISpxRecoEngineAdapter>& ptr) override;

    using REA_Site_Parent::InitDelegatePtr;
    using REA_Site_Audio_Parent::InitDelegatePtr;

private:
    bool ShouldReconnect(const std::shared_ptr<ISpxErrorInformation>& payload);
    void StartReconnect(const std::shared_ptr<ISpxErrorInformation>& payload);
    std::shared_ptr<ISpxRecognitionResult> DiscardAudioUnderTransportErrors();
    std::shared_ptr<ISpxRecognitionResult> CreateFakeFinalResult(const std::shared_ptr<ISpxRecognitionResult>& intermediate);

    bool m_singleShot = false;
    bool m_compressedPassThrough = false;
    SpxWAVEFORMATEX_Type m_currentFormat = nullptr;
    bool m_pendingReconnect = false;
    int32_t m_numMaxRetries = 4;
    int32_t m_retriesDone = 0;
    std::chrono::milliseconds m_retryDurationMS{ 250 };

    std::shared_ptr<ISpxRecognitionResult> m_mostRecentIntermediateRecoResult;

    bool m_checkingUspConnection = false;
    std::chrono::milliseconds m_checkingUspConnectionInterval{ 3000 };
    std::shared_ptr<ISpxRecoEngineAdapter> m_probeUspRecoEngineAdapter = nullptr;

    bool m_uspRetryAllowed = true;
};

}}}} // Microsoft::CognitiveServices::Speech::Impl
