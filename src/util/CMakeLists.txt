add_library(epuck_util STATIC
    Logger.h Logger.cpp
    MathUtils.h MathUtils.cpp
    WorkerThread.h
)

target_link_libraries(epuck_util PUBLIC Qt6::Core)
target_include_directories(epuck_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

if(Eigen3_FOUND)
    target_link_libraries(epuck_util PUBLIC Eigen3::Eigen)
endif()
