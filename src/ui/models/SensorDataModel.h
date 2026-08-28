#pragma once
#include <QAbstractTableModel>
#include "core/SensorData.h"
class SensorDataModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit SensorDataModel(QObject *parent = nullptr);
    void setSensorData(const SensorData &data);
    int rowCount(const QModelIndex & = {}) const override { return 16; }
    int columnCount(const QModelIndex & = {}) const override { return 2; }
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation, int role) const override;
private:
    SensorData m_data;
};
