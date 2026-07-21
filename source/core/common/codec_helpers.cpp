//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "codec_helpers.h"
#include "create_object_helpers.h"
#include "site_helpers.h"
#include "audio_format_id_2_name_map.h"
#include "synthesis_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    bool IsCodecAdapterAvailable()
    {
#if defined(__ANDROID__) || defined(__APPLE__)
        return true;
#else
        static std::once_flag initOnce;
        static bool available;

        std::call_once(initOnce, []() {
            available = false;
            auto adapter = SpxCreateObject<ISpxAudioStreamReader>("CSpxCodecAdapter", SpxGetRootSite());

            // On Linux (not Android) and Windows, the creating of CSpxCodecAdapter only checks GStreamer core library.
            // However, we need to ensure the plugins are installed, i.e. all required elements could be initialized successfully.
            // We don't need to check this on platforms where GST_PLUGIN_STATIC_DECLARE is used to declare all required plugins.
            // See source/extensions/codec/gstreamer_modules.h and
            // public_samples/speech/samples/objective-c/ios/compressed-streams/GStreamerWrapper/GStreamerWrapper/gstreamer_modules.h
            if (adapter)
            {
                available = true;
                try
                {
                    auto initCallbacks = SpxQueryInterface<ISpxAudioStreamReaderInitCallbacks>(adapter);
                    initCallbacks->SetCallbacks(
                        [=](uint8_t* _buffer, uint32_t _size) { UNUSED(_buffer); return _size; },
                        [=]() {  });
                    auto formatStr = GetAudioFormatName(SpeechSynthesisOutputFormat::Audio24Khz48KBitRateMonoMp3);
                    const auto format = CSpxSynthesisHelper::GetSpeechSynthesisOutputFormatFromString(formatStr);
                    auto adapterAsSetFormat = SpxQueryInterface<ISpxAudioStreamInitFormat>(adapter);
                    adapterAsSetFormat->SetFormat(format.get());
                }
                catch (const std::exception &e)
                {
                    SPX_TRACE_ERROR("CSpxCodecAdapter is successfully created, but open failed, error: %s", e.what());
                    available = false;
                }
            }
        });

        return available;
#endif
    }

}}}}
