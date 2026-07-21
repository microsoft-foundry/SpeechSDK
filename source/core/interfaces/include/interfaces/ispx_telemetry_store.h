//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once
#include "base.h"
#include "ispx_telemetry_base.h"
namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {
SPX_INTERFACE(ISpxTelemetryStore)
{
public:
    virtual void StoreTelemetry(ISpxTelemetryBase::Ptr telemetry) = 0;
    virtual ISpxTelemetryBase::Ptr FetchTelemetry() = 0;
};
}}}}
