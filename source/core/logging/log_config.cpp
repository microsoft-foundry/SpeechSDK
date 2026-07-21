//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include <array>
#include <tuple>

#include "log_config.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

decltype(LogConfig::m_configs) LogConfig::m_configs{};

LogLevel GetDefaultLogLevel()
{
    return PAL::SpxGetEnv("AZAC_DIAGNOSTICS_LOG_LEVEL").Map<LogLevel>([](const std::string& v) -> Maybe<LogLevel>
    {
        return Into<LogLevel>{}(v);
    }).GetOr(LogLevel::Verbose);
}

LogConfig::LogConfig():
    m_level{ GetDefaultLogLevel() }
{}

LogConfig& LogConfig::GetConfig(Logger logger)
{
    return m_configs.at(static_cast<size_t>(logger));
}
