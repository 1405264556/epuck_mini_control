#include "FlashDialog.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "comm/DFUProgrammer.h"
#include "comm/OTAProgrammer.h"
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

FlashDialog::FlashDialog(RobotManager *robotMgr, DFUProgrammer *dfu,
                         OTAProgrammer *ota, QWidget *parent)
    : QDialog(parent)
    , m_robotManager(robotMgr)
    , m_dfu(dfu)
    , m_ota(ota)
{
    setWindowTitle("固件烧录");
    resize(560, 420);

    auto *layout = new QVBoxLayout(this);
    m_status = new QLabel("选择固件文件和目标机器人后开始烧录。多机器人模式会复用这里的目标列表。");
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto *fileRow = new QHBoxLayout;
    m_firmwarePath = new QLineEdit;
    m_firmwarePath->setPlaceholderText("选择 .bin/.hex/.elf 固件文件");
    auto *browseBtn = new QPushButton("浏览");
    fileRow->addWidget(m_firmwarePath);
    fileRow->addWidget(browseBtn);
    layout->addLayout(fileRow);

    layout->addWidget(new QLabel("目标机器人（可多选）："));
    m_targetList = new QListWidget;
    layout->addWidget(m_targetList);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    layout->addWidget(m_progress);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *startBtn = buttons->addButton("开始烧录", QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    connect(browseBtn, &QPushButton::clicked, this, &FlashDialog::chooseFirmware);
    connect(startBtn, &QPushButton::clicked, this, &FlashDialog::startFlash);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if (m_dfu) {
        connect(m_dfu, &DFUProgrammer::flashProgress, m_progress, &QProgressBar::setValue);
        connect(m_dfu, &DFUProgrammer::errorOccurred, this,
                [this](const QString &msg) { m_status->setText(msg); });
    }
    if (m_ota) {
        connect(m_ota, &OTAProgrammer::otaProgress, m_progress, &QProgressBar::setValue);
        connect(m_ota, &OTAProgrammer::otaError, this,
                [this](const QString &msg) { m_status->setText(msg); });
    }

    refreshRobotTargets();
}

void FlashDialog::refreshRobotTargets() {
    m_targetList->clear();
    if (!m_robotManager) return;

    for (auto *robot : m_robotManager->allRobots()) {
        auto *item = new QListWidgetItem(QString("%1  [%2]")
            .arg(robot->name(), robot->id()));
        item->setData(Qt::UserRole, robot->id());
        item->setCheckState(robot->state() == RobotConnectionState::Connected
            ? Qt::Checked : Qt::Unchecked);
        m_targetList->addItem(item);
    }

    if (m_targetList->count() == 0) {
        m_status->setText("当前没有机器人。请先扫描并连接机器人。");
    }
}

void FlashDialog::chooseFirmware() {
    const QString path = QFileDialog::getOpenFileName(
        this, "选择固件文件", QString(),
        "Firmware (*.bin *.hex *.elf);;All files (*.*)");
    if (!path.isEmpty()) {
        m_firmwarePath->setText(path);
    }
}

void FlashDialog::startFlash() {
    const QString firmware = m_firmwarePath->text().trimmed();
    if (firmware.isEmpty()) {
        m_status->setText("请先选择固件文件。");
        return;
    }

    QStringList targets;
    for (int i = 0; i < m_targetList->count(); ++i) {
        auto *item = m_targetList->item(i);
        if (item->checkState() == Qt::Checked) {
            targets << item->data(Qt::UserRole).toString();
        }
    }
    if (targets.isEmpty()) {
        m_status->setText("请至少选择一个目标机器人。");
        return;
    }

    m_progress->setValue(0);
    if (targets.size() == 1 && m_dfu) {
        m_status->setText(QString("开始烧录目标：%1").arg(targets.first()));
        m_dfu->flashFirmware(firmware);
        return;
    }

    m_status->setText(QString("已选择 %1 个目标。批量 OTA/DFU 队列入口已准备，底层协议接入后可直接执行。")
                      .arg(targets.size()));
}
