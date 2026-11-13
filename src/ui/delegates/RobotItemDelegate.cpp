#include "RobotItemDelegate.h"
#include "../models/RobotListModel.h"
#include "core/Types.h"
#include <QPainter>

void RobotItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QRect row = option.rect.adjusted(3, 2, -3, -2);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    if (selected || hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? QColor("#e0f1ec") : QColor("#f3f6f7"));
        painter->drawRoundedRect(row, 6, 6);
    }

    const auto state = static_cast<RobotConnectionState>(index.data(RobotListModel::StateRole).toInt());
    QColor dot("#b4c0c5");
    QColor detailColor("#6f828a");
    switch (state) {
    case RobotConnectionState::Connected: dot = QColor("#0f9d8a"); detailColor = QColor("#066b60"); break;
    case RobotConnectionState::Connecting:
    case RobotConnectionState::Disconnecting: dot = QColor("#d39a2c"); detailColor = QColor("#8a5a10"); break;
    case RobotConnectionState::Error: dot = QColor("#d74950"); detailColor = QColor("#b3323a"); break;
    case RobotConnectionState::Disconnected: break;
    }

    // State dot: filled when online, ring otherwise, so it also reads without colour.
    const QPointF dotCenter(row.left() + 14, row.center().y() + 0.5);
    if (state == RobotConnectionState::Connected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(dot);
        painter->drawEllipse(dotCenter, 5, 5);
    } else {
        painter->setPen(QPen(dot, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(dotCenter, 4, 4);
    }

    const QRect textRect = row.adjusted(28, 4, -6, -4);
    QFont nameFont = option.font;
    nameFont.setWeight(QFont::DemiBold);
    painter->setFont(nameFont);
    painter->setPen(QColor(state == RobotConnectionState::Disconnected ? "#52666f" : "#223139"));
    const QFontMetrics nameMetrics(nameFont);
    painter->drawText(QRect(textRect.left(), textRect.top(), textRect.width(), textRect.height() / 2),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(index.data(RobotListModel::NameRole).toString(),
                                             Qt::ElideRight, textRect.width()));

    QFont detailFont = option.font;
    detailFont.setPixelSize(11);
    painter->setFont(detailFont);
    const QFontMetrics detailMetrics(detailFont);
    const QString port = index.data(RobotListModel::PortRole).toString();
    const QString detail = index.data(RobotListModel::DetailRole).toString();
    const QRect detailRect(textRect.left(), textRect.top() + textRect.height() / 2,
                           textRect.width(), textRect.height() / 2);
    painter->setPen(QColor("#6f828a"));
    const QString portText = port + " · ";
    painter->drawText(detailRect, Qt::AlignLeft | Qt::AlignVCenter, portText);
    const int portWidth = detailMetrics.horizontalAdvance(portText);
    painter->setPen(detailColor);
    painter->drawText(detailRect.adjusted(portWidth, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                      detailMetrics.elidedText(detail, Qt::ElideRight, detailRect.width() - portWidth));

    painter->restore();
}

QSize RobotItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
    return QSize(180, 48);
}
