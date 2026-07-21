//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_pump.h: Implementation declarations for CSpxAudioPump C++ class
//

#pragma once
#include "spxcore_common.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "service_helpers.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


class CSpxAudioPump
    : public ISpxAudioPump,
      public ISpxAudioPumpInit,
      public ISpxObjectWithSiteInitImpl<ISpxAudioPumpSite>,
      public ISpxGenericSite,
      public ISpxServiceProvider
{
public:

    CSpxAudioPump();
    ~CSpxAudioPump();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioPumpInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioPump)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
    SPX_INTERFACE_MAP_END()

    // --- IServiceProvider
    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()

    // --- ISpxAudioPumpInit

    void SetReader(std::shared_ptr<ISpxAudioStreamReader> reader) override;

    // --- ISpxAudioPump

    uint16_t GetFormat(SPXWAVEFORMATEX* pformat, uint16_t cbFormat) const override;
    void SetFormat(const SPXWAVEFORMATEX* pformat, uint16_t cbFormat) override;

    void StartPump(std::shared_ptr<ISpxAudioProcessor> pISpxAudioProcessor) override;
    void PausePump() override;
    void StopPump() override;

    State GetState() const override;

    virtual std::string GetPropertyValue(const std::string& key) const override;

private:
    using SitePtr = std::shared_ptr<ISpxAudioPumpSite>;

    CSpxAudioPump(const CSpxAudioPump&) = delete;
    CSpxAudioPump(const CSpxAudioPump&&) = delete;

    CSpxAudioPump& operator=(const CSpxAudioPump&) = delete;
    CSpxAudioPump& operator=(const CSpxAudioPump&&) = delete;

    void PumpThread(std::shared_ptr<ISpxAudioPump> keepAlive, std::shared_ptr<ISpxAudioProcessor> pISpxAudioProcessor);
    void WaitForPumpStart(std::unique_lock<std::mutex>& lock);
    void WaitForPumpIdle(std::unique_lock<std::mutex>& lock);

    void InitializeMasAudioProcessor();

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;

    std::shared_ptr<ISpxAudioStreamReader> m_reader;

    State m_state;
    State m_stateRequested;

    // Set timeouts to double the frame size in milliseconds
    const int m_waitMsStartPumpRequestTimeout = 200;
    const int m_waitMsStopPumpRequestTimeout = 200;

    std::thread m_thread;

    std::shared_ptr<ISpxAudioProcessor> m_masAudioProcessor;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
