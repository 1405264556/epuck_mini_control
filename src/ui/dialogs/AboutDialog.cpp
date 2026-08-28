#include "AboutDialog.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("关于");
    resize(380, 220);

    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel("<h2>e-puck Mini 控制中心</h2>");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    layout->addWidget(new QLabel("Version 0.4.0"));
    layout->addWidget(new QLabel("EPFL e-puck Mini 教学科研移动机器人上位机控制软件。"));
    layout->addWidget(new QLabel("支持多串口并发通信、同步/分控、路径规划和协调算法插件。"));
    layout->addStretch();

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(btns);
}
