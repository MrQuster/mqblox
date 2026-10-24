cmake_minimum_required(VERSION 3.24)
project(mqblox VERSION 36.6.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    OpenGL
    OpenGLWidgets
    Svg
    Concurrent
    Test
)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

file(MAKE_DIRECTORY
    ${CMAKE_BINARY_DIR}/CMakeFiles/mqblox_core.dir/mqblox_core_autogen
    ${CMAKE_BINARY_DIR}/CMakeFiles/mqblox_core.dir/src/core
    ${CMAKE_BINARY_DIR}/CMakeFiles/mqblox.dir/mqblox_autogen
    ${CMAKE_BINARY_DIR}/CMakeFiles/mqblox.dir/mqblox_autogen/3YJK5W5UP7
    ${CMAKE_BINARY_DIR}/CMakeFiles/mqblox.dir/src/core
    ${CMAKE_BINARY_DIR}/CMakeFiles/mqblox.dir/src/ui
    ${CMAKE_BINARY_DIR}/CMakeFiles/mqblox.dir/src/render
    ${CMAKE_BINARY_DIR}/CMakeFiles/tst_mqblox.dir/tst_mqblox_autogen
    ${CMAKE_BINARY_DIR}/CMakeFiles/tst_mqblox.dir/tests
)

add_library(mqblox_core STATIC
    src/core/Types.hpp
    src/core/SettingsRegistry.hpp
    src/core/SettingsManager.hpp
    src/core/SettingsManager.cpp
    src/core/DarlingController.hpp
    src/core/DarlingController.cpp
)

target_include_directories(mqblox_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mqblox_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Concurrent
    Threads::Threads
)

add_executable(mqblox
    src/main.cpp
    src/render/Ton618GlWidget.hpp
    src/render/Ton618GlWidget.cpp
    src/ui/Widgets.hpp
    src/ui/MainWindow.hpp
    src/ui/MainWindow.cpp
    resources/resources.qrc
)

target_include_directories(mqblox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mqblox PRIVATE
    mqblox_core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Svg
    OpenGL::GL
)


foreach(tgt mqblox_core mqblox)
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic -pipe)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${tgt} PRIVATE
            $<$<CONFIG:Release>:-O3 -fdata-sections -ffunction-sections>
        )
        target_link_options(${tgt} PRIVATE
            $<$<CONFIG:Release>:-Wl,--gc-sections>
        )
    endif()
endforeach()

enable_testing()
add_executable(tst_mqblox tests/tst_mqblox.cpp)
target_link_libraries(tst_mqblox PRIVATE mqblox_core Qt6::Test)
target_compile_options(tst_mqblox PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME tst_mqblox COMMAND tst_mqblox)

include(GNUInstallDirs)
install(TARGETS mqblox RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/mqblox.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(DIRECTORY resources/icons/hicolor/ DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor)
install(DIRECTORY shims/ DESTINATION ${CMAKE_INSTALL_DATADIR}/mqblox/shims
        PATTERN "build" EXCLUDE
        PATTERN "*.o" EXCLUDE
        PATTERN "*.dylib" EXCLUDE
        PATTERN "build_debug_shim.sh" EXCLUDE)
install(PROGRAMS shims/build_debug_shim.sh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mqblox/shims)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME})
