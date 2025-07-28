#ifdef _WIN32
#include <windows.h>
#endif

#include <QApplication>
#include <QTranslator>
#include <QLocale>
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
    


    // Enable Windows IME support for multi-language input
    QApplication app(argc, argv);
    
    // Ensure IME is properly enabled for Windows
    #ifdef _WIN32
    app.setAttribute(Qt::AA_EnableHighDpiScaling, true);
    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    #endif

    
    QTranslator translator;
    QString locale = QLocale::system().name(); // e.g., "zh_CN", "es_ES"
    QString langCode = locale.section('_', 0, 0); // e.g., "zh"

    // Debug: Uncomment these lines to debug translation loading issues
    // printf("Locale: %s\n", locale.toStdString().c_str());
    // printf("Language Code: %s\n", langCode.toStdString().c_str());

    // Try multiple paths to find translation files
    QStringList translationPaths = {
        QCoreApplication::applicationDirPath(),  // Same directory as executable
        QCoreApplication::applicationDirPath() + "/translations",  // translations subdirectory
        ":/resources/translations"  // Qt resource system
    };
    
    bool translationLoaded = false;
    for (const QString &path : translationPaths) {
        QString translationFile = path + "/app_" + langCode + ".qm";
        if (translator.load(translationFile)) {
            app.installTranslator(&translator);
            translationLoaded = true;
            // Debug: Uncomment this line to see which translation file was loaded
            // printf("Translation loaded from: %s\n", translationFile.toStdString().c_str());
            break;
        }
    }
    
    if (!translationLoaded) {
        // Debug: Uncomment this line to see when translation loading fails
        // printf("No translation file found for language: %s\n", langCode.toStdString().c_str());
    }

    QString inputFile;
    if (argc >= 2) {
        inputFile = QString::fromLocal8Bit(argv[1]);
        // qDebug() << "Input file received:" << inputFile;
    }

    MainWindow w;
    if (!inputFile.isEmpty()) {
        // Check file extension to determine how to handle it
        if (inputFile.toLower().endsWith(".pdf")) {
            // Handle PDF file association
            w.show(); // Show window first for dialog parent
            w.openPdfFile(inputFile);
        } else if (inputFile.toLower().endsWith(".spn")) {
            // Handle SpeedyNote package
            w.show(); // Show window first
            w.openSpnPackage(inputFile);

        } else {
            // Unknown file type, just show the application
            w.show();
        }
    } else {
        w.show();
    }
    return app.exec();
}
