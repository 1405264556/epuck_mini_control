// End-to-end link tests: the real SerialManager and SerialPortWorker threads talk
// to FakeEpuck over a pseudo terminal. POSIX only.
#include <gtest/gtest.h>
#include <QtGlobal>

#ifdef Q_OS_UNIX
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>
#include <functional>
#include <memory>
#include "FakeEpuck.h"
#include "comm/BLEDeviceInfo.h"
#include "comm/SerialManager.h"
#include "core/RobotInstance.h"
#include "core/RobotManager.h"
#include "control/MultiRobotCoordinator.h"
#include "core/Settings.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QTemporaryDir>

Q_DECLARE_METATYPE(SensorData)

namespace {
struct Link {
    RobotManager manager;
    SerialManager serial{&manager};
    FakeEpuck fake;
    RobotId id;

    Link() {
        qRegisterMetaType<SensorData>("SensorData");
        qRegisterMetaType<RobotId>("RobotId");
        BLEDeviceInfo info;
        info.portName = fake.portPath();
        id = info.deriveId();
    }

    bool connectAndWait(int timeoutMs = 4000) {
        QSignalSpy connected(&serial, &SerialManager::robotConnected);
        if (!serial.connectToPort(fake.portPath(), id)) return false;
        return connected.wait(timeoutMs);
    }

    RobotInstance *robot() { return manager.robot(id); }
};

bool waitFor(const std::function<bool()> &condition, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (condition()) return true;
        QTest::qWait(20);
    }
    return condition();
}
}

TEST(SerialLink, ConnectsAndStreamsTelemetry) {
    Link link;
    ASSERT_TRUE(link.fake.isValid());
    QElapsedTimer timer;
    timer.start();
    ASSERT_TRUE(link.connectAndWait());
    EXPECT_LT(timer.elapsed(), 2000) << "handshake should take well under two seconds";
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Connected);

    QSignalSpy frames(&link.serial, &SerialManager::sensorDataReceived);
    QTest::qWait(1500);
    // 60 ms polling is about 16 Hz; allow generous scheduling slack.
    EXPECT_GE(frames.count(), 15) << "telemetry rate too low";
    const auto &data = link.robot()->latestSensorData();
    EXPECT_TRUE(data.has(SensorData::Proximity));
    EXPECT_TRUE(data.has(SensorData::Encoders));
    EXPECT_TRUE(data.has(SensorData::Accelerometer));
    EXPECT_TRUE(data.has(SensorData::Microphone));
    EXPECT_EQ(data.selectorPosition, 5);
    EXPECT_FLOAT_EQ(data.accelerometer.z, 800);
}

TEST(SerialLink, MotorCommandReachesDeviceAndWatchdogStopsIt) {
    Link link;
    ASSERT_TRUE(link.connectAndWait());
    link.fake.clearMotorCommands();

    QElapsedTimer timer;
    timer.start();
    link.robot()->setMotorSpeeds(0.3, -0.2);
    ASSERT_TRUE(waitFor([&]() { return !link.fake.motorCommands().isEmpty(); }, 500));
    EXPECT_LT(timer.elapsed(), 150) << "command latency";
    EXPECT_EQ(link.fake.lastMotorCommand(), qMakePair(300, -200));

    // No heartbeat follows: the port thread must stop the wheels on its own.
    ASSERT_TRUE(waitFor([&]() { return link.fake.lastMotorCommand() == qMakePair(0, 0); }, 2000));
    EXPECT_GE(timer.elapsed(), 900);
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Connected);
}

TEST(SerialLink, StopOvertakesAQueuedCommand) {
    Link link;
    ASSERT_TRUE(link.connectAndWait());
    link.fake.clearMotorCommands();
    link.robot()->setMotorSpeeds(0.5, 0.5);
    link.robot()->stop();
    ASSERT_TRUE(waitFor([&]() { return !link.fake.motorCommands().isEmpty(); }, 500));
    QTest::qWait(100);
    EXPECT_EQ(link.fake.lastMotorCommand(), qMakePair(0, 0));
}

TEST(SerialLink, EncodersDriveOdometry) {
    Link link;
    link.fake.setEncoderStep(20, 20);
    ASSERT_TRUE(link.connectAndWait());
    QTest::qWait(800);
    EXPECT_GT(link.robot()->position().x, 0.5) << "straight motion should advance +X";
    EXPECT_NEAR(link.robot()->position().y, 0.0, 0.05);
}

TEST(SerialLink, DeadLinkIsDetectedAndReconnected) {
    Link link;
    link.serial.setAutoReconnect(true, 3);
    ASSERT_TRUE(link.connectAndWait());
    QTest::qWait(300);   // let telemetry flow first: only an established link is "lost"

    QSignalSpy reconnecting(&link.serial, &SerialManager::robotReconnecting);
    QSignalSpy status(&link.serial, &SerialManager::telemetryStatus);
    QElapsedTimer timer;
    timer.start();
    link.fake.setMuted(true);
    ASSERT_TRUE(reconnecting.wait(9000)) << "a silent link was never declared lost";
    EXPECT_GT(timer.elapsed(), 3000) << "a brief stall must not drop the link";
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Connecting);
    EXPECT_TRUE(link.serial.isReconnectPending(link.id));
    EXPECT_EQ(reconnecting[0][1].toInt(), 1);

    // The timeout is announced once per streak, not once per missed poll.
    int timeoutMessages = 0;
    for (const auto &entry : status) timeoutMessages += entry[1].toString().contains("超时") ? 1 : 0;
    EXPECT_EQ(timeoutMessages, 1);

    link.fake.setMuted(false);
    QSignalSpy connected(&link.serial, &SerialManager::robotConnected);
    ASSERT_TRUE(connected.wait(6000)) << "link did not come back";
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Connected);
    EXPECT_FALSE(link.serial.isReconnectPending(link.id));
    QSignalSpy frames(&link.serial, &SerialManager::sensorDataReceived);
    EXPECT_TRUE(frames.wait(1000)) << "telemetry did not resume after reconnect";
}

TEST(SerialLink, ReconnectGivesUpAfterConfiguredAttempts) {
    Link link;
    link.serial.setAutoReconnect(true, 1);
    ASSERT_TRUE(link.connectAndWait());
    QTest::qWait(300);
    QSignalSpy errors(&link.serial, &SerialManager::errorOccurred);
    QSignalSpy disconnected(&link.serial, &SerialManager::robotDisconnected);
    link.fake.setMuted(true);
    ASSERT_TRUE(disconnected.wait(15000));
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Error);
    ASSERT_FALSE(errors.isEmpty());
    EXPECT_TRUE(errors.last()[1].toString().contains("自动重连"));
    EXPECT_FALSE(link.serial.isReconnectPending(link.id));
}

TEST(SerialLink, UserDisconnectCancelsPendingReconnect) {
    Link link;
    link.serial.setAutoReconnect(true, 3);
    ASSERT_TRUE(link.connectAndWait());
    QTest::qWait(300);
    QSignalSpy reconnecting(&link.serial, &SerialManager::robotReconnecting);
    link.fake.setMuted(true);
    ASSERT_TRUE(reconnecting.wait(9000));

    link.serial.disconnectRobot(link.id);
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Disconnected);
    EXPECT_FALSE(link.serial.isReconnectPending(link.id));
    link.fake.setMuted(false);
    QSignalSpy connected(&link.serial, &SerialManager::robotConnected);
    EXPECT_FALSE(connected.wait(2500)) << "a cancelled reconnect still fired";
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Disconnected);
}

TEST(SerialLink, TurningAutoReconnectOffCancelsAPendingRetry) {
    Link link;
    link.serial.setAutoReconnect(true, 3);
    ASSERT_TRUE(link.connectAndWait());
    QTest::qWait(300);
    QSignalSpy reconnecting(&link.serial, &SerialManager::robotReconnecting);
    link.fake.setMuted(true);
    ASSERT_TRUE(reconnecting.wait(9000));
    link.serial.setAutoReconnect(false);
    EXPECT_FALSE(link.serial.isReconnectPending(link.id));
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Error);
    link.fake.setMuted(false);
    QSignalSpy connected(&link.serial, &SerialManager::robotConnected);
    EXPECT_FALSE(connected.wait(2500));
}

TEST(SerialLink, AutoReconnectCanBeTurnedOff) {
    Link link;
    link.serial.setAutoReconnect(false);
    ASSERT_TRUE(link.connectAndWait());
    QTest::qWait(300);
    QSignalSpy reconnecting(&link.serial, &SerialManager::robotReconnecting);
    QSignalSpy disconnected(&link.serial, &SerialManager::robotDisconnected);
    link.fake.setMuted(true);
    ASSERT_TRUE(disconnected.wait(9000));
    EXPECT_TRUE(reconnecting.isEmpty());
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Error);
}

TEST(SerialLink, DeviceWithoutTelemetryStaysDrivable) {
    Link link;
    link.fake.setTelemetryEnabled(false);
    QSignalSpy status(&link.serial, &SerialManager::telemetryStatus);
    QSignalSpy disconnected(&link.serial, &SerialManager::robotDisconnected);
    ASSERT_TRUE(link.connectAndWait());
    // Long enough for the slow-poll fallback to engage.
    ASSERT_TRUE(waitFor([&]() {
        for (const auto &entry : status) if (entry[1].toString().contains("降低轮询")) return true;
        return false;
    }, 8000));
    EXPECT_TRUE(disconnected.isEmpty()) << "a telemetry-less device must not be dropped";
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Connected);

    link.fake.clearMotorCommands();
    link.robot()->setMotorSpeeds(0.1, 0.1);
    ASSERT_TRUE(waitFor([&]() { return !link.fake.motorCommands().isEmpty(); }, 500));
    EXPECT_EQ(link.fake.lastMotorCommand(), qMakePair(100, 100));

    // Once slowed down, the device is polled about once a second, not sixteen times.
    const int before = link.fake.telemetryPolls();
    QTest::qWait(2000);
    EXPECT_LE(link.fake.telemetryPolls() - before, 4);
}

TEST(SerialLink, ConnectingToAPortThatNeverAnswersFailsCleanly) {
    Link link;
    link.fake.setMuted(true);
    QSignalSpy errors(&link.serial, &SerialManager::errorOccurred);
    QSignalSpy reconnecting(&link.serial, &SerialManager::robotReconnecting);
    ASSERT_TRUE(link.serial.connectToPort(link.fake.portPath(), link.id));
    ASSERT_TRUE(errors.wait(4000));
    EXPECT_TRUE(errors[0][1].toString().contains("握手"));
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Error);
    EXPECT_TRUE(reconnecting.isEmpty()) << "a link that never came up is not retried";
}

TEST(SerialLink, UnpluggedDeviceEndsInErrorWithoutHanging) {
    Link link;
    link.serial.setAutoReconnect(true, 1);
    ASSERT_TRUE(link.connectAndWait());
    QTest::qWait(300);
    QSignalSpy disconnected(&link.serial, &SerialManager::robotDisconnected);
    link.fake.unplug();
    ASSERT_TRUE(disconnected.wait(15000));
    EXPECT_EQ(link.robot()->state(), RobotConnectionState::Error);
}

TEST(SerialLink, ShutdownIsBoundedWithLiveLinks) {
    FakeEpuck first, second;
    RobotManager manager;
    auto serial = std::make_unique<SerialManager>(&manager);
    QSignalSpy connected(serial.get(), &SerialManager::robotConnected);
    // Robot ids are derived from the port name, so use the same derivation here.
    BLEDeviceInfo a, b;
    a.portName = first.portPath();
    b.portName = second.portPath();
    ASSERT_TRUE(serial->connectToPort(a.portName, a.deriveId()));
    ASSERT_TRUE(serial->connectToPort(b.portName, b.deriveId()));
    ASSERT_TRUE(waitFor([&]() { return connected.count() == 2; }, 4000));
    ASSERT_NE(manager.robot(a.deriveId()), nullptr);
    manager.robot(a.deriveId())->setMotorSpeeds(0.4, 0.4);
    QTest::qWait(100);

    QElapsedTimer timer;
    timer.start();
    serial.reset();
    EXPECT_LT(timer.elapsed(), 2500);
    // Closing sends a final stop before the port goes away.
    QTest::qWait(50);
    EXPECT_EQ(first.lastMotorCommand(), qMakePair(0, 0));
}

TEST(SerialLink, CoordinationStopsWhenALinkDrops) {
    Link link;
    link.fake.setEncoderStep(1, 1);
    link.serial.setAutoReconnect(true, 3);
    ASSERT_TRUE(link.connectAndWait());
    QTest::qWait(300);
    MultiRobotCoordinator coordinator(&link.manager);
    coordinator.startPathFollowing({{link.id, {{0, 0}, {100, 0}}}});
    ASSERT_TRUE(coordinator.isActive());
    QSignalSpy reconnecting(&link.serial, &SerialManager::robotReconnecting);
    link.fake.setMuted(true);
    ASSERT_TRUE(reconnecting.wait(9000));
    EXPECT_FALSE(coordinator.isActive() && !coordinator.isPaused())
        << "an algorithm must not keep running on a dropped link";
    EXPECT_DOUBLE_EQ(link.robot()->commandedSpeeds().left, 0);
}
// The whole chain in one go: remembered device -> auto-connect at startup ->
// keyboard -> heartbeat over the real serial thread -> device, and a clean stop on exit.
TEST(SerialLink, StartupAutoConnectThenKeyboardDrivesTheDevice) {
    qRegisterMetaType<SensorData>("SensorData");
    qRegisterMetaType<RobotId>("RobotId");
    FakeEpuck fake;
    ASSERT_TRUE(fake.isValid());
    QTemporaryDir directory;
    Settings settings(directory.filePath("settings.ini"));
    settings.setKnownDevices({{fake.portPath(), "实验台 1 号", 115200, true}});
    settings.setAutoConnectOnStartup(true);
    settings.setSpeedLimit(400);

    RobotManager manager;
    SerialManager serial(&manager);
    MultiRobotCoordinator coordinator(&manager);
    QSignalSpy connected(&serial, &SerialManager::robotConnected);
    MainWindow window(&manager, &serial, &coordinator, &settings);
    window.resize(1280, 820);
    window.show();
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window, 2000);

    ASSERT_TRUE(connected.wait(5000)) << "remembered device was not connected at startup";
    auto *robot = manager.selectedRobot();
    ASSERT_NE(robot, nullptr);
    EXPECT_EQ(robot->name(), "实验台 1 号");
    QTest::qWait(200);
    fake.clearMotorCommands();

    window.setFocus();
    QTest::keyPress(&window, Qt::Key_W);
    ASSERT_TRUE(waitFor([&]() { return fake.lastMotorCommand() == qMakePair(400, 400); }, 500));
    // Held for longer than the 1 s watchdog: the 100 ms heartbeat must keep it alive.
    fake.clearMotorCommands();
    QTest::qWait(1600);
    EXPECT_GE(fake.motorCommands().size(), 10) << "heartbeat not reaching the device";
    for (const auto &command : fake.motorCommands()) EXPECT_EQ(command, qMakePair(400, 400));

    QTest::keyRelease(&window, Qt::Key_W);
    ASSERT_TRUE(waitFor([&]() { return fake.lastMotorCommand() == qMakePair(0, 0); }, 300));

    QTest::keyPress(&window, Qt::Key_A);
    ASSERT_TRUE(waitFor([&]() { return fake.lastMotorCommand().first < 0 && fake.lastMotorCommand().second > 0; }, 500));
    window.close();     // exit while turning
    ASSERT_TRUE(waitFor([&]() { return fake.lastMotorCommand() == qMakePair(0, 0); }, 500))
        << "closing the window must stop the robot";
    ASSERT_EQ(settings.knownDevices().size(), 1);
    EXPECT_TRUE(settings.knownDevices().first().wasConnected);
}
#endif // Q_OS_UNIX
