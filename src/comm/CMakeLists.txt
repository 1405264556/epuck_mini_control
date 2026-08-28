add_library(epuck_comm STATIC
    IDeviceComm.h
    BLEDeviceInfo.h
    EpuckGATTProtocol.h EpuckGATTProtocol.cpp
    EpuckSerComProtocol.h EpuckSerComProtocol.cpp
    SerialPortWorker.h SerialPortWorker.cpp
    SerialManager.h SerialManager.cpp
    DFUProgrammer.h DFUProgrammer.cpp
    OTAProgrammer.h OTAProgrammer.cpp
)

target_link_libraries(epuck_comm PUBLIC epuck_core epuck_util Qt6::Core Qt6::Bluetooth Qt6::SerialPort)

if(libusb-1.0_FOUND)
    target_link_libraries(epuck_comm PUBLIC libusb-1.0)
    target_compile_definitions(epuck_comm PUBLIC HAS_LIBUSB)
endif()

target_include_directories(epuck_comm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
