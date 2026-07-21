//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include "interfaces/base.h"
#include "interfaces/utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE(ISpxAudioOutput)
{
public:
    virtual uint32_t Write(const uint8_t* buffer, uint32_t size) = 0;
    virtual void WaitUntilDone() = 0;
    virtual void ClearUnread() = 0;
    virtual void Close() = 0;
};

SPX_INTERFACE(ISpxAudioRender)
{
public:
    virtual void StartPlayback() = 0;
    virtual void PausePlayback() = 0;
    virtual void StopPlayback() = 0;
    virtual int64_t GetPlaybackTime() = 0;
    virtual bool IsRenderDeviceAvailable() = 0;
};

} } } }
