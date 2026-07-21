#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(core_tests)

if(NOT BUILD_CORE_ONLY)
  add_subdirectory(network)
endif()
add_subdirectory(util)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")

set(SOURCES
    "${SRC_DIR}/main.cpp"
    "${SRC_DIR}/guid_tests.cpp"
    "${SRC_DIR}/usp_test_case.cpp"
    "${SRC_DIR}/tls_crl_test_case.cpp"
    "${SRC_DIR}/sr_test_case.cpp"
    "${SRC_DIR}/speech_context_json_test.cpp"
    "${SRC_DIR}/event.cpp"
    "${SRC_DIR}/interface_wrapper_tests.cpp"
    "${SRC_DIR}/create_module_object.cpp"
    "${SRC_DIR}/named_properties_test.cpp"
    "${SRC_DIR}/substrings_matcher_tests.cpp"
    $<$<STREQUAL:${CMAKE_SYSTEM_NAME},WindowsStore>:${TESTS}/CMakeTestSign.pfx>
)

set(EXTERNAL_STATIC_SOURCES)

if (EXTENSION_AUDIO_ENABLED)
  set(SOURCES
    ${SOURCES}
    "${SRC_DIR}/mic_test_case.cpp"
  )
endif()

# A limited number of compilands utilize functionality linked from internal libraries that were
# built with runtime type information disabled. Build these files without RTTI so this usage can
# succeed.
if (MSVC)
  set (_NO_RTTI_FLAG "/GR-")
else()
  set (_NO_RTTI_FLAG "-fno-rtti")
endif()

if (NOT DEFINED MACOSX)
  set_source_files_properties(
    speech_context_json_test.cpp
    named_properties_test.cpp
    create_module_object.cpp
    PROPERTIES COMPILE_FLAGS ${_NO_RTTI_FLAG})
endif()

if (EXTENSION_AUDIO_ENABLED)
  list(APPEND SOURCES
    "${SRC_DIR}/mic_test_case.cpp"
  )
endif()

if (EXTENSION_AUDIOCOMPRESSION_ENABLED)
  list(APPEND SOURCES
    "${SRC_DIR}/compression_codec_test.cpp"
  )
endif()

set_source_groups()
source_group("External Files" FILES ${EXTERNAL_STATIC_SOURCES})

add_executable(${PROJECT_NAME} ${SOURCES} ${EXTERNAL_STATIC_SOURCES})
add_dependencies(${PROJECT_NAME} Catch2)
if(NOT BUILD_CORE_ONLY)
  target_codesign(${PROJECT_NAME})
endif()

if (DEFINED MACOSX)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE FALSE)
endif()

set(PRIVATE_AUDIO_HEADERS
  "${audio_SOURCE_DIR}"
  "${audio_extension_SOURCE_DIR}")
set(PRIVATE_SR_HEADERS "${sr_SOURCE_DIR}")

set(ADDITIONAL_LIBS)
if(WIN32)
  list(APPEND ADDITIONAL_LIBS ole32)
elseif(ANDROID)
  list(APPEND ADDITIONAL_LIBS z)
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CARBON_C_API}"
        "${CARBON_CXX_API}"
        "${CARBON_JSON_PARSER}"
        "${catch2_SOURCE_DIR}"
        "${PRIVATE_AUDIO_HEADERS}"
        "${PRIVATE_SR_HEADERS}"
)

target_link_libraries(${PROJECT_NAME}
    test_utils
    interfaces
    sr
    factory
    common
    audio
    usp
    Catch2
    logging
    shared_c_api
    data
    core_tests_util
    network_pal
    ${ADDITIONAL_LIBS})

if(NOT BUILD_CORE_ONLY)
  target_link_libraries(${PROJECT_NAME}
    core_tests_network
  )
endif()

if (MSVC)
  # Don't warn about missing (some external) symbols
  string(APPEND CMAKE_EXE_LINKER_FLAGS
    " /IGNORE:4099"
  )
endif()

if (NOT WIN32)
    find_package (Threads)
    target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
endif()

set_target_properties (core_tests PROPERTIES
    FOLDER tests/unit
    OUTPUT_NAME core_tests
    COMPILE_DEFINITIONS "PAL_CONFIG_EXPORTAPIS"
)

# If we're using a dynamic openssl, we need to export symbols as this binary is taking the 
# place of the core library in the tests, and it would normally export symbols the dynamic lib depends on.
if(${use_openssl_dynamic})
  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-rdynamic")
endif()

if (EXTENSION_AUDIO_ENABLED)
  target_link_libraries(${PROJECT_NAME} audio_extension)
endif()

if (EXTENSION_CODEC_ENABLED AND STATIC_CODEC_EXTENSION)
  target_link_libraries(${PROJECT_NAME} codec)
endif()

if(EXTENSION_KWS_ENABLED AND STATIC_KWS_EXTENSION)
  target_link_libraries(${PROJECT_NAME} kws_factory)
endif()

if(EXTENSION_AUDIO_ENABLED AND STATIC_AUDIO_EXTENSION)
  target_link_libraries(${PROJECT_NAME} audio_extension)
endif()

if (EXTENSION_RNNT_ENABLED AND STATIC_EMBEDDEDSR_EXTENSION)
  target_link_libraries(${PROJECT_NAME} embedded_sr)
endif()

if (EXTENSION_HYBRID_TTS_ENABLED AND STATIC_HYBRIDTTS_EXTENSION)
  target_link_libraries(${PROJECT_NAME} hybrid_tts)
endif()

if (EXTENSION_MAS_ENABLED AND STATIC_MAS_EXTENSION)
  target_link_libraries(${PROJECT_NAME} mas_extension)
endif()

if (EXTENSION_VAD_ENABLED AND STATIC_VAD_EXTENSION)
  target_link_libraries(${PROJECT_NAME} vad_extension)
endif()
