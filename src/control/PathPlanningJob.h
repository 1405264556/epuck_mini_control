#pragma once
#include <QObject>
#include <QTemporaryFile>
#include <memory>
#include "core/Types.h"
class QProcess;

class PathPlanningJob : public QObject {
    Q_OBJECT
public:
    explicit PathPlanningJob(QObject *parent = nullptr);
    ~PathPlanningJob() override;
    void start(const Vec2 &start, const Vec2 &goal, const QVector<Vec2> &obstacles,
               const QString &mode, const QString &script);
signals:
    void finished(const QVector<Vec2> &path, const QString &error);
private:
    QProcess *m_process = nullptr;
    std::unique_ptr<QTemporaryFile> m_input;
    bool m_finished = false;
    void complete(const QVector<Vec2> &path, const QString &error = {});
};
