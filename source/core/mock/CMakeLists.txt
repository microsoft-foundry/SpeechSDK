#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(mock)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")

set(HEADERS)

set(HEADERS_PRIVATE
    "${SRC_DIR}/stdafx.h"
    "${SRC_DIR}/create_module_object.h"
    "${SRC_DIR}/mock_audio_file_impl.h"
    "${SRC_DIR}/mock_audio_reader.h"
    "${SRC_DIR}/mock_audio_reader_impl.h"
    "${SRC_DIR}/mock_interactive_microphone.h"
    "${SRC_DIR}/mock_kws_engine_adapter.h"
    "${SRC_DIR}/mock_reco_engine_adapter.h"
    "${SRC_DIR}/mock_wav_file_reader.h"
    "${SRC_DIR}/mock_wav_file_pump.h"
    "${SRC_DIR}/mocks_manager.h"
    "${SRC_DIR}/mock_vad_engine_adapter.h"
    )

set(SOURCES
    "${SRC_DIR}/create_module_object.cpp"
    "${SRC_DIR}/mock_audio_reader_impl.cpp"
    "${SRC_DIR}/mock_interactive_microphone.cpp"
    "${SRC_DIR}/mock_kws_engine_adapter.cpp"
    "${SRC_DIR}/mock_reco_engine_adapter.cpp"
    "${SRC_DIR}/mock_wav_file_pump.cpp"
    "${SRC_DIR}/mocks_manager.cpp"
    "${SRC_DIR}/stdafx.cpp"
    "${SRC_DIR}/mock_vad_engine_adapter.cpp"
    )

set_source_groups()

add_library(mock SHARED ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_include_directories(mock
    PRIVATE
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Disable LTO for this library as it seems to have problems with GCC 5
  string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fno-lto")
endif()

add_security_compile_flags(${PROJECT_NAME})

set_target_properties (mock PROPERTIES
    OUTPUT_NAME "carbon-mock"
    FOLDER core
)

target_link_libraries(mock PRIVATE interfaces common ${SPEECHSDK_CORE_LIBRARY})
