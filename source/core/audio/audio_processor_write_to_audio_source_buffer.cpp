//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_processor_write_to_audio_source_buffer.cpp: Implementation definitions for CSpxAudioProcessorWriteToAudioSourceBuffer
//

#include "stdafx.h"
#include <inttypes.h>
#include "audio_processor_write_to_audio_source_buffer.h"
#include "service_helpers.h"
#include <property_id_2_name_map.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxAudioProcessorWriteToAudioSourceBuffer::~CSpxAudioProcessorWriteToAudioSourceBuffer()
{
    SPX_DBG_ASSERT(m_format == nullptr);
    SPX_DBG_ASSERT(m_bufferData == nullptr);
    SPX_DBG_ASSERT(m_bufferProperties == nullptr);
    SPX_DBG_ASSERT(m_notifySource == nullptr);
    SPX_DBG_ASSERT(m_notifyTarget == nullptr);
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::Term()
{
    SPX_DBG_ASSERT(m_format == nullptr);
    SPX_DBG_ASSERT(m_bufferData == nullptr);
    SPX_DBG_ASSERT(m_bufferProperties == nullptr);
    SPX_DBG_ASSERT(m_notifySource == nullptr);
    SPX_DBG_ASSERT(m_notifyTarget == nullptr);
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::SetFormat(const SPXWAVEFORMATEX* format)
{
    AudioProcessor_Base::SetFormat(format);
    if (format != nullptr)
    {
        InitFromSite();
    }
    NotifyTarget();
    if (format == nullptr)
    {
        Clean();
    }
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::ProcessAudio(const DataChunkPtr& audioChunk)
{
    AudioProcessor_Base::ProcessAudio(audioChunk);

    SetProperty(PropertyId::DataBuffer_TimeStamp, audioChunk->capturedTime);
    SetProperty(PropertyId::DataBuffer_UserId, audioChunk->userId);

    m_bufferData->Write(audioChunk->data.get(), audioChunk->size);
    NotifyTarget();
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::ProcessCommit(uint32_t token, uint64_t offsetBytes, bool hasChannel, uint32_t channelId)
{
    // Inline commit: audio reaches the session indirectly - ProcessAudio
    // above writes bytes into the audio-source buffer and notifies the
    // shim, which reads them back and calls ProcessAudio on the session.
    // A commit marker carries no bytes, so it cannot ride that buffer.
    // Forward it to the shim, which hands it to the session's audio
    // processor.
    //
    // Ordering is preserved: the notify chain in ProcessAudio runs
    // synchronously on the pump thread, so by the time the pump calls
    // ProcessCommit, every preceding audio chunk has already been
    // delivered to the session.
    auto shim = SpxQueryService<ISpxAudioSessionShim>(GetSite());
    if (shim == nullptr)
    {
        SPX_TRACE_ERROR("%s: no audio session shim reachable; dropping commit token=%" PRIu32, __FUNCTION__, token);
        return;
    }

    SPX_DBG_TRACE_VERBOSE("%s: forwarding commit token=%" PRIu32 " offsetBytes=%" PRIu64, __FUNCTION__, token, offsetBytes);
    shim->ProcessCommit(token, offsetBytes, hasChannel, channelId);
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::SetError(const std::string&)
{
    NotifyTarget();
    Clean();
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::InitFromSite()
{
    InitNotifyTargetFromSite();
    InitBufferDataAndPropertiesFromSite();
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::Clean()
{
    if (m_bufferData != nullptr)
    {
        m_bufferData->Write(nullptr, 0);
    }
    CleanNotifyTarget();
    CleanBufferDataAndProperties();
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::InitNotifyTargetFromSite()
{
    m_notifyTarget = SpxQueryService<ISpxAudioProcessorNotifyMe>(GetSite());
    m_notifySource = SpxSharedPtrFromThis<ISpxAudioProcessor>(this);
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::CleanNotifyTarget()
{
    m_notifyTarget = nullptr;
    m_notifySource = nullptr;
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::InitBufferDataAndPropertiesFromSite()
{
    m_bufferData = SpxQueryService<ISpxBufferDataWriter>(GetSite(), Type<ISpxBufferData>::Id);
    m_bufferProperties = SpxQueryService<ISpxBufferProperties>(GetSite(), Type<ISpxBufferProperties>::Id);
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::CleanBufferDataAndProperties()
{
    m_bufferData = nullptr;
    m_bufferProperties = nullptr;
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::SetProperty(PropertyId id, const std::string& value)
{
    auto name = GetPropertyName(id);
    m_bufferProperties->SetBufferProperty(name, value.c_str());
}

void CSpxAudioProcessorWriteToAudioSourceBuffer::NotifyTarget()
{
    // TODO: Who needs to catch this info?
    if (m_notifyTarget != nullptr)
    {
        m_notifyTarget->NotifyMe(m_format != nullptr ? m_notifySource : nullptr);
    }
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
