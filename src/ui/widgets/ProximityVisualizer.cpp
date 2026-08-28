#include "ProximityVisualizer.h"
#include "core/SensorData.h"
#include <QPainter>
#include <QtMath>

void ProximityVisualizer::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#1e1e2e"));
    if (!m_data.isValid()) {
        p.setPen(QColor("#585b70"));
        p.drawText(rect(), Qt::AlignCenter, "暂无接近传感器数据");
        return;
    }
    int w = width() / 8 - 4;
    for (int i = 0; i < 8; ++i) {
        double frac = qBound(0.0, m_data.proximity[i] / 4095.0, 1.0);
        int barH = static_cast<int>(frac * height() * 0.9);
        int x = i * (w + 4) + 2;
        int y = height() - barH;
        int r = static_cast<int>(frac * 255);
        int g = static_cast<int>((1.0 - frac) * 255);
        p.fillRect(x, y, w, barH, QColor(r, g, 0));
        p.setPen(QColor("#cdd6f4"));
        p.drawText(QRect(x, height() - 20, w, 20), Qt::AlignCenter,
                   QString::number(m_data.proximity[i]));
    }
}
