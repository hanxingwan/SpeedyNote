#include <windows.h>
#include <QApplication>
#include "MainWindow.h"


int main(int argc, char *argv[]) {
    FreeConsole();  // Hide console safely
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}