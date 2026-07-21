//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "named_properties.h"
#include "reco_engine_adapter_delegate_impl.h"
#include "reco_engine_adapter_site_delegate_impl.h"
#include "audio_replayer_delegate_impl.h"
#include "reco_engine_adapter_helpers.h"
#include "create_object_helpers.h"

#include "site_helpers.h"
#include <object_with_site_init_impl.h>
#include "named_properties_with_site_impl.h"
#include <chrono>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

// In the relationship between the CSpxAudioStreamSession and ISpxRecoEngineAdapter's and ISpxDetectorEngineAdapter's that live beneath it, 
// those adapters typically believe that they start with audio at the "zero" audio position (in both ticks and bytes). This causes a problem
// with audio offset recording and reporting in several scenarios, such as, when the Session switches back and forth between two different
// adapters (like switching from KWS to USP or vice versa). 
//
// To facilitate fixing this issue, instead of introducing the concept of "offset fixup" to every instance of Adapter, and/or to keep complex
// state for each adapter inside the Session related to the fixup's required, we can instead use the CSpxRecoEngineAdapterOffsetFixupWrapper
// class as a wrapper of each reco engine adapter kind (like USP and RNNT). This "wrapper" will be able to intercept each call to and from 
// the underlying "wrapped" Adapter, allowing it to keep track of the offset fixup's required, as well as to apply those fixups as needed.

template <typename T>
class CSpxRecoEngineAdapterOffsetFixupWrapper :
    public ISpxRecoEngineAdapterDelegateImpl<>,
    public ISpxRecoEngineAdapterSiteDelegateToSiteImpl<CSpxRecoEngineAdapterOffsetFixupWrapper<T>>,
    public ISpxAudioReplayerSiteDelegateToSiteImpl<CSpxRecoEngineAdapterOffsetFixupWrapper<T>>,
    public ISpxObjectWithSiteInitImpl<ISpxGenericSite>,
    public ISpxNamedPropertiesWithSiteImpl<CSpxRecoEngineAdapterOffsetFixupWrapper<T>>,
    public ISpxServiceProvider,
    public ISpxGenericSite
{
private:
    using REA_Child = ISpxRecoEngineAdapterDelegateImpl<CSpxDelegateToSharedPtrHelper<ISpxRecoEngineAdapter>>;
    using REA_Site_Parent = ISpxRecoEngineAdapterSiteDelegateToSiteImpl<CSpxRecoEngineAdapterOffsetFixupWrapper<T>>;
    using REA_Site_Audio_Parent = ISpxAudioReplayerSiteDelegateToSiteImpl<CSpxRecoEngineAdapterOffsetFixupWrapper<T>>;

public:

    CSpxRecoEngineAdapterOffsetFixupWrapper() = default;
    ~CSpxRecoEngineAdapterOffsetFixupWrapper() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecoEngineAdapter)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecoEngineAdapterSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioProcessor)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioReplayer)
     SPX_INTERFACE_MAP_END()

    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxAudioReplayer)
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()

    // --- ISpxObjectWithSiteInitImpl overrides
    void Init() override
    {
        REA_Site_Audio_Parent::GetMultiChannelProcessingMode(&m_useMultiChannelProcessing);
    }
    void Term() override
    {
        ZombieTermAndClearRecoEngineAdapterDelegate();
    }

    // --- ISpxRecoEngineAdapterSite overrides
    void AdapterStartedTurn(ISpxRecoEngineAdapter* adapter, const std::string& id, OffsetType adapterStartOffset) override
    {
        // When called on AdapterStartedTurn(), we'll obtain the current audio continuation offset,
        // allowing us to fixup audio offsets in various other calls to/from the "wrapped" adapter.
        // We subtract from the current global audio continuation offset the start offset of the adapter
        // in case the adapter (See: USP Adapter in header mode) is already doing any fixup of results.

        uint64_t currentOffsetInTicks = 0;
        REA_Site_Audio_Parent::GetCurrentAudioContinuationOffset(&currentOffsetInTicks);

        if (adapterStartOffset < 0 || (static_cast<uint64_t>(adapterStartOffset) >= currentOffsetInTicks) || m_useMultiChannelProcessing)
        {
            currentOffsetInTicks = 0;
        }
        else
        {
            currentOffsetInTicks -= static_cast<uint64_t>(adapterStartOffset);
        }
        
        m_fixUpInTicks = currentOffsetInTicks;
        SPX_DBG_TRACE_VERBOSE("%s: m_fixUpInTicks %" PRIu64 " adapterStartOffset %" PRIu64, __FUNCTION__, m_fixUpInTicks, adapterStartOffset);

        REA_Site_Parent::DelegateAdapterStartedTurn(adapter, id, adapterStartOffset);
    }

    void FireAdapterResult_Intermediate(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result) override
    {
        // Apply fixup to offset, and delegate the call to the site object above us
        result->SetOffset(result->GetOffset() + m_fixUpInTicks);
        CSpxRecoEngineAdapterHelpers::UpdateServiceResponseJsonResult(result, m_fixUpInTicks);
        REA_Site_Parent::DelegateFireAdapterResult_Intermediate(offset + m_fixUpInTicks, result);
    }

    void FireAdapterResult_FinalResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result) override
    {
        // Apply fixup to offset, and delegate the call to the site object above us
        result->SetOffset(result->GetOffset() + m_fixUpInTicks);
        CSpxRecoEngineAdapterHelpers::UpdateServiceResponseJsonResult(result, m_fixUpInTicks);
        REA_Site_Parent::DelegateFireAdapterResult_FinalResult(offset + m_fixUpInTicks, result);
    }

protected:

    void InitDelegatePtr(std::shared_ptr<ISpxRecoEngineAdapter>& ptr) override
    {
        // Create the "wrapped" adapter, using ourselves as the "site", which enables us
        // to intercept all calls to and from the Adapter so we can coordinate the audio
        // offset fixups required.

        ptr = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>(T::ClassToWrap.c_str(), this);
    }

    using REA_Site_Parent::InitDelegatePtr;
    using REA_Site_Audio_Parent::InitDelegatePtr;

private:

    uint64_t m_fixUpInTicks;
    bool m_useMultiChannelProcessing = false;
};

// For each "wrapped" adapter, use this pattern to facilitate wrapping it. To find an example of this throughout the codebase
// search globally across *.h and *.cpp files for "CSpxUspRecoEngineAdapterRetry|CSpxUspRecoEngineAdapterRetry_OffsetFixupWrapper"

struct CSpxUspRecoEngineAdapterRetry_OffsetWrapperTraits { static const std::string ClassToWrap; };
const std::string CSpxUspRecoEngineAdapterRetry_OffsetWrapperTraits::ClassToWrap = "CSpxUspRecoEngineAdapterRetry";
class CSpxUspRecoEngineAdapterRetry_OffsetFixupWrapper : public CSpxRecoEngineAdapterOffsetFixupWrapper<CSpxUspRecoEngineAdapterRetry_OffsetWrapperTraits> { };

}}}} // Microsoft::CognitiveServices::Speech::Impl
