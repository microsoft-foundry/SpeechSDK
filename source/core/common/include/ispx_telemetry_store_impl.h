//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once
#include "interfaces/ispx_telemetry_store.h"
namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {
class ISpxTelemetryStoreImpl:
    public ISpxTelemetryStore
{
public:
    void StoreTelemetry(ISpxTelemetryBase::Ptr telemetry) override
    {
        m_storedTelemetry = telemetry;
    }
    ISpxTelemetryBase::Ptr FetchTelemetry() override
    {
        return m_storedTelemetry;
    }
private:
    ISpxTelemetryBase::Ptr m_storedTelemetry;
};
}}}}
