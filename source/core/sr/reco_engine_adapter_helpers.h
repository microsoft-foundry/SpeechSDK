//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// reco_engine_adapter_helpers.h: Implementation of helpers
//

#pragma once
#include <ajv.h>
#include <memory>
#include "spxcore_common.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include <interfaces/enum_helpers.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    template<> bool EnumHelpers::TryParse<SegmentationStrategy>(const char* string, SegmentationStrategy& value);

class CSpxRecoEngineAdapterHelpers
{
public:

    static void UpdateServiceResponseJsonResult(std::shared_ptr<ISpxRecognitionResult> result, uint64_t offsetFixup);
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
