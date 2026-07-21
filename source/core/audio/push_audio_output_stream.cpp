//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// push_audio_output_stream.cpp: Implementation definitions for CSpxPushAudioOutputStream C++ class
//

#include "stdafx.h"
#include "push_audio_output_stream.h"
#include <cstring>


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxPushAudioOutputStream::CSpxPushAudioOutputStream()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxPushAudioOutputStream::~CSpxPushAudioOutputStream()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

void CSpxPushAudioOutputStream::SetWriterCallbacks(WriteCallbackFunction_Type writeCallback, CloseCallbackFunction_Type closeCallback)
{
    m_writeCallback = writeCallback;
    m_closeCallback = closeCallback;
}

uint32_t CSpxPushAudioOutputStream::Write(const uint8_t* buffer, uint32_t size)
{
    SPX_TRACE_ERROR_IF(m_closed, "Write called on closed stream. Please make sure the SpeechSynthesizer live until synthesis finishes.");
    AZAC_IFTRUE_THROW_HR(m_closed, AZAC_ERR_INVALID_STATE);
    return m_writeCallback(buffer, size);
}

void CSpxPushAudioOutputStream::Close()
{
    if (m_closed)
    {
        return;
    }

    m_closeCallback();
    m_closed = true;
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
