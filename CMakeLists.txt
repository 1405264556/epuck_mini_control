cmake_minimum_required(VERSION 3.21)
project(epuck_mini_control VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---- Qt6 ----
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Bluetooth SerialPort Charts Network Test Concurrent
)
qt_standard_project_setup()

# ---- Eigen3 (header-only, optional) ----
# No source file includes Eigen at present; it is linked only when available so
# plugins and future code can rely on it.
find_package(Eigen3 3.4 QUIET)
if(NOT Eigen3_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/eigen/CMakeLists.txt")
    add_subdirectory(third_party/eigen)
endif()

# ---- libusb-1.0 (for DFU) ----
find_package(libusb-1.0 QUIET)

# One source of truth for the version shown in the title bar and About box.
add_compile_definitions(EPUCK_VERSION="${PROJECT_VERSION}")

# ---- Subdirectories ----
add_subdirectory(src)

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_subdirectory(tests)
    else()
        message(STATUS "GTest not found; tests disabled. Install via vcpkg: gtest")
    endif()
endif()
