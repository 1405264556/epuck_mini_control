#include "SensorDataModel.h"
SensorDataModel::SensorDataModel(QObject *parent) : QAbstractTableModel(parent) {}
void SensorDataModel::setSensorData(const SensorData &) { emit layoutChanged(); }
QVariant SensorDataModel::data(const QModelIndex &, int role) const {
    if (role != Qt::DisplayRole) return {}; return "-";
}
QVariant SensorDataModel::headerData(int section, Qt::Orientation, int role) const {
    if (role != Qt::DisplayRole) return {};
    return section == 0 ? "传感器" : "数值";
}
