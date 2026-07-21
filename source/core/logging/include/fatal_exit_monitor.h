//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <array>

#if defined(WIN32) && !defined(SPX_UWP)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class FatalExitMonitor
{
public:

    FatalExitMonitor() { Enable(); }
    ~FatalExitMonitor() { Disable(); }

    static FatalExitMonitor& Instance();

    static void Enable(bool enabled);

private:

    static void Enable();
    static void Disable();

    static LONG CALLBACK HandleException(PEXCEPTION_POINTERS exceptionInfo);

    static void FatalExitDetected(uint32_t kind);

    static ULONG m_guaranteeStackSize;
    static PVOID m_exceptionHandler;
};

#elif defined(__clang__) || defined(__GNUG__)

#include <signal.h>

class FatalExitMonitor
{
public:

    FatalExitMonitor() { Enable(); }
    ~FatalExitMonitor() { Disable(); }

    static FatalExitMonitor& Instance();

    static void Enable(bool enabled);

private:

    static void Enable();
    static void Disable();

    static void HandleSignal(int sig);

    static void FatalExitDetected(uint32_t kind);

    static stack_t m_prevStack;
    static bool m_enabled;

    static constexpr std::array<int, 6> m_signals{ SIGINT, SIGILL, SIGFPE, SIGSEGV, SIGTERM, SIGABRT };
    static struct sigaction m_prevHandlers[m_signals.size()];
};

#else

class FatalExitMonitor
{
public:

    FatalExitMonitor() { Enable(); }
    ~FatalExitMonitor() { Disable(); }

    static FatalExitMonitor& Instance();

    static void Enable(bool enabled);

private:

    static void Enable();
    static void Disable();

    static void FatalExitDetected(uint32_t kind);
};

#endif
