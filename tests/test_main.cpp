#include <gtest/gtest.h>
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QDir>

int main(int argc, char **argv) {
#ifdef EPUCK_TEST_QT_PLUGIN_DIR
    QCoreApplication::addLibraryPath(QStringLiteral(EPUCK_TEST_QT_PLUGIN_DIR));
#endif
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    if (qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        const auto fonts = QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts/");
        QFontDatabase::addApplicationFont(fonts + "msyh.ttc");
        QFontDatabase::addApplicationFont(fonts + "segoeui.ttf");
    }
#endif
    QFile style(":/styles/default.qss");
    if (style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
