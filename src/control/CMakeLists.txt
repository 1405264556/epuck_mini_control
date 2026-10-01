add_library(epuck_control STATIC
    IController.h
    IMultiRobotStrategy.h
    MultiRobotStrategyRegistry.h MultiRobotStrategyRegistry.cpp
    FormationController.h FormationController.cpp
    FlockingController.h FlockingController.cpp
    PathPlanner.h PathPlanner.cpp
    PathPlanningJob.h PathPlanningJob.cpp
    MultiRobotCoordinator.h MultiRobotCoordinator.cpp
    CollisionAvoidance.h CollisionAvoidance.cpp
)

target_link_libraries(epuck_control PUBLIC epuck_core epuck_util Qt6::Core Qt6::Concurrent)

if(Eigen3_FOUND)
    target_link_libraries(epuck_control PUBLIC Eigen3::Eigen)
endif()

target_include_directories(epuck_control PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
