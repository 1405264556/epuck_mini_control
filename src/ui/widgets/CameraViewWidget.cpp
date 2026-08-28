#include "CameraViewWidget.h"
#include "core/SensorData.h"
#include <QPixmap>

CameraViewWidget::CameraViewWidget(QWidget *p) : QLabel(p) {
    setText("暂无摄像头数据");
    setAlignment(Qt::AlignCenter);
    setMinimumSize(160, 120);
    setStyleSheet("background-color: #1e1e2e; color: #585b70; border: 1px solid #313244;");
}

void CameraViewWidget::updateData(const SensorData &d) {
    if (d.isValid() && !d.cameraFrame.isNull()) {
        setPixmap(QPixmap::fromImage(d.cameraFrame).scaled(
            size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}
