#include "OTAProgrammer.h"
#include "BLEManager.h"
#include "util/Logger.h"

OTAProgrammer::OTAProgrammer(BLEManager *bleManager, QObject *parent)
    : QObject(parent)
    , m_bleManager(bleManager)
{
}

void OTAProgrammer::startOTA(const RobotId &robotId, const QString &firmwarePath) {
    Q_UNUSED(robotId) Q_UNUSED(firmwarePath)
    qCDebug(logOTA) << "OTA stub called for:" << robotId << firmwarePath;
    emit otaError("OTA flashing not yet implemented. Phase 6.");
    emit otaFinished(false);
}

void OTAProgrammer::cancelOTA() {
    m_otaInProgress = false;
}
