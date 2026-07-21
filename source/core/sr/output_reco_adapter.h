//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <memory>
#include <mutex>
#include <condition_variable>

#include "spxcore_common.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"
#include <object_with_site_init_impl.h>
#include <interface_delegate_helpers.h>
#include <function_helpers.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxOutputRecoEngineAdapter final:
    public ISpxObjectWithSiteInitImpl<ISpxRecoEngineAdapterSite>,
    public ISpxPropertyBagImpl,
    public ISpxGenericSite,
    public ISpxRecoEngineAdapter,
    public ISpxAudioProcessorMinInput,
    public ISpxRetrievable,
    public ISpxAudioDataStream,
    public ISpxAudioDataStreamWrapper
{
public:
    using tick = std::chrono::duration<uint64_t, std::ratio<1, 10000000>>;

    CSpxOutputRecoEngineAdapter()
    {
        SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);
    }
    ~CSpxOutputRecoEngineAdapter()
    {
        SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p", __FUNCTION__, (void*)this);
    }

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecoEngineAdapter)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioProcessor)
        SPX_INTERFACE_MAP_ENTRY(ISpxRetrievable)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioProcessorMinInput)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioDataStream)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioDataStreamWrapper)
    SPX_INTERFACE_MAP_END()

    // --- ISpxObject
    void Init() final {};
    void Term() final {};

    // --- ISpxRecoEngineAdapter
    void SetAdapterMode(bool singleShot) final;

    // --- ISpxAudioProcessor
    /* For tomorrow... SetFormat should init the stream, term should let the site know that we are done */
    void SetFormat(const SPXWAVEFORMATEX* format) final;
    void ProcessAudio(const DataChunkPtr& audioChunk) final;

    void SetMinInputSize(const uint64_t sizeInTicks) final;

    // -- ISpxRetrievable
    void MarkAsRetrieved() noexcept final
    {
        m_retrieved = true;
    }

    bool WasRetrieved() const noexcept final
    {
        return m_retrieved;
    }

    inline StreamStatus GetStatus() const noexcept final
    {
        return m_stream ? m_stream->GetStatus() : StreamStatus::NoData;
    }

    inline void SetStatus(StreamStatus status) noexcept final
    {
        SPX_IFTRUE(m_stream, m_stream->SetStatus(status));
    }

    inline CancellationReason GetCancellationReason() const noexcept final
    {
        return m_stream ? m_stream->GetCancellationReason() : CancellationReason::Error;
    }

    inline std::shared_ptr<ISpxErrorInformation> GetError() final
    {
        return m_stream ? m_stream->GetError() : nullptr;
    }

    inline void SetError(const std::shared_ptr<ISpxErrorInformation> &error) final
    {
        SPX_IFTRUE(m_stream, m_stream->SetError(error));
    }

    inline bool CanReadData(uint32_t requestedSize) final
    {
        return m_stream ? m_stream->CanReadData(requestedSize) : false;
    }

    inline bool CanReadData(uint32_t requestedSize, uint32_t pos) final
    {
        return m_stream ? m_stream->CanReadData(requestedSize, pos) : false;
    }

    inline uint32_t GetAvailableSize() final
    {
        return m_stream ? m_stream->GetAvailableSize() : 0;
    }

    inline uint32_t Read(uint8_t* buffer, uint32_t bufferSize) final
    {
        return m_stream ? m_stream->Read(buffer, bufferSize) : 0;
    }

    inline uint32_t Read(uint8_t* buffer, uint32_t bufferSize, uint32_t pos) final
    {
        return m_stream ? m_stream->Read(buffer, bufferSize, pos) : 0;
    }

    inline void SaveToWaveFile(const char * fileName) final
    {
        SPX_IFTRUE(m_stream, m_stream->SaveToWaveFile(fileName));
    }

    inline uint32_t GetPosition() final
    {
        return m_stream ? m_stream->GetPosition() : 0;
    }

    inline void SetPosition(uint32_t pos) final
    {
        SPX_IFTRUE(m_stream, m_stream->SetPosition(pos));
    }

    // -- ISpxAudioDataStreamWrapper
    void DetachInput() final;

    // --- ISpxNamedProperties
    void SetStringValue(const char* name, const char* value) override;

private:
    inline void UpdateStatus(StreamStatus status)
    {
        std::lock_guard<std::mutex> lk{ m_stateMutex };
        if (m_stream)
        {
            SetStatus(status);
            m_cv.notify_all();
        }
    }

    inline void WaitForStatus(StreamStatus status) const
    {
        std::unique_lock<std::mutex> lk{ m_stateMutex };

        m_cv.wait(lk, [&]()
        {
            if (m_stream)
            {
                // If stream has already AllData and asking PartialData, do not wait
                if (status == StreamStatus::PartialData && (m_stream->GetStatus() == StreamStatus::AllData))
                {
                    return true;
                }
                else
                {
                    return m_stream->GetStatus() == status;
                }
            }
            else // Wait that stream gets created
            {
                return false;
            }
        });
    }

    using SitePtr = std::shared_ptr<ISpxRecoEngineAdapterSite>;

    std::shared_ptr<ISpxAudioDataStream> m_stream;
    std::shared_ptr<ISpxAudioOutput> m_sink;

    std::atomic<bool> m_detaching{ false };
    bool m_retrieved{ false };
    uint32_t m_bytesPerSecond{ 1 };
    uint64_t m_size{ 0 };
    uint64_t m_expectedInTicks{ 0 };
    uint64_t m_minInputSizeTicks{ 0 };
    mutable std::condition_variable m_cv;
    mutable std::mutex m_stateMutex;

};

} } } }
