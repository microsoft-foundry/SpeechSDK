//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "i_process.h"
#include "handle_wrapper.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    class WindowsJob
    {
    private:
        HandleWrapper<HANDLE> m_handle;

    public:
        WindowsJob(const std::string& name = {});
        WindowsJob(const WindowsJob&) = delete;
        WindowsJob(WindowsJob&&) = default;

        void AddProcess(const IProcess* process);
    };

}}}}
