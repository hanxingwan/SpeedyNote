#include "MainWindow.h"
#include "InkCanvas.h"
#include "MarkdownWindowManager.h"
#include "ButtonMappingTypes.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScreen>
#include <QApplication>
#ifdef Q_OS_WIN
#include <windows.h>
#endif 
#include <QGuiApplication>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include "ToolType.h" // Include the header file where ToolType is defined
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QSpinBox>
#include <QTextStream>
#include <QInputDialog>
#include <QDial>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>
#include <QToolTip> // For manual tooltip display
#include <QWindow> // For tablet event safety checks
#include <QtConcurrent/QtConcurrentRun> // For concurrent saving
#include <QFuture>
#include <QFutureWatcher>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QSet>
#include <QWheelEvent>
#include <QTimer>
// #include "HandwritingLineEdit.h"
#include "ControlPanelDialog.h"
#include "SDLControllerManager.h"
#include "LauncherWindow.h" // Added for launcher access
#include "PdfOpenDialog.h" // Added for PDF file association
#include <poppler-qt6.h> // For PDF outline parsing
#include <memory> // For std::shared_ptr

// Linux-specific includes for signal handling
#ifdef Q_OS_LINUX
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <QProcess>
#endif

// Static member definition for single instance
QSharedMemory *MainWindow::sharedMemory = nullptr;
LauncherWindow *MainWindow::sharedLauncher = nullptr;

#ifdef Q_OS_LINUX
// Linux-specific signal handler for cleanup
void linuxSignalHandler(int signal) {
    Q_UNUSED(signal);
    
    // Only do minimal cleanup in signal handler to avoid Qt conflicts
    // The main cleanup will happen in the destructor
    if (MainWindow::sharedMemory && MainWindow::sharedMemory->isAttached()) {
        MainWindow::sharedMemory->detach();
    }
    
    // Remove local server
    QLocalServer::removeServer("SpeedyNote_SingleInstance");
    
    // Exit immediately - don't call QApplication::quit() from signal handler
    // as it can interfere with Qt's event system
    _exit(0);
}

// Function to setup Linux signal handlers
void setupLinuxSignalHandlers() {
    // Only handle SIGTERM and SIGINT, avoid SIGHUP as it can interfere with Qt
    signal(SIGTERM, linuxSignalHandler);
    signal(SIGINT, linuxSignalHandler);
}
#endif

MainWindow::MainWindow(QWidget *parent) 
    : QMainWindow(parent), benchmarking(false), localServer(nullptr) {

    setWindowTitle(tr("SpeedyNote Beta 0.10.2"));

#ifdef Q_OS_LINUX
    // Setup signal handlers for proper cleanup on Linux
    setupLinuxSignalHandlers();
#endif

    // Enable IME support for multi-language input
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setFocusPolicy(Qt::StrongFocus);
    
    // Initialize DPR early
    initialDpr = getDevicePixelRatio();

    // Initialize tooltip timer for pen hover throttling
    tooltipTimer = new QTimer(this);
    tooltipTimer->setSingleShot(true);
    tooltipTimer->setInterval(100); // 100ms delay
    lastHoveredWidget = nullptr;
    connect(tooltipTimer, &QTimer::timeout, this, &MainWindow::showPendingTooltip);

    // QString iconPath = QCoreApplication::applicationDirPath() + "/icon.ico";
    setWindowIcon(QIcon(":/resources/icons/mainicon.png"));
    

    // ✅ Get screen size & adjust window size
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QSize logicalSize = screen->availableGeometry().size() * 0.89;
        resize(logicalSize);
    }
    // ✅ Create a stacked widget to hold multiple canvases
    canvasStack = new QStackedWidget(this);
    setCentralWidget(canvasStack);

    // ✅ Create the first tab (default canvas)
    // addNewTab();
    QSettings settings("SpeedyNote", "App");
    pdfRenderDPI = settings.value("pdfRenderDPI", 192).toInt();
    setPdfDPI(pdfRenderDPI);
    
    setupUi();    // ✅ Move all UI setup here

    controllerManager = new SDLControllerManager();
    controllerThread = new QThread(this);

    controllerManager->moveToThread(controllerThread);
    
    // ✅ Initialize mouse dial control system
    mouseDialTimer = new QTimer(this);
    mouseDialTimer->setSingleShot(true);
    mouseDialTimer->setInterval(500); // 0.5 seconds
    connect(mouseDialTimer, &QTimer::timeout, this, [this]() {
        if (!pressedMouseButtons.isEmpty()) {
            startMouseDialMode(mouseButtonCombinationToString(pressedMouseButtons));
        }
    });
    connect(controllerThread, &QThread::started, controllerManager, &SDLControllerManager::start);
    connect(controllerThread, &QThread::finished, controllerManager, &SDLControllerManager::deleteLater);

    controllerThread->start();

    
    updateZoom(); // ✅ Keep this for initial zoom adjustment
    updatePanRange(); // Set initial slider range  HERE IS THE PROBLEM!!
    // toggleFullscreen(); // ✅ Toggle fullscreen to adjust layout
    // toggleDial(); // ✅ Toggle dial to adjust layout
   
    // zoomSlider->setValue(100 / initialDpr); // Set initial zoom level based on DPR
    // setColorButtonsVisible(false); // ✅ Show color buttons by default
    
    loadUserSettings();

    setBenchmarkControlsVisible(false);
    
    recentNotebooksManager = RecentNotebooksManager::getInstance(this); // Use singleton instance

    // Show dial by default after UI is fully initialized
    QTimer::singleShot(200, this, [this]() {
        if (!dialContainer) {
            toggleDial(); // This will create and show the dial
        }
    });
    
    // Force IME activation after a short delay to ensure proper initialization
    QTimer::singleShot(500, this, [this]() {
        QInputMethod *inputMethod = QGuiApplication::inputMethod();
        if (inputMethod) {
            inputMethod->show();
            inputMethod->reset();
        }
    });

    // Disable tablet tracking for now to prevent crashes
    // TODO: Find a safer way to implement hover tooltips without tablet tracking
    // QTimer::singleShot(100, this, [this]() {
    //     try {
    //         if (windowHandle() && windowHandle()->screen()) {
    //             setAttribute(Qt::WA_TabletTracking, true);
    //         }
    //     } catch (...) {
    //         // Silently ignore tablet tracking errors
    //     }
    // });
}


void MainWindow::setupUi() {
    
    // Ensure IME is properly enabled for the application
    QInputMethod *inputMethod = QGuiApplication::inputMethod();
    if (inputMethod) {
        inputMethod->show();
        inputMethod->reset();
    }
    
    // Create theme-aware button style
    bool darkMode = isDarkMode();
    QString buttonStyle = createButtonStyle(darkMode);


    loadPdfButton = new QPushButton(this);
    clearPdfButton = new QPushButton(this);
    loadPdfButton->setFixedSize(26, 30);
    clearPdfButton->setFixedSize(26, 30);
    QIcon pdfIcon(loadThemedIcon("pdf"));  // Path to your icon in resources
    QIcon pdfDeleteIcon(loadThemedIcon("pdfdelete"));  // Path to your icon in resources
    loadPdfButton->setIcon(pdfIcon);
    clearPdfButton->setIcon(pdfDeleteIcon);
    loadPdfButton->setStyleSheet(buttonStyle);
    clearPdfButton->setStyleSheet(buttonStyle);
    loadPdfButton->setToolTip(tr("Manage PDF"));
    clearPdfButton->setToolTip(tr("Clear PDF"));
    clearPdfButton->setVisible(false); // ✅ Hide clearPdfButton to save space
    connect(loadPdfButton, &QPushButton::clicked, this, &MainWindow::handleSmartPdfButton);
    connect(clearPdfButton, &QPushButton::clicked, this, &MainWindow::clearPdf);

    pdfTextSelectButton = new QPushButton(this);
    pdfTextSelectButton->setFixedSize(14, 30);
    QIcon pdfTextIcon(loadThemedIcon("ibeam"));
    pdfTextSelectButton->setIcon(pdfTextIcon);
    pdfTextSelectButton->setStyleSheet(buttonStyle);
    pdfTextSelectButton->setToolTip(tr("Toggle PDF Text Selection"));
    connect(pdfTextSelectButton, &QPushButton::clicked, this, [this]() {
        if (!currentCanvas()) return;
        
        bool newMode = !currentCanvas()->isPdfTextSelectionEnabled();
        currentCanvas()->setPdfTextSelectionEnabled(newMode);
        updatePdfTextSelectButtonState();
        updateBookmarkButtonState();
        
        // Clear any existing selection when toggling
        if (!newMode) {
            currentCanvas()->clearPdfTextSelection();
        }
    });



    benchmarkButton = new QPushButton(this);
    QIcon benchmarkIcon(loadThemedIcon("benchmark"));  // Path to your icon in resources
    benchmarkButton->setIcon(benchmarkIcon);
    benchmarkButton->setFixedSize(26, 30); // Make the benchmark button smaller
    benchmarkButton->setStyleSheet(buttonStyle);
    benchmarkButton->setToolTip(tr("Toggle Benchmark"));
    benchmarkLabel = new QLabel("PR:N/A", this);
    benchmarkLabel->setFixedHeight(30);  // Make the benchmark bar smaller

    toggleTabBarButton = new QPushButton(this);
    toggleTabBarButton->setIcon(loadThemedIcon("tabs"));  // You can design separate icons for "show" and "hide"
    toggleTabBarButton->setToolTip(tr("Show/Hide Tab Bar"));
    toggleTabBarButton->setFixedSize(26, 30);
    toggleTabBarButton->setStyleSheet(buttonStyle);
    toggleTabBarButton->setProperty("selected", true); // Initially visible
    
    // PDF Outline Toggle Button
    toggleOutlineButton = new QPushButton(this);
    toggleOutlineButton->setIcon(loadThemedIcon("outline"));  // Icon to be added later
    toggleOutlineButton->setToolTip(tr("Show/Hide PDF Outline"));
    toggleOutlineButton->setFixedSize(26, 30);
    toggleOutlineButton->setStyleSheet(buttonStyle);
    toggleOutlineButton->setProperty("selected", false); // Initially hidden
    
    // Bookmarks Toggle Button
    toggleBookmarksButton = new QPushButton(this);
    toggleBookmarksButton->setIcon(loadThemedIcon("bookmark"));  // Using bookpage icon for bookmarks
    toggleBookmarksButton->setToolTip(tr("Show/Hide Bookmarks"));
    toggleBookmarksButton->setFixedSize(26, 30);
    toggleBookmarksButton->setStyleSheet(buttonStyle);
    toggleBookmarksButton->setProperty("selected", false); // Initially hidden
    
    // Add/Remove Bookmark Toggle Button
    toggleBookmarkButton = new QPushButton(this);
    toggleBookmarkButton->setIcon(loadThemedIcon("star"));
    toggleBookmarkButton->setToolTip(tr("Add/Remove Bookmark"));
    toggleBookmarkButton->setFixedSize(26, 30);
    toggleBookmarkButton->setStyleSheet(buttonStyle);
    toggleBookmarkButton->setProperty("selected", false); // For toggle state styling

    // Touch Gestures Toggle Button
    touchGesturesButton = new QPushButton(this);
    touchGesturesButton->setIcon(loadThemedIcon("hand"));  // Using hand icon for touch/gesture
    touchGesturesButton->setToolTip(tr("Toggle Touch Gestures"));
    touchGesturesButton->setFixedSize(26, 30);
    touchGesturesButton->setStyleSheet(buttonStyle);
    touchGesturesButton->setProperty("selected", touchGesturesEnabled); // For toggle state styling

    selectFolderButton = new QPushButton(this);
    selectFolderButton->setFixedSize(26, 30);
    QIcon folderIcon(loadThemedIcon("folder"));  // Path to your icon in resources
    selectFolderButton->setIcon(folderIcon);
    selectFolderButton->setStyleSheet(buttonStyle);
    selectFolderButton->setToolTip(tr("Select Save Folder"));
    selectFolderButton->setVisible(false); // ✅ Hide deprecated folder selection button
    connect(selectFolderButton, &QPushButton::clicked, this, &MainWindow::selectFolder);
    
    
    saveButton = new QPushButton(this);
    saveButton->setFixedSize(26, 30);
    QIcon saveIcon(loadThemedIcon("save"));  // Path to your icon in resources
    saveButton->setIcon(saveIcon);
    saveButton->setStyleSheet(buttonStyle);
    saveButton->setToolTip(tr("Save Notebook"));
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveCurrentPage);
    
    saveAnnotatedButton = new QPushButton(this);
    saveAnnotatedButton->setFixedSize(26, 30);
    QIcon saveAnnotatedIcon(loadThemedIcon("saveannotated"));  // Path to your icon in resources
    saveAnnotatedButton->setIcon(saveAnnotatedIcon);
    saveAnnotatedButton->setStyleSheet(buttonStyle);
    saveAnnotatedButton->setToolTip(tr("Save Page with Background"));
    connect(saveAnnotatedButton, &QPushButton::clicked, this, &MainWindow::saveAnnotated);

    fullscreenButton = new QPushButton(this);
    fullscreenButton->setIcon(loadThemedIcon("fullscreen"));  // Load from resources
    fullscreenButton->setFixedSize(26, 30);
    fullscreenButton->setToolTip(tr("Toggle Fullscreen"));
    fullscreenButton->setStyleSheet(buttonStyle);

    // ✅ Connect button click to toggleFullscreen() function
    connect(fullscreenButton, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);

    // Use the darkMode variable already declared at the beginning of setupUi()

    redButton = new QPushButton(this);
    redButton->setFixedSize(16, 30);  // Half width
    QString redIconPath = darkMode ? ":/resources/icons/pen_light_red.png" : ":/resources/icons/pen_dark_red.png";
    QIcon redIcon(redIconPath);
    redButton->setIcon(redIcon);
    redButton->setStyleSheet(buttonStyle);
    connect(redButton, &QPushButton::clicked, [this]() { 
        handleColorButtonClick();
        currentCanvas()->setPenColor(getPaletteColor("red")); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });
    
    blueButton = new QPushButton(this);
    blueButton->setFixedSize(16, 30);  // Half width
    QString blueIconPath = darkMode ? ":/resources/icons/pen_light_blue.png" : ":/resources/icons/pen_dark_blue.png";
    QIcon blueIcon(blueIconPath);
    blueButton->setIcon(blueIcon);
    blueButton->setStyleSheet(buttonStyle);
    connect(blueButton, &QPushButton::clicked, [this]() { 
        handleColorButtonClick();
        currentCanvas()->setPenColor(getPaletteColor("blue")); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });

    yellowButton = new QPushButton(this);
    yellowButton->setFixedSize(16, 30);  // Half width
    QString yellowIconPath = darkMode ? ":/resources/icons/pen_light_yellow.png" : ":/resources/icons/pen_dark_yellow.png";
    QIcon yellowIcon(yellowIconPath);
    yellowButton->setIcon(yellowIcon);
    yellowButton->setStyleSheet(buttonStyle);
    connect(yellowButton, &QPushButton::clicked, [this]() { 
        handleColorButtonClick();
        currentCanvas()->setPenColor(getPaletteColor("yellow")); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });

    greenButton = new QPushButton(this);
    greenButton->setFixedSize(16, 30);  // Half width
    QString greenIconPath = darkMode ? ":/resources/icons/pen_light_green.png" : ":/resources/icons/pen_dark_green.png";
    QIcon greenIcon(greenIconPath);
    greenButton->setIcon(greenIcon);
    greenButton->setStyleSheet(buttonStyle);
    connect(greenButton, &QPushButton::clicked, [this]() { 
        handleColorButtonClick();
        currentCanvas()->setPenColor(getPaletteColor("green")); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });

    blackButton = new QPushButton(this);
    blackButton->setFixedSize(16, 30);  // Half width
    QString blackIconPath = darkMode ? ":/resources/icons/pen_light_black.png" : ":/resources/icons/pen_dark_black.png";
    QIcon blackIcon(blackIconPath);
    blackButton->setIcon(blackIcon);
    blackButton->setStyleSheet(buttonStyle);
    connect(blackButton, &QPushButton::clicked, [this]() { 
        handleColorButtonClick();
        currentCanvas()->setPenColor(QColor("#000000")); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });

    whiteButton = new QPushButton(this);
    whiteButton->setFixedSize(16, 30);  // Half width
    QString whiteIconPath = darkMode ? ":/resources/icons/pen_light_white.png" : ":/resources/icons/pen_dark_white.png";
    QIcon whiteIcon(whiteIconPath);
    whiteButton->setIcon(whiteIcon);
    whiteButton->setStyleSheet(buttonStyle);
    connect(whiteButton, &QPushButton::clicked, [this]() { 
        handleColorButtonClick();
        currentCanvas()->setPenColor(QColor("#FFFFFF")); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });
    
    customColorInput = new QLineEdit(this);
    customColorInput->setPlaceholderText("Custom HEX");
    customColorInput->setFixedSize(85, 30);
    
    // Enable IME support for multi-language input
    customColorInput->setAttribute(Qt::WA_InputMethodEnabled, true);
    customColorInput->setInputMethodHints(Qt::ImhNone); // Allow all input methods
    customColorInput->installEventFilter(this); // Install event filter for IME handling
    
    connect(customColorInput, &QLineEdit::returnPressed, this, &MainWindow::applyCustomColor);

    
    thicknessButton = new QPushButton(this);
    thicknessButton->setIcon(loadThemedIcon("thickness"));
    thicknessButton->setFixedSize(26, 30);
    thicknessButton->setStyleSheet(buttonStyle);
    connect(thicknessButton, &QPushButton::clicked, this, &MainWindow::toggleThicknessSlider);

    thicknessFrame = new QFrame(this);
    thicknessFrame->setFrameShape(QFrame::StyledPanel);
    thicknessFrame->setStyleSheet(R"(
        background-color: black;
        border: 1px solid black;
        padding: 5px;
    )");
    thicknessFrame->setVisible(false);
    thicknessFrame->setFixedSize(220, 40); // Adjust width/height as needed

    thicknessSlider = new QSlider(Qt::Horizontal, this);
    thicknessSlider->setRange(1, 50);
    thicknessSlider->setValue(5);
    thicknessSlider->setMaximumWidth(200);


    connect(thicknessSlider, &QSlider::valueChanged, this, &MainWindow::updateThickness);

    QVBoxLayout *popupLayoutThickness = new QVBoxLayout();
    popupLayoutThickness->setContentsMargins(10, 5, 10, 5);
    popupLayoutThickness->addWidget(thicknessSlider);
    thicknessFrame->setLayout(popupLayoutThickness);


    toolSelector = new QComboBox(this);
    toolSelector->addItem(loadThemedIcon("pen"), "");
    toolSelector->addItem(loadThemedIcon("marker"), "");
    toolSelector->addItem(loadThemedIcon("eraser"), "");
    toolSelector->setFixedWidth(43);
    toolSelector->setFixedHeight(30);
    connect(toolSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::changeTool);

    // ✅ Individual tool buttons
    penToolButton = new QPushButton(this);
    penToolButton->setFixedSize(26, 30);
    penToolButton->setIcon(loadThemedIcon("pen"));
    penToolButton->setStyleSheet(buttonStyle);
    penToolButton->setToolTip(tr("Pen Tool"));
    connect(penToolButton, &QPushButton::clicked, this, &MainWindow::setPenTool);

    markerToolButton = new QPushButton(this);
    markerToolButton->setFixedSize(26, 30);
    markerToolButton->setIcon(loadThemedIcon("marker"));
    markerToolButton->setStyleSheet(buttonStyle);
    markerToolButton->setToolTip(tr("Marker Tool"));
    connect(markerToolButton, &QPushButton::clicked, this, &MainWindow::setMarkerTool);

    eraserToolButton = new QPushButton(this);
    eraserToolButton->setFixedSize(26, 30);
    eraserToolButton->setIcon(loadThemedIcon("eraser"));
    eraserToolButton->setStyleSheet(buttonStyle);
    eraserToolButton->setToolTip(tr("Eraser Tool"));
    connect(eraserToolButton, &QPushButton::clicked, this, &MainWindow::setEraserTool);

    backgroundButton = new QPushButton(this);
    backgroundButton->setFixedSize(26, 30);
    QIcon bgIcon(loadThemedIcon("background"));  // Path to your icon in resources
    backgroundButton->setIcon(bgIcon);
    backgroundButton->setStyleSheet(buttonStyle);
    backgroundButton->setToolTip(tr("Set Background Pic"));
    connect(backgroundButton, &QPushButton::clicked, this, &MainWindow::selectBackground);

    // Initialize straight line toggle button
    straightLineToggleButton = new QPushButton(this);
    straightLineToggleButton->setFixedSize(26, 30);
    QIcon straightLineIcon(loadThemedIcon("straightLine"));  // Make sure this icon exists or use a different one
    straightLineToggleButton->setIcon(straightLineIcon);
    straightLineToggleButton->setStyleSheet(buttonStyle);
    straightLineToggleButton->setToolTip(tr("Toggle Straight Line Mode"));
    connect(straightLineToggleButton, &QPushButton::clicked, this, [this]() {
        if (!currentCanvas()) return;
        
        // If we're turning on straight line mode, first disable rope tool
        if (!currentCanvas()->isStraightLineMode()) {
            currentCanvas()->setRopeToolMode(false);
            updateRopeToolButtonState();
        }
        
        bool newMode = !currentCanvas()->isStraightLineMode();
        currentCanvas()->setStraightLineMode(newMode);
        updateStraightLineButtonState();
    });
    
    ropeToolButton = new QPushButton(this);
    ropeToolButton->setFixedSize(26, 30);
    QIcon ropeToolIcon(loadThemedIcon("rope")); // Make sure this icon exists
    ropeToolButton->setIcon(ropeToolIcon);
    ropeToolButton->setStyleSheet(buttonStyle);
    ropeToolButton->setToolTip(tr("Toggle Rope Tool Mode"));
    connect(ropeToolButton, &QPushButton::clicked, this, [this]() {
        if (!currentCanvas()) return;
        
        // If we're turning on rope tool mode, first disable straight line
        if (!currentCanvas()->isRopeToolMode()) {
            currentCanvas()->setStraightLineMode(false);
            updateStraightLineButtonState();
        }
        
        bool newMode = !currentCanvas()->isRopeToolMode();
        currentCanvas()->setRopeToolMode(newMode);
        updateRopeToolButtonState();
    });
    
    markdownButton = new QPushButton(this);
    markdownButton->setFixedSize(26, 30);
    QIcon markdownIcon(loadThemedIcon("markdown")); // Using text icon for markdown
    markdownButton->setIcon(markdownIcon);
    markdownButton->setStyleSheet(buttonStyle);
    markdownButton->setToolTip(tr("Add Markdown Window"));
    connect(markdownButton, &QPushButton::clicked, this, [this]() {
        if (!currentCanvas()) return;
        
        // Toggle markdown selection mode
        bool newMode = !currentCanvas()->isMarkdownSelectionMode();
        currentCanvas()->setMarkdownSelectionMode(newMode);
        updateMarkdownButtonState();
    });
    
    // Insert Picture Button
    insertPictureButton = new QPushButton(this);
    insertPictureButton->setFixedSize(26, 30);
    QIcon pictureIcon(loadThemedIcon("background")); // Using import icon as placeholder for now
    insertPictureButton->setIcon(pictureIcon);
    insertPictureButton->setStyleSheet(buttonStyle);
    insertPictureButton->setToolTip(tr("Insert Picture"));
    connect(insertPictureButton, &QPushButton::clicked, this, [this]() {
        // qDebug() << "MainWindow: Insert Picture button clicked!";
        
        if (!currentCanvas()) {
            // qDebug() << "  ERROR: No current canvas!";
            return;
        }
        
        // Toggle picture selection mode
        bool currentMode = currentCanvas()->isPictureSelectionMode();
        bool newMode = !currentMode;
        // qDebug() << "  Current picture selection mode:" << currentMode;
        // qDebug() << "  New picture selection mode:" << newMode;
        
        currentCanvas()->setPictureSelectionMode(newMode);
        updatePictureButtonState();
        // qDebug() << "  Picture button state updated";
    });
    
    deletePageButton = new QPushButton(this);
    deletePageButton->setFixedSize(22, 30);
    QIcon trashIcon(loadThemedIcon("trash"));  // Path to your icon in resources
    deletePageButton->setIcon(trashIcon);
    deletePageButton->setStyleSheet(buttonStyle);
    deletePageButton->setToolTip(tr("Clear All Content"));
    connect(deletePageButton, &QPushButton::clicked, this, &MainWindow::deleteCurrentPage);

    zoomButton = new QPushButton(this);
    zoomButton->setIcon(loadThemedIcon("zoom"));
    zoomButton->setFixedSize(26, 30);
    zoomButton->setStyleSheet(buttonStyle);
    connect(zoomButton, &QPushButton::clicked, this, &MainWindow::toggleZoomSlider);

    // ✅ Create the floating frame (Initially Hidden)
    zoomFrame = new QFrame(this);
    zoomFrame->setFrameShape(QFrame::StyledPanel);
    zoomFrame->setStyleSheet(R"(
        background-color: black;
        border: 1px solid black;
        padding: 5px;
    )");
    zoomFrame->setVisible(false);
    zoomFrame->setFixedSize(440, 40); // Adjust width/height as needed

    zoomSlider = new QSlider(Qt::Horizontal, this);
    zoomSlider->setRange(10, 400);
    zoomSlider->setValue(100);
    zoomSlider->setMaximumWidth(405);

    connect(zoomSlider, &QSlider::valueChanged, this, &MainWindow::onZoomSliderChanged);

    QVBoxLayout *popupLayout = new QVBoxLayout();
    popupLayout->setContentsMargins(10, 5, 10, 5);
    popupLayout->addWidget(zoomSlider);
    zoomFrame->setLayout(popupLayout);
  

    zoom50Button = new QPushButton("0.5x", this);
    zoom50Button->setFixedSize(35, 30);
    zoom50Button->setStyleSheet(buttonStyle);
    zoom50Button->setToolTip(tr("Set Zoom to 50%"));
    connect(zoom50Button, &QPushButton::clicked, [this]() { zoomSlider->setValue(50 / initialDpr); updateDialDisplay(); });

    dezoomButton = new QPushButton("1x", this);
    dezoomButton->setFixedSize(26, 30);
    dezoomButton->setStyleSheet(buttonStyle);
    dezoomButton->setToolTip(tr("Set Zoom to 100%"));
    connect(dezoomButton, &QPushButton::clicked, [this]() { zoomSlider->setValue(100 / initialDpr); updateDialDisplay(); });

    zoom200Button = new QPushButton("2x", this);
    zoom200Button->setFixedSize(31, 30);
    zoom200Button->setStyleSheet(buttonStyle);
    zoom200Button->setToolTip(tr("Set Zoom to 200%"));
    connect(zoom200Button, &QPushButton::clicked, [this]() { zoomSlider->setValue(200 / initialDpr); updateDialDisplay(); });

    panXSlider = new QScrollBar(Qt::Horizontal, this);
    panYSlider = new QScrollBar(Qt::Vertical, this);
    panYSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    
    // Set scrollbar styling
    QString scrollBarStyle = R"(
        QScrollBar {
            background: rgba(200, 200, 200, 80);
            border: none;
            margin: 0px;
        }
        QScrollBar:hover {
            background: rgba(200, 200, 200, 120);
        }
        QScrollBar:horizontal {
            height: 16px !important;  /* Force narrow height */
            max-height: 16px !important;
        }
        QScrollBar:vertical {
            width: 16px !important;   /* Force narrow width */
            max-width: 16px !important;
        }
        QScrollBar::handle {
            background: rgba(100, 100, 100, 150);
            border-radius: 2px;
            min-height: 120px;  /* Longer handle for vertical scrollbar */
            min-width: 120px;   /* Longer handle for horizontal scrollbar */
        }
        QScrollBar::handle:hover {
            background: rgba(80, 80, 80, 210);
        }
        /* Hide scroll buttons */
        QScrollBar::add-line, 
        QScrollBar::sub-line {
            width: 0px;
            height: 0px;
            background: none;
            border: none;
        }
        /* Disable scroll page buttons */
        QScrollBar::add-page, 
        QScrollBar::sub-page {
            background: transparent;
        }
    )";
    
    panXSlider->setStyleSheet(scrollBarStyle);
    panYSlider->setStyleSheet(scrollBarStyle);
    
    // Force fixed dimensions programmatically
    panXSlider->setFixedHeight(16);
    panYSlider->setFixedWidth(16);
    
    // Set up auto-hiding
    panXSlider->setMouseTracking(true);
    panYSlider->setMouseTracking(true);
    panXSlider->installEventFilter(this);
    panYSlider->installEventFilter(this);
    panXSlider->setVisible(false);
    panYSlider->setVisible(false);
    
    // Create timer for auto-hiding
    scrollbarHideTimer = new QTimer(this);
    scrollbarHideTimer->setSingleShot(true);
    scrollbarHideTimer->setInterval(200); // Hide after 0.2 seconds
    connect(scrollbarHideTimer, &QTimer::timeout, this, [this]() {
        panXSlider->setVisible(false);
        panYSlider->setVisible(false);
        scrollbarsVisible = false;
    });
    
    // panXSlider->setFixedHeight(30);
    // panYSlider->setFixedWidth(30);

    connect(panXSlider, &QScrollBar::valueChanged, this, &MainWindow::updatePanX);
    
    connect(panYSlider, &QScrollBar::valueChanged, this, &MainWindow::updatePanY);




    // 🌟 PDF Outline Sidebar
    outlineSidebar = new QWidget(this);
    outlineSidebar->setFixedWidth(250);
    outlineSidebar->setVisible(false); // Hidden by default
    
    QVBoxLayout *outlineLayout = new QVBoxLayout(outlineSidebar);
    outlineLayout->setContentsMargins(5, 5, 5, 5);
    
    QLabel *outlineLabel = new QLabel(tr("PDF Outline"), outlineSidebar);
    outlineLabel->setStyleSheet("font-weight: bold; padding: 5px;");
    outlineLayout->addWidget(outlineLabel);
    
    outlineTree = new QTreeWidget(outlineSidebar);
    outlineTree->setHeaderHidden(true);
    outlineTree->setRootIsDecorated(true);
    outlineTree->setIndentation(15);
    outlineLayout->addWidget(outlineTree);
    
    // Connect outline tree item clicks
    connect(outlineTree, &QTreeWidget::itemClicked, this, &MainWindow::onOutlineItemClicked);
    
    // 🌟 Bookmarks Sidebar
    bookmarksSidebar = new QWidget(this);
    bookmarksSidebar->setFixedWidth(250);
    bookmarksSidebar->setVisible(false); // Hidden by default
    QVBoxLayout *bookmarksLayout = new QVBoxLayout(bookmarksSidebar);
    bookmarksLayout->setContentsMargins(5, 5, 5, 5);
    QLabel *bookmarksLabel = new QLabel(tr("Bookmarks"), bookmarksSidebar);
    bookmarksLabel->setStyleSheet("font-weight: bold; padding: 5px;");
    bookmarksLayout->addWidget(bookmarksLabel);
    bookmarksTree = new QTreeWidget(bookmarksSidebar);
    bookmarksTree->setHeaderHidden(true);
    bookmarksTree->setRootIsDecorated(false);
    bookmarksTree->setIndentation(0);
    bookmarksLayout->addWidget(bookmarksTree);
    // Connect bookmarks tree item clicks
    connect(bookmarksTree, &QTreeWidget::itemClicked, this, &MainWindow::onBookmarkItemClicked);
    // 🌟 Horizontal Tab Bar (like modern web browsers)
    tabList = new QListWidget(this);
    tabList->setFlow(QListView::LeftToRight);  // Make it horizontal
    tabList->setFixedHeight(32);  // Increased to accommodate scrollbar below tabs
    tabList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tabList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabList->setSelectionMode(QAbstractItemView::SingleSelection);
    // Style the tab bar like a modern browser with transparent scrollbar
    tabList->setStyleSheet(R"(
        QListWidget {
            background-color: rgba(240, 240, 240, 255);
            border: none;
            border-bottom: 1px solid rgba(200, 200, 200, 255);
            outline: none;
        }
        QListWidget::item {
            background-color: rgba(220, 220, 220, 255);
            border: 1px solid rgba(180, 180, 180, 255);
            border-bottom: none;
            margin-right: 1px;
            margin-top: 2px;
            padding: 0px;
            min-width: 80px;
            max-width: 120px;
        }
        QListWidget::item:selected {
            background-color: white;
            border: 1px solid rgba(180, 180, 180, 255);
            border-bottom: 1px solid white;
            margin-top: 1px;
        }
        QListWidget::item:hover:!selected {
            background-color: rgba(230, 230, 230, 255);
        }
        QScrollBar:horizontal {
            background: rgba(240, 240, 240, 255);
            height: 8px;
            border: none;
            margin: 0px;
            border-top: 1px solid rgba(200, 200, 200, 255);
        }
        QScrollBar::handle:horizontal {
            background: rgba(150, 150, 150, 120);
            border-radius: 4px;
            min-width: 20px;
            margin: 1px;
        }
        QScrollBar::handle:horizontal:hover {
            background: rgba(120, 120, 120, 200);
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
            height: 0px;
            background: none;
            border: none;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
        }
    )");

    // 🌟 Add Button for New Tab (styled like browser + button)
    addTabButton = new QPushButton(this);
    QIcon addTab(loadThemedIcon("addtab"));  // Path to your icon in resources
    addTabButton->setIcon(addTab);
    addTabButton->setFixedSize(30, 30);  // Even smaller to match thinner layout
    addTabButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(220, 220, 220, 255);
            border: 1px solid rgba(180, 180, 180, 255);
            border-radius: 15px;
            margin: 2px;
        }
        QPushButton:hover {
            background-color: rgba(200, 200, 200, 255);
        }
        QPushButton:pressed {
            background-color: rgba(180, 180, 180, 255);
        }
    )");
    addTabButton->setToolTip(tr("Add New Tab"));
    connect(addTabButton, &QPushButton::clicked, this, &MainWindow::addNewTab);

    if (!canvasStack) {
        canvasStack = new QStackedWidget(this);
    }

    connect(tabList, &QListWidget::currentRowChanged, this, &MainWindow::switchTab);

    // Create horizontal tab container
    tabBarContainer = new QWidget(this);
    tabBarContainer->setObjectName("tabBarContainer");
    tabBarContainer->setFixedHeight(38);  // Increased to accommodate scrollbar below tabs
    
    QHBoxLayout *tabBarLayout = new QHBoxLayout(tabBarContainer);
    tabBarLayout->setContentsMargins(5, 5, 5, 5);
    tabBarLayout->setSpacing(5);
    tabBarLayout->addWidget(tabList, 1);  // Tab list takes most space
    tabBarLayout->addWidget(addTabButton, 0);  // Add button stays at the end

    connect(toggleTabBarButton, &QPushButton::clicked, this, [=]() {
        bool isVisible = tabBarContainer->isVisible();
        tabBarContainer->setVisible(!isVisible);
        
        // Update button toggle state
        toggleTabBarButton->setProperty("selected", !isVisible);
        toggleTabBarButton->style()->unpolish(toggleTabBarButton);
        toggleTabBarButton->style()->polish(toggleTabBarButton);

        QTimer::singleShot(0, this, [this]() {
            if (auto *canvas = currentCanvas()) {
                canvas->setMaximumSize(canvas->getCanvasSize());
            }
        });
    });
    
    connect(toggleOutlineButton, &QPushButton::clicked, this, &MainWindow::toggleOutlineSidebar);
    connect(toggleBookmarksButton, &QPushButton::clicked, this, &MainWindow::toggleBookmarksSidebar);
    connect(toggleBookmarkButton, &QPushButton::clicked, this, &MainWindow::toggleCurrentPageBookmark);
    connect(touchGesturesButton, &QPushButton::clicked, this, [this]() {
        setTouchGesturesEnabled(!touchGesturesEnabled);
        touchGesturesButton->setProperty("selected", touchGesturesEnabled);
        touchGesturesButton->style()->unpolish(touchGesturesButton);
        touchGesturesButton->style()->polish(touchGesturesButton);
    });

    


    // Previous page button
    prevPageButton = new QPushButton(this);
    prevPageButton->setFixedSize(24, 30);
    prevPageButton->setText("◀");
    prevPageButton->setStyleSheet(buttonStyle);
    prevPageButton->setToolTip(tr("Previous Page"));
    connect(prevPageButton, &QPushButton::clicked, this, &MainWindow::goToPreviousPage);

    pageInput = new QSpinBox(this);
    pageInput->setFixedSize(36, 30);
    pageInput->setMinimum(1);
    pageInput->setMaximum(9999);
    pageInput->setValue(1);
    pageInput->setMaximumWidth(100);
    connect(pageInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onPageInputChanged);

    // Next page button
    nextPageButton = new QPushButton(this);
    nextPageButton->setFixedSize(24, 30);
    nextPageButton->setText("▶");
    nextPageButton->setStyleSheet(buttonStyle);
    nextPageButton->setToolTip(tr("Next Page"));
    connect(nextPageButton, &QPushButton::clicked, this, &MainWindow::goToNextPage);

    jumpToPageButton = new QPushButton(this);
    // QIcon jumpIcon(":/resources/icons/bookpage.png");  // Path to your icon in resources
    jumpToPageButton->setFixedSize(26, 30);
    jumpToPageButton->setStyleSheet(buttonStyle);
    jumpToPageButton->setIcon(loadThemedIcon("bookpage"));
    connect(jumpToPageButton, &QPushButton::clicked, this, &MainWindow::showJumpToPageDialog);

    // ✅ Dial Toggle Button
    dialToggleButton = new QPushButton(this);
    dialToggleButton->setIcon(loadThemedIcon("dial"));  // Icon for dial
    dialToggleButton->setFixedSize(26, 30);
    dialToggleButton->setToolTip(tr("Toggle Magic Dial"));
    dialToggleButton->setStyleSheet(buttonStyle);

    // ✅ Connect to toggle function
    connect(dialToggleButton, &QPushButton::clicked, this, &MainWindow::toggleDial);

    // toggleDial();

    

    fastForwardButton = new QPushButton(this);
    fastForwardButton->setFixedSize(26, 30);
    // QIcon ffIcon(":/resources/icons/fastforward.png");  // Path to your icon in resources
    fastForwardButton->setIcon(loadThemedIcon("fastforward"));
    fastForwardButton->setToolTip(tr("Toggle Fast Forward 8x"));
    fastForwardButton->setStyleSheet(buttonStyle);

    // ✅ Toggle fast-forward mode
    connect(fastForwardButton, &QPushButton::clicked, [this]() {
        fastForwardMode = !fastForwardMode;
        updateFastForwardButtonState();
    });

    QComboBox *dialModeSelector = new QComboBox(this);
    dialModeSelector->addItem("Page Switch", PageSwitching);
    dialModeSelector->addItem("Zoom", ZoomControl);
    dialModeSelector->addItem("Thickness", ThicknessControl);

    dialModeSelector->addItem("Tool Switch", ToolSwitching);
    dialModeSelector->setFixedWidth(120);

    connect(dialModeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) { changeDialMode(static_cast<DialMode>(index)); });



    colorPreview = new QPushButton(this);
    colorPreview->setFixedSize(26, 30);
    colorPreview->setStyleSheet("border-radius: 15px; border: 1px solid gray;");
    colorPreview->setEnabled(false);  // ✅ Prevents it from being clicked

    btnPageSwitch = new QPushButton(loadThemedIcon("bookpage"), "", this);
    btnPageSwitch->setStyleSheet(buttonStyle);
    btnPageSwitch->setFixedSize(26, 30);
    btnPageSwitch->setToolTip(tr("Set Dial Mode to Page Switching"));
    btnZoom = new QPushButton(loadThemedIcon("zoom"), "", this);
    btnZoom->setStyleSheet(buttonStyle);
    btnZoom->setFixedSize(26, 30);
    btnZoom->setToolTip(tr("Set Dial Mode to Zoom Ctrl"));
    btnThickness = new QPushButton(loadThemedIcon("thickness"), "", this);
    btnThickness->setStyleSheet(buttonStyle);
    btnThickness->setFixedSize(26, 30);
    btnThickness->setToolTip(tr("Set Dial Mode to Pen Tip Thickness Ctrl"));

    btnTool = new QPushButton(loadThemedIcon("pen"), "", this);
    btnTool->setStyleSheet(buttonStyle);
    btnTool->setFixedSize(26, 30);
    btnTool->setToolTip(tr("Set Dial Mode to Tool Switching"));
    btnPresets = new QPushButton(loadThemedIcon("preset"), "", this);
    btnPresets->setStyleSheet(buttonStyle);
    btnPresets->setFixedSize(26, 30);
    btnPresets->setToolTip(tr("Set Dial Mode to Color Preset Selection"));
    btnPannScroll = new QPushButton(loadThemedIcon("scroll"), "", this);
    btnPannScroll->setStyleSheet(buttonStyle);
    btnPannScroll->setFixedSize(26, 30);
    btnPannScroll->setToolTip(tr("Slide and turn pages with the dial"));

    connect(btnPageSwitch, &QPushButton::clicked, this, [this]() { changeDialMode(PageSwitching); });
    connect(btnZoom, &QPushButton::clicked, this, [this]() { changeDialMode(ZoomControl); });
    connect(btnThickness, &QPushButton::clicked, this, [this]() { changeDialMode(ThicknessControl); });

    connect(btnTool, &QPushButton::clicked, this, [this]() { changeDialMode(ToolSwitching); });
    connect(btnPresets, &QPushButton::clicked, this, [this]() { changeDialMode(PresetSelection); }); 
    connect(btnPannScroll, &QPushButton::clicked, this, [this]() { changeDialMode(PanAndPageScroll); });


    // ✅ Initialize color presets based on palette mode (will be updated after UI setup)
    colorPresets.enqueue(getDefaultPenColor());
    colorPresets.enqueue(QColor("#AA0000"));  // Temporary - will be updated later
    colorPresets.enqueue(QColor("#997700"));
    colorPresets.enqueue(QColor("#0000AA"));
    colorPresets.enqueue(QColor("#007700"));
    colorPresets.enqueue(QColor("#000000"));
    colorPresets.enqueue(QColor("#FFFFFF"));

    // ✅ Button to add current color to presets
    addPresetButton = new QPushButton(loadThemedIcon("savepreset"), "", this);
    addPresetButton->setStyleSheet(buttonStyle);
    addPresetButton->setToolTip(tr("Add Current Color to Presets"));
    addPresetButton->setFixedSize(26, 30);
    connect(addPresetButton, &QPushButton::clicked, this, &MainWindow::addColorPreset);




    openControlPanelButton = new QPushButton(this);
    openControlPanelButton->setIcon(loadThemedIcon("settings"));  // Replace with your actual settings icon
    openControlPanelButton->setStyleSheet(buttonStyle);
    openControlPanelButton->setToolTip(tr("Open Control Panel"));
    openControlPanelButton->setFixedSize(26, 30);  // Adjust to match your other buttons

    connect(openControlPanelButton, &QPushButton::clicked, this, [=]() {
        InkCanvas *canvas = currentCanvas();
        if (canvas) {
            ControlPanelDialog dialog(this, canvas, this);
            dialog.exec();  // Modal
        }
    });

    openRecentNotebooksButton = new QPushButton(this); // Create button
    openRecentNotebooksButton->setIcon(loadThemedIcon("recent")); // Replace with actual icon if available
    openRecentNotebooksButton->setStyleSheet(buttonStyle);
    openRecentNotebooksButton->setToolTip(tr("Return to Launcher"));
    openRecentNotebooksButton->setFixedSize(26, 30);
    connect(openRecentNotebooksButton, &QPushButton::clicked, this, &MainWindow::returnToLauncher);

    customColorButton = new QPushButton(this);
    customColorButton->setFixedSize(62, 30);
    QColor initialColor = getDefaultPenColor();  // Theme-aware default color
    customColorButton->setText(initialColor.name().toUpper());

    if (currentCanvas()) {
        initialColor = currentCanvas()->getPenColor();
    }

    updateCustomColorButtonStyle(initialColor);

    QTimer::singleShot(0, this, [=]() {
        connect(customColorButton, &QPushButton::clicked, this, [=]() {
            if (!currentCanvas()) return;
            
            handleColorButtonClick();
            
            // Get the current custom color from the button text
            QString buttonText = customColorButton->text();
            QColor customColor(buttonText);
            
            // Check if the custom color is already the current pen color
            if (currentCanvas()->getPenColor() == customColor) {
                // Second click - show color picker dialog
            QColor chosen = QColorDialog::getColor(currentCanvas()->getPenColor(), this, "Select Pen Color");
            if (chosen.isValid()) {
                currentCanvas()->setPenColor(chosen);
                    updateCustomColorButtonStyle(chosen);
                updateDialDisplay();
                    updateColorButtonStates();
                }
            } else {
                // First click - apply the custom color
                currentCanvas()->setPenColor(customColor);
                updateDialDisplay();
                updateColorButtonStates();
            }
        });
    });

    QHBoxLayout *controlLayout = new QHBoxLayout;
    
    controlLayout->addWidget(toggleOutlineButton);
    controlLayout->addWidget(toggleBookmarksButton);
    controlLayout->addWidget(toggleBookmarkButton);
    controlLayout->addWidget(touchGesturesButton);
    controlLayout->addWidget(toggleTabBarButton);
    controlLayout->addWidget(selectFolderButton);


    controlLayout->addWidget(loadPdfButton);
    controlLayout->addWidget(clearPdfButton);
    controlLayout->addWidget(pdfTextSelectButton);
    // controlLayout->addWidget(backgroundButton);
    controlLayout->addWidget(saveButton);
    controlLayout->addWidget(saveAnnotatedButton);
    controlLayout->addWidget(openControlPanelButton);
    controlLayout->addWidget(openRecentNotebooksButton); // Add button to layout
    controlLayout->addWidget(redButton);
    controlLayout->addWidget(blueButton);
    controlLayout->addWidget(yellowButton);
    controlLayout->addWidget(greenButton);
    controlLayout->addWidget(blackButton);
    controlLayout->addWidget(whiteButton);
    controlLayout->addWidget(customColorButton);
    controlLayout->addWidget(straightLineToggleButton);
    controlLayout->addWidget(ropeToolButton); // Add rope tool button to layout
    controlLayout->addWidget(markdownButton); // Add markdown button to layout
    controlLayout->addWidget(insertPictureButton); // Add picture button to layout
    // controlLayout->addWidget(colorPreview);
    // controlLayout->addWidget(thicknessButton);
    // controlLayout->addWidget(jumpToPageButton);
    controlLayout->addWidget(dialToggleButton);
    controlLayout->addWidget(fastForwardButton);
    // controlLayout->addWidget(channelSelector);
    controlLayout->addWidget(btnPageSwitch);
    controlLayout->addWidget(btnPannScroll);
    controlLayout->addWidget(btnZoom);
    controlLayout->addWidget(btnThickness);

    controlLayout->addWidget(btnTool);
    controlLayout->addWidget(btnPresets);
    controlLayout->addWidget(addPresetButton);
    // controlLayout->addWidget(dialModeSelector);
    // controlLayout->addStretch();
    
    // controlLayout->addWidget(toolSelector);
    controlLayout->addWidget(fullscreenButton);
    // controlLayout->addWidget(zoomButton);
    controlLayout->addWidget(zoom50Button);
    controlLayout->addWidget(dezoomButton);
    controlLayout->addWidget(zoom200Button);
    controlLayout->addStretch();
    
    
    controlLayout->addWidget(prevPageButton);
    controlLayout->addWidget(pageInput);
    controlLayout->addWidget(nextPageButton);
    controlLayout->addWidget(benchmarkButton);
    controlLayout->addWidget(benchmarkLabel);
    controlLayout->addWidget(deletePageButton);
    
    
    
    controlBar = new QWidget;  // Use member variable instead of local
    controlBar->setObjectName("controlBar");
    // controlBar->setLayout(controlLayout);  // Commented out - responsive layout will handle this
    controlBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    // Theme will be applied later in loadUserSettings -> updateTheme()
    controlBar->setStyleSheet("");

    
        

    canvasStack = new QStackedWidget();
    canvasStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Create a container for the canvas and scrollbars with relative positioning
    QWidget *canvasContainer = new QWidget;
    QVBoxLayout *canvasLayout = new QVBoxLayout(canvasContainer);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    canvasLayout->addWidget(canvasStack);

    // Enable context menu for the workaround
    canvasContainer->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Set up the scrollbars to overlay the canvas
    panXSlider->setParent(canvasContainer);
    panYSlider->setParent(canvasContainer);
    
    // Raise scrollbars to ensure they're visible above the canvas
    panXSlider->raise();
    panYSlider->raise();
    
    // Handle scrollbar intersection
    connect(canvasContainer, &QWidget::customContextMenuRequested, this, [this]() {
        // This connection is just to make sure the container exists
        // and can receive signals - a workaround for some Qt versions
    });
    
    // Position the scrollbars at the bottom and right edges
    canvasContainer->installEventFilter(this);
    
    // Update scrollbar positions initially
    QTimer::singleShot(0, this, [this, canvasContainer]() {
        updateScrollbarPositions();
    });

    // Main layout: toolbar -> tab bar -> canvas (vertical stack)
    QWidget *container = new QWidget;
    container->setObjectName("container");
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 0, 0);  // ✅ Remove extra margins
    mainLayout->setSpacing(0); // ✅ Remove spacing between components
    
    // Add components in vertical order
    mainLayout->addWidget(controlBar);        // Toolbar at top
    mainLayout->addWidget(tabBarContainer);   // Tab bar below toolbar
    
    // Content area with sidebars and canvas
    QHBoxLayout *contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(outlineSidebar, 0); // Fixed width outline sidebar
    contentLayout->addWidget(bookmarksSidebar, 0); // Fixed width bookmarks sidebar
    contentLayout->addWidget(canvasContainer, 1); // Canvas takes remaining space
    
    QWidget *contentWidget = new QWidget;
    contentWidget->setLayout(contentLayout);
    mainLayout->addWidget(contentWidget, 1);

    setCentralWidget(container);

    benchmarkTimer = new QTimer(this);
    connect(benchmarkButton, &QPushButton::clicked, this, &MainWindow::toggleBenchmark);
    connect(benchmarkTimer, &QTimer::timeout, this, &MainWindow::updateBenchmarkDisplay);

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";
    QDir dir(tempDir);

    // Remove all contents (but keep the directory itself)
    if (dir.exists()) {
        dir.removeRecursively();  // Careful: this wipes everything inside
    }
    QDir().mkpath(tempDir);  // Recreate clean directory

    addNewTab();

    // Setup single instance server
    setupSingleInstanceServer();

    // Initialize responsive toolbar layout
    createSingleRowLayout();  // Start with single row layout
    
    // Now that all UI components are created, update the color palette
    updateColorPalette();

}

MainWindow::~MainWindow() {

    saveButtonMappings();  // ✅ Save on exit, as backup
    delete canvas;
    
    // Cleanup shared launcher instance
    if (sharedLauncher) {
        sharedLauncher->deleteLater();
        sharedLauncher = nullptr;
    }
    
    // Cleanup single instance resources
    if (localServer) {
        localServer->close();
        localServer = nullptr;
    }
    
    // Use static cleanup method for consistent cleanup
    cleanupSharedResources();
}

void MainWindow::toggleBenchmark() {
    benchmarking = !benchmarking;
    if (benchmarking) {
        currentCanvas()->startBenchmark();
        benchmarkTimer->start(1000); // Update every second
    } else {
        currentCanvas()->stopBenchmark();
        benchmarkTimer->stop();
        benchmarkLabel->setText(tr("PR:N/A"));
    }
}

void MainWindow::updateBenchmarkDisplay() {
    int sampleRate = currentCanvas()->getProcessedRate();
    benchmarkLabel->setText(QString(tr("PR:%1 Hz")).arg(sampleRate));
}

void MainWindow::applyCustomColor() {
    QString colorCode = customColorInput->text();
    if (!colorCode.startsWith("#")) {
        colorCode.prepend("#");
    }
    currentCanvas()->setPenColor(QColor(colorCode));
    updateDialDisplay(); 
}

void MainWindow::updateThickness(int value) {
    // Calculate thickness based on the slider value at 100% zoom
    // The slider value represents the desired visual thickness
    qreal visualThickness = value; // Scale slider value to reasonable thickness
    
    // Apply zoom scaling to maintain visual consistency
    qreal actualThickness = visualThickness * (100.0 / currentCanvas()->getZoom()); 
    
    currentCanvas()->setPenThickness(actualThickness);
}

void MainWindow::adjustThicknessForZoom(int oldZoom, int newZoom) {
    // Adjust all tool thicknesses to maintain visual consistency when zoom changes
    if (oldZoom == newZoom || oldZoom <= 0 || newZoom <= 0) return;
    
    InkCanvas* canvas = currentCanvas();
    if (!canvas) return;
    
    qreal zoomRatio = qreal(oldZoom) / qreal(newZoom);
    ToolType currentTool = canvas->getCurrentTool();
    
    // Adjust thickness for all tools, not just the current one
    canvas->adjustAllToolThicknesses(zoomRatio);
    
    // Update the thickness slider to reflect the current tool's new thickness
    updateThicknessSliderForCurrentTool();
    
    // ✅ FIXED: Update dial display to show new thickness immediately during zoom changes
    updateDialDisplay();
}


void MainWindow::changeTool(int index) {
    if (index == 0) {
        currentCanvas()->setTool(ToolType::Pen);
    } else if (index == 1) {
        currentCanvas()->setTool(ToolType::Marker);
    } else if (index == 2) {
        currentCanvas()->setTool(ToolType::Eraser);
    }
    updateToolButtonStates();
    updateThicknessSliderForCurrentTool();
    updateDialDisplay();
}

void MainWindow::setPenTool() {
    if (!currentCanvas()) return;
    currentCanvas()->setTool(ToolType::Pen);
    updateToolButtonStates();
    updateThicknessSliderForCurrentTool();
    updateDialDisplay();
}

void MainWindow::setMarkerTool() {
    if (!currentCanvas()) return;
    currentCanvas()->setTool(ToolType::Marker);
    updateToolButtonStates();
    updateThicknessSliderForCurrentTool();
    updateDialDisplay();
}

void MainWindow::setEraserTool() {
    if (!currentCanvas()) return;
    currentCanvas()->setTool(ToolType::Eraser);
    updateToolButtonStates();
    updateThicknessSliderForCurrentTool();
    updateDialDisplay();
}

void MainWindow::updateToolButtonStates() {
    if (!currentCanvas()) return;
    
    // Reset all tool buttons
    penToolButton->setProperty("selected", false);
    markerToolButton->setProperty("selected", false);
    eraserToolButton->setProperty("selected", false);
    
    // Set the selected property for the current tool
    ToolType currentTool = currentCanvas()->getCurrentTool();
    switch (currentTool) {
        case ToolType::Pen:
            penToolButton->setProperty("selected", true);
            break;
        case ToolType::Marker:
            markerToolButton->setProperty("selected", true);
            break;
        case ToolType::Eraser:
            eraserToolButton->setProperty("selected", true);
            break;
    }
    
    // Force style update
    penToolButton->style()->unpolish(penToolButton);
    penToolButton->style()->polish(penToolButton);
    markerToolButton->style()->unpolish(markerToolButton);
    markerToolButton->style()->polish(markerToolButton);
    eraserToolButton->style()->unpolish(eraserToolButton);
    eraserToolButton->style()->polish(eraserToolButton);
}

void MainWindow::handleColorButtonClick() {
    if (!currentCanvas()) return;
    
    ToolType currentTool = currentCanvas()->getCurrentTool();
    
    // If in eraser mode, switch back to pen mode
    if (currentTool == ToolType::Eraser) {
        currentCanvas()->setTool(ToolType::Pen);
        updateToolButtonStates();
        updateThicknessSliderForCurrentTool();
    }
    
    // If rope tool is enabled, turn it off
    if (currentCanvas()->isRopeToolMode()) {
        currentCanvas()->setRopeToolMode(false);
        updateRopeToolButtonState();
    }
    
    // For marker and straight line mode, leave them as they are
    // No special handling needed - they can work with color changes
}

void MainWindow::updateThicknessSliderForCurrentTool() {
    if (!currentCanvas() || !thicknessSlider) return;
    
    // Block signals to prevent recursive calls
    thicknessSlider->blockSignals(true);
    
    // Update slider to reflect current tool's thickness
    qreal currentThickness = currentCanvas()->getPenThickness();
    
    // Convert thickness back to slider value (reverse of updateThickness calculation)
    qreal visualThickness = currentThickness * (currentCanvas()->getZoom() / 100.0);
    int sliderValue = qBound(1, static_cast<int>(qRound(visualThickness)), 50);
    
    thicknessSlider->setValue(sliderValue);
    thicknessSlider->blockSignals(false);
}

bool MainWindow::selectFolder() {
    QString folder = QFileDialog::getExistingDirectory(this, tr("Select Save Folder"));
    if (!folder.isEmpty()) {
        InkCanvas *canvas = currentCanvas();
        if (canvas) {
            if (canvas->isEdited()){
                saveCurrentPage();
            }
            
            // ✅ Check if user wants to convert to .spn package
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                tr("Notebook Format"),
                tr("Would you like to convert this notebook to a SpeedyNote Package (.spn) file?\n\n"
                   ".spn files appear as single files in your file manager but maintain the same performance.\n\n"
                   "Choose 'Yes' to create a .spn package, or 'No' to keep it as a regular folder."),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
            );
            
            if (reply == QMessageBox::Cancel) {
                return false; // User cancelled
            }
            
            QString finalPath = folder;
            if (reply == QMessageBox::Yes) {
                // Convert to .spn package
                QString spnPath;
                if (SpnPackageManager::convertFolderToSpn(folder, spnPath)) {
                    finalPath = spnPath;
                    QMessageBox::information(this, tr("Success"), 
                        tr("Notebook converted to SpeedyNote Package:\n%1").arg(QFileInfo(spnPath).fileName()));
                } else {
                    QMessageBox::warning(this, tr("Conversion Failed"), 
                        tr("Failed to convert folder to .spn package. Using original folder."));
                }
            }
            
            canvas->setSaveFolder(finalPath);
            
            // ✅ Handle missing PDF file if it's a .spn package
            if (SpnPackageManager::isSpnPackage(finalPath)) {
                if (!canvas->handleMissingPdf(this)) {
                    // User cancelled PDF relinking, don't continue
                    return false;
                }
                // ✅ Update scroll behavior based on PDF loading state after relinking
                setScrollOnTopEnabled(canvas->isPdfLoadedFunc());
            }
            
            // ✅ Show last accessed page dialog if available
            if (!showLastAccessedPageDialog(canvas)) {
                // No last accessed page or user chose page 1
                switchPageWithDirection(1, 1); // Going to page 1 is forward direction
                pageInput->setValue(1);
            } else {
                // Dialog handled page switching, update page input
                pageInput->setValue(getCurrentPageForCanvas(canvas) + 1);
            }
        updateTabLabel();
            updateBookmarkButtonState(); // ✅ Update bookmark button state after loading notebook
            recentNotebooksManager->addRecentNotebook(canvas->getDisplayPath(), canvas); // Track the display path
    }
        return true; // Success
    }
    return false; // User cancelled folder selection or no canvas available
}

void MainWindow::saveCanvas() {
    currentCanvas()->saveToFile(getCurrentPageForCanvas(currentCanvas()));
}


void MainWindow::switchPage(int pageNumber) {
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;

    if (currentCanvas()->isEdited()){
        saveCurrentPageConcurrent(); // Use concurrent saving for smoother page flipping
    }

    int oldPage = getCurrentPageForCanvas(currentCanvas()) + 1; // Convert to 1-based for comparison
    int newPage = pageNumber - 1;
    pageMap[canvas] = newPage;  // ✅ Save the page for this tab

    if (canvas->isPdfLoadedFunc() && pageNumber - 1 < canvas->getTotalPdfPages()) {
        canvas->loadPdfPage(newPage);
    } else {
        canvas->loadPage(newPage);
    }

    canvas->setLastActivePage(newPage);
    
    // ✅ Track last accessed page in JSON metadata
    canvas->setLastAccessedPage(newPage);
    
    updateZoom();
    // It seems panXSlider and panYSlider can be null here during startup.
    if(panXSlider && panYSlider){
    canvas->setLastPanX(panXSlider->maximum());
    canvas->setLastPanY(panYSlider->maximum());
        
        // ✅ Enhanced scroll-on-top functionality with explicit direction
        // Now enabled for all notebooks, not just PDFs
        if (panYSlider->maximum() > 0) {
            if (pageNumber > oldPage) {
                // Forward page switching → scroll to top
                QTimer::singleShot(0, this, [this]() {
                    if (panYSlider) panYSlider->setValue(0);
                });
            } else if (pageNumber < oldPage) {
                // Backward page switching → scroll to threshold - offset to match the new switch point
                QTimer::singleShot(0, this, [this, canvas]() {
                    if (panYSlider && canvas) {
                        int threshold = canvas->getAutoscrollThreshold();
                        if (threshold > 0) {
                            // Calculate the backward switch offset (matching InkCanvas logic)
                            int backwardOffset = 300;
                            if (threshold < 600) {
                                // For small thresholds, use proportional offset
                                backwardOffset = threshold / 4;
                            }
                            // Position at threshold - offset to match the new backward switch threshold
                            int targetPan = threshold - backwardOffset;
                            // Ensure we don't go negative if threshold is too small
                            targetPan = qMax(0, targetPan);
                            panYSlider->setValue(targetPan);
                        } else {
                            // Fallback for non-combined canvases or error cases
                            panYSlider->setValue(panYSlider->maximum());
                        }
                    }
                });
            }
        }
    }
    updateDialDisplay();
    updateBookmarkButtonState(); // Update bookmark button state when switching pages
}
void MainWindow::switchPageWithDirection(int pageNumber, int direction) {
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;

    if (currentCanvas()->isEdited()){
        saveCurrentPageConcurrent(); // Use concurrent saving for smoother page flipping
    }

    int newPage = pageNumber - 1;
    pageMap[canvas] = newPage;  // ✅ Save the page for this tab

    if (canvas->isPdfLoadedFunc() && pageNumber - 1 < canvas->getTotalPdfPages()) {
        canvas->loadPdfPage(newPage);
    } else {
        canvas->loadPage(newPage);
    }

    canvas->setLastActivePage(newPage);
    
    // ✅ Track last accessed page in JSON metadata
    canvas->setLastAccessedPage(newPage);
    
    updateZoom();
    // It seems panXSlider and panYSlider can be null here during startup.
    if(panXSlider && panYSlider){
        canvas->setLastPanX(panXSlider->maximum());
        canvas->setLastPanY(panYSlider->maximum());
        
        // ✅ Enhanced scroll-on-top functionality with explicit direction
        // Now enabled for all notebooks, not just PDFs
        if (panYSlider->maximum() > 0) {
            if (direction > 0) {
                // Forward page switching → scroll to top
                QTimer::singleShot(0, this, [this]() {
                    if (panYSlider) panYSlider->setValue(0);
                });
            } else if (direction < 0) {
                // Backward page switching → scroll to threshold - offset to match the new switch point
                QTimer::singleShot(0, this, [this, canvas]() {
                    if (panYSlider && canvas) {
                        int threshold = canvas->getAutoscrollThreshold();
                        if (threshold > 0) {
                            // Calculate the backward switch offset (matching InkCanvas logic)
                            int backwardOffset = 300;
                            if (threshold < 600) {
                                // For small thresholds, use proportional offset
                                backwardOffset = threshold / 4;
                            }
                            // Position at threshold - offset to match the new backward switch threshold
                            int targetPan = threshold - backwardOffset;
                            // Ensure we don't go negative if threshold is too small
                            targetPan = qMax(0, targetPan);
                            panYSlider->setValue(targetPan);
                        } else {
                            // Fallback for non-combined canvases or error cases
                            panYSlider->setValue(panYSlider->maximum());
                        }
                    }
                });
            }
        }
    }
    updateDialDisplay();
    updateBookmarkButtonState(); // Update bookmark button state when switching pages
}

void MainWindow::deleteCurrentPage() {
    // Clear all content from current page (drawing + pictures + markdown) instead of deleting the page file
    currentCanvas()->clearCurrentPage();
}

void MainWindow::saveCurrentPage() {
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;
    
    QString currentFolder = canvas->getSaveFolder();
    QString tempFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";
    
    // ✅ Check if canvas is in temporary directory (not yet saved to .spn)
    if (currentFolder.isEmpty() || currentFolder == tempFolder) {
        // ✅ FIRST: Save the current page to temp folder (like pressing old save button)
        // This ensures the current page is included in the .spn conversion
        int currentPageNumber = getCurrentPageForCanvas(canvas);
        canvas->saveToFile(currentPageNumber);
        
        // ✅ COMBINED MODE FIX: Use combined-aware save for markdown/picture windows
        canvas->saveCombinedWindowsForPage(currentPageNumber);
        
        // ✅ THEN: Check if there are any pages to save
        QDir sourceDir(tempFolder);
        QStringList pageFiles = sourceDir.entryList(QStringList() << "*.png", QDir::Files);

        if (pageFiles.isEmpty()) {
            QMessageBox::information(this, tr("Nothing to Save"), 
                tr("There are no pages to save in this notebook."));
            return;
        }
        
        // Canvas is in temp directory - show Save As dialog directly
        QString suggestedName = "MyNotebook.spn";
        QString selectedSpnPath = QFileDialog::getSaveFileName(this, 
            tr("Save SpeedyNote Package"), 
            suggestedName, 
            "SpeedyNote Package (*.spn)");
            
        if (selectedSpnPath.isEmpty()) {
            // User cancelled save dialog
            return;
        }

        // Ensure .spn extension
        if (!selectedSpnPath.toLower().endsWith(".spn")) {
            selectedSpnPath += ".spn";
        }

        // Create .spn package from temp folder contents
        if (!SpnPackageManager::convertFolderToSpnPath(tempFolder, selectedSpnPath)) {
            QMessageBox::critical(this, tr("Save Failed"), 
                tr("Failed to save the notebook as a SpeedyNote Package.\nPlease try again or choose a different location."));
            return;
        }

        // Update canvas to use the new .spn package
        canvas->setSaveFolder(selectedSpnPath);
        
        // Update tab label to reflect the new save location
        updateTabLabel();

        // ✅ Add to recent notebooks after successful save
        if (recentNotebooksManager) {
            recentNotebooksManager->addRecentNotebook(selectedSpnPath, canvas);
            // Refresh shared launcher if it exists and is visible
            if (sharedLauncher && sharedLauncher->isVisible()) {
                sharedLauncher->refreshRecentNotebooks();
            }
        }
        
        // Show success message
        QMessageBox::information(this, tr("Saved"), 
            tr("Notebook saved successfully as: %1").arg(QFileInfo(selectedSpnPath).fileName()));
            
    } else {
        // Canvas is already tied to an .spn file - normal save behavior
        canvas->saveToFile(getCurrentPageForCanvas(canvas));
        
        // Show brief confirmation for normal saves
        QMessageBox::information(this, tr("Saved"), 
            tr("Current page saved successfully."));
    }
}

void MainWindow::saveCurrentPageConcurrent() {
    InkCanvas *canvas = currentCanvas();
    if (!canvas || !canvas->isEdited()) return;
    
    int pageNumber = getCurrentPageForCanvas(canvas);
    QString saveFolder = canvas->getSaveFolder();
    
    if (saveFolder.isEmpty()) return;
    
    // Create a copy of the buffer for concurrent saving
    QPixmap bufferCopy = canvas->getBuffer();
    
    // Check if this is a combined canvas first to determine window saving strategy
    QPixmap backgroundImage = canvas->getBackgroundImage();
    bool isCombinedCanvas = false;
    
    // If we have a background image (PDF) and buffer is roughly double its height, it's combined
    if (!backgroundImage.isNull() && bufferCopy.height() >= backgroundImage.height() * 1.8) {
        isCombinedCanvas = true;
    } else if (bufferCopy.height() > 2000) { // Fallback heuristic for very tall buffers
        isCombinedCanvas = true;
    }
    
    // Save windows using the appropriate strategy (this must be done on the main thread)
    if (isCombinedCanvas) {
        // Use combined window saving logic for cross-page coordinate adjustments
        canvas->saveCombinedWindowsForPage(pageNumber);
    } else {
        // Use standard single-page window saving
        if (canvas->getMarkdownManager()) {
            canvas->getMarkdownManager()->saveWindowsForPage(pageNumber);
        }
        if (canvas->getPictureManager()) {
            canvas->getPictureManager()->saveWindowsForPage(pageNumber);
        }
    }
    
    // ✅ Get notebook ID from JSON metadata before concurrent operation
    QString notebookId = canvas->getNotebookId();
    if (notebookId.isEmpty()) {
        canvas->loadNotebookMetadata(); // Ensure metadata is loaded
        notebookId = canvas->getNotebookId();
    }
    
    // Calculate single page height for canvas splitting
    int singlePageHeight = bufferCopy.height();
    if (isCombinedCanvas) {
        if (!backgroundImage.isNull()) {
            singlePageHeight = backgroundImage.height() / 2; // Each PDF page in the combined image
        } else {
            singlePageHeight = bufferCopy.height() / 2; // Fallback for combined canvas
        }
    }
    
    // Run the save operation concurrently
    concurrentSaveFuture = QtConcurrent::run([saveFolder, pageNumber, bufferCopy, notebookId, isCombinedCanvas, singlePageHeight]() {
        if (isCombinedCanvas) {
            // Split the combined canvas and save both halves
            int bufferWidth = bufferCopy.width();
            
            // Save current page (top half)
            QString currentFilePath = saveFolder + QString("/%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));
            QPixmap currentPageBuffer = bufferCopy.copy(0, 0, bufferWidth, singlePageHeight);
            QImage currentImage(currentPageBuffer.size(), QImage::Format_ARGB32);
            currentImage.fill(Qt::transparent);
            {
                QPainter painter(&currentImage);
                painter.drawPixmap(0, 0, currentPageBuffer);
            }
            currentImage.save(currentFilePath, "PNG");
            
            // Save next page (bottom half) by MERGING with existing content
            int nextPageNumber = pageNumber + 1;
            QString nextFilePath = saveFolder + QString("/%1_%2.png").arg(notebookId).arg(nextPageNumber, 5, 10, QChar('0'));
            QPixmap nextPageBuffer = bufferCopy.copy(0, singlePageHeight, bufferWidth, singlePageHeight);
            
            // Check if bottom half has any non-transparent content
            QImage nextCheck = nextPageBuffer.toImage();
            bool hasNewContent = false;
            for (int y = 0; y < nextCheck.height() && !hasNewContent; ++y) {
                const QRgb *row = reinterpret_cast<const QRgb*>(nextCheck.scanLine(y));
                for (int x = 0; x < nextCheck.width() && !hasNewContent; ++x) {
                    if (qAlpha(row[x]) != 0) {
                        hasNewContent = true;
                    }
                }
            }
            
            if (hasNewContent) {
                // Load existing next page content and merge with new content
                QPixmap existingNextPage;
                if (QFile::exists(nextFilePath)) {
                    existingNextPage.load(nextFilePath);
                }
                
                // Create merged image
                QImage mergedNextImage(nextPageBuffer.size(), QImage::Format_ARGB32);
                mergedNextImage.fill(Qt::transparent);
                {
                    QPainter painter(&mergedNextImage);
                    
                    // Draw existing content first
                    if (!existingNextPage.isNull()) {
                        painter.drawPixmap(0, 0, existingNextPage);
                    }
                    
                    // Draw new content on top
                    painter.drawPixmap(0, 0, nextPageBuffer);
                }
                
                mergedNextImage.save(nextFilePath, "PNG");
            }
        } else {
            // Standard single page save
            QString filePath = saveFolder + QString("/%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));
            
            QImage image(bufferCopy.size(), QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.drawPixmap(0, 0, bufferCopy);
            image.save(filePath, "PNG");
        }
    });
    
    // Mark as not edited since we're saving
    canvas->setEdited(false);
}

void MainWindow::selectBackground() {
    QString filePath = QFileDialog::getOpenFileName(this, tr("Select Background Image"), "", "Images (*.png *.jpg *.jpeg)");
    if (!filePath.isEmpty()) {
        currentCanvas()->setBackground(filePath, getCurrentPageForCanvas(currentCanvas()));
        updateZoom(); // ✅ Update zoom and pan range after background is set
    }
}

void MainWindow::saveAnnotated() {
    currentCanvas()->saveAnnotated(getCurrentPageForCanvas(currentCanvas()));
}


void MainWindow::updateZoom() {
    InkCanvas *canvas = currentCanvas();
    if (canvas) {
        canvas->setZoom(zoomSlider->value());
        canvas->setLastZoomLevel(zoomSlider->value());  // ✅ Store zoom level per tab
        updatePanRange();
        // updateThickness(thicknessSlider->value()); // ✅ REMOVED: This was resetting thickness on page switch
        // updateDialDisplay();
    }
}

qreal MainWindow::getDevicePixelRatio(){
    QScreen *screen = QGuiApplication::primaryScreen();
    qreal devicePixelRatio = screen ? screen->devicePixelRatio() : 1.0; // Default to 1.0 if null
    return devicePixelRatio;
}

void MainWindow::updatePanRange() {
    int zoom = currentCanvas()->getZoom();

    QSize canvasSize = currentCanvas()->getCanvasSize();
    QSize viewportSize = QGuiApplication::primaryScreen()->size() * QGuiApplication::primaryScreen()->devicePixelRatio();
    qreal dpr = initialDpr;
    
    // Get the actual widget size instead of screen size for more accurate calculation
    QSize actualViewportSize = size();
    
    // Adjust viewport size for toolbar and tab bar layout
    QSize effectiveViewportSize = actualViewportSize;
    int toolbarHeight = isToolbarTwoRows ? 80 : 50; // Toolbar height
    int tabBarHeight = (tabBarContainer && tabBarContainer->isVisible()) ? 38 : 0; // Tab bar height
    effectiveViewportSize.setHeight(actualViewportSize.height() - toolbarHeight - tabBarHeight);
    
    // Calculate scaled canvas size using proper DPR scaling
    int scaledCanvasWidth = canvasSize.width() * zoom / 100;
    int scaledCanvasHeight = canvasSize.height() * zoom / 100;
    
    // Calculate max pan values - if canvas is smaller than viewport, pan should be 0
    int maxPanX = qMax(0, scaledCanvasWidth - effectiveViewportSize.width());
    int maxPanY = qMax(0, scaledCanvasHeight - effectiveViewportSize.height());

    // Scale the pan range properly
    int maxPanX_scaled = maxPanX * 100 / zoom;
    int maxPanY_scaled = maxPanY * 100 / zoom;

    // Set range to 0 when canvas is smaller than viewport (centered)
    if (scaledCanvasWidth <= effectiveViewportSize.width()) {
        panXSlider->setRange(0, 0);
        panXSlider->setValue(0);
        // No need for horizontal scrollbar
        panXSlider->setVisible(false);
    } else {
    panXSlider->setRange(0, maxPanX_scaled);
        // Show scrollbar only if mouse is near and timeout hasn't occurred
        if (scrollbarsVisible && !scrollbarHideTimer->isActive()) {
            scrollbarHideTimer->start();
        }
    }
    
    if (scaledCanvasHeight <= effectiveViewportSize.height()) {
        panYSlider->setRange(0, 0);
        panYSlider->setValue(0);
        // No need for vertical scrollbar
        panYSlider->setVisible(false);
    } else {
        // Check if this is a combined canvas to extend pan Y range to negative values
        InkCanvas *canvas = currentCanvas();
        int minPanY = 0;
        if (canvas && canvas->getAutoscrollThreshold() > 0) {
            // For combined canvases, allow negative pan Y for backward scrolling
            int threshold = canvas->getAutoscrollThreshold();
            // Calculate the required negative range based on the dynamic backward switch threshold
            int backwardOffset = (threshold < 600) ? (threshold / 4) : 300;
            minPanY = qMin(-backwardOffset, -(threshold / 10)); // Ensure we can reach the switch threshold
        }
        
        panYSlider->setRange(minPanY, maxPanY_scaled);
        // Show scrollbar only if mouse is near and timeout hasn't occurred
        if (scrollbarsVisible && !scrollbarHideTimer->isActive()) {
            scrollbarHideTimer->start();
        }
    }
}

void MainWindow::updatePanX(int value) {
    InkCanvas *canvas = currentCanvas();
    if (canvas) {
        canvas->setPanX(value);
        canvas->setLastPanX(value);  // ✅ Store panX per tab
        
        // Show horizontal scrollbar temporarily
        if (panXSlider->maximum() > 0) {
            panXSlider->setVisible(true);
            scrollbarsVisible = true;
            
            // Make sure scrollbar position matches the canvas position
            if (panXSlider->value() != value) {
                panXSlider->blockSignals(true);
                panXSlider->setValue(value);
                panXSlider->blockSignals(false);
            }
            
            if (scrollbarHideTimer->isActive()) {
                scrollbarHideTimer->stop();
            }
            scrollbarHideTimer->start();
        }
    }
}

void MainWindow::updatePanY(int value) {
    InkCanvas *canvas = currentCanvas();
    if (canvas) {
        canvas->setPanY(value);
        canvas->setLastPanY(value);  // ✅ Store panY per tab
        
        // Show vertical scrollbar temporarily
        if (panYSlider->maximum() > 0) {
            panYSlider->setVisible(true);
            scrollbarsVisible = true;
            
            // Make sure scrollbar position matches the canvas position
            if (panYSlider->value() != value) {
                panYSlider->blockSignals(true);
                panYSlider->setValue(value);
                panYSlider->blockSignals(false);
            }
            
            if (scrollbarHideTimer->isActive()) {
                scrollbarHideTimer->stop();
            }
            scrollbarHideTimer->start();
        }
    }
}
void MainWindow::applyZoom() {
    bool ok;
    int zoomValue = zoomInput->text().toInt(&ok);
    if (ok && zoomValue > 0) {
        currentCanvas()->setZoom(zoomValue);
        updatePanRange(); // Update slider range after zoom change
    }
}

void MainWindow::forceUIRefresh() {
    setWindowState(Qt::WindowNoState);  // Restore first
    setWindowState(Qt::WindowMaximized);  // Maximize again
}

void MainWindow::loadPdf() {
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;

    QString saveFolder = canvas->getSaveFolder();
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";
    
    // Check if no save folder is set or if it's the temporary directory
    if (saveFolder.isEmpty() || saveFolder == tempDir) {
        QMessageBox::warning(this, tr("Cannot Load PDF"), 
            tr("Please select a permanent save folder before loading a PDF.\n\nClick the folder icon to choose a location for your notebook."));
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(this, tr("Select PDF"), "", "PDF Files (*.pdf)");
    if (!filePath.isEmpty()) {
        currentCanvas()->loadPdf(filePath);
        
        // ✅ Load the current page to display the PDF immediately
        int currentPage = getCurrentPageForCanvas(currentCanvas());
        currentCanvas()->loadPdfPage(currentPage);

        updateTabLabel(); // ✅ Update the tab name after assigning a PDF
        updateZoom(); // ✅ Update zoom and pan range after PDF is loaded
        
        // Refresh PDF outline if sidebar is visible
        if (outlineSidebarVisible) {
            loadPdfOutline();
        }
        
        // ✅ Automatically enable scroll on top when PDF is loaded (required for pseudo smooth scrolling)
        setScrollOnTopEnabled(true);
        
        // ✅ Refresh the canvas to show the PDF immediately
        currentCanvas()->update();
    }
}

void MainWindow::clearPdf() {
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;
    
    canvas->clearPdf();
    
    // ✅ Update the tab label to reflect PDF removal
    updateTabLabel();
    
    // ✅ Automatically disable scroll on top when PDF is cleared (not needed for independent canvases)
    setScrollOnTopEnabled(false);
    
    // ✅ Refresh the canvas to show the cleared state immediately
    canvas->update();
    
    updateZoom(); // ✅ Update zoom and pan range after PDF is cleared
    
    // ✅ Clear PDF outline if sidebar is visible
    if (outlineSidebarVisible) {
        loadPdfOutline(); // This will clear the outline since no PDF is loaded
    }
}

void MainWindow::handleSmartPdfButton() {
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;
    
    QString currentFolder = canvas->getSaveFolder();
    QString tempFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";
    
    // ✅ 1. Check if tab is not saved to .spn file - warn user
    if (currentFolder.isEmpty() || currentFolder == tempFolder) {
        QMessageBox::warning(this, tr("Cannot Manage PDF"), 
            tr("Please save this notebook as a SpeedyNote Package (.spn) file before managing PDF.\n\n"
               "Click the Save button to save your notebook first."));
        return;
    }
    
    // ✅ 2. Check if PDF is already loaded
    bool hasPdf = canvas->isPdfLoadedFunc();
    
    if (!hasPdf) {
        // ✅ 3. No PDF loaded - act like old loadPdfButton
        loadPdf();
    } else {
        // ✅ 4. PDF already loaded - show Replace/Delete dialog
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("PDF Management"));
        msgBox.setText(tr("A PDF is already loaded in this notebook."));
        msgBox.setInformativeText(tr("What would you like to do?"));
        
        QPushButton *replaceButton = msgBox.addButton(tr("Replace PDF"), QMessageBox::ActionRole);
        QPushButton *deleteButton = msgBox.addButton(tr("Remove PDF"), QMessageBox::DestructiveRole);
        QPushButton *cancelButton = msgBox.addButton(QMessageBox::Cancel);
        
        msgBox.setDefaultButton(replaceButton);
        msgBox.exec();
        
        if (msgBox.clickedButton() == replaceButton) {
            // Replace PDF - call loadPdf which will replace the existing one
            loadPdf();
        } else if (msgBox.clickedButton() == deleteButton) {
            // Delete PDF - call clearPdf
            clearPdf();
        }
        // If Cancel, do nothing
    }
}


void MainWindow::switchTab(int index) {
    if (!canvasStack || !tabList || !pageInput || !zoomSlider || !panXSlider || !panYSlider) {
        // qDebug() << "Error: switchTab() called before UI was fully initialized!";
        return;
    }

    if (index >= 0 && index < canvasStack->count()) {
        canvasStack->setCurrentIndex(index);
        
        // Ensure the tab list selection is properly set and styled
        if (tabList->currentRow() != index) {
            tabList->setCurrentRow(index);
        }
        
        InkCanvas *canvas = currentCanvas();
        if (canvas) {
            int savedPage = canvas->getLastActivePage();
            
            // ✅ Only call blockSignals if pageInput is valid
            if (pageInput) {  
                pageInput->blockSignals(true);
                pageInput->setValue(savedPage + 1);
                pageInput->blockSignals(false);
            }

            // ✅ Ensure zoomSlider exists before calling methods
            if (zoomSlider) {
                zoomSlider->blockSignals(true);
                zoomSlider->setValue(canvas->getLastZoomLevel());
                zoomSlider->blockSignals(false);
                canvas->setZoom(canvas->getLastZoomLevel());
            }

            // ✅ Ensure pan sliders exist before modifying values
            if (panXSlider && panYSlider) {
                panXSlider->blockSignals(true);
                panYSlider->blockSignals(true);
                panXSlider->setValue(canvas->getLastPanX());
                panYSlider->setValue(canvas->getLastPanY());
                panXSlider->blockSignals(false);
                panYSlider->blockSignals(false);
                updatePanRange();
            }
            updateDialDisplay();
            updateColorButtonStates();  // Update button states when switching tabs
            updateStraightLineButtonState();  // Update straight line button state when switching tabs
            updateRopeToolButtonState(); // Update rope tool button state when switching tabs
            updateMarkdownButtonState(); // Update markdown button state when switching tabs
            updatePictureButtonState(); // Update picture button state when switching tabs
            updatePdfTextSelectButtonState(); // Update PDF text selection button state when switching tabs
            updateBookmarkButtonState(); // Update bookmark button state when switching tabs
            updateDialButtonState();     // Update dial button state when switching tabs
            updateFastForwardButtonState(); // Update fast forward button state when switching tabs
            updateToolButtonStates();   // Update tool button states when switching tabs
            updateThicknessSliderForCurrentTool(); // Update thickness slider for current tool when switching tabs
            
            // ✅ Update scroll on top behavior based on PDF loading state
            setScrollOnTopEnabled(canvas->isPdfLoadedFunc());
            
            // Refresh PDF outline if sidebar is visible
            if (outlineSidebarVisible) {
                loadPdfOutline();
            }
            
            // ✅ Refresh bookmarks if sidebar is visible
            if (bookmarksSidebarVisible) {
                loadBookmarks();
            }
            
            // ✅ Update recent notebooks when switching to a tab to bump it to the top
            QString folderPath = canvas->getSaveFolder();
            QString tempDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";
            if (!folderPath.isEmpty() && folderPath != tempDir && recentNotebooksManager) {
                    // Add to recent notebooks to bump it to the top
                    recentNotebooksManager->addRecentNotebook(folderPath, canvas);
                    // Refresh shared launcher if it exists and is visible
                    if (sharedLauncher && sharedLauncher->isVisible()) {
                        sharedLauncher->refreshRecentNotebooks();
                    }
            }
        }
    }
}


void MainWindow::addNewTab() {
    if (!tabList || !canvasStack) return;  // Ensure tabList and canvasStack exist

    int newTabIndex = tabList->count();  // New tab index
    QWidget *tabWidget = new QWidget();  // Custom tab container
    tabWidget->setObjectName("tabWidget"); // Name the widget for easy retrieval later
    QHBoxLayout *tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(5, 2, 5, 2);

    // ✅ Create the label (Tab Name) - optimized for horizontal layout
    QLabel *tabLabel = new QLabel(QString("Tab %1").arg(newTabIndex + 1), tabWidget);    
    tabLabel->setObjectName("tabLabel"); // ✅ Name the label for easy retrieval later
    tabLabel->setWordWrap(false); // ✅ No wrapping for horizontal tabs
    tabLabel->setFixedWidth(115); // ✅ Narrower width for compact layout
    tabLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    tabLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // Left-align to show filename start
    tabLabel->setTextFormat(Qt::PlainText); // Ensure plain text for proper eliding
    // Tab label styling will be updated by theme

    // ✅ Create the close button (❌) - styled for browser-like tabs
    QPushButton *closeButton = new QPushButton(tabWidget);
    closeButton->setFixedSize(14, 14); // Smaller to fit narrower tabs
    closeButton->setIcon(loadThemedIcon("cross")); // Set themed icon
    closeButton->setStyleSheet(R"(
        QPushButton { 
            border: none; 
            background: transparent; 
            border-radius: 6px;
            padding: 1px;
        }
        QPushButton:hover { 
            background: rgba(255, 100, 100, 150); 
            border-radius: 6px;
        }
        QPushButton:pressed { 
            background: rgba(255, 50, 50, 200); 
            border-radius: 6px;
        }
    )"); // Themed styling with hover and press effects
    
    // ✅ Create new InkCanvas instance EARLIER so it can be captured by the lambda
    InkCanvas *newCanvas = new InkCanvas(this);
    
    // ✅ Handle tab closing when the button is clicked
    connect(closeButton, &QPushButton::clicked, this, [=]() { // newCanvas is now captured

        // ✅ Prevent multiple executions by disabling the button immediately
        closeButton->setEnabled(false);
        
        // ✅ Safety check: Ensure the canvas still exists and is in the stack
        if (!newCanvas || !canvasStack) {
            qWarning() << "Canvas or canvas stack is null during tab close";
            closeButton->setEnabled(true); // Re-enable on error
            return;
        }

        // ✅ Find the index by directly searching for the canvas in canvasStack
        // This is more reliable than trying to correlate tabList and canvasStack
        int indexToRemove = -1;
        for (int i = 0; i < canvasStack->count(); ++i) {
            if (canvasStack->widget(i) == newCanvas) {
                indexToRemove = i;
                break;
            }
        }

        if (indexToRemove == -1) {
            qWarning() << "Could not find canvas in canvasStack during tab close";
            closeButton->setEnabled(true); // Re-enable on error
            return; // Critical error, cannot proceed.
        }
        
        // ✅ Verify that tabList and canvasStack are in sync
        if (indexToRemove >= tabList->count()) {
            qWarning() << "Tab lists are out of sync! Canvas index:" << indexToRemove << "Tab count:" << tabList->count();
            closeButton->setEnabled(true); // Re-enable on error
                return;
            }
        
        // At this point, newCanvas is the InkCanvas instance for the tab being closed.
        // And indexToRemove is its index in tabList and canvasStack.

        // ✅ Auto-save the current page before closing the tab
        if (newCanvas && newCanvas->isEdited()) {
            int pageNumber = getCurrentPageForCanvas(newCanvas);
            newCanvas->saveToFile(pageNumber);
            
            // ✅ COMBINED MODE FIX: Use combined-aware save for markdown/picture windows
            newCanvas->saveCombinedWindowsForPage(pageNumber);
            
            // ✅ Mark as not edited to prevent double-saving in destructor
            newCanvas->setEdited(false);
        }
        
        // ✅ Save the last accessed page and bookmarks before closing tab
        if (newCanvas) {
            // ✅ Additional safety check before accessing canvas methods
            try {
                int currentPage = getCurrentPageForCanvas(newCanvas);
                newCanvas->setLastAccessedPage(currentPage);
                
                // ✅ Save current bookmarks to JSON metadata
                saveBookmarks();
            } catch (...) {
                qWarning() << "Exception occurred while saving last accessed page";
            }
        }

        // ✅ 1. PRIORITY: Handle saving first - user can cancel here
        if (!ensureTabHasUniqueSaveFolder(newCanvas)) {
            closeButton->setEnabled(true); // Re-enable on cancellation
            return; // User cancelled saving, don't close tab
        }

        // ✅ 2. ONLY AFTER SAVING: Check if it's the last remaining tab
        if (tabList->count() <= 1) {
            QMessageBox::information(this, tr("Notice"), tr("At least one tab must remain open."));
            closeButton->setEnabled(true); // Re-enable if can't close last tab
            return;
        }

        // 2. Get the final save folder path and update recent notebooks EARLY
        QString folderPath = newCanvas->getSaveFolder();
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";

        // 3. EARLY: Update cover preview and recent list BEFORE any UI changes
        if (!folderPath.isEmpty() && folderPath != tempDir && recentNotebooksManager) {
            // Force canvas to update and render current state before thumbnail generation
            newCanvas->update();
            newCanvas->repaint();
            QApplication::processEvents(); // Ensure all pending updates are processed
            
            // Generate thumbnail IMMEDIATELY while canvas is guaranteed to be valid
            recentNotebooksManager->generateAndSaveCoverPreview(folderPath, newCanvas);
            // Add/update in recent list. This also moves it to the top.
            recentNotebooksManager->addRecentNotebook(folderPath, newCanvas);
            // Refresh shared launcher if it exists (but only if visible to avoid issues)
            if (sharedLauncher && sharedLauncher->isVisible()) {
                sharedLauncher->refreshRecentNotebooks();
            }
        }
        
        // 4. Update the tab's label directly as folderPath might have changed
        QLabel *label = tabWidget->findChild<QLabel*>("tabLabel");
        if (label) {
            QString tabNameText;
            if (!folderPath.isEmpty() && folderPath != tempDir) { // Only for permanent notebooks
                QString metadataFile = folderPath + "/.pdf_path.txt";
                if (QFile::exists(metadataFile)) {
                    QFile file(metadataFile);
                    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        QTextStream in(&file);
                        QString pdfPath = in.readLine().trimmed();
                        file.close();
                        if (QFile::exists(pdfPath)) { // Check if PDF file actually exists
                            tabNameText = elideTabText(QFileInfo(pdfPath).fileName(), 90); // Elide to fit tab width
                        }
                    }
                }
                // Fallback to folder name if no PDF or PDF path invalid
                if (tabNameText.isEmpty()) {
                    tabNameText = elideTabText(QFileInfo(folderPath).fileName(), 90); // Elide to fit tab width
                }
            }
            // Only update the label if a new valid name was determined.
            // If it's still a temp folder, the original "Tab X" label remains appropriate.
            if (!tabNameText.isEmpty()) {
                label->setText(tabNameText);
            }
        }

        // 5. Remove the tab
        removeTabAt(indexToRemove);
    });


    // ✅ Add widgets to the tab layout
    tabLayout->addWidget(tabLabel);
    tabLayout->addWidget(closeButton);
    tabLayout->setStretch(0, 1);
    tabLayout->setStretch(1, 0);
    
    // ✅ Create the tab item and set widget (horizontal layout)
    QListWidgetItem *tabItem = new QListWidgetItem();
    tabItem->setSizeHint(QSize(135, 22)); // ✅ Narrower and thinner for compact layout
    tabList->addItem(tabItem);
    tabList->setItemWidget(tabItem, tabWidget);  // Attach tab layout

    canvasStack->addWidget(newCanvas);

    // ✅ Connect touch gesture signals
    connect(newCanvas, &InkCanvas::zoomChanged, this, &MainWindow::handleTouchZoomChange);
    connect(newCanvas, &InkCanvas::panChanged, this, &MainWindow::handleTouchPanChange);
    connect(newCanvas, &InkCanvas::touchGestureEnded, this, &MainWindow::handleTouchGestureEnd);
    connect(newCanvas, &InkCanvas::touchPanningChanged, this, &MainWindow::handleTouchPanningChanged);
    connect(newCanvas, &InkCanvas::ropeSelectionCompleted, this, &MainWindow::showRopeSelectionMenu);
    connect(newCanvas, &InkCanvas::pdfLinkClicked, this, [this](int targetPage) {
        // Navigate to the target page when a PDF link is clicked
        if (targetPage >= 0 && targetPage < 9999) {
            switchPageWithDirection(targetPage + 1, (targetPage + 1 > getCurrentPageForCanvas(currentCanvas()) + 1) ? 1 : -1);
            pageInput->setValue(targetPage + 1);
        }
    });
    connect(newCanvas, &InkCanvas::pdfLoaded, this, [this]() {
        // Refresh PDF outline if sidebar is visible
        if (outlineSidebarVisible) {
            loadPdfOutline();
        }
    });
    connect(newCanvas, &InkCanvas::markdownSelectionModeChanged, this, &MainWindow::updateMarkdownButtonState);
    connect(newCanvas, &InkCanvas::annotatedImageSaved, this, &MainWindow::onAnnotatedImageSaved);
    connect(newCanvas, &InkCanvas::autoScrollRequested, this, &MainWindow::onAutoScrollRequested);
    connect(newCanvas, &InkCanvas::earlySaveRequested, this, &MainWindow::onEarlySaveRequested);
    
    // Install event filter to detect mouse movement for scrollbar visibility
    newCanvas->setMouseTracking(true);
    newCanvas->installEventFilter(this);
    
    // Disable tablet tracking for canvases for now to prevent crashes
    // TODO: Find a safer way to implement hover tooltips without tablet tracking
    // QTimer::singleShot(50, this, [newCanvas]() {
    //     try {
    //         if (newCanvas && newCanvas->window() && newCanvas->window()->windowHandle()) {
    //             newCanvas->setAttribute(Qt::WA_TabletTracking, true);
    //         }
    //     } catch (...) {
    //         // Silently ignore tablet tracking errors
    //     }
    // });
    
    // ✅ Apply touch gesture setting
    newCanvas->setTouchGesturesEnabled(touchGesturesEnabled);

    pageMap[newCanvas] = 0;

    // ✅ Select the new tab
    tabList->setCurrentItem(tabItem);
    canvasStack->setCurrentWidget(newCanvas);

    zoomSlider->setValue(100 / initialDpr); // Set initial zoom level based on DPR
    updateDialDisplay();
    updateStraightLineButtonState();  // Initialize straight line button state for the new tab
    updateRopeToolButtonState(); // Initialize rope tool button state for the new tab
    updatePdfTextSelectButtonState(); // Initialize PDF text selection button state for the new tab
    updateBookmarkButtonState(); // Initialize bookmark button state for the new tab
    updateMarkdownButtonState(); // Initialize markdown button state for the new tab
    updatePictureButtonState(); // Initialize picture button state for the new tab
    updateDialButtonState();     // Initialize dial button state for the new tab
    updateFastForwardButtonState(); // Initialize fast forward button state for the new tab
    updateToolButtonStates();   // Initialize tool button states for the new tab

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";
    newCanvas->setSaveFolder(tempDir);
    
    // Load persistent background settings
    BackgroundStyle defaultStyle;
    QColor defaultColor;
    int defaultDensity;
    loadDefaultBackgroundSettings(defaultStyle, defaultColor, defaultDensity);
    
    newCanvas->setBackgroundStyle(defaultStyle);
    newCanvas->setBackgroundColor(defaultColor);
    newCanvas->setBackgroundDensity(defaultDensity);
    
    // ✅ New tabs start without PDFs, so disable scroll on top initially
    // It will be automatically enabled when a PDF is loaded
    setScrollOnTopEnabled(false);
    newCanvas->setPDFRenderDPI(getPdfDPI());
    
    // Update color button states for the new tab
    updateColorButtonStates();
}
void MainWindow::removeTabAt(int index) {
    if (!tabList || !canvasStack) return; // Ensure UI elements exist
    if (index < 0 || index >= canvasStack->count()) return;

    // ✅ Remove tab entry
    QListWidgetItem *item = tabList->takeItem(index);
    delete item;

    // ✅ Remove and delete the canvas safely
        QWidget *canvasWidget = canvasStack->widget(index); // Get widget before removal
        // ensureTabHasUniqueSaveFolder(currentCanvas()); // Moved to the close button lambda

        if (canvasWidget) {
            // ✅ Disconnect all signals from this canvas to prevent memory leaks
            InkCanvas *canvasInstance = qobject_cast<InkCanvas*>(canvasWidget);
            if (canvasInstance) {
                // Disconnect all signals between this canvas and MainWindow
                disconnect(canvasInstance, nullptr, this, nullptr);
                // Remove event filter
                canvasInstance->removeEventFilter(this);
            }
            
            canvasStack->removeWidget(canvasWidget); // Remove from stack
            canvasWidget->deleteLater(); // ✅ Use deleteLater() for safer deletion
    }

    // ✅ Select the previous tab (or first tab if none left)
    if (tabList->count() > 0) {
        int newIndex = qMax(0, index - 1);
        tabList->setCurrentRow(newIndex);
        canvasStack->setCurrentWidget(canvasStack->widget(newIndex));
    }

    // QWidget *canvasWidget = canvasStack->widget(index); // Redeclaration - remove this block
    // InkCanvas *canvasInstance = qobject_cast<InkCanvas*>(canvasWidget);
    //
    // if (canvasInstance) {
    //     QString folderPath = canvasInstance->getSaveFolder();
    //     if (!folderPath.isEmpty() && folderPath != QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session") {
    //         // recentNotebooksManager->addRecentNotebook(folderPath, canvasInstance); // Moved to close button lambda
    //     }
    // }
}

bool MainWindow::ensureTabHasUniqueSaveFolder(InkCanvas* canvas) {
    if (!canvas) return true; // No canvas to save, allow closure

    if (canvasStack->count() == 0) return true; // No tabs, allow closure

    QString currentFolder = canvas->getSaveFolder();
    QString tempFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";

    if (currentFolder.isEmpty() || currentFolder == tempFolder) {

        QDir sourceDir(tempFolder);
        QStringList pageFiles = sourceDir.entryList(QStringList() << "*.png", QDir::Files);

        // No pages to save → allow closure without prompting
        if (pageFiles.isEmpty()) {
            return true;
        }

        QMessageBox::StandardButton reply = QMessageBox::question(this, 
            tr("Save Notebook"), 
            tr("This notebook contains unsaved work.\n\n"
               "Would you like to save it as a SpeedyNote Package (.spn) file before closing?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

        if (reply == QMessageBox::Cancel) {
            return false; // User cancelled, don't close tab
        }
        
        if (reply == QMessageBox::Discard) {
            return true; // User chose to discard, allow closure
        }

        // User chose Save - prompt for .spn file location
        QString suggestedName = "MyNotebook.spn";
        QString selectedSpnPath = QFileDialog::getSaveFileName(this, 
            tr("Save SpeedyNote Package"), 
            suggestedName, 
            "SpeedyNote Package (*.spn)");
            
        if (selectedSpnPath.isEmpty()) {
            return false; // User cancelled save dialog, don't close tab
        }

        // Ensure .spn extension
        if (!selectedSpnPath.toLower().endsWith(".spn")) {
            selectedSpnPath += ".spn";
        }

        // Create .spn package from temp folder contents
        if (!SpnPackageManager::convertFolderToSpnPath(tempFolder, selectedSpnPath)) {
            QMessageBox::critical(this, tr("Save Failed"), 
                tr("Failed to save the notebook as a SpeedyNote Package.\nPlease try again or choose a different location."));
            return false; // Save failed, don't close tab
        }

        // Update canvas to use the new .spn package
        canvas->setSaveFolder(selectedSpnPath);

        // ✅ Add to recent notebooks after successful save
        if (recentNotebooksManager) {
            recentNotebooksManager->addRecentNotebook(selectedSpnPath, canvas);
            // Refresh shared launcher if it exists and is visible
            if (sharedLauncher && sharedLauncher->isVisible()) {
                sharedLauncher->refreshRecentNotebooks();
            }
        }
        
        QMessageBox::information(this, tr("Saved Successfully"), 
            tr("Notebook saved as: %1").arg(QFileInfo(selectedSpnPath).fileName()));
    }

    return true; // Success, allow tab closure
}



InkCanvas* MainWindow::currentCanvas() {
    if (!canvasStack || canvasStack->currentWidget() == nullptr) return nullptr;
    return static_cast<InkCanvas*>(canvasStack->currentWidget());
}


void MainWindow::updateTabLabel() {
    int index = tabList->currentRow();
    if (index < 0) return;

    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;

    QString folderPath = canvas->getDisplayPath(); // ✅ Get display path (shows .spn filename instead of temp dir)
    if (folderPath.isEmpty()) return;

    QString tabName;

    // ✅ Get PDF path from JSON metadata
    canvas->loadNotebookMetadata(); // Ensure metadata is loaded
    QString pdfPath = canvas->getPdfPath();
    if (!pdfPath.isEmpty()) {
            QFileInfo pdfInfo(pdfPath);
            if (pdfInfo.exists()) {
            tabName = elideTabText(pdfInfo.fileName(), 90); // e.g., "mydocument.pdf" (elided)
        }
    }

    // ✅ If no PDF, use appropriate fallback
    if (tabName.isEmpty()) {
        if (folderPath.toLower().endsWith(".spn")) {
            // For .spn packages, use the .spn filename
            QFileInfo spnInfo(folderPath);
            tabName = elideTabText(spnInfo.fileName(), 90); // e.g., "MyNotebook.spn" (elided)
        } else {
            // For regular folders, use the folder name
        QFileInfo folderInfo(folderPath);
            tabName = elideTabText(folderInfo.fileName(), 90); // e.g., "MyNotebook" (elided)
        }
    }

    QListWidgetItem *tabItem = tabList->item(index);
    if (tabItem) {
        QWidget *tabWidget = tabList->itemWidget(tabItem); // Get the tab's custom widget
        if (tabWidget) {
            QLabel *tabLabel = tabWidget->findChild<QLabel *>(); // Get the QLabel inside
            if (tabLabel) {
                tabLabel->setText(tabName); // ✅ Update tab label
                tabLabel->setWordWrap(false); // No wrapping for horizontal tabs
            }
        }
    }
}

int MainWindow::getCurrentPageForCanvas(InkCanvas *canvas) {
    return pageMap.contains(canvas) ? pageMap[canvas] : 0;
}

void MainWindow::toggleZoomSlider() {
    if (zoomFrame->isVisible()) {
        zoomFrame->hide();
        return;
    }

    // ✅ Set as a standalone pop-up window so it can receive events
    zoomFrame->setWindowFlags(Qt::Popup);

    // ✅ Position it right below the button
    QPoint buttonPos = zoomButton->mapToGlobal(QPoint(0, zoomButton->height()));
    zoomFrame->move(buttonPos.x(), buttonPos.y() + 5);
    zoomFrame->show();
}

void MainWindow::toggleThicknessSlider() {
    if (thicknessFrame->isVisible()) {
        thicknessFrame->hide();
        return;
    }

    // ✅ Set as a standalone pop-up window so it can receive events
    thicknessFrame->setWindowFlags(Qt::Popup);

    // ✅ Position it right below the button
    QPoint buttonPos = thicknessButton->mapToGlobal(QPoint(0, thicknessButton->height()));
    thicknessFrame->move(buttonPos.x(), buttonPos.y() + 5);

    thicknessFrame->show();
}


void MainWindow::toggleFullscreen() {
    if (isFullScreen()) {
        showNormal();  // Exit fullscreen mode
    } else {
        showFullScreen();  // Enter fullscreen mode
    }
}

void MainWindow::showJumpToPageDialog() {
    bool ok;
    int currentPage = getCurrentPageForCanvas(currentCanvas()) + 1;  // ✅ Convert zero-based to one-based
    int newPage = QInputDialog::getInt(this, "Jump to Page", "Enter Page Number:", 
                                       currentPage, 1, 9999, 1, &ok);
    if (ok) {
        // ✅ Use direction-aware page switching for jump-to-page
        int direction = (newPage > currentPage) ? 1 : (newPage < currentPage) ? -1 : 0;
        if (direction != 0) {
            switchPageWithDirection(newPage, direction);
        } else {
            switchPage(newPage); // Same page, no direction needed
        }
        pageInput->setValue(newPage);
    }
}

void MainWindow::goToPreviousPage() {
    int currentPage = getCurrentPageForCanvas(currentCanvas()) + 1;  // Convert to 1-based
    if (currentPage > 1) {
        int newPage = currentPage - 1;
        switchPageWithDirection(newPage, -1); // -1 indicates backward
        pageInput->blockSignals(true);
        pageInput->setValue(newPage);
        pageInput->blockSignals(false);
    }
}

void MainWindow::goToNextPage() {
    int currentPage = getCurrentPageForCanvas(currentCanvas()) + 1;  // Convert to 1-based
    int newPage = currentPage + 1;
    switchPageWithDirection(newPage, 1); // 1 indicates forward
    pageInput->blockSignals(true);
    pageInput->setValue(newPage);
    pageInput->blockSignals(false);
}

void MainWindow::onPageInputChanged(int newPage) {
    int currentPage = getCurrentPageForCanvas(currentCanvas()) + 1;  // Convert to 1-based
    
    // ✅ Use direction-aware page switching for spinbox
    int direction = (newPage > currentPage) ? 1 : (newPage < currentPage) ? -1 : 0;
    if (direction != 0) {
        switchPageWithDirection(newPage, direction);
    } else {
        switchPage(newPage); // Same page, no direction needed
    }
}

void MainWindow::toggleDial() {
    if (!dialContainer) {  
        // ✅ Create floating container for the dial
        dialContainer = new QWidget(this);
        dialContainer->setObjectName("dialContainer");
        dialContainer->setFixedSize(140, 140);
        dialContainer->setAttribute(Qt::WA_TranslucentBackground);
        dialContainer->setAttribute(Qt::WA_NoSystemBackground);
        dialContainer->setAttribute(Qt::WA_OpaquePaintEvent);
        dialContainer->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        dialContainer->setStyleSheet("background: transparent; border-radius: 100px;");  // ✅ More transparent

        // ✅ Create dial
        pageDial = new QDial(dialContainer);
        pageDial->setFixedSize(140, 140);
        pageDial->setMinimum(0);
        pageDial->setMaximum(360);
        pageDial->setWrapping(true);  // ✅ Allow full-circle rotation
        
        // Apply theme color immediately when dial is created
        QColor accentColor = getAccentColor();
        pageDial->setStyleSheet(QString(R"(
        QDial {
            background-color: %1;
            }
        )").arg(accentColor.name()));

        /*

        modeDial = new QDial(dialContainer);
        modeDial->setFixedSize(150, 150);
        modeDial->setMinimum(0);
        modeDial->setMaximum(300);  // 6 modes, 60° each
        modeDial->setSingleStep(60);
        modeDial->setWrapping(true);
        modeDial->setStyleSheet("background:rgb(0, 76, 147);");
        modeDial->move(25, 25);
        
        */
        

        dialColorPreview = new QFrame(dialContainer);
        dialColorPreview->setFixedSize(30, 30);
        dialColorPreview->setStyleSheet("border-radius: 15px; border: 1px solid black;");
        dialColorPreview->move(55, 35); // Center of dial

        dialIconView = new QLabel(dialContainer);
        dialIconView->setFixedSize(30, 30);
        dialIconView->setStyleSheet("border-radius: 1px; border: 1px solid black;");
        dialIconView->move(55, 35); // Center of dial

        // ✅ Position dial near top-right corner initially
        positionDialContainer();

        dialDisplay = new QLabel(dialContainer);
        dialDisplay->setAlignment(Qt::AlignCenter);
        dialDisplay->setFixedSize(80, 80);
        dialDisplay->move(30, 30);  // Center position inside the dial
        

        int fontId = QFontDatabase::addApplicationFont(":/resources/fonts/Jersey20-Regular.ttf");
        // int chnFontId = QFontDatabase::addApplicationFont(":/resources/fonts/NotoSansSC-Medium.ttf");
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);

        if (!fontFamilies.isEmpty()) {
            QFont pixelFont(fontFamilies.at(0), 11);
            dialDisplay->setFont(pixelFont);
        }

        dialDisplay->setStyleSheet("background-color: black; color: white; font-size: 14px; border-radius: 4px;");

        dialHiddenButton = new QPushButton(dialContainer);
        dialHiddenButton->setFixedSize(80, 80);
        dialHiddenButton->move(30, 30); // Same position as dialDisplay
        dialHiddenButton->setStyleSheet("background: transparent; border: none;"); // ✅ Fully invisible
        dialHiddenButton->setFocusPolicy(Qt::NoFocus); // ✅ Prevents accidental focus issues
        dialHiddenButton->setEnabled(false);  // ✅ Disabled by default

        // ✅ Connection will be set in changeDialMode() based on current mode

        dialColorPreview->raise();  // ✅ Ensure it's on top
        dialIconView->raise();
        // ✅ Connect dial input and release
        // connect(pageDial, &QDial::valueChanged, this, &MainWindow::handleDialInput);
        // connect(pageDial, &QDial::sliderReleased, this, &MainWindow::onDialReleased);

        // connect(modeDial, &QDial::valueChanged, this, &MainWindow::handleModeSelection);
        changeDialMode(currentDialMode);  // ✅ Set initial mode

        // ✅ Enable drag detection
        dialContainer->installEventFilter(this);
    }

    // ✅ Ensure that `dialContainer` is always initialized before setting visibility
    if (dialContainer) {
        dialContainer->setVisible(!dialContainer->isVisible());
    }

    initializeDialSound();  // ✅ Ensure sound is loaded

    // Inside toggleDial():
    
    if (!dialDisplay) {
        dialDisplay = new QLabel(dialContainer);
    }
    updateDialDisplay(); // ✅ Ensure it's updated before showing

    if (controllerManager) {
        connect(controllerManager, &SDLControllerManager::buttonHeld, this, &MainWindow::handleButtonHeld);
        connect(controllerManager, &SDLControllerManager::buttonReleased, this, &MainWindow::handleButtonReleased);
        connect(controllerManager, &SDLControllerManager::leftStickAngleChanged, pageDial, &QDial::setValue);
        connect(controllerManager, &SDLControllerManager::leftStickReleased, pageDial, &QDial::sliderReleased);
        connect(controllerManager, &SDLControllerManager::buttonSinglePress, this, &MainWindow::handleControllerButton);
    }

    loadButtonMappings();  // ✅ Load button mappings for the controller
    loadMouseDialMappings(); // ✅ Load mouse dial mappings

    // Update button state to reflect dial visibility
    updateDialButtonState();
}

void MainWindow::positionDialContainer() {
    if (!dialContainer) return;
    
    // Get window dimensions
    int windowWidth = width();
    int windowHeight = height();
    
    // Get dial dimensions
    int dialWidth = dialContainer->width();   // 140px
    int dialHeight = dialContainer->height(); // 140px
    
    // Calculate toolbar height based on current layout
    int toolbarHeight = isToolbarTwoRows ? 80 : 50; // Approximate heights
    
    // Add tab bar height if visible
    int tabBarHeight = (tabBarContainer && tabBarContainer->isVisible()) ? 38 : 0;
    
    // Define margins from edges
    int rightMargin = 20;  // Distance from right edge
    int topMargin = 20;    // Distance from top edge (below toolbar and tabs)
    
    // Calculate ideal position (top-right corner with margins)
    int idealX = windowWidth - dialWidth - rightMargin;
    int idealY = toolbarHeight + tabBarHeight + topMargin;
    
    // Ensure dial stays within window bounds with minimum margins
    int minMargin = 10;
    int maxX = windowWidth - dialWidth - minMargin;
    int maxY = windowHeight - dialHeight - minMargin;
    
    // Clamp position to stay within bounds
    int finalX = qBound(minMargin, idealX, maxX);
    int finalY = qBound(toolbarHeight + tabBarHeight + minMargin, idealY, maxY);
    
    // Move the dial to the calculated position
    dialContainer->move(finalX, finalY);
}

void MainWindow::updateDialDisplay() {
    if (!dialDisplay) return;
    if (!dialColorPreview) return;
    if (!dialIconView) return;
    dialIconView->show();
    qreal dpr = initialDpr;
    QColor currentColor = currentCanvas()->getPenColor();
    switch (currentDialMode) {
        case DialMode::PageSwitching:
            if (fastForwardMode){
                dialDisplay->setText(QString(tr("\n\nPage\n%1").arg(getCurrentPageForCanvas(currentCanvas()) + 1 + tempClicks * 8)));
            }
            else {
                dialDisplay->setText(QString(tr("\n\nPage\n%1").arg(getCurrentPageForCanvas(currentCanvas()) + 1 + tempClicks)));
            }
            dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/bookpage_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            break;
        case DialMode::ThicknessControl:
            {
                QString toolName;
                switch (currentCanvas()->getCurrentTool()) {
                    case ToolType::Pen:
                        toolName = tr("Pen");
            break;
                    case ToolType::Marker:
                        toolName = tr("Marker");
            break;
                    case ToolType::Eraser:
                        toolName = tr("Eraser");
                    break;
            }
                dialDisplay->setText(QString(tr("\n\n%1\n%2").arg(toolName).arg(QString::number(currentCanvas()->getPenThickness(), 'f', 1))));
            dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/thickness_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;  
        case DialMode::ZoomControl:
            dialDisplay->setText(QString(tr("\n\nZoom\n%1%").arg(currentCanvas() ? currentCanvas()->getZoom() * initialDpr : zoomSlider->value() * initialDpr)));
            dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/zoom_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            break;
  
        case DialMode::ToolSwitching:
            // ✅ Convert ToolType to QString for display
            switch (currentCanvas()->getCurrentTool()) {
                case ToolType::Pen:
                    dialDisplay->setText(tr("\n\n\nPen"));
                    dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/pen_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    break;
                case ToolType::Marker:
                    dialDisplay->setText(tr("\n\n\nMarker"));
                    dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/marker_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    break;
                case ToolType::Eraser:
                    dialDisplay->setText(tr("\n\n\nEraser"));
                    dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/eraser_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    break;
            }
            break;
        case PresetSelection:
            dialColorPreview->show();
            dialIconView->hide();
            dialColorPreview->setStyleSheet(QString("background-color: %1; border-radius: 15px; border: 1px solid black;")
                                            .arg(colorPresets[currentPresetIndex].name()));
            dialDisplay->setText(QString(tr("\n\nPreset %1\n#%2"))
                                            .arg(currentPresetIndex + 1)  // ✅ Display preset index (1-based)
                                            .arg(colorPresets[currentPresetIndex].name().remove("#"))); // ✅ Display hex color
            // dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/preset_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            break;
        case DialMode::PanAndPageScroll:
            dialIconView->setPixmap(QPixmap(":/resources/icons/scroll_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            QString fullscreenStatus = controlBarVisible ? tr("Etr") : tr("Exit");
            dialDisplay->setText(QString(tr("\n\nPage %1\n%2 FulScr")).arg(getCurrentPageForCanvas(currentCanvas()) + 1).arg(fullscreenStatus));
            break;
    }
}

/*

void MainWindow::handleModeSelection(int angle) {
    static int lastModeIndex = -1;  // ✅ Store last mode index

    // ✅ Snap to closest fixed 60° step
    int snappedAngle = (angle + 30) / 60 * 60;  // Round to nearest 60°
    int modeIndex = snappedAngle / 60;  // Convert to mode index

    if (modeIndex >= 6) modeIndex = 0;  // ✅ Wrap around (if 360°, reset to 0° mode)

    if (modeIndex != lastModeIndex) {  // ✅ Only switch mode if it's different
        changeDialMode(static_cast<DialMode>(modeIndex));

        // ✅ Play click sound when snapping to new mode
        if (dialClickSound) {
            dialClickSound->play();
        }

        lastModeIndex = modeIndex;  // ✅ Update last mode
    }
}

*/



void MainWindow::handleDialInput(int angle) {
    if (!tracking) {
        startAngle = angle;  // ✅ Set initial position
        accumulatedRotation = 0;  // ✅ Reset tracking
        tracking = true;
        lastAngle = angle;
        return;
    }

    int delta = angle - lastAngle;

    // ✅ Handle 360-degree wrapping
    if (delta > 180) delta -= 360;  // Example: 350° → 10° should be -20° instead of +340°
    if (delta < -180) delta += 360; // Example: 10° → 350° should be +20° instead of -340°

    accumulatedRotation += delta;  // ✅ Accumulate movement

    // ✅ Detect crossing a 45-degree boundary
    int currentClicks = accumulatedRotation / 45; // Total number of "clicks" crossed
    int previousClicks = (accumulatedRotation - delta) / 45; // Previous click count

    if (currentClicks != previousClicks) {  // ✅ Play sound if a new boundary is crossed
        
        if (dialClickSound) {
            dialClickSound->play();
    
            // ✅ Vibrate controller
            SDL_Joystick *joystick = controllerManager->getJoystick();
            if (joystick) {
                // Note: SDL_JoystickRumble requires SDL 2.0.9+
                // For older versions, this will be a no-op
                #if SDL_VERSION_ATLEAST(2, 0, 9)
                SDL_JoystickRumble(joystick, 0xA000, 0xF000, 10);  // Vibrate shortly
                #endif
            }
    
            grossTotalClicks += 1;
            tempClicks = currentClicks;
            updateDialDisplay();
    
            // Only load PDF previews for page-switching dial modes
            if (isLowResPreviewEnabled() && 
                (currentDialMode == PageSwitching || currentDialMode == PanAndPageScroll)) {
                int previewPage = qBound(1, getCurrentPageForCanvas(currentCanvas()) + currentClicks, 99999);
                currentCanvas()->loadPdfPreviewAsync(previewPage);
            }
        }
    }

    lastAngle = angle;  // ✅ Store last position
}



void MainWindow::onDialReleased() {
    if (!tracking) return;  // ✅ Ignore if no tracking

    int pagesToAdvance = fastForwardMode ? 8 : 1;
    int totalClicks = accumulatedRotation / 45;  // ✅ Convert degrees to pages

    /*
    int leftOver = accumulatedRotation % 45;  // ✅ Track remaining rotation
    if (leftOver > 22 && totalClicks >= 0) {
        totalClicks += 1;  // ✅ Round up if more than halfway
    } 
    else if (leftOver <= -22 && totalClicks >= 0) {
        totalClicks -= 1;  // ✅ Round down if more than halfway
    }
    */
    

    if (totalClicks != 0 || grossTotalClicks != 0) {  // ✅ Only switch pages if movement happened
        // Use concurrent autosave for smoother dial operation
        if (currentCanvas()->isEdited()) {
            saveCurrentPageConcurrent();
        }

        int currentPage = getCurrentPageForCanvas(currentCanvas()) + 1;
        int newPage = qBound(1, currentPage + totalClicks * pagesToAdvance, 99999);
        
        // ✅ Use direction-aware page switching for dial
        int direction = (totalClicks * pagesToAdvance > 0) ? 1 : -1;
        switchPageWithDirection(newPage, direction);
        pageInput->setValue(newPage);
        tempClicks = 0;
        updateDialDisplay(); 
        /*
        if (dialClickSound) {
            dialClickSound->play();
        }
        */
    }

    accumulatedRotation = 0;  // ✅ Reset tracking
    grossTotalClicks = 0;
    tracking = false;
}


void MainWindow::handleToolSelection(int angle) {
    static int lastToolIndex = -1;  // ✅ Track last tool index

    // ✅ Snap to closest fixed 120° step
    int snappedAngle = (angle + 60) / 120 * 120;  // Round to nearest 120°
    int toolIndex = snappedAngle / 120;  // Convert to tool index (0, 1, 2)

    if (toolIndex >= 3) toolIndex = 0;  // ✅ Wrap around at 360° → Back to Pen (0)

    if (toolIndex != lastToolIndex) {  // ✅ Only switch if tool actually changes
        toolSelector->setCurrentIndex(toolIndex);  // ✅ Change tool
        lastToolIndex = toolIndex;  // ✅ Update last selected tool

        // ✅ Play click sound when tool changes
        if (dialClickSound) {
            dialClickSound->play();
        }

        SDL_Joystick *joystick = controllerManager->getJoystick();

        if (joystick) {
            #if SDL_VERSION_ATLEAST(2, 0, 9)
            SDL_JoystickRumble(joystick, 0xA000, 0xF000, 20);  // ✅ Vibrate controller
            #endif
        }

        updateToolButtonStates();  // ✅ Update tool button states
        updateDialDisplay();  // ✅ Update dial display]
    }
}

void MainWindow::onToolReleased() {
    
}
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    static bool dragging = false;
    static QPoint lastMousePos;
    static QTimer *longPressTimer = nullptr;

    // Handle IME focus events for text input widgets
    QLineEdit *lineEdit = qobject_cast<QLineEdit*>(obj);
    if (lineEdit) {
        if (event->type() == QEvent::FocusIn) {
            // Ensure IME is enabled when text field gets focus
            lineEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
            QInputMethod *inputMethod = QGuiApplication::inputMethod();
            if (inputMethod) {
                inputMethod->show();
            }
        }
        else if (event->type() == QEvent::FocusOut) {
            // Keep IME available but reset state
            QInputMethod *inputMethod = QGuiApplication::inputMethod();
            if (inputMethod) {
                inputMethod->reset();
            }
        }
    }

    // Handle resize events for canvas container
    QWidget *container = canvasStack ? canvasStack->parentWidget() : nullptr;
    if (obj == container && event->type() == QEvent::Resize) {
        updateScrollbarPositions();
        return false; // Let the event propagate
    }

    // Handle scrollbar visibility
    if (obj == panXSlider || obj == panYSlider) {
        if (event->type() == QEvent::Enter) {
            // Mouse entered scrollbar area
            if (scrollbarHideTimer->isActive()) {
                scrollbarHideTimer->stop();
            }
            return false;
        } 
        else if (event->type() == QEvent::Leave) {
            // Mouse left scrollbar area - start timer to hide
            if (!scrollbarHideTimer->isActive()) {
                scrollbarHideTimer->start();
            }
            return false;
        }
    }

    // Check if this is a canvas event for scrollbar handling
    InkCanvas* canvas = qobject_cast<InkCanvas*>(obj);
    if (canvas) {
        // Handle mouse movement for scrollbar visibility
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            handleEdgeProximity(canvas, mouseEvent->pos());
        }
        // Handle tablet events for stylus hover (safely)
        else if (event->type() == QEvent::TabletMove) {
            try {
                QTabletEvent* tabletEvent = static_cast<QTabletEvent*>(event);
                handleEdgeProximity(canvas, tabletEvent->position().toPoint());
            } catch (...) {
                // Ignore tablet event errors to prevent crashes
            }
        }
        // Handle mouse button press events for forward/backward navigation
        else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            
            // ✅ Don't handle BackButton/ForwardButton here anymore - handled by mouse dial system
            // They will handle short press page navigation and long press dial mode
            /*
            // Mouse button 4 (Back button) - Previous page
            if (mouseEvent->button() == Qt::BackButton) {
                if (prevPageButton) {
                    prevPageButton->click();
                }
                return true; // Consume the event
            }
            // Mouse button 5 (Forward button) - Next page
            else if (mouseEvent->button() == Qt::ForwardButton) {
                if (nextPageButton) {
                    nextPageButton->click();
                }
                return true; // Consume the event
            }
            */
            // ✅ Don't handle ExtraButton1/ExtraButton2 here anymore - handled by mouse dial system
        }
        // Handle wheel events for scrolling
        else if (event->type() == QEvent::Wheel) {
            // ✅ Don't handle wheel events if mouse dial mode is active
            if (mouseDialModeActive) {
                return false; // Let the main window wheelEvent handle it
            }
            
            QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
            
            // Check if we need scrolling
            bool needHorizontalScroll = panXSlider->maximum() > 0;
            bool needVerticalScroll = panYSlider->maximum() > 0;
            
            // Handle vertical scrolling (Y axis)
            if (wheelEvent->angleDelta().y() != 0 && needVerticalScroll) {
                // Calculate scroll amount (negative because wheel up should scroll up)
                int scrollDelta = -wheelEvent->angleDelta().y() / 8; // Convert from 1/8 degree units
                
                // Detect if this is a trackpad (smooth scrolling) vs mouse wheel (stepped)
                // Trackpad typically sends smaller, more frequent events
                bool isTrackpad = qAbs(wheelEvent->angleDelta().y()) < 120; // Less than typical mouse wheel step
                
                if (isTrackpad) {
                    // Trackpad: Use direct, smooth scrolling for better responsiveness
                    scrollDelta = scrollDelta / 2; // Reduce sensitivity for smooth trackpad scrolling
                } else {
                    // Mouse wheel: Use stepped scrolling as before
                    scrollDelta = scrollDelta / 15; // Convert to steps (typical wheel step is 15 degrees)
                    
                    // Much faster base scroll speed - aim for ~3-5 scrolls to reach bottom
                    int baseScrollAmount = panYSlider->maximum() / 8; // 1/8 of total range per scroll
                    scrollDelta = scrollDelta * qMax(baseScrollAmount, 50); // Minimum 50 units per scroll
                }
                
                // Apply the scroll
                int currentPan = panYSlider->value();
                int newPan = qBound(panYSlider->minimum(), currentPan + scrollDelta, panYSlider->maximum());
                panYSlider->setValue(newPan); // This triggers autoscroll via valueChanged signal
                
                // Show scrollbar temporarily
                panYSlider->setVisible(true);
                scrollbarsVisible = true;
                if (scrollbarHideTimer->isActive()) {
                    scrollbarHideTimer->stop();
                }
                scrollbarHideTimer->start();
                
                // Consume the event
                return true;
            }
            
            // Handle horizontal scrolling (X axis) - for completeness
            if (wheelEvent->angleDelta().x() != 0 && needHorizontalScroll) {
                // Calculate scroll amount
                int scrollDelta = wheelEvent->angleDelta().x() / 8; // Convert from 1/8 degree units
                scrollDelta = scrollDelta / 15; // Convert to steps
                
                // Much faster base scroll speed - same logic as vertical
                int baseScrollAmount = panXSlider->maximum() / 8; // 1/8 of total range per scroll
                scrollDelta = scrollDelta * qMax(baseScrollAmount, 50); // Minimum 50 units per scroll
                
                // Apply the scroll
                int currentPan = panXSlider->value();
                int newPan = qBound(panXSlider->minimum(), currentPan + scrollDelta, panXSlider->maximum());
                panXSlider->setValue(newPan);
                
                // Show scrollbar temporarily
                panXSlider->setVisible(true);
                scrollbarsVisible = true;
                if (scrollbarHideTimer->isActive()) {
                    scrollbarHideTimer->stop();
                }
                scrollbarHideTimer->start();
                
                // Consume the event
                return true;
            }
            
            // If no scrolling was needed, let the event propagate
            return false;
        }
    }

    // Handle dial container drag events
    if (obj == dialContainer) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            lastMousePos = mouseEvent->globalPos();
            dragging = false;

            if (!longPressTimer) {
                longPressTimer = new QTimer(this);
                longPressTimer->setSingleShot(true);
                connect(longPressTimer, &QTimer::timeout, [this]() {
                    dragging = true;  // ✅ Allow movement after long press
                });
            }
            longPressTimer->start(1500);  // ✅ Start long press timer (500ms)
            return true;
        }

        if (event->type() == QEvent::MouseMove && dragging) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QPoint delta = mouseEvent->globalPos() - lastMousePos;
            dialContainer->move(dialContainer->pos() + delta);
            lastMousePos = mouseEvent->globalPos();
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            if (longPressTimer) longPressTimer->stop();
            dragging = false;  // ✅ Stop dragging on release
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}


void MainWindow::initializeDialSound() {
    if (!dialClickSound) {
        dialClickSound = new SimpleAudio();
        if (!dialClickSound->loadWavFile(":/resources/sounds/dial_click.wav")) {
            qWarning() << "Failed to load dial click sound - audio will be disabled";
        }
        dialClickSound->setVolume(0.8);  // ✅ Set volume (0.0 - 1.0)
        dialClickSound->setMinimumInterval(5); // ✅ DirectSound can handle much faster rates (5ms minimum)
    }
}

void MainWindow::changeDialMode(DialMode mode) {

    if (!dialContainer) return;  // ✅ Ensure dial container exists
    currentDialMode = mode; // ✅ Set new mode
    updateDialDisplay();

    // ✅ Enable dialHiddenButton for PanAndPageScroll and ZoomControl modes
    dialHiddenButton->setEnabled(currentDialMode == PanAndPageScroll || currentDialMode == ZoomControl);

    // ✅ Disconnect previous slots
    disconnect(pageDial, &QDial::valueChanged, nullptr, nullptr);
    disconnect(pageDial, &QDial::sliderReleased, nullptr, nullptr);
    
    // ✅ Disconnect dialHiddenButton to reconnect with appropriate function
    disconnect(dialHiddenButton, &QPushButton::clicked, nullptr, nullptr);
    
    // ✅ Connect dialHiddenButton to appropriate function based on mode
    if (currentDialMode == PanAndPageScroll) {
        connect(dialHiddenButton, &QPushButton::clicked, this, &MainWindow::toggleControlBar);
    } else if (currentDialMode == ZoomControl) {
        connect(dialHiddenButton, &QPushButton::clicked, this, &MainWindow::cycleZoomLevels);
    }

    dialColorPreview->hide();
    dialDisplay->setStyleSheet("background-color: black; color: white; font-size: 14px; border-radius: 40px;");

    // ✅ Connect the correct function set for the current mode
    switch (currentDialMode) {
        case PageSwitching:
            connect(pageDial, &QDial::valueChanged, this, &MainWindow::handleDialInput);
            connect(pageDial, &QDial::sliderReleased, this, &MainWindow::onDialReleased);
            break;
        case ZoomControl:
            connect(pageDial, &QDial::valueChanged, this, &MainWindow::handleDialZoom);
            connect(pageDial, &QDial::sliderReleased, this, &MainWindow::onZoomReleased);
            break;
        case ThicknessControl:
            connect(pageDial, &QDial::valueChanged, this, &MainWindow::handleDialThickness);
            connect(pageDial, &QDial::sliderReleased, this, &MainWindow::onThicknessReleased);
            break;

        case ToolSwitching:
            connect(pageDial, &QDial::valueChanged, this, &MainWindow::handleToolSelection);
            connect(pageDial, &QDial::sliderReleased, this, &MainWindow::onToolReleased);
            break;
        case PresetSelection:
            connect(pageDial, &QDial::valueChanged, this, &MainWindow::handlePresetSelection);
            connect(pageDial, &QDial::sliderReleased, this, &MainWindow::onPresetReleased);
            break;
        case PanAndPageScroll:
            connect(pageDial, &QDial::valueChanged, this, &MainWindow::handleDialPanScroll);
            connect(pageDial, &QDial::sliderReleased, this, &MainWindow::onPanScrollReleased);
            break;
        
    }
}

void MainWindow::handleDialZoom(int angle) {
    if (!tracking) {
        startAngle = angle;  
        accumulatedRotation = 0;  
        tracking = true;
        lastAngle = angle;
        return;
    }

    int delta = angle - lastAngle;

    // ✅ Handle 360-degree wrapping
    if (delta > 180) delta -= 360;
    if (delta < -180) delta += 360;

    accumulatedRotation += delta;

    if (abs(delta) < 4) {  
        return;  
    }

    // ✅ Apply zoom dynamically (instead of waiting for release)
    int oldZoom = zoomSlider->value();
    int newZoom = qBound(10, oldZoom + (delta / 4), 400);  
    zoomSlider->setValue(newZoom);
    updateZoom();  // ✅ Ensure zoom updates immediately
    updateDialDisplay(); 

    lastAngle = angle;
}

void MainWindow::onZoomReleased() {
    accumulatedRotation = 0;
    tracking = false;
}

// New variable (add to MainWindow.h near accumulatedRotation)
int accumulatedRotationAfterLimit = 0;

void MainWindow::handleDialPanScroll(int angle) {
    if (!tracking) {
        startAngle = angle;
        accumulatedRotation = 0;
        accumulatedRotationAfterLimit = 0;
        tracking = true;
        lastAngle = angle;
        pendingPageFlip = 0;
        return;
    }

    int delta = angle - lastAngle;

    // Handle 360 wrap
    if (delta > 180) delta -= 360;
    if (delta < -180) delta += 360;

    accumulatedRotation += delta;

    // Pan scroll
    int panDelta = delta * 4;  // Adjust scroll sensitivity here
    int currentPan = panYSlider->value();
    int newPan = currentPan + panDelta;

    // Clamp pan slider
    newPan = qBound(panYSlider->minimum(), newPan, panYSlider->maximum());
    panYSlider->setValue(newPan);

    // ✅ NEW → if slider reached top/bottom, accumulate AFTER LIMIT
    if (newPan == panYSlider->maximum()) {
        accumulatedRotationAfterLimit += delta;

        if (accumulatedRotationAfterLimit >= 120) {
            pendingPageFlip = +1;  // Flip next when released
        }
    } 
    else if (newPan == panYSlider->minimum()) {
        accumulatedRotationAfterLimit += delta;

        if (accumulatedRotationAfterLimit <= -120) {
            pendingPageFlip = -1;  // Flip previous when released
        }
    } 
    else {
        // Reset after limit accumulator when not at limit
        accumulatedRotationAfterLimit = 0;
        pendingPageFlip = 0;
    }

    lastAngle = angle;
}

void MainWindow::onPanScrollReleased() {
    // ✅ Perform page flip only when dial released and flip is pending
    if (pendingPageFlip != 0) {
        // Use concurrent saving for smooth dial operation
        if (currentCanvas()->isEdited()) {
            saveCurrentPageConcurrent();
        }

        int currentPage = getCurrentPageForCanvas(currentCanvas());
        int newPage = qBound(1, currentPage + pendingPageFlip + 1, 99999);
        
        // ✅ Use direction-aware page switching for pan-and-scroll dial
        switchPageWithDirection(newPage, pendingPageFlip);
        pageInput->setValue(newPage);
        updateDialDisplay();

        SDL_Joystick *joystick = controllerManager->getJoystick();
        if (joystick) {
            #if SDL_VERSION_ATLEAST(2, 0, 9)
            SDL_JoystickRumble(joystick, 0xA000, 0xF000, 25);  // Vibrate shortly
            #endif
        }
    }

    // Reset states
    pendingPageFlip = 0;
    accumulatedRotation = 0;
    accumulatedRotationAfterLimit = 0;
    tracking = false;
}



void MainWindow::handleDialThickness(int angle) {
    if (!tracking) {
        startAngle = angle;
        tracking = true;
        lastAngle = angle;
        return;
    }

    int delta = angle - lastAngle;
    if (delta > 180) delta -= 360;
    if (delta < -180) delta += 360;

    int thicknessStep = fastForwardMode ? 5 : 1;
    currentCanvas()->setPenThickness(qBound<qreal>(1.0, currentCanvas()->getPenThickness() + (delta / 10.0) * thicknessStep, 50.0));

    updateDialDisplay();
    lastAngle = angle;
}

void MainWindow::onThicknessReleased() {
    accumulatedRotation = 0;
    tracking = false;
}

void MainWindow::handlePresetSelection(int angle) {
    static int lastAngle = angle;
    int delta = angle - lastAngle;

    // ✅ Handle 360-degree wrapping
    if (delta > 180) delta -= 360;
    if (delta < -180) delta += 360;

    if (abs(delta) >= 60) {  // ✅ Change preset every 60° (6 presets)
        lastAngle = angle;
        currentPresetIndex = (currentPresetIndex + (delta > 0 ? 1 : -1) + colorPresets.size()) % colorPresets.size();
        
        QColor selectedColor = colorPresets[currentPresetIndex];
        currentCanvas()->setPenColor(selectedColor);
        updateCustomColorButtonStyle(selectedColor);
        updateDialDisplay();
        updateColorButtonStates();  // Update button states when preset is selected
        
        if (dialClickSound) dialClickSound->play();  // ✅ Provide feedback
        SDL_Joystick *joystick = controllerManager->getJoystick();
            if (joystick) {
                #if SDL_VERSION_ATLEAST(2, 0, 9)
                SDL_JoystickRumble(joystick, 0xA000, 0xF000, 25);  // Vibrate shortly
                #endif
            }
    }
}

void MainWindow::onPresetReleased() {
    accumulatedRotation = 0;
    tracking = false;
}






void MainWindow::addColorPreset() {
    QColor currentColor = currentCanvas()->getPenColor();

    // ✅ Prevent duplicates
    if (!colorPresets.contains(currentColor)) {
        if (colorPresets.size() >= 6) {
            colorPresets.dequeue();  // ✅ Remove oldest color
        }
        colorPresets.enqueue(currentColor);
    }
}

// Static method to update Qt application palette based on Windows dark mode
void MainWindow::updateApplicationPalette() {
#ifdef Q_OS_WIN
    // Detect if Windows is in dark mode
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 
                       QSettings::NativeFormat);
    int appsUseLightTheme = settings.value("AppsUseLightTheme", 1).toInt();
    bool isDarkMode = (appsUseLightTheme == 0);
    
    if (isDarkMode) {
        // Switch to Fusion style on Windows for proper dark mode support
        // The default Windows style doesn't respect custom palettes properly
        QApplication::setStyle("Fusion");
        
        // Create a comprehensive dark palette for Qt widgets
        QPalette darkPalette;
        
        // Base colors
        QColor darkGray(53, 53, 53);
        QColor gray(128, 128, 128);
        QColor black(25, 25, 25);
        QColor blue(42, 130, 218);
        QColor lightGray(180, 180, 180);
        
        // Window colors (main background)
        darkPalette.setColor(QPalette::Window, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        
        // Base (text input background) colors
        darkPalette.setColor(QPalette::Base, QColor(35, 35, 35));
        darkPalette.setColor(QPalette::AlternateBase, darkGray);
        darkPalette.setColor(QPalette::Text, Qt::white);
        
        // Tooltip colors
        darkPalette.setColor(QPalette::ToolTipBase, QColor(60, 60, 60));
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        
        // Button colors (critical for dialogs)
        darkPalette.setColor(QPalette::Button, darkGray);
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        
        // 3D effects and borders (critical for proper widget rendering)
        darkPalette.setColor(QPalette::Light, QColor(80, 80, 80));
        darkPalette.setColor(QPalette::Midlight, QColor(65, 65, 65));
        darkPalette.setColor(QPalette::Dark, QColor(35, 35, 35));
        darkPalette.setColor(QPalette::Mid, QColor(50, 50, 50));
        darkPalette.setColor(QPalette::Shadow, QColor(20, 20, 20));
        
        // Bright text
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        
        // Link colors
        darkPalette.setColor(QPalette::Link, blue);
        darkPalette.setColor(QPalette::LinkVisited, QColor(blue).lighter());
        
        // Highlight colors (selection)
        darkPalette.setColor(QPalette::Highlight, blue);
        darkPalette.setColor(QPalette::HighlightedText, Qt::white);
        
        // Placeholder text (for line edits, spin boxes, etc.)
        darkPalette.setColor(QPalette::PlaceholderText, gray);
        
        // Disabled colors (all color groups)
        darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::Base, QColor(50, 50, 50));
        darkPalette.setColor(QPalette::Disabled, QPalette::Button, QColor(50, 50, 50));
        darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
        
        QApplication::setPalette(darkPalette);
    } else {
        // Use default Windows style and palette for light mode
        QApplication::setStyle("windowsvista");
        QApplication::setPalette(QPalette());
    }
#endif
    // On Linux, don't override palette - desktop environment handles it
}

// to support dark mode icon switching.
bool MainWindow::isDarkMode() {
#ifdef Q_OS_WIN
    // On Windows, read the registry to detect dark mode
    // This works on Windows 10 1809+ and Windows 11
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 
                       QSettings::NativeFormat);
    
    // AppsUseLightTheme: 0 = dark mode, 1 = light mode
    // If the key doesn't exist (older Windows), default to light mode
    int appsUseLightTheme = settings.value("AppsUseLightTheme", 1).toInt();
    return (appsUseLightTheme == 0);
#else
    // On Linux and other platforms, use palette-based detection
    QColor bg = palette().color(QPalette::Window);
    return bg.lightness() < 128;  // Lightness scale: 0 (black) - 255 (white)
#endif
}

QColor MainWindow::getDefaultPenColor() {
    return isDarkMode() ? Qt::white : Qt::black;
}

QIcon MainWindow::loadThemedIcon(const QString& baseName) {
    QString path = isDarkMode()
        ? QString(":/resources/icons/%1_reversed.png").arg(baseName)
        : QString(":/resources/icons/%1.png").arg(baseName);
    return QIcon(path);
}

QString MainWindow::createButtonStyle(bool darkMode) {
    if (darkMode) {
        // Dark mode: Keep current white highlights (good contrast)
        return R"(
            QPushButton {
                background: transparent;
                border: none;
                padding: 6px;
            }
            QPushButton:hover {
                background: rgba(255, 255, 255, 50);
            }
            QPushButton:pressed {
                background: rgba(0, 0, 0, 50);
            }
            QPushButton[selected="true"] {
                background: rgba(255, 255, 255, 100);
                border: 2px solid rgba(255, 255, 255, 150);
                padding: 4px;
                border-radius: 4px;
            }
            QPushButton[selected="true"]:hover {
                background: rgba(255, 255, 255, 120);
            }
            QPushButton[selected="true"]:pressed {
                background: rgba(0, 0, 0, 50);
            }
        )";
    } else {
        // Light mode: Use darker colors for better visibility
        return R"(
            QPushButton {
                background: transparent;
                border: none;
                padding: 6px;
            }
            QPushButton:hover {
                background: rgba(0, 0, 0, 30);
            }
            QPushButton:pressed {
                background: rgba(0, 0, 0, 60);
            }
            QPushButton[selected="true"] {
                background: rgba(0, 0, 0, 80);
                border: 2px solid rgba(0, 0, 0, 120);
                padding: 4px;
                border-radius: 4px;
            }
            QPushButton[selected="true"]:hover {
                background: rgba(0, 0, 0, 100);
            }
            QPushButton[selected="true"]:pressed {
                background: rgba(0, 0, 0, 140);
            }
        )";
    }
}



QColor MainWindow::getAccentColor() const {
    if (useCustomAccentColor && customAccentColor.isValid()) {
        return customAccentColor;
    }
    
    // Return system accent color
    QPalette palette = QGuiApplication::palette();
    return palette.highlight().color();
}

void MainWindow::setCustomAccentColor(const QColor &color) {
    if (customAccentColor != color) {
        customAccentColor = color;
        saveThemeSettings();
        // Always update theme if custom accent color is enabled
        if (useCustomAccentColor) {
            updateTheme();
        }
    }
}

void MainWindow::setUseCustomAccentColor(bool use) {
    if (useCustomAccentColor != use) {
        useCustomAccentColor = use;
        updateTheme();
        saveThemeSettings();
    }
}
void MainWindow::updateTheme() {
    // Update control bar background color
    QColor accentColor = getAccentColor();
    if (controlBar) {
        controlBar->setStyleSheet(QString(R"(
        QWidget#controlBar {
            background-color: %1;
            }
        )").arg(accentColor.name()));
    }
    
    // Update dial background color
    if (pageDial) {
        pageDial->setStyleSheet(QString(R"(
        QDial {
            background-color: %1;
            }
        )").arg(accentColor.name()));
    }
    
    // Update add tab button styling
    if (addTabButton) {
        bool darkMode = isDarkMode();
        QString buttonBgColor = darkMode ? "rgba(80, 80, 80, 255)" : "rgba(220, 220, 220, 255)";
        QString buttonHoverColor = darkMode ? "rgba(90, 90, 90, 255)" : "rgba(200, 200, 200, 255)";
        QString buttonPressColor = darkMode ? "rgba(70, 70, 70, 255)" : "rgba(180, 180, 180, 255)";
        QString borderColor = darkMode ? "rgba(100, 100, 100, 255)" : "rgba(180, 180, 180, 255)";
        
        addTabButton->setStyleSheet(QString(R"(
            QPushButton {
                background-color: %1;
                border: 1px solid %2;
                border-radius: 12px;
                margin: 2px;
            }
            QPushButton:hover {
                background-color: %3;
            }
            QPushButton:pressed {
                background-color: %4;
            }
        )").arg(buttonBgColor).arg(borderColor).arg(buttonHoverColor).arg(buttonPressColor));
    }
    
    // Update PDF outline sidebar styling
    if (outlineSidebar && outlineTree) {
        bool darkMode = isDarkMode();
        QString bgColor = darkMode ? "rgba(45, 45, 45, 255)" : "rgba(250, 250, 250, 255)";
        QString borderColor = darkMode ? "rgba(80, 80, 80, 255)" : "rgba(200, 200, 200, 255)";
        QString textColor = darkMode ? "#E0E0E0" : "#333";
        QString hoverColor = darkMode ? "rgba(60, 60, 60, 255)" : "rgba(240, 240, 240, 255)";
        QString selectedColor = QString("rgba(%1, %2, %3, 100)").arg(accentColor.red()).arg(accentColor.green()).arg(accentColor.blue());
        
        outlineSidebar->setStyleSheet(QString(R"(
            QWidget {
                background-color: %1;
                border-right: 1px solid %2;
            }
            QLabel {
                color: %3;
                background: transparent;
            }
        )").arg(bgColor).arg(borderColor).arg(textColor));
        
        outlineTree->setStyleSheet(QString(R"(
            QTreeWidget {
                background-color: %1;
                border: none;
                color: %2;
                outline: none;
            }
            QTreeWidget::item {
                padding: 4px;
                border: none;
            }
            QTreeWidget::item:hover {
                background-color: %3;
            }
            QTreeWidget::item:selected {
                background-color: %4;
                color: %2;
            }
            QTreeWidget::branch {
                background: transparent;
            }
            QTreeWidget::branch:has-children:!has-siblings:closed,
            QTreeWidget::branch:closed:has-children:has-siblings {
                border-image: none;
                image: url(:/resources/icons/down_arrow.png);
            }
            QTreeWidget::branch:open:has-children:!has-siblings,
            QTreeWidget::branch:open:has-children:has-siblings {
                border-image: none;
                image: url(:/resources/icons/up_arrow.png);
            }
            QScrollBar:vertical {
                background: rgba(200, 200, 200, 80);
                border: none;
                margin: 0px;
                width: 16px !important;
                max-width: 16px !important;
            }
            QScrollBar:vertical:hover {
                background: rgba(200, 200, 200, 120);
            }
            QScrollBar::handle:vertical {
                background: rgba(100, 100, 100, 150);
                border-radius: 2px;
                min-height: 120px;
            }
            QScrollBar::handle:vertical:hover {
                background: rgba(80, 80, 80, 210);
            }
            QScrollBar::add-line:vertical, 
            QScrollBar::sub-line:vertical {
                width: 0px;
                height: 0px;
                background: none;
                border: none;
            }
            QScrollBar::add-page:vertical, 
            QScrollBar::sub-page:vertical {
                background: transparent;
            }
        )").arg(bgColor).arg(textColor).arg(hoverColor).arg(selectedColor));
        
        // Apply same styling to bookmarks tree
        bookmarksTree->setStyleSheet(QString(R"(
            QTreeWidget {
                background-color: %1;
                border: none;
                color: %2;
                outline: none;
            }
            QTreeWidget::item {
                padding: 2px;
                border: none;
                min-height: 26px;
            }
            QTreeWidget::item:hover {
                background-color: %3;
            }
            QTreeWidget::item:selected {
                background-color: %4;
                color: %2;
            }
            QScrollBar:vertical {
                background: rgba(200, 200, 200, 80);
                border: none;
                margin: 0px;
                width: 16px !important;
                max-width: 16px !important;
            }
            QScrollBar:vertical:hover {
                background: rgba(200, 200, 200, 120);
            }
            QScrollBar::handle:vertical {
                background: rgba(100, 100, 100, 150);
                border-radius: 2px;
                min-height: 120px;
            }
            QScrollBar::handle:vertical:hover {
                background: rgba(80, 80, 80, 210);
            }
            QScrollBar::add-line:vertical, 
            QScrollBar::sub-line:vertical {
                width: 0px;
                height: 0px;
                background: none;
                border: none;
            }
            QScrollBar::add-page:vertical, 
            QScrollBar::sub-page:vertical {
                background: transparent;
            }
        )").arg(bgColor).arg(textColor).arg(hoverColor).arg(selectedColor));
    }
    
    // Update horizontal tab bar styling with accent color
    if (tabList) {
        bool darkMode = isDarkMode();
        QString bgColor = darkMode ? "rgba(60, 60, 60, 255)" : "rgba(240, 240, 240, 255)";
        QString itemBgColor = darkMode ? "rgba(80, 80, 80, 255)" : "rgba(220, 220, 220, 255)";
        QString selectedBgColor = darkMode ? "rgba(45, 45, 45, 255)" : "white";
        QString borderColor = darkMode ? "rgba(100, 100, 100, 255)" : "rgba(180, 180, 180, 255)";
        QString hoverBgColor = darkMode ? "rgba(90, 90, 90, 255)" : "rgba(230, 230, 230, 255)";
        
        tabList->setStyleSheet(QString(R"(
        QListWidget {
            background-color: %1;
            border: none;
            border-bottom: 2px solid %2;
            outline: none;
        }
        QListWidget::item {
            background-color: %3;
            border: 1px solid %4;
            border-bottom: none;
            margin-right: 1px;
            margin-top: 2px;
            padding: 0px;
            min-width: 80px;
            max-width: 120px;
        }
        QListWidget::item:selected {
            background-color: %5;
            border: 1px solid %4;
            border-bottom: 2px solid %2;
            margin-top: 1px;
        }
        QListWidget::item:hover:!selected {
            background-color: %6;
        }
        QScrollBar:horizontal {
            background: %1;
            height: 8px;
            border: none;
            margin: 0px;
            border-top: 1px solid %4;
        }
        QScrollBar::handle:horizontal {
            background: rgba(150, 150, 150, 120);
            border-radius: 4px;
            min-width: 20px;
            margin: 1px;
        }
        QScrollBar::handle:horizontal:hover {
            background: rgba(120, 120, 120, 200);
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
            height: 0px;
            background: none;
            border: none;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
        }
        )").arg(bgColor)
           .arg(accentColor.name())
           .arg(itemBgColor)
           .arg(borderColor)
           .arg(selectedBgColor)
           .arg(hoverBgColor));
    }
    

    
    // Force icon reload for all buttons that use themed icons
    if (loadPdfButton) loadPdfButton->setIcon(loadThemedIcon("pdf"));
    if (clearPdfButton) clearPdfButton->setIcon(loadThemedIcon("pdfdelete"));
    if (pdfTextSelectButton) pdfTextSelectButton->setIcon(loadThemedIcon("ibeam"));

    if (benchmarkButton) benchmarkButton->setIcon(loadThemedIcon("benchmark"));
    if (toggleTabBarButton) toggleTabBarButton->setIcon(loadThemedIcon("tabs"));
    if (toggleOutlineButton) toggleOutlineButton->setIcon(loadThemedIcon("outline"));
    if (toggleBookmarksButton) toggleBookmarksButton->setIcon(loadThemedIcon("bookmark"));
    if (toggleBookmarkButton) toggleBookmarkButton->setIcon(loadThemedIcon("star"));
    if (selectFolderButton) selectFolderButton->setIcon(loadThemedIcon("folder"));
    if (saveButton) saveButton->setIcon(loadThemedIcon("save"));
    if (saveAnnotatedButton) saveAnnotatedButton->setIcon(loadThemedIcon("saveannotated"));
    if (fullscreenButton) fullscreenButton->setIcon(loadThemedIcon("fullscreen"));
    // if (backgroundButton) backgroundButton->setIcon(loadThemedIcon("background"));
    if (straightLineToggleButton) straightLineToggleButton->setIcon(loadThemedIcon("straightLine"));
    if (ropeToolButton) ropeToolButton->setIcon(loadThemedIcon("rope"));
    if (markdownButton) markdownButton->setIcon(loadThemedIcon("markdown"));
    if (deletePageButton) deletePageButton->setIcon(loadThemedIcon("trash"));
    if (zoomButton) zoomButton->setIcon(loadThemedIcon("zoom"));
    if (dialToggleButton) dialToggleButton->setIcon(loadThemedIcon("dial"));
    if (fastForwardButton) fastForwardButton->setIcon(loadThemedIcon("fastforward"));
    if (jumpToPageButton) jumpToPageButton->setIcon(loadThemedIcon("bookpage"));
    if (thicknessButton) thicknessButton->setIcon(loadThemedIcon("thickness"));
    if (btnPageSwitch) btnPageSwitch->setIcon(loadThemedIcon("bookpage"));
    if (btnZoom) btnZoom->setIcon(loadThemedIcon("zoom"));
    if (btnThickness) btnThickness->setIcon(loadThemedIcon("thickness"));
    if (btnTool) btnTool->setIcon(loadThemedIcon("pen"));
    if (btnPresets) btnPresets->setIcon(loadThemedIcon("preset"));
    if (btnPannScroll) btnPannScroll->setIcon(loadThemedIcon("scroll"));
    if (addPresetButton) addPresetButton->setIcon(loadThemedIcon("savepreset"));
    if (openControlPanelButton) openControlPanelButton->setIcon(loadThemedIcon("settings"));
    if (openRecentNotebooksButton) openRecentNotebooksButton->setIcon(loadThemedIcon("recent"));
    if (penToolButton) penToolButton->setIcon(loadThemedIcon("pen"));
    if (markerToolButton) markerToolButton->setIcon(loadThemedIcon("marker"));
    if (eraserToolButton) eraserToolButton->setIcon(loadThemedIcon("eraser"));
    
    // Update button styles with new theme
    bool darkMode = isDarkMode();
    QString newButtonStyle = createButtonStyle(darkMode);
    
    // Update all buttons that use the buttonStyle
    if (loadPdfButton) loadPdfButton->setStyleSheet(newButtonStyle);
    if (clearPdfButton) clearPdfButton->setStyleSheet(newButtonStyle);
    if (pdfTextSelectButton) pdfTextSelectButton->setStyleSheet(newButtonStyle);

    if (benchmarkButton) benchmarkButton->setStyleSheet(newButtonStyle);
    if (toggleTabBarButton) toggleTabBarButton->setStyleSheet(newButtonStyle);
    if (toggleOutlineButton) toggleOutlineButton->setStyleSheet(newButtonStyle);
    if (toggleBookmarksButton) toggleBookmarksButton->setStyleSheet(newButtonStyle);
    if (toggleBookmarkButton) toggleBookmarkButton->setStyleSheet(newButtonStyle);
    if (selectFolderButton) selectFolderButton->setStyleSheet(newButtonStyle);
    if (saveButton) saveButton->setStyleSheet(newButtonStyle);
    if (saveAnnotatedButton) saveAnnotatedButton->setStyleSheet(newButtonStyle);
    if (fullscreenButton) fullscreenButton->setStyleSheet(newButtonStyle);
    if (redButton) redButton->setStyleSheet(newButtonStyle);
    if (blueButton) blueButton->setStyleSheet(newButtonStyle);
    if (yellowButton) yellowButton->setStyleSheet(newButtonStyle);
    if (greenButton) greenButton->setStyleSheet(newButtonStyle);
    if (blackButton) blackButton->setStyleSheet(newButtonStyle);
    if (whiteButton) whiteButton->setStyleSheet(newButtonStyle);
    if (thicknessButton) thicknessButton->setStyleSheet(newButtonStyle);
    if (penToolButton) penToolButton->setStyleSheet(newButtonStyle);
    if (markerToolButton) markerToolButton->setStyleSheet(newButtonStyle);
    if (eraserToolButton) eraserToolButton->setStyleSheet(newButtonStyle);
    // if (backgroundButton) backgroundButton->setStyleSheet(newButtonStyle);
    if (straightLineToggleButton) straightLineToggleButton->setStyleSheet(newButtonStyle);
    if (ropeToolButton) ropeToolButton->setStyleSheet(newButtonStyle);
    if (markdownButton) markdownButton->setStyleSheet(newButtonStyle);
    if (insertPictureButton) insertPictureButton->setStyleSheet(newButtonStyle);
    if (deletePageButton) deletePageButton->setStyleSheet(newButtonStyle);
    if (zoomButton) zoomButton->setStyleSheet(newButtonStyle);
    if (dialToggleButton) dialToggleButton->setStyleSheet(newButtonStyle);
    if (fastForwardButton) fastForwardButton->setStyleSheet(newButtonStyle);
    if (jumpToPageButton) jumpToPageButton->setStyleSheet(newButtonStyle);
    if (btnPageSwitch) btnPageSwitch->setStyleSheet(newButtonStyle);
    if (btnZoom) btnZoom->setStyleSheet(newButtonStyle);
    if (btnThickness) btnThickness->setStyleSheet(newButtonStyle);
    if (btnTool) btnTool->setStyleSheet(newButtonStyle);
    if (btnPresets) btnPresets->setStyleSheet(newButtonStyle);
    if (btnPannScroll) btnPannScroll->setStyleSheet(newButtonStyle);
    if (addPresetButton) addPresetButton->setStyleSheet(newButtonStyle);
    if (openControlPanelButton) openControlPanelButton->setStyleSheet(newButtonStyle);
    if (openRecentNotebooksButton) openRecentNotebooksButton->setStyleSheet(newButtonStyle);
    if (zoom50Button) zoom50Button->setStyleSheet(newButtonStyle);
    if (dezoomButton) dezoomButton->setStyleSheet(newButtonStyle);
    if (zoom200Button) zoom200Button->setStyleSheet(newButtonStyle);
    if (prevPageButton) prevPageButton->setStyleSheet(newButtonStyle);
    if (nextPageButton) nextPageButton->setStyleSheet(newButtonStyle);
    
    // Update color buttons with palette-based icons
    if (redButton) {
        QString redIconPath = useBrighterPalette ? ":/resources/icons/pen_light_red.png" : ":/resources/icons/pen_dark_red.png";
        redButton->setIcon(QIcon(redIconPath));
    }
    if (blueButton) {
        QString blueIconPath = useBrighterPalette ? ":/resources/icons/pen_light_blue.png" : ":/resources/icons/pen_dark_blue.png";
        blueButton->setIcon(QIcon(blueIconPath));
    }
    if (yellowButton) {
        QString yellowIconPath = useBrighterPalette ? ":/resources/icons/pen_light_yellow.png" : ":/resources/icons/pen_dark_yellow.png";
        yellowButton->setIcon(QIcon(yellowIconPath));
    }
    if (greenButton) {
        QString greenIconPath = useBrighterPalette ? ":/resources/icons/pen_light_green.png" : ":/resources/icons/pen_dark_green.png";
        greenButton->setIcon(QIcon(greenIconPath));
    }
    if (blackButton) {
        QString blackIconPath = darkMode ? ":/resources/icons/pen_light_black.png" : ":/resources/icons/pen_dark_black.png";
        blackButton->setIcon(QIcon(blackIconPath));
    }
    if (whiteButton) {
        QString whiteIconPath = darkMode ? ":/resources/icons/pen_light_white.png" : ":/resources/icons/pen_dark_white.png";
        whiteButton->setIcon(QIcon(whiteIconPath));
    }
    
    // Update tab close button icons and label styling
    if (tabList) {
        bool darkMode = isDarkMode();
        QString labelColor = darkMode ? "#E0E0E0" : "#333";
        
        for (int i = 0; i < tabList->count(); ++i) {
            QListWidgetItem *item = tabList->item(i);
            if (item) {
                QWidget *tabWidget = tabList->itemWidget(item);
                if (tabWidget) {
                    QPushButton *closeButton = tabWidget->findChild<QPushButton*>();
                    if (closeButton) {
                        closeButton->setIcon(loadThemedIcon("cross"));
                    }
                    
                    QLabel *tabLabel = tabWidget->findChild<QLabel*>("tabLabel");
                    if (tabLabel) {
                        tabLabel->setStyleSheet(QString("color: %1; font-weight: 500; padding: 2px; text-align: left;").arg(labelColor));
                    }
                }
            }
        }
    }
    
    // Update dial display
    updateDialDisplay();
}

void MainWindow::saveThemeSettings() {
    QSettings settings("SpeedyNote", "App");
    settings.setValue("useCustomAccentColor", useCustomAccentColor);
    if (customAccentColor.isValid()) {
        settings.setValue("customAccentColor", customAccentColor.name());
    }
    settings.setValue("useBrighterPalette", useBrighterPalette);
}

void MainWindow::loadThemeSettings() {
    QSettings settings("SpeedyNote", "App");
    useCustomAccentColor = settings.value("useCustomAccentColor", false).toBool();
    QString colorName = settings.value("customAccentColor", "#0078D4").toString();
    customAccentColor = QColor(colorName);
    useBrighterPalette = settings.value("useBrighterPalette", false).toBool();
    
    // Ensure valid values
    if (!customAccentColor.isValid()) {
        customAccentColor = QColor("#0078D4"); // Default blue
    }
    
    // Apply theme immediately after loading
    updateTheme();
}

// performance optimizations
void MainWindow::setLowResPreviewEnabled(bool enabled) {
    lowResPreviewEnabled = enabled;

    QSettings settings("SpeedyNote", "App");
    settings.setValue("lowResPreviewEnabled", enabled);
}

bool MainWindow::isLowResPreviewEnabled() const {
    return lowResPreviewEnabled;
}

// ui optimizations

bool MainWindow::areBenchmarkControlsVisible() const {
    return benchmarkButton->isVisible() && benchmarkLabel->isVisible();
}

void MainWindow::setBenchmarkControlsVisible(bool visible) {
    benchmarkButton->setVisible(visible);
    benchmarkLabel->setVisible(visible);
}

bool MainWindow::areZoomButtonsVisible() const {
    return zoomButtonsVisible;
}

void MainWindow::setZoomButtonsVisible(bool visible) {
    zoom50Button->setVisible(visible);
    dezoomButton->setVisible(visible);
    zoom200Button->setVisible(visible);

    QSettings settings("SpeedyNote", "App");
    settings.setValue("zoomButtonsVisible", visible);
    
    // Update zoomButtonsVisible flag and trigger layout update
    zoomButtonsVisible = visible;
    
    // Trigger layout update to adjust responsive thresholds
    if (layoutUpdateTimer) {
        layoutUpdateTimer->stop();
        layoutUpdateTimer->start(50); // Quick update for settings change
    } else {
        updateToolbarLayout(); // Direct update if no timer exists yet
    }
}



bool MainWindow::isScrollOnTopEnabled() const {
    return scrollOnTopEnabled;
}

void MainWindow::setScrollOnTopEnabled(bool enabled) {
    scrollOnTopEnabled = enabled;

    QSettings settings("SpeedyNote", "App");
    settings.setValue("scrollOnTopEnabled", enabled);
}

bool MainWindow::areTouchGesturesEnabled() const {
    return touchGesturesEnabled;
}

void MainWindow::setTouchGesturesEnabled(bool enabled) {
    touchGesturesEnabled = enabled;
    
    // Apply to all canvases
    for (int i = 0; i < canvasStack->count(); ++i) {
        InkCanvas *canvas = qobject_cast<InkCanvas*>(canvasStack->widget(i));
        if (canvas) {
            canvas->setTouchGesturesEnabled(enabled);
        }
    }
    
    QSettings settings("SpeedyNote", "App");
    settings.setValue("touchGesturesEnabled", enabled);
}

void MainWindow::setTemporaryDialMode(DialMode mode) {
    if (temporaryDialMode == None) {
        temporaryDialMode = currentDialMode;
    }
    changeDialMode(mode);
}

void MainWindow::clearTemporaryDialMode() {
    if (temporaryDialMode != None) {
        changeDialMode(temporaryDialMode);
        temporaryDialMode = None;
    }
}



void MainWindow::handleButtonHeld(const QString &buttonName) {
    QString mode = buttonHoldMapping.value(buttonName, "None");
    if (mode != "None") {
        setTemporaryDialMode(dialModeFromString(mode));
        return;
    }
}

void MainWindow::handleButtonReleased(const QString &buttonName) {
    QString mode = buttonHoldMapping.value(buttonName, "None");
    if (mode != "None") {
        clearTemporaryDialMode();
    }
}

void MainWindow::setHoldMapping(const QString &buttonName, const QString &dialMode) {
    buttonHoldMapping[buttonName] = dialMode;
}

void MainWindow::setPressMapping(const QString &buttonName, const QString &action) {
    buttonPressMapping[buttonName] = action;
    buttonPressActionMapping[buttonName] = stringToAction(action);  // ✅ THIS LINE WAS MISSING
}


DialMode MainWindow::dialModeFromString(const QString &mode) {
    // Convert internal key to our existing DialMode enum
    InternalDialMode internalMode = ButtonMappingHelper::internalKeyToDialMode(mode);
    
    switch (internalMode) {
        case InternalDialMode::None: return PageSwitching; // Default fallback
        case InternalDialMode::PageSwitching: return PageSwitching;
        case InternalDialMode::ZoomControl: return ZoomControl;
        case InternalDialMode::ThicknessControl: return ThicknessControl;

        case InternalDialMode::ToolSwitching: return ToolSwitching;
        case InternalDialMode::PresetSelection: return PresetSelection;
        case InternalDialMode::PanAndPageScroll: return PanAndPageScroll;
    }
    return PanAndPageScroll;  // Default fallback
}

// MainWindow.cpp

QString MainWindow::getHoldMapping(const QString &buttonName) {
    return buttonHoldMapping.value(buttonName, "None");
}

QString MainWindow::getPressMapping(const QString &buttonName) {
    return buttonPressMapping.value(buttonName, "None");
}

void MainWindow::saveButtonMappings() {
    QSettings settings("SpeedyNote", "App");

    settings.beginGroup("ButtonHoldMappings");
    for (auto it = buttonHoldMapping.begin(); it != buttonHoldMapping.end(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();

    settings.beginGroup("ButtonPressMappings");
    for (auto it = buttonPressMapping.begin(); it != buttonPressMapping.end(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
}

void MainWindow::loadButtonMappings() {
    QSettings settings("SpeedyNote", "App");

    // First, check if we need to migrate old settings
    migrateOldButtonMappings();

    settings.beginGroup("ButtonHoldMappings");
    QStringList holdKeys = settings.allKeys();
    for (const QString &key : holdKeys) {
        buttonHoldMapping[key] = settings.value(key, "none").toString();
    }
    settings.endGroup();

    settings.beginGroup("ButtonPressMappings");
    QStringList pressKeys = settings.allKeys();
    for (const QString &key : pressKeys) {
        QString value = settings.value(key, "none").toString();
        buttonPressMapping[key] = value;

        // ✅ Convert internal key to action enum
        buttonPressActionMapping[key] = stringToAction(value);
    }
    settings.endGroup();
}

void MainWindow::migrateOldButtonMappings() {
    QSettings settings("SpeedyNote", "App");
    
    // Check if migration is needed by looking for old format strings
    settings.beginGroup("ButtonHoldMappings");
    QStringList holdKeys = settings.allKeys();
    bool needsMigration = false;
    
    for (const QString &key : holdKeys) {
        QString value = settings.value(key).toString();
        // If we find old English strings, we need to migrate
        if (value == "PageSwitching" || value == "ZoomControl" || value == "ThicknessControl" ||
            value == "ToolSwitching" || value == "PresetSelection" ||
            value == "PanAndPageScroll") {
            needsMigration = true;
            break;
        }
    }
    settings.endGroup();
    
    if (!needsMigration) {
        settings.beginGroup("ButtonPressMappings");
        QStringList pressKeys = settings.allKeys();
        for (const QString &key : pressKeys) {
            QString value = settings.value(key).toString();
            // Check for old English action strings
            if (value == "Toggle Fullscreen" || value == "Toggle Dial" || value == "Zoom 50%" ||
                value == "Add Preset" || value == "Delete Page" || value == "Fast Forward" ||
                value == "Open Control Panel" || value == "Custom Color") {
                needsMigration = true;
                break;
            }
        }
        settings.endGroup();
    }
    
    if (!needsMigration) return;
    
    // Perform migration
    // qDebug() << "Migrating old button mappings to new format...";
    
    // Migrate hold mappings
    settings.beginGroup("ButtonHoldMappings");
    holdKeys = settings.allKeys();
    for (const QString &key : holdKeys) {
        QString oldValue = settings.value(key).toString();
        QString newValue = migrateOldDialModeString(oldValue);
        if (newValue != oldValue) {
            settings.setValue(key, newValue);
        }
    }
    settings.endGroup();
    
    // Migrate press mappings
    settings.beginGroup("ButtonPressMappings");
    QStringList pressKeys = settings.allKeys();
    for (const QString &key : pressKeys) {
        QString oldValue = settings.value(key).toString();
        QString newValue = migrateOldActionString(oldValue);
        if (newValue != oldValue) {
            settings.setValue(key, newValue);
        }
    }
    settings.endGroup();
    
   // qDebug() << "Button mapping migration completed.";
}

QString MainWindow::migrateOldDialModeString(const QString &oldString) {
    // Convert old English strings to new internal keys
    if (oldString == "None") return "none";
    if (oldString == "PageSwitching") return "page_switching";
    if (oldString == "ZoomControl") return "zoom_control";
    if (oldString == "ThicknessControl") return "thickness_control";

    if (oldString == "ToolSwitching") return "tool_switching";
    if (oldString == "PresetSelection") return "preset_selection";
    if (oldString == "PanAndPageScroll") return "pan_and_page_scroll";
    return oldString; // Return as-is if not found (might already be new format)
}

QString MainWindow::migrateOldActionString(const QString &oldString) {
    // Convert old English strings to new internal keys
    if (oldString == "None") return "none";
    if (oldString == "Toggle Fullscreen") return "toggle_fullscreen";
    if (oldString == "Toggle Dial") return "toggle_dial";
    if (oldString == "Zoom 50%") return "zoom_50";
    if (oldString == "Zoom Out") return "zoom_out";
    if (oldString == "Zoom 200%") return "zoom_200";
    if (oldString == "Add Preset") return "add_preset";
    if (oldString == "Delete Page") return "delete_page";
    if (oldString == "Fast Forward") return "fast_forward";
    if (oldString == "Open Control Panel") return "open_control_panel";
    if (oldString == "Red") return "red_color";
    if (oldString == "Blue") return "blue_color";
    if (oldString == "Yellow") return "yellow_color";
    if (oldString == "Green") return "green_color";
    if (oldString == "Black") return "black_color";
    if (oldString == "White") return "white_color";
    if (oldString == "Custom Color") return "custom_color";
    if (oldString == "Toggle Sidebar") return "toggle_sidebar";
    if (oldString == "Save") return "save";
    if (oldString == "Straight Line Tool") return "straight_line_tool";
    if (oldString == "Rope Tool") return "rope_tool";
    if (oldString == "Set Pen Tool") return "set_pen_tool";
    if (oldString == "Set Marker Tool") return "set_marker_tool";
    if (oldString == "Set Eraser Tool") return "set_eraser_tool";
    if (oldString == "Toggle PDF Text Selection") return "toggle_pdf_text_selection";
    return oldString; // Return as-is if not found (might already be new format)
}
void MainWindow::handleControllerButton(const QString &buttonName) {  // This is for single press functions
    ControllerAction action = buttonPressActionMapping.value(buttonName, ControllerAction::None);

    switch (action) {
        case ControllerAction::ToggleFullscreen:
            fullscreenButton->click();
            break;
        case ControllerAction::ToggleDial:
            toggleDial();
            break;
        case ControllerAction::Zoom50:
            zoom50Button->click();
            break;
        case ControllerAction::ZoomOut:
            dezoomButton->click();
            break;
        case ControllerAction::Zoom200:
            zoom200Button->click();
            break;
        case ControllerAction::AddPreset:
            addPresetButton->click();
            break;
        case ControllerAction::DeletePage:
            deletePageButton->click();  // assuming you have this
            break;
        case ControllerAction::FastForward:
            fastForwardButton->click();  // assuming you have this
            break;
        case ControllerAction::OpenControlPanel:
            openControlPanelButton->click();
            break;
        case ControllerAction::RedColor:
            redButton->click();
            break;
        case ControllerAction::BlueColor:
            blueButton->click();
            break;
        case ControllerAction::YellowColor:
            yellowButton->click();
            break;
        case ControllerAction::GreenColor:
            greenButton->click();
            break;
        case ControllerAction::BlackColor:
            blackButton->click();
            break;
        case ControllerAction::WhiteColor:
            whiteButton->click();
            break;
        case ControllerAction::CustomColor:
            customColorButton->click();
            break;
        case ControllerAction::ToggleSidebar:
            toggleTabBarButton->click();
            break;
        case ControllerAction::Save:
            saveButton->click();
            break;
        case ControllerAction::StraightLineTool:
            straightLineToggleButton->click();
            break;
        case ControllerAction::RopeTool:
            ropeToolButton->click();
            break;
        case ControllerAction::SetPenTool:
            setPenTool();
            break;
        case ControllerAction::SetMarkerTool:
            setMarkerTool();
            break;
        case ControllerAction::SetEraserTool:
            setEraserTool();
            break;
        case ControllerAction::TogglePdfTextSelection:
            pdfTextSelectButton->click();
            break;
        case ControllerAction::ToggleOutline:
            toggleOutlineButton->click();
            break;
        case ControllerAction::ToggleBookmarks:
            toggleBookmarksButton->click();
            break;
        case ControllerAction::AddBookmark:
            toggleBookmarkButton->click();
            break;
        case ControllerAction::ToggleTouchGestures:
            touchGesturesButton->click();
            break;
        case ControllerAction::PreviousPage:
            goToPreviousPage();
            break;
        case ControllerAction::NextPage:
            goToNextPage();
            break;
        default:
            break;
    }
}




void MainWindow::openPdfFile(const QString &pdfPath) {
    // Check if the PDF file exists
    if (!QFile::exists(pdfPath)) {
        QMessageBox::warning(this, tr("File Not Found"), tr("The PDF file could not be found:\n%1").arg(pdfPath));
        return;
    }

    // First, check if there's already a valid notebook folder for this PDF
    QString existingFolderPath;
    if (PdfOpenDialog::hasValidNotebookFolder(pdfPath, existingFolderPath)) {
        // Found a valid notebook folder, open it directly without showing dialog
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;

        // Save current work if edited
        if (canvas->isEdited()) {
            saveCurrentPage();
        }
        
        // Set the existing folder as save folder
        canvas->setSaveFolder(existingFolderPath);
        
        // Load the PDF
        canvas->loadPdf(pdfPath);
        
        // ✅ Automatically enable scroll on top when PDF is loaded (required for pseudo smooth scrolling)
        setScrollOnTopEnabled(true);
        
        // Update tab label
        updateTabLabel();
        updateBookmarkButtonState(); // ✅ Update bookmark button state after loading notebook
        
        // ✅ Show last accessed page dialog if available
        if (!showLastAccessedPageDialog(canvas)) {
            // No last accessed page, start from page 1
            switchPageWithDirection(1, 1);
            pageInput->setValue(1);
        } else {
            // Dialog handled page switching, update page input
            pageInput->setValue(getCurrentPageForCanvas(canvas) + 1);
        }
        updateZoom();
        updatePanRange();
        
        // ✅ Add to recent notebooks AFTER PDF is loaded to ensure proper thumbnail generation
        if (recentNotebooksManager) {
            // Use QPointer to safely handle canvas deletion
            QPointer<InkCanvas> canvasPtr(canvas);
            
            // Check if PDF is already loaded
            if (canvasPtr && canvasPtr->isPdfLoadedFunc()) {
                // PDF is already loaded, add to recent notebooks immediately
                recentNotebooksManager->addRecentNotebook(existingFolderPath, canvasPtr.data());
                // Refresh shared launcher if it exists and is visible
                if (sharedLauncher && sharedLauncher->isVisible()) {
                    sharedLauncher->refreshRecentNotebooks();
                }
            } else {
                // PDF is still loading, wait for pdfLoaded signal
                // Use a shared_ptr to safely manage the connection lifetime
                auto connection = std::make_shared<QMetaObject::Connection>();
                *connection = connect(canvasPtr.data(), &InkCanvas::pdfLoaded, this, [this, existingFolderPath, canvasPtr, connection]() {
                    if (recentNotebooksManager && canvasPtr && !canvasPtr.isNull()) {
                        // Add to recent notebooks immediately - RecentNotebooksManager handles delayed thumbnail generation
                        recentNotebooksManager->addRecentNotebook(existingFolderPath, canvasPtr.data());
                        // Refresh shared launcher if it exists and is visible
                        if (sharedLauncher && sharedLauncher->isVisible()) {
                            sharedLauncher->refreshRecentNotebooks();
                        }
                    }
                    // Disconnect the signal to avoid multiple calls
                    if (connection && *connection) {
                        disconnect(*connection);
                    }
                });
            }
        }
        
        return; // Exit early, no need to show dialog
    }
    
    // No valid notebook folder found, show the dialog with options
    PdfOpenDialog dialog(pdfPath, this);
    dialog.exec();
    
    PdfOpenDialog::Result result = dialog.getResult();
    QString selectedFolder = dialog.getSelectedFolder();
    
    if (result == PdfOpenDialog::Cancel) {
        return; // User cancelled, do nothing
    }
    
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;
    
    // Save current work if edited
    if (canvas->isEdited()) {
        saveCurrentPage();
    }
    
    if (result == PdfOpenDialog::CreateNewFolder) {
        // Set the new folder as save folder
        canvas->setSaveFolder(selectedFolder);
        
        // ✅ Apply default background settings to new PDF notebook
        applyDefaultBackgroundToCanvas(canvas);
        
        // Load the PDF
        canvas->loadPdf(pdfPath);
        
        // ✅ Automatically enable scroll on top when PDF is loaded (required for pseudo smooth scrolling)
        setScrollOnTopEnabled(true);
        
        // Update tab label
        updateTabLabel();
        
        // Switch to page 1 for new folders (no last accessed page)
        switchPageWithDirection(1, 1);
        pageInput->setValue(1);
        updateZoom();
        updatePanRange();
        
        // ✅ Add to recent notebooks AFTER PDF is loaded to ensure proper thumbnail generation
        if (recentNotebooksManager) {
            // Use QPointer to safely handle canvas deletion
            QPointer<InkCanvas> canvasPtr(canvas);
            
            // Check if PDF is already loaded
            if (canvasPtr && canvasPtr->isPdfLoadedFunc()) {
                // PDF is already loaded, add to recent notebooks immediately
                recentNotebooksManager->addRecentNotebook(selectedFolder, canvasPtr.data());
                // Refresh shared launcher if it exists and is visible
                if (sharedLauncher && sharedLauncher->isVisible()) {
                    sharedLauncher->refreshRecentNotebooks();
                }
            } else {
                // PDF is still loading, wait for pdfLoaded signal
                // Use a shared_ptr to safely manage the connection lifetime
                auto connection = std::make_shared<QMetaObject::Connection>();
                *connection = connect(canvasPtr.data(), &InkCanvas::pdfLoaded, this, [this, selectedFolder, canvasPtr, connection]() {
                    if (recentNotebooksManager && canvasPtr && !canvasPtr.isNull()) {
                        // Add to recent notebooks immediately - RecentNotebooksManager handles delayed thumbnail generation
                        recentNotebooksManager->addRecentNotebook(selectedFolder, canvasPtr.data());
                    }
                    // Disconnect the signal to avoid multiple calls
                    if (connection && *connection) {
                        disconnect(*connection);
                    }
                });
            }
        }
        
    } else if (result == PdfOpenDialog::UseExistingFolder) {
        // ✅ Check if the existing folder is linked to the same PDF using JSON metadata
        canvas->setSaveFolder(selectedFolder); // Load metadata first
        QString existingPdfPath = canvas->getPdfPath();
        bool isLinkedToSamePdf = false;
        
        if (!existingPdfPath.isEmpty()) {
            // Compare absolute paths
            QFileInfo existingInfo(existingPdfPath);
            QFileInfo newInfo(pdfPath);
            isLinkedToSamePdf = (existingInfo.absoluteFilePath() == newInfo.absoluteFilePath());
        }
        
        if (!isLinkedToSamePdf && !existingPdfPath.isEmpty()) {
            // Folder is linked to a different PDF, ask user what to do
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                tr("Different PDF Linked"),
                tr("This notebook folder is already linked to a different PDF file.\n\nDo you want to replace the link with the new PDF?"),
                QMessageBox::Yes | QMessageBox::No
            );
            
            if (reply == QMessageBox::No) {
                return; // User chose not to replace
            }
        }
        
        // Set the existing folder as save folder
        canvas->setSaveFolder(selectedFolder);
        
        // ✅ Handle missing PDF file if it's a .spn package
        if (SpnPackageManager::isSpnPackage(selectedFolder)) {
            if (!canvas->handleMissingPdf(this)) {
                // User cancelled PDF relinking, don't continue
                return;
            }
            // ✅ Update scroll behavior based on PDF loading state after relinking
            setScrollOnTopEnabled(canvas->isPdfLoadedFunc());
        } else {
            // Load the PDF for regular folders
            canvas->loadPdf(pdfPath);
            // ✅ Automatically enable scroll on top when PDF is loaded (required for pseudo smooth scrolling)
            setScrollOnTopEnabled(true);
        }
        
        // Update tab label
        updateTabLabel();
        updateBookmarkButtonState(); // ✅ Update bookmark button state after loading notebook
        
        // ✅ Show last accessed page dialog if available for existing folders
        if (!showLastAccessedPageDialog(canvas)) {
            // No last accessed page or user chose page 1
            switchPageWithDirection(1, 1);
            pageInput->setValue(1);
        } else {
            // Dialog handled page switching, update page input
            pageInput->setValue(getCurrentPageForCanvas(canvas) + 1);
        }
        updateZoom();
        updatePanRange();
        
        // ✅ Add to recent notebooks AFTER PDF is loaded to ensure proper thumbnail generation
        if (recentNotebooksManager) {
            // Use QPointer to safely handle canvas deletion
            QPointer<InkCanvas> canvasPtr(canvas);
            
            // Check if PDF is already loaded
            if (canvasPtr && canvasPtr->isPdfLoadedFunc()) {
                // PDF is already loaded, add to recent notebooks immediately
                recentNotebooksManager->addRecentNotebook(selectedFolder, canvasPtr.data());
                // Refresh shared launcher if it exists and is visible
                if (sharedLauncher && sharedLauncher->isVisible()) {
                    sharedLauncher->refreshRecentNotebooks();
                }
            } else {
                // PDF is still loading, wait for pdfLoaded signal
                // Use a shared_ptr to safely manage the connection lifetime
                auto connection = std::make_shared<QMetaObject::Connection>();
                *connection = connect(canvasPtr.data(), &InkCanvas::pdfLoaded, this, [this, selectedFolder, canvasPtr, connection]() {
                    if (recentNotebooksManager && canvasPtr && !canvasPtr.isNull()) {
                        // Add to recent notebooks immediately - RecentNotebooksManager handles delayed thumbnail generation
                        recentNotebooksManager->addRecentNotebook(selectedFolder, canvasPtr.data());
                    }
                    // Disconnect the signal to avoid multiple calls
                    if (connection && *connection) {
                        disconnect(*connection);
                    }
                });
            }
        }
    }
}

void MainWindow::setPdfDPI(int dpi) {
    if (dpi != pdfRenderDPI) {
        pdfRenderDPI = dpi;
        savePdfDPI(dpi);

        // Apply immediately to current canvas if needed
        if (currentCanvas()) {
            currentCanvas()->setPDFRenderDPI(dpi);
            currentCanvas()->clearPdfCache();
            currentCanvas()->loadPdfPage(getCurrentPageForCanvas(currentCanvas()));  // Optional: add this if needed
            updateZoom();
            updatePanRange();
        }
    }
}

void MainWindow::savePdfDPI(int dpi) {
    QSettings settings("SpeedyNote", "App");
    settings.setValue("pdfRenderDPI", dpi);
}

void MainWindow::loadUserSettings() {
    QSettings settings("SpeedyNote", "App");

    // Load low-res toggle
    lowResPreviewEnabled = settings.value("lowResPreviewEnabled", true).toBool();
    setLowResPreviewEnabled(lowResPreviewEnabled);

    
    zoomButtonsVisible = settings.value("zoomButtonsVisible", true).toBool();
    setZoomButtonsVisible(zoomButtonsVisible);

    scrollOnTopEnabled = settings.value("scrollOnTopEnabled", true).toBool();
    setScrollOnTopEnabled(scrollOnTopEnabled);

    touchGesturesEnabled = settings.value("touchGesturesEnabled", true).toBool();
    setTouchGesturesEnabled(touchGesturesEnabled);
    
    // Update button visual state to match loaded setting
    touchGesturesButton->setProperty("selected", touchGesturesEnabled);
    touchGesturesButton->style()->unpolish(touchGesturesButton);
    touchGesturesButton->style()->polish(touchGesturesButton);
    
    // Initialize default background settings if they don't exist
    if (!settings.contains("defaultBackgroundStyle")) {
        saveDefaultBackgroundSettings(BackgroundStyle::Grid, Qt::white, 30);
    }
    
    // Load keyboard mappings
    loadKeyboardMappings();
    
    // Load theme settings
    loadThemeSettings();
}

void MainWindow::toggleControlBar() {
    // Proper fullscreen toggle: handle both sidebar and control bar
    
    if (controlBarVisible) {
        // Going into fullscreen mode
        
        // First, remember current tab bar state
        sidebarWasVisibleBeforeFullscreen = tabBarContainer->isVisible();
        
        // Hide tab bar if it's visible
        if (tabBarContainer->isVisible()) {
            tabBarContainer->setVisible(false);
        }
        
        // Hide control bar
        controlBarVisible = false;
        controlBar->setVisible(false);
        
        // Hide floating popup widgets when control bar is hidden to prevent stacking
        if (zoomFrame && zoomFrame->isVisible()) zoomFrame->hide();
        if (thicknessFrame && thicknessFrame->isVisible()) thicknessFrame->hide();
        
        // Hide orphaned widgets that are not added to any layout
        if (colorPreview) colorPreview->hide();
        if (thicknessButton) thicknessButton->hide();
        if (jumpToPageButton) jumpToPageButton->hide();

        if (toolSelector) toolSelector->hide();
        if (zoomButton) zoomButton->hide();
        if (customColorInput) customColorInput->hide();
        
        // Find and hide local widgets that might be orphaned
        QList<QComboBox*> comboBoxes = findChildren<QComboBox*>();
        for (QComboBox* combo : comboBoxes) {
            if (combo->parent() == this && !combo->isVisible()) {
                // Already hidden, keep it hidden
            } else if (combo->parent() == this) {
                // This might be the orphaned dialModeSelector or similar
                combo->hide();
            }
        }
    } else {
        // Coming out of fullscreen mode
        
        // Restore control bar
        controlBarVisible = true;
        controlBar->setVisible(true);
        
        // Restore tab bar to its previous state
        tabBarContainer->setVisible(sidebarWasVisibleBeforeFullscreen);
        
        // Show orphaned widgets when control bar is visible
        // Note: These are kept hidden normally since they're not in the layout
        // Only show them if they were specifically intended to be visible
    }
    
    // Update dial display to reflect new status
    updateDialDisplay();
    
    // Force layout update to recalculate space
    if (auto *canvas = currentCanvas()) {
        QTimer::singleShot(0, this, [this, canvas]() {
            canvas->setMaximumSize(canvas->getCanvasSize());
        });
    }
}

void MainWindow::cycleZoomLevels() {
    if (!zoomSlider) return;
    
    int currentZoom = zoomSlider->value();
    int targetZoom;
    
    // Calculate the scaled zoom levels based on initial DPR
    int zoom50 = 50 / initialDpr;
    int zoom100 = 100 / initialDpr;
    int zoom200 = 200 / initialDpr;
    
    // Cycle through 0.5x -> 1x -> 2x -> 0.5x...
    if (currentZoom <= zoom50 + 5) { // Close to 0.5x (with small tolerance)
        targetZoom = zoom100; // Go to 1x
    } else if (currentZoom <= zoom100 + 5) { // Close to 1x
        targetZoom = zoom200; // Go to 2x
    } else { // Any other zoom level or close to 2x
        targetZoom = zoom50; // Go to 0.5x
    }
    
    zoomSlider->setValue(targetZoom);
    updateZoom();
    updateDialDisplay();
}

void MainWindow::handleTouchZoomChange(int newZoom) {
    // Update zoom slider without triggering updateZoom again
    zoomSlider->blockSignals(true);
    int oldZoom = zoomSlider->value();  // Get the old zoom value before updating the slider
    zoomSlider->setValue(newZoom);
    zoomSlider->blockSignals(false);
    
    // Show both scrollbars during gesture
    if (panXSlider->maximum() > 0) {
        panXSlider->setVisible(true);
    }
    if (panYSlider->maximum() > 0) {
        panYSlider->setVisible(true);
    }
    scrollbarsVisible = true;
    
    // Update canvas zoom directly
    InkCanvas *canvas = currentCanvas();
    if (canvas) {
        // The canvas zoom has already been set by the gesture processing in InkCanvas::event()
        // So we don't need to set it again, just update the last zoom level
        canvas->setLastZoomLevel(newZoom);
        updatePanRange();
        
        // ✅ FIXED: Add thickness adjustment for pinch-to-zoom gestures to maintain visual consistency
        adjustThicknessForZoom(oldZoom, newZoom);
        
        updateDialDisplay();
    }
}

void MainWindow::handleTouchPanChange(int panX, int panY) {
    // Clamp values to valid ranges
    panX = qBound(panXSlider->minimum(), panX, panXSlider->maximum());
    panY = qBound(panYSlider->minimum(), panY, panYSlider->maximum());
    
    // Show both scrollbars during gesture
    if (panXSlider->maximum() > 0) {
        panXSlider->setVisible(true);
    }
    if (panYSlider->maximum() > 0) {
        panYSlider->setVisible(true);
    }
    scrollbarsVisible = true;
    
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;
    
    // ⚡ OPTIMIZATION: During touch panning, only update sliders (don't call setPanX/setPanY)
    // The pan offsets are already set by setPanWithTouchScroll() in InkCanvas
    // Calling setPanX/setPanY would trigger update() and defeat the optimization
    if (canvas->isTouchPanningActive()) {
        // Just update the slider positions without triggering canvas updates
        panXSlider->blockSignals(true);
        panXSlider->setValue(panX);
        panXSlider->blockSignals(false);
        
        panYSlider->blockSignals(true);
        panYSlider->setValue(panY);
        panYSlider->blockSignals(false);
        
        // Store the pan values for later use
        canvas->setLastPanX(panX);
        canvas->setLastPanY(panY);
    } else {
        // Normal flow for non-touch panning (mouse wheel, etc.)
        // Update sliders - allow panY signals for autoscroll, block panX to avoid redundancy
        panXSlider->blockSignals(true);
        panXSlider->setValue(panX);
        panXSlider->blockSignals(false);
        
        // For panY: DON'T block signals so it triggers the same autoscroll flow as mouse wheel
        panYSlider->setValue(panY); // This will trigger updatePanY() -> setPanY() -> autoscroll logic
        
        // Update canvas X pan directly (Y pan is handled by the setValue signal above)
        canvas->setPanX(panX);
        canvas->setLastPanX(panX);
        // Note: panY is handled by panYSlider->setValue() signal flow above
    }
}

// Now add two new slots to handle gesture end events
void MainWindow::handleTouchGestureEnd() {
    // Hide scrollbars immediately when touch gesture ends
    panXSlider->setVisible(false);
    panYSlider->setVisible(false);
    scrollbarsVisible = false;
}

void MainWindow::handleTouchPanningChanged(bool active) {
    // ✅ PERFORMANCE: Control window frame-only mode for touch panning
    InkCanvas* canvas = currentCanvas();
    if (!canvas) return;
    
    MarkdownWindowManager* markdownMgr = canvas->getMarkdownManager();
    PictureWindowManager* pictureMgr = canvas->getPictureManager();
    
    if (active) {
        // Touch panning started - enable frame-only mode for performance
        if (markdownMgr) {
            markdownMgr->setWindowsFrameOnlyMode(true);
        }
        if (pictureMgr) {
            pictureMgr->setWindowsFrameOnlyMode(true);
        }
    } else {
        // Touch panning ended - restore full window display
        if (markdownMgr) {
            markdownMgr->setWindowsFrameOnlyMode(false);
        }
        if (pictureMgr) {
            pictureMgr->setWindowsFrameOnlyMode(false);
        }
    }
}

void MainWindow::updateColorButtonStates() {
    // Check if there's a current canvas
    if (!currentCanvas()) return;
    
    // Get current pen color
    QColor currentColor = currentCanvas()->getPenColor();
    
    // Determine if we're in dark mode to match the correct colors
    bool darkMode = isDarkMode();
    
    // Reset all color buttons to original style
    redButton->setProperty("selected", false);
    blueButton->setProperty("selected", false);
    yellowButton->setProperty("selected", false);
    greenButton->setProperty("selected", false);
    blackButton->setProperty("selected", false);
    whiteButton->setProperty("selected", false);
    
    // Set the selected property for the matching color button based on current palette
    QColor redColor = getPaletteColor("red");
    QColor blueColor = getPaletteColor("blue");
    QColor yellowColor = getPaletteColor("yellow");
    QColor greenColor = getPaletteColor("green");
    
    if (currentColor == redColor) {
        redButton->setProperty("selected", true);
    } else if (currentColor == blueColor) {
        blueButton->setProperty("selected", true);
    } else if (currentColor == yellowColor) {
        yellowButton->setProperty("selected", true);
    } else if (currentColor == greenColor) {
        greenButton->setProperty("selected", true);
    } else if (currentColor == QColor("#000000")) {
        blackButton->setProperty("selected", true);
    } else if (currentColor == QColor("#FFFFFF")) {
        whiteButton->setProperty("selected", true);
    }
    
    // Force style update
    redButton->style()->unpolish(redButton);
    redButton->style()->polish(redButton);
    blueButton->style()->unpolish(blueButton);
    blueButton->style()->polish(blueButton);
    yellowButton->style()->unpolish(yellowButton);
    yellowButton->style()->polish(yellowButton);
    greenButton->style()->unpolish(greenButton);
    greenButton->style()->polish(greenButton);
    blackButton->style()->unpolish(blackButton);
    blackButton->style()->polish(blackButton);
    whiteButton->style()->unpolish(whiteButton);
    whiteButton->style()->polish(whiteButton);
}

void MainWindow::selectColorButton(QPushButton* selectedButton) {
    updateColorButtonStates();
}

QColor MainWindow::getContrastingTextColor(const QColor &backgroundColor) {
    // Calculate relative luminance using the formula from WCAG 2.0
    double r = backgroundColor.redF();
    double g = backgroundColor.greenF();
    double b = backgroundColor.blueF();
    
    // Gamma correction
    r = (r <= 0.03928) ? r/12.92 : pow((r + 0.055)/1.055, 2.4);
    g = (g <= 0.03928) ? g/12.92 : pow((g + 0.055)/1.055, 2.4);
    b = (b <= 0.03928) ? b/12.92 : pow((b + 0.055)/1.055, 2.4);
    
    // Calculate luminance
    double luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    
    // Use white text for darker backgrounds
    return (luminance < 0.5) ? Qt::white : Qt::black;
}

void MainWindow::updateCustomColorButtonStyle(const QColor &color) {
    QColor textColor = getContrastingTextColor(color);
    customColorButton->setStyleSheet(QString("background-color: %1; color: %2")
        .arg(color.name())
        .arg(textColor.name()));
    customColorButton->setText(QString("%1").arg(color.name()).toUpper());
}

void MainWindow::updateStraightLineButtonState() {
    // Check if there's a current canvas
    if (!currentCanvas()) return;
    
    // Update the button state to match the canvas straight line mode
    bool isEnabled = currentCanvas()->isStraightLineMode();
    
    // Set visual indicator that the button is active/inactive
    if (straightLineToggleButton) {
        straightLineToggleButton->setProperty("selected", isEnabled);
        
        // Force style update
        straightLineToggleButton->style()->unpolish(straightLineToggleButton);
        straightLineToggleButton->style()->polish(straightLineToggleButton);
    }
}

void MainWindow::updateRopeToolButtonState() {
    // Check if there's a current canvas
    if (!currentCanvas()) return;

    // Update the button state to match the canvas rope tool mode
    bool isEnabled = currentCanvas()->isRopeToolMode();

    // Set visual indicator that the button is active/inactive
    if (ropeToolButton) {
        ropeToolButton->setProperty("selected", isEnabled);

        // Force style update
        ropeToolButton->style()->unpolish(ropeToolButton);
        ropeToolButton->style()->polish(ropeToolButton);
    }
}

void MainWindow::updateMarkdownButtonState() {
    // Check if there's a current canvas
    if (!currentCanvas()) return;

    // Update the button state to match the canvas markdown selection mode
    bool isEnabled = currentCanvas()->isMarkdownSelectionMode();

    // Set visual indicator that the button is active/inactive
    if (markdownButton) {
        markdownButton->setProperty("selected", isEnabled);

        // Force style update
        markdownButton->style()->unpolish(markdownButton);
        markdownButton->style()->polish(markdownButton);
    }
}

void MainWindow::updatePictureButtonState() {
    // Check if there's a current canvas
    if (!currentCanvas()) return;

    // Update the button state to match the canvas picture selection mode
    bool isEnabled = currentCanvas()->isPictureSelectionMode();

    // Set visual indicator that the button is active/inactive
    if (insertPictureButton) {
        insertPictureButton->setProperty("selected", isEnabled);

        // Force style update
        insertPictureButton->style()->unpolish(insertPictureButton);
        insertPictureButton->style()->polish(insertPictureButton);
    }
}

void MainWindow::onAnnotatedImageSaved(const QString &filePath) {
    // ✅ Show success message to user
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QString dirPath = fileInfo.absolutePath();
    
    QMessageBox::information(this, tr("Annotated Image Saved"), 
        tr("Annotated page saved successfully!\n\n"
           "File: %1\n"
           "Location: %2").arg(fileName, dirPath));
}

void MainWindow::updateDialButtonState() {
    // Check if dial is visible
    bool isDialVisible = dialContainer && dialContainer->isVisible();
    
    if (dialToggleButton) {
        dialToggleButton->setProperty("selected", isDialVisible);
        
        // Force style update
        dialToggleButton->style()->unpolish(dialToggleButton);
        dialToggleButton->style()->polish(dialToggleButton);
    }
}

void MainWindow::updateFastForwardButtonState() {
    if (fastForwardButton) {
        fastForwardButton->setProperty("selected", fastForwardMode);
        
        // Force style update
        fastForwardButton->style()->unpolish(fastForwardButton);
        fastForwardButton->style()->polish(fastForwardButton);
    }
}

// Add this new method
void MainWindow::updateScrollbarPositions() {
    QWidget *container = canvasStack->parentWidget();
    if (!container || !panXSlider || !panYSlider) return;
    
    // Add small margins for better visibility
    const int margin = 3;
    
    // Get scrollbar dimensions
    const int scrollbarWidth = panYSlider->width();
    const int scrollbarHeight = panXSlider->height();
    
    // Calculate sizes based on container
    int containerWidth = container->width();
    int containerHeight = container->height();
    
    // Leave a bit of space for the corner
    int cornerOffset = 15;
    
    // Position horizontal scrollbar at top
    panXSlider->setGeometry(
        cornerOffset + margin,  // Leave space at left corner
        margin,
        containerWidth - cornerOffset - margin*2,  // Full width minus corner and right margin
        scrollbarHeight
    );
    
    // Position vertical scrollbar at left
    panYSlider->setGeometry(
        margin,
        cornerOffset + margin,  // Leave space at top corner
        scrollbarWidth,
        containerHeight - cornerOffset - margin*2  // Full height minus corner and bottom margin
    );
}
// Add the new helper method for edge detection
void MainWindow::handleEdgeProximity(InkCanvas* canvas, const QPoint& pos) {
    if (!canvas) return;
    
    // Get canvas dimensions
    int canvasWidth = canvas->width();
    int canvasHeight = canvas->height();
    
    // Edge detection zones - show scrollbars when pointer is within 50px of edges
    bool nearLeftEdge = pos.x() < 25;  // For vertical scrollbar
    bool nearTopEdge = pos.y() < 25;   // For horizontal scrollbar - entire top edge
    
    // Only show scrollbars if canvas is larger than viewport
    bool needHorizontalScroll = panXSlider->maximum() > 0;
    bool needVerticalScroll = panYSlider->maximum() > 0;
    
    // Show/hide scrollbars based on pointer position
    if (nearLeftEdge && needVerticalScroll) {
        panYSlider->setVisible(true);
        scrollbarsVisible = true;
        if (scrollbarHideTimer->isActive()) {
            scrollbarHideTimer->stop();
        }
        scrollbarHideTimer->start();
    }
    
    if (nearTopEdge && needHorizontalScroll) {
        panXSlider->setVisible(true);
        scrollbarsVisible = true;
        if (scrollbarHideTimer->isActive()) {
            scrollbarHideTimer->stop();
        }
        scrollbarHideTimer->start();
    }
}

void MainWindow::returnToLauncher() {
    // Save current work before returning to launcher
    if (currentCanvas() && currentCanvas()->isEdited()) {
        saveCurrentPage();
    }
    
    // Use shared launcher instance to prevent memory leaks
    if (!sharedLauncher) {
        sharedLauncher = new LauncherWindow();
        
        // Connect to handle when launcher is destroyed - clean up static reference
        connect(sharedLauncher, &LauncherWindow::destroyed, []() {
            MainWindow::sharedLauncher = nullptr;
        });
    }
    
    // Preserve window state
    if (isMaximized()) {
        sharedLauncher->showMaximized();
    } else if (isFullScreen()) {
        sharedLauncher->showFullScreen();
    } else {
        sharedLauncher->resize(size());
        sharedLauncher->move(pos());
        sharedLauncher->show();
    }
    
    // Don't refresh here - showEvent will handle it automatically
    // This prevents double population (returnToLauncher + showEvent)
    
    // Hide this main window
    hide();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    
    // Use a timer to delay layout updates during resize to prevent excessive switching
    if (!layoutUpdateTimer) {
        layoutUpdateTimer = new QTimer(this);
        layoutUpdateTimer->setSingleShot(true);
        connect(layoutUpdateTimer, &QTimer::timeout, this, [this]() {
            updateToolbarLayout();
            // Also reposition dial after resize finishes
            if (dialContainer && dialContainer->isVisible()) {
                positionDialContainer();
            }
        });
    }
    
    layoutUpdateTimer->stop();
    layoutUpdateTimer->start(100); // Wait 100ms after resize stops
}

void MainWindow::updateToolbarLayout() {
    // Calculate scaled width using device pixel ratio
    QScreen *screen = QGuiApplication::primaryScreen();
    // qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
    int scaledWidth = width();
    
    // Dynamic threshold based on zoom button visibility
    int threshold = areZoomButtonsVisible() ? 1388 : 1278;
    
    // Debug output to understand what's happening
    // qDebug() << "Window width:" << scaledWidth << "Threshold:" << threshold << "Zoom buttons visible:" << areZoomButtonsVisible();
    
    bool shouldBeTwoRows = scaledWidth <= threshold;
    
    // qDebug() << "Should be two rows:" << shouldBeTwoRows << "Currently is two rows:" << isToolbarTwoRows;
    
    if (shouldBeTwoRows != isToolbarTwoRows) {
        isToolbarTwoRows = shouldBeTwoRows;
        
        // qDebug() << "Switching to" << (isToolbarTwoRows ? "two rows" : "single row");
        
        if (isToolbarTwoRows) {
            createTwoRowLayout();
        } else {
            createSingleRowLayout();
        }
    }
}

void MainWindow::createSingleRowLayout() {
    // Delete separator line if it exists (from previous 2-row layout)
    if (separatorLine) {
        delete separatorLine;
        separatorLine = nullptr;
    }
    
    // Create new single row layout first
    QHBoxLayout *newLayout = new QHBoxLayout;
    
    // Add all widgets to single row (same order as before)
    newLayout->addWidget(toggleTabBarButton);
            newLayout->addWidget(toggleOutlineButton);
        newLayout->addWidget(toggleBookmarksButton);
        newLayout->addWidget(toggleBookmarkButton);
        newLayout->addWidget(touchGesturesButton);
        newLayout->addWidget(selectFolderButton);
    
    newLayout->addWidget(loadPdfButton);
    newLayout->addWidget(clearPdfButton);
    newLayout->addWidget(pdfTextSelectButton);
    // newLayout->addWidget(backgroundButton);
    newLayout->addWidget(saveButton);
    newLayout->addWidget(saveAnnotatedButton);
    newLayout->addWidget(openControlPanelButton);
    newLayout->addWidget(openRecentNotebooksButton);
    newLayout->addWidget(redButton);
    newLayout->addWidget(blueButton);
    newLayout->addWidget(yellowButton);
    newLayout->addWidget(greenButton);
    newLayout->addWidget(blackButton);
    newLayout->addWidget(whiteButton);
    newLayout->addWidget(customColorButton);
    newLayout->addWidget(penToolButton);
    newLayout->addWidget(markerToolButton);
    newLayout->addWidget(eraserToolButton);
    newLayout->addWidget(straightLineToggleButton);
    newLayout->addWidget(ropeToolButton);
    newLayout->addWidget(markdownButton);
    newLayout->addWidget(insertPictureButton);
    newLayout->addWidget(dialToggleButton);
    newLayout->addWidget(fastForwardButton);
    newLayout->addWidget(btnPageSwitch);
    newLayout->addWidget(btnPannScroll);
    newLayout->addWidget(btnZoom);
    newLayout->addWidget(btnThickness);

    newLayout->addWidget(btnTool);
    newLayout->addWidget(btnPresets);
    newLayout->addWidget(addPresetButton);
    newLayout->addWidget(fullscreenButton);
    
    // Only add zoom buttons if they're visible
    if (areZoomButtonsVisible()) {
        newLayout->addWidget(zoom50Button);
        newLayout->addWidget(dezoomButton);
        newLayout->addWidget(zoom200Button);
    }
    
    newLayout->addStretch();
    newLayout->addWidget(prevPageButton);
    newLayout->addWidget(pageInput);
    newLayout->addWidget(nextPageButton);
    newLayout->addWidget(benchmarkButton);
    newLayout->addWidget(benchmarkLabel);
    newLayout->addWidget(deletePageButton);
    
    // Safely replace the layout
    QLayout* oldLayout = controlBar->layout();
    if (oldLayout) {
        // Remove all items from old layout (but don't delete widgets)
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            // Just removing, not deleting widgets
        }
        delete oldLayout;
    }
    
    // Set the new layout
    controlBar->setLayout(newLayout);
    controlLayoutSingle = newLayout;
    
    // Clean up other layout pointers
    controlLayoutVertical = nullptr;
    controlLayoutFirstRow = nullptr;
    controlLayoutSecondRow = nullptr;
    
    // Update pan range after layout change
    updatePanRange();
}

void MainWindow::createTwoRowLayout() {
    // Create new layouts first
    QVBoxLayout *newVerticalLayout = new QVBoxLayout;
    QHBoxLayout *newFirstRowLayout = new QHBoxLayout;
    QHBoxLayout *newSecondRowLayout = new QHBoxLayout;
    
    // Add comfortable spacing and margins for 2-row layout
    newFirstRowLayout->setContentsMargins(8, 8, 8, 6);  // More generous margins
    newFirstRowLayout->setSpacing(3);  // Add spacing between buttons for less cramped feel
    newSecondRowLayout->setContentsMargins(8, 6, 8, 8);  // More generous margins
    newSecondRowLayout->setSpacing(3);  // Add spacing between buttons for less cramped feel
    
    // First row: up to customColorButton
    newFirstRowLayout->addWidget(toggleTabBarButton);
            newFirstRowLayout->addWidget(toggleOutlineButton);
        newFirstRowLayout->addWidget(toggleBookmarksButton);
        newFirstRowLayout->addWidget(toggleBookmarkButton);
        newFirstRowLayout->addWidget(touchGesturesButton);
        newFirstRowLayout->addWidget(selectFolderButton);
    
    newFirstRowLayout->addWidget(loadPdfButton);
    newFirstRowLayout->addWidget(clearPdfButton);
    newFirstRowLayout->addWidget(pdfTextSelectButton);
    // newFirstRowLayout->addWidget(backgroundButton);
    newFirstRowLayout->addWidget(saveButton);
    newFirstRowLayout->addWidget(saveAnnotatedButton);
    newFirstRowLayout->addWidget(openControlPanelButton);
    newFirstRowLayout->addWidget(openRecentNotebooksButton);
    newFirstRowLayout->addWidget(redButton);
    newFirstRowLayout->addWidget(blueButton);
    newFirstRowLayout->addWidget(yellowButton);
    newFirstRowLayout->addWidget(greenButton);
    newFirstRowLayout->addWidget(blackButton);
    newFirstRowLayout->addWidget(whiteButton);
    newFirstRowLayout->addWidget(customColorButton);
    newFirstRowLayout->addWidget(penToolButton);
    newFirstRowLayout->addWidget(markerToolButton);
    newFirstRowLayout->addWidget(eraserToolButton);
    newFirstRowLayout->addStretch();
    
    // Create a separator line
    if (!separatorLine) {
        separatorLine = new QFrame();
        separatorLine->setFrameShape(QFrame::HLine);
        separatorLine->setFrameShadow(QFrame::Sunken);
        separatorLine->setLineWidth(1);
        separatorLine->setStyleSheet("QFrame { color: rgba(255, 255, 255, 255); }");
    }
    
    // Second row: everything after customColorButton
    newSecondRowLayout->addWidget(straightLineToggleButton);
    newSecondRowLayout->addWidget(ropeToolButton);
    newSecondRowLayout->addWidget(markdownButton);
    newSecondRowLayout->addWidget(insertPictureButton);
    newSecondRowLayout->addWidget(dialToggleButton);
    newSecondRowLayout->addWidget(fastForwardButton);
    newSecondRowLayout->addWidget(btnPageSwitch);
    newSecondRowLayout->addWidget(btnPannScroll);
    newSecondRowLayout->addWidget(btnZoom);
    newSecondRowLayout->addWidget(btnThickness);

    newSecondRowLayout->addWidget(btnTool);
    newSecondRowLayout->addWidget(btnPresets);
    newSecondRowLayout->addWidget(addPresetButton);
    newSecondRowLayout->addWidget(fullscreenButton);
    
    // Only add zoom buttons if they're visible
    if (areZoomButtonsVisible()) {
        newSecondRowLayout->addWidget(zoom50Button);
        newSecondRowLayout->addWidget(dezoomButton);
        newSecondRowLayout->addWidget(zoom200Button);
    }
    
    newSecondRowLayout->addStretch();
    newSecondRowLayout->addWidget(prevPageButton);
    newSecondRowLayout->addWidget(pageInput);
    newSecondRowLayout->addWidget(nextPageButton);
    newSecondRowLayout->addWidget(benchmarkButton);
    newSecondRowLayout->addWidget(benchmarkLabel);
    newSecondRowLayout->addWidget(deletePageButton);
    
    // Add layouts to vertical layout with separator
    newVerticalLayout->addLayout(newFirstRowLayout);
    newVerticalLayout->addWidget(separatorLine);
    newVerticalLayout->addLayout(newSecondRowLayout);
    newVerticalLayout->setContentsMargins(0, 0, 0, 0);
    newVerticalLayout->setSpacing(0); // No spacing since we have our own separator
    
    // Safely replace the layout
    QLayout* oldLayout = controlBar->layout();
    if (oldLayout) {
        // Remove all items from old layout (but don't delete widgets)
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            // Just removing, not deleting widgets
        }
        delete oldLayout;
    }
    
    // Set the new layout
    controlBar->setLayout(newVerticalLayout);
    controlLayoutVertical = newVerticalLayout;
    controlLayoutFirstRow = newFirstRowLayout;
    controlLayoutSecondRow = newSecondRowLayout;
    
    // Clean up other layout pointer
    controlLayoutSingle = nullptr;
    
    // Update pan range after layout change
    updatePanRange();
}

// New: Keyboard mapping implementation
void MainWindow::handleKeyboardShortcut(const QString &keySequence) {
    ControllerAction action = keyboardActionMapping.value(keySequence, ControllerAction::None);
    
    // Use the same handler as Joy-Con buttons
    switch (action) {
        case ControllerAction::ToggleFullscreen:
            fullscreenButton->click();
            break;
        case ControllerAction::ToggleDial:
            toggleDial();
            break;
        case ControllerAction::Zoom50:
            zoom50Button->click();
            break;
        case ControllerAction::ZoomOut:
            dezoomButton->click();
            break;
        case ControllerAction::Zoom200:
            zoom200Button->click();
            break;
        case ControllerAction::AddPreset:
            addPresetButton->click();
            break;
        case ControllerAction::DeletePage:
            deletePageButton->click();
            break;
        case ControllerAction::FastForward:
            fastForwardButton->click();
            break;
        case ControllerAction::OpenControlPanel:
            openControlPanelButton->click();
            break;
        case ControllerAction::RedColor:
            redButton->click();
            break;
        case ControllerAction::BlueColor:
            blueButton->click();
            break;
        case ControllerAction::YellowColor:
            yellowButton->click();
            break;
        case ControllerAction::GreenColor:
            greenButton->click();
            break;
        case ControllerAction::BlackColor:
            blackButton->click();
            break;
        case ControllerAction::WhiteColor:
            whiteButton->click();
            break;
        case ControllerAction::CustomColor:
            customColorButton->click();
            break;
        case ControllerAction::ToggleSidebar:
            toggleTabBarButton->click();
            break;
        case ControllerAction::Save:
            saveButton->click();
            break;
        case ControllerAction::StraightLineTool:
            straightLineToggleButton->click();
            break;
        case ControllerAction::RopeTool:
            ropeToolButton->click();
            break;
        case ControllerAction::SetPenTool:
            setPenTool();
            break;
        case ControllerAction::SetMarkerTool:
            setMarkerTool();
            break;
        case ControllerAction::SetEraserTool:
            setEraserTool();
            break;
        case ControllerAction::TogglePdfTextSelection:
            pdfTextSelectButton->click();
            break;
        case ControllerAction::ToggleOutline:
            toggleOutlineButton->click();
            break;
        case ControllerAction::ToggleBookmarks:
            toggleBookmarksButton->click();
            break;
        case ControllerAction::AddBookmark:
            toggleBookmarkButton->click();
            break;
        case ControllerAction::ToggleTouchGestures:
            touchGesturesButton->click();
            break;
        case ControllerAction::PreviousPage:
            goToPreviousPage();
            break;
        case ControllerAction::NextPage:
            goToNextPage();
            break;
        default:
            break;
    }
}

void MainWindow::addKeyboardMapping(const QString &keySequence, const QString &action) {
    // List of IME-related shortcuts that should not be intercepted
    QStringList imeShortcuts = {
        "Ctrl+Space",      // Primary IME toggle
        "Ctrl+Shift",      // Language switching
        "Ctrl+Alt",        // IME functions
        "Shift+Alt",       // Alternative language switching
        "Alt+Shift"        // Alternative language switching (reversed)
    };
    
    // Don't allow mapping of IME-related shortcuts
    if (imeShortcuts.contains(keySequence)) {
        qWarning() << "Cannot map IME-related shortcut:" << keySequence;
        return;
    }
    
    keyboardMappings[keySequence] = action;
    keyboardActionMapping[keySequence] = stringToAction(action);
    saveKeyboardMappings();
}

void MainWindow::removeKeyboardMapping(const QString &keySequence) {
    keyboardMappings.remove(keySequence);
    keyboardActionMapping.remove(keySequence);
    saveKeyboardMappings();
}

void MainWindow::saveKeyboardMappings() {
    QSettings settings("SpeedyNote", "App");
    settings.beginGroup("KeyboardMappings");
    for (auto it = keyboardMappings.begin(); it != keyboardMappings.end(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
}

void MainWindow::loadKeyboardMappings() {
    QSettings settings("SpeedyNote", "App");
    settings.beginGroup("KeyboardMappings");
    QStringList keys = settings.allKeys();
    
    // List of IME-related shortcuts that should not be intercepted
    QStringList imeShortcuts = {
        "Ctrl+Space",      // Primary IME toggle
        "Ctrl+Shift",      // Language switching
        "Ctrl+Alt",        // IME functions
        "Shift+Alt",       // Alternative language switching
        "Alt+Shift"        // Alternative language switching (reversed)
    };
    
    for (const QString &key : keys) {
        // Skip IME-related shortcuts
        if (imeShortcuts.contains(key)) {
            // Remove from settings if it exists
            settings.remove(key);
            continue;
        }
        
        QString value = settings.value(key).toString();
        keyboardMappings[key] = value;
        keyboardActionMapping[key] = stringToAction(value);
    }
    settings.endGroup();
    
    // Save settings to persist the removal of IME shortcuts
    settings.sync();
}

QMap<QString, QString> MainWindow::getKeyboardMappings() const {
    return keyboardMappings;
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // Don't intercept keyboard events when text input widgets have focus
    // This prevents conflicts with Windows TextInputFramework
    QWidget *focusWidget = QApplication::focusWidget();
    if (focusWidget) {
        bool isTextInputWidget = qobject_cast<QLineEdit*>(focusWidget) || 
                               qobject_cast<QSpinBox*>(focusWidget) || 
                               qobject_cast<QTextEdit*>(focusWidget) ||
                               qobject_cast<QPlainTextEdit*>(focusWidget) ||
                               qobject_cast<QComboBox*>(focusWidget);
        
        if (isTextInputWidget) {
            // Let text input widgets handle their own keyboard events
            QMainWindow::keyPressEvent(event);
            return;
        }
    }
    
    // Don't intercept IME-related keyboard shortcuts
    // These are reserved for Windows Input Method Editor
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Space ||           // Ctrl+Space (IME toggle)
            event->key() == Qt::Key_Shift ||           // Ctrl+Shift (language switch)
            event->key() == Qt::Key_Alt) {             // Ctrl+Alt (IME functions)
            // Let Windows handle IME shortcuts
            QMainWindow::keyPressEvent(event);
            return;
        }
    }
    
    // Don't intercept Shift+Alt (another common IME shortcut)
    if ((event->modifiers() & Qt::ShiftModifier) && (event->modifiers() & Qt::AltModifier)) {
        QMainWindow::keyPressEvent(event);
        return;
    }
    
    // Build key sequence string
    QStringList modifiers;
    
    if (event->modifiers() & Qt::ControlModifier) modifiers << "Ctrl";
    if (event->modifiers() & Qt::ShiftModifier) modifiers << "Shift";
    if (event->modifiers() & Qt::AltModifier) modifiers << "Alt";
    if (event->modifiers() & Qt::MetaModifier) modifiers << "Meta";
    
    QString keyString = QKeySequence(event->key()).toString();
    
    QString fullSequence;
    if (!modifiers.isEmpty()) {
        fullSequence = modifiers.join("+") + "+" + keyString;
    } else {
        fullSequence = keyString;
    }
    
    // Check if this sequence is mapped
    if (keyboardMappings.contains(fullSequence)) {
        handleKeyboardShortcut(fullSequence);
        event->accept();
        return;
    }
    
    // If not handled, pass to parent
    QMainWindow::keyPressEvent(event);
}

void MainWindow::tabletEvent(QTabletEvent *event) {
    // Since tablet tracking is disabled to prevent crashes, we now only handle
    // basic tablet events that come through when stylus is touching the surface
    if (!event) {
        return;
    }
    
    // Just pass tablet events to parent safely without custom hover handling
    // (hover tooltips will work through normal mouse events instead)
    try {
        QMainWindow::tabletEvent(event);
    } catch (...) {
        // Catch any exceptions and just accept the event
        event->accept();
    }
}

void MainWindow::showPendingTooltip() {
    // This function is now unused since we disabled tablet tracking
    // Tooltips will work through normal mouse hover events instead
    // Keeping the function for potential future use
}

void MainWindow::onZoomSliderChanged(int value) {
    // This handles manual zoom slider changes and preserves thickness
    int oldZoom = currentCanvas() ? currentCanvas()->getZoom() : 100;
    int newZoom = value;
    
    updateZoom();
    adjustThicknessForZoom(oldZoom, newZoom); // Maintain visual thickness consistency
}

void MainWindow::saveDefaultBackgroundSettings(BackgroundStyle style, QColor color, int density) {
    QSettings settings("SpeedyNote", "App");
    settings.setValue("defaultBackgroundStyle", static_cast<int>(style));
    settings.setValue("defaultBackgroundColor", color.name());
    settings.setValue("defaultBackgroundDensity", density);
}

// PDF Outline functionality
void MainWindow::toggleOutlineSidebar() {
    outlineSidebarVisible = !outlineSidebarVisible;
    
    // Hide bookmarks sidebar if it's visible when opening outline
    if (outlineSidebarVisible && bookmarksSidebar && bookmarksSidebar->isVisible()) {
        bookmarksSidebar->setVisible(false);
        bookmarksSidebarVisible = false;
        // Update bookmarks button state
        if (toggleBookmarksButton) {
            toggleBookmarksButton->setProperty("selected", false);
            toggleBookmarksButton->style()->unpolish(toggleBookmarksButton);
            toggleBookmarksButton->style()->polish(toggleBookmarksButton);
        }
    }
    
    outlineSidebar->setVisible(outlineSidebarVisible);
    
    // Update button toggle state
    if (toggleOutlineButton) {
        toggleOutlineButton->setProperty("selected", outlineSidebarVisible);
        toggleOutlineButton->style()->unpolish(toggleOutlineButton);
        toggleOutlineButton->style()->polish(toggleOutlineButton);
    }
    
    // Load PDF outline when showing sidebar for the first time
    if (outlineSidebarVisible) {
        loadPdfOutline();
    }
}

void MainWindow::onOutlineItemClicked(QTreeWidgetItem *item, int column) {
    Q_UNUSED(column);
    
    if (!item) return;
    
    // Get the page number stored in the item data
    QVariant pageData = item->data(0, Qt::UserRole);
    if (pageData.isValid()) {
        int pageNumber = pageData.toInt();
        if (pageNumber >= 0) {
            // Switch to the selected page (pageNumber is already 1-based from PDF outline)
            switchPage(pageNumber);
            pageInput->setValue(pageNumber);
        }
    }
}

void MainWindow::loadPdfOutline() {
    if (!outlineTree) return;
    
    outlineTree->clear();
    
    // Get current PDF document
    Poppler::Document* pdfDoc = getPdfDocument();
    if (!pdfDoc) return;
    
    // Get the outline from the PDF document
    QVector<Poppler::OutlineItem> outlineItems = pdfDoc->outline();
    
    if (outlineItems.isEmpty()) {
        // If no outline exists, show page numbers as fallback
        int pageCount = pdfDoc->numPages();
        for (int i = 0; i < pageCount; ++i) {
            QTreeWidgetItem* item = new QTreeWidgetItem(outlineTree);
            item->setText(0, QString(tr("Page %1")).arg(i + 1));
            item->setData(0, Qt::UserRole, i + 1); // Store 1-based page index to match outline behavior
        }
    } else {
        // Process the actual PDF outline
        for (const Poppler::OutlineItem& outlineItem : outlineItems) {
            addOutlineItem(outlineItem, nullptr);
        }
    }
    
    // Expand the first level by default
    outlineTree->expandToDepth(0);
}

void MainWindow::addOutlineItem(const Poppler::OutlineItem& outlineItem, QTreeWidgetItem* parentItem) {
    if (outlineItem.isNull()) return;
    
    QTreeWidgetItem* item;
    if (parentItem) {
        item = new QTreeWidgetItem(parentItem);
    } else {
        item = new QTreeWidgetItem(outlineTree);
    }
    
    // Set the title
    item->setText(0, outlineItem.name());
    
    // Try to get the page number from the destination
    int pageNumber = -1;
    auto destination = outlineItem.destination();
    if (destination) {
        pageNumber = destination->pageNumber();
    }
    
    // Store the page number (already 1-based from PDF) in the item data
    if (pageNumber >= 0) {
        item->setData(0, Qt::UserRole, pageNumber); // pageNumber is already 1-based from PDF outline
    }
    
    // Add child items recursively
    if (outlineItem.hasChildren()) {
        QVector<Poppler::OutlineItem> children = outlineItem.children();
        for (const Poppler::OutlineItem& child : children) {
            addOutlineItem(child, item);
        }
    }
}

Poppler::Document* MainWindow::getPdfDocument() {
    InkCanvas* canvas = currentCanvas();
    if (!canvas || !canvas->isPdfLoadedFunc()) {
        return nullptr;
    }
    return canvas->getPdfDocument();
}

void MainWindow::loadDefaultBackgroundSettings(BackgroundStyle &style, QColor &color, int &density) {
    QSettings settings("SpeedyNote", "App");
    style = static_cast<BackgroundStyle>(settings.value("defaultBackgroundStyle", static_cast<int>(BackgroundStyle::Grid)).toInt());
    color = QColor(settings.value("defaultBackgroundColor", "#FFFFFF").toString());
    density = settings.value("defaultBackgroundDensity", 30).toInt();
    
    // Ensure valid values
    if (!color.isValid()) color = Qt::white;
    if (density < 10) density = 10;
    if (density > 200) density = 200;
}

void MainWindow::applyDefaultBackgroundToCanvas(InkCanvas *canvas) {
    if (!canvas) return;
    
    BackgroundStyle defaultStyle;
    QColor defaultColor;
    int defaultDensity;
    loadDefaultBackgroundSettings(defaultStyle, defaultColor, defaultDensity);
    
    canvas->setBackgroundStyle(defaultStyle);
    canvas->setBackgroundColor(defaultColor);
    canvas->setBackgroundDensity(defaultDensity);
    canvas->saveBackgroundMetadata(); // Save the settings
    canvas->update(); // Refresh the display
}

void MainWindow::showRopeSelectionMenu(const QPoint &position) {
    // Create context menu for rope tool selection
    QMenu *contextMenu = new QMenu(this);
    contextMenu->setAttribute(Qt::WA_DeleteOnClose);
    
    // Add Copy action
    QAction *copyAction = contextMenu->addAction(tr("Copy"));
    copyAction->setIcon(loadThemedIcon("copy"));
    connect(copyAction, &QAction::triggered, this, [this]() {
        if (currentCanvas()) {
            currentCanvas()->copyRopeSelection();
        }
    });
    
    // Add Copy to Clipboard action
    QAction *copyToClipboardAction = contextMenu->addAction(tr("Copy to Clipboard"));
    copyToClipboardAction->setIcon(loadThemedIcon("clipboard"));
    connect(copyToClipboardAction, &QAction::triggered, this, [this]() {
        if (currentCanvas()) {
            currentCanvas()->copyRopeSelectionToClipboard();
        }
    });
    
    // Add Delete action
    QAction *deleteAction = contextMenu->addAction(tr("Delete"));
    deleteAction->setIcon(loadThemedIcon("trash"));
    connect(deleteAction, &QAction::triggered, this, [this]() {
        if (currentCanvas()) {
            currentCanvas()->deleteRopeSelection();
        }
    });
    
    // Add Cancel action
    QAction *cancelAction = contextMenu->addAction(tr("Cancel"));
    cancelAction->setIcon(loadThemedIcon("cross"));
    connect(cancelAction, &QAction::triggered, this, [this]() {
        if (currentCanvas()) {
            currentCanvas()->cancelRopeSelection();
        }
    });
    
    // Convert position from canvas coordinates to global coordinates
    QPoint globalPos = currentCanvas()->mapToGlobal(position);
    
    // Show the menu at the specified position
    contextMenu->popup(globalPos);
}
void MainWindow::updatePdfTextSelectButtonState() {
    // Check if PDF text selection is enabled
    bool isEnabled = currentCanvas() && currentCanvas()->isPdfTextSelectionEnabled();
    
    if (pdfTextSelectButton) {
        pdfTextSelectButton->setProperty("selected", isEnabled);
        
        // Force style update (uses the same buttonStyle as other toggle buttons)
        pdfTextSelectButton->style()->unpolish(pdfTextSelectButton);
        pdfTextSelectButton->style()->polish(pdfTextSelectButton);
    }
}

QString MainWindow::elideTabText(const QString &text, int maxWidth) {
    // Create a font metrics object using the default font
    QFontMetrics fontMetrics(QApplication::font());
    
    // Elide the text from the right (showing the beginning)
    return fontMetrics.elidedText(text, Qt::ElideRight, maxWidth);
}

// Bookmark functionality implementation
void MainWindow::toggleBookmarksSidebar() {
    if (!bookmarksSidebar) return;
    
    bool isVisible = bookmarksSidebar->isVisible();
    
    // Hide outline sidebar if it's visible
    if (!isVisible && outlineSidebar && outlineSidebar->isVisible()) {
        outlineSidebar->setVisible(false);
        outlineSidebarVisible = false;
        // Update outline button state
        if (toggleOutlineButton) {
            toggleOutlineButton->setProperty("selected", false);
            toggleOutlineButton->style()->unpolish(toggleOutlineButton);
            toggleOutlineButton->style()->polish(toggleOutlineButton);
        }
    }
    
    bookmarksSidebar->setVisible(!isVisible);
    bookmarksSidebarVisible = !isVisible;
    
    // Update button toggle state
    if (toggleBookmarksButton) {
        toggleBookmarksButton->setProperty("selected", bookmarksSidebarVisible);
        toggleBookmarksButton->style()->unpolish(toggleBookmarksButton);
        toggleBookmarksButton->style()->polish(toggleBookmarksButton);
    }
    
    if (bookmarksSidebarVisible) {
        loadBookmarks(); // Refresh bookmarks when opening
    }
}

void MainWindow::onBookmarkItemClicked(QTreeWidgetItem *item, int column) {
    Q_UNUSED(column);
    if (!item) return;
    
    // Get the page number from the item data
    bool ok;
    int pageNumber = item->data(0, Qt::UserRole).toInt(&ok);
    if (ok && pageNumber > 0) {
        // Navigate to the bookmarked page
        switchPageWithDirection(pageNumber, (pageNumber > getCurrentPageForCanvas(currentCanvas()) + 1) ? 1 : -1);
        pageInput->setValue(pageNumber);
    }
}

void MainWindow::loadBookmarks() {
    if (!bookmarksTree || !currentCanvas()) return;
    
    bookmarksTree->clear();
    bookmarks.clear();
    
    // ✅ Get bookmarks from InkCanvas JSON metadata
    QStringList bookmarkList = currentCanvas()->getBookmarks();
    for (const QString &line : bookmarkList) {
        if (line.isEmpty()) continue;
        
        QStringList parts = line.split('\t', Qt::KeepEmptyParts);
        if (parts.size() >= 2) {
            bool ok;
            int pageNum = parts[0].toInt(&ok);
            if (ok) {
                QString title = parts[1];
                bookmarks[pageNum] = title;
            }
        }
    }
    
    // Populate the tree widget
    for (auto it = bookmarks.begin(); it != bookmarks.end(); ++it) {
        QTreeWidgetItem *item = new QTreeWidgetItem(bookmarksTree);
        
        // Create a custom widget for each bookmark item
        QWidget *itemWidget = new QWidget();
        QHBoxLayout *itemLayout = new QHBoxLayout(itemWidget);
        itemLayout->setContentsMargins(5, 2, 5, 2);
        itemLayout->setSpacing(5);
        
        // Page number label (fixed width)
        QLabel *pageLabel = new QLabel(QString(tr("Page %1")).arg(it.key()));
        pageLabel->setFixedWidth(60);
        pageLabel->setStyleSheet("font-weight: bold; color: #666;");
        itemLayout->addWidget(pageLabel);
        
        // Editable title (supports Windows handwriting if available)
        QLineEdit *titleEdit = new QLineEdit(it.value());
        titleEdit->setPlaceholderText("Enter bookmark title...");
        titleEdit->setProperty("pageNumber", it.key()); // Store page number for saving
        
        // Enable IME support for multi-language input
        titleEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
        titleEdit->setInputMethodHints(Qt::ImhNone); // Allow all input methods
        titleEdit->installEventFilter(this); // Install event filter for IME handling
        
        // Connect to save when editing is finished
        connect(titleEdit, &QLineEdit::editingFinished, this, [this, titleEdit]() {
            int pageNum = titleEdit->property("pageNumber").toInt();
            QString newTitle = titleEdit->text().trimmed();
            
            if (newTitle.isEmpty()) {
                // Remove bookmark if title is empty
                bookmarks.remove(pageNum);
            } else {
                // Update bookmark title
                bookmarks[pageNum] = newTitle;
            }
            saveBookmarks();
            updateBookmarkButtonState(); // Update button state
        });
        
        itemLayout->addWidget(titleEdit, 1);
        
        // Store page number in item data for navigation
        item->setData(0, Qt::UserRole, it.key());
        
        // Set the custom widget
        bookmarksTree->setItemWidget(item, 0, itemWidget);
        
        // Set item height
        item->setSizeHint(0, QSize(0, 30));
    }
    
    updateBookmarkButtonState(); // Update button state after loading
}

void MainWindow::saveBookmarks() {
    if (!currentCanvas()) return;
    
    // ✅ Save bookmarks to InkCanvas JSON metadata
    QStringList bookmarkList;
    
    // Sort bookmarks by page number
    QList<int> sortedPages = bookmarks.keys();
    std::sort(sortedPages.begin(), sortedPages.end());
    
    for (int pageNum : sortedPages) {
        bookmarkList.append(QString("%1\t%2").arg(pageNum).arg(bookmarks[pageNum]));
    }
    
    currentCanvas()->setBookmarks(bookmarkList);
}

void MainWindow::toggleCurrentPageBookmark() {
    if (!currentCanvas()) return;
    
    int currentPage = getCurrentPageForCanvas(currentCanvas()) + 1; // Convert to 1-based
    
    if (bookmarks.contains(currentPage)) {
        // Remove bookmark
        bookmarks.remove(currentPage);
    } else {
        // Add bookmark with default title
        QString defaultTitle = QString(tr("Bookmark %1")).arg(currentPage);
        bookmarks[currentPage] = defaultTitle;
    }
    
    saveBookmarks();
    updateBookmarkButtonState();
    
    // Refresh bookmarks view if visible
    if (bookmarksSidebarVisible) {
        loadBookmarks();
    }
}

void MainWindow::updateBookmarkButtonState() {
    if (!toggleBookmarkButton || !currentCanvas()) return;
    
    int currentPage = getCurrentPageForCanvas(currentCanvas()) + 1; // Convert to 1-based
    bool isBookmarked = bookmarks.contains(currentPage);
    
    toggleBookmarkButton->setProperty("selected", isBookmarked);
    
    // Update tooltip
    if (isBookmarked) {
        toggleBookmarkButton->setToolTip(tr("Remove Bookmark"));
    } else {
        toggleBookmarkButton->setToolTip(tr("Add Bookmark"));
    }
    
    // Force style update
    toggleBookmarkButton->style()->unpolish(toggleBookmarkButton);
    toggleBookmarkButton->style()->polish(toggleBookmarkButton);
}

// IME support for multi-language input
void MainWindow::inputMethodEvent(QInputMethodEvent *event) {
    // Forward IME events to the focused widget
    QWidget *focusWidget = QApplication::focusWidget();
    if (focusWidget && focusWidget != this) {
        QApplication::sendEvent(focusWidget, event);
        event->accept();
        return;
    }
    
    // Default handling
    QMainWindow::inputMethodEvent(event);
}

QVariant MainWindow::inputMethodQuery(Qt::InputMethodQuery query) const {
    // Forward IME queries to the focused widget
    QWidget *focusWidget = QApplication::focusWidget();
    if (focusWidget && focusWidget != this) {
        return focusWidget->inputMethodQuery(query);
    }
    
    // Default handling
    return QMainWindow::inputMethodQuery(query);
}

// Color palette management
void MainWindow::setUseBrighterPalette(bool use) {
    if (useBrighterPalette != use) {
        useBrighterPalette = use;
        
        // Update all colors - call updateColorPalette which handles null checks
        updateColorPalette();
        
        // Save preference
        QSettings settings("SpeedyNote", "App");
        settings.setValue("useBrighterPalette", useBrighterPalette);
    }
}

void MainWindow::updateColorPalette() {
    // Clear existing presets
    colorPresets.clear();
    currentPresetIndex = 0;
    
    // Add default pen color (theme-aware)
    colorPresets.enqueue(getDefaultPenColor());
    
    // Add palette colors
    colorPresets.enqueue(getPaletteColor("red"));
    colorPresets.enqueue(getPaletteColor("yellow"));
    colorPresets.enqueue(getPaletteColor("blue"));
    colorPresets.enqueue(getPaletteColor("green"));
    colorPresets.enqueue(QColor("#000000")); // Black (always same)
    colorPresets.enqueue(QColor("#FFFFFF")); // White (always same)
    
    // Only update UI elements if they exist
    if (redButton && blueButton && yellowButton && greenButton) {
        bool darkMode = isDarkMode();
        
        // Update color button icons based on current palette (not theme)
        QString redIconPath = useBrighterPalette ? ":/resources/icons/pen_light_red.png" : ":/resources/icons/pen_dark_red.png";
        QString blueIconPath = useBrighterPalette ? ":/resources/icons/pen_light_blue.png" : ":/resources/icons/pen_dark_blue.png";
        QString yellowIconPath = useBrighterPalette ? ":/resources/icons/pen_light_yellow.png" : ":/resources/icons/pen_dark_yellow.png";
        QString greenIconPath = useBrighterPalette ? ":/resources/icons/pen_light_green.png" : ":/resources/icons/pen_dark_green.png";
        
        redButton->setIcon(QIcon(redIconPath));
        blueButton->setIcon(QIcon(blueIconPath));
        yellowButton->setIcon(QIcon(yellowIconPath));
        greenButton->setIcon(QIcon(greenIconPath));
        
        // Update color button states
        updateColorButtonStates();
    }
}

QColor MainWindow::getPaletteColor(const QString &colorName) {
    if (useBrighterPalette) {
        // Brighter colors (good for dark backgrounds)
        if (colorName == "red") return QColor("#FF7755");
        if (colorName == "yellow") return QColor("#EECC00");
        if (colorName == "blue") return QColor("#66CCFF");
        if (colorName == "green") return QColor("#55FF77");
    } else {
        // Darker colors (good for light backgrounds)
        if (colorName == "red") return QColor("#AA0000");
        if (colorName == "yellow") return QColor("#997700");
        if (colorName == "blue") return QColor("#0000AA");
        if (colorName == "green") return QColor("#007700");
    }
    
    // Fallback colors
    if (colorName == "black") return QColor("#000000");
    if (colorName == "white") return QColor("#FFFFFF");
    
    return QColor("#000000"); // Default fallback
}

void MainWindow::reconnectControllerSignals() {
    if (!controllerManager || !pageDial) {
        return;
    }
    
    // Reset internal dial state
    tracking = false;
    accumulatedRotation = 0;
    grossTotalClicks = 0;
    tempClicks = 0;
    lastAngle = 0;
    startAngle = 0;
    pendingPageFlip = 0;
    accumulatedRotationAfterLimit = 0;
    
    // Disconnect all existing connections to avoid duplicates
    disconnect(controllerManager, nullptr, this, nullptr);
    disconnect(controllerManager, nullptr, pageDial, nullptr);
    
    // Reconnect all controller signals
    connect(controllerManager, &SDLControllerManager::buttonHeld, this, &MainWindow::handleButtonHeld);
    connect(controllerManager, &SDLControllerManager::buttonReleased, this, &MainWindow::handleButtonReleased);
    connect(controllerManager, &SDLControllerManager::leftStickAngleChanged, pageDial, &QDial::setValue);
    connect(controllerManager, &SDLControllerManager::leftStickReleased, pageDial, &QDial::sliderReleased);
    connect(controllerManager, &SDLControllerManager::buttonSinglePress, this, &MainWindow::handleControllerButton);
    
    // Re-establish dial mode connections by changing to current mode
    DialMode currentMode = currentDialMode;
    changeDialMode(currentMode);
    
    // Update dial display to reflect current state
    updateDialDisplay();
    
    // qDebug() << "Controller signals reconnected successfully";
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    // Detect Windows theme changes at runtime
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        
        // WM_SETTINGCHANGE (0x001A) is sent when system settings change
        if (msg->message == 0x001A) {
            // Check if this is a theme-related setting change
            if (msg->lParam != 0) {
                const wchar_t *lparam = reinterpret_cast<const wchar_t *>(msg->lParam);
                if (lparam && wcscmp(lparam, L"ImmersiveColorSet") == 0) {
                    // Windows theme changed - update Qt palette and our UI
                    // Use a small delay to ensure registry has been updated
                    QTimer::singleShot(100, this, [this]() {
                        MainWindow::updateApplicationPalette(); // Update Qt's global palette
                        updateTheme(); // Update our custom theme
                    });
                }
            }
        }
    }
    
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::closeEvent(QCloseEvent *event) {
    // Auto-save all tabs before closing the program
    if (canvasStack) {
        for (int i = 0; i < canvasStack->count(); ++i) {
            InkCanvas *canvas = qobject_cast<InkCanvas*>(canvasStack->widget(i));
            if (canvas) {
                // Save current page if edited
                if (canvas->isEdited()) {
                    int pageNumber = getCurrentPageForCanvas(canvas);
                    canvas->saveToFile(pageNumber);
                    
                    // ✅ COMBINED MODE FIX: Use combined-aware save for markdown/picture windows
                    canvas->saveCombinedWindowsForPage(pageNumber);
                }
                
                // ✅ Save last accessed page for each canvas
                int currentPage = getCurrentPageForCanvas(canvas);
                canvas->setLastAccessedPage(currentPage);
            }
        }
        
        // ✅ Save current bookmarks before closing
        saveBookmarks();
    }
    
    // Accept the close event to allow the program to close
    event->accept();
}

bool MainWindow::showLastAccessedPageDialog(InkCanvas *canvas) {
    if (!canvas) return false;
    
    int lastPage = canvas->getLastAccessedPage();
    if (lastPage <= 0) return false; // No last accessed page or page 0 (first page)
    
    QString message = tr("This notebook was last accessed on page %1.\n\nWould you like to go directly to page %1, or start from page 1?").arg(lastPage + 1);
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Last Accessed Page"));
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton *gotoLastPageBtn = msgBox.addButton(tr("Go to Page %1").arg(lastPage + 1), QMessageBox::AcceptRole);
    QPushButton *startFromFirstBtn = msgBox.addButton(tr("Start from Page 1"), QMessageBox::RejectRole);
    
    msgBox.setDefaultButton(gotoLastPageBtn);
    
    int result = msgBox.exec();
    
    if (msgBox.clickedButton() == gotoLastPageBtn) {
        // User wants to go to last accessed page (convert 0-based to 1-based)
        switchPageWithDirection(lastPage + 1, 1);
        return true;
    } else {
        // User wants to start from page 1
        switchPageWithDirection(1, 1);
        return true;
    }
}

void MainWindow::openSpnPackage(const QString &spnPath)
{
    if (!SpnPackageManager::isValidSpnPackage(spnPath)) {
        QMessageBox::warning(this, tr("Invalid Package"), 
            tr("The selected file is not a valid SpeedyNote package."));
        return;
    }
    
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;
    
    // Save current work if edited
    if (canvas->isEdited()) {
        saveCurrentPage();
    }
    
    // Set the .spn package as save folder
    canvas->setSaveFolder(spnPath);
    
    // ✅ Handle missing PDF file
    if (!canvas->handleMissingPdf(this)) {
        // User cancelled PDF relinking, don't open the package
        return;
    }
    
    // ✅ Update scroll behavior based on PDF loading state after relinking
    setScrollOnTopEnabled(canvas->isPdfLoadedFunc());
    
    // Update tab label
    updateTabLabel();
    updateBookmarkButtonState(); // ✅ Update bookmark button state after loading notebook
    
    // ✅ Show last accessed page dialog if available
    if (!showLastAccessedPageDialog(canvas)) {
        // No last accessed page, start from page 1
        switchPageWithDirection(1, 1);
        pageInput->setValue(1);
    } else {
        // Dialog handled page switching, update page input
        pageInput->setValue(getCurrentPageForCanvas(canvas) + 1);
    }
    updateZoom();
    updatePanRange();
    
    if (recentNotebooksManager) {
        recentNotebooksManager->addRecentNotebook(spnPath, canvas);
        // Refresh shared launcher if it exists and is visible
        if (sharedLauncher && sharedLauncher->isVisible()) {
            sharedLauncher->refreshRecentNotebooks();
        }
    }
}

void MainWindow::createNewSpnPackage(const QString &spnPath)
{
    // Check if file already exists
    if (QFile::exists(spnPath)) {
        QMessageBox::warning(this, tr("File Exists"), 
            tr("A file with this name already exists. Please choose a different name."));
        return;
    }
    
    // Get the base name for the notebook (without .spn extension)
    QFileInfo fileInfo(spnPath);
    QString notebookName = fileInfo.baseName();
    
    // Create the new .spn package
    if (!SpnPackageManager::createSpnPackage(spnPath, notebookName)) {
        QMessageBox::critical(this, tr("Creation Failed"), 
            tr("Failed to create the SpeedyNote package. Please check file permissions."));
        return;
    }
    
    // Get current canvas and save any existing work
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;
    
    if (canvas->isEdited()) {
        saveCurrentPage();
    }
    
    // Open the newly created package
    canvas->setSaveFolder(spnPath);
    
    // ✅ Apply default background settings to new package
    applyDefaultBackgroundToCanvas(canvas);
    
    // Update UI
    updateTabLabel();
    updateBookmarkButtonState();
    
    // Start from page 1 (no last accessed page for new packages)
    switchPageWithDirection(1, 1);
    pageInput->setValue(1);
    updateZoom();
    updatePanRange();
    
    // Add to recent notebooks
    if (recentNotebooksManager) {
        recentNotebooksManager->addRecentNotebook(spnPath, canvas);
        // Refresh shared launcher if it exists and is visible
        if (sharedLauncher && sharedLauncher->isVisible()) {
            sharedLauncher->refreshRecentNotebooks();
        }
    }
    
    // Show success message
    QMessageBox::information(this, tr("Package Created"), 
        tr("New SpeedyNote package '%1' has been created successfully!").arg(notebookName));
}

// ========================================
// Single Instance Implementation
// ========================================

bool MainWindow::isInstanceRunning()
{
    if (!sharedMemory) {
        sharedMemory = new QSharedMemory("SpeedyNote_SingleInstance");
    }
    
    // First, try to create shared memory segment
    if (sharedMemory->create(1)) {
        // Successfully created, we're the first instance
        return false;
    }
    
    // Creation failed, check why
    QSharedMemory::SharedMemoryError error = sharedMemory->error();
    
#ifdef Q_OS_LINUX
    // On Linux, handle the alternating crash pattern by being more aggressive with cleanup
    if (error == QSharedMemory::AlreadyExists) {
        // Try to connect to the local server to see if instance is actually running
        QLocalSocket testSocket;
        testSocket.connectToServer("SpeedyNote_SingleInstance");
        
        // Wait briefly for connection - reduced timeout for faster response
        if (!testSocket.waitForConnected(500)) {
            // No server responding, definitely stale shared memory
            // qDebug() << "Detected stale shared memory on Linux, attempting cleanup...";
            
            // Delete current shared memory object and create a fresh one
            delete sharedMemory;
            sharedMemory = new QSharedMemory("SpeedyNote_SingleInstance");
            
            // Try to attach to the existing segment and then detach to clean it up
            if (sharedMemory->attach()) {
                sharedMemory->detach();
                
                // Create a new shared memory object again after cleanup
                delete sharedMemory;
                sharedMemory = new QSharedMemory("SpeedyNote_SingleInstance");
                
                // Now try to create again
                if (sharedMemory->create(1)) {
                    // qDebug() << "Successfully cleaned up stale shared memory on Linux";
                    return false; // We're now the first instance
                }
            }
            
            // If attach failed, try more aggressive cleanup
            // This handles the case where the segment exists but is corrupted
            delete sharedMemory;
            sharedMemory = nullptr;
            
            // Use system command to remove stale shared memory (last resort)
            // Run this asynchronously to avoid blocking the startup
            QProcess *cleanupProcess = new QProcess();
            cleanupProcess->start("sh", QStringList() << "-c" << "ipcs -m | grep $(whoami) | awk '/SpeedyNote/{print $2}' | xargs -r ipcrm -m");
            
            // Clean up the process when it finishes
            QObject::connect(cleanupProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                           cleanupProcess, &QProcess::deleteLater);
            
            // Create fresh shared memory object
            sharedMemory = new QSharedMemory("SpeedyNote_SingleInstance");
            if (sharedMemory->create(1)) {
                // qDebug() << "Cleaned up stale shared memory using system command";
                return false;
            }
            
            // If we still can't create, log the issue
            qWarning() << "Failed to clean up stale shared memory on Linux. Manual cleanup may be required.";
        } else {
            // Server is responding, there's actually another instance running
            testSocket.disconnectFromServer();
        }
    }
#endif
    
    // Another instance is running (or cleanup failed)
    return true;
}

bool MainWindow::sendToExistingInstance(const QString &filePath)
{
    QLocalSocket socket;
    socket.connectToServer("SpeedyNote_SingleInstance");
    
    if (!socket.waitForConnected(3000)) {
        return false; // Failed to connect to existing instance
    }
    
    // Send the file path to the existing instance
    QByteArray data = filePath.toUtf8();
    socket.write(data);
    socket.waitForBytesWritten(3000);
    socket.disconnectFromServer();
    
    return true;
}

void MainWindow::setupSingleInstanceServer()
{
    localServer = new QLocalServer(this);
    
    // Remove any existing server (in case of improper shutdown)
    QLocalServer::removeServer("SpeedyNote_SingleInstance");
    
    // Start listening for new connections
    if (!localServer->listen("SpeedyNote_SingleInstance")) {
        qWarning() << "Failed to start single instance server:" << localServer->errorString();
        return;
    }
    
    // Connect to handle new connections
    connect(localServer, &QLocalServer::newConnection, this, &MainWindow::onNewConnection);
}

void MainWindow::onNewConnection()
{
    QLocalSocket *clientSocket = localServer->nextPendingConnection();
    if (!clientSocket) return;
    
    // Set up the socket to auto-delete when disconnected
    clientSocket->setParent(this); // Ensure proper cleanup
    
    // Use QPointer for safe access in lambdas
    QPointer<QLocalSocket> socketPtr(clientSocket);
    
    // Handle data reception with improved error handling
    connect(clientSocket, &QLocalSocket::readyRead, this, [this, socketPtr]() {
        if (!socketPtr || socketPtr->state() != QLocalSocket::ConnectedState) {
            return; // Socket was deleted or disconnected
        }
        
        QByteArray data = socketPtr->readAll();
        QString command = QString::fromUtf8(data);
        
        if (!command.isEmpty()) {
            // Use QTimer::singleShot to defer processing to avoid signal/slot conflicts
            QTimer::singleShot(0, this, [this, command]() {
                // Bring window to front and focus (already on main thread)
                raise();
                activateWindow();
                
                // Parse command
                if (command.startsWith("--create-new|")) {
                    // Handle create new package command
                    QString filePath = command.mid(13); // Remove "--create-new|" prefix
                    createNewSpnPackage(filePath);
                } else {
                    // Regular file opening
                    openFileInNewTab(command);
                }
            });
        }
        
        // Close the connection after processing with a small delay
        QTimer::singleShot(10, this, [socketPtr]() {
            if (socketPtr && socketPtr->state() == QLocalSocket::ConnectedState) {
                socketPtr->disconnectFromServer();
            }
        });
    });
    
    // Handle connection errors
    connect(clientSocket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::errorOccurred),
            this, [socketPtr](QLocalSocket::LocalSocketError error) {
        Q_UNUSED(error);
        if (socketPtr) {
            socketPtr->disconnectFromServer();
        }
    });
    
    // Clean up when disconnected
    connect(clientSocket, &QLocalSocket::disconnected, clientSocket, &QLocalSocket::deleteLater);
    
    // Set a reasonable timeout (3 seconds) with safe pointer
    QTimer::singleShot(3000, this, [socketPtr]() {
        if (socketPtr && socketPtr->state() != QLocalSocket::UnconnectedState) {
            socketPtr->disconnectFromServer();
        }
    });
}

// Static cleanup method for signal handlers and emergency cleanup
void MainWindow::cleanupSharedResources()
{
    // Minimal cleanup to avoid Qt conflicts
    if (sharedMemory) {
        if (sharedMemory->isAttached()) {
            sharedMemory->detach();
        }
        delete sharedMemory;
        sharedMemory = nullptr;
    }
    
    // Remove local server
    QLocalServer::removeServer("SpeedyNote_SingleInstance");
    
#ifdef Q_OS_LINUX
    // On Linux, try to clean up stale shared memory segments
    // Use system() instead of QProcess to avoid Qt dependencies in cleanup
    system("ipcs -m | grep $(whoami) | awk '/SpeedyNote/{print $2}' | xargs -r ipcrm -m 2>/dev/null");
#endif
}

void MainWindow::openFileInNewTab(const QString &filePath)
{
    // Create a new tab first
    addNewTab();
    
    // Open the file in the new tab
    if (filePath.toLower().endsWith(".pdf")) {
        openPdfFile(filePath);
    } else if (filePath.toLower().endsWith(".spn")) {
        openSpnPackage(filePath);
    }
}

// ✅ MOUSE DIAL CONTROL IMPLEMENTATION

void MainWindow::mousePressEvent(QMouseEvent *event) {
    // Only track side buttons and right button for dial combinations
    if (event->button() == Qt::RightButton || 
        event->button() == Qt::BackButton || 
        event->button() == Qt::ForwardButton) {
        
        pressedMouseButtons.insert(event->button());
        
        // Start timer for long press detection
        if (!mouseDialTimer->isActive()) {
            mouseDialTimer->start();
        }
    }
    
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (pressedMouseButtons.contains(event->button())) {
        // Check if this was a short press (timer still running) for page navigation
        bool wasShortPress = mouseDialTimer->isActive();
        // Check if this button was part of a combination (more than one button pressed)
        bool wasPartOfCombination = pressedMouseButtons.size() > 1;
        
        pressedMouseButtons.remove(event->button());
        
        // If this was the last button released and we're in dial mode, stop it
        if (pressedMouseButtons.isEmpty()) {
            mouseDialTimer->stop();
            if (mouseDialModeActive) {
                stopMouseDialMode();
            } else if (wasShortPress && !wasPartOfCombination) {
                // Only handle short press if it was NOT part of a combination
                if (event->button() == Qt::BackButton) {
                    goToPreviousPage();
                } else if (event->button() == Qt::ForwardButton) {
                    goToNextPage();
                }
            }
        }
    }
    
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::wheelEvent(QWheelEvent *event) {
    // Only handle wheel events if mouse dial mode is active
    if (mouseDialModeActive) {
        handleMouseWheelDial(event->angleDelta().y());
        event->accept();
        return;
    }
    
    QMainWindow::wheelEvent(event);
}
QString MainWindow::mouseButtonCombinationToString(const QSet<Qt::MouseButton> &buttons) const {
    QStringList buttonNames;
    
    if (buttons.contains(Qt::RightButton)) {
        buttonNames << "Right";
    }
    if (buttons.contains(Qt::BackButton)) {
        buttonNames << "Side1";
    }
    if (buttons.contains(Qt::ForwardButton)) {
        buttonNames << "Side2";
    }
    
    // Sort to ensure consistent combination strings
    buttonNames.sort();
    return buttonNames.join("+");
}

void MainWindow::startMouseDialMode(const QString &combination) {
    if (mouseDialMappings.contains(combination)) {
        QString dialModeKey = mouseDialMappings[combination];
        DialMode mode = dialModeFromString(dialModeKey);
        
        mouseDialModeActive = true;
        currentMouseDialCombination = combination;
        setTemporaryDialMode(mode);
        
        // Show brief tooltip to indicate mode activation
        QToolTip::showText(QCursor::pos(), 
            tr("Mouse Dial: %1").arg(ButtonMappingHelper::internalKeyToDisplay(dialModeKey, true)),
            this, QRect(), 1500);
    }
}

void MainWindow::stopMouseDialMode() {
    if (mouseDialModeActive) {
        // ✅ Trigger the appropriate dial release function before stopping
        if (pageDial) {
            // Emit the sliderReleased signal to trigger the current mode's release function
            emit pageDial->sliderReleased();
        }
        
        mouseDialModeActive = false;
        currentMouseDialCombination.clear();
        clearTemporaryDialMode();
    }
}

void MainWindow::handleMouseWheelDial(int delta) {
    if (!mouseDialModeActive || !dialContainer) return;
    
    // Calculate step size based on current dial mode
    int stepDegrees = 15; // Default step
    
    switch (currentDialMode) {
        case PageSwitching:
            stepDegrees = 45; // 45 degrees per page (8 pages per full rotation)
            break;
        case PresetSelection:
            stepDegrees = 60; // 60 degrees per preset (6 presets per full rotation)
            break;
        case ZoomControl:
            stepDegrees = 30; // 30 degrees per zoom step (12 steps per rotation)
            break;
        case ThicknessControl:
            stepDegrees = 20; // 20 degrees per thickness step (18 steps per rotation)
            break;
        case ToolSwitching:
            stepDegrees = 120; // 120 degrees per tool (3 tools per rotation)
            break;
        case PanAndPageScroll:
            stepDegrees = 15; // 15 degrees per pan step (24 steps per rotation)
            break;
        default:
            stepDegrees = 15;
            break;
    }
    
    // Convert wheel delta to dial angle change (reversed: down = increase, up = decrease)
    int angleChange = (delta > 0) ? -stepDegrees : stepDegrees;
    
    // Apply the angle change to the dial
    int currentAngle = pageDial->value();
    int newAngle = (currentAngle + angleChange + 360) % 360;
    
    pageDial->setValue(newAngle);
    
    // Trigger the dial input handling
    handleDialInput(newAngle);
}

void MainWindow::setMouseDialMapping(const QString &combination, const QString &dialMode) {
    mouseDialMappings[combination] = dialMode;
    saveMouseDialMappings();
}

QString MainWindow::getMouseDialMapping(const QString &combination) const {
    return mouseDialMappings.value(combination, "none");
}

QMap<QString, QString> MainWindow::getMouseDialMappings() const {
    return mouseDialMappings;
}

void MainWindow::saveMouseDialMappings() {
    QSettings settings("SpeedyNote", "App");
    settings.beginGroup("MouseDialMappings");
    
    for (auto it = mouseDialMappings.begin(); it != mouseDialMappings.end(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    
    settings.endGroup();
}

void MainWindow::loadMouseDialMappings() {
    QSettings settings("SpeedyNote", "App");
    settings.beginGroup("MouseDialMappings");
    
    QStringList keys = settings.allKeys();
    
    if (keys.isEmpty()) {
        // Set default mappings
        mouseDialMappings["Right"] = "page_switching";
        mouseDialMappings["Side1"] = "zoom_control";
        mouseDialMappings["Side2"] = "thickness_control";
        mouseDialMappings["Right+Side1"] = "tool_switching";
        mouseDialMappings["Right+Side2"] = "preset_selection";
        mouseDialMappings["Side1+Side2"] = "pan_and_page_scroll"; // ✅ Added 6th combination
        
        saveMouseDialMappings(); // Save defaults
    } else {
        // Load saved mappings
        for (const QString &key : keys) {
            mouseDialMappings[key] = settings.value(key).toString();
        }
    }
    
    settings.endGroup();
}

void MainWindow::onAutoScrollRequested(int direction)
{
    // If there's a pending concurrent save, wait for it to complete before autoscrolling
    // This ensures that content is saved before switching pages
    if (concurrentSaveFuture.isValid() && !concurrentSaveFuture.isFinished()) {
        // Wait for the save to complete (this should be very fast since it's just file I/O)
        concurrentSaveFuture.waitForFinished();
    }
    
    if (direction > 0) {
        goToNextPage();
    } else if (direction < 0) {
        goToPreviousPage();
    }
}

void MainWindow::onEarlySaveRequested()
{
    // Proactive save triggered when approaching autoscroll threshold
    // This ensures content is saved before the actual page switch happens
    InkCanvas *canvas = currentCanvas();
    if (canvas && canvas->isEdited()) {
        saveCurrentPageConcurrent();
    }
}