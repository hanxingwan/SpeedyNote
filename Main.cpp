#ifdef _WIN32
#include <windows.h>
#endif

#include <QApplication>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QLoggingCategory>
#include <QInputMethod>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
#ifdef _WIN32
    FreeConsole();  // Hide console safely on Windows
#endif
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));  // ✅ Enable Virtual Keyboard
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
