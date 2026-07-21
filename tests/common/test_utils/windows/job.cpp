//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <sstream>

#include "job.h"

namespace
{
    void DestroyWindowsHandle(HANDLE handle)
    {
        if (handle)
        {
            CloseHandle(handle);
        }
    }
}

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    WindowsJob::WindowsJob(const std::string& name) :
        m_handle(DestroyWindowsHandle)
    {
        m_handle = CreateJobObjectA(nullptr, name.empty() ? nullptr : name.c_str());
        if (m_handle == nullptr)
        {
            DWORD error = GetLastError();
            std::ostringstream oss;
            oss << "Failed to create job. Error " << error << " (0x" << std::hex << error << ")";
            throw std::runtime_error(oss.str());
        }

        // set all processes associated with job to terminate when the last job handle is destroyed
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION extendedLimitInfo;
        ZeroMemory(&extendedLimitInfo, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
        extendedLimitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        BOOL bResult = SetInformationJobObject(
            m_handle,
            JOBOBJECTINFOCLASS::JobObjectExtendedLimitInformation,
            &extendedLimitInfo,
            sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
        if (!bResult)
        {
            DWORD error = GetLastError();
            std::ostringstream oss;
            oss << "Failed to set extended job information. Error " << error << " (0x" << std::hex << error << ")";
            throw std::runtime_error(oss.str());
        }
    }

    void WindowsJob::AddProcess(const IProcess* process)
    {
        if (process == nullptr)
        {
            throw std::invalid_argument("Passed in process was null");
        }

        std::string stringProcHandle = process->Handle();
        uint64_t parsedProcHandle = std::strtoull(stringProcHandle.c_str(), nullptr, 10);
        if (parsedProcHandle == 0 || parsedProcHandle > std::numeric_limits<size_t>::max())
        {
            throw std::invalid_argument("Process handle is not valid");
        }

        auto hProcess = reinterpret_cast<HANDLE>(parsedProcHandle);

        BOOL result = AssignProcessToJobObject(m_handle, hProcess);
        if (!result)
        {
            DWORD error = GetLastError();
            std::ostringstream oss;
            oss << "Failed to assign process to job. Error " << error << " (0x" << std::hex << error << ")";
            throw std::runtime_error(oss.str());
        }
    }

}}}}
