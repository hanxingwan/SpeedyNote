#include "MainWindow.h"
#include "InkCanvas.h"
#include "ButtonMappingTypes.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScreen>
#include <QApplication> 
#include <QGuiApplication>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include "ToolType.h" // Include the header file where ToolType is defined
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QSpinBox>
#include <QTextStream>
#include <QInputDialog>
#include <QDial>
#include <QSoundEffect>
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
// #include "HandwritingLineEdit.h"
#include "ControlPanelDialog.h"
#include "SDLControllerManager.h"
#include "RecentNotebooksDialog.h" // Added

MainWindow::MainWindow(QWidget *parent) 
    : QMainWindow(parent), benchmarking(false) {

    setWindowTitle(tr("SpeedyNote Beta 0.4.13"));

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
    pdfRenderDPI = settings.value("pdfRenderDPI", 288).toInt();
    setPdfDPI(pdfRenderDPI);
    setupUi();    // ✅ Move all UI setup here

    controllerManager = new SDLControllerManager();
    controllerThread = new QThread(this);

    controllerManager->moveToThread(controllerThread);
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
    
    recentNotebooksManager = new RecentNotebooksManager(this); // Initialize manager

    // Show dial by default after UI is fully initialized
    QTimer::singleShot(200, this, [this]() {
        if (!dialContainer) {
            toggleDial(); // This will create and show the dial
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
    

    QString buttonStyle = R"(
        QPushButton {
            background: transparent; /* Make buttons blend with toolbar */
            border: none; /* Remove default button borders */
            padding: 6px; /* Ensure padding remains */
        }

        QPushButton:hover {
            background: rgba(255, 255, 255, 50); /* Subtle highlight on hover */
        }

        QPushButton:pressed {
            background: rgba(0, 0, 0, 50); /* Darken on click */
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
    loadPdfButton->setToolTip(tr("Load PDF"));
    clearPdfButton->setToolTip(tr("Clear PDF"));
    connect(loadPdfButton, &QPushButton::clicked, this, &MainWindow::loadPdf);
    connect(clearPdfButton, &QPushButton::clicked, this, &MainWindow::clearPdf);

    exportNotebookButton = new QPushButton(this);
    exportNotebookButton->setFixedSize(26, 30);
    QIcon exportIcon(loadThemedIcon("export"));  // Path to your icon in resources
    exportNotebookButton->setIcon(exportIcon);
    exportNotebookButton->setStyleSheet(buttonStyle);
    exportNotebookButton->setToolTip(tr("Export Notebook Into .SNPKG File"));
    importNotebookButton = new QPushButton(this);
    importNotebookButton->setFixedSize(26, 30);
    QIcon importIcon(loadThemedIcon("import"));  // Path to your icon in resources
    importNotebookButton->setIcon(importIcon);
    importNotebookButton->setStyleSheet(buttonStyle);
    importNotebookButton->setToolTip(tr("Import Notebook From .SNPKG File"));

    connect(exportNotebookButton, &QPushButton::clicked, this, [=]() {
        QString filename = QFileDialog::getSaveFileName(this, tr("Export Notebook"), "", "SpeedyNote Package (*.snpkg)");
        if (!filename.isEmpty()) {
            if (!filename.endsWith(".snpkg")) filename += ".snpkg";
            currentCanvas()->exportNotebook(filename);
        }
    });
    
    connect(importNotebookButton, &QPushButton::clicked, this, [=]() {
        QString filename = QFileDialog::getOpenFileName(this, tr("Import Notebook"), "", "SpeedyNote Package (*.snpkg)");
        if (!filename.isEmpty()) {
            currentCanvas()->importNotebook(filename);
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
    toggleTabBarButton->setToolTip(tr("Show/Hide Tabs"));
    toggleTabBarButton->setFixedSize(26, 30);
    toggleTabBarButton->setStyleSheet(buttonStyle);

    selectFolderButton = new QPushButton(this);
    selectFolderButton->setFixedSize(26, 30);
    QIcon folderIcon(loadThemedIcon("folder"));  // Path to your icon in resources
    selectFolderButton->setIcon(folderIcon);
    selectFolderButton->setStyleSheet(buttonStyle);
    selectFolderButton->setToolTip(tr("Select Save Folder"));
    connect(selectFolderButton, &QPushButton::clicked, this, &MainWindow::selectFolder);
    
    
    saveButton = new QPushButton(this);
    saveButton->setFixedSize(26, 30);
    QIcon saveIcon(loadThemedIcon("save"));  // Path to your icon in resources
    saveButton->setIcon(saveIcon);
    saveButton->setStyleSheet(buttonStyle);
    saveButton->setToolTip(tr("Save Current Page"));
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

    // Determine if we're in dark mode to choose appropriate icons and colors
    bool darkMode = isDarkMode();

    redButton = new QPushButton(this);
    redButton->setFixedSize(18, 30);  // Half width
    QString redIconPath = darkMode ? ":/resources/icons/pen_light_red.png" : ":/resources/icons/pen_dark_red.png";
    QIcon redIcon(redIconPath);
    redButton->setIcon(redIcon);
    redButton->setStyleSheet(buttonStyle);
    connect(redButton, &QPushButton::clicked, [this, darkMode]() { 
        QColor redColor = darkMode ? QColor("#FF7755") : QColor("#AA0000");
        currentCanvas()->setPenColor(redColor); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });
    
    blueButton = new QPushButton(this);
    blueButton->setFixedSize(18, 30);  // Half width
    QString blueIconPath = darkMode ? ":/resources/icons/pen_light_blue.png" : ":/resources/icons/pen_dark_blue.png";
    QIcon blueIcon(blueIconPath);
    blueButton->setIcon(blueIcon);
    blueButton->setStyleSheet(buttonStyle);
    connect(blueButton, &QPushButton::clicked, [this, darkMode]() { 
        QColor blueColor = darkMode ? QColor("#66CCFF") : QColor("#0000AA");
        currentCanvas()->setPenColor(blueColor); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });

    yellowButton = new QPushButton(this);
    yellowButton->setFixedSize(18, 30);  // Half width
    QString yellowIconPath = darkMode ? ":/resources/icons/pen_light_yellow.png" : ":/resources/icons/pen_dark_yellow.png";
    QIcon yellowIcon(yellowIconPath);
    yellowButton->setIcon(yellowIcon);
    yellowButton->setStyleSheet(buttonStyle);
    connect(yellowButton, &QPushButton::clicked, [this, darkMode]() { 
        QColor yellowColor = darkMode ? QColor("#EECC00") : QColor("#997700");
        currentCanvas()->setPenColor(yellowColor); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });

    greenButton = new QPushButton(this);
    greenButton->setFixedSize(18, 30);  // Half width
    QString greenIconPath = darkMode ? ":/resources/icons/pen_light_green.png" : ":/resources/icons/pen_dark_green.png";
    QIcon greenIcon(greenIconPath);
    greenButton->setIcon(greenIcon);
    greenButton->setStyleSheet(buttonStyle);
    connect(greenButton, &QPushButton::clicked, [this, darkMode]() { 
        QColor greenColor = darkMode ? QColor("#55FF77") : QColor("#007700");
        currentCanvas()->setPenColor(greenColor); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });

    blackButton = new QPushButton(this);
    blackButton->setFixedSize(18, 30);  // Half width
    QString blackIconPath = darkMode ? ":/resources/icons/pen_light_black.png" : ":/resources/icons/pen_dark_black.png";
    QIcon blackIcon(blackIconPath);
    blackButton->setIcon(blackIcon);
    blackButton->setStyleSheet(buttonStyle);
    connect(blackButton, &QPushButton::clicked, [this]() { 
        currentCanvas()->setPenColor(QColor("#000000")); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });

    whiteButton = new QPushButton(this);
    whiteButton->setFixedSize(18, 30);  // Half width
    QString whiteIconPath = darkMode ? ":/resources/icons/pen_light_white.png" : ":/resources/icons/pen_dark_white.png";
    QIcon whiteIcon(whiteIconPath);
    whiteButton->setIcon(whiteIcon);
    whiteButton->setStyleSheet(buttonStyle);
    connect(whiteButton, &QPushButton::clicked, [this]() { 
        currentCanvas()->setPenColor(QColor("#FFFFFF")); 
        updateDialDisplay(); 
        updateColorButtonStates();
    });
    
    customColorInput = new QLineEdit(this);
    customColorInput->setPlaceholderText("Custom HEX");
    customColorInput->setFixedSize(85, 30);
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
    thicknessSlider->setRange(1, 27);
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
    
    deletePageButton = new QPushButton(this);
    deletePageButton->setFixedSize(26, 30);
    QIcon trashIcon(loadThemedIcon("trash"));  // Path to your icon in resources
    deletePageButton->setIcon(trashIcon);
    deletePageButton->setStyleSheet(buttonStyle);
    deletePageButton->setToolTip(tr("Delete Current Page"));
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




    // 🌟 Left Side: Tabs List
    tabList = new QListWidget(this);
    tabList->setMinimumWidth(122);  // Adjust width as needed
    tabList->setSelectionMode(QAbstractItemView::SingleSelection);


    // 🌟 Add Button for New Tab
    addTabButton = new QPushButton(this);
    QIcon addTab(loadThemedIcon("addtab"));  // Path to your icon in resources
    addTabButton->setIcon(addTab);
    addTabButton->setMinimumWidth(122);
    addTabButton->setFixedHeight(45);  // Adjust height as needed
    connect(addTabButton, &QPushButton::clicked, this, &MainWindow::addNewTab);

    if (!canvasStack) {
        canvasStack = new QStackedWidget(this);
    }

    connect(tabList, &QListWidget::currentRowChanged, this, &MainWindow::switchTab);

    sidebarContainer = new QWidget(this);  // <-- New container
    sidebarContainer->setObjectName("sidebarContainer");
    sidebarContainer->setContentsMargins(5, 0, 5, 5);  // <-- Remove margins
    sidebarContainer->setMaximumWidth(140);  // <-- Set max width
    QVBoxLayout *tabLayout = new QVBoxLayout(sidebarContainer);
    tabLayout->setContentsMargins(0, 0, 1, 0); 
    tabLayout->addWidget(tabList);
    tabLayout->addWidget(addTabButton);

    connect(toggleTabBarButton, &QPushButton::clicked, this, [=]() {
        bool isVisible = sidebarContainer->isVisible();
        sidebarContainer->setVisible(!isVisible);

        QTimer::singleShot(0, this, [this]() {
            if (auto *canvas = currentCanvas()) {
                canvas->setMaximumSize(canvas->getCanvasSize());
                // canvas->adjustSize();
            }
        });

        // Force layout recalc & canvas size restore
        // Force layout recalculation

    
        // Optional: switch icon or tooltip
        // toggleTabBarButton->setIcon(loadThemedIcon(isVisible ? "show_tabs" : "hide_tabs"));
        // toggleTabBarButton->setToolTip(isVisible ? "Show Tabs" : "Hide Tabs");
    });

    


    // Previous page button
    prevPageButton = new QPushButton(this);
    prevPageButton->setFixedSize(26, 30);
    prevPageButton->setText("◀");
    prevPageButton->setStyleSheet(buttonStyle);
    prevPageButton->setToolTip(tr("Previous Page"));
    connect(prevPageButton, &QPushButton::clicked, this, &MainWindow::goToPreviousPage);

    pageInput = new QSpinBox(this);
    pageInput->setFixedSize(42, 30);
    pageInput->setMinimum(1);
    pageInput->setMaximum(9999);
    pageInput->setValue(1);
    pageInput->setMaximumWidth(100);
    connect(pageInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onPageInputChanged);

    // Next page button
    nextPageButton = new QPushButton(this);
    nextPageButton->setFixedSize(26, 30);
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


    // ✅ Ensure at least one preset exists (theme-aware default color)
    // Initialize color presets based on dark/light mode
    colorPresets.enqueue(getDefaultPenColor());
    if (darkMode) {
        colorPresets.enqueue(QColor("#FF7755"));  // Light red for dark mode
        colorPresets.enqueue(QColor("#EECC00"));  // Light yellow for dark mode
        colorPresets.enqueue(QColor("#66CCFF"));  // Light blue for dark mode
        colorPresets.enqueue(QColor("#55FF77"));  // Light green for dark mode
    } else {
        colorPresets.enqueue(QColor("#AA0000"));  // Dark red for light mode
        colorPresets.enqueue(QColor("#997700"));  // Dark yellow for light mode
        colorPresets.enqueue(QColor("#0000AA"));  // Dark blue for light mode
        colorPresets.enqueue(QColor("#007700"));  // Dark green for light mode
    }
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
    openRecentNotebooksButton->setToolTip(tr("Open Recent Notebooks"));
    openRecentNotebooksButton->setFixedSize(26, 30);
    connect(openRecentNotebooksButton, &QPushButton::clicked, this, &MainWindow::openRecentNotebooksDialog);

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
    
    controlLayout->addWidget(toggleTabBarButton);
    controlLayout->addWidget(selectFolderButton);

    controlLayout->addWidget(exportNotebookButton);
    controlLayout->addWidget(importNotebookButton);
    controlLayout->addWidget(loadPdfButton);
    controlLayout->addWidget(clearPdfButton);
    controlLayout->addWidget(backgroundButton);
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

    QHBoxLayout *content_layout = new QHBoxLayout;
    content_layout->setContentsMargins(0, 0, 0, 0); 
    content_layout->addWidget(sidebarContainer); 
    content_layout->addWidget(canvasContainer, 1); // Add stretch factor to expand canvas

    QWidget *container = new QWidget;
    container->setObjectName("container");
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 0, 0);  // ✅ Remove extra margins
    // mainLayout->setSpacing(0); // ✅ Remove spacing between toolbar and content
    mainLayout->addWidget(controlBar);
    mainLayout->addLayout(content_layout);

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

    // Initialize responsive toolbar layout
    createSingleRowLayout();  // Start with single row layout

}

MainWindow::~MainWindow() {

    saveButtonMappings();  // ✅ Save on exit, as backup
    delete canvas;
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
    qreal thickness = 90.0 * value / currentCanvas()->getZoom(); 
    currentCanvas()->setPenThickness(thickness);
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
    updateDialDisplay();
}

void MainWindow::setPenTool() {
    if (!currentCanvas()) return;
    currentCanvas()->setTool(ToolType::Pen);
    updateToolButtonStates();
    updateDialDisplay();
}

void MainWindow::setMarkerTool() {
    if (!currentCanvas()) return;
    currentCanvas()->setTool(ToolType::Marker);
    updateToolButtonStates();
    updateDialDisplay();
}

void MainWindow::setEraserTool() {
    if (!currentCanvas()) return;
    currentCanvas()->setTool(ToolType::Eraser);
    updateToolButtonStates();
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

void MainWindow::selectFolder() {
    QString folder = QFileDialog::getExistingDirectory(this, tr("Select Save Folder"));
    if (!folder.isEmpty()) {
        InkCanvas *canvas = currentCanvas();
        if (canvas) {
            if (canvas->isEdited()){
                saveCurrentPage();
            }
            canvas->setSaveFolder(folder);
        switchPageWithDirection(1, 1); // Going to page 1 is forward direction
        pageInput->setValue(1);
        updateTabLabel();
            recentNotebooksManager->addRecentNotebook(folder, canvas); // Track when folder is selected
        }
    }
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
    updateZoom();
    // It seems panXSlider and panYSlider can be null here during startup.
    if(panXSlider && panYSlider){
    canvas->setLastPanX(panXSlider->maximum());
    canvas->setLastPanY(panYSlider->maximum());
        
        // ✅ Enhanced scroll-on-top functionality
        if (scrollOnTopEnabled && panYSlider->maximum() > 0) {
            if (pageNumber > oldPage) {
                // Forward page switching → scroll to top
                panYSlider->setValue(0);
            } else if (pageNumber < oldPage) {
                // Backward page switching → scroll to bottom
                panYSlider->setValue(panYSlider->maximum());
            }
        }
    }
    updateDialDisplay();
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
    updateZoom();
    // It seems panXSlider and panYSlider can be null here during startup.
    if(panXSlider && panYSlider){
        canvas->setLastPanX(panXSlider->maximum());
        canvas->setLastPanY(panYSlider->maximum());
        
        // ✅ Enhanced scroll-on-top functionality with explicit direction
        if (scrollOnTopEnabled && panYSlider->maximum() > 0) {
            if (direction > 0) {
                // Forward page switching → scroll to top
                panYSlider->setValue(0);
            } else if (direction < 0) {
                // Backward page switching → scroll to bottom
                panYSlider->setValue(panYSlider->maximum());
            }
        }
    }
    updateDialDisplay();
}

void MainWindow::deleteCurrentPage() {
    currentCanvas()->deletePage(getCurrentPageForCanvas(currentCanvas()));
}

void MainWindow::saveCurrentPage() {
    currentCanvas()->saveToFile(getCurrentPageForCanvas(currentCanvas()));
}

void MainWindow::saveCurrentPageConcurrent() {
    InkCanvas *canvas = currentCanvas();
    if (!canvas || !canvas->isEdited()) return;
    
    int pageNumber = getCurrentPageForCanvas(canvas);
    QString saveFolder = canvas->getSaveFolder();
    
    if (saveFolder.isEmpty()) return;
    
    // Create a copy of the buffer for concurrent saving
    QPixmap bufferCopy = canvas->getBuffer();
    
    // Run the save operation concurrently
    QFuture<void> future = QtConcurrent::run([saveFolder, pageNumber, bufferCopy]() {
        // Get notebook ID from the save folder (similar to how InkCanvas does it)
        QString notebookId = "notebook"; // Default fallback
        QString idFile = saveFolder + "/.notebook_id.txt";
        if (QFile::exists(idFile)) {
            QFile file(idFile);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                notebookId = in.readLine().trimmed();
                file.close();
            }
        }
        
        QString filePath = saveFolder + QString("/%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));
        
        QImage image(bufferCopy.size(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.drawPixmap(0, 0, bufferCopy);
        image.save(filePath, "PNG");
    });
    
    // Mark as not edited since we're saving
    canvas->setEdited(false);
}

void MainWindow::selectBackground() {
    QString filePath = QFileDialog::getOpenFileName(this, tr("Select Background Image"), "", "Images (*.png *.jpg *.jpeg)");
    if (!filePath.isEmpty()) {
        currentCanvas()->setBackground(filePath, getCurrentPageForCanvas(currentCanvas()));
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
    
    // Adjust viewport size for 2-row toolbar layout
    QSize effectiveViewportSize = actualViewportSize;
    if (isToolbarTwoRows) {
        // In 2-row mode, reduce effective height by toolbar height
        effectiveViewportSize.setHeight(actualViewportSize.height() - 80); // Approximate toolbar height
    } else {
        // In single-row mode, reduce effective height by toolbar height
        effectiveViewportSize.setHeight(actualViewportSize.height() - 50); // Approximate toolbar height
    }
    
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
    panYSlider->setRange(0, maxPanY_scaled);
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
        updateTabLabel(); // ✅ Update the tab name after assigning a PDF
    }
}

void MainWindow::clearPdf() {
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;
    
    canvas->clearPdf();
}


void MainWindow::switchTab(int index) {
    if (!canvasStack || !tabList || !pageInput || !zoomSlider || !panXSlider || !panYSlider) {
        qDebug() << "Error: switchTab() called before UI was fully initialized!";
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
            updateDialButtonState();     // Update dial button state when switching tabs
            updateFastForwardButtonState(); // Update fast forward button state when switching tabs
            updateToolButtonStates();   // Update tool button states when switching tabs
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

    // ✅ Create the label (Tab Name)
    QLabel *tabLabel = new QLabel(QString("Tab %1").arg(newTabIndex + 1), tabWidget);    
    tabLabel->setObjectName("tabLabel"); // ✅ Name the label for easy retrieval later
    tabLabel->setWordWrap(true); // ✅ Allow text to wrap
    tabLabel->setFixedWidth(95); // ✅ Adjust width for better readability
    tabLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // ✅ Create the close button (❌)
    QPushButton *closeButton = new QPushButton(tabWidget);
    closeButton->setFixedSize(10, 10); // Adjust button size
    closeButton->setIcon(loadThemedIcon("cross")); // Set themed icon
    closeButton->setStyleSheet(R"(
        QPushButton { 
            border: none; 
            background: transparent; 
            border-radius: 5px;
        }
        QPushButton:hover { 
            background: rgba(255, 0, 0, 100); 
            border-radius: 5px;
        }
        QPushButton:pressed { 
            background: rgba(255, 0, 0, 150); 
            border-radius: 5px;
        }
    )"); // Themed styling with hover and press effects
    
    // ✅ Create new InkCanvas instance EARLIER so it can be captured by the lambda
    InkCanvas *newCanvas = new InkCanvas(this);
    
    // ✅ Handle tab closing when the button is clicked
    connect(closeButton, &QPushButton::clicked, this, [=]() { // newCanvas is now captured

        // Prevent closing if it's the last remaining tab
        if (tabList->count() <= 1) {
            // Optional: show a message or do nothing silently
            QMessageBox::information(this, tr("Notice"), tr("At least one tab must remain open."));

            return;
        }

        // Find the index of the tab associated with this button's parent (tabWidget)
        int indexToRemove = -1;
        // newCanvas is captured by the lambda, representing the canvas of the tab being closed.
        // tabWidget is also captured.
        for (int i = 0; i < tabList->count(); ++i) {
            if (tabList->itemWidget(tabList->item(i)) == tabWidget) {
                indexToRemove = i;
                break;
            }
        }

        if (indexToRemove == -1) {
            qWarning() << "Could not find tab to remove based on tabWidget.";
            // Fallback or error handling if needed, though this shouldn't happen if tabWidget is valid.
            // As a fallback, try to find the index based on newCanvas if lists are in sync.
            for (int i = 0; i < canvasStack->count(); ++i) {
                if (canvasStack->widget(i) == newCanvas) {
                    indexToRemove = i;
                    break;
                }
            }
            if (indexToRemove == -1) {
                 qWarning() << "Could not find tab to remove based on newCanvas either.";
                 return; // Critical error, cannot proceed.
            }
        }
        
        // At this point, newCanvas is the InkCanvas instance for the tab being closed.
        // And indexToRemove is its index in tabList and canvasStack.

        // 1. Ensure the notebook has a unique save folder if it's temporary/edited
        ensureTabHasUniqueSaveFolder(newCanvas); // Pass the specific canvas

        // 2. Get the final save folder path
        QString folderPath = newCanvas->getSaveFolder();
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";

        // 3. Update cover preview and recent list if it's a permanent notebook
        if (!folderPath.isEmpty() && folderPath != tempDir && recentNotebooksManager) {
            recentNotebooksManager->generateAndSaveCoverPreview(folderPath, newCanvas);
            // Add/update in recent list. This also moves it to the top.
            recentNotebooksManager->addRecentNotebook(folderPath, newCanvas);
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
                            tabNameText = QFileInfo(pdfPath).fileName();
                        }
                    }
                }
                // Fallback to folder name if no PDF or PDF path invalid
                if (tabNameText.isEmpty()) {
                    tabNameText = QFileInfo(folderPath).fileName();
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
    
    // ✅ Create the tab item and set widget
    QListWidgetItem *tabItem = new QListWidgetItem();
    tabItem->setSizeHint(QSize(84, 45)); // ✅ Adjust height only, keep default style
    tabList->addItem(tabItem);
    tabList->setItemWidget(tabItem, tabWidget);  // Attach tab layout

    canvasStack->addWidget(newCanvas);

    // ✅ Connect touch gesture signals
    connect(newCanvas, &InkCanvas::zoomChanged, this, &MainWindow::handleTouchZoomChange);
    connect(newCanvas, &InkCanvas::panChanged, this, &MainWindow::handleTouchPanChange);
    connect(newCanvas, &InkCanvas::touchGestureEnded, this, &MainWindow::handleTouchGestureEnd);
    connect(newCanvas, &InkCanvas::ropeSelectionCompleted, this, &MainWindow::showRopeSelectionMenu);
    
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
        canvasStack->removeWidget(canvasWidget); // Remove from stack
        // InkCanvas *canvasInstance = qobject_cast<InkCanvas*>(canvasWidget);
        // if (canvasInstance) {
        //     QString folderPath = canvasInstance->getSaveFolder();
        //     QString tempDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";
        //     if (!folderPath.isEmpty() && folderPath != tempDir && recentNotebooksManager) {
        //         recentNotebooksManager->addRecentNotebook(folderPath, canvasInstance); // Moved to close button lambda
        //     }
        // }
        delete canvasWidget; // Now delete the widget (and its InkCanvas)
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

void MainWindow::ensureTabHasUniqueSaveFolder(InkCanvas* canvas) {
    if (!canvas) return;

    if (canvasStack->count() == 0) return;

    QString currentFolder = canvas->getSaveFolder();
    QString tempFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";

    if (currentFolder.isEmpty() || currentFolder == tempFolder) {

        QDir sourceDir(tempFolder);
        QStringList pageFiles = sourceDir.entryList(QStringList() << "*.png", QDir::Files);

        // No pages to save → skip prompting
        if (pageFiles.isEmpty()) {
            return;
        }

        QMessageBox::warning(this, tr("Unsaved Notebook"),
                             tr("This notebook is still using a temporary session folder.\nPlease select a permanent folder to avoid data loss."));

        QString selectedFolder = QFileDialog::getExistingDirectory(this, tr("Select Save Folder"));
        if (selectedFolder.isEmpty()) return;

        QDir destDir(selectedFolder);
        if (!destDir.exists()) {
            QDir().mkpath(selectedFolder);
        }

        // Copy contents from temp to selected folder
        for (const QString &file : sourceDir.entryList(QDir::Files)) {
            QString srcFilePath = tempFolder + "/" + file;
            QString dstFilePath = selectedFolder + "/" + file;

            // If file already exists at destination, remove it to avoid rename failure
            if (QFile::exists(dstFilePath)) {
                QFile::remove(dstFilePath);
            }

            QFile::rename(srcFilePath, dstFilePath);  // This moves the file
        }

        canvas->setSaveFolder(selectedFolder);
        // updateTabLabel(); // No longer needed here, handled by the close button lambda
    }

    return;
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

    QString folderPath = canvas->getSaveFolder(); // ✅ Get save folder
    if (folderPath.isEmpty()) return;

    QString tabName;

    // ✅ Check if there is an assigned PDF
    QString metadataFile = folderPath + "/.pdf_path.txt";
    if (QFile::exists(metadataFile)) {
        QFile file(metadataFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString pdfPath = in.readLine().trimmed();
            file.close();

            // ✅ Extract just the PDF filename (not full path)
            QFileInfo pdfInfo(pdfPath);
            if (pdfInfo.exists()) {
                tabName = pdfInfo.fileName(); // e.g., "mydocument.pdf"
            }
        }
    }

    // ✅ If no PDF, use the folder name
    if (tabName.isEmpty()) {
        QFileInfo folderInfo(folderPath);
        tabName = folderInfo.fileName(); // e.g., "MyNotebook"
    }

    QListWidgetItem *tabItem = tabList->item(index);
    if (tabItem) {
        QWidget *tabWidget = tabList->itemWidget(tabItem); // Get the tab's custom widget
        if (tabWidget) {
            QLabel *tabLabel = tabWidget->findChild<QLabel *>(); // Get the QLabel inside
            if (tabLabel) {
                tabLabel->setText(tabName); // ✅ Update tab label
                tabLabel->setWordWrap(true);
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
        pageInput->setValue(newPage);
    }
}

void MainWindow::goToNextPage() {
    int currentPage = getCurrentPageForCanvas(currentCanvas()) + 1;  // Convert to 1-based
    int newPage = currentPage + 1;
    switchPageWithDirection(newPage, 1); // 1 indicates forward
    pageInput->setValue(newPage);
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
    
    // Define margins from edges
    int rightMargin = 20;  // Distance from right edge
    int topMargin = 20;    // Distance from top edge (below toolbar)
    
    // Calculate ideal position (top-right corner with margins)
    int idealX = windowWidth - dialWidth - rightMargin;
    int idealY = toolbarHeight + topMargin;
    
    // Ensure dial stays within window bounds with minimum margins
    int minMargin = 10;
    int maxX = windowWidth - dialWidth - minMargin;
    int maxY = windowHeight - dialHeight - minMargin;
    
    // Clamp position to stay within bounds
    int finalX = qBound(minMargin, idealX, maxX);
    int finalY = qBound(toolbarHeight + minMargin, idealY, maxY);
    
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
            dialDisplay->setText(QString(tr("\n\nThickness\n%1").arg(currentCanvas()->getPenThickness())));
            dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/thickness_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            break;
        case DialMode::ZoomControl:
            dialDisplay->setText(QString(tr("\n\nZoom\n%1%").arg(zoomSlider->value() * initialDpr)));
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
            SDL_GameController *controller = controllerManager->getController();
            if (controller) {
                SDL_GameControllerRumble(controller, 0xA000, 0xF000, 10);  // Vibrate shortly
            }
    
            grossTotalClicks += 1;
            tempClicks = currentClicks;
            updateDialDisplay();
    
            if (isLowResPreviewEnabled()) {
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

        SDL_GameController *controller = controllerManager->getController();

        if (controller) {
            SDL_GameControllerRumble(controller, 0xA000, 0xF000, 20);  // ✅ Vibrate controller
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
        }
        // Handle wheel events for scrolling
        else if (event->type() == QEvent::Wheel) {
            QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
            
            // Check if we need scrolling
            bool needHorizontalScroll = panXSlider->maximum() > 0;
            bool needVerticalScroll = panYSlider->maximum() > 0;
            
            // Handle vertical scrolling (Y axis)
            if (wheelEvent->angleDelta().y() != 0 && needVerticalScroll) {
                // Calculate scroll amount (negative because wheel up should scroll up)
                int scrollDelta = -wheelEvent->angleDelta().y() / 8; // Convert from 1/8 degree units
                scrollDelta = scrollDelta / 15; // Convert to steps (typical wheel step is 15 degrees)
                
                // Much faster base scroll speed - aim for ~3-5 scrolls to reach bottom
                int baseScrollAmount = panYSlider->maximum() / 8; // 1/8 of total range per scroll
                scrollDelta = scrollDelta * qMax(baseScrollAmount, 50); // Minimum 50 units per scroll
                
                // Apply the scroll
                int currentPan = panYSlider->value();
                int newPan = qBound(panYSlider->minimum(), currentPan + scrollDelta, panYSlider->maximum());
                panYSlider->setValue(newPan);
                
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
        dialClickSound = new QSoundEffect(this);
        dialClickSound->setSource(QUrl::fromLocalFile(":/resources/sounds/dial_click.wav")); // ✅ Path to the sound file
        dialClickSound->setVolume(0.8);  // ✅ Set volume (0.0 - 1.0)
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
    int newZoom = qBound(10, zoomSlider->value() + (delta / 4), 400);  
    zoomSlider->setValue(newZoom);
    updateZoom();  // ✅ Ensure zoom updates immediately
    updateThickness(thicknessSlider->value()); // ✅ Update thickness for manual zoom changes
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

        SDL_GameController *controller = controllerManager->getController();
        if (controller) {
            SDL_GameControllerRumble(controller, 0xA000, 0xF000, 25);  // Vibrate shortly
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
        SDL_GameController *controller = controllerManager->getController();
            if (controller) {
                SDL_GameControllerRumble(controller, 0xA000, 0xF000, 25);  // Vibrate shortly
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

// to support dark mode icon switching.
bool MainWindow::isDarkMode() {
    QColor bg = palette().color(QPalette::Window);
    return bg.lightness() < 128;  // Lightness scale: 0 (black) - 255 (white)
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
    
    // Update tab list selection color
    if (tabList) {
        tabList->setStyleSheet(QString(R"(
        QListWidget {
            outline: none;
            border: none;
        }
        QListWidget::item {
            outline: none;
            border: none;
            padding: 2px;
        }
        QListWidget::item:selected {
            background-color: rgba(%1, %2, %3, 100) !important;
            border-left: 3px solid %4 !important;
            outline: none !important;
        }
        QListWidget::item:selected:active {
            background-color: rgba(%1, %2, %3, 100) !important;
            border-left: 3px solid %4 !important;
            outline: none !important;
        }
        QListWidget::item:selected:!active {
            background-color: rgba(%1, %2, %3, 100) !important;
            border-left: 3px solid %4 !important;
            outline: none !important;
        }
        QListWidget::item:hover {
            background-color: rgba(%1, %2, %3, 50);
            outline: none;
        }
        QListWidget::item:focus {
            outline: none !important;
            border: none;
        }
        )").arg(accentColor.red())
           .arg(accentColor.green())
           .arg(accentColor.blue())
           .arg(accentColor.name()));
    }
    

    
    // Force icon reload for all buttons that use themed icons
    if (loadPdfButton) loadPdfButton->setIcon(loadThemedIcon("pdf"));
    if (clearPdfButton) clearPdfButton->setIcon(loadThemedIcon("pdfdelete"));
    if (exportNotebookButton) exportNotebookButton->setIcon(loadThemedIcon("export"));
    if (importNotebookButton) importNotebookButton->setIcon(loadThemedIcon("import"));
    if (benchmarkButton) benchmarkButton->setIcon(loadThemedIcon("benchmark"));
    if (toggleTabBarButton) toggleTabBarButton->setIcon(loadThemedIcon("tabs"));
    if (selectFolderButton) selectFolderButton->setIcon(loadThemedIcon("folder"));
    if (saveButton) saveButton->setIcon(loadThemedIcon("save"));
    if (saveAnnotatedButton) saveAnnotatedButton->setIcon(loadThemedIcon("saveannotated"));
    if (fullscreenButton) fullscreenButton->setIcon(loadThemedIcon("fullscreen"));
    if (backgroundButton) backgroundButton->setIcon(loadThemedIcon("background"));
    if (straightLineToggleButton) straightLineToggleButton->setIcon(loadThemedIcon("straightLine"));
    if (ropeToolButton) ropeToolButton->setIcon(loadThemedIcon("rope"));
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
    
    // Update color buttons with new theme
    bool darkMode = isDarkMode();
    if (redButton) {
        QString redIconPath = darkMode ? ":/resources/icons/pen_light_red.png" : ":/resources/icons/pen_dark_red.png";
        redButton->setIcon(QIcon(redIconPath));
    }
    if (blueButton) {
        QString blueIconPath = darkMode ? ":/resources/icons/pen_light_blue.png" : ":/resources/icons/pen_dark_blue.png";
        blueButton->setIcon(QIcon(blueIconPath));
    }
    if (yellowButton) {
        QString yellowIconPath = darkMode ? ":/resources/icons/pen_light_yellow.png" : ":/resources/icons/pen_dark_yellow.png";
        yellowButton->setIcon(QIcon(yellowIconPath));
    }
    if (greenButton) {
        QString greenIconPath = darkMode ? ":/resources/icons/pen_light_green.png" : ":/resources/icons/pen_dark_green.png";
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
    
    // Update tab close button icons
    if (tabList) {
        for (int i = 0; i < tabList->count(); ++i) {
            QListWidgetItem *item = tabList->item(i);
            if (item) {
                QWidget *tabWidget = tabList->itemWidget(item);
                if (tabWidget) {
                    QPushButton *closeButton = tabWidget->findChild<QPushButton*>();
                    if (closeButton) {
                        closeButton->setIcon(loadThemedIcon("cross"));
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
}

void MainWindow::loadThemeSettings() {
    QSettings settings("SpeedyNote", "App");
    useCustomAccentColor = settings.value("useCustomAccentColor", false).toBool();
    QString colorName = settings.value("customAccentColor", "#0078D4").toString();
    customAccentColor = QColor(colorName);
    
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
        default:
            break;
    }
}


void MainWindow::importNotebookFromFile(const QString &packageFile) {

    QString destDir = QFileDialog::getExistingDirectory(this, tr("Select Working Directory for Notebook"));

    if (destDir.isEmpty()) {
        QMessageBox::warning(this, tr("Import Cancelled"), tr("No directory selected. Notebook will not be opened."));
        return;
    }

    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;

    canvas->importNotebookTo(packageFile, destDir);

    // Change saveFolder in InkCanvas
    canvas->setSaveFolder(destDir);
    canvas->loadPage(0);
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
        
        // First, remember current sidebar state
        sidebarWasVisibleBeforeFullscreen = sidebarContainer->isVisible();
        
        // Hide sidebar if it's visible
        if (sidebarContainer->isVisible()) {
            sidebarContainer->setVisible(false);
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
        
        // Restore sidebar to its previous state
        sidebarContainer->setVisible(sidebarWasVisibleBeforeFullscreen);
        
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
        canvas->setZoom(newZoom);
        canvas->setLastZoomLevel(newZoom);
        updatePanRange();
        // updateThickness(thicknessSlider->value()); // ✅ REMOVED: Don't reset thickness during touch zoom
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
    
    // Update sliders without triggering their valueChanged signals
    panXSlider->blockSignals(true);
    panYSlider->blockSignals(true);
    panXSlider->setValue(panX);
    panYSlider->setValue(panY);
    panXSlider->blockSignals(false);
    panYSlider->blockSignals(false);
    
    // Update canvas pan directly
    InkCanvas *canvas = currentCanvas();
    if (canvas) {
        canvas->setPanX(panX);
        canvas->setPanY(panY);
        canvas->setLastPanX(panX);
        canvas->setLastPanY(panY);
    }
}

// Now add two new slots to handle gesture end events
void MainWindow::handleTouchGestureEnd() {
    // Hide scrollbars immediately when touch gesture ends
    panXSlider->setVisible(false);
    panYSlider->setVisible(false);
    scrollbarsVisible = false;
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
    
    // Set the selected property for the matching color button based on mode
    QColor redColor = darkMode ? QColor("#FF7755") : QColor("#AA0000");
    QColor blueColor = darkMode ? QColor("#66CCFF") : QColor("#0000AA");
    QColor yellowColor = darkMode ? QColor("#EECC00") : QColor("#997700");
    QColor greenColor = darkMode ? QColor("#55FF77") : QColor("#007700");
    
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

void MainWindow::openRecentNotebooksDialog() {
    RecentNotebooksDialog dialog(this, recentNotebooksManager, this);
    dialog.exec();
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
    int threshold = areZoomButtonsVisible() ? 1362 : 1252;
    
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
    newLayout->addWidget(selectFolderButton);
    newLayout->addWidget(exportNotebookButton);
    newLayout->addWidget(importNotebookButton);
    newLayout->addWidget(loadPdfButton);
    newLayout->addWidget(clearPdfButton);
    newLayout->addWidget(backgroundButton);
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
    newFirstRowLayout->addWidget(selectFolderButton);
    newFirstRowLayout->addWidget(exportNotebookButton);
    newFirstRowLayout->addWidget(importNotebookButton);
    newFirstRowLayout->addWidget(loadPdfButton);
    newFirstRowLayout->addWidget(clearPdfButton);
    newFirstRowLayout->addWidget(backgroundButton);
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
        default:
            break;
    }
}

void MainWindow::addKeyboardMapping(const QString &keySequence, const QString &action) {
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
    for (const QString &key : keys) {
        QString value = settings.value(key).toString();
        keyboardMappings[key] = value;
        keyboardActionMapping[key] = stringToAction(value);
    }
    settings.endGroup();
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
    updateZoom();
    updateThickness(thicknessSlider->value());
}

void MainWindow::saveDefaultBackgroundSettings(BackgroundStyle style, QColor color, int density) {
    QSettings settings("SpeedyNote", "App");
    settings.setValue("defaultBackgroundStyle", static_cast<int>(style));
    settings.setValue("defaultBackgroundColor", color.name());
    settings.setValue("defaultBackgroundDensity", density);
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

void MainWindow::showRopeSelectionMenu(const QPoint &position) {
    // Create context menu for rope tool selection
    QMenu *contextMenu = new QMenu(this);
    contextMenu->setAttribute(Qt::WA_DeleteOnClose);
    
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