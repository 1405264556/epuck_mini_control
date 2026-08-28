#include "SettingsDialog.h"
#include "core/Settings.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QTabWidget>

SettingsDialog::SettingsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle("设置"); resize(400, 300);
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget;

    auto *bleTab = new QWidget;
    auto *bleForm = new QFormLayout(bleTab);
    auto *scanTimeout = new QSpinBox; scanTimeout->setRange(1000, 30000);
    scanTimeout->setValue(m_settings->bleScanTimeoutMs());
    bleForm->addRow("扫描超时 (毫秒)：", scanTimeout);
    auto *reconnAttempts = new QSpinBox; reconnAttempts->setRange(0, 10);
    reconnAttempts->setValue(m_settings->bleReconnectAttempts());
    bleForm->addRow("重连次数：", reconnAttempts);
    auto *rssiThreshold = new QSpinBox; rssiThreshold->setRange(-100, 0);
    rssiThreshold->setValue(m_settings->bleRssiThreshold());
    bleForm->addRow("RSSI 阈值 (dBm)：", rssiThreshold);
    tabs->addTab(bleTab, "蓝牙");

    auto *ctrlTab = new QWidget;
    auto *ctrlForm = new QFormLayout(ctrlTab);
    auto *speed = new QDoubleSpinBox; speed->setRange(0.1, 1.0); speed->setSingleStep(0.1);
    speed->setValue(m_settings->defaultMotorSpeed());
    ctrlForm->addRow("默认速度：", speed);
    auto *sens = new QDoubleSpinBox; sens->setRange(0.1, 3.0); sens->setSingleStep(0.1);
    sens->setValue(m_settings->joystickSensitivity());
    ctrlForm->addRow("灵敏度：", sens);
    tabs->addTab(ctrlTab, "控制");

    layout->addWidget(tabs);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, [=]() {
        m_settings->setBleScanTimeoutMs(scanTimeout->value());
        m_settings->setBleReconnectAttempts(reconnAttempts->value());
        m_settings->setBleRssiThreshold(rssiThreshold->value());
        m_settings->setDefaultMotorSpeed(speed->value());
        m_settings->setJoystickSensitivity(sens->value());
        accept();
    });
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btns);
}
