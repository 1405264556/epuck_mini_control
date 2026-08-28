add_library(epuck_core STATIC
    Types.h
    SensorData.h SensorData.cpp
    RobotInstance.h RobotInstance.cpp
    RobotManager.h RobotManager.cpp
    Settings.h Settings.cpp
)

target_link_libraries(epuck_core PUBLIC epuck_util epuck_comm Qt6::Core Qt6::Gui Qt6::Bluetooth Qt6::SerialPort)
target_include_directories(epuck_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
