#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(interfaces)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${INC_DIR}/ispxinterfaces.h"
    "${INC_DIR}/interfaces/aggregates.h"
    "${INC_DIR}/interfaces/audio.h"
    "${INC_DIR}/interfaces/audio_output.h"
    "${INC_DIR}/interfaces/base.h"
    "${INC_DIR}/interfaces/containers.h"
    "${INC_DIR}/interfaces/conversation.h"
    "${INC_DIR}/interfaces/meeting.h"
    "${INC_DIR}/interfaces/data.h"
    "${INC_DIR}/interfaces/enum_helpers.h"
    "${INC_DIR}/interfaces/errors.h"
    "${INC_DIR}/interfaces/event_args.h"
    "${INC_DIR}/interfaces/generic_site.h"
    "${INC_DIR}/interfaces/http_method.h"
    "${INC_DIR}/interfaces/http_event_args.h"
    "${INC_DIR}/interfaces/i_http_endpoint_info.h"
    "${INC_DIR}/interfaces/ispx_http_error_handler.h"
    "${INC_DIR}/interfaces/ispx_http_request.h"
    "${INC_DIR}/interfaces/ispx_http_response.h"
    "${INC_DIR}/interfaces/web_socket.h"
    "${INC_DIR}/interfaces/i_web_socket_init.h"
    "${INC_DIR}/interfaces/i_web_socket_message.h"
    "${INC_DIR}/interfaces/i_web_socket_state.h"
    "${INC_DIR}/interfaces/ispx_init_from_properties.h"
    "${INC_DIR}/interfaces/ispx_http_transport_factory.h"
    "${INC_DIR}/interfaces/ispx_reco_engine_adapter_token_provider.h"
    "${INC_DIR}/interfaces/ispx_telemetry_base.h"
    "${INC_DIR}/interfaces/ispx_telemetry_store.h"
    "${INC_DIR}/interfaces/ispx_usp_connection.h"
    "${INC_DIR}/interfaces/keyword.h"
    "${INC_DIR}/interfaces/named_properties.h"
    "${INC_DIR}/interfaces/notify_me.h"
    "${INC_DIR}/interfaces/object_factory.h"
    "${INC_DIR}/interfaces/object_init.h"
    "${INC_DIR}/interfaces/object_with_site.h"
    "${INC_DIR}/interfaces/proxy_server_info.h"
    "${INC_DIR}/interfaces/recognizers.h"
    "${INC_DIR}/interfaces/results.h"
    "${INC_DIR}/interfaces/read_write_buffer.h"
    "${INC_DIR}/interfaces/read_write_buffer_init.h"
    "${INC_DIR}/interfaces/service_provider.h"
    "${INC_DIR}/interfaces/types.h"
    "${INC_DIR}/interfaces/thread_service.h"
    "${INC_DIR}/interfaces/utils.h"
)

add_library(${PROJECT_NAME} INTERFACE ${HEADERS})

target_include_directories(${PROJECT_NAME}
    INTERFACE
        ${INC_DIR}
)
target_link_libraries(${PROJECT_NAME} INTERFACE types util)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER core/shared)
