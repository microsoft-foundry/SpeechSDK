//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "fatal_exit_monitor.h"
#include "memory_logger.h"

FatalExitMonitor& FatalExitMonitor::Instance()
{
    static FatalExitMonitor instance;
    return instance;
}

void FatalExitMonitor::Enable(bool enabled)
{
    if (enabled) Enable();
    if (!enabled) Disable();
}

void FatalExitMonitor::FatalExitDetected(uint32_t kind)
{
    SPX_TRACE_ERROR("FatalExitMonitor::FatalExitDetected, kind=0x%x", kind);
    MemoryLogger::Instance().Exit();
}

#if defined(WIN32) && !defined(SPX_UWP)

ULONG FatalExitMonitor::m_guaranteeStackSize = 0x10000; // 32k
PVOID FatalExitMonitor::m_exceptionHandler = nullptr;

void FatalExitMonitor::Enable()
{
    if (m_exceptionHandler == nullptr)
    {
        m_exceptionHandler = AddVectoredExceptionHandler(1, HandleException);
        SetThreadStackGuarantee(&m_guaranteeStackSize);
    }
}

void FatalExitMonitor::Disable()
{
    if (m_exceptionHandler)
    {
        RemoveVectoredExceptionHandler(m_exceptionHandler);
        m_exceptionHandler = nullptr;
    }
}

LONG CALLBACK FatalExitMonitor::HandleException(PEXCEPTION_POINTERS exceptionInfo)
{
    switch (exceptionInfo->ExceptionRecord->ExceptionCode)
    {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_STACK_OVERFLOW:
            FatalExitDetected(exceptionInfo->ExceptionRecord->ExceptionCode);
            break;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

#elif defined(__clang__) || defined(__GNUG__)

#include <signal.h>

stack_t FatalExitMonitor::m_prevStack = { };
bool FatalExitMonitor::m_enabled = false;
struct sigaction FatalExitMonitor::m_prevHandlers[] = { };
constexpr std::array<int, 6> FatalExitMonitor::m_signals;

void FatalExitMonitor::Enable()
{
    static const auto sigStackSize = 32768 >= MINSIGSTKSZ ? 32768 : MINSIGSTKSZ;
    static std::unique_ptr<char[]> altStackMem(new char[sigStackSize]);

    if (!m_enabled)
    {
        stack_t stack;
        stack.ss_sp = altStackMem.get();
        stack.ss_size = sigStackSize;
        stack.ss_flags = 0;
        sigaltstack(&stack, &m_prevStack);

        struct sigaction sa = { };
        sa.sa_handler = HandleSignal;
        sa.sa_flags = SA_ONSTACK;

        int i = 0;
        for (auto id: m_signals)
        {
            sigaction(id, &sa, &m_prevHandlers[i]);
        }

        m_enabled = true;
    }
}

void FatalExitMonitor::Disable()
{
    if (m_enabled)
    {
        m_enabled = false;
        int i = 0;
        for (auto id: m_signals)
        {
            sigaction(id, &m_prevHandlers[i], nullptr);
        }
        sigaltstack(&m_prevStack, nullptr);
    }
}

void FatalExitMonitor::HandleSignal(int sig)
{
    Disable();
    FatalExitDetected(sig);
    raise(sig);
}

#else

void FatalExitMonitor::Enable()
{
}

void FatalExitMonitor::Disable()
{
}

#endif
