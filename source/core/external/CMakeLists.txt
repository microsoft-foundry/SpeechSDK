#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
cmake_minimum_required(VERSION 3.14)
# Allow use of IN_LIST operator in if statements.
cmake_policy(SET CMP0057 NEW)

project(azure_c_shared_utility_builder)

include(ExternalProject)

function(build_azure_c_shared_utility openssl_dir output_directory)
    unset(OPENSSL_ROOT_DIR CACHE)

    file(GLOB_RECURSE AZURE_SOURCES
       "${CMAKE_CURRENT_LIST_DIR}/azure-c-shared-utility/*.c"
       "${CMAKE_CURRENT_LIST_DIR}/azure-c-shared-utility/*.h"
       "${CMAKE_CURRENT_LIST_DIR}/azure-c-shared-utility/*.cpp"
       "${CMAKE_CURRENT_LIST_DIR}/azure-c-shared-utility/*.hpp"
       "${CMAKE_CURRENT_LIST_DIR}/azure-c-shared-utility/CMakeLists.txt")

    add_custom_target(${output_directory}_sources DEPENDS ${AZURE_SOURCES})

    ExternalProject_Add(${output_directory}
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/azure-c-shared-utility"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/${output_directory}"
        CMAKE_ARGS -DOPENSSL_ROOT_DIR=${openssl_dir}
                   -Duse_builtin_httpapi=ON
                   -Dskip_samples=ON
                   -Duse_cppunittest=OFF
                   -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/${output_directory}/install
                   -DCMAKE_VERBOSE_MAKEFILE=ON # Enable verbose logging
                   -Duse_openssl_dynamic=${use_openssl_dynamic}
                   -Duse_openssl=${use_openssl}
                   -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                   -DARCHITECTURE=${ARCHITECTURE}
                   -DCMAKE_FIND_DEBUG_MODE=On
                   -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH
                   -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH
                   -DPLATFORM=${PLATFORM}
        INSTALL_COMMAND ""
        LOG_DIR "${CMAKE_CURRENT_BINARY_DIR}/${output_directory}/logs"
        LOG_BUILD TRUE
        LOG_CONFIGURE TRUE
        LOG_MERGED_STDOUTERR # Merge stdout and stderr into a single log file
        LOG_OUTPUT_ON_FAILURE TRUE
        DEPENDS ${output_directory}_sources
    )

    ExternalProject_Add_Step(${output_directory} check_sources
        COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/${output_directory}_timestamp"
        DEPENDEES download
        DEPENDERS build
        DEPENDS ${AZURE_SOURCES}
    )
    set(AZURE_SHARED_LIB ${output_directory} PARENT_SCOPE)
endfunction()

if (NATIVE_NETWORKING_IMPL STREQUAL "Azure_C_Shared")
    message(STATUS "Building Azure C Shared Utility")

    if(${use_openssl_dynamic})
        message(STATUS "Treating Azure C Shared as an external project")
        build_azure_c_shared_utility("" "azure_c_shared_utility")

        set(AZURE_SHARED_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/azure_c_shared_utility" PARENT_SCOPE)

        set(OPENSSL_MULTIPLE_ARCHS 
            "Linux-arm32"
            "Linux-arm64"
            "Linux-x64"
            )

        if(${CARBONSDK_TARGET_PLATFORM} IN_LIST OPENSSL_MULTIPLE_ARCHS)
            message(STATUS "Building Azure C Shared Utility with OpenSSL 3 on ${CARBONSDK_TARGET_PLATFORM}")

            if(${CARBONSDK_TARGET_PLATFORM} STREQUAL "Linux-arm32")
                set(OPENSSL_ROOT_DIR "linux-armv4")
            elseif(${CARBONSDK_TARGET_PLATFORM} STREQUAL "Linux-arm64")
                set(OPENSSL_ROOT_DIR "linux-aarch64")
            elseif(${CARBONSDK_TARGET_PLATFORM} STREQUAL "Linux-x64" OR ${CARBONSDK_TARGET_PLATFORM} STREQUAL "Linux-x86")
                set(OPENSSL_ROOT_DIR "linux-x86_64")
            endif()

            if(NOT BUILD_CORE_ONLY)
              set(OPENSSL_ROOT_DIR "${CARBON_EXTERNAL}/openssl/${OPENSSL_ROOT_DIR}/usr/local")
              message(STATUS "Using OpenSSL from ${OPENSSL_ROOT_DIR}")
            endif()

            build_azure_c_shared_utility("${OPENSSL_ROOT_DIR}" "azure_c_shared_utility_openssl3")
            set(AZURE_SHARED_LIB_DIR_OPENSSL3 "${CMAKE_CURRENT_BINARY_DIR}/azure_c_shared_utility_openssl3" PARENT_SCOPE)
        endif()

        set(AZURE_SHARED_UTIL_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/azure-c-shared-utility/inc" PARENT_SCOPE)
        set(AZURE_SHARED_LIB "aziotsharedutil" PARENT_SCOPE)
    else()
        message(STATUS "Adding Azure C Shared as an inline project")

        set(AZURE_SHARED_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/azure-c-shared-utility")
        if(NOT EXISTS ${AZURE_SHARED_LIB_DIR} OR NOT EXISTS ${AZURE_SHARED_LIB_DIR}/CMakeLists.txt)
            # Note: we do not need recursive Git submodule init as of now.
            execute_process(COMMAND git submodule update --init)
        endif()

        # Override AZURE_SHARED_LIB build options
        set(use_builtin_httpapi ON CACHE BOOL "")
        set(skip_samples ON CACHE BOOL "")
        set(use_cppunittest OFF CACHE BOOL "")

        add_subdirectory(${AZURE_SHARED_LIB_DIR} azure_c_shared_utility EXCLUDE_FROM_ALL SYSTEM)

        set(AZURE_SHARED_LIB aziotsharedutil)
        set_target_properties(${AZURE_SHARED_LIB} PROPERTIES FOLDER external_libs)

        if (UNIX)
            set_target_properties(${AZURE_SHARED_LIB} PROPERTIES COMPILE_FLAGS
            "-Wno-error=unused-variable -Wno-error=implicit-function-declaration -fexceptions")
        endif()

        if(WIN32 AND SDL_FLAGS_ENABLED)
            target_compile_options(${AZURE_SHARED_LIB} PRIVATE /sdl /Qspectre /ZH:SHA_256)
            if(CARBONSDK_ARCH STREQUAL "x64")
                target_link_options(${AZURE_SHARED_LIB} PRIVATE "/CETCOMPAT")
            endif()
        endif()

        set(AZURE_SHARED_UTIL_INCLUDE ${SHARED_UTIL_INC_FOLDER} PARENT_SCOPE)
        set(AZURE_SHARED_LIB ${AZURE_SHARED_LIB} PARENT_SCOPE)

    endif()
endif()
