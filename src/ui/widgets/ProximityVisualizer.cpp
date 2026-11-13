#include "ProximityVisualizer.h"
#include "core/SensorData.h"
#include <QPainter>
#include <QtMath>

void ProximityVisualizer::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#ffffff"));
    if (!m_data.has(SensorData::Proximity)) {
        p.setPen(QColor("#6f828a"));
        p.drawText(rect(), Qt::AlignCenter, "暂无接近传感器数据");
        return;
    }

    const int top = 26, bottom = 24, side = 10, gap = 6;
    const QRectF plot(side, top, width() - side * 2, qMax(10, height() - top - bottom));

    // Quarter guides give the bars a readable scale without numbers on an axis.
    p.setPen(QPen(QColor("#edf1f2"), 1));
    for (int i = 1; i <= 3; ++i) {
        const double y = plot.bottom() - plot.height() * i / 4.0;
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
    p.setPen(QPen(QColor("#dbe2e5"), 1));
    p.drawLine(plot.bottomLeft(), plot.bottomRight());

    const double slot = plot.width() / 8.0;
    const double barWidth = qMax(6.0, slot - gap);
    QFont small = p.font();
    small.setPixelSize(11);
    p.setFont(small);
    for (int i = 0; i < 8; ++i) {
        const double frac = qBound(0.0, m_data.proximity[i] / 4095.0, 1.0);
        const double x = plot.left() + slot * i + (slot - barWidth) / 2.0;
        // A sliver stays visible at zero so a present-but-idle channel is distinguishable.
        const double barHeight = qMax(2.0, frac * plot.height());
        const QRectF track(x, plot.top(), barWidth, plot.height());
        const QRectF bar(x, plot.bottom() - barHeight, barWidth, barHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#f3f6f7"));
        p.drawRoundedRect(track, 3, 3);
        p.setBrush(frac > 0.7 ? QColor("#d74950") : frac > 0.3 ? QColor("#d39a2c") : QColor("#0f9d8a"));
        p.drawRoundedRect(bar, 3, 3);
        p.setPen(QColor("#5d7079"));
        p.drawText(QRectF(plot.left() + slot * i, 4, slot, 18), Qt::AlignCenter, QString("IR%1").arg(i));
        p.setPen(QColor("#223139"));
        p.drawText(QRectF(plot.left() + slot * i, height() - bottom + 3, slot, 18), Qt::AlignCenter,
                   QString::number(m_data.proximity[i]));
    }
}
