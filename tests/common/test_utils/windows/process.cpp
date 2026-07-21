//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <sstream>
#include <limits>

#include "i_process.h"
#include "job.h"
#include "handle_wrapper.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace
{
    void DestroyWindowsHandle(HANDLE handle)
    {
        if (handle)
        {
            CloseHandle(handle);
        }
    }

    std::string GetDirectoryName()
    {
        std::string fileName(MAX_PATH, '\0');
        GetModuleFileNameA(nullptr, &fileName[0], MAX_PATH);
        size_t len = strnlen(fileName.c_str(), MAX_PATH);
        fileName.resize(len);
        auto pos = fileName.find_last_of("\\/");
        if (pos != std::string::npos)
        {
            fileName.resize(pos + 1);
        }

        return fileName;
    }

    bool HasWhiteSpace(const std::string& str)
    {
        for (size_t i = 0; i < str.size(); i++)
        {
            if (isspace(str[i]))
            {
                return true;
            }
        }

        return false;
    }

    std::string GetLastErrorDescription(DWORD error)
    {
        Azure::AI::Test::Tools::HandleWrapper<LPSTR> pDescr([](LPTSTR h) { if (h) LocalFree(h); });

        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
            (LPSTR)&pDescr,
            0,
            NULL);

        if (size == 0)
        {
            // failed to get message
            return "Failed to get error description. Error code: " + std::to_string(GetLastError());
        }

        return std::string{ static_cast<char*>(pDescr), size };
    }
}

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    class WindowsProcess : public IProcess
    {
    private:
        const bool m_terminateOnExit;
        HandleWrapper<HANDLE> m_procHandle;
        HandleWrapper<HANDLE> m_threadHandle;
        std::unique_ptr<WindowsJob> m_job;

    public:
        WindowsProcess(const ProcessStartInfo& startup) :
            m_terminateOnExit(startup.terminateWithParent),
            m_procHandle(DestroyWindowsHandle),
            m_threadHandle(DestroyWindowsHandle),
            m_job()
        {
            std::string currentDir = GetDirectoryName();
            std::string exePath = currentDir + startup.executable;
            const char* pszExePath = startup.executable.empty() ? nullptr : exePath.c_str();
            const char* pszWorkingDir = startup.workingDir.empty() ? currentDir.c_str() : startup.workingDir.c_str();

            STARTUPINFOA startupInfo = { 0 };
            startupInfo.cb = sizeof(STARTUPINFOA);

            PROCESS_INFORMATION processInfo = { 0 };

            std::string args;
            {
                std::ostringstream oss;
                bool first = true;
                for (const auto& arg : startup.args)
                {
                    if (arg.empty())
                    {
                        continue;
                    }
                    else if (first)
                    {
                        first = false;
                    }
                    else
                    {
                        oss << ' ';
                    }

                    bool hasWhiteSpace = HasWhiteSpace(arg);
                    if (hasWhiteSpace) oss << "\"";
                    oss << arg;
                    if (hasWhiteSpace) oss << "\"";
                }

                args = oss.str();
            }

            BOOL result = CreateProcessA(
                pszExePath,                 // full path to application
                &args[0],                   // arguments as a string
                nullptr,                    // process attributes (handle cannot be inherited)
                nullptr,                    // thread attributes (handle cannot be inherited)
                FALSE,                      // don't allow the new child process to inherit handles from parent
                CREATE_BREAKAWAY_FROM_JOB,  // creation flags. This is set to allow us to assign the process to a different job later
                nullptr,                    // inherit current environment
                pszWorkingDir,              // working directory
                &startupInfo,               // additional startup information. Only using defaults
                &processInfo);              // where the information about the created process is stored
            if (!result)
            {
                std::stringstream oss;
                DWORD error = GetLastError();
                oss << "Failed to start test server process. Error: " << error << " (0x" << std::hex << error << ") - "
                    << GetLastErrorDescription(error);
                throw std::runtime_error(oss.str());
            }

            m_procHandle = processInfo.hProcess;
            m_threadHandle = processInfo.hThread;

            if (startup.terminateWithParent)
            {
                m_job = std::make_unique<WindowsJob>();
                m_job->AddProcess(this);
            }
        }

        ~WindowsProcess()
        {
            if (m_terminateOnExit && m_procHandle)
            {
                TerminateProcess(m_procHandle, std::numeric_limits<UINT>::max());
            }
        }

        virtual std::string Handle() const override
        {
            size_t handle = reinterpret_cast<size_t>((HANDLE)m_procHandle);
            return std::to_string(handle);
        }

        virtual int64_t GetExitCode(const std::chrono::milliseconds& timeout = std::chrono::milliseconds(-1))
        {
            DWORD dwTimeoutMs;
            int64_t timeoutMs = timeout.count();
            if (timeoutMs < 0)
            {
                dwTimeoutMs = INFINITE;
            }
            else if (static_cast<uint64_t>(timeoutMs) > std::numeric_limits<DWORD>::max())
            {
                throw std::invalid_argument("Timeout in milliseconds is too large");
            }
            else
            {
                dwTimeoutMs = static_cast<DWORD>(timeoutMs);
            }

            DWORD result = WaitForSingleObject(m_procHandle, dwTimeoutMs);
            switch (result)
            {
            case 0:
            {
                // process has exited, let's get its exit code;
                DWORD exitCode;
                BOOL bResult = GetExitCodeProcess(m_procHandle, &exitCode);
                if (!bResult)
                {
                    DWORD error = GetLastError();
                    std::ostringstream oss;
                    oss << "Failed to get process exit code. Error: " << error << " (0x" << std::hex << error << ")";
                    throw std::runtime_error(oss.str());
                }

                return static_cast<int64_t>(exitCode);
            }

            case WAIT_TIMEOUT:
                throw std::runtime_error("Timed out waiting for the process to complete");

            case WAIT_ABANDONED:
                throw std::runtime_error("Waiting for the process was abandoned");

            default:
            case WAIT_FAILED:
            {
                DWORD error = GetLastError();
                std::ostringstream oss;
                oss << "Failed while waiting for the process to terminate. Error: " << error << " (0x" << std::hex << error << ")";

                throw std::runtime_error(oss.str());
            }
            }
        }
    };

    std::unique_ptr<IProcess> StartProcess(const ProcessStartInfo& info)
    {
        return std::make_unique<WindowsProcess>(info);
    }

    std::string GetCurrentProcessId()
    {
        auto pid = ::GetCurrentProcessId();
        return std::to_string(pid);
    }

}}}}
