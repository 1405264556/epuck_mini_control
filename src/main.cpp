#include "app/Application.h"
#include "ui/MainWindow.h"
#include "util/Logger.h"
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char *argv[]) {
    Application app(argc, argv);
    const QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(logDir);
    Logger::instance()->setLogFile(logDir + "/epuck_mini_control.log");

    // Load stylesheet
    QFile styleFile(":/styles/default.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    if (!app.initialize()) {
        QMessageBox::critical(nullptr, "初始化错误",
                              "应用程序初始化失败。");
        return 1;
    }

    MainWindow mainWindow(app.robotManager(),
                          app.serialManager(), app.coordinator(), app.settings());

    QObject::connect(&app, &Application::fatalError, &mainWindow, [&](const QString &msg) {
        QMessageBox::critical(&mainWindow, "致命错误", msg);
        app.quit();
    });

    mainWindow.show();

    return app.exec();
}
