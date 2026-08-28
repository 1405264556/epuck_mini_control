#pragma once
#include <QDialog>
#include "core/Types.h"
#include "comm/BLEDeviceInfo.h"
class QTableWidget;
class QPushButton;
class QLineEdit;
class QLabel;
class SerialManager;

class ConnectDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectDialog(SerialManager *serialMgr, QWidget *parent = nullptr);
    BLEDeviceInfo selectedDevice() const;
    QString connectionType() const; // always "serial"

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onScanClicked();
    void onDeviceSelected();

private:
    void addDeviceRow(const BLEDeviceInfo &info);
    void startCOMScan();

    SerialManager *m_serialManager;
    QMetaObject::Connection m_deviceDiscoveredConn;
    QMetaObject::Connection m_scanFinishedConn;
    QMetaObject::Connection m_scanProgressConn;
    QTableWidget *m_table;
    QPushButton *m_scanBtn;
    QPushButton *m_manualConnectBtn;
    QLineEdit *m_manualPortEdit;
    QLabel *m_statusLabel;
    QList<BLEDeviceInfo> m_devices;
    BLEDeviceInfo m_selected;
};
