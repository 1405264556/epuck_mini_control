#include "ProximityVisualizer.h"
#include "core/SensorData.h"
#include <QPainter>
#include <QtMath>

void ProximityVisualizer::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#ffffff"));
    if (!m_data.has(SensorData::Proximity)) {
        p.setPen(QColor("#617078"));
        p.drawText(rect(), Qt::AlignCenter, "暂无接近传感器数据");
        return;
    }
    int w = width() / 8 - 4;
    for (int i = 0; i < 8; ++i) {
        double frac = qBound(0.0, m_data.proximity[i] / 4095.0, 1.0);
        const int usable = qMax(1, height()-45);
        int barH = static_cast<int>(frac * usable);
        int x = i * (w + 4) + 2;
        int y = height() - 28 - barH;
        p.fillRect(x, y, w, barH, frac > 0.7 ? QColor("#d74950") : frac > 0.3 ? QColor("#c19a36") : QColor("#188b7a"));
        p.setPen(QColor("#223139"));
        p.drawText(QRect(x, 2, w, 20), Qt::AlignCenter, QString("IR%1").arg(i));
        p.drawText(QRect(x, height() - 20, w, 20), Qt::AlignCenter,
                   QString::number(m_data.proximity[i]));
    }
}
