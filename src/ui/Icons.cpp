#include "Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <cmath>

namespace icons {
namespace {

void paintGlyph(QPainter &p, Glyph glyph, const QColor &color) {
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    auto arrowHead = [&p](const QPointF &tip, const QPointF &a, const QPointF &b) {
        p.drawPolyline(QPolygonF{a, tip, b});
    };
    auto filled = [&p, &color](const QPolygonF &polygon) {
        p.save();
        p.setBrush(color);
        p.drawPolygon(polygon);
        p.restore();
    };

    switch (glyph) {
    case Glyph::Scan: {
        // Two arcs with arrow heads: refresh / rescan.
        p.drawArc(QRectF(4.5, 4.5, 15, 15), 35 * 16, 150 * 16);
        p.drawArc(QRectF(4.5, 4.5, 15, 15), 215 * 16, 150 * 16);
        arrowHead({18.6, 7.2}, {18.9, 3.6}, {15.2, 7.4});
        arrowHead({5.4, 16.8}, {5.1, 20.4}, {8.8, 16.6});
        break;
    }
    case Glyph::Link:
    case Glyph::LinkAll: {
        p.save();
        p.translate(12, 12);
        p.rotate(-45);
        p.drawRoundedRect(QRectF(-9.5, -3.2, 9, 6.4), 3.2, 3.2);
        p.drawRoundedRect(QRectF(0.5, -3.2, 9, 6.4), 3.2, 3.2);
        p.drawLine(QPointF(-3.5, 0), QPointF(3.5, 0));
        p.restore();
        if (glyph == Glyph::LinkAll) {
            p.drawLine(QPointF(17.5, 3.5), QPointF(17.5, 8.5));
            p.drawLine(QPointF(15, 6), QPointF(20, 6));
        }
        break;
    }
    case Glyph::Unlink: {
        p.save();
        p.translate(12, 12);
        p.rotate(-45);
        p.drawRoundedRect(QRectF(-10.5, -3.2, 8, 6.4), 3.2, 3.2);
        p.drawRoundedRect(QRectF(2.5, -3.2, 8, 6.4), 3.2, 3.2);
        p.restore();
        p.drawLine(QPointF(5, 5), QPointF(7.5, 7.5));
        p.drawLine(QPointF(16.5, 16.5), QPointF(19, 19));
        break;
    }
    case Glyph::Stop: {
        // Octagon with a bar: the conventional stop sign.
        QPolygonF octagon;
        const double r = 9.0;
        for (int i = 0; i < 8; ++i) {
            const double angle = (22.5 + 45.0 * i) * 3.14159265358979 / 180.0;
            octagon << QPointF(12 + r * std::cos(angle), 12 + r * std::sin(angle));
        }
        p.drawPolygon(octagon);
        p.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(8, 12), QPointF(16, 12));
        break;
    }
    case Glyph::Settings: {
        // Three sliders.
        const double ys[3] = {6.5, 12, 17.5};
        const double knobs[3] = {15, 8.5, 13};
        for (int i = 0; i < 3; ++i) {
            p.drawLine(QPointF(4, ys[i]), QPointF(20, ys[i]));
            p.save();
            p.setBrush(QColor("#ffffff"));
            p.drawEllipse(QPointF(knobs[i], ys[i]), 2.2, 2.2);
            p.restore();
        }
        break;
    }
    case Glyph::Fit: {
        p.drawPolyline(QPolygonF{{4, 9}, {4, 4}, {9, 4}});
        p.drawPolyline(QPolygonF{{15, 4}, {20, 4}, {20, 9}});
        p.drawPolyline(QPolygonF{{20, 15}, {20, 20}, {15, 20}});
        p.drawPolyline(QPolygonF{{9, 20}, {4, 20}, {4, 15}});
        p.drawRect(QRectF(9, 9, 6, 6));
        break;
    }
    case Glyph::Target: {
        p.drawEllipse(QPointF(12, 12), 7, 7);
        p.drawEllipse(QPointF(12, 12), 2.2, 2.2);
        p.drawLine(QPointF(12, 2.5), QPointF(12, 6));
        p.drawLine(QPointF(12, 18), QPointF(12, 21.5));
        p.drawLine(QPointF(2.5, 12), QPointF(6, 12));
        p.drawLine(QPointF(18, 12), QPointF(21.5, 12));
        break;
    }
    case Glyph::Obstacle: {
        p.drawRoundedRect(QRectF(5, 5, 14, 14), 2, 2);
        p.drawLine(QPointF(5.5, 14), QPointF(14, 5.5));
        p.drawLine(QPointF(10, 18.5), QPointF(18.5, 10));
        break;
    }
    case Glyph::Trash: {
        p.drawLine(QPointF(4.5, 7), QPointF(19.5, 7));
        p.drawPolyline(QPolygonF{{9.5, 7}, {9.5, 4.5}, {14.5, 4.5}, {14.5, 7}});
        p.drawPolyline(QPolygonF{{6.5, 7}, {7.5, 20}, {16.5, 20}, {17.5, 7}});
        p.drawLine(QPointF(10.2, 10.5), QPointF(10.5, 16.5));
        p.drawLine(QPointF(13.8, 10.5), QPointF(13.5, 16.5));
        break;
    }
    case Glyph::Play:
        filled(QPolygonF{{8, 5.5}, {18.5, 12}, {8, 18.5}});
        break;
    case Glyph::Pause: {
        p.setPen(QPen(color, 3.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(9, 6.5), QPointF(9, 17.5));
        p.drawLine(QPointF(15, 6.5), QPointF(15, 17.5));
        break;
    }
    case Glyph::Check: {
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF{{5, 12.5}, {10, 17.5}, {19, 7}});
        break;
    }
    case Glyph::ArrowUp:
        filled(QPolygonF{{12, 5}, {19, 17}, {5, 17}});
        break;
    case Glyph::ArrowDown:
        filled(QPolygonF{{12, 19}, {19, 7}, {5, 7}});
        break;
    case Glyph::ArrowLeft:
        filled(QPolygonF{{5, 12}, {17, 5}, {17, 19}});
        break;
    case Glyph::ArrowRight:
        filled(QPolygonF{{19, 12}, {7, 5}, {7, 19}});
        break;
    case Glyph::Single: {
        p.drawEllipse(QPointF(12, 12), 7.5, 7.5);
        p.drawLine(QPointF(12, 12), QPointF(12, 4.5));
        break;
    }
    case Glyph::Fleet: {
        p.drawEllipse(QPointF(7.5, 8), 4, 4);
        p.drawEllipse(QPointF(16.5, 8), 4, 4);
        p.drawEllipse(QPointF(12, 16.5), 4, 4);
        break;
    }
    case Glyph::Flash: {
        p.drawPolygon(QPolygonF{{13.5, 3}, {6, 13.5}, {11.5, 13.5}, {10.5, 21}, {18, 10.5}, {12.5, 10.5}});
        break;
    }
    case Glyph::Log: {
        p.drawRoundedRect(QRectF(5, 3.5, 14, 17), 2, 2);
        p.drawLine(QPointF(8.5, 8.5), QPointF(15.5, 8.5));
        p.drawLine(QPointF(8.5, 12), QPointF(15.5, 12));
        p.drawLine(QPointF(8.5, 15.5), QPointF(13, 15.5));
        break;
    }
    case Glyph::Keyboard: {
        p.drawRoundedRect(QRectF(3, 6.5, 18, 11), 2, 2);
        p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));
        for (double x : {7.0, 10.3, 13.7, 17.0}) p.drawPoint(QPointF(x, 10));
        p.drawLine(QPointF(8, 14), QPointF(16, 14));
        break;
    }
    case Glyph::Home: {
        p.drawPolyline(QPolygonF{{4, 12}, {12, 4.5}, {20, 12}});
        p.drawPolyline(QPolygonF{{6.5, 10.5}, {6.5, 19.5}, {17.5, 19.5}, {17.5, 10.5}});
        break;
    }
    case Glyph::Route: {
        p.drawEllipse(QPointF(6, 18), 2.2, 2.2);
        p.drawEllipse(QPointF(18, 6), 2.2, 2.2);
        QPainterPath path;
        path.moveTo(8, 17);
        path.cubicTo(15, 16, 9, 8, 16, 7);
        p.setPen(QPen(color, 1.8, Qt::DashLine, Qt::RoundCap));
        p.drawPath(path);
        break;
    }
    }
}

QPixmap render(Glyph glyph, const QColor &color, int logicalSize, qreal devicePixelRatio) {
    QPixmap pixmap(QSize(logicalSize, logicalSize) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);
    paintGlyph(painter, glyph, color);
    return pixmap;
}

} // namespace

QIcon make(Glyph glyph, const QColor &color) {
    QIcon icon;
    QColor disabled = color;
    disabled.setAlphaF(0.35f);
    for (const int size : {16, 20, 24}) {
        for (const qreal ratio : {1.0, 2.0}) {
            icon.addPixmap(render(glyph, color, size, ratio), QIcon::Normal);
            icon.addPixmap(render(glyph, disabled, size, ratio), QIcon::Disabled);
        }
    }
    return icon;
}

} // namespace icons
