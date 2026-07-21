//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <stdint.h>
#include <speechapi_c_audio_stream_format.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

#ifndef WAVE_FORMAT_PCM // suppresses C2059 on Windows, compiling *audio*.cpp files
    static constexpr unsigned short WAVE_FORMAT_PCM = 1;
#endif

#ifndef WAVE_FORMAT_ALAW // suppresses C2059 on Windows, compiling *audio*.cpp files
    static constexpr unsigned short WAVE_FORMAT_ALAW = 6;
#endif

#ifndef WAVE_FORMAT_MULAW // suppresses C2059 on Windows, compiling *audio*.cpp files
    static constexpr unsigned short WAVE_FORMAT_MULAW = 7;
#endif

#ifndef WAVE_FORMAT_FLAC // suppresses C2059 on Windows, compiling *audio*.cpp files
    static constexpr unsigned short WAVE_FORMAT_FLAC = static_cast<uint16_t>(Audio_Stream_Container_Format::StreamFormat_Flac);
#endif

#ifndef WAVE_FORMAT_OPUS // suppresses C2059 on Windows, compiling *audio*.cpp files
    static constexpr unsigned short WAVE_FORMAT_OPUS = 674;
#endif

#ifndef WAVE_FORMAT_AMR_WB // suppresses C2059 on Windows, compiling *audio*.cpp files
    // https://en.wikipedia.org/wiki/Adaptive_Multi-Rate_Wideband
    static constexpr unsigned short WAVE_FORMAT_AMR_WB = 675;
#endif

    static constexpr unsigned short WAVE_FORMAT_SIREN = 654;
    static constexpr unsigned short WAVE_FORMAT_G722 = 655;
    static constexpr unsigned short WAVE_FORMAT_MP3 = static_cast<uint16_t>(Audio_Stream_Container_Format::StreamFormat_Mp3);
    static constexpr unsigned short WAVE_FORMAT_SILK_SKYPE = 671;
    static constexpr unsigned short WAVE_FORMAT_OGG_OPUS = static_cast<uint16_t>(Audio_Stream_Container_Format::StreamFormat_Ogg_Opus);
    static constexpr unsigned short WAVE_FORMAT_WEBM_OPUS = 673;
    static constexpr unsigned short WAVE_FORMAT_G722_TTS = 674;
    static constexpr unsigned short BITS_PER_SAMPLE = 16;
    static constexpr unsigned short CHANNELS = 1;
    static constexpr unsigned short SAMPLES_PER_SECOND = 16000;
    static constexpr unsigned short BLOCK_ALIGN = (BITS_PER_SAMPLE >> 3)*CHANNELS;
    static constexpr unsigned short AVG_BYTES_PER_SECOND = BLOCK_ALIGN * SAMPLES_PER_SECOND;

} } } } // Microsoft::CognitiveServices::Speech::Impl
