#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
if(WIN32)
  # Version >= 3.10 is required for generating VS 2017 solutions.
  cmake_minimum_required(VERSION 3.14)
elseif(APPLE)
  cmake_minimum_required(VERSION 3.12)
else()
  cmake_minimum_required(VERSION 3.12)
endif()

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for nested third-party CMake projects")
endif()

function(set_source_groups)
  file(GLOB_RECURSE INTERFACE_HEADERS ${interfaces_SOURCE_DIR}/include/*)
  source_group("Interface Headers" FILES ${INTERFACE_HEADERS})
  if(HEADERS)
    source_group("Header Files (public)" FILES ${HEADERS})
  endif()
  if(HEADERS_PRIVATE)
    source_group("Header Files (private)" FILES ${HEADERS_PRIVATE})
  endif()
  if(SOURCES)
    source_group("Source Files" FILES ${SOURCES})
  endif()
endfunction()

function(add_security_compile_flags target)
  if(WIN32 AND SDL_FLAGS_ENABLED)
      target_compile_options(${target} PRIVATE /sdl /Qspectre /ZH:SHA_256)
      if(CARBONSDK_ARCH STREQUAL "x64")
          target_link_options(${target} PRIVATE "/CETCOMPAT")
      endif()
  elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR ANDROID OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${target} PRIVATE -Wformat-nonliteral -Wformat-security)
  endif()
endfunction()

# Only interpret ``if()`` arguments as variables or keywords when unquoted.
# cmake --help-policy CMP0054
cmake_policy(SET CMP0054 NEW)

# CMake version 3.19 changed how it handles an imported target missing its location property.
# Suppress warnings. For more details, type: cmake --help-policy CMP0111
#if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
#  cmake_policy(SET CMP0111 OLD)
#endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(carbonsdk_functions)

# Enable a core-only build
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/bindings" AND
   NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/extensions")
  option(ENABLE_CORE_ONLY_BUILD "Enable a core-only build" ON)
else()
  option(ENABLE_CORE_ONLY_BUILD "Enable a core-only build" OFF)
endif()

if(ENABLE_CORE_ONLY_BUILD)
  set(BUILD_CORE_ONLY ON)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCARBONSDK_BUILD_CORE_ONLY")
  set(CARBONSDK_BUILD_TYPE "prod" CACHE STRING "SDK build type (local, dev, int, prod)")
else()
  set(BUILD_CORE_ONLY OFF)
  set(CARBONSDK_BUILD_TYPE "local" CACHE STRING "SDK build type (local, dev, int, prod)")
endif()

get_local_version(_LOCAL_VERSION)
set(SPEECHSDK_VERSION "${_LOCAL_VERSION}" CACHE STRING "Version to use")
message(STATUS "Building Carbon SDK version ${SPEECHSDK_VERSION}")

parse_version(
  BUILD_TYPE ${CARBONSDK_BUILD_TYPE}
  VERSION ${SPEECHSDK_VERSION}
  PREFIX SPEECHSDK
  )

if(BUILD_CORE_ONLY)
  message(STATUS "Building core features only")
endif()

get_local_platform(_LOCAL_PLATFORM)
set(CARBONSDK_TARGET_PLATFORM "${_LOCAL_PLATFORM}" CACHE STRING
  "SDK target platform ({Windows,WindowsUwp,Linux,OSX,Android,IOS,IOSSIMULATOR}-{x86,x64,arm32,arm64}; not all combinations are supported)")
message(STATUS "CARBONSDK_TARGET_PLATFORM=${CARBONSDK_TARGET_PLATFORM}")

parse_platform(TARGET_PLATFORM ${CARBONSDK_TARGET_PLATFORM} PREFIX CARBONSDK)

# Ensure submodules are up to date
option(AUTOMATICALLY_UPDATE_SUBMODULES "Automatically update submodules" ON)
find_package(Git QUIET)
if(GIT_FOUND AND AUTOMATICALLY_UPDATE_SUBMODULES AND NOT BUILD_CORE_ONLY)
  message(STATUS "Automatically updating submodules...")
  execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  RESULT_VARIABLE GIT_SUBMOD_RESULT)
  if(NOT GIT_SUBMOD_RESULT EQUAL "0")
      message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
  endif()
endif()

option(SDL_FLAGS_ENABLED "Build with SDL flags enabled" OFF)

# Enable Address Sanitizer
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
message(STATUS "ENABLE_ASAN=${ENABLE_ASAN}")

# Make the compilation of KWS optional
option(EXTENSION_KWS_ENABLED "Build the KWS extension based on the VoiceActivation component" OFF)

# Make the compilation of audio optional
if(BUILD_CORE_ONLY)
  option(EXTENSION_AUDIO_ENABLED "Build the audio extension component" OFF)
else()
  option(EXTENSION_AUDIO_ENABLED "Build the audio extension component" ON)
endif()

# Make the compilation of compression codec optional
option(EXTENSION_AUDIOCOMPRESSION_ENABLED "Build the compression codec extension component" OFF)

# Make the compilation of gstreamer (for decoding audio format) optional
option(EXTENSION_CODEC_ENABLED "Build the CODEC extension based on the supported platform" OFF)

# Make the compilation of hybrid tts optional
option(EXTENSION_HYBRID_TTS_ENABLED "Build the hybrid TTS extension based on the supported platform" OFF)

# Make the compilation of RNN-T based recognizer optional
option(EXTENSION_RNNT_ENABLED "Build RNN-T based recognizer component" OFF)

# Make the compilation of VAD optional
option(EXTENSION_VAD_ENABLED "Build the VAD extension component" OFF)

# Make the compilation of custom commands optional
option(EXTENSION_CUSTOM_COMMANDS_ENABLED "Build Custom Commands component" OFF)

set(CMAKE_CONFIGURATION_TYPES Debug Release)

# Make the compilation of MAS optional
option(EXTENSION_MAS_ENABLED "Build the MAS extension component" OFF)

# Make the compilation of the CLI tools optional
option(TOOL_SPX_ENABLED "Build SPX" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release"
      FORCE)
endif()

# Create a .Net Framework .csrpoj file for the C# speech bindings that can be used normally in Visual Studio
option(ENABLE_VS_INTEGRATION "Enable C# .Net Framework .csproj generation for integrating into Visual Studio" OFF)
message(STATUS "ENABLE_VS_INTEGRATION=${ENABLE_VS_INTEGRATION}")

option(ENABLE_TEST_BUILD "Enable building of tests" ON)

# Determine which networking implementation to use
if (DEFINED NATIVE_NETWORKING_IMPL OR DEFINED CACHE{NATIVE_NETWORKING_IMPL})
  # Value was already set. Don't override it
else()
  set(NATIVE_NETWORKING_IMPL "Azure_C_Shared" CACHE STRING "Sets the native networking implementation to use")
endif()

set_property(CACHE NATIVE_NETWORKING_IMPL PROPERTY STRINGS "Azure_C_Shared" "Azure_Core" "None") # sets the supported values
message(STATUS "NATIVE_NETWORKING_IMPL=${NATIVE_NETWORKING_IMPL}")

# The way we link some our of test code for iOS doesn't correctly pull in the dependencies of the libraries
# we link against. This is because we use libtool to combine multiple libraries into one and we need to pass
# in the full paths to the libraries to combine into one static library. As a quick fix, let's create a
# global list of "additional" iOS specific libraries to link the tests against. You would add any needed
# libraries using e.g.
# - Link to another project: set(IOS_ADDITIONAL_STATIC_LIBS ${IOS_ADDITIONAL_STATIC_LIBS} $<TARGET_FILE:project_name_goes_here> PARENT_SCOPE)
# - Link with an external lib: set(IOS_ADDITIONAL_STATIC_LIBS ${IOS_ADDITIONAL_STATIC_LIBS} "full_path_to_lib" PARENT_SCOPE)
set(IOS_ADDITIONAL_STATIC_LIBS)

if(NOT BUILD_CORE_ONLY)
  include(codesigning)
endif()

# Set nuget versions
set(NUGET_AZURE_CORE_VER "1.44.1" CACHE STRING "NuGet package version for Azure Core")
set(NUGET_AZURE_IDENTITY_VER "1.13.1" CACHE STRING "NuGet package version for Azure Identity")

if(NOT BUILD_CORE_ONLY)

# Configure VCPKG
message(STATUS "Configuring VCPKG")
set(VCPKG_PACKAGES)

set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_LIST_DIR}/external/custom-triplets")

if(NATIVE_NETWORKING_IMPL STREQUAL "Azure_Core")
  if (CARBONSDK_OS STREQUAL "Windows")
    list(APPEND VCPKG_PACKAGES "\"wil\"")
  endif()

  if (NOT ((CARBONSDK_OS STREQUAL "Windows")
           OR (CARBONSDK_OS STREQUAL "Linux")))
    list(APPEND VCPKG_PACKAGES "\"openssl\"")
  endif()
endif()

list(REMOVE_DUPLICATES VCPKG_PACKAGES)
list(JOIN VCPKG_PACKAGES ", " VCPKG_PACKAGES_PARSED)
message(STATUS "Generating VCPKG json file. Packages to include: ${VCPKG_PACKAGES_PARSED}")
configure_file("vcpkg.json.template" "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json" @ONLY)

string(TOLOWER "${CARBONSDK_ARCH}" VCPKG_TARGET_ARCHITECTURE)
string(TOLOWER "${CARBONSDK_OS}" VCPKG_CMAKE_SYSTEM_NAME)

if ((CARBONSDK_ARCH STREQUAL arm32) OR (CARBONSDK_ARCH STREQUAL armv7))
  set(VCPKG_TARGET_ARCHITECTURE arm)
endif()

if (CARBONSDK_OS STREQUAL "WindowsUwp")
  set(VCPKG_CMAKE_SYSTEM_NAME "uwp")
elseif(CARBONSDK_TARGET_PLATFORM MATCHES "^IOS")
  set(VCPKG_CMAKE_SYSTEM_NAME "ios")
endif()

# As of the newer Android NDK CMAKE toolchains, disabling neon is no longer supported for ARM builds.
# Force the use of the arm-neon-android triplet instead to make sure neon is enabled
if ("${VCPKG_TARGET_ARCHITECTURE}-${VCPKG_CMAKE_SYSTEM_NAME}" STREQUAL "arm-android")
  set(VCPKG_TARGET_ARCHITECTURE "${VCPKG_TARGET_ARCHITECTURE}-neon")
endif()

set(VCPKG_TARGET_TRIPLET "${VCPKG_TARGET_ARCHITECTURE}-${VCPKG_CMAKE_SYSTEM_NAME}")
message(STATUS "VCPKG Triplet was set to ${VCPKG_TARGET_TRIPLET}")

if(VCPKG_PACKAGES_PARSED
    OR CARBONSDK_OS STREQUAL "Windows"
    OR (CARBONSDK_OS STREQUAL "Linux" AND NOT (VCPKG_TARGET_ARCHITECTURE MATCHES "^arm")))
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

endif()  # NOT BUILD_CORE_ONLY

if(ENABLE_TEST_BUILD)
  set(BUILD_TESTS ON)
else()
  set(BUILD_TESTS OFF)
  message(STATUS "Tests build disabled")
endif()

if(CARBONSDK_OS STREQUAL "WindowsUwp")
  option(BUILD_PYTHON "Build Python distribution" OFF)
  option(BUILD_CSHARP "Build C# module" ON)
  option(BUILD_JAVA "Build Java/Android module" OFF)
  set(use_default_uuid ON CACHE BOOL "")
  set(use_schannel OFF CACHE BOOL "")
  set(use_winrt_networking ON CACHE BOOL "")
  set(exclude_aziotsharedutil_networking ON CACHE BOOL "")
  set(use_openssl OFF CACHE BOOL "")
  set(CMAKE_SYSTEM_NAME WindowsStore)
  set(CMAKE_SYSTEM_VERSION 10.0.22000.194)
  set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION_MAXIMUM OFF)
elseif(CARBONSDK_OS MATCHES "^IOS")
  option(BUILD_PYTHON "Build Python distribution" OFF)
  option(BUILD_CSHARP "Build C# module" OFF)
  option(BUILD_JAVA "Build Java/Android module" OFF)
  # override setting from toolchain file to allow finding GStreamer
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
  set(use_builtin_httpapi ON CACHE BOOL "")
  set(use_openssl OFF CACHE BOOL "")
  set(ENABLE_BITCODE FALSE CACHE BOOL "Whether or not to enable bitcode")
  set(ENABLE_VISIBILITY TRUE CACHE BOOL "Whether or not to hide symbols (-fvisibility=hidden)")
  set(ENABLE_ARC TRUE CACHE BOOL "Enable automatic reference counting (-fvisibility=hidden)")
  if(CARBONSDK_OS STREQUAL "IOSSIMULATOR")
    if(CARBONSDK_ARCH STREQUAL "x64")
      set(PLATFORM SIMULATOR64)
    elseif(CARBONSDK_ARCH STREQUAL "x86")
      set(PLATFORM SIMULATOR)
    elseif(CARBONSDK_ARCH STREQUAL "arm64")
      set(PLATFORM SIMULATORARM64)
    else()
      # Should not reach based on earlier checks.
      message(FATAL_ERROR "internal error")
    endif()
  elseif(CARBONSDK_OS STREQUAL "IOSMACCATALYST")
    if(CARBONSDK_ARCH STREQUAL "x64")
      set(PLATFORM MAC_CATALYST)
    elseif(CARBONSDK_ARCH STREQUAL "arm64")
      set(PLATFORM MAC_CATALYST_ARM64)
    else()
      # Should not reach based on earlier checks.
      message(FATAL_ERROR "internal error")
    endif()
  else()  # Currently this means CARBONSDK_OS STREQUAL "IOS"
    set(PLATFORM OS)
    set(ARCHS ${CARBONSDK_ARCH})
    set(BUILD_TESTS OFF)  # by default, do not build tests for iOS
  endif()
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/external/ios-cmake/ios.toolchain.cmake")
  enable_language(OBJC)
  enable_language(OBJCXX)
  if(NOT DEFINED DEPLOYMENT_TARGET)
    set(DEPLOYMENT_TARGET 12.0)
  endif()
  if(EXTENSION_CODEC_ENABLED)
    add_compile_definitions("STATIC_CODEC_EXTENSION")
    set(STATIC_CODEC_EXTENSION TRUE)
  endif()
  if (EXTENSION_KWS_ENABLED)
    add_compile_definitions("STATIC_KWS_EXTENSION")
    set(STATIC_KWS_EXTENSION TRUE)
  endif()
  if (EXTENSION_AUDIO_ENABLED)
    add_compile_definitions("STATIC_AUDIO_EXTENSION")
    set(STATIC_AUDIO_EXTENSION TRUE)
  endif()
  if (EXTENSION_CUSTOM_COMMANDS_ENABLED)
    add_compile_definitions("STATIC_CUSTOM_COMMANDS_EXTENSION")
    set(STATIC_CUSTOM_COMMANDS_EXTENSION TRUE)
  endif()
  if (EXTENSION_RNNT_ENABLED)
    add_compile_definitions("STATIC_EMBEDDEDSR_EXTENSION")
    set(STATIC_EMBEDDEDSR_EXTENSION TRUE)
  endif()
  if (EXTENSION_HYBRID_TTS_ENABLED)
    add_compile_definitions("STATIC_HYBRIDTTS_EXTENSION")
    set(STATIC_HYBRIDTTS_EXTENSION TRUE)
  endif()
  if (EXTENSION_MAS_ENABLED)
    add_compile_definitions("STATIC_MAS_EXTENSION")
    set(STATIC_MAS_EXTENSION TRUE)
  endif()
  if (EXTENSION_VAD_ENABLED)
    add_compile_definitions("STATIC_VAD_EXTENSION")
    set(STATIC_VAD_EXTENSION TRUE)
  endif()
  set(MACOSX ON)
elseif(CARBONSDK_OS STREQUAL "OSX")
  option(BUILD_PYTHON "Build Python distribution" OFF)
  if(BUILD_CORE_ONLY)
    option(BUILD_CSHARP "Build C# module" OFF)
    option(BUILD_JAVA "Build Java/Android module" OFF)
  else()
    option(BUILD_CSHARP "Build C# module" ON)
    option(BUILD_JAVA "Build Java/Android module" ON)
  endif()
  set(ENABLE_BITCODE FALSE CACHE BOOL "Whether or not to enable bitcode")
  set(ENABLE_VISIBILITY TRUE CACHE BOOL "Whether or not to hide symbols (-fvisibility=hidden)")
  set(ENABLE_ARC TRUE CACHE BOOL "Enable automatic reference counting (-fvisibility=hidden)")
  if(CARBONSDK_ARCH STREQUAL "x64")
    set(PLATFORM MAC)
    if(NOT DEFINED DEPLOYMENT_TARGET)
      set(DEPLOYMENT_TARGET 10.14)
    endif()
  elseif(CARBONSDK_ARCH STREQUAL "arm64")
    set(PLATFORM MAC_ARM64)
    if(NOT DEFINED DEPLOYMENT_TARGET)
      set(DEPLOYMENT_TARGET 11.0)
    endif()
  else()
    # Should not reach based on earlier checks.
    message(FATAL_ERROR "internal error")
  endif()
  if(NOT BUILD_CORE_ONLY)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/external/ios-cmake/ios.toolchain.cmake")
    enable_language(OBJC)
    enable_language(OBJCXX)
  endif()
  set(use_builtin_httpapi ON CACHE BOOL "")
  set(use_openssl OFF CACHE BOOL "")
  set(MACOSX ON)
  set(DISABLE_KWS_TESTS OFF)
  add_compile_definitions("UNDER_OSX_ONLY")
  if (EXTENSION_KWS_ENABLED)
    add_compile_definitions("STATIC_KWS_EXTENSION")
    set(STATIC_KWS_EXTENSION TRUE)
  endif()
  if (EXTENSION_AUDIO_ENABLED)
    add_compile_definitions("STATIC_AUDIO_EXTENSION")
    set(STATIC_AUDIO_EXTENSION TRUE)
  endif()
  if (EXTENSION_MAS_ENABLED)
    add_compile_definitions("STATIC_MAS_EXTENSION")
    set(STATIC_MAS_EXTENSION TRUE)
  endif()
  if (EXTENSION_VAD_ENABLED)
    add_compile_definitions("STATIC_VAD_EXTENSION")
    set(STATIC_VAD_EXTENSION TRUE)
  endif()
elseif(CARBONSDK_OS STREQUAL "Android")
  option(BUILD_PYTHON "Build Python distribution" OFF)
  option(BUILD_CSHARP "Build C# module" OFF)
  option(BUILD_JAVA "Build Java/Android module" ON)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
  set(use_default_uuid ON CACHE BOOL "")
  set(CMAKE_SYSTEM_NAME Android)
  set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)
  if(CARBONSDK_ARCH STREQUAL "arm32")
    set(CMAKE_ANDROID_ARCH_ABI "armeabi-v7a")
    set(CMAKE_SYSTEM_VERSION 21)
    set(CMAKE_ANDROID_API 21)
  elseif(CARBONSDK_ARCH STREQUAL arm64)
    set(CMAKE_ANDROID_ARCH_ABI "arm64-v8a")
    set(CMAKE_SYSTEM_VERSION 21)
    set(CMAKE_ANDROID_API 21)
  elseif(CARBONSDK_ARCH STREQUAL "x64")
    set(CMAKE_ANDROID_ARCH_ABI x86_64)
    set(CMAKE_SYSTEM_VERSION 21)
    set(CMAKE_ANDROID_API 21)
  else()
    # Should never reach this based on earlier checks.
    message(FATAL_ERROR "internal error")
  endif()
  set(CMAKE_ANDROID_STL_TYPE c++_static)
  if(NATIVE_NETWORKING_IMPL STREQUAL "Azure_C_Shared")
    set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/android-openssl/${CMAKE_ANDROID_ARCH_ABI}/usr/local" CACHE STRING "")
  endif()
  set(use_openssl ON CACHE BOOL "")
elseif(CARBONSDK_OS STREQUAL "Windows")
  option(BUILD_PYTHON "Build Python distribution" OFF)
  if(BUILD_CORE_ONLY)
    option(BUILD_CSHARP "Build C# module" OFF)
    option(BUILD_JAVA "Build Java/Android module" OFF)
  else()
    option(BUILD_CSHARP "Build C# module" ON)
    option(BUILD_JAVA "Build Java/Android module" ON)
  endif()
  set(use_openssl OFF CACHE BOOL "")
elseif(CARBONSDK_OS STREQUAL "Linux")
  set(use_openssl ON CACHE BOOL "")
  set(use_openssl_dynamic ON CACHE BOOL "")
  option(BUILD_PYTHON "Build Python distribution" OFF)
  if(BUILD_CORE_ONLY)
    option(BUILD_JAVA "Build Java/Android module" OFF)
  else()
    option(BUILD_JAVA "Build Java/Android module" ON)
  endif()
  option(BUILD_CSHARP "Build C# module" OFF)
else()
  message(FATAL_ERROR "internal error")
endif()

if(BUILD_JAVA)
  find_program(MAVEN_EXECUTABLE mvn)
  if(NOT MAVEN_EXECUTABLE)
    message(WARNING "Maven executable 'mvn' was not found. Disabling BUILD_JAVA for this configure.")
    set(BUILD_JAVA OFF CACHE BOOL "Build Java/Android module" FORCE)
  endif()
endif()

message(STATUS "CMake version info: ${CMAKE_VERSION}")
message(STATUS "BUILD_PYTHON=${BUILD_PYTHON}")
message(STATUS "BUILD_CSHARP=${BUILD_CSHARP}")
message(STATUS "BUILD_JAVA=${BUILD_JAVA}")

# Configure CCache if available
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(carbon)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

if(use_openssl)
  add_compile_definitions("SPEECHSDK_USE_OPENSSL")
endif()

if (CARBONSDK_BUILD_TYPE STREQUAL "prod")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCARBONSDK_BUILD_TYPE_PROD")
elseif(CARBONSDK_BUILD_TYPE STREQUAL "int")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCARBONSDK_BUILD_TYPE_INT")
endif()

if(MSVC)
  add_compile_options(
    -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
    /W4 # Generate L4 warnings
    /we4242 # enable warning C4242 (default off)
    /we4254 # enable warning C4254 (default off)
    /we4477
    /we4313
    /WX # ...and treat them as errors
    /MP # Enable multiprocessor optimizations (build faster with more cores)
    /guard:cf # Add code flow guard protection -- small size hit for security -- must add to linker, too
    /Zi # Emit debug information to a separate PDB
    /Gw # Export global data (not just functions via auto-applied /Gy) to separate COMDAT for optimizations
    /GL # Perform whole-program optimizations (allows cross-module awareness)
    /Ob0 # Disable all inline expansion
    /utf-8 # Make sure we use utf-8 character set for the files.
    /EHsc # Exception unwinding
    /Zc:__cplusplus # Enable the __cplusplus macro to report the supported standard C++ language level
    $<$<CONFIG:DEBUG>:/Od> # For debug builds: don't do additional optimization (faster compilation/better debug)
    $<$<CONFIG:DEBUG>:/JMC> # For debug builds: enable 'just my code' to not automatically step into external items
    $<$<CONFIG:RELEASE>:/O1> # For release builds: use the O1 "optimize for size" presets
    $<$<STREQUAL:${CMAKE_SYSTEM_NAME},WindowsStore>:/DSPX_UWP>
  )

  # Disable runtime type information (RTTI) -- significant size reduction, no dynamic_*_cast
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
  if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
    # MSVC ASAN requires /INCREMENTAL:NO
    add_link_options(/INCREMENTAL:NO)
    add_compile_definitions(ASAN_BUILD=1)
  endif()

  add_link_options(
    /WX # Always treat linker warnings as errors
    /INCREMENTAL:NO # Never use an incremental build (required with other options)
    /LTCG # Allow the linker to work with the compiler for whole-program, cross-module optimization
    /DEBUG # Always emit debug information, even for release builds
    /OPT:REF,ICF # Remove unreferenced COMDAT sections and fold duplicate COMDAT sections
    /MAP # Always emit a code map
    /guard:cf # Add code flow guard protection -- small size hit for security -- must add to compiler, too
    /DEBUGTYPE:CV,FIXUP # Allow CFGCheck analysis
  )

  if (CARBONSDK_ARCH STREQUAL "arm64")
    # CMake's "helpful" logic autogenerates a lib path that includes an arm64 lib folder that does not exist.
    # This causes a cs1668 warning that's very challenging to suppress. Failing all else, let's just create the
    # folder.
    set(_PROGX86 "ProgramFiles(x86)")
    set(_NETFX_SDK_LIB_FOLDER "$ENV{${_PROGX86}}/Windows Kits/NETFXSDK/4.8/lib/um")
    if (EXISTS ${_NETFX_SDK_LIB_FOLDER})
      file(MAKE_DIRECTORY "${_NETFX_SDK_LIB_FOLDER}\\arm64")
    endif()
  endif()

elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wno-long-long -Wpedantic -Wformat-security")
  if (CMAKE_CXX_STANDARD EQUAL 14)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-noexcept-type")
  endif()
  set(CMAKE_POSITION_INDEPENDENT_CODE ON) # -fPIC -fPIE

  add_compile_options(
    -g3 # Include debug information in release builds (strip will remove it later)
    $<$<CONFIG:DEBUG>:-Og> # Optimize debug builds for debugging
    $<$<CONFIG:RELEASE>:-Os> # Optimize release builds for size
    -finput-charset=UTF-8 # Make sure we use utf-8 character set.
    #[[
        To further reduce size, we enable link-time optimization (LTO) for release builds.

        NOTE: "fat" LTO objects cause a BIG impact to compilation time, but are necessary
          to build properly-functioning binaries both with and without LTO enabled at link
          time. libcarbon-mock on GCC 5 has issues with LTO and needs LTO disabled (fixed in
          GCC 7/9 and can likely go away once 16.04 is deprecated), which means we need fat
          objects.
    ]]
    $<$<CONFIG:RELEASE>:-flto> # Enable link-time optimization
    $<$<CONFIG:RELEASE>:-ffat-lto-objects> # Necessary for functionally correct link with LTO off
  )

  # Disable runtime type information (RTTI) -- significant size reduction, no dynamic_*_cast
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
  
  # Add Address Sanitizer support if enabled
  if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    add_compile_definitions(ASAN_BUILD=1)
  endif()

  # Link-time optimization needs some extra tools specificity to build correctly
  # across all platforms
  SET(CMAKE_AR  "gcc-ar")
  SET(CMAKE_RANLIB "gcc-ranlib")

elseif(ANDROID)
  # Android compiles with Clang--using a few extra settings.
  set(CMAKE_POSITION_INDEPENDENT_CODE ON) # -fPIC -fPIE
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wno-long-long -Wpedantic -Wno-gnu-zero-variadic-macro-arguments -Wno-attributes -Wno-unused-lambda-capture -Wformat-security")

  add_compile_options(
    #[[
      compilation flags required by security checks
      stack protector adds checks to prevent buffer overflows on stack allocated arrays
      _FORTIFY_SOURCE=2 replaces glibc functions that are otherwise unsafe with their safe replacements (e.g. strcpy vs. strncpy)
      See https://linux.die.net/man/7/feature_test_macros
      The stack protector setting enables protection against stack attacks using buffer overflows on stack allocated memory.
      See http://lists.llvm.org/pipermail/cfe-dev/2017-April/053662.html
    ]]
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
    -fvisibility=hidden # Reduces storing unnecessary symbol information that is private to the Shared Objects.
    -fvisibility-inlines-hidden # Give inline C++ member functions hidden visibility by default
    -ffunction-sections # Separates functions out to individual sections that can be optimized out.
    -fdata-sections
    -g3 # Include debug information in release builds (strip will remove it later)
    $<$<CONFIG:DEBUG>:-Og> # Optimize debug builds for debugging
    $<$<CONFIG:RELEASE>:-Oz> # Optimize release builds for size
    -D__ANDROID__ # Use __ANDROID__ to make the macro in codes simpler
  )

  add_link_options(
    -Wl,--gc-sections # strips dead code built with -fdata-sections
    -Wl,-z,max-page-size=16384 # Support 16 KB page sizes
    )

  # Disable runtime type information (RTTI) -- significant size reduction, no dynamic_*_cast
  # Setting fno-rtti using CMAKE_CXX_FLAGS will not impact test application targets
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
  
  # Add Address Sanitizer support if enabled
  if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    add_compile_definitions(ASAN_BUILD=1)
  endif()

  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -z nocopyreloc -z relro -z now") # -Wl,-z,relro
  
  # For ARM64, link compiler-rt builtins to resolve atomic intrinsics from OpenSSL
  # This must come AFTER the shared linker flags to ensure proper link order
  if(CARBONSDK_ARCH STREQUAL arm64 AND DEFINED ENV{ARM64_CLANG_RT_BUILTINS})
    message(STATUS "Adding compiler-rt builtins library for ARM64 Android: $ENV{ARM64_CLANG_RT_BUILTINS}")
    link_libraries($ENV{ARM64_CLANG_RT_BUILTINS})
  endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # compilation flags required by security checks
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong -Wformat -Wformat-security -Werror")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong -Wformat -Wformat-security -Werror")

  # Disable runtime type information (RTTI) -- significant size reduction, no dynamic_*_cast
  # NOTE: OSX has a version of clang that uniquely makes the combination of RTTI off and exceptions enabled
  # a somewhat undefined state. Pending full investigation and/or turning exceptions off, we leave RTTI on
  # for OSX.
  if (NOT DEFINED MACOSX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
  endif()
  
  # Add Address Sanitizer support if enabled
  if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    add_compile_definitions(ASAN_BUILD=1)
  endif()

  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g")

  set(CMAKE_POSITION_INDEPENDENT_CODE ON) # -fPIC -fPIE

  # visibility
  cmake_policy(SET CMP0063 NEW)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
endif()

# Enable RNN-T build with 'cmake -DEXTENSION_RNNT_ENABLED=true'
if(EXTENSION_RNNT_ENABLED AND
   ((CARBONSDK_OS MATCHES "^Windows" AND NOT CARBONSDK_ARCH STREQUAL "arm32") OR
    CARBONSDK_TARGET_PLATFORM STREQUAL "Linux-x64" OR
    CARBONSDK_TARGET_PLATFORM STREQUAL "Linux-arm64" OR
    CARBONSDK_TARGET_PLATFORM STREQUAL "Linux-arm32" OR
    CARBONSDK_OS STREQUAL "Android" OR
    CARBONSDK_OS STREQUAL "OSX" OR
    CARBONSDK_TARGET_PLATFORM MATCHES "^IOS"))
  set(BUILD_RNNT ON)
  add_compile_definitions(BUILD_RNNT)
else()
  set(BUILD_RNNT OFF)
endif()

message(STATUS "BUILD_RNNT=${BUILD_RNNT}")

# Enable CLI tool with 'cmake -DTOOL_SPX_ENABLED=true'
if(TOOL_SPX_ENABLED)
  set(BUILD_SPX_TOOL ON)
else()
  set(BUILD_SPX_TOOL OFF)
endif()

message(STATUS "BUILD_SPX_TOOL=${BUILD_SPX_TOOL}")

set(CARBON_ROOT ${PROJECT_SOURCE_DIR})
set(CARBON_SOURCE "${CARBON_ROOT}/source")
set(CARBON_PUBLIC "${CARBON_SOURCE}/public")
set(CARBON_BINDINGS "${CARBON_SOURCE}/bindings")
set(CARBON_EXTERNAL "${CARBON_ROOT}/external")
set(CARBON_EXTENSIONS "${CARBON_SOURCE}/extensions")
set(CARBON_JSON_PARSER "${CARBON_SOURCE}/core/json")

# configure the C# strong name signing keys to use
if (BUILD_CSHARP)
  set(STRONG_NAME_SIGNING_KEY "${CARBON_ROOT}/ci/build/cognitive_services_speech_keypair.snk")
  set(STRONG_NAME_TEST_SIGNING_KEY "${CARBON_ROOT}/ci/build/cognitive_services_tests_keypair.snk")

endif()

# include folders
set(CARBON_C_API "${CARBON_PUBLIC}/c_api")
set(CARBON_CXX_API "${CARBON_PUBLIC}/cxx_api")

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
  set(SPEECHSDK_CORE_LIBRARY "Microsoft.CognitiveServices.Speech.core")
elseif(APPLE)
  set(SPEECHSDK_CORE_LIBRARY "Microsoft.CognitiveServices.Speech.core")
  set(SPEECHSDK_CORE_FRAMEWORK "MicrosoftCognitiveServicesSpeech")
else()
  set(SPEECHSDK_CORE_LIBRARY "Microsoft.CognitiveServices.Speech.core")
endif()

set(JAVA_JNI_LIBRARY Microsoft.CognitiveServices.Speech.java.bindings)

file(GLOB SPEECH_C_API_HEADERS "${CARBON_C_API}/*.h")
file(GLOB SPEECH_CXX_API_HEADERS "${CARBON_CXX_API}/*.h")


# Enable hybrid TTS build with 'cmake -DEXTENSION_HYBRID_TTS_ENABLED=true'
if(EXTENSION_HYBRID_TTS_ENABLED AND
   ((CARBONSDK_OS MATCHES "^Windows" AND NOT CARBONSDK_ARCH STREQUAL "arm32") OR
    CARBONSDK_TARGET_PLATFORM STREQUAL "Linux-x64" OR
    CARBONSDK_TARGET_PLATFORM STREQUAL "Linux-arm64" OR
    CARBONSDK_TARGET_PLATFORM STREQUAL "Linux-arm32" OR
    CARBONSDK_OS STREQUAL "Android" OR
    CARBONSDK_OS STREQUAL "OSX" OR
    CARBONSDK_TARGET_PLATFORM MATCHES "^IOS"))
  set(BUILD_HYBRID_TTS ON)
  add_compile_definitions("EXTENSION_HYBRID_TTS_ENABLED")
else()
  set (BUILD_HYBRID_TTS OFF)
endif()

message(STATUS "BUILD_HYBRID_TTS=${BUILD_HYBRID_TTS}")

set(BUILD_1DS_TELEMETRY OFF)
if (BUILD_HYBRID_TTS OR BUILD_RNNT)
  if ((CARBONSDK_OS MATCHES "^Windows" AND NOT CARBONSDK_ARCH STREQUAL "arm32") OR
      (CARBONSDK_OS STREQUAL "Linux" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 9.0) OR
      CARBONSDK_OS STREQUAL "OSX" OR
      CARBONSDK_OS STREQUAL "Android")
    set(BUILD_1DS_TELEMETRY ON)
  endif()
endif()

message(STATUS "BUILD_1DS_TELEMETRY=${BUILD_1DS_TELEMETRY}")

set(TESTS "${CARBON_ROOT}/tests")
set(JAVA_SPEECH_TESTS "${TESTS}/functional/java/speech/src/test/java")

set(SPX_TOOL "${CARBON_ROOT}/clis/.carbondev/spx")
set(SPX_TEST_ADAPTER "${CARBON_ROOT}/clis/.carbondev/adapter")
set(SPX_TEST_RUNNER "${CARBON_ROOT}/clis/.carbondev/runner")

set(COMMON_GENERATED_INCLUDE ${CMAKE_BINARY_DIR}/common-generated-include)

if(NOT BUILD_CORE_ONLY)
  add_subdirectory("${CARBON_EXTERNAL}")
endif()

# needs to be BOTH to find GStreamer for iOS
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)

if(EXTENSION_CODEC_ENABLED)
  add_definitions(-DSPEECHSDK_GSTREAMER_CODEC_ENABLED)
  if((CARBONSDK_OS STREQUAL "Windows"))
    if(CARBONSDK_ARCH STREQUAL "x64")
      set(GSTREAMER_PKG "${GST_ROOT}/1.0/msvc_x86_64")
    endif()
    if(CARBONSDK_ARCH STREQUAL "x86")
      set(GSTREAMER_PKG "${GST_ROOT}/1.0/msvc_x86")
    endif()
  endif()

  if(CARBONSDK_OS STREQUAL "Android")
    set(GSTREAMER_PKG "${GST_ROOT}")
  endif()

  set(GSTREAMER_OS_LIST Android Linux IOS IOSSIMULATOR IOSSIMULATOR64 Windows)
  if(CARBONSDK_OS IN_LIST GSTREAMER_OS_LIST)
    set(BUILD_CODEC ON)
  endif()
endif()

add_subdirectory(${CARBON_SOURCE})


# Add a top level Nuget restore command
if (BUILD_CSHARP AND (NOT ENABLE_VS_INTEGRATION) AND (NOT APPLE))
  find_program(NUGET_PROGRAM nuget)
  if (NOT NUGET_PROGRAM)
    message(FATAL_ERROR "Could not find Nuget.")
  endif()

  # when restoring nuget packages on Linux, must use `|| true` to prevent first time failure (no csprojs created yet, so nothing to restore)
  if (UNIX)
    add_custom_target(NUGET_RESTORE ALL
      COMMENT "Restoring Nuget packags"
      COMMAND ${NUGET_PROGRAM} restore || true
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  else()
    add_custom_target(NUGET_RESTORE ALL
      COMMENT "Restoring Nuget packags"
      COMMAND ${NUGET_PROGRAM} restore
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  endif()
  set_target_properties(NUGET_RESTORE PROPERTIES FOLDER bindings/csharp)
endif()

if(NOT BUILD_CORE_ONLY)
  add_subdirectory(${CARBON_BINDINGS})
endif()

if(BUILD_TESTS OR NOT BUILD_CORE_ONLY)
  add_subdirectory(${TESTS})
endif()

if(BUILD_SPX_TOOL)
  add_subdirectory(${SPX_TOOL})
endif()
if(BUILD_SPX_TOOL)
  add_subdirectory(${SPX_TEST_ADAPTER})
  add_subdirectory(${SPX_TEST_RUNNER})
endif()

if(BUILD_CORE_ONLY)
  if(BUILD_TESTS)
    # Copy the minimum required test settings for core-only tests
    if(CARBONSDK_OS STREQUAL "Windows")
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")
      file(COPY "${TESTS}/test.audio.utterances.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")
    else()
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
      file(COPY "${TESTS}/test.audio.utterances.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin")
    endif()
  endif()
else()

# test.subscriptions.regions.json is an exceedingly commonly-populated file for local development and validation, but
# it's also exceedingly important that a populated version not reach source control. .gitignore is a useful tool, but
# can only apply for files that aren't under source control -- so here we use indirection and keep an empty "template"
# file in source control that we failover copy if and only if another version hasn't been copied.
if(NOT EXISTS "${TESTS}/test.subscriptions.regions.json")
  file(TOUCH "${TESTS}/test.subscriptions.regions.json")
    ##TODO: Populate file from key vault
endif()

# create a dummy certificate file first
if(NOT EXISTS "${TESTS}/test.certificates.json")
  file(TOUCH "${TESTS}/test.certificates.json")
endif()

if(${CMAKE_VERSION} VERSION_GREATER "3.14.4")
  #Since windows might build debug or release version after running cmake, we copy to both just in case

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/carbontests.runsettings" "${CMAKE_BINARY_DIR}" "/.runsettings")

  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.defaults.json" "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug" "/test.defaults.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.subscriptions.regions.json" "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug" "/test.subscriptions.regions.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.audio.utterances.json" "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug" "/test.audio.utterances.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/input" "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug" "/input")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.certificates.json" "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug" "/test.certificates.json")

  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.defaults.json" "${CARBON_ROOT}/tests/functional/java/speech" "/test.defaults.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.subscriptions.regions.json" "${CARBON_ROOT}/tests/functional/java/speech" "/test.subscriptions.regions.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.audio.utterances.json" "${CARBON_ROOT}/tests/functional/java/speech" "/test.audio.utterances.json")
  if(NOT EXISTS "${CARBON_ROOT}//tests/functional/java/speech/input")
    CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/input" "${CARBON_ROOT}/tests/functional/java/speech" "/input")
  endif()

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.defaults.json" "${CMAKE_CURRENT_BINARY_DIR}/bin/Release" "/test.defaults.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.subscriptions.regions.json" "${CMAKE_CURRENT_BINARY_DIR}/bin/Release" "/test.subscriptions.regions.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.audio.utterances.json" "${CMAKE_CURRENT_BINARY_DIR}/bin/Release" "/test.audio.utterances.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.certificates.json" "${CMAKE_CURRENT_BINARY_DIR}/bin/Release" "/test.certificates.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/input" "${CMAKE_CURRENT_BINARY_DIR}/bin/Release" "/input")

  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.defaults.json" "${CMAKE_CURRENT_BINARY_DIR}/bin" "/test.defaults.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.subscriptions.regions.json" "${CMAKE_CURRENT_BINARY_DIR}/bin" "/test.subscriptions.regions.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.audio.utterances.json" "${CMAKE_CURRENT_BINARY_DIR}/bin" "/test.audio.utterances.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.certificates.json" "${CMAKE_CURRENT_BINARY_DIR}/bin" "/test.certificates.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/input" "${CMAKE_CURRENT_BINARY_DIR}/bin" "/input")

  if(NOT EXISTS "${CARBON_ROOT}/tests/functional/java/speech/lib")
    CREATE_LINK_WITH_COPY_FAILOVER("${CMAKE_CURRENT_BINARY_DIR}/lib" "${CARBON_ROOT}/tests/functional/java/speech" "/lib")
  endif()

  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.defaults.json" "${CMAKE_CURRENT_SOURCE_DIR}/source/bindings/python" "/test.defaults.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.subscriptions.regions.json" "${CMAKE_CURRENT_SOURCE_DIR}/source/bindings/python" "/test.subscriptions.regions.json")
  CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/test.audio.utterances.json" "${CMAKE_CURRENT_SOURCE_DIR}/source/bindings/python" "/test.audio.utterances.json")

  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/bindings/python/input")
    CREATE_LINK_WITH_COPY_FAILOVER("${TESTS}/input" "${CMAKE_CURRENT_SOURCE_DIR}/source/bindings/python" "/input")
  endif()
else()
  if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")
    if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/bin")
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
    endif()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")
  endif()

  file(COPY "${TESTS}/carbontests.runsettings" "${CMAKE_BINARY_DIR}/.runsettings")
  file(COPY "${TESTS}/test.defaults.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")
  file(COPY "${TESTS}/test.subscriptions.regions.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")
  file(COPY "${TESTS}/test.audio.utterances.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")
  file(COPY "${TESTS}/input" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")
  file(COPY "${TESTS}/test.certificates.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/Debug")

  if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")
  endif()

  file(COPY "${TESTS}/test.defaults.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")
  file(COPY "${TESTS}/test.subscriptions.regions.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")
  file(COPY "${TESTS}/test.audio.utterances.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")
  file(COPY "${TESTS}/input" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")
  file(COPY "${TESTS}/test.certificates.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/Release")

  file(COPY "${TESTS}/test.defaults.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin")
  file(COPY "${TESTS}/test.subscriptions.regions.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin")
  file(COPY "${TESTS}/test.audio.utterances.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin")
  file(COPY "${TESTS}/input" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin")
  file(COPY "${TESTS}/test.certificates.json" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin")

  if(NOT EXISTS "${CARBON_ROOT}/tests/functional/java/speech/lib")
    file(COPY "${CMAKE_CURRENT_BINARY_DIR}/lib" DESTINATION "${CARBON_ROOT}/tests/functional/java/speech")
  endif()

  file(COPY "${TESTS}/test.defaults.json" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/source/bindings/python")
  file(COPY "${TESTS}/test.subscriptions.regions.json" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/source/bindings/python")
  file(COPY "${TESTS}/test.audio.utterances.json" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/source/bindings/python")
  file(COPY "${TESTS}/input" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/source/bindings/python")
endif()

endif()  # BUILD_CORE_ONLY
