//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// turn_status_event_args.h: Implementation declaration for CSpxTurnStatusEventArgs C++ class.
//
#pragma once

#include "ispxinterfaces.h"
#include "interface_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


class CSpxTurnStatusEventArgs :
    public ISpxTurnStatusEventArgs,
    public ISpxTurnStatusEventArgsInit
{
public:

    CSpxTurnStatusEventArgs();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxTurnStatusEventArgs)
        SPX_INTERFACE_MAP_ENTRY(ISpxTurnStatusEventArgsInit)
    SPX_INTERFACE_MAP_END()

    // --- ISpxTurnStatusEventArgs
    virtual const std::string& GetInteractionId() const final;
    virtual const std::string& GetConversationId() const final;
    virtual int GetStatusCode() const final;

    // --- ISpxTurnStatusEventArgsInit
    virtual void Init(std::string interactionId, std::string conversationId, int status) final;

private:

    CSpxTurnStatusEventArgs(const CSpxTurnStatusEventArgs&) = delete;
    CSpxTurnStatusEventArgs(CSpxTurnStatusEventArgs&&) = delete;

    CSpxTurnStatusEventArgs& operator=(const CSpxTurnStatusEventArgs&) = delete;

    std::string m_interactionId;
    std::string m_conversationId;
    int m_status;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
