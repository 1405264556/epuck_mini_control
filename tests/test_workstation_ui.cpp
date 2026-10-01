#include <gtest/gtest.h>
#include <QApplication>
#include <QTest>
#include <QSignalSpy>
#include <QTableWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>
#include <QElapsedTimer>
#include <QDockWidget>
#include <QDir>
#include <QDateTime>
#include <QAbstractItemModelTester>
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "comm/SerialManager.h"
#include "control/MultiRobotCoordinator.h"
#include "control/PathPlanningJob.h"
#include "ui/MainWindow.h"
#include "ui/widgets/CoordinationCanvas.h"
#include "ui/models/LogModel.h"

namespace {
void addFleet(RobotManager &manager, int count) {
    for (int i = 0; i < count; ++i) {
        BLEDeviceInfo info; info.portName = QString("COM%1").arg(70+i); info.friendlyName = QString("e-puck %1").arg(i+1);
        auto *robot = manager.addRobot(info);
        robot->setConnectionState(RobotConnectionState::Connected);
        robot->setPosition({(i%2)*70.0-35, (i/2)*50.0-25}, 0.4*i);
        SensorData data; data.timestamp = QDateTime::currentMSecsSinceEpoch();
        data.proximity.fill(100*(i+1)); data.accelerometer.z = 800;
        data.accelerometerInADC = true; data.mark(SensorData::Proximity);
        data.mark(SensorData::Accelerometer); data.mark(SensorData::Encoders);
        robot->updateSensorData(data);
    }
}
void capture(QWidget &widget, const QString &name) {
    const QString directory = qEnvironmentVariable("EPUCK_UI_CAPTURE_DIR");
    if (directory.isEmpty()) return;
    QDir().mkpath(directory);
    EXPECT_TRUE(widget.grab().save(directory + "/" + name + ".png"));
}
}

TEST(WorkstationUI, FourRobotsCanBeControlledTogetherAndIndividually) {
    RobotManager manager; addFleet(manager, 4);
    SerialManager serial(&manager); MultiRobotCoordinator coordinator(&manager);
    MainWindow window(&manager, &serial, &coordinator, nullptr);
    window.resize(1440, 900); window.show(); window.setControlMode(1);
    QTest::qWait(180);
    auto *table = window.findChild<QTableWidget *>("fleetDriveTable");
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->rowCount(), 4);
    EXPECT_GT(table->width(), 800);
    auto *apply = window.findChild<QPushButton *>("applyFleetCommand");
    ASSERT_NE(apply, nullptr);
    apply->click();
    for (auto *robot : manager.connectedRobots()) EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0.3);
    auto *first = table->cellWidget(0,0)->findChild<QCheckBox *>();
    first->setChecked(false);
    auto *a = manager.robot(table->item(0,1)->data(Qt::UserRole).toString());
    EXPECT_DOUBLE_EQ(a->commandedSpeeds().left, 0);
    auto *b = manager.robot(table->item(1,1)->data(Qt::UserRole).toString());
    EXPECT_DOUBLE_EQ(b->commandedSpeeds().left, 0.3);
    qobject_cast<QSpinBox *>(table->cellWidget(1,3))->setValue(200);
    qobject_cast<QSpinBox *>(table->cellWidget(1,4))->setValue(-100);
    qobject_cast<QPushButton *>(table->cellWidget(1,6))->click();
    EXPECT_DOUBLE_EQ(b->commandedSpeeds().left, 0.2);
    EXPECT_DOUBLE_EQ(b->commandedSpeeds().right, -0.1);
    auto *c = manager.robot(table->item(2,1)->data(Qt::UserRole).toString());
    EXPECT_DOUBLE_EQ(c->commandedSpeeds().left, 0.3);
    manager.setSelectedRobot(b->id());
    EXPECT_EQ(manager.connectedRobots().size(), 4);
    capture(window, "multi-1440x900");
    window.setControlMode(0);
    for (auto *robot : manager.connectedRobots()) EXPECT_DOUBLE_EQ(robot->commandedSpeeds().left, 0);
    QTest::qWait(100);
    capture(window, "single-1440x900");
    window.close();
}

TEST(WorkstationUI, CompactModesRemainUsableAndCanvasRemovalIsSafe) {
    RobotManager manager; addFleet(manager, 4);
    SerialManager serial(&manager); MultiRobotCoordinator coordinator(&manager);
    MainWindow window(&manager, &serial, &coordinator, nullptr);
    window.resize(1024, 720); window.show();
    QTest::qWait(80); capture(window, "single-1024x720");
    window.setControlMode(1);
    QTest::qWait(80); capture(window, "multi-1024x720");
    auto *canvas = window.findChild<CoordinationCanvas *>("coordinationCanvas");
    ASSERT_NE(canvas, nullptr);
    EXPECT_GT(canvas->width(), 200);
    EXPECT_GT(canvas->height(), 150);
    const auto id = manager.allRobots().first()->id();
    manager.removeRobot(id);
    QTest::qWait(30);
    EXPECT_EQ(window.findChild<QTableWidget *>("fleetDriveTable")->rowCount(), 3);
    window.close();
}

TEST(PathPlanningJob, PlansWithoutBlockingTheEventLoop) {
    PathPlanningJob job;
    QSignalSpy spy(&job, &PathPlanningJob::finished);
    job.start({0,0}, {180,180}, {{80,80}}, "builtin", {});
    ASSERT_TRUE(spy.wait(3000));
    EXPECT_TRUE(spy[0][1].toString().isEmpty());
}

TEST(PathPlanningJob, MissingScriptReportsErrorImmediately) {
    PathPlanningJob job;
    QSignalSpy spy(&job, &PathPlanningJob::finished);
    job.start({0,0}, {10,10}, {}, "python", "missing_planner.py");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy[0][1].toString().isEmpty());
}

TEST(WorkstationUI, EightRobotTelemetryBurstKeepsEventLoopResponsive) {
    RobotManager manager; addFleet(manager, 8);
    SerialManager serial(&manager); MultiRobotCoordinator coordinator(&manager);
    MainWindow window(&manager, &serial, &coordinator, nullptr);
    window.resize(1440,900); window.show(); window.setControlMode(1);
    QElapsedTimer timer; timer.start();
    const auto robots = manager.connectedRobots();
    for (int frame = 0; frame < 150; ++frame) {
        for (auto *robot : robots) {
            SensorData data;
            data.timestamp = QDateTime::currentMSecsSinceEpoch() + frame;
            data.proximity.fill(frame % 1000);
            data.mark(SensorData::Proximity);
            robot->updateSensorData(data);
        }
        QCoreApplication::processEvents();
    }
    EXPECT_LT(timer.elapsed(), 3000);
    EXPECT_EQ(window.findChild<QTableWidget *>("fleetDriveTable")->rowCount(), 8);
    window.close();
}

TEST(LogModel, RollingBufferUsesValidModelSignals) {
    LogModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    for (int i = 0; i < 2500; ++i) {
        LogEntry entry; entry.message = QString::number(i);
        model.appendEntry(entry);
    }
    EXPECT_GT(model.rowCount(), 0);
    EXPECT_LE(model.rowCount(), 1000);
    EXPECT_EQ(model.data(model.index(model.rowCount()-1,0), Qt::DisplayRole).toString().right(4), "2499");
}
