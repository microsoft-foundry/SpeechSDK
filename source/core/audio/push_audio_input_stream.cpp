//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// push_audio_input_stream.cpp: Implementation definitions for CSpxPushAudioInputStream C++ class
//

#include "stdafx.h"
#include "push_audio_input_stream.h"
#include <property_id_2_name_map.h>
#include <audio_constants.h>
#include <algorithm>
#include <cstring>
#include <chrono>

using namespace std::chrono_literals;

// change it to 1 to debug.
#define TURN_ON_VERBOSE_AUDIO_DEBUGGING 0

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxPushAudioInputStream::CSpxPushAudioInputStream() :
    m_bytesInBuffer(0),
    m_ptrIntoBuffer(nullptr),
    m_bytesLeftInBuffer(0),
    m_endOfStream(false),
    m_waitForPendingData(true)
{
}

void CSpxPushAudioInputStream::SetFormat(const SPXWAVEFORMATEX* format)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_format != nullptr);

    // Allocate the buffer for the format
    auto formatSize = sizeof(SPXWAVEFORMATEX) + format->cbSize;
    m_format = SpxAllocWAVEFORMATEX(formatSize);
    SPX_DBG_TRACE_VERBOSE_IF(TURN_ON_VERBOSE_AUDIO_DEBUGGING, "CSpxPushAudioInputStream::SetFormat is called with format 0x%p", (void*)format);
    // Copy the format
    memcpy(m_format.get(), format, formatSize);
}

void CSpxPushAudioInputStream::Write(uint8_t* buffer, uint32_t size)
{
    SPX_DBG_TRACE_VERBOSE_IF(TURN_ON_VERBOSE_AUDIO_DEBUGGING, "CSpxPushAudioInputStream::Write buffer %p size=%d", (void*)buffer, size);
    if (buffer != nullptr && size > 0)
    {
        WriteBuffer(buffer, size);
        m_endOfStream = false;
    }
    else
    {
        SignalEndOfStream();
    }
}

void CSpxPushAudioInputStream::SetProperty(PropertyId propertyId, const SPXSTRING& value)
{
    if (propertyId == PropertyId::DataBuffer_TimeStamp)
    {
        m_dataInfo[DATA_INFO_TIME_STAMP_KEY] = value;
    }
    else if (propertyId == PropertyId::DataBuffer_UserId)
    {
        m_dataInfo[DATA_INFO_SPEAKER_ID_KEY] = value;
    }
    else
    {
        std::string str = "Error: PropertyId " + std::to_string(static_cast<int>(propertyId)) + " is not supported";
        ThrowInvalidArgumentException(str);
    }
}

void CSpxPushAudioInputStream::SetProperty(const SPXSTRING& name, const SPXSTRING& value)
{
    m_dataInfo[name] = value;
}

uint16_t CSpxPushAudioInputStream::GetFormat(SPXWAVEFORMATEX* formatBuffer, uint16_t formatSize)
{
    uint16_t formatSizeRequired = sizeof(SPXWAVEFORMATEX) + m_format->cbSize;
    SPX_DBG_TRACE_VERBOSE_IF(TURN_ON_VERBOSE_AUDIO_DEBUGGING, "CSpxPushAudioInputStream::GetFormat is called formatBuffer is %s formatSize=%d", formatBuffer? "not null":"null", formatSize);

    if (formatBuffer != nullptr)
    {
        size_t size = std::min(formatSize, formatSizeRequired);
        std::memcpy(formatBuffer, m_format.get(), size);
    }

    return formatSizeRequired;
}

uint32_t CSpxPushAudioInputStream::Read(uint8_t* buffer, uint32_t bytesToRead)
{
    SPX_DBG_TRACE_VERBOSE("CSpxPushAudioInputStream::Read: bytesToRead=%d", bytesToRead);

    uint32_t totalBytesRead = 0;

    while (bytesToRead > 0)
    {
        // If we don't have any bytes in our buffer, let's get a new buffer from
        // the queue - unless the head of the queue is a commit marker, in which
        // case this read stops here.
        //
        // Stopping at the marker is what keeps the commit ordered against the
        // audio: everything written before Commit() was called sits ahead of
        // the marker in the queue and has therefore already been delivered,
        // and everything written afterwards sits behind it and must not be
        // delivered until the pump has drained it and sent audio.commit.
        //
        // Note the audio already copied into the caller's buffer on earlier
        // iterations is kept and reported - this is a short read, not a
        // discarded one. When the marker is at the very start of the read
        // there is nothing to report and the read returns zero bytes, which
        // the pump treats as a commit boundary rather than end of stream
        // because it drains a marker immediately afterwards.
        if (m_bytesLeftInBuffer == 0)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (!m_queue.empty())
            {
                if (m_queue.front().isCommit)
                {
                    SPX_DBG_TRACE_VERBOSE("CSpxPushAudioInputStream::Read: commit marker reached; returning read of %d bytes", totalBytesRead);
                    break;
                }

                auto& item = m_queue.front();
                m_buffer = item.buffer;
                m_bytesInBuffer = item.size;
                m_dataInfoInRead = std::move(item.dataInfo);
                m_queue.pop_front();

                m_ptrIntoBuffer = m_buffer.get();
                m_bytesLeftInBuffer = m_bytesInBuffer;
            }
        }

        // If we still don't have a buffer to work with...
        if (m_bytesLeftInBuffer == 0)
        {
             SPX_DBG_TRACE_VERBOSE_IF(TURN_ON_VERBOSE_AUDIO_DEBUGGING, "CSpxPushAudioInputStream::Read: endOfStream is %s", m_endOfStream ? "true" : "false");

            if (m_endOfStream)
            {
                // Caller told us we're done; we're outta here!
                break;
            }
            else if (WaitForMoreData())
            {
                // Caller provided more data, or a commit marker arrived; back
                // to the top of the loop, which handles either.
                continue;
            }
            else // We waited for more data, but instead...
            {
                SPX_DBG_TRACE_VERBOSE("%s: End of stream detected...", __FUNCTION__);
                break; // We're outta here!
            }
        }

        // Now that we know we have a buffer with data in it ... let's copy some bytes
        uint32_t bytesThisLoop = std::min(bytesToRead, m_bytesLeftInBuffer);
        std::memcpy(buffer, m_ptrIntoBuffer, bytesThisLoop);
        buffer += bytesThisLoop;

        // And update our buffering pointers/members
        m_ptrIntoBuffer += bytesThisLoop;
        m_bytesLeftInBuffer -= bytesThisLoop;
        bytesToRead -= bytesThisLoop;
        totalBytesRead += bytesThisLoop;
    }

    SPX_DBG_TRACE_VERBOSE("CSpxPushAudioInputStream::Read: totalBytesRead=%d", totalBytesRead);
    return totalBytesRead;
}

SPXSTRING CSpxPushAudioInputStream::GetProperty(PropertyId propertyId)
{
    if (propertyId == PropertyId::DataBuffer_TimeStamp)
    {
        if (m_dataInfoInRead.find(DATA_INFO_TIME_STAMP_KEY) != m_dataInfoInRead.end())
        {
            return m_dataInfoInRead[DATA_INFO_TIME_STAMP_KEY];
        }
    }
    else if (propertyId == PropertyId::DataBuffer_UserId)
    {
        if (m_dataInfoInRead.find(DATA_INFO_SPEAKER_ID_KEY) != m_dataInfoInRead.end())
        {
            return m_dataInfoInRead[DATA_INFO_SPEAKER_ID_KEY];
        }
    }
    return "";
}

void CSpxPushAudioInputStream::SetShouldWaitForPendingData(bool shouldWait)
{
    SPX_DBG_TRACE_VERBOSE("%s: %d", __FUNCTION__, shouldWait);
    m_waitForPendingData = shouldWait;
    m_cv.notify_all();
}

bool CSpxPushAudioInputStream::PopPendingCommitMarker(uint32_t* outToken, uint64_t* outOffsetBytes, bool* outHasChannel, uint32_t* outChannelId)
{
    // Inline commit: pop the head marker if the audio ahead of it has all been
    // delivered. Called by the audio pump after each Read() to drain any
    // markers the read stopped at. The channel-scope fields (hasChannel,
    // channelId) are carried through as inert data; the push stream does not
    // interpret them.
    //
    // "The audio ahead of it has been delivered" is a positional test: the
    // marker is at the head of the queue, and the buffer the reader was part
    // way through is exhausted. Both conditions are necessary - a marker can
    // reach the head of the queue while the reader still holds bytes from the
    // buffer that preceded it, and draining then would send audio.commit ahead
    // of audio the application wrote before calling Commit().
    //
    // m_bytesLeftInBuffer is owned by the reader thread, which is the same
    // thread the pump calls this from, so reading it here needs no additional
    // synchronisation beyond the queue's own lock.
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_bytesLeftInBuffer != 0 || m_queue.empty() || !m_queue.front().isCommit)
    {
        return false;
    }
    const auto& head = m_queue.front().marker;
    if (outToken != nullptr) { *outToken = head.token; }
    if (outOffsetBytes != nullptr) { *outOffsetBytes = head.offsetBytes; }
    if (outHasChannel != nullptr) { *outHasChannel = head.hasChannel; }
    if (outChannelId != nullptr) { *outChannelId = head.channelId; }
    m_queue.pop_front();
    return true;
}

void CSpxPushAudioInputStream::WriteBuffer(uint8_t* buffer, uint32_t size)
{
    SPX_DBG_TRACE_VERBOSE("%s: size=%d", __FUNCTION__, size);

    // Allocate the buffer for the audio, and make a copy of the data
    auto newBuffer = SpxAllocSharedAudioBuffer(size);
    memcpy(newBuffer.get(), buffer, size);

    // Store the buffer in our queue
    std::unique_lock<std::mutex> lock(m_mutex);
    QueueEntry entry;
    entry.isCommit = false;
    entry.buffer = newBuffer;
    entry.size = size;
    entry.dataInfo = std::move(m_dataInfo);
    m_queue.push_back(std::move(entry));
    m_totalBytesWritten += size;
    m_cv.notify_all();
}

uint32_t CSpxPushAudioInputStream::Commit()
{
    // Inline commit: all-channels commit. Delegates to the
    // channel-scope-aware helper below.
    return CommitInternal(false, 0);
}

uint32_t CSpxPushAudioInputStream::Commit(uint32_t channelId)
{
    // Inline commit: commit scoped to a specific channel
    // of a multichannel input. The service acknowledges on the named channel
    // only.
    return CommitInternal(true, channelId);
}

uint32_t CSpxPushAudioInputStream::CommitInternal(bool hasChannel, uint32_t channelId)
{
    // Inline commit: allocate a fresh token and enqueue a
    // marker anchored to the current cumulative write position. Non-blocking.
    // Rate limit: if the previous *successful* Commit() on this stream
    // happened less than 100 ms of wall clock ago, this call is rejected -
    // returns 0, emits SPX_TRACE_ERROR, and does not update state.
    // The 100 ms window is measured between successful commits, so a
    // rejected call does not extend the window for the next attempt.
    // Tokens are allocated channel-blind: one monotonic counter
    // regardless of scope.
    std::unique_lock<std::mutex> lock(m_mutex);

    // Inline commit is only meaningful when a byte position in what the
    // application writes denotes a position in the audio the service
    // recognises. That holds only when the stream carries uncompressed audio
    // that the SDK sends on as it arrives.
    //
    // It does not hold for a format that has to be decoded. Two different
    // routes are involved, and the commit is undeliverable on both:
    //
    //  - Decoded in the SDK. The session interposes a codec (GStreamer)
    //    between this stream and the audio pump, so the pump reads from the
    //    codec rather than from here and commit markers recorded on this side
    //    are never drained.
    //  - Passed through compressed, decoded by the service. OGG OPUS with
    //    SPEECH-CompressedPassthrough is pumped by CSpxCompressedAudioAdapter,
    //    which does not drain commit markers either.
    //
    // Rejecting is not merely a convenience for the first route. A marker that
    // is never drained caps every subsequent read at the marker position, and
    // this stream signals that by returning a zero-length read. Downstream of
    // a codec there is nothing that can tell that apart from end of input -
    // the GStreamer feed callback treats a zero-length read as end of stream
    // and tears the pipeline down - so an undeliverable commit would silently
    // truncate the recognition rather than merely lose a boundary.
    //
    // In any case a byte position in encoded audio has no general mapping to a
    // position in the decoded stream: with a variable bitrate the same byte
    // count denotes different durations at different points, so the SDK could
    // not place the boundary the application asked for even if the marker did
    // reach the pump - and for passthrough the decode happens in the service,
    // where the SDK has no position information at all.
    //
    // The formats tested here are exactly those that the session sends
    // directly, without either adapter (see
    // CSpxAudioStreamSession::InitFromStream, which selects a codec or the
    // passthrough adapter for everything outside this set). Keep the two lists
    // in step.
    if (m_format != nullptr &&
        !(m_format->wFormatTag == WAVE_FORMAT_PCM ||
          m_format->wFormatTag == WAVE_FORMAT_ALAW ||
          m_format->wFormatTag == WAVE_FORMAT_MULAW ||
          m_format->wFormatTag == WAVE_FORMAT_G722))
    {
        SPX_TRACE_ERROR("%s: Commit rejected: inline commit is not supported for compressed or encoded audio (wFormatTag=%u); such audio is decoded, in the SDK or by the service, before it is recognised, so a commit position in the written bytes has no defined position in the audio the service receives.",
            __FUNCTION__, static_cast<unsigned>(m_format->wFormatTag));
        return 0;
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_lastCommitTime.time_since_epoch().count() != 0)
    {
        const auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastCommitTime);
        if (interval < std::chrono::milliseconds(kCommitMinIntervalMs))
        {
            SPX_TRACE_ERROR("%s: Commit rate limit: rejected, interval=%lld ms since previous successful commit (min %d ms)",
                __FUNCTION__, static_cast<long long>(interval.count()), kCommitMinIntervalMs);
            return 0;
        }
    }

    uint32_t token = m_nextCommitToken++;
    // Stamp the anchor in the consuming session's byte domain. See
    // m_commitAnchorBase: m_totalBytesWritten counts from the creation of this
    // stream, whereas the session measures sent audio from the start of the
    // session, and the two differ whenever a stream is reused.
    SPX_DBG_ASSERT(m_totalBytesWritten >= m_commitAnchorBase);
    const uint64_t anchorBytes = m_totalBytesWritten - m_commitAnchorBase;
    QueueEntry entry;
    entry.isCommit = true;
    entry.marker = { token, anchorBytes, hasChannel, channelId };
    m_queue.push_back(std::move(entry));
    m_lastCommitTime = now;
    m_cv.notify_all();
    if (hasChannel)
    {
        SPX_DBG_TRACE_VERBOSE("%s: token=%" PRIu32 " offsetBytes=%" PRIu64 " channel=%" PRIu32, __FUNCTION__, token, anchorBytes, channelId);
    }
    else
    {
        SPX_DBG_TRACE_VERBOSE("%s: token=%" PRIu32 " offsetBytes=%" PRIu64 " channel=all", __FUNCTION__, token, anchorBytes);
    }
    return token;
}

void CSpxPushAudioInputStream::ResetCommitAnchorBase()
{
    // Inline commit: re-base commit anchors onto the byte domain of the
    // session that is about to consume this stream. Called by the session when
    // it creates the audio buffer that anchors are compared against, so that
    // both baselines start together.
    //
    // For a stream used by a single session this is a no-op in effect, since
    // nothing has been written yet and the base is already zero. It matters
    // when a stream is reused by a later recognizer: without it, every anchor
    // in the new session would be inflated by the audio the previous session
    // consumed, and the reconnect comparisons in CSpxAudioStreamSession would
    // silently stop matching.
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_commitAnchorBase != m_totalBytesWritten)
    {
        SPX_DBG_TRACE_VERBOSE("%s: re-basing commit anchors from %" PRIu64 " to %" PRIu64 " bytes",
            __FUNCTION__, m_commitAnchorBase, m_totalBytesWritten);
    }
    m_commitAnchorBase = m_totalBytesWritten;
}

bool CSpxPushAudioInputStream::WaitForMoreData()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // A commit marker is an entry in the same queue as the audio, so a
    // Commit() issued while the reader is parked here makes the queue
    // non-empty and ends the wait on the existing condition. No separate test
    // is needed.
    //
    // This is what keeps the "write audio, commit, wait for the acknowledgment
    // before writing more" pattern working: without the wait ending, the
    // application would be waiting for an acknowledgment that cannot be sent
    // until it writes more audio, and neither side would proceed. Because the
    // marker occupies the queue rather than a side channel, that outcome is
    // not merely detected and handled here - it cannot arise.
    //
    // This does not make the reader return spuriously: the pump removes the
    // marker before the next read, so the condition clears.
    const auto shouldEndWait = [&]() { return !m_queue.empty() || m_endOfStream || !m_waitForPendingData; };
    #ifdef _DEBUG
        const auto debugWait = 100ms;
        for (int i = 0; !m_cv.wait_for(lock, debugWait, [&] { return shouldEndWait(); }); i++)
        {
            SPX_DBG_TRACE_VERBOSE("%s: still waiting (%" PRIu64 "ms)...", __FUNCTION__, static_cast<uint64_t>((i + 1) * debugWait.count()));
        }
        SPX_DBG_TRACE_VERBOSE("%s: stream ending wait for new data, queue is %s", __FUNCTION__, (m_queue.empty() ? "empty" : "NOT empty"));
    #else
        m_cv.wait(lock, [&] { return shouldEndWait(); });
    #endif

    // True when there is something to act on - audio or a commit marker - as
    // opposed to a genuine end of stream. The caller handles either.
    return !m_queue.empty();
}

void CSpxPushAudioInputStream::SignalEndOfStream()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    SPX_DBG_TRACE_VERBOSE_IF(TURN_ON_VERBOSE_AUDIO_DEBUGGING, "Signal End of Stream is called");

    // Inline commit: discard any commit markers that have not yet been drained,
    // leaving the audio in the queue untouched.
    //
    // End of audio flushes the buffer and makes the service produce a result
    // covering the audio written so far, which is the same boundary a pending
    // commit was asking for - so the request is already satisfied by other
    // means and dropping it loses nothing.
    //
    // Keeping them would be actively wrong. m_totalBytesWritten is never reset
    // and this stream can be reused: an application may write more audio and
    // start recognition again after the flush. An undrained marker would still
    // be sitting in the queue at that point, so it would be drained by the next
    // session and sent in a new turn, carrying a token issued during the
    // previous one. The application would see a stale token on a
    // current-session result and an unrequested segmentation boundary.
    //
    // Note this is about the marker, not the anchor: a reused stream also has
    // its anchor baseline re-based by ResetCommitAnchorBase() when the next
    // session starts, so even a marker that did survive would carry an offset
    // measured against the wrong baseline.
    //
    // Discarding here follows the general rule for undeliverable commits: the
    // commit is dropped and the loss recorded in the log, never escalated to
    // an error or a session failure.
    //
    // Logged at warning rather than error severity: this is per design and
    // an expected outcome, not a fault. It is also not necessarily rare - an
    // application that commits faster than the pump drains can strand many
    // markers at once, and a single ordinary stop can then emit one line per
    // stranded token.
    for (auto it = m_queue.begin(); it != m_queue.end(); )
    {
        if (it->isCommit)
        {
            SPX_TRACE_WARNING("%s: discarding commit token=%" PRIu32 " at offsetBytes=%" PRIu64 ": end of audio signalled before it was delivered",
                __FUNCTION__, it->marker.token, it->marker.offsetBytes);
            it = m_queue.erase(it);
        }
        else
        {
            ++it;
        }
    }

    m_endOfStream = true;
    m_cv.notify_all();
}
} } } } // Microsoft::CognitiveServices::Speech::Impl
