#include "Application.h"
#include "core/Settings.h"
#include "core/RobotManager.h"
#include "comm/SerialManager.h"
#include "comm/DFUProgrammer.h"
#include "control/MultiRobotCoordinator.h"
#include "util/Logger.h"
#include <QMetaType>

Q_DECLARE_METATYPE(RobotConnectionState)
Q_DECLARE_METATYPE(SensorData)

Application *Application::self() {
    return qobject_cast<Application *>(QCoreApplication::instance());
}

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setApplicationName("e-puck Mini Control");
    setApplicationVersion("0.5.0");
    setOrganizationName("EPFL");
}

Application::~Application() = default;

void Application::registerMetatypes() {
    qRegisterMetaType<RobotConnectionState>("RobotConnectionState");
    qRegisterMetaType<SensorData>("SensorData");
    qRegisterMetaType<RobotId>("RobotId");
    qRegisterMetaType<BLEDeviceInfo>("BLEDeviceInfo");
    qRegisterMetaType<FormationShape>("FormationShape");
    qRegisterMetaType<Vec2>("Vec2");
    qRegisterMetaType<WheelSpeeds>("WheelSpeeds");
}

bool Application::initialize() {
    registerMetatypes();

    m_settings = std::make_unique<Settings>();
    m_robotManager = std::make_unique<RobotManager>();
    m_serialManager = std::make_unique<SerialManager>(m_robotManager.get());
    m_coordinator = std::make_unique<MultiRobotCoordinator>(m_robotManager.get());
    m_dfuProgrammer = std::make_unique<DFUProgrammer>();

    Logger::instance()->setMinimumLevel(static_cast<QtMsgType>(m_settings->logLevel()));
    qCInfo(logApp) << "Application initialized. Version:" << applicationVersion();

    return true;
}
