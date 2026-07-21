//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <array>
#include <stdexcept>
#include <limits>

#include "guid.h"
#include "test_server.h"
#include "http_utils.h"
#include <ajv.h>

#if defined(_MSC_VER)
#define __WINDOWS__
#endif

using namespace std::chrono_literals;
using HttpUtils = Microsoft::CognitiveServices::Speech::Impl::HttpUtils;

namespace
{
#ifdef __WINDOWS__
    constexpr auto PIPE_PREFIX = "\\\\.\\pipe\\";
#else
    constexpr auto PIPE_PREFIX = "/tmp/";
#endif

    constexpr auto NOT_SUPPORTED = "TestServer is not supported on your platform";

    static uint16_t ReadPortFromPipe(uint8_t* buffer, size_t numBytes)
    {
        if (buffer == nullptr)
        {
            throw std::invalid_argument("Null buffer from named pipe passed");
        }

        if (numBytes == 0)
        {
            throw std::runtime_error("Read 0 bytes from the named pipe");
        }
        else if (numBytes < 2)
        {
            throw std::runtime_error("Didn't read enough bytes from the named pipe. Read " + std::to_string(numBytes));
        }

        size_t messageLen = buffer[0] * 256 + buffer[1];
        if (messageLen != 2)
        {
            throw std::runtime_error(
                "Wrong length of message read from the named pipe. Message length: " + std::to_string(messageLen));
        }

        if (numBytes < messageLen + 2)
        {
            throw std::runtime_error(
                "Incomplete read of message from the named pipe. Got " + std::to_string(numBytes)
                + ", expected " + std::to_string(messageLen + 2));
        }

        uint16_t portNumber = buffer[2] * 256 + buffer[3];
        return portNumber;
    }

    static ajv::JsonBuilder CreateCommand(const std::string& command, const std::chrono::milliseconds& delay)
    {
        auto builder = ajv::json::Build();
        builder["command"] = command;
        builder["delay"] = delay.count();

        return builder;
    }
}

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    using WebSocketDisconnectReason = Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason;

    TestServer::TestServer(TestServerConfig config) :
        m_port(config.m_port),
        m_pipe(),
        m_exe()
    {
        std::string managedPipeName = "test_server_" + PAL::GenerateGUID();
        std::string pipeName = PIPE_PREFIX + managedPipeName;

        m_pipe = CreateServerPipe(pipeName, IPipe::Direction::Read);
        if (m_pipe == nullptr)
        {
            throw std::runtime_error(NOT_SUPPORTED);
        }

        ProcessStartInfo startupInfo = CreateStartInfo(config);

#if defined(__WINDOWS__)
        startupInfo.args.push_back("--pipe");
        startupInfo.args.push_back(managedPipeName); // for some reason, it wants the partial name only
#else
        startupInfo.args.push_back("--file");
        startupInfo.args.push_back(pipeName);
#endif

        m_exe = StartProcess(startupInfo);
        if (m_exe == nullptr)
        {
            throw std::runtime_error(NOT_SUPPORTED);
        }

        // get the port the test server is running at from the named pipe
        m_pipe->Connect(30s);

        std::array<uint8_t, 512> buffer;
        size_t bytesRead = m_pipe->Read(buffer.data(), buffer.size(), 30s);

        m_port = ReadPortFromPipe(buffer.data(), bytesRead);
    }

    std::string TestServer::HttpEndpoint() const
    {
        return "http://localhost:" + std::to_string(m_port);
    }

    std::string TestServer::WebSocketEndpoint() const
    {
        return "ws://localhost:" + std::to_string(m_port);
    }

    std::string TestServer::HttpTime() const
    {
        return HttpEndpoint() + "/http/current_time";
    }

    std::string TestServer::HttpEcho() const
    {
        return HttpEndpoint() + "/http/echo";
    }

    std::string TestServer::HttpStatus(unsigned int status, const char *content, const char *contentType) const
    {
        std::string endpoint(HttpEndpoint());
        endpoint += "/http/status?status=";
        endpoint += std::to_string(status);
        
        if (content != nullptr)
        {
            endpoint += "&content=" + HttpUtils::UrlEscape(content);
        }

        if (contentType != nullptr)
        {
            if (contentType == nullptr) contentType = "text/plain";
            endpoint += "&contentType=" + HttpUtils::UrlEscape(contentType);
        }

        return endpoint;
    }

    std::string TestServer::HttpStream() const
    {
        return HttpEndpoint() + "/http/stream";
    }

    std::string TestServer::HttpChunked() const
    {
        return HttpEndpoint() + "/http/chunked";
    }

    std::string TestServer::WSEchoEndpoint() const
    {
        return WebSocketEndpoint() + "/ws/echo";
    }

    uint16_t TestServer::Port() const
    {
        return m_port;
    }

    std::string TestServer::WSCloseEndpoint(WebSocketDisconnectReason status, const std::string& reason, int32_t closeAfter) const
    {
        return WebSocketEndpoint()
            + "/ws/close?after=" + std::to_string(closeAfter)
            + "&status=" + std::to_string((uint16_t)status)
            + "&reason=" + HttpUtils::UrlEscape(reason);
    }

    std::string TestServer::WSProxyEndpoint(const std::string& upstreamUri, const std::string& proxyHost, uint16_t proxyPort) const
    {
        auto endpoint = WebSocketEndpoint()
            + "/ws/proxy?x-proxy-uri=" + HttpUtils::UrlEscape(upstreamUri);

        if (!proxyHost.empty())
        {
            endpoint.append("&x-proxy-proxy=");
            endpoint.append(HttpUtils::UrlEscape(proxyHost));
            endpoint.append(":");
            endpoint.append(std::to_string(proxyPort));
        }

        return endpoint;
    }

    std::string TestServer::GetCommandPrefix()
    {
        return "<<!!" + GetUspCommandPath() + "!!>>";
    }

    std::string TestServer::GetUspCommandPath()
    {
        return "templeton";
    }

    std::string TestServer::GetAbortCommandString(const std::chrono::milliseconds& delay)
    {
        auto json = CreateCommand("AbortConnection", delay);
        return json.AsJson();
    }

    std::string TestServer::GetDisconnectCommandString(Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason status, const std::string& message, const std::chrono::milliseconds& delay)
    {
        auto json = CreateCommand("CloseConnection", delay);
        json["ClientCloseStatus"] = (int)status;
        json["ClientCloseReason"] = message;

        return json.AsJson();
    }

    ProcessStartInfo TestServer::CreateStartInfo(const TestServerConfig& config)
    {
        ProcessStartInfo startupInfo;
        startupInfo.terminateWithParent = true;

#if defined(__WINDOWS__)
        startupInfo.executable = "input\\test_server\\test_server.exe";
        startupInfo.args.push_back("test_server.exe");
#else
        startupInfo.executable = "dotnet";
        startupInfo.args.push_back("input/test_server/test_server.dll");
#endif

        if (config.m_port != 0)
        {
            startupInfo.args.push_back("--port");
            startupInfo.args.push_back(std::to_string(config.m_port));
        }

        auto ppid = GetCurrentProcessId();
        if (!ppid.empty())
        {
            startupInfo.args.push_back("-ppid");
            startupInfo.args.push_back(ppid);
        }

        for (const auto& kvp : config.m_mappings)
        {
            startupInfo.args.push_back(kvp.second.isUsp ? "--add-usp-service" : "--add-web-socket-handler");
            startupInfo.args.push_back(kvp.second.path + "=" + kvp.second.assemblyQualifiedName);
        }

        return startupInfo;
    }

}}}}
