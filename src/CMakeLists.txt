# ---- Source sub-libraries ----
add_subdirectory(util)
add_subdirectory(core)
add_subdirectory(comm)
add_subdirectory(control)
add_subdirectory(ui)
add_subdirectory(app)

# ---- Main executable ----
qt_add_executable(epuck_mini_control
    main.cpp
    ../resources/resources.qrc
)

target_link_libraries(epuck_mini_control PRIVATE
    epuck_app
    epuck_ui
    epuck_control
    epuck_comm
    epuck_core
    epuck_util
    Qt6::Core Qt6::Widgets Qt6::SerialPort Qt6::Charts Qt6::Network
)

target_include_directories(epuck_mini_control PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(epuck_mini_control PROPERTIES
    WIN32_EXECUTABLE ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
