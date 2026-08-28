#include <gtest/gtest.h>
#include "control/FormationController.h"
#include "core/RobotInstance.h"

TEST(FormationController, DefaultNotRunning) {
    FormationController fc;
    EXPECT_FALSE(fc.isRunning());
    EXPECT_EQ(fc.name(), "Formation");
}

TEST(FormationController, ShapeSetters) {
    FormationController fc;
    fc.setShape(FormationShape::Circle);
    fc.setSpacing(20.0);
    // These should not crash
    SUCCEED();
}

TEST(FormationController, ComputesFormationAroundTargetWithoutResettingRobotPose) {
    BLEDeviceInfo firstInfo;
    firstInfo.portName = "COM1";
    firstInfo.friendlyName = "Robot 1";
    BLEDeviceInfo secondInfo;
    secondInfo.portName = "COM2";
    secondInfo.friendlyName = "Robot 2";
    RobotInstance first(firstInfo);
    RobotInstance second(secondInfo);
    first.setPosition({5.0, 7.0}, 0.0);
    second.setPosition({15.0, 17.0}, 0.0);

    FormationController controller;
    controller.setShape(FormationShape::Line);
    controller.setSpacing(20.0);
    controller.setTargetPosition({100.0, 50.0});
    controller.start({&first, &second});

    const auto desired = controller.desiredPositions();
    ASSERT_EQ(desired.size(), 2);
    EXPECT_DOUBLE_EQ(desired.value(first.id()).x, 100.0);
    EXPECT_DOUBLE_EQ(desired.value(first.id()).y, 40.0);
    EXPECT_DOUBLE_EQ(desired.value(second.id()).x, 100.0);
    EXPECT_DOUBLE_EQ(desired.value(second.id()).y, 60.0);
    EXPECT_DOUBLE_EQ(first.position().x, 5.0);
    EXPECT_DOUBLE_EQ(second.position().x, 15.0);
}
