cmake_minimum_required(VERSION 3.16)

project(VPet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia)

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
    src/perception/vision_encoder.cpp
    src/sensor/screenshot_sensor.cpp
    src/llm/llm_client.cpp
    src/llm/vision_llm_client.cpp
    src/agent/agent_dag_graph.cpp
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
    include/vpet/perception/vision_encoder.h
    include/vpet/sensor/screenshot_sensor.h
    include/vpet/llm/llm_client.h
    include/vpet/llm/vision_llm_client.h
    include/vpet/agent/agent_dag_graph.h
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
