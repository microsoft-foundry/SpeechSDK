//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_format_id_2_name_map.h: internal maping function from id to its name
//

#pragma once

#include "speechapi_cxx_enums.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

constexpr const char* GetAudioFormatName(const SpeechSynthesisOutputFormat& formatId)
{
    switch (formatId)
    {
    case SpeechSynthesisOutputFormat::Raw8Khz8BitMonoMULaw: return "raw-8khz-8bit-mono-mulaw";
    case SpeechSynthesisOutputFormat::Riff16Khz16KbpsMonoSiren: return "riff-16khz-16kbps-mono-siren";
    case SpeechSynthesisOutputFormat::Audio16Khz16KbpsMonoSiren: return "audio-16khz-16kbps-mono-siren";
    case SpeechSynthesisOutputFormat::Audio16Khz32KBitRateMonoMp3: return "audio-16khz-32kbitrate-mono-mp3";
    case SpeechSynthesisOutputFormat::Audio16Khz128KBitRateMonoMp3: return "audio-16khz-128kbitrate-mono-mp3";
    case SpeechSynthesisOutputFormat::Audio16Khz64KBitRateMonoMp3: return "audio-16khz-64kbitrate-mono-mp3";
    case SpeechSynthesisOutputFormat::Audio24Khz48KBitRateMonoMp3: return "audio-24khz-48kbitrate-mono-mp3";
    case SpeechSynthesisOutputFormat::Audio24Khz96KBitRateMonoMp3: return "audio-24khz-96kbitrate-mono-mp3";
    case SpeechSynthesisOutputFormat::Audio24Khz160KBitRateMonoMp3: return "audio-24khz-160kbitrate-mono-mp3";
    case SpeechSynthesisOutputFormat::Raw16Khz16BitMonoTrueSilk: return "raw-16khz-16bit-mono-truesilk";
    case SpeechSynthesisOutputFormat::Riff16Khz16BitMonoPcm: return "riff-16khz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Riff8Khz16BitMonoPcm: return "riff-8khz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Riff24Khz16BitMonoPcm: return "riff-24khz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Riff8Khz8BitMonoMULaw: return "riff-8khz-8bit-mono-mulaw";
    case SpeechSynthesisOutputFormat::Raw16Khz16BitMonoPcm: return "raw-16khz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Raw24Khz16BitMonoPcm: return "raw-24khz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Raw8Khz16BitMonoPcm: return "raw-8khz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Ogg16Khz16BitMonoOpus: return "ogg-16khz-16bit-mono-opus";
    case SpeechSynthesisOutputFormat::Ogg24Khz16BitMonoOpus: return "ogg-24khz-16bit-mono-opus";
    case SpeechSynthesisOutputFormat::Raw48Khz16BitMonoPcm: return "raw-48khz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Riff48Khz16BitMonoPcm: return "riff-48khz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Audio48Khz96KBitRateMonoMp3: return "audio-48khz-96kbitrate-mono-mp3";
    case SpeechSynthesisOutputFormat::Audio48Khz192KBitRateMonoMp3: return "audio-48khz-192kbitrate-mono-mp3";
    case SpeechSynthesisOutputFormat::Ogg48Khz16BitMonoOpus: return "ogg-48khz-16bit-mono-opus";
    case SpeechSynthesisOutputFormat::Webm16Khz16BitMonoOpus: return "webm-16khz-16bit-mono-opus";
    case SpeechSynthesisOutputFormat::Webm24Khz16BitMonoOpus: return "webm-24khz-16bit-mono-opus";
    case SpeechSynthesisOutputFormat::Raw24Khz16BitMonoTrueSilk: return "raw-24khz-16bit-mono-truesilk";
    case SpeechSynthesisOutputFormat::Raw8Khz8BitMonoALaw: return "raw-8khz-8bit-mono-alaw";
    case SpeechSynthesisOutputFormat::Riff8Khz8BitMonoALaw: return "riff-8khz-8bit-mono-alaw";
    case SpeechSynthesisOutputFormat::Webm24Khz16Bit24KbpsMonoOpus: return "webm-24khz-16bit-24kbps-mono-opus";
    case SpeechSynthesisOutputFormat::Audio16Khz16Bit32KbpsMonoOpus: return "audio-16khz-16bit-32kbps-mono-opus";
    case SpeechSynthesisOutputFormat::Audio24Khz16Bit48KbpsMonoOpus: return "audio-24khz-16bit-48kbps-mono-opus";
    case SpeechSynthesisOutputFormat::Audio24Khz16Bit24KbpsMonoOpus: return "audio-24khz-16bit-24kbps-mono-opus";
    case SpeechSynthesisOutputFormat::Raw22050Hz16BitMonoPcm: return "raw-22050hz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Riff22050Hz16BitMonoPcm: return "riff-22050hz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Raw44100Hz16BitMonoPcm: return "raw-44100hz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::Riff44100Hz16BitMonoPcm: return "riff-44100hz-16bit-mono-pcm";
    case SpeechSynthesisOutputFormat::AmrWb16000Hz: return "amr-wb-16000hz";
    case SpeechSynthesisOutputFormat::G72216Khz64Kbps: return "g722-16khz-64kbps";
    default:
        return nullptr;
    }
}


}}}}
