#include "SettingsDialog.h"
#include "core/Settings.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QPushButton>

SettingsDialog::SettingsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle("设置");
    resize(440, 320);
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget;

    // ---- Connection ----
    auto *linkTab = new QWidget;
    auto *linkForm = new QFormLayout(linkTab);
    linkForm->setVerticalSpacing(10);
    auto *autoReconnect = new QCheckBox("连接意外中断后自动重连");
    autoReconnect->setChecked(m_settings->autoReconnect());
    linkForm->addRow(autoReconnect);
    auto *attempts = new QSpinBox;
    attempts->setRange(1, 10);
    attempts->setSuffix(" 次");
    attempts->setValue(m_settings->reconnectAttempts());
    attempts->setToolTip("每次间隔依次为 1、2、4、8 秒");
    linkForm->addRow("最多重连：", attempts);
    connect(autoReconnect, &QCheckBox::toggled, attempts, &QWidget::setEnabled);
    attempts->setEnabled(autoReconnect->isChecked());
    auto *autoConnect = new QCheckBox("启动时自动连接上次使用的设备");
    autoConnect->setChecked(m_settings->autoConnectOnStartup());
    linkForm->addRow(autoConnect);
    auto *pollInterval = new QSpinBox;
    pollInterval->setRange(40, 1000);
    pollInterval->setSingleStep(20);
    pollInterval->setSuffix(" ms");
    pollInterval->setValue(m_settings->sensorPollIntervalMs());
    pollInterval->setToolTip("每台机器人的遥测轮询间隔。蓝牙链路不稳定或设备较多时可适当调大。");
    linkForm->addRow("传感器轮询间隔：", pollInterval);
    tabs->addTab(linkTab, "连接");

    // ---- Driving ----
    auto *driveTab = new QWidget;
    auto *driveForm = new QFormLayout(driveTab);
    driveForm->setVerticalSpacing(10);
    auto *speedLimit = new QSpinBox;
    speedLimit->setRange(100, 1000);
    speedLimit->setSingleStep(50);
    speedLimit->setSuffix(" 步/秒");
    speedLimit->setValue(m_settings->speedLimit());
    driveForm->addRow("摇杆 / 键盘限速：", speedLimit);
    auto *keyboard = new QCheckBox("启用键盘驾驶（W A S D，空格停止）");
    keyboard->setChecked(m_settings->keyboardDriveEnabled());
    driveForm->addRow(keyboard);
    auto *note = new QLabel("键盘驾驶只在主窗口处于前台、且焦点不在输入框时生效；"
                            "松开按键或窗口失去焦点会立即停止。");
    note->setObjectName("hintLabel");
    note->setWordWrap(true);
    driveForm->addRow(note);
    tabs->addTab(driveTab, "驾驶");

    layout->addWidget(tabs);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("确定");
    btns->button(QDialogButtonBox::Ok)->setProperty("accent", true);
    btns->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(btns, &QDialogButtonBox::accepted, this, [=]() {
        m_settings->setAutoReconnect(autoReconnect->isChecked());
        m_settings->setReconnectAttempts(attempts->value());
        m_settings->setAutoConnectOnStartup(autoConnect->isChecked());
        m_settings->setSensorPollIntervalMs(pollInterval->value());
        m_settings->setSpeedLimit(speedLimit->value());
        m_settings->setKeyboardDriveEnabled(keyboard->isChecked());
        accept();
    });
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btns);
}
