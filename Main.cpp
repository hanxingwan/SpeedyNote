#ifdef _WIN32
#include <windows.h>
#endif

#include <QApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QLoggingCategory>
#include <QInputMethod>
#include <QTranslator>
#include <QLibraryInfo>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
#ifdef _WIN32
    FreeConsole();  // Hide console safely on Windows

    /*
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    */
    
     // to show console for debugging
    
    
#endif
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");
    SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);

    /*
    qDebug() << "SDL2 version:" << SDL_GetRevision();
    qDebug() << "Num Joysticks:" << SDL_NumJoysticks();

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            qDebug() << "Controller" << i << "is" << SDL_GameControllerNameForIndex(i);
        } else {
            qDebug() << "Joystick" << i << "is not a recognized controller";
        }
    }
    */  // For sdl2 debugging
    


    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));  // ✅ Enable Virtual Keyboard
    QApplication app(argc, argv);


    QTranslator translator;
    QString locale = QLocale::system().name(); // e.g., "zh_CN", "es_ES"
    QString langCode = locale.section('_', 0, 0); // e.g., "zh"

    // printf("Locale: %s\n", locale.toStdString().c_str());
    // printf("Language Code: %s\n", langCode.toStdString().c_str());

    // QString locale = "es-ES"; // e.g., "zh_CN", "es_ES"
    // QString langCode = "es"; // e.g., "zh"

    if (translator.load("./app_" + langCode + ".qm")) {
        app.installTranslator(&translator);
    }

    QString notebookFile;
    if (argc >= 2) {
        notebookFile = QString::fromLocal8Bit(argv[1]);
        qDebug() << "Notebook file received:" << notebookFile;
    }

    MainWindow w;
    if (!notebookFile.isEmpty()) {
        w.importNotebookFromFile(notebookFile);
    }
    w.show();
    return app.exec();
}
