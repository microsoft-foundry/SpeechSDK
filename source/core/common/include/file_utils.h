//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
//

#pragma once

#include <fstream>
#include <vector>

#ifndef _MSC_VER
#define _FILE_OFFSET_BITS 64
#include <string>
typedef int errno_t;
#endif

namespace PAL {

    static constexpr auto PATH_LIST_SEPARATOR = ';';
    static constexpr auto PATH_NAME_SEPARATOR_WIN32 = '\\';
    static constexpr auto PATH_NAME_SEPARATOR_UNIX  = '/';

#if defined(WIN32) && !defined(UNIX)
    static constexpr auto PATH_NAME_SEPARATOR = PATH_NAME_SEPARATOR_WIN32;
#else
    static constexpr auto PATH_NAME_SEPARATOR = PATH_NAME_SEPARATOR_UNIX;
#endif

    int waccess(const wchar_t *path, int mode);
    int access(const char *path, int mode);

    errno_t fopen_s(FILE **file, const char *fileName, const char *mode);
    size_t fread_s(void** buffer, size_t bufferSize, size_t elementSize, size_t count, FILE* file);
    FILE *fsopen(const char* fileName, const char* mode);

    void OpenStream(std::fstream& stream, const std::string& filename, bool readonly);
    std::string AppendPath(const std::string& str1, const std::string& str2);

    bool CreateDirectory(const std::string& path);

    /// <summary>
    /// Recursively search the filesystem for specified filenames.
    /// </summary>
    /// <param name="rootPath">Root path for search. This and all subfolders will be searched.</param>
    /// <param name="filenames">Names of the files to search for.</param>
    /// <returns>Paths to files that were found, if any.</returns>
    std::vector<std::string> FindFiles(const std::string& rootPath, const std::vector<std::string>& filenames);
} // PAL
