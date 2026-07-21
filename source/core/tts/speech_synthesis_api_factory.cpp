//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speech_synthesis_api_factory.cpp: Implementation definitions for CSpxSpeechSynthesisApiFactory C++ class
//

#include "stdafx.h"
#include "spxcore_common.h"
#include "create_object_helpers.h"
#include "speech_synthesis_api_factory.h"
#include "site_helpers.h"
#include "property_id_2_name_map.h"
#include "synthesis_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


std::shared_ptr<ISpxSynthesizer> CSpxSpeechSynthesisApiFactory::CreateSpeechSynthesizerFromConfig(std::shared_ptr<ISpxAudioConfig> audioConfig)
{
    auto factoryAsSite = SpxSiteFromThis(this);

    // Create the synthesizer
    auto synthesizer = SpxCreateObjectWithSite<ISpxSynthesizer>("CSpxSynthesizer", factoryAsSite);

    // Set the synthesizer properties
    auto namedProperties = SpxQueryService<ISpxNamedProperties>(synthesizer);

    std::shared_ptr<ISpxAudioOutput> output = nullptr;

    if (audioConfig != nullptr)
    {
        // See if we have a file, a stream, or neither, so we can initialize the synthesizer correctly...
        auto fileName = audioConfig->GetFileName();
        auto stream = audioConfig->GetStream();

        if (stream != nullptr)
        {
            // Set stream as output
            output = SpxQueryInterface<ISpxAudioOutput>(stream);
        }
        else if (fileName.length() > 0)
        {
            // Set file as output
            auto audioFileWriter = SpxCreateObjectWithSite<ISpxAudioFile>("CSpxWavFileWriter", SpxSiteFromThis(this));
            audioFileWriter->Open(fileName.data());
            output = SpxQueryInterface<ISpxAudioOutput>(audioFileWriter);
        }
        else
        {
            // Set default speaker as output
            output = SpxCreateObjectWithSite<ISpxAudioOutput>("CSpxDefaultSpeaker", SpxSiteFromThis(this));
            SPX_THROW_HR_IF(SPXERR_AUDIO_SYS_LIBRARY_NOT_FOUND, output == nullptr);
        }
    }
    else
    {
        // Set an empty audio output which does nothing, this is used when user just wants to get the output from synthesis result
        output = SpxCreateObjectWithSite<ISpxAudioOutput>("CSpxNullAudioOutput", SpxSiteFromThis(this));
    }

    // Set output
    synthesizer->SetOutput(output);

    // We're done!
    return synthesizer;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
