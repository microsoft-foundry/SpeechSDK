//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// compressed_audio_adapter.h: Implementation declarations for CSpxCompressedAdapter C++ class
//

#pragma once
#include "spxcore_common.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    class CSpxCompressedAudioAdapter
    {
    public:

        CSpxCompressedAudioAdapter(std::shared_ptr<ISpxAudioStreamReader> streamReader);
        void StartCompressedPump(std::shared_ptr<ISpxAudioProcessor> pISpxAudioProcessor);
        void StopCompressedPump();
        ~CSpxCompressedAudioAdapter();
    private:
        void PumpThread(std::shared_ptr<ISpxAudioProcessor> pISpxAudioProcessor);
        std::shared_ptr<ISpxAudioStreamReader> m_streamReader;
        std::thread m_thread;
        std::atomic<bool> m_stopCompressedPump{ false };
    };
} } } } // Microsoft::CognitiveServices::Speech::Impl
