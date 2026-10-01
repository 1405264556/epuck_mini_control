#include "MicrophoneVisualizer.h"
#include <QPainter>
void MicrophoneVisualizer::paintEvent(QPaintEvent *) {
    QPainter p(this); p.fillRect(rect(), QColor("#ffffff"));
    if (!m_data.has(SensorData::Microphone)) { p.setPen(QColor("#617078")); p.drawText(rect(), Qt::AlignCenter, "麦克风尚未接入"); return; }
    const int channels = qBound(1, m_data.microphoneChannels, 4);
    int w = width() / channels - 4;
    for (int i = 0; i < channels; ++i) {
        double frac = qBound(0.0, std::abs(static_cast<double>(m_data.microphones[i])), 1.0);
        int barH = static_cast<int>(frac * qMax(1, height()-40));
        int x = i * (w + 4) + 2;
        p.fillRect(x, height() - barH, w, barH, QColor(100, 200, 100));
        p.setPen(QColor("#223139"));
        p.drawText(QRect(x, 0, w, 20), Qt::AlignCenter, QString("通道%1").arg(i+1));
    }
}
