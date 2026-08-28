#pragma once

#include <QApplication>
#include <memory>

class Settings;
class RobotManager;
class SerialManager;
class MultiRobotCoordinator;
class DFUProgrammer;

class Application : public QApplication {
    Q_OBJECT
public:
    Application(int &argc, char **argv);
    ~Application() override;

    bool initialize();

    static Application *self();

    Settings *settings() const { return m_settings.get(); }
    RobotManager *robotManager() const { return m_robotManager.get(); }
    SerialManager *serialManager() const { return m_serialManager.get(); }
    MultiRobotCoordinator *coordinator() const { return m_coordinator.get(); }
    DFUProgrammer *dfuProgrammer() const { return m_dfuProgrammer.get(); }

signals:
    void fatalError(const QString &message);

private:
    void registerMetatypes();

    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<RobotManager> m_robotManager;
    std::unique_ptr<SerialManager> m_serialManager;
    std::unique_ptr<MultiRobotCoordinator> m_coordinator;
    std::unique_ptr<DFUProgrammer> m_dfuProgrammer;
};
