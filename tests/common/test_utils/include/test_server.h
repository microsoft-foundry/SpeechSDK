//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <string>
#include <map>
#include <interfaces/i_web_socket_state.h>

#include "i_pipe.h"
#include "i_process.h"

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    class TestServer;

    /// <summary>
    /// The test server configuration
    /// </summary>
    class TestServerConfig
    {
    private:
        friend class TestServer;

        struct Mapping
        {
            std::string path;
            std::string assemblyQualifiedName;
            bool isUsp{ false };
        };

        std::map<std::string, Mapping> m_mappings;
        uint16_t m_port{ 0 };

    public:
        /// <summary>
        /// Sets the port to listen on
        /// </summary>
        /// <param name="port">The port to use. Set to 0 to automatically choose an unused port</param>
        TestServerConfig& SetPort(uint16_t port)
        {
            m_port = port;
            return *this;
        }

        /// <summary>
        /// Adds a custom web socket handler to be run by the test server at the specified path
        /// </summary>
        /// <param name="path">The path for the service to listen on</param>
        /// <param name="assemblyQualifiedName">The full assembly qualified name of the type that implements
        /// the C# IWebSocketHandler interface. This must have a parameterless constructor</param>
        TestServerConfig& AddWebSocketRoute(const std::string& path, const std::string& assemblyQualifiedName)
        {
            m_mappings[path] = { path, assemblyQualifiedName, false };
            return *this;
        }

        /// <summary>
        /// Adds a custom USP service to be run by the test server at the specified path
        /// </summary>
        /// <param name="path">The path for the service to listen on</param>
        /// <param name="assemblyQualifiedName">The full assembly qualified name of the type that implements
        /// the C# IUspService interface. This must have a parameterless constructor</param>
        TestServerConfig& AddUspService(const std::string& path, const std::string& assemblyQualifiedName)
        {
            m_mappings[path] = { path, assemblyQualifiedName, true };
            return *this;
        }
    };

    /// <summary>
    /// A test server to be used for writing integration or unit tests. This provides endpoints that can
    /// echo back received requests/messages, trigger specific closures. It also provides a web socket
    /// proxy endpoint that acts as an intermediary between you and a real web socket service. That allows
    /// you to modify/inject/close/etc... live messages
    /// </summary>
    class TestServer
    {
    private:
        uint16_t m_port;
        std::unique_ptr<IPipe> m_pipe;
        std::unique_ptr<IProcess> m_exe;

    public:
        /// <summary>
        /// Creates a new test server instance
        /// </summary>
        /// <param name="config">The test server configuration to use</param>
        TestServer(TestServerConfig config = {});

        /// <summary>
        /// Destructor
        /// </summary>
        ~TestServer() = default;

        TestServer(const TestServer&) = delete;
        TestServer(TestServer&&) = delete;

        /// <summary>
        /// Gets the HTTP endpoint the test server is listening on (e.g. http://localhost:43124)
        /// </summary>
        /// <returns>The HTTP endpoint is the test server is listening on</returns>
        std::string HttpEndpoint() const;

        /// <summary>
        /// Gets the web socket endpoint the test server is listening on (e.g. ws://localhost:43124)
        /// </summary>
        /// <returns>The WS endpoint the test server is listening on</returns>
        std::string WebSocketEndpoint() const;

        /// <summary>
        /// Gets the HTTP endpoint that returns the current UTC time. This supports only GET requests,
        /// and the response will be a "plain/text" string with the following format:
        /// Current time is 2023-08-30T06:29:33.2993220Z
        /// </summary>
        /// <returns>The URI to use</returns>
        std::string HttpTime() const;

        /// <summary>
        /// Gets the HTTP endpoint that echoes back the request. All request headers will be included in the response
        /// but their names will be prefixed with "X-Request-". If a body was sent in the request, it will be echoed
        /// back as well in the response
        /// </summary>
        /// <returns>The URI to use</returns>
        std::string HttpEcho() const;

        /// <summary>
        /// Gets the HTTP endpoint that returns a specified status code. You can optionally provide a body to
        /// return, as well as the content type
        /// </summary>
        /// <param name="status">The status code to return</param>
        /// <param name="content">(Optional) The body to return. If set to null, no body will be returned in the response</param>
        /// <param name="contentType">(Optional) The content type of the body. This is ignored if the content is null</param>
        /// <returns>The URI to use</returns>
        std::string HttpStatus(unsigned int status, const char* content = nullptr, const char* contentType = nullptr) const;

        /// <summary>
        /// Gets the HTTP endpoint that "streams" a large amount of data to the client.
        /// </summary>
        /// <returns>The URI to use</returns>
        std::string HttpStream() const;

        /// <summary>
        /// Gets the HTTP endpoint that "streams" a large amount of data to the client with chunking.
        /// </summary>
        /// <returns>The URI to use</returns>
        std::string HttpChunked() const;

        /// <summary>
        /// Gets the full URL to the web socket endpoint. This echoes back all received messages
        /// </summary>
        /// <returns>The web socket echo endpoint</returns>
        std::string WSEchoEndpoint() const;

        /// <summary>
        /// Gets the port the test server is listening on
        /// </summary>
        /// <returns>The port the test server is listening on</returns>
        uint16_t Port() const;

        /// <summary>
        /// Gets the full URL to the web socket endpoint that closes with the specified reason and message
        /// </summary>
        /// <param name="status">The disconnect reason to return</param>
        /// <param name="reason">The message to return in the close message</param>
        /// <param name="closeAfter">After how many received messages should the web socket be closed. Set
        /// to 0 to close immediately</param>
        /// <returns>The web socket close endpoint</returns>
        std::string WSCloseEndpoint(
            Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason status,
            const std::string& reason,
            int32_t closeAfter = 0) const;

        /// <summary>
        /// Gets the full URL to the web socket endpoint that acts as an intermediary between another
        /// upstream web socket server. This allows you to send specific commands to e.g. delay messages,
        /// modify messages, trigger web socket closures, etc... Please refer to the test server command
        /// documentation
        /// </summary>
        /// <param name="upstreamUri">The full upstream URI to connect to</param>
        /// <param name="proxyHost">(Optional) The proxy host to use when connecting to the upstream URI</param>
        /// <param name="proxyPort">(Optional) the proxy port to use when connecting to the upstream URI</param>
        /// <returns>The web socket proxy endpoint</returns>
        std::string WSProxyEndpoint(
            const std::string& upstreamUri,
            const std::string& proxyHost = "",
            uint16_t proxyPort = 0) const;

        /// <summary>
        /// Gets the command prefix used to send command messages to the WSProxyEndpoint. If you are using a regular
        /// web socket, prefix your text web socket message with this
        /// </summary>
        /// <returns>The text web socket message command prefix</returns>
        static std::string GetCommandPrefix();

        /// <summary>
        /// Gets the path to be used to send command message to the WSProxyEndpoint when you are using the USP
        /// web socket protocol. You should send a text USP message with this path, content type set to
        /// application/json, and the needed command string as the body
        /// </summary>
        /// <returns>The USP command path</returns>
        static std::string GetUspCommandPath();

        /// <summary>
        /// Gets the command text for the proxy endpoint to cause the proxy service to terminated the web socket
        /// connection at the TCP level (simulates network failure)
        /// </summary>
        /// <param name="delay">How long the proxy service should wait before terminating the web socket connection</param>
        /// <returns>The text of the command string</returns>
        static std::string GetAbortCommandString(const std::chrono::milliseconds& delay = std::chrono::milliseconds(0));

        /// <summary>
        /// Gets the command text for the proxy endpoint to cause the proxy service to close the web socket using the
        /// specified status and an optional message
        /// </summary>
        /// </summary>
        /// <param name="status">The web socket close status to use</param>
        /// <param name="message">The web socket close message</param>
        /// <param name="delay">How long to wait before initiating a server requested web socket close</param>
        /// <returns>The text of the command string</returns>
        static std::string GetDisconnectCommandString(
            Microsoft::CognitiveServices::Speech::Impl::WebSocketDisconnectReason status,
            const std::string& message = {},
            const std::chrono::milliseconds& delay = std::chrono::milliseconds(0));

    private:
        static ProcessStartInfo CreateStartInfo(const TestServerConfig & config);
    };

}}}}
