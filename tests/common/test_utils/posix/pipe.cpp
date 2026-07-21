//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <i_pipe.h>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <thread>
#include <limits>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std::chrono_literals;

namespace
{
    const std::chrono::milliseconds POLL_PERIOD{ 250 };
}

std::string ErrnoAsString(int);

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    class PosixPipe : public IPipe
    {
    public:
        PosixPipe(const std::string& name, IPipe::Direction dir) :
            m_dir(dir), m_pipeName(name), m_fd(0)
        {
            if (dir == IPipe::Direction::ReadWrite)
            {
                throw new std::runtime_error("Read/Write mode is not supported for Posix named pipes (aka FIFO)");
            }

            int status = mkfifo(name.c_str(), 0666 /* owner, group, and other can all read/write*/);
            if (status)
            {
                auto error = errno;
                if (error != EEXIST)
                {
                    throw std::runtime_error("Failed to create named pipe. " + ErrnoAsString(error));
                }
            }
        }

        virtual ~PosixPipe()
        {
            if (m_fd)
            {
                // deliberately ignore return value
                close(m_fd);
            }

            if (!m_pipeName.empty())
            {
                unlink(m_pipeName.c_str());
            }
        }

        virtual void Connect(const std::chrono::milliseconds& timeout) override
        {
            int openFlags = 0;
            switch (m_dir)
            {
                case IPipe::Direction::Write:
                    openFlags |= O_WRONLY;
                    break;

                case IPipe::Direction::Read:
                    openFlags |= O_RDONLY;
                    break;

                default:
                    throw std::runtime_error("Named pipe mode is not supported");
            }

            // If you open a FIFO pipe for reading/writing, it will block until there is a writer/reader (respectively)
            // on the other end. I am using O_NDELAY in the open flags to move to non-blocking mode, though it may be
            // possible to implement this in a better way using e.g. poll().

            // There are two side effects from O_NDELAY:
            // - Opening a named pipe (FIFO) for write will always return immediately even if there is no reader on the
            //   other end. Opening a named pipe for read will fail with ENXIO
            // - Reading from the pipe is no longer a blocking operation, and will behave in one of two different ways:
            //   - Return 0 if there is no data read. This unfortunately makes it difficult to distinguish when there is no
            //     data to read, or when the pipe has closed. From our perspective though this distinction is not important
            //   - Return -1, with errno set to EAGAIN/EWOULDBLOCK
            openFlags |= O_NDELAY;

            std::chrono::steady_clock::time_point stopAt = timeout < 0ms
                ? std::chrono::steady_clock::time_point::max()
                : std::chrono::steady_clock::now() + timeout;
            
            while (true)
            {
                int status = open(m_pipeName.c_str(), openFlags);
                if (status < 0)
                {
                    auto error = errno;
                    if (error == ENXIO || error == EAGAIN)
                    {
                        if (stopAt > std::chrono::steady_clock::now())
                        {
                            throw std::runtime_error(
                                std::string("Timed out waiting for someone to connect to the other end of the pipe. ")
                                + ErrnoAsString(error));
                        }

                        std::this_thread::sleep_for(POLL_PERIOD);
                    }
                    else
                    {
                        throw std::runtime_error("Failed to open pipe. " + ErrnoAsString(error));
                    }
                }
                else
                {
                    m_fd = status;
                    return;
                }
            }
        }

        virtual size_t Read(uint8_t* buffer, size_t bufferSize, const std::chrono::milliseconds& timeout) override
        {
            // Please see note in Connect() function for more details
        
            bufferSize = std::min(bufferSize, (size_t)std::numeric_limits<ssize_t>::max());
            std::chrono::steady_clock::time_point stopAt = timeout < 0ms
                ? std::chrono::steady_clock::time_point::max()
                : std::chrono::steady_clock::now() + timeout;

            while (true)
            {
                ssize_t numBytes = read(m_fd, buffer, bufferSize);
                int error = errno;
                if (numBytes > 0)
                {
                    return static_cast<size_t>(numBytes);
                }
                else if (numBytes == 0 || error == EAGAIN || error == EWOULDBLOCK)
                {
                    // There seems to be slight differences in behaviour across OSs when the O_NDELAY flag is set
                    // and we are trying to read when there is no data yet written. Some seem to return from reads
                    // immediately with 0 to indicate no bytes read, others (e.g. MacOS) seem to return -1 with
                    // errno set to EAGAIN or EWOULDBLOCK
                    if (std::chrono::steady_clock::now() > stopAt)
                    {
                        throw std::runtime_error(
                            "Timed out while waiting to read data from named pipe. "
                            + ErrnoAsString(error));
                    }

                    std::this_thread::sleep_for(POLL_PERIOD);
                }
                else /* numBytes < 0 */
                {
                    throw std::runtime_error("Failed to read from named pipe. " + ErrnoAsString(error));
                }
            }
        }

    private:
        IPipe::Direction m_dir;
        std::string m_pipeName;
        int m_fd;
    };

    std::unique_ptr<IPipe> CreateServerPipe(const std::string& name, IPipe::Direction dir)
    {
        return std::make_unique<PosixPipe>(name, dir);
    }

}}}}
