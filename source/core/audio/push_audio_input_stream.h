//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// push_audio_input_stream.h: Implementation definitions for CSpxPushAudioInputStream C++ class
//

#pragma once
#include "stdafx.h"
#include "interface_helpers.h"
#include "service_helpers.h"
#include <chrono>
#include <deque>
#include <queue>


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

#define DATA_INFO_TIME_STAMP_KEY  "timestamp"
#define DATA_INFO_SPEAKER_ID_KEY "speakerid"

class CSpxPushAudioInputStream :
    public ISpxAudioStreamInitFormat,
    public ISpxAudioStream,
    public ISpxAudioStreamWriter,
    public ISpxAudioStreamReader
{
public:
    using DataInfo = std::map<std::string, std::string>;

    CSpxPushAudioInputStream();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStreamInitFormat)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStream)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStreamWriter)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStreamReader)
    SPX_INTERFACE_MAP_END()

    // --- ISpxAudioStreamInitFormat ---
    void SetFormat(const SPXWAVEFORMATEX* format) override;

    // --- ISpxAudioStreamWriter ---
    void Write(uint8_t* buffer, uint32_t size) override;
    void SetProperty(PropertyId propertyId, const SPXSTRING& value) override;
    void SetProperty(const SPXSTRING& name, const SPXSTRING& value) override;
    uint32_t Commit() override;
    uint32_t Commit(uint32_t channelId) override;

    // --- ISpxAudioStreamReader ---
    uint16_t GetFormat(SPXWAVEFORMATEX* format, uint16_t formatSize) override;
    uint32_t Read(uint8_t* pbuffer, uint32_t cbBuffer) override;
    SPXSTRING GetProperty(PropertyId propertyId) override;
    void SetShouldWaitForPendingData(bool shouldWait) override;
    void Close() override { }
    bool PopPendingCommitMarker(uint32_t* outToken, uint64_t* outOffsetBytes, bool* outHasChannel, uint32_t* outChannelId) override;
    void ResetCommitAnchorBase() override;

private:

    DISABLE_COPY_AND_MOVE(CSpxPushAudioInputStream);

    // Inline commit: shared implementation for Commit() and Commit(channelId).
    // hasChannel=false means "all channels"; hasChannel=true scopes to
    // channelId.
    uint32_t CommitInternal(bool hasChannel, uint32_t channelId);

    void WriteBuffer(uint8_t* buffer, uint32_t size);
    bool WaitForMoreData();

    void SignalEndOfStream();

    std::shared_ptr<SPXWAVEFORMATEX> m_format;

    std::mutex m_mutex;
    std::condition_variable m_cv;

    // Inline commit: marker recorded by Commit(), carrying the token and the
    // cumulative bytes-written position at the moment of the call.
    struct CommitMarker
    {
        uint32_t token;
        uint64_t offsetBytes;
        bool hasChannel;
        uint32_t channelId;
    };

    // Commit markers are held in the same queue as the audio rather than in a
    // parallel FIFO because their ordering relative to the audio *is* the
    // information a commit carries: a commit means "segment here, after
    // exactly the audio written so far".
    //
    // This is exact rather than approximate because a marker's position always
    // coincides with a boundary between entries. m_totalBytesWritten is
    // advanced only by WriteBuffer(), by the whole size of one Write() call,
    // and CommitInternal() stamps a marker from it under the same lock - so a
    // marker can never denote a position inside a queued buffer, and no buffer
    // ever has to be split to place one.
    //
    // A commit entry carries no audio. Read() stops when it reaches one and
    // returns what it has, which may be zero bytes; the pump then drains the
    // marker via PopPendingCommitMarker() and reads again. A zero-length read
    // is therefore not end of stream, and the pump distinguishes the two by
    // whether it drained a marker.
    struct QueueEntry
    {
        // Discriminates the two cases below. When true, only 'marker' is
        // meaningful; when false, only the audio fields are.
        bool isCommit = false;

        // Audio payload, valid when isCommit is false.
        SpxSharedAudioBuffer_Type buffer;
        uint32_t size = 0;
        DataInfo dataInfo;

        // Commit marker, valid when isCommit is true.
        CommitMarker marker{};
    };

    // std::deque rather than std::queue: SignalEndOfStream() has to erase
    // commit entries from the middle while leaving the audio in place.
    std::deque<QueueEntry> m_queue;

    std::shared_ptr<uint8_t> m_buffer;
    uint32_t m_bytesInBuffer;
    uint8_t* m_ptrIntoBuffer;
    uint32_t m_bytesLeftInBuffer;

    // Cumulative bytes handed to WriteBuffer(). This is what
    // stamps a marker's offsetBytes, which is reported out through
    // PopPendingCommitMarker() and used by the session to place the commit
    // against the audio it has sent.
    uint64_t m_totalBytesWritten = 0;

    // Inline commit: value of m_totalBytesWritten at the point the consuming
    // session established the audio buffer it measures commit anchors
    // against. A marker's offsetBytes is stamped as the difference between
    // m_totalBytesWritten and this, so anchors are expressed in the session's
    // byte domain rather than the stream's lifetime domain.
    //
    // Zero for a stream consumed by exactly one session, which is the ordinary
    // case; non-zero only when a stream already carried audio before the
    // current session started. m_totalBytesWritten itself is deliberately left
    // running, because SignalEndOfStream() relies on it never being reset and
    // it is the stream's own record of what it has written.
    uint64_t m_commitAnchorBase = 0;

    uint32_t m_nextCommitToken = 1;

    // Inline commit rate limit: reject Commit() calls that
    // arrive less than kCommitMinIntervalMs of wall clock after the previous
    // successful Commit(). Sentinel default value means "no previous commit".
    static constexpr int kCommitMinIntervalMs = 100;
    std::chrono::steady_clock::time_point m_lastCommitTime{};

    DataInfo m_dataInfo;
    DataInfo m_dataInfoInRead;

    bool m_endOfStream;
    bool m_waitForPendingData;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
