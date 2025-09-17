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
#include <QFontDatabase>
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
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // ✅ Qt5: Additional settings for better fractional scaling support
    QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling, false);
    
    // Try to enable fractional scaling (Qt 5.14+)
    #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    #endif
    
    // Set scale factor rounding policy for better 125% support
    #if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
        qputenv("QT_SCALE_FACTOR_ROUNDING_POLICY", "PassThrough");
        qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
    #endif
    QApplication app(argc, argv);
    
    // ✅ Qt5: Modern font rendering and anti-aliasing setup
    #ifdef _WIN32
        // Enable modern font rendering on Windows - English fonts
        QFont::insertSubstitution("MS Shell Dlg 2", "Segoe UI");
        QFont::insertSubstitution("MS Shell Dlg", "Segoe UI");
        QFont::insertSubstitution("MS Sans Serif", "Segoe UI");
        
        // Enable font anti-aliasing and subpixel rendering
        QApplication::setDesktopSettingsAware(true);
        
        // Additional font rendering improvements
        qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");
        qputenv("QT_FONT_DPI", "96"); // Standard Windows DPI
        
        // ✅ Qt5: Chinese font rendering optimizations
        qputenv("QT_FONT_SUBSTITUTION", "1");
        qputenv("QT_ENABLE_GLYPH_CACHE_WORKAROUND", "1");
        
        // ✅ Qt5: Setup optimal Chinese font with fallback chain
        QStringList chineseFontPriority = {
            "Microsoft YaHei UI",    // Windows 10/11 modern Chinese font
            "Microsoft YaHei",       // Windows 7+ Chinese font
            "Segoe UI",              // Fallback to English font
            "Tahoma"                 // Final fallback
        };
        
        QString bestChineseFont = "Microsoft YaHei UI"; // Default
        QFontDatabase fontDb;
        QStringList availableFamilies = fontDb.families();
        
        // Find the best available Chinese font
        for (const QString &font : chineseFontPriority) {
            if (availableFamilies.contains(font)) {
                bestChineseFont = font;
                break;
            }
        }
        
        // Apply Chinese font substitutions with the best available font
        QFont::insertSubstitution("SimSun", bestChineseFont);
        QFont::insertSubstitution("NSimSun", bestChineseFont);
        QFont::insertSubstitution("FangSong", bestChineseFont);
        QFont::insertSubstitution("KaiTi", bestChineseFont);
        QFont::insertSubstitution("SimHei", bestChineseFont);
        QFont::insertSubstitution("MS Song", bestChineseFont);
        QFont::insertSubstitution("MS Hei", bestChineseFont);
        
        // Set up a mixed font for UI that handles both English and Chinese well
        QFont uiFont;
        if (bestChineseFont == "Microsoft YaHei UI" || bestChineseFont == "Microsoft YaHei") {
            // Use YaHei for everything if available (best for mixed content)
            uiFont = QFont(bestChineseFont, 9);
        } else {
            // Fallback: use Segoe UI for English, let Qt handle Chinese fallback
            uiFont = QFont("Segoe UI", 9);
        }
        uiFont.setStyleHint(QFont::SansSerif);
        uiFont.setStyleStrategy(static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::PreferQuality));
        app.setFont(uiFont);
    #endif
    
    // ✅ Qt5: Cross-platform font anti-aliasing
    QApplication::setAttribute(Qt::AA_UseDesktopOpenGL, true);
    
    // ✅ Qt5: Global text rendering improvements
    QApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents, false);
    QApplication::setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents, false);
    
    // Set global text anti-aliasing policy
    qputenv("QT_FONT_HINTING", "1");
    qputenv("QT_FONT_ANTIALIAS", "1");
    qputenv("QT_USE_NATIVE_WINDOWS", "1");
    
    // ✅ Qt5: CJK (Chinese/Japanese/Korean) font rendering improvements
    qputenv("QT_FONTCONFIG_TIMESTAMP", "1");
    qputenv("QT_HARFBUZZ", "old"); // Use Qt's text shaping for better CJK support
    qputenv("QT_ENABLE_REGEXP_JIT", "0"); // Disable for better CJK text processing
    
    // ✅ Qt5: Debug high DPI scaling information
    /*
    #ifdef QT_DEBUG
        qDebug() << "Qt5 High DPI Debug Info:";
        qDebug() << "  Qt version:" << QT_VERSION_STR;
        qDebug() << "  High DPI scaling enabled:" << QCoreApplication::testAttribute(Qt::AA_EnableHighDpiScaling);
        qDebug() << "  Device pixel ratio:" << app.devicePixelRatio();
        qDebug() << "  System font:" << app.font().toString();
        
        // Check all screens for their scale factors
        const auto screens = QGuiApplication::screens();
        for (int i = 0; i < screens.size(); ++i) {
            QScreen *screen = screens[i];
            qDebug() << "  Screen" << i << ":" << screen->name() 
                     << "DPR:" << screen->devicePixelRatio()
                     << "Logical DPI:" << screen->logicalDotsPerInch()
                     << "Physical DPI:" << screen->physicalDotsPerInch();
        }
    #endif
    */
    
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
