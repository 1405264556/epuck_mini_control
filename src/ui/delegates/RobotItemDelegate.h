#pragma once
#include <QStyledItemDelegate>
class RobotItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public: explicit RobotItemDelegate(QObject *p = nullptr) : QStyledItemDelegate(p) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
