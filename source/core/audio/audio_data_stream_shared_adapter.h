//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_data_stream_shared_adapter.h: Implementation definitions for CSpxAudioDataStreamSharedAdapter C++ class
//

// This audio data stream shared adapter works with core audio data stream (CSpxAudioDataStream) for the scenario
// where we want to write data once and read data in many streams. The data is only copied once and stored in the
// core stream, and the shared adapter holds the read position.
// For one synthesis result, there could be many audio data streams. With this shared adapter, we can create one
// core audio data stream and many shared adapers for binding layer audio data stream objects.

#pragma once
#include "stdafx.h"
#include "interface_helpers.h"
#include "service_helpers.h"
#include "property_bag_impl.h"
#include "pull_audio_output_stream.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


class CSpxAudioDataStreamSharedAdapter :
    public ISpxObjectWithSiteInitImpl<ISpxGenericSite>,
    public ISpxAudioDataStream,
    public ISpxAudioDataStreamSharedAdapterInit,
    public ISpxPropertyBagImpl
{
public:

    CSpxAudioDataStreamSharedAdapter() = default;
    ~CSpxAudioDataStreamSharedAdapter();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioDataStream)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioDataStreamSharedAdapterInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()

    // --- ISpxAudioDataStream ---
    StreamStatus GetStatus() const noexcept final;
    void SetStatus(StreamStatus status) noexcept final;
    CancellationReason GetCancellationReason() const noexcept final;
    std::shared_ptr<ISpxErrorInformation> GetError() override;
    void SetError(const std::shared_ptr<ISpxErrorInformation> &error) override;
    bool CanReadData(uint32_t requestedSize) override;
    bool CanReadData(uint32_t requestedSize, uint32_t pos) override;
    uint32_t Read(uint8_t* buffer, uint32_t bufferSize) override;
    uint32_t Read(uint8_t* buffer, uint32_t bufferSize, uint32_t pos) override;
    void SaveToWaveFile(const char * fileName) override;
    uint32_t GetPosition() override;
    void SetPosition(uint32_t pos) override;
    uint32_t GetAvailableSize() override;

    // --- ISpxAudioDataStreamSharedAdapterInit ---
    void InitFromAudioDataStream(const std::shared_ptr<ISpxAudioDataStream> &stream) override;

protected:

    // --- ISpxNamedProperties ---
    std::shared_ptr<ISpxNamedProperties> GetParentProperties() const override;

private:

    DISABLE_COPY_AND_MOVE(CSpxAudioDataStreamSharedAdapter);

    std::shared_ptr<ISpxAudioDataStream> m_stream;

    uint32_t m_position = 0;
    uint32_t m_fileWriterPosition = 0;
    std::mutex m_mutex;
    std::mutex m_fileWriterMutex;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
