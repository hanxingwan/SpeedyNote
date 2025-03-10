#include <windows.h>
#include <QApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QLoggingCategory>
#include <QInputMethod>
#include "MainWindow.h"


int main(int argc, char *argv[]) {
    FreeConsole();  // Hide console safely
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));  // ✅ Enable Virtual Keyboard
    // QGuiApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTabletEvents, false);
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}