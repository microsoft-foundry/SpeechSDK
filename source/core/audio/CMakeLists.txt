#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(audio)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS_PRIVATE
    "${SRC_DIR}/stdafx.h"
    "${SRC_DIR}/audio_pump.h"
    "${SRC_DIR}/audio_config.h"
    "${SRC_DIR}/audio_data_stream.h"
    "${SRC_DIR}/audio_data_stream_shared_adapter.h"
    "${SRC_DIR}/pull_audio_input_stream.h"
    "${SRC_DIR}/push_audio_input_stream.h"
    "${SRC_DIR}/pull_audio_output_stream.h"
    "${SRC_DIR}/push_audio_output_stream.h"
    "${SRC_DIR}/single_to_many_stream_reader_adapter.h"
    "${SRC_DIR}/single_to_many_stream_reader.h"
    "${SRC_DIR}/audio_processor_write_to_audio_source_buffer.h"
    "${SRC_DIR}/create_module_object.h"
    "${SRC_DIR}/interactive_microphone.h"
    "${SRC_DIR}/internal_audio_codec_adapter.h"
    "${SRC_DIR}/wav_file_reader.h"
    "${SRC_DIR}/wav_file_pump.h"
    "${SRC_DIR}/wav_file_writer.h"
    "${SRC_DIR}/audio_source_wrapper.h"
    "${SRC_DIR}/microphone_audio_source_adapter.h"
    "${SRC_DIR}/file_audio_source_adapter.h"
    "${SRC_DIR}/stream_audio_source_adapter.h"
    "${SRC_DIR}/audio_processing_options.h"
    )

set(SOURCES
    "${SRC_DIR}/audio_config.cpp"
    "${SRC_DIR}/audio_pump.cpp"
    "${SRC_DIR}/audio_data_stream.cpp"
    "${SRC_DIR}/audio_data_stream_shared_adapter.cpp"
    "${SRC_DIR}/pull_audio_input_stream.cpp"
    "${SRC_DIR}/push_audio_input_stream.cpp"
    "${SRC_DIR}/pull_audio_output_stream.cpp"
    "${SRC_DIR}/push_audio_output_stream.cpp"
    "${SRC_DIR}/audio_processor_write_to_audio_source_buffer.cpp"
    "${SRC_DIR}/single_to_many_stream_reader_adapter.cpp"
    "${SRC_DIR}/single_to_many_stream_reader.cpp"
    "${SRC_DIR}/create_module_object.cpp"
    "${SRC_DIR}/interactive_microphone.cpp"
    "${SRC_DIR}/internal_audio_codec_adapter.cpp"
    "${SRC_DIR}/pull_audio_input_stream.cpp"
    "${SRC_DIR}/push_audio_input_stream.cpp"
    "${SRC_DIR}/wav_file_pump.cpp"
    "${SRC_DIR}/wav_file_reader.cpp"
    "${SRC_DIR}/wav_file_writer.cpp"
    "${SRC_DIR}/audio_source_wrapper.cpp"
    "${SRC_DIR}/microphone_audio_source_adapter.cpp"
    "${SRC_DIR}/file_audio_source_adapter.cpp"
    "${SRC_DIR}/stream_audio_source_adapter.cpp"
    "${SRC_DIR}/audio_processing_options.cpp"
    )

set(AUDIO_LIBS interfaces common)

if(ANDROID)
    list(APPEND HEADERS_PRIVATE
        "${SRC_DIR}/codec/android_codec_adapter.h"
        )
    list(APPEND SOURCES
        "${SRC_DIR}/codec/android_codec_adapter.cpp"
        )
    list(APPEND AUDIO_LIBS mediandk)
endif()

if(APPLE)
    list(APPEND HEADERS_PRIVATE
        "${SRC_DIR}/codec/apple_codec_adapter.h"
        )
    list(APPEND SOURCES
        "${SRC_DIR}/codec/apple_codec_adapter.cpp"
        )

    FIND_LIBRARY(AUDIO_TOOLBOX AudioToolbox)
    if (NOT AUDIO_TOOLBOX)
        message(FATAL_ERROR "AudioToolbox not found")
    endif()
    list(APPEND AUDIO_LIBS ${AUDIO_TOOLBOX})
endif()

list(APPEND SOURCES
    "${SRC_DIR}/stdafx.cpp"
)

set_source_groups()

add_library(${PROJECT_NAME} STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
        ${SRC_DIR}
    PUBLIC
        ${INC_DIR}
)

add_compile_definitions(USE_SPEECH_ROOT_SITE)

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    # Need C++17 for WinRT API access via cppwinrt.
    set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
    target_compile_definitions(${PROJECT_NAME} PUBLIC _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

add_security_compile_flags(${PROJECT_NAME})

set_target_properties (${PROJECT_NAME} PROPERTIES
    FOLDER core/input
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${AUDIO_LIBS})

