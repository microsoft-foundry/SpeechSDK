//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <chrono>
#include <string>

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    /// <summary>
    /// Represents a pipe for the current operating system.
    /// </summary>
    class IPipe
    {
    public:
        /// <summary>
        /// The direction of the pipe
        /// </summary>
        enum class Direction
        {
            Read,
            Write,
            ReadWrite
        };

        /// <summary>
        /// Destructor
        /// </summary>
        virtual ~IPipe() = default;

        /// <summary>
        /// Connects to the pipe. By default this is a blocking call until both ends of the pipe are connected.
        /// Will throw exceptions in the case of errors or timeouts
        /// </summary>
        /// <param name="timeout">The maximum amount of time to wait</param>
        virtual void Connect(const std::chrono::milliseconds& timeout) = 0;

        /// <summary>
        /// Reads data from the pipe. This is a blocking call until there is data to read from the pipe
        /// </summary>
        /// <param name="buffer">The buffer to copy the received data into</param>
        /// <param name="bufferSize">The maximum size of the buffer</param>
        /// <param name="timeout">The maximum amount of time to wait for data</param>
        /// <returns>The number of bytes read. Will throw exceptions in the case of errors or timeouts</returns>
        virtual size_t Read(uint8_t* buffer, size_t bufferSize, const std::chrono::milliseconds& timeout) = 0;
    };

    /// <summary>
    /// Creates a new named server pipe
    /// </summary>
    /// <param name="name">The name of the pipe to create</param>
    /// <param name="dir">The direction of this end of the pipe</param>
    /// <returns>The created named pipe. After this clients will be able to connect to the named pipe.
    /// Will throw exceptions in the case of errors</returns>
    std::unique_ptr<IPipe> CreateServerPipe(const std::string& name, IPipe::Direction dir);

}}}}
