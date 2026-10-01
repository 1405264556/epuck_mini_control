find_package(GTest REQUIRED)

set(TEST_SOURCES
    test_main.cpp
    test_math_utils.cpp
    test_sensor_data.cpp
    test_gatt_protocol.cpp
    test_formation.cpp
    test_strategy_registry.cpp
    test_path_planner.cpp
    test_serial_protocol.cpp
    test_control_runtime.cpp
    test_workstation_ui.cpp
)

add_executable(epuck_tests ${TEST_SOURCES} ../resources/resources.qrc)

target_link_libraries(epuck_tests PRIVATE
    GTest::gtest GTest::gtest_main
    epuck_core epuck_comm epuck_control epuck_util
    epuck_ui epuck_app Qt6::Core Qt6::Test Qt6::Widgets
)

target_include_directories(epuck_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GoogleTest)
get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
execute_process(COMMAND "${QT_QMAKE_EXECUTABLE}" -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE TEST_QT_PLUGIN_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
if(IS_DIRECTORY "${TEST_QT_PLUGIN_DIR}")
    target_compile_definitions(epuck_tests PRIVATE EPUCK_TEST_QT_PLUGIN_DIR="${TEST_QT_PLUGIN_DIR}")
endif()
gtest_discover_tests(epuck_tests DISCOVERY_TIMEOUT 30 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
