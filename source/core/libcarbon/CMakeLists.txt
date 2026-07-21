#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(${SPEECHSDK_CORE_LIBRARY})

set(SRC_DIR "${PROJECT_SOURCE_DIR}")

set(HEADERS)

set(HEADERS_PRIVATE "${SRC_DIR}/stdafx.h")

set(SOURCES
  "${SRC_DIR}/dll.cpp"
  "${SRC_DIR}/stdafx.cpp"
  )

  if(CARBONSDK_OS MATCHES "^IOS")
  if(CARBONSDK_OS STREQUAL "IOSSIMULATOR")
      if(CARBONSDK_ARCH STREQUAL "x64")
          set(ONNX_RUNTIME_PATH "${CARBON_EXTERNAL}/onnx_runtime/iOS_x86_64")
      else()
          set(ONNX_RUNTIME_PATH "${CARBON_EXTERNAL}/onnx_runtime/iOS_arm64_Simulator")
      endif()
  else()
      if(CARBONSDK_ARCH STREQUAL "arm64" OR CARBONSDK_ARCH STREQUAL "all")
          set(ONNX_RUNTIME_PATH "${CARBON_EXTERNAL}/onnx_runtime/iOS_arm64")
      endif()
  endif()
elseif(CARBONSDK_OS STREQUAL "OSX")
  if(CARBONSDK_ARCH STREQUAL "x64")
      set(ONNX_RUNTIME_PATH "${CARBON_EXTERNAL}/onnx_runtime/MacOS_x64")
  elseif(CARBONSDK_ARCH STREQUAL "arm64")
      set(ONNX_RUNTIME_PATH "${CARBON_EXTERNAL}/onnx_runtime/MacOS_arm64")
  else()
      # Should not reach here.
      message(FATAL_ERROR "${CARBONSDK_ARCH} is not supported for ${CARBONSDK_OS}.")
  endif()
endif()

set(ADDITIONAL_LIBS)

if(WIN32)
  if (MSVC)
    set_source_files_properties("${SRC_DIR}/stdafx.cpp"
      PROPERTIES
      COMPILE_FLAGS "/Ycstdafx.h"
      )

    # For precompiled header.
    # Set
    # "Precompiled Header" to "Use (/Yu)"
    # "Precompiled Header File" to "stdafx.h"
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Yustdafx.h /FIstdafx.h")

  endif()
  list(APPEND HEADERS_PRIVATE "${SRC_DIR}/windows/targetver.h")

  set(SPEECHSDK_ORIGINAL_FILENAME "${SPEECHSDK_CORE_LIBRARY}.dll")
  configure_file(
    ${CARBON_ROOT}/ci/version.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/windows/version.rc)

  list(APPEND SOURCES "${CMAKE_CURRENT_BINARY_DIR}/windows/version.rc")
  list(APPEND ADDITIONAL_LIBS ole32)
endif()

set_source_groups()

add_library(${PROJECT_NAME} SHARED ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES} $<TARGET_OBJECTS:c_api>)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${SRC_DIR}
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
)

# Disable LNK4286 (caused by importing diagnostics_log_message)
if(MSVC)
  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4286")
endif()

if(APPLE)
  add_library(${PROJECT_NAME}_static STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES} $<TARGET_OBJECTS:c_api> $<TARGET_OBJECTS:shared_c_api>)

  target_include_directories(${PROJECT_NAME}_static
      PRIVATE
          ${SRC_DIR}
          ${CARBON_C_API}
          ${CARBON_CXX_API}
          ${CARBON_JSON_PARSER}
  )

  target_link_libraries(${PROJECT_NAME}_static
      PRIVATE interfaces
      PRIVATE common
      PRIVATE sr
      PRIVATE logging
      PRIVATE factory
      PRIVATE factory_speech
      PRIVATE usp
      PRIVATE shared_c_api
  )

  set_target_properties(${PROJECT_NAME}
    PROPERTIES BUILD_RPATH "@loader_path")
elseif(UNIX)
  set_target_properties(${PROJECT_NAME}
    PROPERTIES BUILD_RPATH "$ORIGIN")
  target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-z,now")
endif()

add_security_compile_flags(${PROJECT_NAME})

if(${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
  if (${use_openssl})
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
  endif()
endif()

# These lines are only a workaround around some gcc5 lto bugs. When no longer using gcc5 this is fine to be removed.
if((CMAKE_CXX_COMPILER_ID MATCHES "GNU") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "6"))
  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-flto-partition=one")
endif()

set_target_properties (${PROJECT_NAME} PROPERTIES
  FOLDER core
  COMPILE_DEFINITIONS "PAL_CONFIG_EXPORTAPIS"
)

# the following list are needed to make ld producing a .core.so that can be loaded by a python wheel
target_link_libraries(${PROJECT_NAME}
    PRIVATE interfaces
    PRIVATE common
    PRIVATE logging
    PRIVATE factory
    PRIVATE factory_speech
    PRIVATE usp
    PRIVATE shared_c_api
    PRIVATE pal_binding
    PRIVATE ${ADDITIONAL_LIBS}
  )

if(ANDROID)
  if(${use_openssl})
    target_link_libraries(${PROJECT_NAME} PRIVATE z dl) # libz dl
  endif()
endif()

if(APPLE)
  FIND_LIBRARY(CORE_FOUNDATION CoreFoundation)
  if (NOT CORE_FOUNDATION)
    message(FATAL_ERROR "CoreFoundation not found")
  endif()
  target_link_libraries(${PROJECT_NAME}
    PRIVATE ${CORE_FOUNDATION}
    )
endif()

if (EXTENSION_CODEC_ENABLED AND STATIC_CODEC_EXTENSION)
  target_link_libraries(${PROJECT_NAME} PRIVATE codec)
endif()

if (EXTENSION_KWS_ENABLED AND STATIC_KWS_EXTENSION)
  target_link_libraries(${PROJECT_NAME} PRIVATE kws_factory)
endif()

if (EXTENSION_AUDIO_ENABLED AND STATIC_AUDIO_EXTENSION)
  target_link_libraries(${PROJECT_NAME} PRIVATE audio_extension)
endif()

if (EXTENSION_RNNT_ENABLED AND STATIC_EMBEDDEDSR_EXTENSION)
  target_link_libraries(${PROJECT_NAME} PRIVATE embedded_sr ${ONNX_RUNTIME_PATH}/libonnxruntime_static.a)
endif()

if (EXTENSION_HYBRID_TTS_ENABLED AND STATIC_HYBRIDTTS_EXTENSION)
  target_link_libraries(${PROJECT_NAME} PRIVATE hybrid_tts)
endif()

if (EXTENSION_MAS_ENABLED AND STATIC_MAS_EXTENSION)
  target_link_libraries(${PROJECT_NAME} mas_extension)
endif()

if (EXTENSION_VAD_ENABLED AND STATIC_VAD_EXTENSION)
  target_link_libraries(${PROJECT_NAME} PRIVATE vad_extension)
endif()
