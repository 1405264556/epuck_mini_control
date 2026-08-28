#pragma once
#include <QWidget>
#include "core/Types.h"
class QTabWidget;
class QComboBox;
class QStackedWidget;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class RobotManager;
class MultiRobotCoordinator;
class MotorControlWidget;
class LEDControlWidget;
class MultiRobotDriveWidget;

class RobotControlPanel : public QWidget {
    Q_OBJECT
public:
    explicit RobotControlPanel(RobotManager *robotMgr, MultiRobotCoordinator *coordinator,
                               QWidget *parent = nullptr);

public slots:
    void setGoalFromCanvas(const Vec2 &pos);
    void setVirtualObstacles(const QVector<Vec2> &obstacles);

signals:
    void goalPickRequested();
    void obstacleEditRequested();
    void canvasClearRequested();
    void pathPlanned(const RobotId &id, const QVector<Vec2> &path);

private slots:
    void onSelectionChanged(const RobotId &id);
    void refreshRobotSummary();
    void planSingleRobotPath();
    void startSingleRobotPath();
    void requestGoalPick();
    void browsePlannerScript();
    void pausePathFollowing();
    void resumePathFollowing();
    void returnSingleRobotOrigin();
    void clearCanvas();
    void startFormationControl();
    void stopCoordination();
private:
    QWidget *createSingleRobotPage();
    QWidget *createMultiRobotPage();
    QList<RobotId> connectedRobotIds() const;
    QVector<Vec2> planWithBuiltInAStar(const Vec2 &start, const Vec2 &goal) const;
    QVector<Vec2> planWithExternalScript(const Vec2 &start, const Vec2 &goal) const;

    RobotManager *m_robotManager;
    MultiRobotCoordinator *m_coordinator;
    QComboBox *m_modeCombo;
    QStackedWidget *m_modeStack;
    QTabWidget *m_singleTabs;
    MotorControlWidget *m_motorWidget;
    LEDControlWidget *m_ledWidget;
    QLabel *m_singleSummary;
    QLabel *m_multiSummary;
    QLabel *m_pathStatus;
    QComboBox *m_algorithmCombo;
    QLineEdit *m_scriptPathEdit;
    QDoubleSpinBox *m_goalXSpin;
    QDoubleSpinBox *m_goalYSpin;
    QPushButton *m_startPathBtn;
    QComboBox *m_formationShapeCombo;
    QComboBox *m_coordinationModeCombo;
    QDoubleSpinBox *m_spacingSpin;
    QDoubleSpinBox *m_targetXSpin;
    QDoubleSpinBox *m_targetYSpin;
    MultiRobotDriveWidget *m_multiDriveWidget;
    RobotId m_currentId;
    QVector<Vec2> m_lastPlannedPath;
    QVector<Vec2> m_virtualObstacles;
};
