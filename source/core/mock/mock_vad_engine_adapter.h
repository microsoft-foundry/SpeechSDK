//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// mock_vad_engine_adapter.h: Implementation declarations for CSpxMockVadEngineAdapter C++ class
//

#pragma once
#include <memory>
#include <queue>
#include "spxcore_common.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxMockVadEngineAdapter :
    public ISpxObjectWithSiteInitImpl<ISpxDetectorEngineAdapterSite>,
    public ISpxDetectorEngineAdapter
{
public:
    CSpxMockVadEngineAdapter();
    virtual ~CSpxMockVadEngineAdapter();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxDetectorEngineAdapter)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioProcessor)
    SPX_INTERFACE_MAP_END()

     // --- ISpxObject

    void Init() override;
    void Term() override;

    // --- ISpxAudioProcessor

    void SetFormat(const SPXWAVEFORMATEX* pformat) override;
    void ProcessAudio(const DataChunkPtr& audioChunk) override;

private:

    CSpxMockVadEngineAdapter(const CSpxMockVadEngineAdapter&) = delete;
    CSpxMockVadEngineAdapter(const CSpxMockVadEngineAdapter&&) = delete;
    CSpxMockVadEngineAdapter& operator=(const CSpxMockVadEngineAdapter&) = delete;
    CSpxMockVadEngineAdapter& operator=(const CSpxMockVadEngineAdapter&&) = delete;

    bool HasFormat() { return m_format.get() != nullptr; }

    void InitFormat(const SPXWAVEFORMATEX* pformat);
    void TermFormat();
    void End();

    void FireVoiceActivityDetected();

private:
    const uint64_t ticksPerSecond = 1000 * 1000 * 10; // 1000 == to_msec, 1000 == to_usec, 10 == to_100nsec
    const uint64_t m_numMsVADDetected = 700;
    const uint64_t m_numMsVADOffset = 610;
    const uint64_t m_numMsVADDuration = 300;


    SpxWAVEFORMATEX_Type m_format;
    std::list<DataChunkPtr> m_audio;
    uint64_t m_cbAudioProcessed;
    uint64_t m_cbFireVAD;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
