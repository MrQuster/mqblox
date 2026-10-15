cmake_minimum_required(VERSION 3.22)
project(mqblox VERSION 26.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Concurrent
    WebEngineCore
    WebEngineWidgets
    Test
)

add_library(mqblox_core STATIC
    src/core/Types.hpp
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
target_link_libraries(mqblox PRIVATE
    mqblox_core
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
)

enable_testing()
add_executable(tst_mqblox tests/tst_mqblox.cpp)
target_link_libraries(tst_mqblox PRIVATE mqblox_core Qt6::Test)
add_test(NAME tst_mqblox COMMAND tst_mqblox)
