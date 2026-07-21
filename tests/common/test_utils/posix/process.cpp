//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <i_process.h>
#include <stdexcept>
#include <cstring>
#include <signal.h>
#include <thread>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include "handle_wrapper.h"

#ifndef __APPLE__
#include <sys/prctl.h>
#endif

using namespace std::chrono_literals;

std::string ErrnoAsString(int);

extern "C"
{
    int start_process(const char* pszExec, int argc, char* argv[], pid_t* pPid, int killWithParent)
    {
        (void)argc;

        if (pszExec == nullptr || argv == nullptr || pPid == nullptr)
        {
            return -1;
        }

        pid_t expectedParentPid = getpid();
        pid_t childPid = fork();
        if (childPid == 0)
        {
            // CHILD process

            if (killWithParent)
            {
#ifndef __APPLE__
                // TODO PRCTL  only works on Linux. Figure out a way to make this work on MacOS/BSD based systems
                // using e.g.
                // - kqueue?
                // - or some weird pipe based things?
                // Many options outlined here: https://stackoverflow.com/questions/284325/how-to-make-child-process-die-after-parent-exits
                // or maybe: https://jmmv.dev/2019/11/wait-for-process-group-darwin.html

                // tell the operating system to terminate us if our parent dies
                int ret = prctl(PR_SET_PDEATHSIG, SIGKILL);
                if (ret == -1)
                {
                    fprintf(stderr, ">>>Failed to instruct OS to terminate child process if the parent dies. %s\n", ErrnoAsString(errno).c_str());
                    return -1;
                }
#endif

                // it is possible for the original parent process to have died before we installed the death signal. As per
                // POSIX standards, orphaned child processes will be inherited by an implementation defined system process.
                // So let's check if our parent ID is different than what we expect
                if (getppid() != expectedParentPid)
                {
                    fprintf(stderr, ">>>The parent process has already terminated. Exiting\n");
                    return -1;
                }
            }

            fprintf(stdout, ">>>Launching: '%s'\n", pszExec);

            // launch the executable in question
            execvp(pszExec, argv);

            // If we get here, that means an error occurred
            fprintf(stderr, ">>>Failed to launch '%s'. %s\n", pszExec, ErrnoAsString(errno).c_str());

            return -1;
        }
        else if (childPid < 0)
        {
            // PARENT process
            int error = errno;
            fprintf(stderr, ">>>Failed to create child process '%s'. %s", pszExec, ErrnoAsString(error).c_str());
            return error;
        }
        else
        {
            // PARENT process
            *pPid = childPid;
            return 0;
        }
    }
}

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    class PosixProcess : public IProcess
    {
    private:
        pid_t m_pid{ 0 };

    public:
        PosixProcess(const ProcessStartInfo& startup)
        {
            size_t numArgEntries = startup.args.size()
                + 1     // by convention first one is the program being executed
                + 1;    // by convention, the last entry is a nullptr

            const auto pArgs = (char**)malloc(sizeof(char*) * numArgEntries);
            auto pStartupGuard = MakeScopeGuard([pArgs, numArgEntries]()
            {
                if (pArgs == nullptr)
                {
                    return;
                }

                for (size_t i = 0; i < numArgEntries; i++)
                {
                    auto pszStr = pArgs[i];
                    if (pszStr) free(pszStr);
                }
                
                free(pArgs);
            });

            pArgs[0] = strdup(startup.executable.c_str());
            pArgs[numArgEntries - 1] = nullptr;
            for (size_t i = 0; i < startup.args.size(); i++)
            {
                pArgs[i + 1] = strdup(startup.args[i].c_str());
            }

            int ret = start_process(
                startup.executable.c_str(),
                numArgEntries,
                pArgs,
                &m_pid,
                (int)startup.terminateWithParent);
            if (ret)
            {
                m_pid = 0; // reset pid to 0 just in case
                throw std::runtime_error("Failed to launch process. " + ErrnoAsString(ret));
            }
        }

        virtual ~PosixProcess()
        {
            if (m_pid)
            {
                // deliberately ignore any errors here
                kill(m_pid, SIGKILL);
            }
        }

        virtual std::string Handle() const override
        {
            return std::to_string((size_t)m_pid);
        }

        virtual int64_t GetExitCode(const std::chrono::milliseconds& timeout) override
        {
            if (m_pid <= 1)
            {
                throw std::runtime_error("Invalid child PID");
            }

            // TODO do a non-busy timed wait using signals? * sigh * why is this so unnecessarily convoluted?
            // https://stackoverflow.com/questions/282176/waitpid-equivalent-with-timeout

            // Poll to simulate the timed wait
            std::chrono::steady_clock::time_point stopAt = timeout < 0ms
                ? std::chrono::steady_clock::time_point::max()
                : std::chrono::steady_clock::now() + timeout;
            
            while(true)
            {
                int status = 0;
                pid_t childPid = waitpid(m_pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
                if (childPid == -1)
                {
                    throw std::runtime_error("Failed while waiting process to terminate. " + ErrnoAsString(errno));
                }
                else if (childPid == m_pid)
                {
                    if (WIFEXITED(status))
                    {
                        return WEXITSTATUS(status);
                    }
                    else if (WIFSIGNALED(status))
                    {
                        throw std::runtime_error("Process was aborted via signal " + std::to_string(WTERMSIG(status)));
                    }
                }

                if (std::chrono::steady_clock::now() > stopAt)
                {
                    throw std::runtime_error("Timed out waiting for the process to terminate");
                }

                std::this_thread::sleep_for(250ms);
            }

            // should never get here
            return -1;
        }
    };

    std::unique_ptr<IProcess> StartProcess(const ProcessStartInfo& info)
    {
        return std::make_unique<PosixProcess>(info);
    }

    std::string GetCurrentProcessId()
    {
        auto pid = getpid();
        return std::to_string(pid);
    }

}}}}
