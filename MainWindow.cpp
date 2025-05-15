
#include "MainWindow.h"
#include "InkCanvas.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScreen>
#include <QApplication> 
#include <QGuiApplication>
#include <QLineEdit>
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
// #include "HandwritingLineEdit.h"
#include "ControlPanelDialog.h"
#include "SDLControllerManager.h"

MainWindow::MainWindow(QWidget *parent) 
    : QMainWindow(parent), benchmarking(false) {

    setWindowTitle("SpeedyNote Beta 0.4.1");
    

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
   
    zoomSlider->setValue(100 / initialDpr); // Set initial zoom level based on DPR

    QSettings settings("SpeedyNote", "App");
    lowResPreviewEnabled = settings.value("lowResPreviewEnabled", true).toBool();

    setBenchmarkControlsVisible(false);

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
    )";


    loadPdfButton = new QPushButton(this);
    clearPdfButton = new QPushButton(this);
    loadPdfButton->setFixedSize(30, 30);
    clearPdfButton->setFixedSize(30, 30);
    QIcon pdfIcon(loadThemedIcon("pdf"));  // Path to your icon in resources
    QIcon pdfDeleteIcon(loadThemedIcon("pdfdelete"));  // Path to your icon in resources
    loadPdfButton->setIcon(pdfIcon);
    clearPdfButton->setIcon(pdfDeleteIcon);
    loadPdfButton->setStyleSheet(buttonStyle);
    clearPdfButton->setStyleSheet(buttonStyle);
    loadPdfButton->setToolTip("Load PDF");
    clearPdfButton->setToolTip("Clear PDF");
    connect(loadPdfButton, &QPushButton::clicked, this, &MainWindow::loadPdf);
    connect(clearPdfButton, &QPushButton::clicked, this, &MainWindow::clearPdf);

    exportNotebookButton = new QPushButton(this);
    exportNotebookButton->setFixedSize(30, 30);
    QIcon exportIcon(loadThemedIcon("export"));  // Path to your icon in resources
    exportNotebookButton->setIcon(exportIcon);
    exportNotebookButton->setStyleSheet(buttonStyle);
    exportNotebookButton->setToolTip("Export Notebook Into .SNPKG File");
    importNotebookButton = new QPushButton(this);
    importNotebookButton->setFixedSize(30, 30);
    QIcon importIcon(loadThemedIcon("import"));  // Path to your icon in resources
    importNotebookButton->setIcon(importIcon);
    importNotebookButton->setStyleSheet(buttonStyle);
    importNotebookButton->setToolTip("Import Notebook From .SNPKG File");

    connect(exportNotebookButton, &QPushButton::clicked, this, [=]() {
        QString filename = QFileDialog::getSaveFileName(this, "Export Notebook", "", "SpeedyNote Package (*.snpkg)");
        if (!filename.isEmpty()) {
            if (!filename.endsWith(".snpkg")) filename += ".snpkg";
            currentCanvas()->exportNotebook(filename);
        }
    });
    
    connect(importNotebookButton, &QPushButton::clicked, this, [=]() {
        QString filename = QFileDialog::getOpenFileName(this, "Import Notebook", "", "SpeedyNote Package (*.snpkg)");
        if (!filename.isEmpty()) {
            currentCanvas()->importNotebook(filename);
        }
    });

    benchmarkButton = new QPushButton(this);
    QIcon benchmarkIcon(loadThemedIcon("benchmark"));  // Path to your icon in resources
    benchmarkButton->setIcon(benchmarkIcon);
    benchmarkButton->setFixedSize(30, 30); // Make the benchmark button smaller
    benchmarkButton->setStyleSheet(buttonStyle);
    benchmarkButton->setToolTip("Toggle Benchmark");
    benchmarkLabel = new QLabel("PR:N/A", this);
    benchmarkLabel->setFixedHeight(30);  // Make the benchmark bar smaller

    toggleTabBarButton = new QPushButton(this);
    toggleTabBarButton->setIcon(loadThemedIcon("tabs"));  // You can design separate icons for "show" and "hide"
    toggleTabBarButton->setToolTip("Show/Hide Tabs");
    toggleTabBarButton->setFixedSize(30, 30);
    toggleTabBarButton->setStyleSheet(buttonStyle);

    selectFolderButton = new QPushButton(this);
    selectFolderButton->setFixedSize(30, 30);
    QIcon folderIcon(loadThemedIcon("folder"));  // Path to your icon in resources
    selectFolderButton->setIcon(folderIcon);
    selectFolderButton->setStyleSheet(buttonStyle);
    selectFolderButton->setToolTip("Select Save Folder");
    connect(selectFolderButton, &QPushButton::clicked, this, &MainWindow::selectFolder);
    
    
    saveButton = new QPushButton(this);
    saveButton->setFixedSize(30, 30);
    QIcon saveIcon(loadThemedIcon("save"));  // Path to your icon in resources
    saveButton->setIcon(saveIcon);
    saveButton->setStyleSheet(buttonStyle);
    saveButton->setToolTip("Save Current Page");
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveCurrentPage);
    
    saveAnnotatedButton = new QPushButton(this);
    saveAnnotatedButton->setFixedSize(30, 30);
    QIcon saveAnnotatedIcon(loadThemedIcon("saveannotated"));  // Path to your icon in resources
    saveAnnotatedButton->setIcon(saveAnnotatedIcon);
    saveAnnotatedButton->setStyleSheet(buttonStyle);
    saveAnnotatedButton->setToolTip("Save Page with Background");
    connect(saveAnnotatedButton, &QPushButton::clicked, this, &MainWindow::saveAnnotated);

    fullscreenButton = new QPushButton(this);
    fullscreenButton->setIcon(loadThemedIcon("fullscreen"));  // Load from resources
    fullscreenButton->setFixedSize(30, 30);
    fullscreenButton->setToolTip("Toggle Fullscreen");
    fullscreenButton->setStyleSheet(buttonStyle);

    // ✅ Connect button click to toggleFullscreen() function
    connect(fullscreenButton, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);

    redButton = new QPushButton(this);
    redButton->setFixedSize(30, 30);
    QIcon redIcon(":/resources/icons/red.png");  // Path to your icon in resources
    redButton->setIcon(redIcon);
    redButton->setStyleSheet(buttonStyle);
    connect(redButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#EE0000")); updateDialDisplay(); });
    
    blueButton = new QPushButton(this);
    blueButton->setFixedSize(30, 30);
    QIcon blueIcon(":/resources/icons/blue.png");  // Path to your icon in resources
    blueButton->setIcon(blueIcon);
    blueButton->setStyleSheet(buttonStyle);
    connect(blueButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#0033FF")); updateDialDisplay(); });

    yellowButton = new QPushButton(this);
    yellowButton->setFixedSize(30, 30);
    QIcon yellowIcon(":/resources/icons/yellow.png");  // Path to your icon in resources
    yellowButton->setIcon(yellowIcon);
    yellowButton->setStyleSheet(buttonStyle);
    connect(yellowButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#FFEE00")); updateDialDisplay(); });


    greenButton = new QPushButton(this);
    greenButton->setFixedSize(30, 30);
    QIcon greenIcon(":/resources/icons/green.png");  // Path to your icon in resources
    greenButton->setIcon(greenIcon);
    greenButton->setStyleSheet(buttonStyle);
    connect(greenButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#33EE00")); updateDialDisplay(); });
    

    blackButton = new QPushButton(this);
    blackButton->setFixedSize(30, 30);
    QIcon blackIcon(":/resources/icons/black.png");  // Path to your icon in resources
    blackButton->setIcon(blackIcon);
    blackButton->setStyleSheet(buttonStyle);
    connect(blackButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#000000")); updateDialDisplay(); });

    whiteButton = new QPushButton(this);
    whiteButton->setFixedSize(30, 30);
    QIcon whiteIcon(":/resources/icons/white.png");  // Path to your icon in resources
    whiteButton->setIcon(whiteIcon);
    whiteButton->setStyleSheet(buttonStyle);
    connect(whiteButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#FFFFFF")); updateDialDisplay(); });
    
    customColorInput = new QLineEdit(this);
    customColorInput->setPlaceholderText("Custom HEX");
    customColorInput->setFixedSize(85, 30);
    connect(customColorInput, &QLineEdit::returnPressed, this, &MainWindow::applyCustomColor);

    
    thicknessButton = new QPushButton(this);
    thicknessButton->setIcon(loadThemedIcon("thickness"));
    thicknessButton->setFixedSize(30, 30);
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

    backgroundButton = new QPushButton(this);
    backgroundButton->setFixedSize(30, 30);
    QIcon bgIcon(loadThemedIcon("background"));  // Path to your icon in resources
    backgroundButton->setIcon(bgIcon);
    backgroundButton->setStyleSheet(buttonStyle);
    backgroundButton->setToolTip("Set Background Pic");
    connect(backgroundButton, &QPushButton::clicked, this, &MainWindow::selectBackground);

    
    
    deletePageButton = new QPushButton(this);
    deletePageButton->setFixedSize(30, 30);
    QIcon trashIcon(loadThemedIcon("trash"));  // Path to your icon in resources
    deletePageButton->setIcon(trashIcon);
    deletePageButton->setStyleSheet(buttonStyle);
    deletePageButton->setToolTip("Delete Current Page");
    connect(deletePageButton, &QPushButton::clicked, this, &MainWindow::deleteCurrentPage);

    zoomButton = new QPushButton(this);
    zoomButton->setIcon(loadThemedIcon("zoom"));
    zoomButton->setFixedSize(30, 30);
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
    zoomSlider->setRange(20, 250);
    zoomSlider->setValue(100);
    zoomSlider->setMaximumWidth(405);

    connect(zoomSlider, &QSlider::valueChanged, this, &MainWindow::updateZoom);

    QVBoxLayout *popupLayout = new QVBoxLayout();
    popupLayout->setContentsMargins(10, 5, 10, 5);
    popupLayout->addWidget(zoomSlider);
    zoomFrame->setLayout(popupLayout);
  

    zoom50Button = new QPushButton("0.5x", this);
    zoom50Button->setFixedSize(35, 30);
    zoom50Button->setStyleSheet(buttonStyle);
    zoom50Button->setToolTip("Set Zoom to 50%");
    connect(zoom50Button, &QPushButton::clicked, [this]() { zoomSlider->setValue(50); updateDialDisplay(); });

    dezoomButton = new QPushButton("1x", this);
    dezoomButton->setFixedSize(30, 30);
    dezoomButton->setStyleSheet(buttonStyle);
    dezoomButton->setToolTip("Set Zoom to 100%");
    connect(dezoomButton, &QPushButton::clicked, [this]() { zoomSlider->setValue(100); updateDialDisplay();});

    zoom200Button = new QPushButton("2x", this);
    zoom200Button->setFixedSize(31, 30);
    zoom200Button->setStyleSheet(buttonStyle);
    zoom200Button->setToolTip("Set Zoom to 200%");
    connect(zoom200Button, &QPushButton::clicked, [this]() { zoomSlider->setValue(200); updateDialDisplay();});

    panXSlider = new QSlider(Qt::Horizontal, this);
    panYSlider = new QSlider(Qt::Vertical, this);
    panYSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    connect(panXSlider, &QSlider::valueChanged, this, &MainWindow::updatePanX);
    
    connect(panYSlider, &QSlider::valueChanged, this, &MainWindow::updatePanY);




    // 🌟 Left Side: Tabs List
    tabList = new QListWidget(this);
    tabList->setFixedWidth(122);  // Adjust width as needed
    tabList->setSelectionMode(QAbstractItemView::SingleSelection);


    // 🌟 Add Button for New Tab
    addTabButton = new QPushButton(this);
    QIcon addTab(":/resources/icons/addtab.png");  // Path to your icon in resources
    addTabButton->setIcon(addTab);
    addTabButton->setFixedWidth(122);
    addTabButton->setFixedHeight(45);  // Adjust height as needed
    connect(addTabButton, &QPushButton::clicked, this, &MainWindow::addNewTab);

    if (!canvasStack) {
        canvasStack = new QStackedWidget(this);
    }

    connect(tabList, &QListWidget::currentRowChanged, this, &MainWindow::switchTab);

    sidebarContainer = new QWidget(this);  // <-- New container
    sidebarContainer->setObjectName("sidebarContainer");
    sidebarContainer->setContentsMargins(0, 0, 0, 0);  // <-- Remove margins
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

    


    pageInput = new QSpinBox(this);
    pageInput->setFixedSize(42, 30);
    pageInput->setMinimum(1);
    pageInput->setMaximum(9999);
    pageInput->setValue(1);
    pageInput->setMaximumWidth(100);
    connect(pageInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::switchPage);

    jumpToPageButton = new QPushButton(this);
    // QIcon jumpIcon(":/resources/icons/bookpage.png");  // Path to your icon in resources
    jumpToPageButton->setFixedSize(30, 30);
    jumpToPageButton->setStyleSheet(buttonStyle);
    jumpToPageButton->setIcon(loadThemedIcon("bookpage"));
    connect(jumpToPageButton, &QPushButton::clicked, this, &MainWindow::showJumpToPageDialog);

    // ✅ Dial Toggle Button
    dialToggleButton = new QPushButton(this);
    dialToggleButton->setIcon(loadThemedIcon("dial"));  // Icon for dial
    dialToggleButton->setFixedSize(30, 30);
    dialToggleButton->setToolTip("Toggle Magic Dial");
    dialToggleButton->setStyleSheet(buttonStyle);

    // ✅ Connect to toggle function
    connect(dialToggleButton, &QPushButton::clicked, this, &MainWindow::toggleDial);

    // toggleDial();

    

    fastForwardButton = new QPushButton(this);
    fastForwardButton->setFixedSize(30, 30);
    // QIcon ffIcon(":/resources/icons/fastforward.png");  // Path to your icon in resources
    fastForwardButton->setIcon(loadThemedIcon("fastforward"));
    fastForwardButton->setToolTip("Toggle Fast Forward 8x");
    fastForwardButton->setStyleSheet(buttonStyle);

    // ✅ Toggle fast-forward mode
    connect(fastForwardButton, &QPushButton::clicked, [this]() {
        fastForwardMode = !fastForwardMode;
    });

    QComboBox *dialModeSelector = new QComboBox(this);
    dialModeSelector->addItem("Page Switch", PageSwitching);
    dialModeSelector->addItem("Zoom", ZoomControl);
    dialModeSelector->addItem("Thickness", ThicknessControl);
    dialModeSelector->addItem("Color Adjust", ColorAdjustment);
    dialModeSelector->addItem("Tool Switch", ToolSwitching);
    dialModeSelector->setFixedWidth(120);

    connect(dialModeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) { changeDialMode(static_cast<DialMode>(index)); });

    channelSelector = new QComboBox(this);
    channelSelector->addItem("Red");
    channelSelector->addItem("Green");
    channelSelector->addItem("Blue");
    channelSelector->setFixedWidth(90);

    connect(channelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateSelectedChannel);

    colorPreview = new QPushButton(this);
    colorPreview->setFixedSize(30, 30);
    colorPreview->setStyleSheet("border-radius: 15px; border: 1px solid gray;");
    colorPreview->setEnabled(false);  // ✅ Prevents it from being clicked

    btnPageSwitch = new QPushButton(loadThemedIcon("bookpage"), "", this);
    btnPageSwitch->setStyleSheet(buttonStyle);
    btnPageSwitch->setToolTip("Set Dial Mode to Page Switching");
    btnZoom = new QPushButton(loadThemedIcon("zoom"), "", this);
    btnZoom->setStyleSheet(buttonStyle);
    btnZoom->setToolTip("Set Dial Mode to Zoom Ctrl");
    btnThickness = new QPushButton(loadThemedIcon("thickness"), "", this);
    btnThickness->setStyleSheet(buttonStyle);
    btnThickness->setToolTip("Set Dial Mode to Pen Tip Thickness Ctrl");
    btnColor = new QPushButton(loadThemedIcon("color"), "", this);
    btnColor->setStyleSheet(buttonStyle);
    btnColor->setToolTip("Set Dial Mode to Color Adjustment");
    btnTool = new QPushButton(loadThemedIcon("pen"), "", this);
    btnTool->setStyleSheet(buttonStyle);
    btnTool->setToolTip("Set Dial Mode to Tool Switching");
    btnPresets = new QPushButton(loadThemedIcon("preset"), "", this);
    btnPresets->setStyleSheet(buttonStyle);
    btnPresets->setToolTip("Set Dial Mode to Color Preset Selection");
    btnPannScroll = new QPushButton(loadThemedIcon("scroll"), "", this);
    btnPannScroll->setStyleSheet(buttonStyle);
    btnPannScroll->setToolTip("Slide and turn pages with the dial");

    connect(btnPageSwitch, &QPushButton::clicked, this, [this]() { changeDialMode(PageSwitching); });
    connect(btnZoom, &QPushButton::clicked, this, [this]() { changeDialMode(ZoomControl); });
    connect(btnThickness, &QPushButton::clicked, this, [this]() { changeDialMode(ThicknessControl); });
    connect(btnColor, &QPushButton::clicked, this, [this]() { changeDialMode(ColorAdjustment); });
    connect(btnTool, &QPushButton::clicked, this, [this]() { changeDialMode(ToolSwitching); });
    connect(btnPresets, &QPushButton::clicked, this, [this]() { changeDialMode(PresetSelection); }); 
    connect(btnPannScroll, &QPushButton::clicked, this, [this]() { changeDialMode(PanAndPageScroll); });


    // ✅ Ensure at least one preset exists (black placeholder)
    colorPresets.enqueue(QColor("#000000"));
    colorPresets.enqueue(QColor("#EE0000"));
    colorPresets.enqueue(QColor("#FFEE00"));
    colorPresets.enqueue(QColor("#0033FF"));
    colorPresets.enqueue(QColor("#33EE00"));
    colorPresets.enqueue(QColor("#FFFFFF"));

    // ✅ Button to add current color to presets
    addPresetButton = new QPushButton(loadThemedIcon("savepreset"), "", this);
    addPresetButton->setStyleSheet(buttonStyle);
    addPresetButton->setToolTip("Add Current Color to Presets");
    connect(addPresetButton, &QPushButton::clicked, this, &MainWindow::addColorPreset);


    openControlPanelButton = new QPushButton(this);
    openControlPanelButton->setIcon(loadThemedIcon("settings"));  // Replace with your actual settings icon
    openControlPanelButton->setStyleSheet(buttonStyle);
    openControlPanelButton->setToolTip("Open Control Panel");
    openControlPanelButton->setFixedSize(30, 30);  // Adjust to match your other buttons

    connect(openControlPanelButton, &QPushButton::clicked, this, [=]() {
        InkCanvas *canvas = currentCanvas();
        if (canvas) {
            ControlPanelDialog dialog(this, canvas, this);
            dialog.exec();  // Modal
        }
    });

    customColorButton = new QPushButton(this);
    customColorButton->setFixedSize(62, 30);
    customColorButton->setText("#000000");
    QColor initialColor = Qt::black;  // Default fallback color

    if (currentCanvas()) {
        initialColor = currentCanvas()->getPenColor();
    }

    customColorButton->setStyleSheet(QString("background-color: %1").arg(initialColor.name()));

    QTimer::singleShot(0, this, [=]() {
        connect(customColorButton, &QPushButton::clicked, this, [=]() {
            if (!currentCanvas()) return;
            QColor chosen = QColorDialog::getColor(currentCanvas()->getPenColor(), this, "Select Pen Color");
            if (chosen.isValid()) {
                currentCanvas()->setPenColor(chosen);
                customColorButton->setStyleSheet(QString("background-color: %1").arg(chosen.name()));
                customColorButton->setText(QString("%1").arg(chosen.name()).toUpper());
                updateDialDisplay();
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
    controlLayout->addWidget(redButton);
    controlLayout->addWidget(blueButton);
    controlLayout->addWidget(yellowButton);
    controlLayout->addWidget(greenButton);
    controlLayout->addWidget(blackButton);
    controlLayout->addWidget(whiteButton);
    controlLayout->addWidget(customColorButton);
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
    controlLayout->addWidget(btnColor);
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
    
    
    controlLayout->addWidget(pageInput);
    controlLayout->addWidget(benchmarkButton);
    controlLayout->addWidget(benchmarkLabel);
    controlLayout->addWidget(deletePageButton);
    
    
    

    QWidget *controlBar = new QWidget;
    controlBar->setObjectName("controlBar");
    controlBar->setLayout(controlLayout);
    controlBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QPalette palette = QGuiApplication::palette();
    QColor highlightColor = palette.highlight().color();  // System highlight color
    controlBar->setStyleSheet(QString(R"(
    QWidget#controlBar {
        background-color: %1;
        }
    )").arg(highlightColor.name()));

    
        

    canvasStack = new QStackedWidget();
    canvasStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *canvasLayout = new QVBoxLayout;
    canvasLayout->addWidget(panXSlider);
    canvasLayout->addWidget(canvasStack);

    QHBoxLayout *content_layout = new QHBoxLayout;
    content_layout->setContentsMargins(5, 0, 5, 5); 
    content_layout->addWidget(sidebarContainer); 
    content_layout->addWidget(panYSlider);
    content_layout->addLayout(canvasLayout);

    



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
        benchmarkLabel->setText("PR:N/A");
    }
}

void MainWindow::updateBenchmarkDisplay() {
    int sampleRate = currentCanvas()->getProcessedRate();
    benchmarkLabel->setText(QString("PR:%1 Hz").arg(sampleRate));
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
}

void MainWindow::selectFolder() {
    QString folder = QFileDialog::getExistingDirectory(this, "Select Save Folder");
    if (!folder.isEmpty()) {
        currentCanvas()->setSaveFolder(folder);
        switchPage(1);
        pageInput->setValue(1);

        updateTabLabel();
    }
}

void MainWindow::saveCanvas() {
    currentCanvas()->saveToFile(getCurrentPageForCanvas(currentCanvas()));
}


void MainWindow::switchPage(int pageNumber) {
    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;

    int newPage = pageNumber - 1;
    pageMap[canvas] = newPage;  // ✅ Save the page for this tab

    if (canvas->isPdfLoadedFunc() && pageNumber - 1 < canvas->getTotalPdfPages()) {
        canvas->loadPdfPage(newPage);
    } else {
        canvas->loadPage(newPage);
    }

    canvas->setLastActivePage(newPage);
    updateZoom();
    canvas->setLastPanX(panXSlider->maximum());
    canvas->setLastPanY(panYSlider->maximum());
    updateDialDisplay();
}

void MainWindow::deleteCurrentPage() {
    currentCanvas()->deletePage(getCurrentPageForCanvas(currentCanvas()));
}

void MainWindow::saveCurrentPage() {
    currentCanvas()->saveToFile(getCurrentPageForCanvas(currentCanvas()));
}

void MainWindow::selectBackground() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select Background Image", "", "Images (*.png *.jpg *.jpeg)");
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
        updateThickness(thicknessSlider->value());
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
    qreal dps = getDevicePixelRatio();
    int maxPanX = qMax(0, static_cast<int>(canvasSize.width() * zoom * dps / 100 - viewportSize.width()));
    int maxPanY = qMax(0, static_cast<int>(canvasSize.height() * zoom * dps / 100 - viewportSize.height()));
 

    int maxPanX_scaled = maxPanX * 110 / dps / zoom;
    int maxPanY_scaled = maxPanY * 110 / dps / zoom;  // Here I intentionally changed 100 to 110. 

    panXSlider->setRange(0, maxPanX_scaled);
    panYSlider->setRange(0, maxPanY_scaled);
}

void MainWindow::updatePanX(int value) {
    InkCanvas *canvas = currentCanvas();
    if (canvas) {
        canvas->setPanX(value);
        canvas->setLastPanX(value);  // ✅ Store panX per tab
    }
}

void MainWindow::updatePanY(int value) {
    InkCanvas *canvas = currentCanvas();
    if (canvas) {
        canvas->setPanY(value);
        canvas->setLastPanY(value);  // ✅ Store panY per tab
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
    QString filePath = QFileDialog::getOpenFileName(this, "Select PDF", "", "PDF Files (*.pdf)");
    if (!filePath.isEmpty()) {
        currentCanvas()->loadPdf(filePath);

        updateTabLabel(); // ✅ Update the tab name after assigning a PDF
    }
}

void MainWindow::clearPdf() {
    currentCanvas()->clearPdf();
}


void MainWindow::switchTab(int index) {
    if (!canvasStack || !tabList || !pageInput || !zoomSlider || !panXSlider || !panYSlider) {
        qDebug() << "Error: switchTab() called before UI was fully initialized!";
        return;
    }

    if (index >= 0 && index < canvasStack->count()) {
        canvasStack->setCurrentIndex(index);
        
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
    closeButton->setIcon(QIcon(":/resources/icons/cross.png")); // Set icon
    closeButton->setStyleSheet("QPushButton { border: none; background: transparent; }"); // Hide button border
    
    // ✅ Handle tab closing when the button is clicked
    connect(closeButton, &QPushButton::clicked, this, [=]() {

        // Prevent closing if it's the last remaining tab
        if (tabList->count() <= 1) {
            // Optional: show a message or do nothing silently
            QMessageBox::information(this, "Notice", "At least one tab must remain open.");

            return;
        }

        // Find the item associated with this button's parent (tabWidget)
        for (int i = 0; i < tabList->count(); ++i) {
            QListWidgetItem *item = tabList->item(i);
            QWidget *widget = tabList->itemWidget(item);
            if (widget == tabWidget) {
                removeTabAt(i);
                return;
            }
        }
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

    // ✅ Create new InkCanvas instance
    InkCanvas *newCanvas = new InkCanvas(this);
    canvasStack->addWidget(newCanvas);

    pageMap[newCanvas] = 0;

    // ✅ Select the new tab
    tabList->setCurrentItem(tabItem);
    canvasStack->setCurrentWidget(newCanvas);

    zoomSlider->setValue(100 / initialDpr); // Set initial zoom level based on DPR
    updateDialDisplay();

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/temp_session";
    newCanvas->setSaveFolder(tempDir);
    newCanvas->setBackgroundStyle(BackgroundStyle::Grid);
    newCanvas->setBackgroundColor(Qt::white);
    newCanvas->setBackgroundDensity(30);  // The default bg settings are here
    
}


void MainWindow::removeTabAt(int index) {
    if (!tabList || !canvasStack) return; // Ensure UI elements exist
    if (index < 0 || index >= canvasStack->count()) return;

    // ✅ Remove tab entry
    QListWidgetItem *item = tabList->takeItem(index);
    delete item;

    // ✅ Remove and delete the canvas safely
    QWidget *canvas = canvasStack->widget(index);
    if (canvas) {
        canvasStack->removeWidget(canvas);
        delete canvas;
    }

    // ✅ Select the previous tab (or first tab if none left)
    if (tabList->count() > 0) {
        int newIndex = qMax(0, index - 1);
        tabList->setCurrentRow(newIndex);
        canvasStack->setCurrentWidget(canvasStack->widget(newIndex));
    }
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
        switchPage(newPage);
        pageInput->setValue(newPage);
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
        pageDial->setStyleSheet("background:rgba(85, 3, 144, 0);");

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

        // ✅ Move dial to center of canvas
        dialContainer->move(width() / 2 + 100, height() / 2 - 200);

        dialDisplay = new QLabel(dialContainer);
        dialDisplay->setAlignment(Qt::AlignCenter);
        dialDisplay->setFixedSize(80, 80);
        dialDisplay->move(30, 30);  // Center position inside the dial
        

        int fontId = QFontDatabase::addApplicationFont(":/resources/fonts/Jersey20-Regular.ttf");
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

        // ✅ Connect to channel switching function
        connect(dialHiddenButton, &QPushButton::clicked, this, &MainWindow::cycleColorChannel);



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

}

void MainWindow::updateDialDisplay() {
    if (!dialDisplay) return;
    if (!dialColorPreview) return;
    if (!dialIconView) return;
    dialIconView->show();
    QColor currentColor = currentCanvas()->getPenColor();
    switch (currentDialMode) {
        case DialMode::PageSwitching:
            if (fastForwardMode){
                dialDisplay->setText(QString("\n\nPage\n%1").arg(getCurrentPageForCanvas(currentCanvas()) + 1 + tempClicks * 8));
            }
            else {
                dialDisplay->setText(QString("\n\nPage\n%1").arg(getCurrentPageForCanvas(currentCanvas()) + 1 + tempClicks));
            }
            dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/bookpage_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            break;
        case DialMode::ThicknessControl:
            dialDisplay->setText(QString("\n\nThickness\n%1").arg(currentCanvas()->getPenThickness()));
            dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/thickness_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            break;
        case DialMode::ZoomControl:
            dialDisplay->setText(QString("\n\nZoom\n%1%").arg(zoomSlider->value()));
            dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/zoom_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            break;
        case DialMode::ColorAdjustment:
            dialIconView->hide();
            switch (selectedChannel) {
                case 0:
                    dialDisplay->setText(QString("\n\nAdjust Red\n#%1").arg(currentCanvas()->getPenColor().name().remove("#")));
                    break;
                case 1:
                    dialDisplay->setText(QString("\n\nAdjust Green\n#%1").arg(currentCanvas()->getPenColor().name().remove("#")));
                    break;
                case 2:
                    dialDisplay->setText(QString("\n\nAdjust Blue\n#%1").arg(currentCanvas()->getPenColor().name().remove("#")));
                    break;
            }
            // dialDisplay->setText(QString("\n\nPen Color\n#%1").arg(currentCanvas()->getPenColor().name().remove("#")));
            dialColorPreview->setStyleSheet(QString("border-radius: 15px; border: 1px solid black; background-color: %1;").arg(currentColor.name()));
            break;  
        case DialMode::ToolSwitching:
            // ✅ Convert ToolType to QString for display
            switch (currentCanvas()->getCurrentTool()) {
                case ToolType::Pen:
                    dialDisplay->setText("\n\n\nPen");
                    dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/pen_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    break;
                case ToolType::Marker:
                    dialDisplay->setText("\n\n\nMarker");
                    dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/marker_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    break;
                case ToolType::Eraser:
                    dialDisplay->setText("\n\n\nEraser");
                    dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/eraser_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    break;
            }
            break;
        case PresetSelection:
            dialColorPreview->show();
            dialIconView->hide();
            dialColorPreview->setStyleSheet(QString("background-color: %1; border-radius: 15px; border: 1px solid black;")
                                            .arg(colorPresets[currentPresetIndex].name()));
            dialDisplay->setText(QString("\n\nPreset %1\n#%2")
                                            .arg(currentPresetIndex + 1)  // ✅ Display preset index (1-based)
                                            .arg(colorPresets[currentPresetIndex].name().remove("#"))); // ✅ Display hex color
            // dialIconView->setPixmap(QPixmap(":/resources/reversed_icons/preset_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            break;
        case DialMode::PanAndPageScroll:
            dialIconView->setPixmap(QPixmap(":/resources/icons/scroll_reversed.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            dialDisplay->setText(QString("\n\nPage\n%1").arg(getCurrentPageForCanvas(currentCanvas()) + 1));
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
        saveCurrentPage(); // autosave

        int currentPage = getCurrentPageForCanvas(currentCanvas()) + 1;
        int newPage = qBound(1, currentPage + totalClicks * pagesToAdvance, 99999);
        switchPage(newPage);
        pageInput->setValue(newPage);
        tempClicks = 0;
        updateDialDisplay(); 
        // currentCanvas()->setPanY(0);
        if (scrollOnTopEnabled) {
            panYSlider->setValue(0);
        }   // This line toggles whether the page is scrolled to the top after switching
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

        
        updateDialDisplay();  // ✅ Update dial display]
    }
}

void MainWindow::onToolReleased() {
    
}




bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    static bool dragging = false;
    static QPoint lastMousePos;
    static QTimer *longPressTimer = nullptr;

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

    dialHiddenButton->setEnabled(currentDialMode == ColorAdjustment);

    // ✅ Disconnect previous slots
    disconnect(pageDial, &QDial::valueChanged, nullptr, nullptr);
    disconnect(pageDial, &QDial::sliderReleased, nullptr, nullptr);

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
        case ColorAdjustment:
            connect(pageDial, &QDial::valueChanged, this, &MainWindow::handleDialColor);
            connect(pageDial, &QDial::sliderReleased, this, &MainWindow::onColorReleased);
            dialColorPreview->show();  // ✅ Ensure it's visible in Color mode
            dialDisplay->setStyleSheet("background-color: black; color: white; font-size: 14px; border-radius: 40px;");
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

    if (abs(delta) < 3) {  
        return;  
    }

    // ✅ Apply zoom dynamically (instead of waiting for release)
    int newZoom = qBound(20, zoomSlider->value() + (delta / 4), 250);  
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
        saveCurrentPage();

        int currentPage = getCurrentPageForCanvas(currentCanvas());
        int newPage = qBound(1, currentPage + pendingPageFlip + 1, 99999);
        switchPage(newPage);
        pageInput->setValue(newPage);
        updateDialDisplay();

        SDL_GameController *controller = controllerManager->getController();
        if (controller) {
            SDL_GameControllerRumble(controller, 0xA000, 0xF000, 25);  // Vibrate shortly
        }

        // Reset pan to top or bottom accordingly
        if (pendingPageFlip == +1) {
            panYSlider->setValue(0);  // New page → scroll top
        } else if (pendingPageFlip == -1) {
            panYSlider->setValue(panYSlider->maximum());  // Previous page → scroll bottom
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
        customColorButton->setText(selectedColor.name().toUpper());
        customColorButton->setStyleSheet(QString("background-color: %1").arg(selectedColor.name()));
        updateDialDisplay();
        
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


void MainWindow::handleDialColor(int angle) {
    if (!tracking) {
        startAngle = angle;
        accumulatedRotation = 0;
        tracking = true;
        lastAngle = angle;
        return;
    }

    int delta = angle - lastAngle;
    if (delta > 180) delta -= 360;
    if (delta < -180) delta += 360;

    accumulatedRotation += delta;

    if (abs(delta) < 5) {  // 🔹 If movement is too small, force an update
        return;
    }
    

    QColor color = currentCanvas()->getPenColor();
    int changeAmount = fastForwardMode ? 4 : 1;

    if (selectedChannel == 0) {  // Red
        color.setRed(qBound(0, color.red() + (delta / 5) * changeAmount, 255));
    } else if (selectedChannel == 1) {  // Green
        color.setGreen(qBound(0, color.green() + (delta / 5) * changeAmount, 255));
    } else if (selectedChannel == 2) {  // Blue
        color.setBlue(qBound(0, color.blue() + (delta / 5) * changeAmount, 255));
    }

    currentCanvas()->setPenColor(color);
    customColorButton->setText(color.name().toUpper());
    customColorButton->setStyleSheet(QString("background-color: %1").arg(color.name()));
    updateDialDisplay(); 

    colorPreview->setStyleSheet(QString("border-radius: 15px; border: 1px solid black; background-color: %1;").arg(color.name()));
    

    lastAngle = angle;
}

void MainWindow::onColorReleased() {
    accumulatedRotation = 0;
    tracking = false;
}

void MainWindow::updateSelectedChannel(int index) {
    selectedChannel = index;  // ✅ Store which channel to modify
}

void MainWindow::cycleColorChannel() {
    if (currentDialMode != ColorAdjustment) return; // ✅ Ensure it's only active in color mode

    selectedChannel = (selectedChannel + 1) % 3; // ✅ Cycle between 0 (Red), 1 (Green), 2 (Blue)
    channelSelector->setCurrentIndex(selectedChannel); // ✅ Update dropdown UI
    updateDialDisplay(); // ✅ Update dial display
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

QIcon MainWindow::loadThemedIcon(const QString& baseName) {
    QString path = isDarkMode()
        ? QString(":/resources/icons/%1_reversed.png").arg(baseName)
        : QString(":/resources/icons/%1.png").arg(baseName);
    return QIcon(path);
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

bool MainWindow::isScrollOnTopEnabled() const {
    return scrollOnTopEnabled;
}

void MainWindow::setScrollOnTopEnabled(bool enabled) {
    scrollOnTopEnabled = enabled;
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
    if (mode == "PageSwitching") return PageSwitching;
    if (mode == "ZoomControl") return ZoomControl;
    if (mode == "ThicknessControl") return ThicknessControl;
    if (mode == "ColorAdjustment") return ColorAdjustment;
    if (mode == "ToolSwitching") return ToolSwitching;
    if (mode == "PresetSelection") return PresetSelection;
    if (mode == "PanAndPageScroll") return PanAndPageScroll;
    return PanAndPageScroll;
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

    settings.beginGroup("ButtonHoldMappings");
    QStringList holdKeys = settings.allKeys();
    for (const QString &key : holdKeys) {
        buttonHoldMapping[key] = settings.value(key, "None").toString();
    }
    settings.endGroup();

    settings.beginGroup("ButtonPressMappings");
    QStringList pressKeys = settings.allKeys();
    for (const QString &key : pressKeys) {
        QString value = settings.value(key, "None").toString();
        buttonPressMapping[key] = value;

        // ✅ ADD THIS LINE
        buttonPressActionMapping[key] = stringToAction(value);
    }
    settings.endGroup();
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
        default:
            break;
    }
}


void MainWindow::importNotebookFromFile(const QString &packageFile) {

    QString destDir = QFileDialog::getExistingDirectory(this, "Select Working Directory for Notebook");

    if (destDir.isEmpty()) {
        QMessageBox::warning(this, "Import Cancelled", "No directory selected. Notebook will not be opened.");
        return;
    }

    InkCanvas *canvas = currentCanvas();
    if (!canvas) return;

    canvas->importNotebookTo(packageFile, destDir);

    // Change saveFolder in InkCanvas
    canvas->setSaveFolder(destDir);
    canvas->loadPage(0);
}
