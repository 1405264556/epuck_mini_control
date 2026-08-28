#pragma once
#include <QDialog>
#include "core/Types.h"
class QProgressBar;
class QLabel;
class QLineEdit;
class QPushButton;
class QListWidget;
class RobotManager;
class DFUProgrammer;
class OTAProgrammer;

class FlashDialog : public QDialog {
    Q_OBJECT
public:
    explicit FlashDialog(RobotManager *robotMgr, DFUProgrammer *dfu, OTAProgrammer *ota,
                         QWidget *parent = nullptr);
private:
    void refreshRobotTargets();
    void chooseFirmware();
    void startFlash();

    RobotManager *m_robotManager;
    DFUProgrammer *m_dfu;
    OTAProgrammer *m_ota;
    QProgressBar *m_progress;
    QLabel *m_status;
    QLineEdit *m_firmwarePath;
    QListWidget *m_targetList;
};
