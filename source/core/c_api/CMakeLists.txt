#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(c_api)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")

set(HEADERS)

set(HEADERS_PRIVATE
    "${SRC_DIR}/stdafx.h"
    "${SRC_DIR}/common.h"
    "${SRC_DIR}/event_helpers.h"
    "${SRC_DIR}/binding_callback_storage.h"
    "${SRC_DIR}/binding_helpers.h"
    )

set(SOURCES
    "${SRC_DIR}/event_helpers.cpp"
    "${SRC_DIR}/binding_helpers.cpp"
    "${SRC_DIR}/speechapi_c_audio_config.cpp"
    "${SRC_DIR}/speechapi_c_audio_processing_options.cpp"
    "${SRC_DIR}/speechapi_c_audio_stream_format.cpp"
    "${SRC_DIR}/speechapi_c_audio_stream.cpp"
    "${SRC_DIR}/speechapi_c_auto_detect_source_lang_config.cpp"
    "${SRC_DIR}/speechapi_c_connection.cpp"
    "${SRC_DIR}/speechapi_c_conversation_transcription_result.cpp"
    "${SRC_DIR}/speechapi_c_meeting.cpp"
    "${SRC_DIR}/speechapi_c_meeting_transcription_result.cpp"
    "${SRC_DIR}/speechapi_c_dialog_service_config.cpp"
    "${SRC_DIR}/speechapi_c_dialog_service_connector.cpp"
    "${SRC_DIR}/speechapi_c_embedded_speech_config.cpp"
    "${SRC_DIR}/speechapi_c_factory.cpp"
    "${SRC_DIR}/speechapi_c_grammar.cpp"
    "${SRC_DIR}/speechapi_c_hybrid_speech_config.cpp"
    "${SRC_DIR}/speechapi_c_keyword_recognition_model.cpp"
    "${SRC_DIR}/speechapi_c_operations.cpp"
    "${SRC_DIR}/speechapi_c_participant.cpp"
    "${SRC_DIR}/speechapi_c_pronunciation_assessment_config.cpp"
    "${SRC_DIR}/speechapi_c_recognizer.cpp"
    "${SRC_DIR}/speechapi_c_result.cpp"
    "${SRC_DIR}/speechapi_c_session.cpp"
    "${SRC_DIR}/speechapi_c_source_lang_config.cpp"
    "${SRC_DIR}/speechapi_c_speech_config.cpp"
    "${SRC_DIR}/speechapi_c_speech_recognition_model.cpp"
    "${SRC_DIR}/speechapi_c_speech_translation_config.cpp"
    "${SRC_DIR}/speechapi_c_speech_translation_model.cpp"
    "${SRC_DIR}/speechapi_c_synthesis_request.cpp"
    "${SRC_DIR}/speechapi_c_synthesizer.cpp"
    "${SRC_DIR}/speechapi_c_translation_recognizer.cpp"
    "${SRC_DIR}/speechapi_c_translation_result.cpp"
    "${SRC_DIR}/speechapi_c_user.cpp"
    "${SRC_DIR}/stdafx.cpp"
    )

set_source_groups()

source_group("Speech C API headers" FILES ${SPEECH_C_API_HEADERS})

add_library(c_api OBJECT
    ${SPEECH_C_API_HEADERS}
    ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES}
    )

target_include_directories(c_api
    PRIVATE
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
)

target_link_libraries(c_api PRIVATE interfaces common factory logging)

add_security_compile_flags(${PROJECT_NAME})

set_target_properties (c_api PROPERTIES
    FOLDER core
    COMPILE_DEFINITIONS "AZAC_CONFIG_EXPORTAPIS"
)
