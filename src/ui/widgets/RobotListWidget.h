#pragma once

#include <QWidget>
#include "core/Types.h"

class QListView;
class QPushButton;
class QProgressBar;
class RobotManager;
class SerialManager;
class RobotListModel;
class RobotItemDelegate;

class RobotListWidget : public QWidget {
    Q_OBJECT
public:
    explicit RobotListWidget(RobotManager *robotMgr, SerialManager *serialMgr,
                              QWidget *parent = nullptr);

signals:
    void connectRequested(const RobotId &id);
    void disconnectRequested(const RobotId &id);
    void connectAllRequested(const QList<RobotId> &ids);
    void disconnectAllRequested(const QList<RobotId> &ids);
    void currentRobotChanged(const RobotId &id);

private slots:
    void onScanClicked();
    void onConnectClicked();
    void onDisconnectClicked();
    void onSelectionChanged();
    void refreshButtons();

private:
    RobotManager *m_robotManager;
    SerialManager *m_serialManager;
    QListView *m_listView;
    RobotListModel *m_model;
    RobotItemDelegate *m_delegate;
    QPushButton *m_scanBtn;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
    QPushButton *m_connectAllBtn;
    QPushButton *m_disconnectAllBtn;
    QProgressBar *m_scanProgress;
    RobotId m_selectedId;
};
