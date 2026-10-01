#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QTest>
#include <QDateTime>
#include "comm/MotorCommandScheduler.h"
#include "control/MultiRobotCoordinator.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"

TEST(MotorScheduler, CoalescesInputAndHonorsCommonDeadline) {
    MotorCommandScheduler a, b;
    QSignalSpy first(&a, &MotorCommandScheduler::commandReady), second(&b, &MotorCommandScheduler::commandReady);
    const auto due = MotorCommandScheduler::nowMs()+35;
    a.schedule("old", due, 1); a.schedule("latest", due+20, 2);
    b.schedule("peer", due, 2);
    ASSERT_TRUE(first.wait(300));
    if (second.isEmpty()) ASSERT_TRUE(second.wait(300));
    EXPECT_EQ(first.count(), 1);
    EXPECT_EQ(first[0][0].toByteArray(), "latest");
    EXPECT_GE(first[0][2].toLongLong(), due);
    EXPECT_LE(std::abs(first[0][2].toLongLong()-second[0][2].toLongLong()), 30);
}

TEST(MotorScheduler, StopReplacesPendingMotionAndRejectsOlderSequence) {
    MotorCommandScheduler scheduler;
    QSignalSpy spy(&scheduler, &MotorCommandScheduler::commandReady);
    scheduler.schedule("move", MotorCommandScheduler::nowMs()+100, 1);
    scheduler.schedule("stop", MotorCommandScheduler::nowMs(), 2);
    scheduler.schedule("stale", MotorCommandScheduler::nowMs(), 1);
    ASSERT_TRUE(spy.wait(100));
    EXPECT_EQ(spy[0][0].toByteArray(), "stop");
    QTest::qWait(120);
    EXPECT_EQ(spy.count(), 1);
}

TEST(Coordinator, ManualControlStopsAlgorithmAndKeepsIndependentSpeeds) {
    RobotManager manager;
    BLEDeviceInfo info; info.portName = "COM91";
    auto *a = manager.addRobot(info); a->setConnectionState(RobotConnectionState::Connected);
    info.portName = "COM92";
    auto *b = manager.addRobot(info); b->setConnectionState(RobotConnectionState::Connected);
    MultiRobotCoordinator coordinator(&manager);
    coordinator.startFormation(FormationShape::Line, {a->id(), b->id()}, 20, {0,0});
    coordinator.manualDrive({{a->id(), {0.2,0.3}}, {b->id(), {-0.4,0.1}}});
    EXPECT_FALSE(coordinator.isActive());
    EXPECT_DOUBLE_EQ(a->commandedSpeeds().left, 0.2);
    EXPECT_DOUBLE_EQ(b->commandedSpeeds().left, -0.4);
    manager.setSelectedRobot(b->id());
    EXPECT_EQ(manager.connectedRobots().size(), 2);
    EXPECT_DOUBLE_EQ(a->commandedSpeeds().left, 0.2);
}

TEST(Coordinator, MissingFeedbackPausesAndDoesNotFabricatePose) {
    RobotManager manager;
    BLEDeviceInfo info; info.portName = "COM93";
    auto *robot = manager.addRobot(info); robot->setConnectionState(RobotConnectionState::Connected);
    MultiRobotCoordinator coordinator(&manager);
    coordinator.startFormation(FormationShape::Line, {robot->id()}, 20, {50,50});
    QTest::qWait(100);
    EXPECT_TRUE(coordinator.isPaused());
    EXPECT_DOUBLE_EQ(robot->position().x, 0);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
    coordinator.stopAll();
}
