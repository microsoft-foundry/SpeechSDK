//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// file_logger.h: FileLogger() implementation declaration
//

#pragma once

#include <string>
#include "log_utils.h"

class FileLogger
{
public:
    static FileLogger& Instance();

    FileLogger(FileLogger const&) = delete;
    void operator=(FileLogger const&) = delete;

    void SetFileOptions(std::shared_ptr<Microsoft::CognitiveServices::Speech::Impl::ISpxNamedProperties> properties);
    std::string GetFilename();
    bool IsFileLoggingEnabled();
    void CloseFile();

    void LogToFile(const char *format);
private:
    FileLogger() = default;

    std::string m_filename;
    std::string m_baseFilename;
    uint32_t m_fileDurationMB;
    uint32_t m_fileDurationSeconds;
    bool m_append = false;
    bool m_shouldFlush = true;

    ReaderWriterLock m_fileNameLock;

    uint32_t m_currentFileAppendix;
    std::chrono::steady_clock::time_point m_lastFileStartTime = std::chrono::steady_clock::time_point::min();
    std::atomic_size_t m_fileDataWritten;

    void AssignFile();
    std::string BuildFileName(std::string currentName);

    volatile FILE* m_file = nullptr;
    std::mutex m_mtx;

    LogFilter m_filter;
};
