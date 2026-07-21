//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#if defined(ANDROID) || defined(__ANDROID__)
#define _ANDROID_LEGACY_SIGNAL_INLINES_H_
#endif

#include "debug_utils.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <functional>
#include "string_utils.h"
#include <algorithm>
#include <iostream>
#include <signal.h>
#include <speechapi_cxx_utils.h>

#ifdef _WIN32
#include <windows.h>
#include <WinBase.h>
#pragma warning(push)
#pragma warning(disable: 4091) // 'typedef ': ignored on left of '' when no variable is declared
#include "DbgHelp.h"
#pragma warning(pop)
#undef min
#elif defined(ANDROID) || defined(__ANDROID__) // execinfo.h not available on Android.
#include <iostream>
#include <iomanip>
#include <unwind.h>
#include <dlfcn.h>

struct AndroidStackFrame
{
    void** current;
    void** end;
    };

static _Unwind_Reason_Code AndroidUnwindCallback(struct _Unwind_Context* context, void* arg)
{
    AndroidStackFrame* state = static_cast<AndroidStackFrame*>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        }
        else {
            *state->current++ = reinterpret_cast<void*>(pc);
        }
    }
    return _URC_NO_REASON;
}
#elif !defined(EMSCRIPTEN)  // execinfo.h not available on Emscripten.
#include <execinfo.h>
#include <cxxabi.h>
#endif

namespace Debug {

// What follows was lifted from CNTK (see ExceptionWithCallStack.cpp).
#ifdef _WIN32
static std::string FormatWin32Error(DWORD error)
{
    wchar_t buf[1024] = { 0 };
    ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, "", error, 0, buf, sizeof(buf) / sizeof(*buf) - 1, NULL);
    std::wstring res(buf);
    // eliminate newlines (and spaces) from the end
    size_t last = res.find_last_not_of(L" \t\r\n");
    if (last != std::string::npos)
        res.erase(last + 1, res.length());
    return PAL::ToString(res);
}
#endif

/// <summary>This function collects the stack tracke and writes it through the provided write function
/// <param name="write">Function for writing the text associated to a the callstack</param>
/// </summary>
static void CollectCallStack(size_t skipLevels, const std::function<void(std::string)>& write)
{
    write("\n[CALL STACK BEGIN]\n");

#ifdef _WIN32
    static std::recursive_mutex _dbgHelpMutex;

    static constexpr ULONG MAX_CALL_STACK_DEPTH = 20;
    static constexpr size_t MAX_NAME_LEN = 255;

    // RtlCaptureStackBackTrace() is a kernel API without default binding, we must manually determine its function pointer.
    typedef USHORT(WINAPI * CaptureStackBackTraceType)(__in ULONG, __in ULONG, __out PVOID*, __out_opt PULONG);
    auto kernelLib = LoadLibrary("kernel32.dll");
    CaptureStackBackTraceType RtlCaptureStackBackTrace = nullptr;
    if (kernelLib != NULL) {
        RtlCaptureStackBackTrace = (CaptureStackBackTraceType)(GetProcAddress(kernelLib, "RtlCaptureStackBackTrace"));
    }

    if (RtlCaptureStackBackTrace == nullptr) // failed somehow
    {
        return write("Failed to generate CALL STACK. GetProcAddress(\"RtlCaptureStackBackTrace\") failed with error " + FormatWin32Error(GetLastError()) + "\n");
    }

    HANDLE process = GetCurrentProcess();

    {
        // NOTE: As per MSDN documentation, all DbgHelp functions are single threaded. Calls from multiple threads
        //       to functions could result in memory corruption or other unexpected behaviour:
        //       https://learn.microsoft.com/windows/win32/api/dbghelp/nf-dbghelp-syminitialize
        //       https://learn.microsoft.com/windows/win32/api/dbghelp/nf-dbghelp-symfromaddr
        //       A recursive lock here seems like the simplest band-aid fix for now while taking into account a single
        //       thread somehow recursively calling this function. No analysis has been made to assess the performance
        //       impact of this, but ensuring correctness and not seg-faulting is more important in the short term.
        std::lock_guard<std::recursive_mutex> lock(_dbgHelpMutex);

        // NOTE: This does not appear to be the best usage of SymInitialize. Please refer to the Remarks section in the
        //       MSDN documentation for more information. Potential issues:
        //       - This should generally be called only once per process (e.g. from DLL init)
        //       - Potentially instead of calling SymInitialize every time, we should instead either be calling SymLoadModuleEx
        //         each time we manually use LoadLibrary(), or calling SymRefreshModuleList here to reload all modules.
        //         More info: https://learn.microsoft.com/windows/win32/debug/symbol-handler-initialization
        //       - By default the paths searched for symbols doesn't automatically include the directory containing the
        //         the executable
        if (!SymInitialize(process, nullptr, TRUE))
        {
            return write("Failed to generate CALL STACK. SymInitialize() failed with error " + FormatWin32Error(GetLastError()) + "\n");
        }

        auto symCleanupScopeGuard = Microsoft::CognitiveServices::Speech::Utils::MakeScopeGuard([&process]()
        {
            SymCleanup(process);
        });

        // get the call stack
        void* callStack[MAX_CALL_STACK_DEPTH];
        unsigned short frames;
        frames = RtlCaptureStackBackTrace(0, MAX_CALL_STACK_DEPTH, callStack, nullptr);

        uint8_t symbolInfoArray[sizeof(SYMBOL_INFO) + (MAX_NAME_LEN * sizeof(TCHAR))];
        SYMBOL_INFO* symbolInfo = reinterpret_cast<SYMBOL_INFO*>(&symbolInfoArray);

        symbolInfo->MaxNameLen = (ULONG)MAX_NAME_LEN;
        symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);

        // format and emit
        std::string callStackLine;
        size_t firstFrame = skipLevels + 1; // skip CollectCallStack()
        for (size_t i = firstFrame; i < frames; i++)
        {
            callStackLine.clear();
            if (i == firstFrame)
                callStackLine += "    > ";
            else
                callStackLine += "    - ";

            if (SymFromAddr(process, (DWORD64)(callStack[i]), 0, symbolInfo))
            {
                callStackLine += std::string(symbolInfo->Name, symbolInfo->NameLen);
                write(callStackLine);
            }
            else
            {
                DWORD error = GetLastError();
                char buf[17];
                sprintf_s(buf, "%p", callStack[i]);
                callStackLine += buf;
                callStackLine += " (SymFromAddr() error: ";
                callStackLine += FormatWin32Error(error);
                callStackLine += ")";
                write(callStackLine);
            }
        }

        write({});
    }

#elif !defined(ANDROID) && !defined(__ANDROID__) && !defined(EMSCRIPTEN)

    const unsigned int MAX_NUM_FRAMES = 20;
    void* backtraceAddresses[MAX_NUM_FRAMES];
    unsigned int numFrames = backtrace(backtraceAddresses, MAX_NUM_FRAMES);
    char** symbolList = backtrace_symbols(backtraceAddresses, numFrames);

    for (size_t i = skipLevels; i < numFrames; i++)
    {
        // Find parentheses and +address offset surrounding the mangled name

        std::string current(symbolList[i]);

        auto beginName = current.find('(');
        auto beginOffset = current.find('+', beginName);

        std::ostringstream buffer;

        if (beginName != std::string::npos && beginOffset != std::string::npos && beginName < beginOffset)
        {
            buffer << current.substr(0, beginName + 1);
            auto mangled_name = current.substr(beginName + 1, beginOffset - beginName - 1);
            int status = 0;
            char* ret = abi::__cxa_demangle(mangled_name.c_str(), NULL, NULL, &status);
            if (status == 0)
                buffer << ret;
            else
                buffer << mangled_name;
            free(ret);
            buffer << current.substr(beginOffset);
        }
        else // Nothing to demangle. Print the whole line as it came.
            buffer << current;

        write(buffer.str());
    }

    free(symbolList);
#elif defined(EMSCRIPTEN)
    UNUSED(skipLevels);
    // execinfo.h not available on Emscripten.
    // stack trace only available in JS land or ASAN build with libunwind.
    // anyhow stack will be printed through console.error or window.onerror.
    // sufficient for the purpose of this function (test only).
#else
    void *androidStackFrames[32];
    AndroidStackFrame state = { &androidStackFrames[0], &androidStackFrames [31] };
    _Unwind_Backtrace(AndroidUnwindCallback, &state);

    std::ostringstream os;
    size_t numStackFramesValid = state.current - androidStackFrames;

    for (size_t idx = skipLevels; idx < numStackFramesValid; ++idx)
    {
        const void* addr = androidStackFrames[idx];
        const char* symbol = "???";

        Dl_info info;
        if (dladdr(addr, &info) && info.dli_sname)
        {
            symbol = info.dli_sname;
        }

        os << "  #" << std::setw(2) << (idx - skipLevels) << ": " << addr << "  " << symbol << "\n";
    }
    write(os.str().c_str());
#endif

    write("[CALL STACK END]\n");
}


std::string GetCallStack(size_t skipLevels/* = 0*/)
{
    try
    {
        std::ostringstream buffer;
        CollectCallStack(skipLevels + 1/*skip this function*/, [&](const std::string& stack)
        {
            buffer << stack << "\n";
        });
        return  buffer.str();
    }
    catch (...) // since we run as part of error reporting, don't get hung up on our own error
    {
        return std::string();
    }
}


// In case of Android, we claim to run with API level 19 and later.
// The problem with this is that some functions were not availble
// on the earlier versions (19-21) while part of the libc in later
// api versions.
// So, to support the old as well as the newer api levels, we provide
// stub functions with "weak" attributes for the linker to link against
// in case no other function is found.
// Please note that this is not a 100% solution since the Android
// linker takes "the first" function with a given name, not the first
// non-weak, only falling back to the weak function if no other has
// been found.
// That means, even if the libc provides the functions below, we could
// still end up with our stubs.
// Second note: signal() is only used for debugging and to hide
// pipe signals in the networking code.
// The tc*etattr() are only used to configure the terminal - which is
// not used in our library.
// The pthread_setclock() is used to select a particular clock used for
// timestamps.
#if defined(ANDROID) || defined(__ANDROID__)
#if __ANDROID_API__ < 21
extern "C" sighandler_t __attribute__((weak)) __attribute__((visibility("default"))) signal(int, void(*)(int))
{
    SPX_TRACE_VERBOSE("signal called on weak reference. probably running on api level 19");
    return (sighandler_t)0;
}

extern "C" void __attribute__((weak)) __attribute__((visibility("default"))) tcsetattr(int, int, void*)
{
    SPX_TRACE_VERBOSE("tcsetattr called on weak reference. probably running on api level 19");
}

extern "C" int __attribute__((weak)) __attribute__((visibility("default"))) tcgetattr(int, void*)
{
    SPX_TRACE_VERBOSE("tcgetattr called on weak reference. probably running on api level 19");
    return -1;
}

extern "C" void __attribute__((weak)) __attribute__((visibility("default"))) pthread_condattr_setclock(void *, int)
{
    SPX_TRACE_VERBOSE("pthread_condattr_setclock called on weak reference. probably running on api level 19");
}
#endif
#endif

static void SignalHandler()
{
    auto callstack = Debug::GetCallStack(1);
    SPX_TRACE_ERROR("%s", callstack.c_str());
}

void SignalHandler(int sig) 
{
    SPX_TRACE_ERROR("\nReceived an error signal: %d\n", sig);
    SignalHandler();
}

void HookSignalHandlers()
{
    signal(SIGSEGV, SignalHandler);
    signal(SIGABRT, SignalHandler);

    std::set_terminate(SignalHandler);
}

}
