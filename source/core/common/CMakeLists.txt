#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(common)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")


set(HEADERS
    "${INC_DIR}/asyncop.h"
    "${INC_DIR}/audio_file_logger.h"
    "${INC_DIR}/audio_format_id_2_name_map.h"
    "${INC_DIR}/audio_pump_delegate_helper.h"
    "${INC_DIR}/audio_source_buffer_data_delegate_impl.h"
    "${INC_DIR}/audio_source_buffer_data_writer_delegate_impl.h"
    "${INC_DIR}/audio_source_buffer_service_provider_helper.h"
    "${INC_DIR}/audio_source_control_adapts_audio_pump_impl.h"
    "${INC_DIR}/audio_source_control_delegate_impl.h"
    "${INC_DIR}/audio_source_control_simple_impl.h"
    "${INC_DIR}/audio_source_file_pump_impl.h"
    "${INC_DIR}/audio_source_init_delegate_impl.h"
    "${INC_DIR}/audio_source_init_not_impl.h"
    "${INC_DIR}/audio_source_microphone_pump_impl.h"
    "${INC_DIR}/audio_source_simple_impl.h"
    "${INC_DIR}/audio_source_stream_pump_impl.h"
    "${INC_DIR}/base64.h"
    "${INC_DIR}/buffer_helpers.h"
    "${INC_DIR}/create_object_helpers.h"
    "${INC_DIR}/codec_helpers.h"
    "${INC_DIR}/data_buffer.h"
    "${INC_DIR}/debug_utils.h"
    "${INC_DIR}/delegate_audio_file_impl.h"
    "${INC_DIR}/delegate_audio_pump_impl.h"
    "${INC_DIR}/dynamic_module.h"
    "${INC_DIR}/error_info.h"
    "${INC_DIR}/exception_helpers.h"
    "${INC_DIR}/exception.h"
    "${INC_DIR}/factory_helpers.h"
    "${INC_DIR}/file_utils.h"
    "${INC_DIR}/function_helpers.h"
    "${INC_DIR}/guid_utils.h"
    "${INC_DIR}/handle_helpers.h"
    "${INC_DIR}/handle_table.h"
    "${INC_DIR}/http_status_codes.h"
    "${INC_DIR}/i_telemetry.h"
    "${INC_DIR}/log_helpers.h"
    "${INC_DIR}/memory_utils.h"
    "${INC_DIR}/metric_message_type.h"
    "${INC_DIR}/mock_controller.h"
    "${INC_DIR}/module_factory.h"
    "${INC_DIR}/named_properties.h"
    "${INC_DIR}/no_op_telemetry.h"
    "${INC_DIR}/object_init_impl.h"
    "${INC_DIR}/object_with_site_init_impl.h"
    "${INC_DIR}/platform.h"
    "${INC_DIR}/property_bag_impl.h"
    "${INC_DIR}/property_id_2_name_map.h"
    "${INC_DIR}/result_helpers.h"
    "${INC_DIR}/service_helpers.h"
    "${INC_DIR}/service_provider_impl.h"
    "${INC_DIR}/shared_ptr_helpers.h"
    "${INC_DIR}/singleton.h"
    "${INC_DIR}/site_helpers.h"
    "${INC_DIR}/spx_namespace.h"
    "${INC_DIR}/spxcore_common.h"
    "${INC_DIR}/string_utils.h"
    "${INC_DIR}/substrings_matcher.h"
    "${INC_DIR}/synthesis_helper.h"
    "${INC_DIR}/thread_service.h"
    "${INC_DIR}/task_helpers.h"
    "${INC_DIR}/time_utils.h"
    "${INC_DIR}/try_catch_helpers.h"
    "${INC_DIR}/interface_wrapper.h"
    "${INC_DIR}/http_platform.h"
    "${INC_DIR}/i_http_platform.h"
    )

set(HEADERS_PRIVATE
    "${SRC_DIR}/stdafx.h"
    )

set(SOURCES
    "${SRC_DIR}/base64.cpp"
    "${SRC_DIR}/audio_file_logger.cpp"
    "${SRC_DIR}/codec_helpers.cpp"
    "${SRC_DIR}/dynamic_module.cpp"
    "${SRC_DIR}/error_info.cpp"
    "${SRC_DIR}/exception.cpp"
    "${SRC_DIR}/extension_site_helpers.cpp"
    "${SRC_DIR}/file_utils.cpp"
    "${SRC_DIR}/guid_utils.cpp"
    "${SRC_DIR}/handle_table.cpp"
    "${SRC_DIR}/memory_utils.cpp"
    "${SRC_DIR}/mock_controller.cpp"
    "${SRC_DIR}/module_factory.cpp"
    "${SRC_DIR}/platform.cpp"
    "${SRC_DIR}/property_bag_impl.cpp"
    "${SRC_DIR}/string_utils.cpp"
    "${SRC_DIR}/substrings_matcher.cpp"
    "${SRC_DIR}/thread_service.cpp"
    "${SRC_DIR}/time_utils.cpp"
    "${SRC_DIR}/http_platform.cpp"
    )

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore" OR CARBONSDK_OS STREQUAL "OSX")
    list(APPEND SOURCES
            "${SRC_DIR}/debug_utils_dummy.cpp"
    )
else()
   list(APPEND SOURCES
           "${SRC_DIR}/debug_utils.cpp"
    )
endif()

configure_file(
    spx_build_information.h.in
    ${COMMON_GENERATED_INCLUDE}/spx_build_information.h
    ESCAPE_QUOTES)

# make this the last source file to avoid link warning LNK4221
list(APPEND SOURCES "${SRC_DIR}/stdafx.cpp")

set_source_groups()

add_library(common STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_include_directories(common
    PRIVATE
        ## TODO: These should come from target_link_libraries
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
    PUBLIC
        ${INC_DIR}
)
    
add_security_compile_flags(common)

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    # Need C++17 for WinRT API access via cppwinrt.
    set_target_properties(common PROPERTIES CXX_STANDARD 17)
    target_compile_definitions(common PUBLIC _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

set_target_properties(common PROPERTIES
    FOLDER core/shared
)

target_compile_definitions(common PUBLIC PAL_CONFIG_EXPORTAPIS)

target_link_libraries(common
    PUBLIC interfaces
    PUBLIC pal
    PUBLIC network_interfaces
)

if(WIN32)
    target_link_libraries(common PRIVATE Dbghelp.lib)
else()
    target_link_libraries(common PUBLIC dl)
endif()

# Adding logcat logging
if(ANDROID)
    target_link_libraries(common PRIVATE log)
endif()
