//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    /// <summary>
    /// Represents another process in the current operating system
    /// </summary>
    class IProcess
    {
    public:
        /// <summary>
        /// Destructor. This will usually terminate the process
        /// </summary>
        virtual ~IProcess() = default;

        /// <summary>
        /// Gets the handle to the process as a string. This will be operating system specific
        /// </summary>
        /// <returns>The handle as a string</returns>
        virtual std::string Handle() const = 0;

        /// <summary>
        /// Waits for the process to close up to the timeout, and returns its exit code
        /// </summary>
        /// <param name="timeout">The maximum amount of time to wait. Use a negative value to indicate an infinite wait</param>
        /// <returns>The exit code of the process. Will throw exceptions in the case of errors or timeouts</returns>
        virtual int64_t GetExitCode(const std::chrono::milliseconds& timeout = std::chrono::milliseconds(-1)) = 0;
    };

    /// <summary>
    /// Information about the process that you want to start
    /// </summary>
    struct ProcessStartInfo
    {
        /// <summary>
        /// The path to the executable
        /// </summary>
        std::string executable;

        /// <summary>
        /// All the arguments you wish to pass to the executable
        /// </summary>
        std::vector<std::string> args;

        /// <summary>
        /// The working directory. Leave empty to use the current processes's working directory
        /// </summary>
        std::string workingDir;

        /// <summary>
        /// Whether or not the child process should be automatically terminated when the parent
        /// process is terminated. This will work even in the case of abnormal parent termination
        /// </summary>
        bool terminateWithParent{ true };
    };

    /// <summary>
    /// Starts a new process
    /// </summary>
    /// <param name="startup">The information about the process to start</param>
    /// <returns>The started process. This will throw exceptions in the case of errors</returns>
    std::unique_ptr<IProcess> StartProcess(const ProcessStartInfo& startup);

    /// <summary>
    /// Gets the ID of the current process
    /// </summary>
    /// <returns>The current process identifier</returns>
    std::string GetCurrentProcessId();

}}}}
