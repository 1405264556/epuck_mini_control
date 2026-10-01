#include "ToFVisualizer.h"
#include <QPainter>
void ToFVisualizer::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#ffffff"));
    if (!m_data.has(SensorData::ToF)) { p.setPen(QColor("#617078")); p.drawText(rect(), Qt::AlignCenter, "ToF 尚未接入"); return; }
    double cellW = width() / 8.0, cellH = height() / 8.0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            uint16_t v = m_data.tof[r * 8 + c];
            int intensity = qBound(0, 255 - static_cast<int>(v / 8.0), 255);
            p.fillRect(QRectF(c * cellW, r * cellH, cellW, cellH), QColor(0, 0, intensity));
        }
    }
}
