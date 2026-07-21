//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
//

#include  <wchar.h>
#include "file_utils.h"
#include "platform.h"
#include <cstring>
#include "string_utils.h"
#include "spxdebug.h"

#ifndef _MSC_VER
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#else
#include <io.h>
#include <stdio.h>
#include <share.h>
#include <direct.h>
#endif

namespace PAL {

errno_t fopen_s(FILE** file, const char* fileName, const char* mode)
{
#ifdef _MSC_VER
    return ::_wfopen_s(file, PAL::ToWString(fileName).c_str(), PAL::ToWString(mode).c_str());
#else
    FILE *f = fopen(fileName, mode);
    if (f == NULL)
    {
        return -1;
    }
    *file = f;
    return 0;
#endif
}

size_t fread_s(void** buffer, size_t bufferSize, size_t elementSize, size_t count, FILE* file)
{
#ifdef _MSC_VER
    return fread_s(*buffer, bufferSize, elementSize, count, file);
#else
    // UNUSED
    (void )bufferSize;
    return fread(buffer, elementSize, count, file);
#endif
}

FILE *fsopen(const char* fileName, const char* mode)
{
#ifdef _MSC_VER
    return ::_wfsopen(PAL::ToWString(fileName).c_str(), PAL::ToWString(mode).c_str(), _SH_DENYWR);
#else
    FILE *f = fopen(fileName, mode);
    return f;
#endif
}

int waccess(const wchar_t *path, int mode)
{
#ifdef _MSC_VER
    return _waccess(path, mode);
#else
    return path ? ::access(PAL::ToString(path).c_str(), mode) : -1;
#endif
}

int access(const char *path, int mode)
{
#ifdef _MSC_VER
    auto s = ToWString(path);
    return _waccess(s.c_str(), mode);
#else
    return ::access(path, mode);
#endif
}

void OpenStream(std::fstream& stream, const std::string& filename, bool readonly)
{
    if (filename.empty())
        throw std::runtime_error("File: filename is empty");

    std::ios_base::openmode mode = std::ios_base::binary;
    mode = mode | (readonly ? std::ios_base::in : std::ios_base::out);

    // This is required on Windows. See more: http://utf8everywhere.org/#how.files
    #ifdef _MSC_VER
        stream.open(PAL::ToWString(filename), mode);
    #else
        stream.open(filename.c_str(), mode);
    #endif
}

std::string AppendPath(const std::string& str1, const std::string& str2)
{
    std::string appendedPath;
    char separator = '/';
    std::string tmp = str1;

#ifdef _WIN32
    separator = '\\';
#endif

    if (str1[str1.length()] != separator)
    {
        tmp += separator;
        appendedPath = tmp + str2;
    }
    else
    {
        appendedPath = str1 + str2;
    }
    return appendedPath;
}

// Change to use filesystem when we use C++17
bool CreateDirectory(const std::string& path)
{
    int nError = 0;
#ifdef _WIN32
    nError = _mkdir(path.c_str());
#else
    mode_t nMode = 0755; // UNIX style permissions
    nError = mkdir(path.c_str(), nMode);
#endif
    return nError == 0;
}

std::vector<std::string> FindFiles(const std::string& rootPath, const std::vector<std::string>& filenames)
{
    std::vector<std::string> result;

    if (rootPath.empty() || filenames.empty())
    {
        return result; // empty
    }

    auto searchFolder = rootPath;
    if (rootPath.back() != PAL::PATH_NAME_SEPARATOR_UNIX && // works also in Windows paths
        rootPath.back() != PAL::PATH_NAME_SEPARATOR_WIN32)
    {
        searchFolder += PAL::PATH_NAME_SEPARATOR;
    }

    // std::filesystem (non-experimental) is only available in C++17 and later,
    // so the file search needs different implementations on Windows and other
    // platforms (with GCC and POSIX support).

#ifdef _MSC_VER

    auto searchPattern = PAL::ToWString(searchFolder) + L"*.*";

    struct _wfinddata_t finddata;
    intptr_t nPtr = -1;
    nPtr = _wfindfirst(searchPattern.c_str(), &finddata);

    if (nPtr != -1)
    {
        while (_wfindnext(nPtr, &finddata) != -1)
        {
            if (finddata.attrib & _A_SUBDIR)
            {
                if (wcscmp(finddata.name, L".") * wcscmp(finddata.name, L"..") != 0)
                {
                    auto searchSubFolder = searchFolder + PAL::ToString(finddata.name);
                    auto subFolderResult = FindFiles(searchSubFolder, filenames);

                    if (!subFolderResult.empty())
                    {
                        result.insert(result.end(), subFolderResult.begin(), subFolderResult.end());
                    }
                }
            }
            else // file
            {
                for (const auto& filename : filenames)
                {
                    if (wcscmp(finddata.name, PAL::ToWString(filename).c_str()) == 0)
                    {
                        std::string filepath = searchFolder + PAL::ToString(finddata.name);
                        result.push_back(filepath);
                    }
                }
            }
        }
        _findclose(nPtr);
    }

#else

    DIR* pDir = opendir(searchFolder.c_str());

    if (pDir != NULL)
    {
        struct dirent* pDirEntry = NULL;

        while (true)
        {
            errno = 0;
            pDirEntry = readdir(pDir);
            if (pDirEntry == NULL)
            {
                SPX_TRACE_WARNING_IF(errno != 0, "%s: readdir returned NULL, errno %d", __FUNCTION__, errno);
                break;
            }

            std::string direntName(pDirEntry->d_name);

            if (pDirEntry->d_type & DT_DIR)
            {
                if (strcmp(pDirEntry->d_name, ".") * strcmp(pDirEntry->d_name, "..") != 0)
                {
                    auto searchSubFolder = searchFolder + direntName;
                    auto subFolderResult = FindFiles(searchSubFolder, filenames);

                    if (!subFolderResult.empty())
                    {
                        result.insert(result.end(), subFolderResult.begin(), subFolderResult.end());
                    }
                }
            }
            else // file
            {
                for (const auto& filename : filenames)
                {
                    if (strcmp(pDirEntry->d_name, filename.c_str()) == 0)
                    {
                        std::string filepath = searchFolder + direntName;
                        result.push_back(filepath);
                    }
                }
            }
        }
        closedir(pDir);
    }

#endif

    return result;
}

} // PAL
