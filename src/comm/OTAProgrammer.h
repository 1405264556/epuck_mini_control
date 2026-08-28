#pragma once

#include <QObject>
#include <QString>
#include "core/Types.h"

class BLEManager;

class OTAProgrammer : public QObject {
    Q_OBJECT
public:
    explicit OTAProgrammer(BLEManager *bleManager, QObject *parent = nullptr);

    void startOTA(const RobotId &robotId, const QString &firmwarePath);
    void cancelOTA();

signals:
    void otaProgress(int percent);
    void otaFinished(bool success);
    void otaError(const QString &msg);

private:
    BLEManager *m_bleManager;
    bool m_otaInProgress = false;
};
