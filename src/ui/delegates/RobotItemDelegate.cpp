#include "RobotItemDelegate.h"
#include "../models/RobotListModel.h"
#include <QPainter>

void RobotItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Background
    QColor bg = option.state & QStyle::State_Selected ? QColor("#45475a") : QColor("#1e1e2e");
    painter->fillRect(option.rect, bg);

    // State indicator dot
    int state = index.data(RobotListModel::StateRole).toInt();
    QColor dotColor = (state == 2) ? QColor("#a6e3a1")   // Connected
                    : (state == 1) ? QColor("#f9e2af")   // Connecting
                    : QColor("#f38ba8");                   // Disconnected
    QRect dotRect(option.rect.left() + 8, option.rect.top() + 8, 12, 12);
    painter->setBrush(dotColor);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(dotRect);

    // Name
    QRect textRect(option.rect.left() + 28, option.rect.top(),
                   option.rect.width() - 36, option.rect.height());
    painter->setPen(QColor("#cdd6f4"));
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                      index.data(RobotListModel::NameRole).toString());

    painter->restore();
}

QSize RobotItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
    return QSize(200, 28);
}
