#include "ConnectDialog.h"
#include "comm/SerialManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QShowEvent>
#include <QCloseEvent>
#include <QColor>

ConnectDialog::ConnectDialog(SerialManager *serialMgr, QWidget *parent)
    : QDialog(parent), m_serialManager(serialMgr)
{
    setWindowTitle("连接到机器人 (COM 串口)");
    resize(600, 440);
    auto *layout = new QVBoxLayout(this);

    // Row 1: Manual COM port entry
    auto *manualRow = new QHBoxLayout;
    manualRow->addWidget(new QLabel("手动端口："));
    m_manualPortEdit = new QLineEdit;
    m_manualPortEdit->setPlaceholderText("如 COM3, COM4, /dev/ttyUSB0");
    manualRow->addWidget(m_manualPortEdit);
    m_manualConnectBtn = new QPushButton("直接连接此端口");
    manualRow->addWidget(m_manualConnectBtn);
    layout->addLayout(manualRow);

    // Row 2: Scan button + status
    auto *btnRow = new QHBoxLayout;
    m_scanBtn = new QPushButton("扫描 COM 端口");
    btnRow->addWidget(m_scanBtn);
    m_statusLabel = new QLabel("");
    btnRow->addWidget(m_statusLabel);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    // Table: Name | Type | Port | Status | Details
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({"设备名称", "类型", "端口", "状态"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table);

    // Bottom buttons
    auto *dialogBtns = new QHBoxLayout;
    auto *connectBtn = new QPushButton("连接选中设备");
    auto *cancelBtn = new QPushButton("取消");
    connectBtn->setEnabled(false);
    dialogBtns->addStretch();
    dialogBtns->addWidget(connectBtn);
    dialogBtns->addWidget(cancelBtn);
    layout->addLayout(dialogBtns);

    // Scan logic
    connect(m_scanBtn, &QPushButton::clicked, this, [this]() {
        m_scanBtn->setEnabled(false);
        m_table->setRowCount(0);
        m_devices.clear();
        m_selected = BLEDeviceInfo{};
        m_statusLabel->setText("正在扫描 COM 端口...");
        startCOMScan();
    });

    // Manual port connection
    connect(m_manualConnectBtn, &QPushButton::clicked, this, [this]() {
        QString portName = m_manualPortEdit->text().trimmed();
        if (portName.isEmpty()) return;
        BLEDeviceInfo info;
        info.portName = portName;
        info.friendlyName = portName;
        info.isEpuckMini = true;
        m_selected = info;
        accept();
    });

    auto doConnect = [this, connectBtn]() {
        int row = m_table->currentRow();
        if (row < 0) row = 0;
        if (row >= 0 && row < m_devices.size()) {
            m_selected = m_devices[row];
            accept();
        }
    };

    // Table: auto-select first row, enable connect button when rows exist
    connect(m_table, &QTableWidget::currentCellChanged, this,
        [connectBtn](int row, int, int, int) {
            connectBtn->setEnabled(row >= 0);
        });

    // Double-click to connect directly
    connect(m_table, &QTableWidget::cellDoubleClicked, this,
        [this](int row, int) {
            if (row >= 0 && row < m_devices.size()) {
                m_selected = m_devices[row];
                accept();
            }
        });

    // Connect button
    connect(connectBtn, &QPushButton::clicked, this, doConnect);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

BLEDeviceInfo ConnectDialog::selectedDevice() const { return m_selected; }
QString ConnectDialog::connectionType() const { return "serial"; }

void ConnectDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    m_scanBtn->click();
}

void ConnectDialog::closeEvent(QCloseEvent *event) {
    m_serialManager->stopScan();
    QDialog::closeEvent(event);
}

void ConnectDialog::addDeviceRow(const BLEDeviceInfo &info) {
    // Check for duplicates
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].portName == info.portName) {
            // Update existing row
            m_devices[i] = info;
            if (info.isEpuckMini) {
                m_table->item(i, 0)->setForeground(QColor("#a6e3a1"));
                m_table->item(i, 0)->setText(info.friendlyName);
                m_table->item(i, 1)->setText("e-puck Mini");
                m_table->item(i, 1)->setForeground(QColor("#a6e3a1"));
                m_table->item(i, 3)->setText("已确认");
                m_table->item(i, 3)->setForeground(QColor("#a6e3a1"));
            }
            return;
        }
    }

    m_devices.append(info);
    int r = m_table->rowCount();
    m_table->insertRow(r);
    // Auto-select first row so connect button enables immediately
    if (r == 0) {
        m_table->selectRow(0);
    }

    // Name
    auto *nameItem = new QTableWidgetItem(info.friendlyName.isEmpty()
        ? QString("未知设备") : info.friendlyName);
    if (info.isEpuckMini) {
        nameItem->setForeground(QColor("#a6e3a1"));
    } else {
        nameItem->setForeground(QColor("#585b70"));
    }
    m_table->setItem(r, 0, nameItem);

    // Type
    auto *typeItem = new QTableWidgetItem(
        info.isEpuckMini ? "e-puck Mini" : "未知");
    typeItem->setForeground(info.isEpuckMini ? QColor("#a6e3a1") : QColor("#585b70"));
    m_table->setItem(r, 1, typeItem);

    // Port
    m_table->setItem(r, 2, new QTableWidgetItem(info.portName));

    // Status
    auto *statusItem = new QTableWidgetItem(info.isEpuckMini ? "已确认" : "验证中...");
    statusItem->setForeground(info.isEpuckMini ? QColor("#a6e3a1") : QColor("#585b70"));
    m_table->setItem(r, 3, statusItem);
}

void ConnectDialog::startCOMScan() {
    if (m_deviceDiscoveredConn) {
        disconnect(m_deviceDiscoveredConn);
        m_deviceDiscoveredConn = {};
    }
    if (m_scanFinishedConn) {
        disconnect(m_scanFinishedConn);
        m_scanFinishedConn = {};
    }
    if (m_scanProgressConn) {
        disconnect(m_scanProgressConn);
        m_scanProgressConn = {};
    }

    m_deviceDiscoveredConn = connect(m_serialManager, &SerialManager::deviceDiscovered, this,
        [this](const BLEDeviceInfo &info) {
            addDeviceRow(info);
        });
    m_scanFinishedConn = connect(m_serialManager, &SerialManager::scanFinished, this,
        [this]() {
            if (m_deviceDiscoveredConn) {
                disconnect(m_deviceDiscoveredConn);
                m_deviceDiscoveredConn = {};
            }
            if (m_scanProgressConn) {
                disconnect(m_scanProgressConn);
                m_scanProgressConn = {};
            }
            m_scanFinishedConn = {};
            m_scanBtn->setEnabled(true);
            int total = m_devices.size();
            int confirmed = 0;
            for (const auto &d : m_devices) {
                if (d.isEpuckMini) confirmed++;
            }
            m_statusLabel->setText(QString("扫描完成 — %1 个端口，%2 个 e-puck 已确认")
                .arg(total).arg(confirmed));
        }, Qt::SingleShotConnection);
    m_scanProgressConn = connect(m_serialManager, &SerialManager::scanProgress, this,
        [this](int completed, int total, const QString &portName, bool verified) {
            m_statusLabel->setText(QString("正在验证 %1/%2：%3 %4")
                .arg(completed).arg(total).arg(portName, verified ? "已识别" : "无响应"));
        });
    m_serialManager->scanPorts();
}

void ConnectDialog::onScanClicked() {}
void ConnectDialog::onDeviceSelected() {}
