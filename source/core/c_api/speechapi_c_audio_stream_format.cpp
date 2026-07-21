//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_audio_stream_format.cpp: Public API definitions for audio stream format related C methods and types

#include "stdafx.h"
#include "create_object_helpers.h"
#include "event_helpers.h"
#include "handle_helpers.h"
#include "platform.h"
#include "site_helpers.h"
#include "string_utils.h"
#include "speechapi_cxx_audio_stream_format.h"

using namespace Microsoft::CognitiveServices::Speech::Audio;
using namespace Microsoft::CognitiveServices::Speech::Impl;

static_assert((int)AudioStreamContainerFormat::MP3 == (int)StreamFormat_Mp3, "AudioStreamContainerFormat should match between C and C++ layers");
static_assert((int)AudioStreamContainerFormat::OGG_OPUS == (int)StreamFormat_Ogg_Opus, "AudioStreamContainerFormat should match between C and C++ layers");
static_assert((int)AudioStreamContainerFormat::FLAC == (int)StreamFormat_Flac, "AudioStreamContainerFormat should match between C and C++ layers");
static_assert((int)AudioStreamContainerFormat::ALAW == (int)StreamFormat_Alaw, "AudioStreamContainerFormat should match between C and C++ layers");
static_assert((int)AudioStreamContainerFormat::MULAW == (int)StreamFormat_Mulaw, "AudioStreamContainerFormat should match between C and C++ layers");
static_assert((int)AudioStreamContainerFormat::AMRNB == (int)StreamFormat_Amrnb, "AudioStreamContainerFormat should match between C and C++ layers");
static_assert((int)AudioStreamContainerFormat::AMRWB == (int)StreamFormat_Amrwb, "AudioStreamContainerFormat should match between C and C++ layers");
static_assert((int)AudioStreamContainerFormat::ANY == (int)StreamFormat_Any, "AudioStreamContainerFormat should match between C and C++ layers");

SPXAPI_(bool) audio_stream_format_is_handle_valid(SPXAUDIOSTREAMFORMATHANDLE hformat)
{
    return CSpxApiManager::IsValid<SPXAUDIOSTREAMFORMATHANDLE, SPXWAVEFORMATEX>(hformat);
}

SPXAPI audio_stream_format_create_from_default_input(SPXAUDIOSTREAMFORMATHANDLE* hformat)
{
    return audio_stream_format_create_from_waveformat(hformat, 16000, 16, 1, Audio_Stream_Wave_Format::StreamWaveFormat_PCM);
}

SPXAPI audio_stream_format_create_from_waveformat_pcm(SPXAUDIOSTREAMFORMATHANDLE* hformat, uint32_t samplesPerSecond, uint8_t bitsPerSample, uint8_t channels)
{
    return audio_stream_format_create_from_waveformat(hformat, samplesPerSecond, bitsPerSample, channels, Audio_Stream_Wave_Format::StreamWaveFormat_PCM);
}

SPXAPI audio_stream_format_create_from_waveformat(SPXAUDIOSTREAMFORMATHANDLE* hformat,
                                                  uint32_t samplesPerSecond,
                                                  uint8_t bitsPerSample,
                                                  uint8_t channels,
                                                  Audio_Stream_Wave_Format waveFormat)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hformat == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *hformat = SPXHANDLE_INVALID;

        auto format = SpxAllocWAVEFORMATEX(sizeof(SPXWAVEFORMATEX));
        format->wFormatTag = (uint16_t)waveFormat;
        format->nChannels = channels;
        format->nSamplesPerSec = samplesPerSecond;
        format->nAvgBytesPerSec = (uint32_t)(samplesPerSecond * (bitsPerSample / 8.0) * channels);
        format->nBlockAlign = (channels * bitsPerSample) < 8 ? 1 : (channels * bitsPerSample) / 8;
        format->wBitsPerSample = bitsPerSample;
        format->cbSize = 0;

        *hformat = CSpxSharedPtrHandleTableManager::TrackHandle<SPXWAVEFORMATEX, SPXAUDIOSTREAMFORMATHANDLE>(format);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_stream_format_create_from_default_output(SPXAUDIOSTREAMFORMATHANDLE* hformat)
{
    return audio_stream_format_create_from_waveformat(hformat, 16000, 16, 1, Audio_Stream_Wave_Format::StreamWaveFormat_PCM);
}

SPXAPI audio_stream_format_create_from_compressed_format(SPXAUDIOSTREAMFORMATHANDLE* hformat, Audio_Stream_Container_Format compressedFormat)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hformat == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *hformat = SPXHANDLE_INVALID;

        auto format = SpxAllocWAVEFORMATEX(sizeof(SPXWAVEFORMATEX));
        memset(format.get(), 0, sizeof(SPXWAVEFORMATEX));
        format->wFormatTag = static_cast<uint16_t>(compressedFormat);
        *hformat = CSpxSharedPtrHandleTableManager::TrackHandle<SPXWAVEFORMATEX, SPXAUDIOSTREAMFORMATHANDLE>(format);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI audio_stream_format_release(SPXAUDIOSTREAMFORMATHANDLE hformat)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXAUDIOSTREAMFORMATHANDLE, SPXWAVEFORMATEX>(hformat);
}
