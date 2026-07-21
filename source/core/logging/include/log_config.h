//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <array>
#include <string>
#include <tuple>

#include "log_utils.h"
#include "util/maybe.h"
#include "function_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

enum class LogLevel: int
{
    Error = __AZAC_TRACE_LEVEL_ERROR,
    Warning = __AZAC_TRACE_LEVEL_ERROR | __AZAC_TRACE_LEVEL_WARNING,
    Info = __AZAC_TRACE_LEVEL_ERROR | __AZAC_TRACE_LEVEL_WARNING | __AZAC_TRACE_LEVEL_INFO,
    Verbose = __AZAC_TRACE_LEVEL_ERROR | __AZAC_TRACE_LEVEL_WARNING | __AZAC_TRACE_LEVEL_INFO | __AZAC_TRACE_LEVEL_VERBOSE
};

enum class Logger: size_t
{
    Console = 0,
    File,
    Events,
    Memory,
    EventSource,
};

template<typename>
struct Into
{
};

template<>
struct Into<LogLevel>
{
    LogLevel operator()(const std::string& level)
    {
        constexpr std::array<std::tuple<const char *, LogLevel>, 4> levelMappings{ {
            { "error", LogLevel::Error },
            { "warning", LogLevel::Warning },
            { "info", LogLevel::Info },
            { "verbose", LogLevel::Verbose }
        } };
        return FindTupleInArrayOr(levelMappings, level, LogLevel::Verbose);
    }
};

template<>
struct Into<Logger>
{
    Logger operator()(const std::string& logger)
    {
        constexpr std::array<std::tuple<const char *, Logger>, 5> loggerMappings{ {
            { "console", Logger::Console },
            { "file", Logger::File },
            { "event", Logger::Events },
            { "memory", Logger::Memory },
            { "eventsource", Logger::EventSource }
        }};
        return FindTupleInArrayOr(loggerMappings, logger, Logger::Console);
    }
};


class LogConfig
{
public:
    LogConfig();

    LogConfig(const LogConfig&) = delete;
    LogConfig(LogConfig&&) = delete;

    static LogConfig& GetConfig(Logger logger);

    inline LogLevel Level() const noexcept
    {
        return m_level;
    }

    inline void Level(LogLevel level) noexcept
    {
        m_level = level;
    }
private:
    static std::array<LogConfig, 5> m_configs;

    LogLevel m_level;
};

} } } }
