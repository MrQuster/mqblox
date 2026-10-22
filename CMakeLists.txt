cmake_minimum_required(VERSION 3.20)
project(mqblox VERSION 35.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

option(MQBLOX_SANITIZE_ADDRESS "Enable ASan+UBSan" OFF)
option(MQBLOX_SANITIZE_THREAD "Enable TSan" OFF)

if(MQBLOX_SANITIZE_ADDRESS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
elseif(MQBLOX_SANITIZE_THREAD)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent WebEngineCore WebEngineWidgets Test)

add_library(mqblox_core STATIC
    src/core/Types.hpp
    src/core/SettingsRegistry.hpp
    src/core/SecretStore.hpp
    src/core/SecretStore.cpp
    src/core/SettingsManager.hpp
    src/core/SettingsManager.cpp
    src/core/SoberController.hpp
    src/core/SoberController.cpp
    src/net/RobloxApiClient.hpp
    src/net/RobloxApiClient.cpp
    src/render/Ton618GlWidget.hpp
    src/render/Ton618GlWidget.cpp
)
target_include_directories(mqblox_core PUBLIC src)
target_link_libraries(mqblox_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
)

add_executable(mqblox
    src/main.cpp
    src/ui/Widgets.hpp
    src/ui/MainWindow.hpp
    src/ui/MainWindow.cpp
    resources/resources.qrc
)
target_include_directories(mqblox PRIVATE src)
target_link_libraries(mqblox PRIVATE
    mqblox_core
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
)

enable_testing()
add_executable(tst_mqblox tests/tst_mqblox.cpp)
target_include_directories(tst_mqblox PRIVATE src)
target_link_libraries(tst_mqblox PRIVATE mqblox_core Qt6::Test)
add_test(NAME MqBloxCoreTests COMMAND tst_mqblox)
