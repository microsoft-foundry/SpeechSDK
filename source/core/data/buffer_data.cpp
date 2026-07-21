//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"

#include <string>

#include "buffer_data.h"
#include "create_object_helpers.h"
#include "interfaces/read_write_buffer_init.h"
#include "read_write_buffer_delegate_helper.h"
#include "property_id_2_name_map.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

// Keep in sync with the value defined in buffer_properties.cpp. See comments there.
constexpr SizeType DEFAULT_AUDIO_SOURCE_BUFFER_DATA_SIZE_SECONDS = 3;

CSpxBufferData::CSpxBufferData()
{
    m_bytesDead = m_bytesRead = 0;
    SPX_DBG_ASSERT(m_ringBuffer.IsClear());
}

CSpxBufferData::~CSpxBufferData()
{
    // The buffer is shared between writers and readers, trying to call Term from either side breaks the
    // ref counting pattern especially since readers may linger owning the buffer longer.
    TermRingBuffer();
    SPX_DBG_ASSERT(m_ringBuffer.IsClear());
}

void CSpxBufferData::Init()
{
    EnsureInitRingBuffer();
}

void CSpxBufferData::Term()
{
    TermRingBuffer();
}

uint64_t CSpxBufferData::GetOffset()
{
    return m_bytesDead + m_bytesRead;
}

uint64_t CSpxBufferData::GetNewMultiReaderOffset()
{
    EnsureInitRingBuffer();
    auto writePos = m_ringBuffer.DelegateGetWritePos();
    return writePos;
}

uint32_t CSpxBufferData::Read(uint8_t* buffer, uint32_t size)
{
    size_t bytesRead = 0;

    EnsureInitRingBuffer();
    m_ringBuffer.DelegateRead(buffer, size, &bytesRead);

    m_bytesRead += bytesRead;

    return (uint32_t)bytesRead;
}

uint32_t CSpxBufferData::ReadAt(uint64_t offset, uint8_t* buffer, uint32_t size)
{
    size_t bytesRead;

    EnsureInitRingBuffer();
    m_ringBuffer.DelegateReadAtBytePos(offset, buffer, size, &bytesRead);

    return (uint32_t)bytesRead;
}

uint64_t CSpxBufferData::GetBytesDead()
{
    return m_bytesDead;
}

uint64_t CSpxBufferData::GetBytesRead()
{
    return m_bytesRead;
}

uint64_t CSpxBufferData::GetBytesReady()
{
    EnsureInitRingBuffer();
    auto writePos = m_ringBuffer.DelegateGetWritePos();
    auto readPos = m_ringBuffer.DelegateGetReadPos();

    return writePos - readPos;
}

uint64_t CSpxBufferData::GetBytesReadyMax()
{
    return std::numeric_limits<uint32_t>::max();
}

void CSpxBufferData::Write(uint8_t* buffer, uint32_t size)
{
    EnsureInitRingBuffer();
    m_ringBuffer.DelegateWrite(buffer, size);
}

void CSpxBufferData::EnsureInitRingBuffer()
{
    if (!m_ringBuffer.IsClear())
    {
        return;
    }

    SPX_TRACE_VERBOSE("[%p]CSpxBufferData::EnsureInitRingBuffer - Init", (void*) this);
    // CSpxBlockingReadWriteRingBuffer does not have a site actually

    auto init = SpxCreateObjectWithSite<ISpxReadWriteBufferInit>("CSpxBlockingReadWriteRingBuffer", this);
    init->SetName("BufferData");
    init->AllowOverflow(GetBufferAllowOverflow());
    init->SetSize(GetBufferDataSize());
    init->SetInitPos(GetBufferDataInitPos());

    auto rwb = SpxQueryInterface<ISpxReadWriteBuffer>(init);
    m_ringBuffer.SetDelegate(rwb);
}

void CSpxBufferData::TermRingBuffer()
{
    if (m_ringBuffer.IsClear())
    {
        return;
    }

    SpxTermAndClearDelegate(m_ringBuffer);
    SPX_DBG_ASSERT(m_ringBuffer.IsClear());
}

size_t CSpxBufferData::GetBufferDataSize()
{
    auto properties = SpxGetSiteQueryService<ISpxNamedProperties>(this);
    auto size = properties->GetOr<size_t>("BufferDataSizeInBytes", GetDefaultBufferDataSize());
    return size;
}

ISpxReadWriteBufferInit::OverflowBehavior CSpxBufferData::GetBufferAllowOverflow()
{
    auto properties = SpxGetSiteQueryService<ISpxNamedProperties>(this);
    auto bufferAllowOverflow = properties->GetOr<bool>("BufferAllowOverflow", false);
    using Behavior = ISpxReadWriteBufferInit::OverflowBehavior;
    return bufferAllowOverflow ? Behavior::AllowWithTraces : Behavior::DoNotAllow;
}

uint64_t CSpxBufferData::GetDefaultBufferDataSize()
{
    auto properties = SpxGetSiteQueryService<ISpxNamedProperties>(this);
    auto numberOfChannels = properties->GetOr<uint64_t>(PropertyId::AudioConfig_NumberOfChannelsForCapture, 0);
    auto sampleRate = properties->GetOr<uint64_t>(PropertyId::AudioConfig_SampleRateForCapture, 0);
    auto bitsPerSample = properties->GetOr<uint64_t>(PropertyId::AudioConfig_BitsPerSampleForCapture, 0);

    uint64_t bytesPerSecond = 0;
    if (bitsPerSample == 4)
    {
        bytesPerSecond = (sampleRate * numberOfChannels) / 2;
    }
    else
    {
        bytesPerSecond = sampleRate * (bitsPerSample >> 3) * numberOfChannels;
    }

    return DEFAULT_AUDIO_SOURCE_BUFFER_DATA_SIZE_SECONDS * bytesPerSecond;
}

uint64_t CSpxBufferData::GetBufferDataInitPos()
{
    return GetOffset();
}

void CSpxBufferData::InitDelegatePtr(std::shared_ptr<ISpxBufferProperties>& ptr)
{
    auto site = ISpxInterfaceBase::QueryInterface<ISpxGenericSite>();
    ptr = SpxCreateObjectWithSite<ISpxBufferProperties>("CSpxBufferProperties", site);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
