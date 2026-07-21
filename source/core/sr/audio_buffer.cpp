//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <cmath>
#include "audio_buffer.h"
#include "time_utils.h"

using namespace std::chrono;

/*
    The audio buffer is a std::deque -- a double-ended queue -- an indexed sequence container that allows fast insertion and deletion at both its beginning and its end.
    The elements in the queue are of type DataChunk, containing (potentially) variable-size audio buffers and associated meta-data.

    The queue is segmented into two parts. At the beginning of the queue (indexes 0 to m_currentChunk-1) we have the "non acknowledged" (or unacknowledged) audio. This is audio that was
    already sent to the cloud, but the client did not yet get a recognition result (or no-match result) for this audio. At the end of the queue (indexes m_currentChunk
    to m_audioBuffers.size()-1) is the "stashed audio" that was not yet sent to the cloud. It still needs to be processed:

        std::deque                                                                       std::deque
           Font                                                                             Back
                     Non-Acknowledged audio                Stashed audio 
            <----------------------------------->|<------------------------------------------>
    Index:  0  1  2  3  4  ...  m_currentChunk-1   m_currentChunk               m_audioBuffers.size()-1

           Oldest                                                                          Newest
           audio                                                                           audio

    When a recognition result is received, it contains the start time and duration of the segment of audio that was recognized. This triggers a call to PcmAudioBuffer::DiscardBytes()
    to remove all DataChunks from the front of the queue (left-most side of the above diagram) that contain audio that was recognized.

    When a new audio buffer is received from the input source (WAV file, input stream, live microphone), a call to PcmAudioBuffer::Add() is made to push the new DataChunk to the
    end of the queue (right-most side of the above diagram). The same thread then calls PcmAudioBuffer::GetNext() to get the next DataChunk that needs to be processed (e.g.
    sent to the UspRecoEngineAdapter). The index m_currentChunk marks the location of next DataChunk that has not yet been processed. Calling PcmAudioBuffer::GetNext() results
    in incrementing m_currentChunk, which means the DataChunk moves from the stashed audio segment to the non-acknowledged audio segment. PcmAudioBuffer::GetNext() is called several
    times in a loop until there is no more DataChunks to process. This means that in normal operation, the stashed audio segment contain 0 or 1 DataChunks, whereas the non-acknowledged
    audio segment contains lots of DataChunks corresponding to audio that has not yet been included in a recognition result.

    When the PcmAudioBuffer::NewTurn() is called, the index m_currentChunk is set to zero. This means that now all DataChunks that were non-acknowledged audio, become stashed audio.
    Therefore in the following cmAudioBuffer::GetNext() calls they will be sent for processing again (sent to the cloud). This is our way of re-sending audio that has not yet been
    recognized.

    Member variables:

        m_totalSizeInBytes - The total number of audio bytes in the queue (stashed plus non-acknowledged). This is the sum of DataChunk.size for all DataChunks in the queue.
        When PcmAudioBuffer::Add() is called m_totalSizeInBytes get incremented. When PcmAudioBuffer::DiscardBytes() is called m_totalSizeInBytes gets decremented. 

        m_currentChunk - Per the above, this is the index of the oldest DataChunk that has not yet been processed (not yet sent to the cloud). The oldest DataChunk in the stashed audio.

        m_bufferStartOffsetInBytesAbsolute - This value is always increasing. It represent the audio duration (in bytes offset from the beginning of the audio source), corresponding
        to the first audio byte in the oldest DataChunk (left most in the above diagram).
*/

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    PcmAudioBuffer::PcmAudioBuffer(const SPXWAVEFORMATEX& header)
        : m_header{ header },
          m_totalSizeInBytes{ 0 },
          m_currentChunk{ 0 },
          m_bufferStartOffsetInBytesAbsolute{ 0 },
          m_bitsPerSample{ header.wBitsPerSample },
          m_samplesPerSecond{ header.nSamplesPerSec }
    {
        if (header.wBitsPerSample % 8 != 0 && header.wBitsPerSample != 4)
        {
            SPX_TRACE_ERROR("going to throw wrong bit per sample runtime_error");
            ThrowRuntimeError("Bits per sample '" + std::to_string(header.wBitsPerSample) + "' is not supported. It should be dividable by 8 or be exactly 4.");
        }
    }

    void PcmAudioBuffer::Add(const DataChunkPtr& audioChunk)
    {
        std::unique_lock<std::mutex> guard(m_lock);
        m_audioBuffers.push_back(audioChunk);
        m_totalSizeInBytes += audioChunk->size;
    }

    DataChunkPtr PcmAudioBuffer::GetNext()
    {
        std::unique_lock<std::mutex> guard(m_lock);
        return GetNextUnlocked();
    }

    void PcmAudioBuffer::NewTurn()
    {
        std::unique_lock<std::mutex> guard(m_lock);

        SPX_DBG_TRACE_FUNCTION();

        m_currentChunk = 0;
    }

    void PcmAudioBuffer::DiscardBytes(uint64_t bytes)
    {
        std::unique_lock<std::mutex> guard(m_lock);
        DiscardBytesUnlocked(bytes);
    }

    // In case when we need to have a shared pointer A into a buffer that already
    // managed by some shared pointer B, we define a custom deleter that captures pointer B
    // and resets in when ref counter of pointer A gets to 0.
    struct PtrHolder
    {
        // Have to be mutable in order to reset it in the custom deleter.
        mutable std::shared_ptr<uint8_t> data;
    };

    void PcmAudioBuffer::DiscardBytesUnlocked(uint64_t bytes)
    {
        SPX_DBG_TRACE_VERBOSE("%s discarding %" PRIu64 " bytes.", __FUNCTION__, bytes);

        uint64_t chunkBytes = 0;
        while (!m_audioBuffers.empty() && bytes &&
               (chunkBytes = m_audioBuffers.front()->size) <= bytes)
        {
            bytes -= chunkBytes;
            m_audioBuffers.pop_front();
            m_currentChunk--;
            SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, m_totalSizeInBytes < chunkBytes);
            m_totalSizeInBytes -= chunkBytes;
            m_bufferStartOffsetInBytesAbsolute += chunkBytes;
        }

        if (m_audioBuffers.empty())
        {
            if (m_totalSizeInBytes != 0)
            {
                SPX_TRACE_ERROR("%s: Invalid state of the audio buffer, no chunks but totalSize %d", __FUNCTION__, (int)m_totalSizeInBytes);
                SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
            }

            if (bytes > 0)
            {
                SPX_TRACE_WARNING("%s: Discarding %d more bytes than were available in the buffer.",
                        __FUNCTION__, (int)bytes);
            }

            m_currentChunk = 0;
        }
        else if (bytes > 0)
        {
            // At this point, bytes is less than the size of current chunk, so safe to cast to uint32_t.
            m_audioBuffers.front()->size -= (uint32_t)bytes;
            m_bufferStartOffsetInBytesAbsolute += bytes;
            auto holder = PtrHolder{ m_audioBuffers.front()->data };
            m_audioBuffers.front()->data = std::shared_ptr<uint8_t>(holder.data.get() + bytes, [holder](void *) { holder.data.reset(); });
            SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, m_totalSizeInBytes < bytes);
            m_totalSizeInBytes -= bytes;
        }
    }

    void PcmAudioBuffer::DiscardTill(uint64_t offsetInTicks)
    {
        std::unique_lock<std::mutex> guard(m_lock);
        DiscardTillUnlocked(offsetInTicks);
    }

    uint64_t PcmAudioBuffer::DurationToBytes(uint64_t durationInTicks) const
    {
        if (m_samplesPerSecond % MillisecondsInSecond == 0)
        {
            if (m_bitsPerSample == 4)
            {
                return (static_cast<uint64_t>(m_header.nChannels) *
                    (m_samplesPerSecond / MillisecondsInSecond) *
                    (durationInTicks / TicksInMillisecond)) / 2;
            }
            else
            {
                return static_cast<uint64_t>(m_header.nChannels) * (m_bitsPerSample / 8) *
                    (m_samplesPerSecond / MillisecondsInSecond) *
                    (durationInTicks / TicksInMillisecond);
            }
        }
        else
        {
            if (m_bitsPerSample == 4)
            {
                return ((uint64_t)std::ceil(((double)m_samplesPerSecond / MillisecondsInSecond) *
                    (durationInTicks / TicksInMillisecond))
                    * m_header.nChannels) / 2;
            }
            else
            {
                return (uint64_t)std::ceil(((double)m_samplesPerSecond / MillisecondsInSecond) *
                    (durationInTicks / TicksInMillisecond))
                    * m_header.nChannels * (m_bitsPerSample / 8);
            }
        }
    }

    uint64_t PcmAudioBuffer::BytesToDurationInTicks(uint64_t bytes) const
    {
        if (m_header.nChannels == 0 || m_bitsPerSample == 0 || m_samplesPerSecond == 0)
        {
            SPX_TRACE_ERROR("%s: Invalid channel count %d or bitsPerSample %d or samplesPerSecond %d, none can be zero", __FUNCTION__, m_header.nChannels, m_bitsPerSample, m_samplesPerSecond);
            SPX_THROW_HR(SPXERR_RUNTIME_ERROR);
        }
        else
        {
            if (m_samplesPerSecond % MillisecondsInSecond == 0)
            {
                if (m_bitsPerSample == 4)
                {
                    return ((bytes * TicksInMillisecond * MillisecondsInSecond) /
                        (static_cast<uint64_t>(m_header.nChannels) * m_samplesPerSecond)) / 2;
                }
                else
                {
                    return (bytes * TicksInMillisecond * MillisecondsInSecond) /
                        (static_cast<uint64_t>(m_header.nChannels) * (m_bitsPerSample / 8) * m_samplesPerSecond);
                }
            }
            else
            {
                if (m_bitsPerSample == 4)
                {
                    return ((uint64_t)std::ceil(bytes * TicksInMillisecond * MillisecondsInSecond / (double)m_samplesPerSecond) /
                        m_header.nChannels) / 2;
                }
                else
                {
                    return (uint64_t)std::ceil(bytes * TicksInMillisecond * MillisecondsInSecond / (double)m_samplesPerSecond) /
                        (static_cast<uint64_t>(m_header.nChannels) * (m_bitsPerSample / 8));
                }
            }
        }
    }

    uint64_t PcmAudioBuffer::ToAbsolute(uint64_t offsetInTicksTurnRelative) const
    {
        int64_t bytes = DurationToBytes(offsetInTicksTurnRelative);
        return BytesToDurationInTicks(m_bufferStartOffsetInBytesAbsolute + bytes);
    }

    uint64_t PcmAudioBuffer::StashedSizeInBytes() const
    {
        std::unique_lock<std::mutex> guard(m_lock);
        return StashedSizeInBytesUnlocked();
    }

    uint64_t PcmAudioBuffer::StashedSizeInBytesUnlocked() const
    {
        uint64_t size = 0;
        for (size_t i = m_currentChunk; i < m_audioBuffers.size(); ++i)
        {
            size += m_audioBuffers[i]->size;
        }
        return size;
    }

    uint64_t PcmAudioBuffer::NonAcknowledgedSizeInBytesUnlocked() const
    {
        uint64_t unconfirmedBytes = 0;
        for (size_t i = 0; (i < m_currentChunk && i < m_audioBuffers.size()); ++i)
        {
            unconfirmedBytes += m_audioBuffers[i]->size;
        }

        return unconfirmedBytes;
    }

    void PcmAudioBuffer::Drop()
    {
        std::unique_lock<std::mutex> guard(m_lock);

        // Discarding unconfirmed bytes that we have already sent to the service.
        uint64_t unconfirmedBytes = NonAcknowledgedSizeInBytesUnlocked();
        DiscardBytesUnlocked(unconfirmedBytes);

        // Discarding chunks that we have not yet sent to the service.
        DataChunkPtr chunk;
        while ((chunk = GetNextUnlocked()) != nullptr)
        {
            DiscardBytesUnlocked(chunk->size);
        }
    }

    void PcmAudioBuffer::CopyNonAcknowledgedDataTo(AudioBufferPtr buffer) const
    {
        if (buffer.get() == this)
        {
            return;
        }

        std::unique_lock<std::mutex> guard(m_lock);
        for (const auto& c : this->m_audioBuffers)
            buffer->Add(std::make_shared<DataChunk>(c->data, c->size, c->receivedTime));
    }

    DataChunkPtr PcmAudioBuffer::GetNextUnlocked()
    {
        if (m_currentChunk >= m_audioBuffers.size())
        {
            // No data available.
            return nullptr;
        }

        DataChunkPtr result = m_audioBuffers[m_currentChunk];
        m_currentChunk++;
        return result;
    }

    void PcmAudioBuffer::DiscardTillUnlocked(uint64_t offsetInTicks)
    {
        uint64_t offsetInBytes = DurationToBytes(offsetInTicks);
        if (offsetInBytes < m_bufferStartOffsetInBytesAbsolute)
        {
            SPX_TRACE_WARNING("%s: Offset is not monotonically increasing. Current offset in bytes %d, discarding bytes %d",
                __FUNCTION__, (int)m_bufferStartOffsetInBytesAbsolute, (int)offsetInBytes);
            return;
        }

        DiscardBytesUnlocked(offsetInBytes - m_bufferStartOffsetInBytesAbsolute);
    }

    uint64_t PcmAudioBuffer::NonAcknowledgedSizeInBytes() const
    {
        std::unique_lock<std::mutex> guard(m_lock);
        return NonAcknowledgedSizeInBytesUnlocked();
    }

    uint64_t PcmAudioBuffer::GetAbsoluteOffset() const
    {
        std::unique_lock<std::mutex> guard(m_lock);
        return BytesToDurationInTicks(m_bufferStartOffsetInBytesAbsolute);
    }

    uint64_t PcmAudioBuffer::GetCurrentOffset()
    {
        std::unique_lock<std::mutex> guard(m_lock);
        return m_bufferStartOffsetInBytesAbsolute;
    }

    void PcmAudioBuffer::SetCurrentOffset(uint64_t offset)
    {
        std::unique_lock<std::mutex> guard(m_lock);
        m_bufferStartOffsetInBytesAbsolute = offset;
    }

    ProcessedAudioTimestampPtr PcmAudioBuffer::GetTimestamp(uint64_t offsetInTicks) const
    {
        std::unique_lock<std::mutex> guard(m_lock);

        uint64_t offsetInBytes = DurationToBytes(offsetInTicks);
        if (offsetInBytes < m_bufferStartOffsetInBytesAbsolute)
        {
            SPX_TRACE_WARNING("%s: Offset is no longer in the buffer. Current base offset in bytes %d, offset to get timestamp in bytes %d. Returning oldest timestamp.",
                __FUNCTION__, (int)m_bufferStartOffsetInBytesAbsolute, (int)offsetInBytes);
            return (m_audioBuffers.size() == 0) ? nullptr : std::make_shared<ProcessedAudioTimestamp>(m_audioBuffers[0]->receivedTime, 0);
        }

        uint64_t bytes = offsetInBytes - m_bufferStartOffsetInBytesAbsolute;
        uint64_t chunkBytes = 0;
        auto queueSize = m_audioBuffers.size();
        size_t index;
        for (index = 0; index < queueSize; index++)
        {
            chunkBytes = m_audioBuffers[index]->size;
            if (chunkBytes >= bytes)
            {
                break;
            }
            else
            {
                bytes -= chunkBytes;
            }
        }

        uint64_t remainingInTicks = 0;
        system_clock::time_point audioTimestamp;
        if (queueSize != 0)
        {
            if (index >= queueSize)
            {
                audioTimestamp = m_audioBuffers.back()->receivedTime;
                SPX_TRACE_ERROR("%s: Offset exceeds what is available in the buffer %d. No timestamp can be retrieved, using oldest available timestamp %s.", __FUNCTION__, (int)bytes, PAL::GetTimeInString(audioTimestamp).c_str());
                SPX_DBG_ASSERT_WITH_MESSAGE(bytes > 0, "Reach end of queue, but no bytes left.");
            }
            else
            {
                audioTimestamp = m_audioBuffers[index]->receivedTime;
                remainingInTicks = BytesToDurationInTicks(chunkBytes - bytes);
            }
        }
        else
        {
            SPX_TRACE_WARNING("%s: Audio queue is empty. No timestamp can be retrieved, using the default epoch value.", __FUNCTION__);
        }

        return std::make_shared<ProcessedAudioTimestamp>(audioTimestamp, remainingInTicks);
    }

}}}}
