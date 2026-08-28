#include "MicrophoneVisualizer.h"
#include <QPainter>
void MicrophoneVisualizer::paintEvent(QPaintEvent *) {
    QPainter p(this); p.fillRect(rect(), QColor("#1e1e2e"));
    if (!m_data.isValid()) { p.setPen(QColor("#585b70")); p.drawText(rect(), Qt::AlignCenter, "暂无麦克风数据"); return; }
    int w = width() / 4 - 4;
    for (int i = 0; i < 4; ++i) {
        double frac = qBound(0.0, (m_data.microphones[i] + 1.0) / 2.0, 1.0);
        int barH = static_cast<int>(frac * height() * 0.9);
        int x = i * (w + 4) + 2;
        p.fillRect(x, height() - barH, w, barH, QColor(100, 200, 100));
        p.setPen(QColor("#cdd6f4"));
        p.drawText(QRect(x, 0, w, 20), Qt::AlignCenter, QString("通道%1").arg(i+1));
    }
}
