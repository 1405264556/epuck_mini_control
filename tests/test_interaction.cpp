// Interaction tests that need no serial port: keyboard driving, emergency stop,
// map canvas gestures, the device list and persisted settings.
#include <gtest/gtest.h>
#include <QApplication>
#include <QDateTime>
#include <QComboBox>
#include <QDockWidget>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <cmath>
#include <memory>
#include "comm/SerialManager.h"
#include "control/MultiRobotCoordinator.h"
#include "core/RobotInstance.h"
#include "core/RobotManager.h"
#include "core/Settings.h"
#include "ui/MainWindow.h"
#include "ui/models/LogModel.h"
#include "ui/models/RobotListModel.h"
#include "ui/widgets/CoordinationCanvas.h"
#include "ui/widgets/MotorControlWidget.h"

namespace {
RobotInstance *addRobot(RobotManager &manager, const QString &port, bool connected = true) {
    BLEDeviceInfo info;
    info.portName = port;
    info.friendlyName = "e-puck " + port;
    auto *robot = manager.addRobot(info);
    if (connected) {
        robot->setConnectionState(RobotConnectionState::Connected);
        SensorData data;
        data.timestamp = QDateTime::currentMSecsSinceEpoch();
        data.mark(SensorData::Proximity);
        data.mark(SensorData::Encoders);
        robot->updateSensorData(data);
    }
    return robot;
}

struct Station {
    RobotManager manager;
    SerialManager serial{&manager};
    MultiRobotCoordinator coordinator{&manager};
    std::unique_ptr<MainWindow> window;

    explicit Station(int robots, Settings *settings = nullptr) {
        for (int i = 0; i < robots; ++i) addRobot(manager, QString("COM%1").arg(80 + i));
        window = std::make_unique<MainWindow>(&manager, &serial, &coordinator, settings);
        window->resize(1280, 820);
        window->show();
        window->activateWindow();
        (void)QTest::qWaitForWindowActive(window.get(), 2000);
        QTest::qWait(50);
    }

    // Keys go to whatever has focus, like real input; the application-wide filter
    // in MainWindow decides whether they drive.
    QWidget *keyTarget() const {
        return QApplication::focusWidget() ? QApplication::focusWidget() : window.get();
    }
    void press(Qt::Key key) { QTest::keyPress(keyTarget(), key); }
    void release(Qt::Key key) { QTest::keyRelease(keyTarget(), key); }
};
}

TEST(DriveMix, ForwardTurnAndDiagonalStayWithinLimit) {
    const auto forward = MotorControlWidget::mixDrive(1, 0, 500);
    EXPECT_DOUBLE_EQ(forward.left, 0.5);
    EXPECT_DOUBLE_EQ(forward.right, 0.5);

    const auto spin = MotorControlWidget::mixDrive(0, 1, 500);
    EXPECT_GT(spin.left, 0);
    EXPECT_DOUBLE_EQ(spin.left, -spin.right);
    EXPECT_LT(spin.left, 0.5) << "turning in place is deliberately gentler than full speed";

    // Forward + right: an arc. The outer wheel sits at the limit and the inner one
    // is slower but still forward, i.e. the curvature is kept instead of clipped.
    const auto arc = MotorControlWidget::mixDrive(1, 1, 500);
    EXPECT_DOUBLE_EQ(arc.left, 0.5);
    EXPECT_GT(arc.right, 0);
    EXPECT_LT(arc.right, arc.left);

    const auto reverse = MotorControlWidget::mixDrive(-1, 0, 1000);
    EXPECT_DOUBLE_EQ(reverse.left, -1.0);
    EXPECT_DOUBLE_EQ(reverse.right, -1.0);

    const auto invalid = MotorControlWidget::mixDrive(std::nan(""), 1, 500);
    EXPECT_DOUBLE_EQ(invalid.left, 0);
    EXPECT_DOUBLE_EQ(invalid.right, 0);
}

TEST(Joystick, DragDrivesAndReleaseStops) {
    JoystickWidget joystick;
    joystick.resize(240, 240);
    joystick.show();
    QSignalSpy moved(&joystick, &JoystickWidget::moved);
    QSignalSpy released(&joystick, &JoystickWidget::released);

    const QPoint center(120, 120);
    QTest::mousePress(&joystick, Qt::LeftButton, {}, center + QPoint(2, 1));
    ASSERT_EQ(moved.count(), 1);
    EXPECT_DOUBLE_EQ(moved[0][0].toDouble(), 0) << "inside the dead zone";
    EXPECT_DOUBLE_EQ(moved[0][1].toDouble(), 0);

    QTest::mouseMove(&joystick, center + QPoint(0, -200));   // far past the rim, straight up
    ASSERT_EQ(moved.count(), 2);
    EXPECT_NEAR(moved[1][0].toDouble(), 1.0, 1e-6);
    EXPECT_NEAR(moved[1][1].toDouble(), 0.0, 1e-6);

    QTest::mouseRelease(&joystick, Qt::LeftButton, {}, center + QPoint(0, -200));
    EXPECT_EQ(released.count(), 1);
    EXPECT_FALSE(joystick.isDragging());
}

TEST(KeyboardDrive, KeysDriveSelectedRobotAndReleaseStops) {
    Station station(2);
    auto *robot = station.manager.selectedRobot();
    ASSERT_NE(robot, nullptr);
    auto *other = station.manager.allRobots().at(1);
    ASSERT_NE(other, robot);

    station.press(Qt::Key_W);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0.5);   // default limit 500
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().right, 0.5);
    EXPECT_DOUBLE_EQ(other->commandedSpeeds().left, 0) << "only the selected robot moves";

    station.press(Qt::Key_D);                                // W + D: arc to the right
    EXPECT_GT(robot->commandedSpeeds().left, robot->commandedSpeeds().right);
    EXPECT_GT(robot->commandedSpeeds().right, 0);

    station.release(Qt::Key_D);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().right, 0.5);
    station.release(Qt::Key_W);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().right, 0);

    station.press(Qt::Key_S);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, -0.5);
    station.press(Qt::Key_Space);                            // space stops even with S held
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
    station.release(Qt::Key_S);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
}

TEST(KeyboardDrive, SpeedLimitScalesTheCommand) {
    Station station(1);
    auto *limit = station.window->findChild<QSpinBox *>("speedLimitSpin");
    ASSERT_NE(limit, nullptr);
    limit->setValue(200);
    station.window->setFocus();
    station.press(Qt::Key_W);
    EXPECT_DOUBLE_EQ(station.manager.selectedRobot()->commandedSpeeds().left, 0.2);
    station.release(Qt::Key_W);
}

TEST(KeyboardDrive, IgnoredWhileTypingAndWhenDisabled) {
    Station station(1);
    auto *robot = station.manager.selectedRobot();
    auto *limit = station.window->findChild<QSpinBox *>("speedLimitSpin");
    ASSERT_NE(limit, nullptr);

    limit->setFocus();
    ASSERT_EQ(QApplication::focusWidget(), limit);
    station.press(Qt::Key_W);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0) << "typing in a field must not drive";
    station.release(Qt::Key_W);

    station.window->setFocus();
    station.window->setKeyboardDriveEnabled(false);
    station.press(Qt::Key_W);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
    station.release(Qt::Key_W);

    station.window->setKeyboardDriveEnabled(true);
    station.press(Qt::Key_W);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0.5);
    // Moving focus into a field while a key is held releases the drive.
    limit->setFocus();
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
}

TEST(KeyboardDrive, LosingWindowFocusStops) {
    Station station(1);
    auto *robot = station.manager.selectedRobot();
    station.press(Qt::Key_W);
    ASSERT_DOUBLE_EQ(robot->commandedSpeeds().left, 0.5);
    QEvent deactivate(QEvent::WindowDeactivate);
    QApplication::sendEvent(station.window.get(), &deactivate);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
    // The key-up arriving later (or never) must not restart anything.
    station.release(Qt::Key_W);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
}

TEST(KeyboardDrive, ReleaseWhileAPopupIsOpenStillStops) {
    Station station(1);
    auto *robot = station.manager.selectedRobot();
    station.press(Qt::Key_W);
    ASSERT_DOUBLE_EQ(robot->commandedSpeeds().left, 0.5);

    // A context menu or combo list opens without deactivating the main window.
    QMenu menu(station.window.get());
    menu.addAction("item");
    menu.popup(station.window->mapToGlobal(QPoint(300, 300)));
    QTest::qWait(50);
    ASSERT_NE(QApplication::activePopupWidget(), nullptr);
    QTest::keyRelease(&menu, Qt::Key_W);
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0) << "key released under a popup stayed held";
    menu.close();
    QTest::qWait(250);                 // several heartbeats
    EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
}

TEST(KeyboardDrive, RobotJoiningTheGroupMidHoldIsNotLeftRunning) {
    Station station(2);
    station.window->setControlMode(1);
    QTest::qWait(50);
    auto *late = addRobot(station.manager, "COM99", false);
    QTest::qWait(50);
    station.window->setFocus();
    station.press(Qt::Key_W);
    // The third robot comes online while W is held, then the command changes.
    late->setConnectionState(RobotConnectionState::Connected);
    QTest::qWait(150);
    station.press(Qt::Key_D);
    station.release(Qt::Key_D);
    station.release(Qt::Key_W);
    QTest::qWait(250);
    for (auto *robot : station.manager.allRobots()) {
        EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0) << robot->id().toStdString();
        EXPECT_DOUBLE_EQ(robot->commandedSpeeds().right, 0);
    }
}

TEST(KeyboardDrive, DrivesTheSyncGroupInFleetMode) {
    Station station(3);
    station.window->setControlMode(1);
    QTest::qWait(50);
    station.window->setFocus();
    station.press(Qt::Key_W);
    for (auto *robot : station.manager.connectedRobots()) {
        EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0.3);   // sync-group speed 300
        EXPECT_DOUBLE_EQ(robot->commandedSpeeds().right, 0.3);
    }
    station.release(Qt::Key_W);
    for (auto *robot : station.manager.connectedRobots()) {
        EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
    }
}

TEST(EmergencyStop, ButtonStopsManualAndCoordinatedMotion) {
    Station station(2);
    auto *button = station.window->findChild<QToolButton *>("emergencyStop");
    ASSERT_NE(button, nullptr);
    EXPECT_TRUE(button->isVisible());

    station.press(Qt::Key_W);
    ASSERT_GT(station.manager.selectedRobot()->commandedSpeeds().left, 0);
    button->click();
    for (auto *robot : station.manager.allRobots()) EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
    station.release(Qt::Key_W);

    QList<RobotId> ids;
    for (auto *robot : station.manager.allRobots()) ids.append(robot->id());
    station.coordinator.startFormation(FormationShape::Line, ids, 20, {50, 50});
    ASSERT_TRUE(station.coordinator.isActive());
    station.press(Qt::Key_Escape);
    EXPECT_FALSE(station.coordinator.isActive());
}

TEST(MapCanvas, ClickSelectsWithoutMovingAndDragMovesPose) {
    Station station(2);
    auto *canvas = station.window->findChild<CoordinationCanvas *>("coordinationCanvas");
    ASSERT_NE(canvas, nullptr);
    auto *first = station.manager.allRobots().at(0);
    auto *second = station.manager.allRobots().at(1);
    first->setPosition({-40, 0}, 0);
    second->setPosition({40, 0}, 0);
    canvas->fitAllRobots();
    QTest::qWait(30);
    station.manager.setSelectedRobot(first->id());

    const QPoint onSecond = canvas->mapFromScene(QPointF(40, 0));
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, {}, onSecond);
    QTest::mouseMove(canvas->viewport(), onSecond + QPoint(1, 1));   // hand tremor, under threshold
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, {}, onSecond + QPoint(1, 1));
    EXPECT_EQ(station.manager.selectedRobotId(), second->id());
    EXPECT_DOUBLE_EQ(second->position().x, 40) << "selecting a robot must not move it";
    EXPECT_DOUBLE_EQ(second->position().y, 0);

    QTest::mousePress(canvas->viewport(), Qt::LeftButton, {}, onSecond);
    QTest::mouseMove(canvas->viewport(), onSecond + QPoint(0, -30));
    QTest::mouseMove(canvas->viewport(), onSecond + QPoint(0, -60));
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, {}, onSecond + QPoint(0, -60));
    EXPECT_GT(second->position().y, 5) << "a deliberate drag sets the map pose";

    // While an algorithm runs it owns the pose: dragging is refused.
    const Vec2 before = second->position();
    canvas->setRobotDragEnabled(false);
    const QPoint now = canvas->mapFromScene(QPointF(before.x, -before.y));
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, {}, now);
    QTest::mouseMove(canvas->viewport(), now + QPoint(50, 0));
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, {}, now + QPoint(50, 0));
    EXPECT_DOUBLE_EQ(second->position().x, before.x);
}

TEST(SensorPanel, RawTableKeepsScrollPositionAcrossRefreshes) {
    Station station(1);
    auto *typeSelector = station.window->findChild<QComboBox *>("sensorTypeSelector");
    ASSERT_NE(typeSelector, nullptr);
    typeSelector->setCurrentIndex(5);   // raw data and freshness
    QTest::qWait(400);

    QTableWidget *table = nullptr;
    for (auto *candidate : station.window->findChildren<QTableWidget *>()) {
        if (candidate->isVisible() && candidate->columnCount() == 3 && candidate->rowCount() > 15) table = candidate;
    }
    ASSERT_NE(table, nullptr);
    table->resize(table->width(), 150);
    QTest::qWait(50);
    auto *bar = table->verticalScrollBar();
    ASSERT_GT(bar->maximum(), 0);
    bar->setValue(bar->maximum());
    const int scrolled = bar->value();
    const int rows = table->rowCount();
    QTest::qWait(900);                  // several refresh cycles
    EXPECT_EQ(bar->value(), scrolled) << "refresh reset the scroll position";
    EXPECT_EQ(table->rowCount(), rows);
}

TEST(DeviceList, ShowsStateAndReconnectProgress) {
    RobotManager manager;
    auto *robot = addRobot(manager, "COM90", false);
    RobotListModel model(&manager);
    const auto index = model.index(0, 0);
    EXPECT_EQ(model.data(index, RobotListModel::PortRole).toString(), "COM90");
    EXPECT_EQ(model.data(index, RobotListModel::DetailRole).toString(), "未连接");

    robot->setConnectionState(RobotConnectionState::Connecting);
    model.setNote(robot->id(), "重连中 2/3…");
    EXPECT_EQ(model.data(index, RobotListModel::DetailRole).toString(), "重连中 2/3…");

    robot->setConnectionState(RobotConnectionState::Connected);
    EXPECT_EQ(model.data(index, RobotListModel::DetailRole).toString(), "已连接")
        << "a settled state replaces the transient note";
}

TEST(SettingsStore, RemembersDevicesAndDrivingPreferences) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString file = directory.filePath("settings.ini");
    {
        Settings settings(file);
        EXPECT_TRUE(settings.knownDevices().isEmpty());
        EXPECT_TRUE(settings.autoReconnect());
        EXPECT_EQ(settings.speedLimit(), 500);
        settings.setKnownDevices({{"COM7", "机器人 A", 115200, true}, {"COM9", "", 57600, false}});
        settings.setSpeedLimit(5000);       // clamped
        settings.setReconnectAttempts(5);
        settings.setLastControlMode(1);
    }
    Settings reopened(file);
    const auto devices = reopened.knownDevices();
    ASSERT_EQ(devices.size(), 2);
    EXPECT_EQ(devices[0].portName, "COM7");
    EXPECT_EQ(devices[0].name, "机器人 A");
    EXPECT_TRUE(devices[0].wasConnected);
    EXPECT_EQ(devices[1].baudRate, 57600);
    EXPECT_FALSE(devices[1].wasConnected);
    EXPECT_EQ(reopened.speedLimit(), 1000);
    EXPECT_EQ(reopened.reconnectAttempts(), 5);
    EXPECT_EQ(reopened.lastControlMode(), 1);
}

TEST(SettingsStore, WindowRestoresDevicesModeAndSpeedLimit) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    Settings settings(directory.filePath("settings.ini"));
    settings.setKnownDevices({{"COM31", "实验室 1 号", 115200, false}});
    settings.setLastControlMode(1);
    settings.setSpeedLimit(350);
    settings.setKeyboardDriveEnabled(false);

    Station station(0, &settings);
    ASSERT_EQ(station.manager.robotCount(), 1);
    auto *robot = station.manager.allRobots().first();
    EXPECT_EQ(robot->deviceInfo().portName, "COM31");
    EXPECT_EQ(robot->name(), "实验室 1 号");
    EXPECT_EQ(robot->state(), RobotConnectionState::Disconnected);
    EXPECT_EQ(station.window->controlMode(), 1);
    EXPECT_FALSE(station.window->keyboardDriveEnabled());
    EXPECT_EQ(station.window->findChild<QSpinBox *>("speedLimitSpin")->value(), 350);

    station.window->setControlMode(0);
    station.window->close();
    EXPECT_EQ(settings.lastControlMode(), 0);
    ASSERT_EQ(settings.knownDevices().size(), 1);
}

TEST(LogModel, ShowsTimeAndColoursWarnings) {
    LogModel model;
    LogEntry entry;
    entry.timestamp = QDateTime(QDate(2026, 1, 2), QTime(3, 4, 5, 678));
    entry.category = "serial";
    entry.level = QtWarningMsg;
    entry.message = "link lost";
    model.appendEntry(entry);
    EXPECT_EQ(model.data(model.index(0, 0), Qt::DisplayRole).toString(), "03:04:05.678 [serial] link lost");
    EXPECT_TRUE(model.data(model.index(0, 0), Qt::ForegroundRole).isValid());
}
