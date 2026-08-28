add_library(epuck_app STATIC
    Application.h Application.cpp
)

target_link_libraries(epuck_app PUBLIC epuck_core epuck_comm epuck_util Qt6::Core Qt6::Widgets)
target_include_directories(epuck_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
