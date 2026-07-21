//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_data_stream.cpp: Implementation definitions for CSpxAudioDataStream C++ class
//

#include "stdafx.h"
#include "create_object_helpers.h"
#include "site_helpers.h"
#include "property_id_2_name_map.h"
#include "audio_data_stream.h"
#include <algorithm>
#include <cstring>
#include <chrono>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxAudioDataStream::~CSpxAudioDataStream()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

void CSpxAudioDataStream::InitFromFile(const char* fileName)
{
    // open the audio file using the wav reader
    auto audioFile = SpxCreateObjectWithSite<ISpxAudioFile>("CSpxWavFileReader", GetSite());
    audioFile->Open(fileName);

    // get the reader interface, and the format
    auto reader = SpxQueryInterface<ISpxAudioStreamReader>(audioFile);
    auto formatSize = reader->GetFormat(nullptr, 0);
    auto format = SpxAllocWAVEFORMATEX(formatSize);
    reader->GetFormat(format.get(), formatSize);

    // loop thru the audio data, 1 second at a time, copying buffers from the reader
    auto readSize = format->nAvgBytesPerSec;
    auto buffer = SpxAllocSharedUint8Buffer(readSize);
    while (readSize > 0)
    {
        readSize = reader->Read(buffer.get(), readSize);
        this->Write(buffer.get(), readSize);
    }

    // mark that we're done reading ...
    m_writingEnded = true;
}

void CSpxAudioDataStream::InitFromFormat(const SPXWAVEFORMATEX* format, bool hasHeader)
{
    SetFormat(format);
    SetHeader(hasHeader);
    m_status = StreamStatus::NoData;
}

StreamStatus CSpxAudioDataStream::GetStatus() const noexcept
{
    return m_status;
}

void CSpxAudioDataStream::SetStatus(StreamStatus status) noexcept
{
    m_status = status;
}

CancellationReason CSpxAudioDataStream::GetCancellationReason() const noexcept
{
    return m_error == nullptr ? CancellationReason::EndOfStream : m_error->GetCancellationReason();
}

std::shared_ptr<ISpxErrorInformation> CSpxAudioDataStream::GetError()
{
    return m_error;
}

void CSpxAudioDataStream::SetError(const std::shared_ptr<ISpxErrorInformation> &error)
{
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, error == nullptr);
    m_error = error;
    m_status = StreamStatus::Canceled;
    Set(PropertyId::CancellationDetails_ReasonDetailedText, error->GetDetails().c_str());
    SignalEndOfWriting();
}

bool CSpxAudioDataStream::CanReadData(uint32_t requestedSize)
{
    return requestedSize <= m_inventorySize - m_position;
}

bool CSpxAudioDataStream::CanReadData(uint32_t requestedSize, uint32_t pos)
{
    return pos <= m_inventorySize && requestedSize <= m_inventorySize - pos;
}

uint32_t CSpxAudioDataStream::GetAvailableSize()
{
    return m_inventorySize - m_position;
}

void CSpxAudioDataStream::SaveToWaveFile(const char * fileName)
{
    // Re-use the CSpxWavFileWriter class to write the audio to file
    auto waveFileWriter = SpxCreateObjectWithSite<ISpxAudioFile>("CSpxWavFileWriter", SpxGetRootSite());
    waveFileWriter->Open(fileName);

    auto formatInit = SpxQueryInterface<ISpxAudioStreamInitFormat>(waveFileWriter);
    formatInit->SetFormat(m_format.get());

    auto outputFormatInit = SpxQueryInterface<ISpxAudioOutputInitFormat>(waveFileWriter);
    outputFormatInit->SetHeader(m_hasHeader);

    // Re-use Read method to collect the audio data from audio list to a buffer
    auto buffer = SpxAllocSharedAudioBuffer(m_inventorySize - m_position);
    auto dataSize = Read(buffer.get(), m_inventorySize - m_position);

    auto audioOutput = SpxQueryInterface<ISpxAudioOutput>(waveFileWriter);
    audioOutput->Write(buffer.get(), dataSize);
    audioOutput->Close();
}

uint32_t CSpxAudioDataStream::GetPosition()
{
    return m_position;
}

void CSpxAudioDataStream::SetPosition(uint32_t pos)
{
    m_position = pos;
}

uint32_t CSpxAudioDataStream::Read(uint8_t* buffer, uint32_t bufferSize)
{
    return Read(buffer, bufferSize, m_position);
}

uint32_t CSpxAudioDataStream::Read(uint8_t* buffer, uint32_t bufferSize, uint32_t pos)
{
    SPX_DBG_TRACE_VERBOSE("CSpxAudioDataStream::%s: is called", __FUNCTION__);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, buffer == nullptr);

    // Wait until either enough data is collected, or writing is finished, otherwise it's unexpected
    if (!WaitForMoreData(pos + bufferSize) && !m_writingEnded)
    {
        SPX_THROW_HR(SPXERR_UNEXPECTED_AUDIO_OUTPUT_FAILURE);
    }

    return FillBuffer(buffer, bufferSize, pos);
}

uint32_t CSpxAudioDataStream::Write(const uint8_t* buffer, uint32_t size)
{
    SPX_DBG_TRACE_VERBOSE("CSpxAudioDataStream::%s buffer %p size=%d", __FUNCTION__, (void*)buffer, size);

    if (size == 0)
    {
        return 0;
    }

    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, buffer == nullptr);

    // Allocate the buffer for the audio, and make a copy of the data
    auto newBuffer = SpxAllocSharedAudioBuffer(size);
    memcpy(newBuffer.get(), buffer, size);

    // Store the buffer into our audio list
    std::unique_lock<std::mutex> lock(m_mutex);
    m_writingEnded = false;
    m_audioList.emplace_back(newBuffer, size);
    m_inventorySize += size;
    m_status = StreamStatus::PartialData;
    m_cv.notify_all();

    return size;
}

void CSpxAudioDataStream::Close()
{
    if (m_error == nullptr)
    {
        m_status = StreamStatus::AllData;
    }

    SignalEndOfWriting();
}

uint32_t CSpxAudioDataStream::FillBuffer(uint8_t* buffer, uint32_t bufferSize, uint32_t pos)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Set position
    m_position = pos;

    // Calculate the count of bytes to be read
    uint32_t totalBytesToBeRead = 0;
    if (m_inventorySize > m_position)
    {
        totalBytesToBeRead = std::min(m_inventorySize - m_position, bufferSize);
    }

    auto remainedBytesToBeRead = totalBytesToBeRead;
    uint32_t offsetInBuffer = 0;

    // Seek to m_position
    uint32_t scannedSize = 0;
    auto iterator = m_audioList.begin();
    while (iterator != m_audioList.end() && scannedSize + iterator->second <= m_position)
    {
        scannedSize += iterator->second;
        ++iterator;
    }

    uint32_t positionInItem = m_position - scannedSize;

    // Read data from current item
    if (remainedBytesToBeRead > 0)
    {
        SPX_DBG_ASSERT_WITH_MESSAGE(iterator != m_audioList.end(), "m_position is out of m_audioList, which is unexpected.");
        uint32_t bytesToBeRead = std::min(iterator->second - positionInItem, remainedBytesToBeRead);
        memcpy(buffer, iterator->first.get() + positionInItem, bytesToBeRead);
        remainedBytesToBeRead -= bytesToBeRead;
        offsetInBuffer += bytesToBeRead;
        m_position += bytesToBeRead;
    }

    if (remainedBytesToBeRead > 0)
    {
        SPX_DBG_ASSERT_WITH_MESSAGE(iterator != m_audioList.end(), "m_position is out of m_audioList, which is unexpected.");
        ++iterator;
    }

    // Read data from more items until remainedBytesToBeRead == 0
    while (iterator != m_audioList.end() && remainedBytesToBeRead > 0)
    {
        uint32_t bytesToBeRead = std::min(iterator->second, remainedBytesToBeRead);
        memcpy(buffer + offsetInBuffer, iterator->first.get(), bytesToBeRead);
        remainedBytesToBeRead -= bytesToBeRead;
        offsetInBuffer += bytesToBeRead;
        m_position += bytesToBeRead;

        ++iterator;
    }

    SPX_DBG_TRACE_VERBOSE("CSpxAudioDataStream::%s: bytesRead=%d", __FUNCTION__, totalBytesToBeRead);
    return totalBytesToBeRead;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
