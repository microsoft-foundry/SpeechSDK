//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md m_file in the project root for full license information.
//

#define _CRT_SECURE_NO_WARNINGS

#include "stdafx.h"
#include "file_logger.h"
#include "file_utils.h"
#include "exception.h"
#include <stdio.h>
#include <string.h>

void FileLogger::SetFileOptions(std::shared_ptr<Microsoft::CognitiveServices::Speech::Impl::ISpxNamedProperties> properties)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    bool nameSet = properties->HasStringValue("SPEECH-LogFilename");
    std::string name;
    if (nameSet)
    {
        name = properties->GetStringValue("SPEECH-LogFilename", "");
        if (name.find("NoFlush") != std::string::npos)
        {
            m_shouldFlush = false;
        }
    }

    bool filterSet = properties->HasStringValue("SPEECH-LogFileFilters");
    std::string filterValue;
    if (filterSet)
    {
        filterValue = properties->GetStringValue("SPEECH-LogFileFilters", "");
    }

    bool fileDurationSet = properties->HasStringValue("SPEECH-FileLogDurationSeconds");
    uint32_t fileDuration = 0;
    if (fileDurationSet)
    {
        fileDuration = static_cast<uint32_t>(std::stoul(properties->GetStringValue("SPEECH-FileLogDurationSeconds", "0")));
    }

    bool fileDurationSizeSet = properties->HasStringValue("SPEECH-FileLogSizeMB");
    uint32_t fileDurationSize = 0;
    if (fileDurationSizeSet)
    {
        fileDurationSize = static_cast<uint32_t>(std::stoul(properties->GetStringValue("SPEECH-FileLogSizeMB", "0")));
    }

    bool appendToFileSet = properties->HasStringValue("SPEECH-AppendToLogFile");
    uint32_t appendToFile = 0;
    if (appendToFileSet)
    {
        appendToFile = static_cast<uint32_t>(std::stoul(properties->GetStringValue("SPEECH-AppendToLogFile", "0")));
        m_append = (0 != appendToFile);
    }

    if (filterSet)
    {
        m_filter.SetFilter(filterValue);
    }

    if (nameSet && name.compare(m_baseFilename))
    {
        m_currentFileAppendix = 0;
        m_baseFilename = name;
    }

    std::string currentFileName = m_baseFilename;
    bool counterIncreased = false;

    if (fileDurationSet)
    {
        m_fileDurationSeconds = fileDuration;
    }

    if (m_fileDurationSeconds > 0 && !currentFileName.empty())
    {
        auto nextTime = m_lastFileStartTime + std::chrono::seconds(m_fileDurationSeconds);
        if (nextTime <= std::chrono::steady_clock::now())
        {
            m_currentFileAppendix++;
            counterIncreased = true;
        }
        currentFileName = BuildFileName(currentFileName);
    }

    if (fileDurationSizeSet)
    {
        m_fileDurationMB = fileDurationSize;
    }

    if (m_fileDurationMB > 0 && !currentFileName.empty() && !counterIncreased)
    {
        if (m_fileDataWritten.load() > m_fileDurationMB * 1024 * 1024) // MB
        {
            m_currentFileAppendix++;
        }
        currentFileName = BuildFileName(currentFileName);
    }

    // if user tries to change a m_filename, let them.
    if (!currentFileName.compare(m_filename))
    {
        return;
    }

    m_filename = currentFileName;

    AssignFile();
}

std::string FileLogger::GetFilename()
{
    return m_filename;
}

bool FileLogger::IsFileLoggingEnabled()
{
    return m_file != nullptr;
}

void FileLogger::CloseFile()
{
    WriteLock lock(&m_fileNameLock);
    if (m_file != nullptr)
    {
        fclose((FILE *)m_file);
        m_file = nullptr;
    }

    // Someone wanting to turn logging on can set a m_file name.
    m_baseFilename.clear();
    m_filename.clear();
}

void FileLogger::LogToFile(const char *logLine)
{
    if (m_file != nullptr && m_filter.ShouldLog(logLine))
    {
        ReadLock lock(&m_fileNameLock);
        FILE *fileToUse = (FILE *)m_file;
        if (fileToUse != nullptr)
        {
            fprintf(fileToUse, "%s", logLine);
            if (m_shouldFlush)
            {
                fflush(fileToUse);
            }
            m_fileDataWritten.fetch_add(strlen(logLine));
        }
    }
}

FileLogger& FileLogger::Instance()
{
    static FileLogger instance;
    return instance;
}

void FileLogger::AssignFile()
{
    WriteLock lock(&m_fileNameLock);
    if (nullptr != m_file)
    {
        fclose((FILE *)m_file);
        m_file = nullptr;
    }

    if (!m_filename.empty())
    {
        FILE *newFile = PAL::fsopen(m_filename.c_str(), m_append ? "a" : "w");
        SPX_THROW_HR_IF(SPXERR_LOG_FILE_OPEN_FAILED, newFile == nullptr);
        m_file = (volatile FILE *)newFile;
        m_lastFileStartTime = std::chrono::steady_clock::now();
        m_fileDataWritten.store(0);
    }
}

std::string FileLogger::BuildFileName(std::string fileName)
{
    auto lastDot = fileName.find_last_of('.');
    if (std::string::npos == lastDot)
    {
        // Just append it.
        return fileName += "-" + std::to_string(m_currentFileAppendix);
    }
    else
    {
        return fileName.substr(0, lastDot) + "-" + std::to_string(m_currentFileAppendix) + fileName.substr(lastDot);
    }
}
