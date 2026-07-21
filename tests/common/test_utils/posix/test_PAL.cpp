//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <string>
#include <cstring>
#include <fstream>
#include <iostream>

#include "test_PAL.h"

namespace
{
    constexpr auto TRACER_PID = "TracerPid:";
}

bool TestUtils::m_debugFlag = false;

bool TestUtils::IsDebuggerAttached()
{
    try
    {
        std::ifstream statusFile{ "/proc/self/status" };
        if (!statusFile.is_open())
        {
            return false;
        }

        std::string line;
        while(std::getline(statusFile, line))
        {
            size_t index = line.find(TRACER_PID);
            if (index == std::string::npos)
            {
                continue;
            }

            for (index = index + strlen(TRACER_PID); index < line.size(); index++)
            {
                if (std::isspace(line[index]))
                {
                    continue;
                }
                else
                {
                    return std::isdigit(line[index]) && line[index] != '0';
                }
            }
        }

        statusFile.close();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Failed to determine if debugger is attached. Cause: " << ex.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "Failed to determine if debugger is attached.\n";
    }

    return false;
}

bool TestUtils::GetSystemProxy(ProxyServerInfo&)
{
    return false;
}

void TestUtils::SetDebugFlag(bool debugFlag)
{
    m_debugFlag = debugFlag;
}

bool TestUtils::GetDebugFlag()
{
    return m_debugFlag;
}

std::string ErrnoAsString(int errNo)
{
    return std::string("Error: [")
        + std::to_string(errNo) + "] "
        + strerror(errNo);
}