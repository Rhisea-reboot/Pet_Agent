cmake_minimum_required(VERSION 3.16)

project(VPet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia)

include(CTest)

set(VPET_SOURCES
    src/main.cpp
    src/main_window.cpp
    src/common_types.cpp
    src/animation_frame.cpp
    src/animation_segment.cpp
    src/animation_clip.cpp
    src/animation_resource_manager.cpp
    src/pet_state_machine.cpp
    src/pet_controller.cpp
    src/bubble_message.cpp
    src/hit_region.cpp
    src/say_dialog.cpp
    src/tts_client.cpp
    src/tts_audio_player.cpp
    src/tts_server_manager.cpp
    src/chat_bubble_window.cpp
    src/splash_window.cpp
    src/perception/frame_buffer.cpp
    src/perception/perception_pipeline.cpp
    src/perception/vision_encoder.cpp
    src/sensor/screenshot_sensor.cpp
    src/speech/voice_input_manager.cpp
    src/llm/llm_client.cpp
    src/llm/vision_llm_client.cpp
    src/web/web_search_client.cpp
    src/web/web_search_tool.cpp
    src/web/web_research_engine.cpp
    src/agent/agent_context.cpp
    src/agent/agent_dag_graph.cpp
    src/agent/emotion_rewrite_node.cpp
    src/agent/proactive_topic_node.cpp
    src/agent/web_research_node.cpp
    src/agent/invocation_queue_policy.cpp
    src/agent/agent_output_policy.cpp
    src/agent/agent_node_registry.cpp
    src/agent/agent_graph_executor.cpp
    src/agent/agent_async_bridge.cpp
    src/agent/agent_runtime.cpp
)

set(VPET_HEADERS
    include/vpet/common_types.h
    include/vpet/animation_frame.h
    include/vpet/animation_segment.h
    include/vpet/animation_clip.h
    include/vpet/animation_resource_manager.h
    include/vpet/pet_state_machine.h
    include/vpet/pet_controller.h
    include/vpet/bubble_message.h
    include/vpet/hit_region.h
    include/vpet/say_dialog.h
    include/vpet/tts_client.h
    include/vpet/tts_audio_player.h
    include/vpet/tts_server_manager.h
    include/vpet/chat_bubble_window.h
    include/vpet/splash_window.h
    include/vpet/perception/frame_buffer.h
    include/vpet/perception/perception_pipeline.h
    include/vpet/perception/vision_encoder.h
    include/vpet/sensor/screenshot_sensor.h
    include/vpet/speech/voice_input_manager.h
    include/vpet/llm/llm_client.h
    include/vpet/llm/vision_llm_client.h
    include/vpet/web/web_search_client.h
    include/vpet/web/web_search_tool.h
    include/vpet/web/web_research_engine.h
    include/vpet/agent/agent_context.h
    include/vpet/agent/agent_context_keys.h
    include/vpet/agent/agent_dag_graph.h
    include/vpet/agent/emotion_rewrite_node.h
    include/vpet/agent/proactive_topic_node.h
    include/vpet/agent/web_research_node.h
    include/vpet/agent/invocation_queue_policy.h
    include/vpet/agent/agent_output_policy.h
    include/vpet/agent/agent_node_registry.h
    include/vpet/agent/agent_graph_executor.h
    include/vpet/agent/agent_async_bridge.h
    include/vpet/agent/agent_runtime.h
    src/main_window.h
)

add_executable(${PROJECT_NAME} ${VPET_SOURCES} ${VPET_HEADERS})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Multimedia
)

if(WIN32)
    set_property(TARGET ${PROJECT_NAME} PROPERTY WIN32_EXECUTABLE TRUE)
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(agent_dag_graph_tests
        tests/agent_dag_graph_test.cpp
        src/agent/agent_dag_graph.cpp
        include/vpet/agent/agent_dag_graph.h
    )

    target_include_directories(agent_dag_graph_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(agent_dag_graph_tests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    set_property(TARGET agent_dag_graph_tests PROPERTY AUTOMOC_COMPILER_PREDEFINES OFF)

    add_test(NAME agent_dag_graph_tests COMMAND agent_dag_graph_tests)

    add_executable(agent_runtime_scheduler_tests
        tests/agent_runtime_scheduler_test.cpp
        src/agent/agent_context.cpp
        src/agent/agent_dag_graph.cpp
        src/agent/emotion_rewrite_node.cpp
        src/agent/proactive_topic_node.cpp
        src/agent/web_research_node.cpp
        src/agent/invocation_queue_policy.cpp
        src/agent/agent_output_policy.cpp
        src/agent/agent_node_registry.cpp
        src/agent/agent_graph_executor.cpp
        src/agent/agent_async_bridge.cpp
        src/agent/agent_runtime.cpp
        src/web/web_search_client.cpp
        src/web/web_search_tool.cpp
        src/web/web_research_engine.cpp
        src/llm/llm_client.cpp
        src/llm/vision_llm_client.cpp
        include/vpet/agent/agent_context.h
        include/vpet/agent/agent_context_keys.h
        include/vpet/agent/agent_dag_graph.h
        include/vpet/agent/emotion_rewrite_node.h
        include/vpet/agent/proactive_topic_node.h
        include/vpet/agent/web_research_node.h
        include/vpet/agent/invocation_queue_policy.h
        include/vpet/agent/agent_output_policy.h
        include/vpet/agent/agent_node_registry.h
        include/vpet/agent/agent_graph_executor.h
        include/vpet/agent/agent_async_bridge.h
        include/vpet/agent/agent_runtime.h
        include/vpet/web/web_search_client.h
        include/vpet/web/web_search_tool.h
        include/vpet/web/web_research_engine.h
        include/vpet/llm/llm_client.h
        include/vpet/llm/vision_llm_client.h
    )

    target_include_directories(agent_runtime_scheduler_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(agent_runtime_scheduler_tests
        PRIVATE
            Qt6::Core
            Qt6::Network
            Qt6::Test
    )

    set_property(TARGET agent_runtime_scheduler_tests PROPERTY AUTOMOC_COMPILER_PREDEFINES OFF)

    add_test(NAME agent_runtime_scheduler_tests COMMAND agent_runtime_scheduler_tests)

    add_executable(application_integration_tests
        tests/application_integration_test.cpp
        src/agent/agent_context.cpp
        src/agent/agent_dag_graph.cpp
        src/agent/emotion_rewrite_node.cpp
        src/agent/proactive_topic_node.cpp
        src/agent/web_research_node.cpp
        src/agent/invocation_queue_policy.cpp
        src/agent/agent_output_policy.cpp
        src/agent/agent_node_registry.cpp
        src/agent/agent_graph_executor.cpp
        src/agent/agent_async_bridge.cpp
        src/agent/agent_runtime.cpp
        src/web/web_search_client.cpp
        src/web/web_search_tool.cpp
        src/web/web_research_engine.cpp
        src/llm/llm_client.cpp
        src/llm/vision_llm_client.cpp
        include/vpet/agent/agent_context.h
        include/vpet/agent/agent_context_keys.h
        include/vpet/agent/agent_dag_graph.h
        include/vpet/agent/emotion_rewrite_node.h
        include/vpet/agent/proactive_topic_node.h
        include/vpet/agent/web_research_node.h
        include/vpet/agent/invocation_queue_policy.h
        include/vpet/agent/agent_output_policy.h
        include/vpet/agent/agent_node_registry.h
        include/vpet/agent/agent_graph_executor.h
        include/vpet/agent/agent_async_bridge.h
        include/vpet/agent/agent_runtime.h
        include/vpet/web/web_search_client.h
        include/vpet/web/web_search_tool.h
        include/vpet/web/web_research_engine.h
        include/vpet/llm/llm_client.h
        include/vpet/llm/vision_llm_client.h
    )

    target_include_directories(application_integration_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(application_integration_tests
        PRIVATE
            Qt6::Core
            Qt6::Network
            Qt6::Test
    )

    set_property(TARGET application_integration_tests PROPERTY AUTOMOC_COMPILER_PREDEFINES OFF)

    add_test(NAME application_integration_tests COMMAND application_integration_tests)

    add_executable(web_search_client_tests
        tests/web_search_client_test.cpp
        src/web/web_search_client.cpp
        src/web/web_search_tool.cpp
        include/vpet/web/web_search_client.h
        include/vpet/web/web_search_tool.h
    )

    target_include_directories(web_search_client_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(web_search_client_tests
        PRIVATE
            Qt6::Core
            Qt6::Network
            Qt6::Test
    )

    set_property(TARGET web_search_client_tests PROPERTY AUTOMOC_COMPILER_PREDEFINES OFF)

    add_test(NAME web_search_client_tests COMMAND web_search_client_tests)

    add_executable(web_research_engine_tests
        tests/web_research_engine_test.cpp
        src/web/web_search_client.cpp
        src/web/web_search_tool.cpp
        src/web/web_research_engine.cpp
        include/vpet/web/web_search_client.h
        include/vpet/web/web_search_tool.h
        include/vpet/web/web_research_engine.h
    )

    target_include_directories(web_research_engine_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(web_research_engine_tests
        PRIVATE
            Qt6::Core
            Qt6::Network
            Qt6::Test
    )

    set_property(TARGET web_research_engine_tests PROPERTY AUTOMOC_COMPILER_PREDEFINES OFF)

    add_test(NAME web_research_engine_tests COMMAND web_research_engine_tests)

    add_executable(web_search_daemon_integration_tests
        tests/web_search_daemon_integration_test.cpp
        src/web/web_search_client.cpp
        src/web/web_search_tool.cpp
        include/vpet/web/web_search_client.h
        include/vpet/web/web_search_tool.h
    )

    target_include_directories(web_search_daemon_integration_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(web_search_daemon_integration_tests
        PRIVATE
            Qt6::Core
            Qt6::Network
            Qt6::Test
    )

    set_property(TARGET web_search_daemon_integration_tests PROPERTY AUTOMOC_COMPILER_PREDEFINES OFF)

    add_test(NAME web_search_daemon_integration_tests COMMAND web_search_daemon_integration_tests)
endif()
