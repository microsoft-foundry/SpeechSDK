//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "../stdafx.h"
#include "interface_delegate_helpers.h"
#include "ispxinterfaces.h"
#include "interfaces/i_web_socket_init.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class DelegateToHelperT = CSpxDelegateToSharedPtrHelper<ISpxWebSocketInit>>
class CSpxWebSocketInitDelegateHelper :
    public DelegateToHelperT
{
private:

    using I = ISpxWebSocketInit;
    using C = CSpxWebSocketInitDelegateHelper<DelegateToHelperT>;

public:

    SPX_DELEGATE_ACCESSORS(CSpxWebSocketInit, DelegateToHelperT, ISpxWebSocketInit);

    void DelegateInit(
        const std::shared_ptr<ISpxThreadService>& threadService,
        const ISpxThreadService::Affinity affinity,
        const std::chrono::milliseconds& pollingIntervalMs,
        const std::shared_ptr<ISpxWebSocketTelemetry>& telemetry,
        const std::shared_ptr<ISpxHttpErrorHandler>& httpErrorHandler = nullptr)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::Init, threadService, affinity, pollingIntervalMs, telemetry, httpErrorHandler);
    }

    void DelegateSetPollingInterval(const std::chrono::milliseconds& pollingIntervalMs)
    {
        InvokeOnDelegate(C::GetDelegate(), &I::SetPollingInterval, pollingIntervalMs);
    }

};
}}}} // Microsoft::CognitiveServices::Speech::Impl
