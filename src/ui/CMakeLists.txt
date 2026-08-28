add_library(epuck_ui STATIC
    MainWindow.h MainWindow.cpp
    widgets/RobotListWidget.h widgets/RobotListWidget.cpp
    widgets/SensorDisplayWidget.h widgets/SensorDisplayWidget.cpp
    widgets/ProximityVisualizer.h widgets/ProximityVisualizer.cpp
    widgets/IMUVisualizer.h widgets/IMUVisualizer.cpp
    widgets/ToFVisualizer.h widgets/ToFVisualizer.cpp
    widgets/MicrophoneVisualizer.h widgets/MicrophoneVisualizer.cpp
    widgets/CameraViewWidget.h widgets/CameraViewWidget.cpp
    widgets/RobotControlPanel.h widgets/RobotControlPanel.cpp
    widgets/MultiRobotDriveWidget.h widgets/MultiRobotDriveWidget.cpp
    widgets/MotorControlWidget.h widgets/MotorControlWidget.cpp
    widgets/LEDControlWidget.h widgets/LEDControlWidget.cpp
    widgets/CoordinationCanvas.h widgets/CoordinationCanvas.cpp
    models/RobotListModel.h models/RobotListModel.cpp
    models/SensorDataModel.h models/SensorDataModel.cpp
    models/LogModel.h models/LogModel.cpp
    delegates/RobotItemDelegate.h delegates/RobotItemDelegate.cpp
    dialogs/ConnectDialog.h dialogs/ConnectDialog.cpp
    dialogs/FlashDialog.h dialogs/FlashDialog.cpp
    dialogs/FormationDialog.h dialogs/FormationDialog.cpp
    dialogs/SettingsDialog.h dialogs/SettingsDialog.cpp
    dialogs/AboutDialog.h dialogs/AboutDialog.cpp
)

target_link_libraries(epuck_ui PUBLIC epuck_core epuck_comm epuck_control epuck_util
    Qt6::Core Qt6::Widgets Qt6::Charts)
target_include_directories(epuck_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
