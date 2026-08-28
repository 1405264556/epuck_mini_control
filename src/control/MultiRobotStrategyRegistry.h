#pragma once

#include <QList>
#include <QObject>
#include <QStringList>

class IMultiRobotStrategy;
class QPluginLoader;

class MultiRobotStrategyRegistry : public QObject {
    Q_OBJECT
public:
    explicit MultiRobotStrategyRegistry(QObject *parent = nullptr);
    ~MultiRobotStrategyRegistry() override;

    int loadDirectory(const QString &directoryPath);
    void unloadAll();
    IMultiRobotStrategy *strategy(const QString &id) const;
    QList<IMultiRobotStrategy *> strategies() const;
    QStringList errors() const { return m_errors; }

signals:
    void strategiesChanged();

private:
    QList<QPluginLoader *> m_loaders;
    QList<IMultiRobotStrategy *> m_strategies;
    QStringList m_errors;
};
