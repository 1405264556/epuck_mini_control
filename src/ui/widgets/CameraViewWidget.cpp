#include "CameraViewWidget.h"
#include "core/SensorData.h"
#include <QPixmap>

CameraViewWidget::CameraViewWidget(QWidget *p) : QLabel(p) {
    setText("暂无摄像头数据");
    setAlignment(Qt::AlignCenter);
    setMinimumSize(160, 120);
    setStyleSheet("background-color: #f6f8fa; color: #617078; border: 1px solid #dce2e5;");
}

void CameraViewWidget::updateData(const SensorData &d) {
    m_frame = d.has(SensorData::Camera) ? d.cameraFrame : QImage{};
    refreshFrame();
}

void CameraViewWidget::resizeEvent(QResizeEvent *event) { QLabel::resizeEvent(event); refreshFrame(); }
void CameraViewWidget::refreshFrame() {
    if (m_frame.isNull()) { clear(); setText("等待摄像头帧"); return; }
    setPixmap(QPixmap::fromImage(m_frame).scaled(size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}
