find_package(GTest REQUIRED)

set(TEST_SOURCES
    test_main.cpp
    test_math_utils.cpp
    test_sensor_data.cpp
    test_gatt_protocol.cpp
    test_formation.cpp
    test_strategy_registry.cpp
    test_path_planner.cpp
)

add_executable(epuck_tests ${TEST_SOURCES})

target_link_libraries(epuck_tests PRIVATE
    GTest::gtest GTest::gtest_main
    epuck_core epuck_comm epuck_control epuck_util
    Qt6::Core Qt6::Test
)

target_include_directories(epuck_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GoogleTest)
gtest_discover_tests(epuck_tests)
