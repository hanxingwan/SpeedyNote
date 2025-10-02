#ifdef _WIN32
#include <windows.h>
#endif

#include <QApplication>
#include <QGuiApplication>
#include <QTranslator>
#include <QLocale>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QColor>
#include <QFont>
#include "MainWindow.h"
#include "LauncherWindow.h"
#include "SpnPackageManager.h"
#include "InkCanvas.h" // For BackgroundStyle enum

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

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
    // Qt5 requires high DPI attributes to be set BEFORE creating QApplication
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // Qt 5.14+ supports high DPI scale factor rounding policy for better font rendering
    #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    #endif
    
    QApplication app(argc, argv);
    
    // Ensure IME is properly enabled for Windows
    #ifdef _WIN32
    app.setAttribute(Qt::AA_EnableHighDpiScaling, true);
    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    #endif

    
    QTranslator translator;
    
    // Check for manual language override
    QSettings settings("SpeedyNote", "App");
    bool useSystemLanguage = settings.value("useSystemLanguage", true).toBool();
    QString langCode;
    
    if (useSystemLanguage) {
        // Use system language
        QString locale = QLocale::system().name(); // e.g., "zh_CN", "es_ES"
        langCode = locale.section('_', 0, 0); // e.g., "zh"
    } else {
        // Use manual override
        langCode = settings.value("languageOverride", "en").toString();
    }

    // Debug: Uncomment these lines to debug translation loading issues
    // printf("Locale: %s\n", locale.toStdString().c_str());
    // printf("Language Code: %s\n", langCode.toStdString().c_str());

    // Try multiple paths to find translation files
    QStringList translationPaths = {
        QCoreApplication::applicationDirPath(),  // Same directory as executable (Windows/portable)
        QCoreApplication::applicationDirPath() + "/translations",  // translations subdirectory (Windows)
        "/usr/share/speedynote/translations",  // Standard Linux installation path
        "/usr/local/share/speedynote/translations",  // Local Linux installation path
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "speedynote/translations", QStandardPaths::LocateDirectory),  // XDG data directories
        ":/resources/translations"  // Qt resource system (fallback)
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
    
    // Qt5: Set modern system font explicitly for better rendering on Windows
    // This fixes the "Windows XP style old font" issue in Qt5
    #ifdef Q_OS_WIN
    QFont appFont = app.font();
    
    // Set appropriate modern font based on language
    if (langCode == "zh") {
        // Detect if it's Simplified or Traditional Chinese
        QString locale = QLocale::system().name();
        if (locale.startsWith("zh_TW") || locale.startsWith("zh_HK")) {
            // Traditional Chinese - use Microsoft JhengHei
            appFont.setFamily("Microsoft JhengHei UI");
        } else {
            // Simplified Chinese - use Microsoft YaHei
            appFont.setFamily("Microsoft YaHei UI");
        }
        appFont.setPointSize(9);
    } else if (langCode == "ja") {
        // Japanese - use Yu Gothic UI
        appFont.setFamily("Yu Gothic UI");
        appFont.setPointSize(9);
    } else if (langCode == "ko") {
        // Korean - use Malgun Gothic
        appFont.setFamily("Malgun Gothic");
        appFont.setPointSize(9);
    } else {
        // English and other languages - use Segoe UI
        appFont.setFamily("Segoe UI");
        appFont.setPointSize(9);
    }
    
    app.setFont(appFont);
    #endif

    QString inputFile;
    bool createNewPackage = false;
    bool createSilent = false;
    
    if (argc >= 2) {
        QString firstArg = QString::fromLocal8Bit(argv[1]);
        if (firstArg == "--create-new" && argc >= 3) {
            // Handle --create-new command (opens SpeedyNote)
            createNewPackage = true;
            inputFile = QString::fromLocal8Bit(argv[2]);
        } else if (firstArg == "--create-silent" && argc >= 3) {
            // Handle --create-silent command (creates file and exits)
            createSilent = true;
            inputFile = QString::fromLocal8Bit(argv[2]);
        } else {
            // Regular file argument
            inputFile = firstArg;
        }
        // qDebug() << "Input file received:" << inputFile << "Create new:" << createNewPackage << "Create silent:" << createSilent;
    }

    // Handle silent creation (context menu) - create file and exit immediately
    if (createSilent && !inputFile.isEmpty()) {
        if (inputFile.toLower().endsWith(".spn")) {
            // Check if file already exists
            if (QFile::exists(inputFile)) {
                return 1; // Exit with error code
            }
            
            // Get the base name for the notebook (without .spn extension)
            QFileInfo fileInfo(inputFile);
            QString notebookName = fileInfo.baseName();
            
            // ✅ Load user's default background settings
            QSettings settings("SpeedyNote", "App");
            BackgroundStyle defaultStyle = static_cast<BackgroundStyle>(
                settings.value("defaultBackgroundStyle", static_cast<int>(BackgroundStyle::Grid)).toInt());
            QColor defaultColor = QColor(settings.value("defaultBackgroundColor", "#FFFFFF").toString());
            int defaultDensity = settings.value("defaultBackgroundDensity", 30).toInt();
            
            // Ensure valid values
            if (!defaultColor.isValid()) defaultColor = Qt::white;
            if (defaultDensity < 10) defaultDensity = 10;
            if (defaultDensity > 200) defaultDensity = 200;
            
            // Create the new .spn package with user's preferred background settings
            if (SpnPackageManager::createSpnPackageWithBackground(inputFile, notebookName, 
                                                                  defaultStyle, defaultColor, defaultDensity)) {
                // ✅ Notify Windows Explorer to refresh (fix file manager issue)
#ifdef Q_OS_WIN
                SHChangeNotify(SHCNE_CREATE, SHCNF_PATH, inputFile.toStdWString().c_str(), nullptr);
#endif
                return 0; // Exit successfully
            } else {
                return 1; // Exit with error code
            }
        }
        return 1; // Invalid file extension
    }

    // Check if another instance is already running
    if (MainWindow::isInstanceRunning()) {
        if (!inputFile.isEmpty()) {
            // Prepare command for existing instance
            QString command;
            if (createNewPackage) {
                command = QString("--create-new|%1").arg(inputFile);
            } else {
                command = inputFile;
            }
            
            // Send command to existing instance
            if (MainWindow::sendToExistingInstance(command)) {
                return 0; // Exit successfully, command sent to existing instance
            }
        }
        // If no command to send or sending failed, just exit
        return 0;
    }

    // Determine which window to show based on command line arguments
    if (!inputFile.isEmpty()) {
        // If a file is specified, go directly to MainWindow
        MainWindow w;
        if (createNewPackage) {
            // Handle --create-new command
            if (inputFile.toLower().endsWith(".spn")) {
                w.show(); // Show window first
                w.createNewSpnPackage(inputFile);
            } else {
                // Invalid file extension for new package
                w.show();
            }
        } else {
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
        }
        return app.exec();
    } else {
        // No file specified - show the launcher window
        // Create the launcher and set it as the shared instance
        LauncherWindow *launcher = new LauncherWindow();
        MainWindow::sharedLauncher = launcher; // Set as shared instance
        launcher->show();
        return app.exec();
    }
}
