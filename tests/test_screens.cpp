// Renders the documentation screenshots. Does nothing unless EPUCK_UI_CAPTURE_DIR
// is set, so a normal test run is not slowed down:
//   EPUCK_UI_CAPTURE_DIR=docs/images QT_QPA_PLATFORM=offscreen epuck_tests --gtest_filter=Screens.*
// The robots shown are simulated; no hardware is involved.
#include <gtest/gtest.h>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include "comm/SerialManager.h"
#include "control/MultiRobotCoordinator.h"
#include "core/RobotInstance.h"
#include "core/RobotManager.h"
#include "core/Settings.h"
#include "ui/MainWindow.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/widgets/CoordinationCanvas.h"
#include "ui/widgets/MotorControlWidget.h"

namespace {
void save(QWidget &widget, const QString &name) {
    const QString directory = qEnvironmentVariable("EPUCK_UI_CAPTURE_DIR");
    QDir().mkpath(directory);
    EXPECT_TRUE(widget.grab().save(directory + "/" + name + ".png"));
}

void feed(RobotInstance *robot, int index, int frame, qint64 now) {
    SensorData data;
    data.timestamp = now;
    for (int i = 0; i < 8; ++i) {
        data.proximity[i] = static_cast<uint16_t>(
            qBound(0.0, 900 + 1600 * std::sin(0.7 * i + index + frame * 0.05), 3900.0));
    }
    data.mark(SensorData::Proximity);
    data.accelerometer = {static_cast<float>(60 * std::sin(frame * 0.2 + index)),
                          static_cast<float>(40 * std::cos(frame * 0.15)),
                          static_cast<float>(800 + 25 * std::sin(frame * 0.3))};
    data.accelerometerInADC = true;
    data.mark(SensorData::Accelerometer);
    data.encoders = {static_cast<int16_t>(frame * (6 + index)), static_cast<int16_t>(frame * (6 + index) + frame * (index % 2 ? 1 : -1))};
    data.mark(SensorData::Encoders);
    robot->updateSensorData(data);
}
}

TEST(Screens, DocumentationCaptures) {
    if (qEnvironmentVariable("EPUCK_UI_CAPTURE_DIR").isEmpty()) GTEST_SKIP() << "capture directory not set";

    QTemporaryDir directory;
    Settings settings(directory.filePath("settings.ini"));
    RobotManager manager;
    SerialManager serial(&manager);
    MultiRobotCoordinator coordinator(&manager);
    const Vec2 starts[4] = {{-70, -35}, {-20, 45}, {55, 30}, {70, -50}};
    for (int i = 0; i < 4; ++i) {
        BLEDeviceInfo info;
        info.portName = QString("COM%1").arg(5 + i);
        info.friendlyName = QString("e-puck Mini %1").arg(i + 1);
        auto *robot = manager.addRobot(info);
        robot->setConnectionState(i == 3 ? RobotConnectionState::Disconnected : RobotConnectionState::Connected);
        robot->setPosition(starts[i], 0.6 * i);
    }
    MainWindow window(&manager, &serial, &coordinator, &settings);
    window.resize(1440, 900);
    window.show();
    QTest::qWait(100);

    auto *canvas = window.findChild<CoordinationCanvas *>("coordinationCanvas");
    ASSERT_NE(canvas, nullptr);
    const qint64 start = QDateTime::currentMSecsSinceEpoch() - 6000;
    for (int frame = 0; frame < 120; ++frame) {
        for (int i = 0; i < 3; ++i) feed(manager.allRobots().at(i), i, frame, start + frame * 50);
    }
    // Stamp the last frame "now" so the freshness indicators read as live.
    for (int i = 0; i < 3; ++i) feed(manager.allRobots().at(i), i, 120, QDateTime::currentMSecsSinceEpoch());
    manager.setSelectedRobot(manager.allRobots().at(0)->id());
    const Vec2 from = manager.allRobots().at(0)->position();
    canvas->showPath(manager.allRobots().at(0)->id(),
                     {from, {from.x + 30, from.y + 5}, {from.x + 60, from.y + 40}, {from.x + 110, from.y + 70}});
    canvas->fitAllRobots();
    QTest::qWait(350);
    save(window, "single-workstation");

    auto *typeSelector = window.findChild<QComboBox *>("sensorTypeSelector");
    ASSERT_NE(typeSelector, nullptr);
    typeSelector->setCurrentIndex(1);
    for (int frame = 121; frame < 160; ++frame) {
        feed(manager.allRobots().at(0), 0, frame, QDateTime::currentMSecsSinceEpoch());
        QTest::qWait(12);
    }
    for (auto *tabs : window.findChildren<QTabWidget *>()) {
        if (tabs->count() == 4 && tabs->tabText(1) == "轮速") tabs->setCurrentIndex(3);
    }
    QTest::qWait(350);
    save(window, "single-path-imu");

    typeSelector->setCurrentIndex(0);
    window.setControlMode(1);
    for (int i = 0; i < 3; ++i) feed(manager.allRobots().at(i), i, 200, QDateTime::currentMSecsSinceEpoch());
    QTest::qWait(400);
    save(window, "multi-workstation");

    SettingsDialog dialog(&settings, &window);
    dialog.show();
    QTest::qWait(100);
    save(dialog, "settings");
    dialog.close();
    window.close();
}

TEST(Screens, EmptyStartupCapture) {
    if (qEnvironmentVariable("EPUCK_UI_CAPTURE_DIR").isEmpty()) GTEST_SKIP() << "capture directory not set";
    RobotManager manager;
    SerialManager serial(&manager);
    MultiRobotCoordinator coordinator(&manager);
    MainWindow window(&manager, &serial, &coordinator, nullptr);
    window.resize(1200, 760);
    window.show();
    QTest::qWait(200);
    save(window, "first-start");
    window.close();
}
