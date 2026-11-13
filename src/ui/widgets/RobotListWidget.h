#pragma once

#include <QWidget>
#include "core/Types.h"

class QLabel;
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
    void removeRequested(const RobotId &id);
    void manualConnectRequested();
    void currentRobotChanged(const RobotId &id);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onScanClicked();
    void onSelectionChanged();
    void refreshButtons();

private:
    void toggleConnection(const RobotId &id);
    void showContextMenu(const QPoint &pos);
    void refreshEmptyHint();
    void selectRow(const RobotId &id);

    RobotManager *m_robotManager;
    SerialManager *m_serialManager;
    QListView *m_listView;
    QLabel *m_emptyHint;
    RobotListModel *m_model;
    RobotItemDelegate *m_delegate;
    QPushButton *m_scanBtn;
    QPushButton *m_connectBtn;
    QPushButton *m_connectAllBtn;
    QPushButton *m_disconnectAllBtn;
    QProgressBar *m_scanProgress;
    RobotId m_selectedId;
    bool m_scannedOnce = false;
};
