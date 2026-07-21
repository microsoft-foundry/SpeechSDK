//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "web_socketInit_delegate_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <typename DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxWebSocketInit>>
class ISpxWebSocketInitDelegateImpl :
    protected CSpxWebSocketInitDelegateHelper<DelegateToHelperT>,
    public ISpxWebSocketInit
{
private:

    using D = CSpxWebSocketInitDelegateHelper<DelegateToHelperT>;

public:

    void Init(
        const std::shared_ptr<ISpxThreadService>& threadService,
        const ISpxThreadService::Affinity affinity,
        const std::chrono::milliseconds& pollingIntervalMs,
        const std::shared_ptr<ISpxWebSocketTelemetry>& telemetry,
        const std::shared_ptr<ISpxHttpErrorHandler>& httpErrorHandler = nullptr) override
    {
        D::DelegateInit(threadService, affinity, pollingIntervalMs, telemetry, httpErrorHandler);
    }

    void SetPollingInterval(const std::chrono::milliseconds& pollingIntervalMs) override
    {
        D::DelegateSetPollingInterval(pollingIntervalMs);
    }
};

}}}} // Microsoft::CognitiveServices::Speech::Impl
