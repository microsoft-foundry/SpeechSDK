//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_data_stream_shared_adapter.cpp: Implementation definitions for CSpxAudioDataStreamSharedAdapter C++ class
//

#include "stdafx.h"
#include "create_object_helpers.h"
#include "site_helpers.h"
#include "property_id_2_name_map.h"
#include "audio_data_stream_shared_adapter.h"
#include <algorithm>
#include <cstring>
#include <chrono>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxAudioDataStreamSharedAdapter::~CSpxAudioDataStreamSharedAdapter()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

void CSpxAudioDataStreamSharedAdapter::InitFromAudioDataStream(const std::shared_ptr<ISpxAudioDataStream> &stream)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_stream != nullptr);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, stream == nullptr);

    m_stream = stream;
}

std::shared_ptr<ISpxNamedProperties> CSpxAudioDataStreamSharedAdapter::GetParentProperties() const
{
    return SpxQueryInterface<ISpxNamedProperties>(m_stream);
}

StreamStatus CSpxAudioDataStreamSharedAdapter::GetStatus() const noexcept
{
    return m_stream->GetStatus();
}

void CSpxAudioDataStreamSharedAdapter::SetStatus(StreamStatus status) noexcept
{
    return m_stream->SetStatus(status);
}

CancellationReason CSpxAudioDataStreamSharedAdapter::GetCancellationReason() const noexcept
{
    return m_stream->GetCancellationReason();
}

std::shared_ptr<ISpxErrorInformation> CSpxAudioDataStreamSharedAdapter::GetError()
{
    return m_stream->GetError();
}

void CSpxAudioDataStreamSharedAdapter::SetError(const std::shared_ptr<ISpxErrorInformation> &error)
{
    return m_stream->SetError(error);
}

bool CSpxAudioDataStreamSharedAdapter::CanReadData(uint32_t requestedSize)
{
    return CanReadData(requestedSize, m_position);
}

bool CSpxAudioDataStreamSharedAdapter::CanReadData(uint32_t requestedSize, uint32_t pos)
{
    auto streamReader = SpxQueryInterface<ISpxAudioOutputReader>(m_stream);
    auto inventorySize = streamReader->AvailableSize();

    return pos <= inventorySize && requestedSize <= inventorySize - pos;
}

uint32_t CSpxAudioDataStreamSharedAdapter::GetAvailableSize()
{
    auto streamReader = SpxQueryInterface<ISpxAudioOutputReader>(m_stream);
    return streamReader->AvailableSize() - m_position;
}

void CSpxAudioDataStreamSharedAdapter::SaveToWaveFile(const char * fileName)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_fileWriterPosition = 0;
    // Re-use the CSpxWavFileWriter class to write the audio to file
    auto waveFileWriter = SpxCreateObjectWithSite<ISpxAudioFile>("CSpxWavFileWriter", GetSite());
    waveFileWriter->Open(fileName);

    auto formatInit = SpxQueryInterface<ISpxAudioStreamInitFormat>(waveFileWriter);
    auto streamFormat = SpxQueryInterface<ISpxAudioOutputFormat>(m_stream);
    auto format = streamFormat->GetFormat();
    formatInit->SetFormat(format.get());

    auto outputFormatInit = SpxQueryInterface<ISpxAudioOutputInitFormat>(waveFileWriter);
    outputFormatInit->SetHeader(streamFormat->HasHeader());

    // chunk size: 100 ms audio.
    const auto chunkSize = format->nAvgBytesPerSec / 10;
    auto buffer = SpxAllocSharedAudioBuffer(chunkSize);
    auto audioOutput = SpxQueryInterface<ISpxAudioOutput>(waveFileWriter);

    while (true)
    {
        auto dataSize = m_stream->Read(buffer.get(), chunkSize, m_fileWriterPosition);
        if (dataSize <= 0)
        {
            break;
        }
        audioOutput->Write(buffer.get(), dataSize);
        m_fileWriterPosition += dataSize;
    }

    audioOutput->Close();
}

uint32_t CSpxAudioDataStreamSharedAdapter::GetPosition()
{
    return m_position;
}

void CSpxAudioDataStreamSharedAdapter::SetPosition(uint32_t pos)
{
    m_position = pos;
}

uint32_t CSpxAudioDataStreamSharedAdapter::Read(uint8_t* buffer, uint32_t bufferSize)
{
    return Read(buffer, bufferSize, m_position);
}

uint32_t CSpxAudioDataStreamSharedAdapter::Read(uint8_t* buffer, uint32_t bufferSize, uint32_t pos)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto readSize = m_stream->Read(buffer, bufferSize, pos);
    m_position = pos + readSize;
    return readSize;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
