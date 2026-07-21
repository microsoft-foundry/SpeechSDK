//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <array>
#include <sstream>
#include <thread>
#include <limits>

#include "guid_utils.h"
#include "i_pipe.h"
#include "handle_wrapper.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

using namespace std::chrono_literals;

namespace
{
    using namespace Azure::AI::Test::Tools;

    constexpr size_t DEFAULT_BUFFER_SIZE = 1024;

    DWORD ToPipeDirection(IPipe::Direction dir)
    {
        switch (dir)
        {
        case IPipe::Direction::Read:        return PIPE_ACCESS_INBOUND;
        case IPipe::Direction::ReadWrite:   return PIPE_ACCESS_DUPLEX;
        case IPipe::Direction::Write:       return PIPE_ACCESS_OUTBOUND;
        }

        return 0;
    }
}

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    class WindowsPipe : public IPipe
    {
    private:
        std::string m_name;
        HandleWrapper<HANDLE> m_handle;

    public:
        WindowsPipe(const std::string& name, IPipe::Direction dir) :
            m_name(name),
            m_handle([](HANDLE handle) { if (handle) { CloseHandle(handle); } })
        {
            m_handle = CreateNamedPipeA(
                name.c_str(),
                ToPipeDirection(dir),
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT,
                1,
                DEFAULT_BUFFER_SIZE,
                DEFAULT_BUFFER_SIZE,
                0,
                nullptr);

            if (m_handle == INVALID_HANDLE_VALUE)
            {
                DWORD error = GetLastError();
                std::stringstream oss;
                oss << "Failed to create named pipe '" << name << "'. Error: " << error
                    << " (0x" << std::hex << error << ")";

                throw std::runtime_error(oss.str());
            }
        }

        virtual void Connect(const std::chrono::milliseconds& timeout) override
        {
            auto stopAt = std::chrono::steady_clock::now() + timeout;

            while (std::chrono::steady_clock::now() <= stopAt)
            {
                BOOL boolResult = ConnectNamedPipe(m_handle, nullptr);
                if (boolResult)
                {
                    return;
                }

                DWORD error = GetLastError();
                switch (error)
                {
                case ERROR_PIPE_CONNECTED:
                    return;

                case ERROR_PIPE_LISTENING:
                case ERROR_NO_DATA:
                    std::this_thread::sleep_for(500ms);
                    break;

                default:
                {
                    std::ostringstream oss;
                    oss << "Failed to connect to named pipe '" << m_name << "'. Error "
                        << error << " (0x" << std::hex << error << ")";

                    throw std::runtime_error(oss.str());
                }
                }
            }

            std::ostringstream oss;
            oss << "Timed out trying to connect to named pipe '" << m_name << "'";
            throw std::runtime_error(oss.str());
        }

        virtual size_t Read(uint8_t* buffer, size_t bufferSize, const std::chrono::milliseconds& timeout) override
        {
            auto stopAt = std::chrono::steady_clock::now() + timeout;

            DWORD dwBytesRead = 0;
            DWORD dwBufferSize = bufferSize > std::numeric_limits<DWORD>::max()
                ? std::numeric_limits<DWORD>::max()
                : static_cast<DWORD>(bufferSize);

            while (std::chrono::steady_clock::now() <= stopAt)
            {
                BOOL boolResult = ReadFile(m_handle, buffer, dwBufferSize, &dwBytesRead, nullptr);
                if (boolResult)
                {
                    return static_cast<size_t>(dwBytesRead);
                }

                DWORD error = GetLastError();
                switch (error)
                {
                case ERROR_NO_DATA:
                case ERROR_IO_PENDING:
                    std::this_thread::sleep_for(500ms);
                    break;

                case ERROR_BROKEN_PIPE:
                {
                    std::ostringstream oss;
                    oss << "Failed to read from named pipe '" << m_name << "'. Client disconnected";
                    throw std::runtime_error(oss.str());
                }

                default:
                {
                    std::ostringstream oss;
                    oss << "Failed to read from named pipe '" << m_name << "'. Error "
                        << error << " (0x" << std::hex << error << ")";
                    throw std::runtime_error(oss.str());
                }
                }
            }

            std::ostringstream oss;
            oss << "Timed out trying to read from named pipe '" << m_name << "'";
            throw std::runtime_error(oss.str());
        }
    };

    std::unique_ptr<IPipe> CreateServerPipe(const std::string& name, IPipe::Direction dir)
    {
        return std::make_unique<WindowsPipe>(name, dir);
    }

}}}}
