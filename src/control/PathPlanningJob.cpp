#include "PathPlanningJob.h"
#include "PathPlanner.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QProcess>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <cmath>

PathPlanningJob::PathPlanningJob(QObject *parent) : QObject(parent) {}
PathPlanningJob::~PathPlanningJob() { if (m_process) m_process->kill(); }

void PathPlanningJob::complete(const QVector<Vec2> &path, const QString &error) {
    if (m_finished) return;
    m_finished = true;
    emit finished(path, error);
}

void PathPlanningJob::start(const Vec2 &start, const Vec2 &goal, const QVector<Vec2> &obstacles,
                           const QString &mode, const QString &script) {
    if (mode == "builtin") {
        auto *watcher = new QFutureWatcher<QVector<Vec2>>(this);
        connect(watcher, &QFutureWatcher<QVector<Vec2>>::finished, this, [this, watcher]() {
            const auto path = watcher->result();
            watcher->deleteLater();
            complete(path, path.isEmpty() ? "目标或路径被障碍物阻挡" : QString{});
        });
        watcher->setFuture(QtConcurrent::run([start, goal, obstacles]() {
            PathPlanner planner;
            planner.setGridSize(401, 401, 1.0);
            QVector<Vec2> shifted;
            for (const auto &obstacle : obstacles) shifted.append(obstacle + Vec2(200, 200));
            planner.setObstacles(shifted, 8.0);
            auto path = planner.findPath(start + Vec2(200, 200), goal + Vec2(200, 200));
            for (auto &point : path) point -= Vec2(200, 200);
            return path;
        }));
        return;
    }
    if (!QFileInfo::exists(script)) { complete({}, "请选择有效的算法脚本"); return; }
    QJsonArray obstacleJson;
    for (const auto &point : obstacles) obstacleJson.append(QJsonArray{point.x, point.y});
    const QJsonObject input{{"start", QJsonArray{start.x, start.y}}, {"goal", QJsonArray{goal.x, goal.y}},
        {"obstacles", obstacleJson}, {"bounds", QJsonArray{-200, 200, -200, 200}}, {"obstacle_radius", 8}};
    m_input = std::make_unique<QTemporaryFile>(QDir::tempPath() + "/epuck_planner_XXXXXX.json");
    if (!m_input->open()) { complete({}, "无法创建算法输入文件"); return; }
    m_input->write(QJsonDocument(input).toJson(QJsonDocument::Compact));
    m_input->flush();
    m_process = new QProcess(this);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) complete({}, "算法运行环境无法启动：" + m_process->errorString());
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit || exitCode != 0) {
            complete({}, QString::fromLocal8Bit(m_process->readAllStandardError()).left(200));
            return;
        }
        const auto output = m_process->readAllStandardOutput().trimmed();
        const int first = output.indexOf('['), last = output.lastIndexOf(']');
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(output.mid(first, last-first+1), &error);
        if (first < 0 || last <= first || !doc.isArray() || error.error != QJsonParseError::NoError) {
            complete({}, "脚本未返回有效路径 JSON 数组"); return;
        }
        QVector<Vec2> path;
        for (const auto &value : doc.array()) {
            const auto p = value.toArray();
            if (p.size() != 2 || !p[0].isDouble() || !p[1].isDouble()
                || !std::isfinite(p[0].toDouble()) || !std::isfinite(p[1].toDouble())
                || std::abs(p[0].toDouble()) > 200 || std::abs(p[1].toDouble()) > 200) {
                complete({}, "路径包含无效或越界坐标"); return;
            }
            path.append({p[0].toDouble(), p[1].toDouble()});
        }
        complete(path, path.size() < 2 ? "路径至少需要两个点" : QString{});
    });
    QTimer::singleShot(30000, this, [this]() {
        if (!m_finished && m_process) { m_process->kill(); complete({}, "算法超过 30 秒未完成"); }
    });
    if (mode == "python") {
        m_process->start("python", {script, m_input->fileName()});
    } else {
        const QFileInfo info(script);
        if (!QRegularExpression("^[A-Za-z][A-Za-z0-9_]*$").match(info.completeBaseName()).hasMatch()) {
            complete({}, "MATLAB 函数名无效"); return;
        }
        auto quotePath = [](QString path) { return path.replace('\\', '/').replace("'", "''"); };
        const QString batch = QString("addpath('%1'); data=jsondecode(fileread('%2')); path=%3(data); disp(jsonencode(path));")
            .arg(quotePath(info.absolutePath()), quotePath(m_input->fileName()), info.completeBaseName());
        m_process->start("matlab", {"-batch", batch});
    }
}
