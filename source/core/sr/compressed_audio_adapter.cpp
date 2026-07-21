//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// compressed_audio_adapter.cpp: Implementation definitions for CSpxCompressedAudioAdapter C++ class
//


#include "stdafx.h"
#include <memory>
#include <iostream>
#include <istream>
#include <fstream>
#include <thread>
#include "compressed_audio_adapter.h"
#include "service_helpers.h"
#include "property_id_2_name_map.h"

// Most of the time OPUS compressed stream for speech is 32kbps = 32768 bits per second = 4096 bytes per second = 409.6 bytes per 100ms.
// Since we always try to send 100ms data to the speech service. But right now we do not have throttle logic in place. we may need to ask
// the application to put a sleep for 50ms in case of 2x and 100ms in case of 1x
#define COMPRESSED_CHUNK_SIZE 400

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxCompressedAudioAdapter::CSpxCompressedAudioAdapter(std::shared_ptr<ISpxAudioStreamReader> streamReader) :
    m_streamReader(streamReader)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxCompressedAudioAdapter::CSpxCompressedAudioAdapter", (void*)this);
}

CSpxCompressedAudioAdapter::~CSpxCompressedAudioAdapter()
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxCompressedAudioAdapter::~CSpxCompressedAudioAdapter", (void*)this);
}

void CSpxCompressedAudioAdapter::StartCompressedPump(std::shared_ptr<ISpxAudioProcessor> pISpxAudioProcessor)
{
    auto cbFormat = m_streamReader->GetFormat(nullptr, 0);
    auto waveformat = SpxAllocWAVEFORMATEX(cbFormat);
    SPX_TRACE_ERROR_IF(waveformat == nullptr, "CSpxCompressedAudioAdapter::StartCompressedPump(): SpxAllocWAVEFORMATEX(cbFormat) == nullptr !!! Unexpected !!");
    m_streamReader->GetFormat(waveformat.get(), cbFormat);

    SPX_DBG_TRACE_VERBOSE("CSpxCompressedAudioAdapter::StartCompressedPump(): setting format on processor...");
    SPX_TRACE_ERROR_IF(pISpxAudioProcessor == nullptr, "CSpxCompressedAudioAdapter::StartCompressedPump(): pISpxAudioProcessor == nullptr !!! Unexpected !!");
    pISpxAudioProcessor->SetFormat(waveformat.get());
    m_thread = std::thread(&CSpxCompressedAudioAdapter::PumpThread, std::move(this), pISpxAudioProcessor);
    m_thread.detach();
}

void CSpxCompressedAudioAdapter::PumpThread(std::shared_ptr<ISpxAudioProcessor> pISpxAudioProcessor)
{
    while (true)
    {
        std::string capturedTime, userId;
        auto data = SpxAllocSharedAudioBuffer(COMPRESSED_CHUNK_SIZE);
        auto cbRead = m_streamReader->Read(data.get(), COMPRESSED_CHUNK_SIZE);
        pISpxAudioProcessor->ProcessAudio(std::make_shared<DataChunk>(data, cbRead, std::move(capturedTime), std::move(userId)));
        if (cbRead == 0 || m_stopCompressedPump == true)
        {
            pISpxAudioProcessor->SetFormat(nullptr);
            break;
        }
    }
}

void CSpxCompressedAudioAdapter::StopCompressedPump()
{
    m_stopCompressedPump = true;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
