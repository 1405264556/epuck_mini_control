#include "MultiRobotStrategyRegistry.h"

#include "IMultiRobotStrategy.h"

#include <QDir>
#include <QLibrary>
#include <QPluginLoader>
#include <utility>

MultiRobotStrategyRegistry::MultiRobotStrategyRegistry(QObject *parent)
    : QObject(parent)
{
}

MultiRobotStrategyRegistry::~MultiRobotStrategyRegistry() {
    unloadAll();
}

int MultiRobotStrategyRegistry::loadDirectory(const QString &directoryPath) {
    unloadAll();
    QDir directory(directoryPath);
    if (!directory.exists()) return 0;

    const auto files = directory.entryInfoList(QDir::Files, QDir::Name);
    for (const auto &file : files) {
        if (!QLibrary::isLibrary(file.absoluteFilePath())) continue;

        auto *loader = new QPluginLoader(file.absoluteFilePath(), this);
        QObject *instance = loader->instance();
        auto *loadedStrategy = instance ? qobject_cast<IMultiRobotStrategy *>(instance) : nullptr;
        if (!loadedStrategy || loadedStrategy->id().trimmed().isEmpty()
            || strategy(loadedStrategy->id())) {
            m_errors.append(QString("%1：%2")
                .arg(file.fileName(), loader->errorString().isEmpty()
                    ? QString("接口无效或算法 ID 重复") : loader->errorString()));
            loader->unload();
            delete loader;
            continue;
        }

        m_loaders.append(loader);
        m_strategies.append(loadedStrategy);
    }
    emit strategiesChanged();
    return m_strategies.size();
}

void MultiRobotStrategyRegistry::unloadAll() {
    m_strategies.clear();
    for (auto *loader : std::as_const(m_loaders)) {
        loader->unload();
        delete loader;
    }
    m_loaders.clear();
    m_errors.clear();
}

IMultiRobotStrategy *MultiRobotStrategyRegistry::strategy(const QString &id) const {
    for (auto *loadedStrategy : m_strategies) {
        if (loadedStrategy->id() == id) return loadedStrategy;
    }
    return nullptr;
}

QList<IMultiRobotStrategy *> MultiRobotStrategyRegistry::strategies() const {
    return m_strategies;
}
