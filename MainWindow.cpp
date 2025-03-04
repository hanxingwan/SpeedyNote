
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

MainWindow::MainWindow(QWidget *parent) 
    : QMainWindow(parent), benchmarking(false) {

    setWindowTitle("SpeedyNote Alpha 0.1.5 Legacy");

    QString iconPath = QCoreApplication::applicationDirPath() + "/icon.ico"; 
    setWindowIcon(QIcon(iconPath));
    

    // ✅ Get screen size & adjust window size
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QSize nativeSize = screen->availableGeometry().size();
        resize(nativeSize);
    }
    // ✅ Create a stacked widget to hold multiple canvases
    canvasStack = new QStackedWidget(this);
    setCentralWidget(canvasStack);

    // ✅ Create the first tab (default canvas)
    addNewTab();
    setupUi();    // ✅ Move all UI setup here
    updateZoom(); // ✅ Keep this for initial zoom adjustment
    updatePanRange(); // Set initial slider range  HERE IS THE PROBLEM!!

    qApp->setStyleSheet(R"(
        QMainWindow {
            background: qlineargradient(x1: 0, x2: 1, stop: 0 #eff4ff, stop: 1 #cdd9f2);
        }

        QPushButton {
            background: qlineargradient(x1: 0, x2: 1, stop: 0 #d0e0f7, stop: 1 #aac1e0);
            border: 1px solid #6b90c5;
            border-radius: 6px;
            padding: 6px 12px;
        }

        QPushButton:hover {
            background: qlineargradient(x1: 0, x2: 1, stop: 0 #b8d0f0, stop: 1 #88a8d0);
        }

        QComboBox {
            background: qlineargradient(y1: 0, y2: 1, stop: 0 #e1ecf6, stop: 1 #aac1e0);
            border: 2px solid #6b90c5;
            border-radius: 6px;
            padding: 5px;
            font-size: 14px;
            color: #1e1e1e;
        }
    
        QComboBox:hover {
            background: qlineargradient(y1: 0, y2: 1, stop: 0 #d6e7f6, stop: 1 #8fb5df);
        }
        
        QComboBox::drop-down {
            border: none;
            width: 25px;
        }

        QComboBox::down-arrow {
            image: url(:resources/icons/down_arrow.ico); /* Replace with your own */
            width: 16px;
            height: 16px;
        }

        QComboBox::down-arrow:on {
            top: 1px;
        }

        QComboBox QAbstractItemView {
            background: white;
            border: 1px solid #6b90c5;
            selection-background-color: #8fb5df;
            padding: 4px;
        }


        /* ========== QSpinBox (FIXED UP/DOWN ARROWS) ========== */

        QSpinBox {
            background: qlineargradient(y1: 0, y2: 1, stop: 0 #e1ecf6, stop: 1 #aac1e0);
            border: 2px solid #6b90c5;
            border-radius: 6px;
            padding: 3px;
            font-size: 20px;
            color: #1e1e1e;
            min-width: 60px;
            min-height: 50px;
        }

        QSpinBox::up-button, QSpinBox::down-button {
            width: 20px;
            border: none;
            background: none;
        }

        QSpinBox::up-arrow {
            image: url(:/resources/icons/up_arrow.ico);
            width: 14px;
            height: 14px;
        }

        QSpinBox::down-arrow {
            image: url(:/resources/icons/down_arrow.ico);
            width: 14px;
            height: 14px;
        }

        QSpinBox::up-arrow:pressed, QSpinBox::down-arrow:pressed {
            top: 1px;
        }


        
        /* ========== QListWidget (Side Tabs) ========== */
        QListWidget {
            background: qlineargradient(y1: 0, y2: 1, stop: 0 #ddebf6, stop: 1 #aac1e0);
            border: 2px solid #6b90c5;
            border-radius: 6px;
            padding: 5px;
            font-size: 14px;
            color: #1e1e1e;
        }

        QListWidget::item {
            padding: 8px;
            border-bottom: 1px solid #8fb5df;
            background: qlineargradient(y1: 0, y2: 1, stop: 0 #e1ecf6, stop: 1 #aac1e0);
            border-radius: 4px;
        }

        QListWidget::item:selected {
            background: qlineargradient(y1: 0, y2: 1, stop: 0 #8fb5df, stop: 1 #6b90c5);
            color: white;
        }

        QListWidget::item:hover {
            background: qlineargradient(y1: 0, y2: 1, stop: 0 #d6e7f6, stop: 1 #8fb5df);
        }

        /* ========== QLineEdit ========== */
        QLineEdit {
            background: qlineargradient(y1: 0, y2: 1, stop: 0 #ffffff, stop: 1 #ddebf6);
            border: 2px solid #6b90c5;
            border-radius: 6px;
            padding: 5px;
            font-size: 20px;
            color: #1e1e1e;
            selection-background-color: #8fb5df;
        }

        QLineEdit:hover {
            border: 2px solid #507dbc;
        }

        QLineEdit:focus {
            border: 2px solid #0046ad;
            background: qlineargradient(y1: 0, y2: 1, stop: 0 #ffffff, stop: 1 #c2dbf2);
        }



    )");
}


void MainWindow::setupUi() {
    


    QString sliderStyle = R"(
        QSlider::groove:horizontal {
            border: 1px solid #666;
            height: 7px;
            background: qlineargradient(x1: 0, x2: 1, stop: 0 #d8e2f2, stop: 1 #aac1e0);
            border-radius: 3px;
        }
    
        QSlider::handle:horizontal {
            background: qradialgradient(cx: 0.5, cy: 0.5, radius: 0.5,
                                        fx: 0.5, fy: 0.5, stop: 0 #ffffff, stop: 1 #96b8e7);
            width: 35px;
            height: 14px;
            margin: -5px 0;
            border: 1px solid #666;
            border-radius: 4px;
            
        }
    
        QSlider::add-page:horizontal {
            background: qlineargradient(x1: 0, x2: 1, stop: 0 #aac1e0, stop: 1 #6b90c5);
            border-radius: 4px;
        }
    
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1: 0, x2: 1, stop: 0 #6b90c5, stop: 1 #aac1e0);
            border-radius: 4px;
        }

        QSlider::groove:vertical {
            border: 1px solid #666;
            width: 7px;
            background: qlineargradient(x1: 0, x2: 1, stop: 0 #d8e2f2, stop: 1 #aac1e0);
            border-radius: 3px;
        }

        QSlider::handle:vertical {
            background: qradialgradient(cx: 0.5, cy: 0.5, radius: 0.5,
                                        fx: 0.5, fy: 0.5, stop: 0 #ffffff, stop: 1 #96b8e7);
            width: 14px;
            height: 35px;
            margin: 0 -5px;
            border-radius: 5px;
            border: 1px solid #666;
        }    
    )";

    QString buttonStyle = R"(
        QPushButton {
            padding: 0px;
            border: none;
        }
    )";
    


    loadPdfButton = new QPushButton(this);
    clearPdfButton = new QPushButton(this);
    loadPdfButton->setFixedSize(60, 60);
    clearPdfButton->setFixedSize(60, 60);
    QIcon pdfIcon(":/resources/icons/pdf.ico");  // Path to your icon in resources
    QIcon pdfDeleteIcon(":/resources/icons/pdfdelete.ico");  // Path to your icon in resources
    loadPdfButton->setIcon(pdfIcon);
    clearPdfButton->setIcon(pdfDeleteIcon);
    connect(loadPdfButton, &QPushButton::clicked, this, &MainWindow::loadPdf);
    connect(clearPdfButton, &QPushButton::clicked, this, &MainWindow::clearPdf);

    benchmarkButton = new QPushButton(this);
    QIcon benchmarkIcon(":/resources/icons/benchmark.ico");  // Path to your icon in resources
    benchmarkButton->setIcon(benchmarkIcon);
    benchmarkButton->setFixedSize(60, 60); // Make the benchmark button smaller
    benchmarkLabel = new QLabel("Sample Rate: N/A", this);
    benchmarkLabel->setFixedHeight(40);  // Make the benchmark bar smaller

    selectFolderButton = new QPushButton(this);
    selectFolderButton->setFixedSize(60, 60);
    QIcon folderIcon(":/resources/icons/folder.ico");  // Path to your icon in resources
    selectFolderButton->setIcon(folderIcon);
    connect(selectFolderButton, &QPushButton::clicked, this, &MainWindow::selectFolder);
    
    
    saveButton = new QPushButton(this);
    saveButton->setFixedSize(60, 60);
    QIcon saveIcon(":/resources/icons/save.ico");  // Path to your icon in resources
    saveButton->setIcon(saveIcon);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveCurrentPage);
    
    saveAnnotatedButton = new QPushButton(this);
    saveAnnotatedButton->setFixedSize(60, 60);
    QIcon saveAnnotatedIcon(":/resources/icons/saveannotated.ico");  // Path to your icon in resources
    saveAnnotatedButton->setIcon(saveAnnotatedIcon);
    connect(saveAnnotatedButton, &QPushButton::clicked, this, &MainWindow::saveAnnotated);

    redButton = new QPushButton(this);
    redButton->setFixedSize(60, 60);
    QIcon redIcon(":/resources/icons/red.ico");  // Path to your icon in resources
    redButton->setIcon(redIcon);
    connect(redButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#EE0000")); });
    
    blueButton = new QPushButton(this);
    blueButton->setFixedSize(60, 60);
    QIcon blueIcon(":/resources/icons/blue.ico");  // Path to your icon in resources
    blueButton->setIcon(blueIcon);
    connect(blueButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#1122FF")); });

    yellowButton = new QPushButton(this);
    yellowButton->setFixedSize(60, 60);
    QIcon yellowIcon(":/resources/icons/yellow.ico");  // Path to your icon in resources
    yellowButton->setIcon(yellowIcon);
    connect(yellowButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#EEDD00")); });


    greenButton = new QPushButton(this);
    greenButton->setFixedSize(60, 60);
    QIcon greenIcon(":/resources/icons/green.ico");  // Path to your icon in resources
    greenButton->setIcon(greenIcon);
    connect(greenButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#00CC00")); });
    

    blackButton = new QPushButton(this);
    blackButton->setFixedSize(60, 60);
    QIcon blackIcon(":/resources/icons/black.ico");  // Path to your icon in resources
    blackButton->setIcon(blackIcon);
    connect(blackButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#000000")); });

    whiteButton = new QPushButton(this);
    whiteButton->setFixedSize(60, 60);
    QIcon whiteIcon(":/resources/icons/white.ico");  // Path to your icon in resources
    whiteButton->setIcon(whiteIcon);
    connect(whiteButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#FFFFFF")); });
    
    customColorInput = new QLineEdit(this);
    customColorInput->setPlaceholderText("Custom HEX");
    customColorInput->setFixedSize(140, 60);
    connect(customColorInput, &QLineEdit::returnPressed, this, &MainWindow::applyCustomColor);

    thicknessButton = new QPushButton(this);
    thicknessButton->setIcon(QIcon(":/resources/icons/thickness.ico"));
    thicknessButton->setFixedSize(60, 60);
    connect(thicknessButton, &QPushButton::clicked, this, &MainWindow::toggleThicknessSlider);

    thicknessFrame = new QFrame(this);
    thicknessFrame->setFrameShape(QFrame::StyledPanel);
    thicknessFrame->setStyleSheet(R"(
        background-color: white;
        border: 1px solid white;
        padding: 5px;
    )");
    thicknessFrame->setVisible(false);
    thicknessFrame->setFixedSize(600, 80); // Adjust width/height as needed

    thicknessSlider = new QSlider(Qt::Horizontal, this);
    thicknessSlider->setRange(1, 27);
    thicknessSlider->setValue(5);
    thicknessSlider->setMaximumWidth(530);
    thicknessSlider->setStyleSheet(sliderStyle);

    connect(thicknessSlider, &QSlider::valueChanged, this, &MainWindow::updateThickness);

    QVBoxLayout *popupLayoutThickness = new QVBoxLayout();
    popupLayoutThickness->setContentsMargins(10, 5, 10, 5);
    popupLayoutThickness->addWidget(thicknessSlider);
    thicknessFrame->setLayout(popupLayoutThickness);


    toolSelector = new QComboBox(this);
    toolSelector->addItem(QIcon(":/resources/icons/pen.ico"), "");
    toolSelector->addItem(QIcon(":/resources/icons/marker.ico"), "");
    toolSelector->addItem(QIcon(":/resources/icons/eraser.ico"), "");
    toolSelector->setFixedWidth(100);
    toolSelector->setFixedHeight(60);
    connect(toolSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::changeTool);

    backgroundButton = new QPushButton(this);
    backgroundButton->setFixedSize(60, 60);
    QIcon bgIcon(":/resources/icons/background.ico");  // Path to your icon in resources
    backgroundButton->setIcon(bgIcon);
    connect(backgroundButton, &QPushButton::clicked, this, &MainWindow::selectBackground);

    pageInput = new QSpinBox(this);
    pageInput->setMinimum(1);
    pageInput->setMaximum(9999);
    pageInput->setValue(1);
    pageInput->setMaximumWidth(120);
    connect(pageInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::switchPage);

    
    deletePageButton = new QPushButton(this);
    deletePageButton->setFixedSize(60, 60);
    QIcon trashIcon(":/resources/icons/trash.ico");  // Path to your icon in resources
    deletePageButton->setIcon(trashIcon);
    connect(deletePageButton, &QPushButton::clicked, this, &MainWindow::deleteCurrentPage);

    zoomButton = new QPushButton(this);
    zoomButton->setIcon(QIcon(":/resources/icons/zoom.ico"));
    zoomButton->setFixedSize(60, 60);
    connect(zoomButton, &QPushButton::clicked, this, &MainWindow::toggleZoomSlider);

    // ✅ Create the floating frame (Initially Hidden)
    zoomFrame = new QFrame(this);
    zoomFrame->setFrameShape(QFrame::StyledPanel);
    zoomFrame->setStyleSheet(R"(
        background-color: white;
        border: 1px solid white;
        padding: 5px;
    )");
    zoomFrame->setVisible(false);
    zoomFrame->setFixedSize(750, 80); // Adjust width/height as needed

    zoomSlider = new QSlider(Qt::Horizontal, this);
    zoomSlider->setRange(20, 250);
    zoomSlider->setValue(100);
    zoomSlider->setMaximumWidth(700);
    zoomSlider->setStyleSheet(sliderStyle);

    connect(zoomSlider, &QSlider::valueChanged, this, &MainWindow::updateZoom);

    QVBoxLayout *popupLayout = new QVBoxLayout();
    popupLayout->setContentsMargins(10, 5, 10, 5);
    popupLayout->addWidget(zoomSlider);
    zoomFrame->setLayout(popupLayout);
  

    zoom50Button = new QPushButton("0.5x", this);
    zoom50Button->setFixedSize(80, 60);
    connect(zoom50Button, &QPushButton::clicked, [this]() { zoomSlider->setValue(50); });

    dezoomButton = new QPushButton("1x", this);
    dezoomButton->setFixedSize(80, 60);
    connect(dezoomButton, &QPushButton::clicked, [this]() { zoomSlider->setValue(100); });

    zoom200Button = new QPushButton("2x", this);
    zoom200Button->setFixedSize(80, 60);
    connect(zoom200Button, &QPushButton::clicked, [this]() { zoomSlider->setValue(200); });

    panXSlider = new QSlider(Qt::Horizontal, this);
    panYSlider = new QSlider(Qt::Vertical, this);
    panYSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    panXSlider->setStyleSheet(sliderStyle);
    panYSlider->setStyleSheet(sliderStyle);

    connect(panXSlider, &QSlider::valueChanged, this, &MainWindow::updatePanX);
    
    connect(panYSlider, &QSlider::valueChanged, this, &MainWindow::updatePanY);




    // 🌟 Left Side: Tabs List
    tabList = new QListWidget(this);
    tabList->setFixedWidth(210);  // Adjust width as needed
    tabList->setSelectionMode(QAbstractItemView::SingleSelection);


    // 🌟 Add Button for New Tab
    addTabButton = new QPushButton(this);
    QIcon addTab(":/resources/icons/addtab.ico");  // Path to your icon in resources
    addTabButton->setIcon(addTab);
    connect(addTabButton, &QPushButton::clicked, this, &MainWindow::addNewTab);

    if (!canvasStack) {
        canvasStack = new QStackedWidget(this);
    }

    connect(tabList, &QListWidget::currentRowChanged, this, &MainWindow::switchTab);


    QVBoxLayout *tabLayout = new QVBoxLayout();
    tabLayout->addWidget(tabList);
    tabLayout->addWidget(addTabButton);



    QHBoxLayout *controlLayout = new QHBoxLayout;
    controlLayout->addWidget(selectFolderButton);
    controlLayout->addWidget(loadPdfButton);
    controlLayout->addWidget(clearPdfButton);
    controlLayout->addWidget(backgroundButton);
    controlLayout->addWidget(saveButton);
    controlLayout->addWidget(saveAnnotatedButton);
    controlLayout->addWidget(redButton);
    controlLayout->addWidget(blueButton);
    controlLayout->addWidget(yellowButton);
    controlLayout->addWidget(greenButton);
    controlLayout->addWidget(blackButton);
    controlLayout->addWidget(whiteButton);
    controlLayout->addWidget(customColorInput);
    controlLayout->addWidget(thicknessButton);
    // controlLayout->addStretch();
    
    controlLayout->addWidget(toolSelector);
    controlLayout->addWidget(zoomButton);
    controlLayout->addWidget(zoom50Button);
    controlLayout->addWidget(dezoomButton);
    controlLayout->addWidget(zoom200Button);
    controlLayout->addStretch();
    controlLayout->addWidget(pageInput);
    controlLayout->addWidget(benchmarkButton);
    controlLayout->addWidget(benchmarkLabel);
    controlLayout->addWidget(deletePageButton);
    
    
    

    canvasStack = new QStackedWidget();

    QVBoxLayout *canvasLayout = new QVBoxLayout;
    canvasLayout->addWidget(panXSlider);
    canvasLayout->addWidget(canvasStack);


    QHBoxLayout *content_layout = new QHBoxLayout;
    content_layout->addLayout(tabLayout); 
    content_layout->addWidget(panYSlider);
    content_layout->addLayout(canvasLayout);



    QWidget *container = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->addLayout(controlLayout);
    mainLayout->addLayout(content_layout);

    setCentralWidget(container);

    benchmarkTimer = new QTimer(this);
    connect(benchmarkButton, &QPushButton::clicked, this, &MainWindow::toggleBenchmark);
    connect(benchmarkTimer, &QTimer::timeout, this, &MainWindow::updateBenchmarkDisplay);

    addNewTab();

}

MainWindow::~MainWindow() {
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
        benchmarkLabel->setText("Processed Rate: N/A");
    }
}

void MainWindow::updateBenchmarkDisplay() {
    int sampleRate = currentCanvas()->getProcessedRate();
    benchmarkLabel->setText(QString("Processed Rate: %1 Hz").arg(sampleRate));
}

void MainWindow::applyCustomColor() {
    QString colorCode = customColorInput->text();
    if (!colorCode.startsWith("#")) {
        colorCode.prepend("#");
    }
    currentCanvas()->setPenColor(QColor(colorCode));
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

    if (canvas->isPdfLoadedFunc() && pageNumber < canvas->getTotalPdfPages()) {
        canvas->loadPdfPage(newPage);
        canvas->loadPage(newPage);
    } else {
        canvas->loadPage(newPage);
    }

    canvas->setLastActivePage(newPage);
    updateZoom();
    canvas->setLastPanX(panXSlider->maximum());
    canvas->setLastPanY(panYSlider->maximum());
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
    }
}

qreal getDevicePixelRatio(){
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
 

    int maxPanX_scaled = maxPanX * 100 / dps / zoom;
    int maxPanY_scaled = maxPanY * 100 / dps / zoom;

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
    if (index >= 0 && index < canvasStack->count()) {
        canvasStack->setCurrentIndex(index);
        
        // ✅ Get current canvas and its last active page
        InkCanvas *canvas = currentCanvas();
        if (canvas) {
            int savedPage = canvas->getLastActivePage();

            

            pageInput->blockSignals(true);  // Prevent triggering unwanted switchPage calls
            pageInput->setValue(savedPage + 1);
            pageInput->blockSignals(false);

            // ✅ Restore zoom
            zoomSlider->blockSignals(true);
            zoomSlider->setValue(canvas->getLastZoomLevel());
            zoomSlider->blockSignals(false);
            canvas->setZoom(canvas->getLastZoomLevel());

            // ✅ Restore pan
            panXSlider->blockSignals(true);
            panYSlider->blockSignals(true);
            panXSlider->setValue(canvas->getLastPanX());
            panYSlider->setValue(canvas->getLastPanY());
            panXSlider->blockSignals(false);
            panYSlider->blockSignals(false);
            updatePanRange();  // ✅ Update pan range after switching tabs

            if (panXSlider->maximum() > 0) {
                panXSlider->setValue(canvas->getLastPanX());
            }
            if (panYSlider->maximum() > 0) {
                panYSlider->setValue(canvas->getLastPanY());
            }
        }
    }
}

void MainWindow::addNewTab() {
    if (!tabList || !canvasStack) return;  // Ensure tabList and canvasStack exist

    int newTabIndex = tabList->count();  // New tab index
    QWidget *tabWidget = new QWidget();  // Custom tab container
    QHBoxLayout *tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(5, 2, 5, 2);

    // ✅ Create the label (Tab Name)
    QLabel *tabLabel = new QLabel(QString("Tab %1").arg(newTabIndex + 1), tabWidget);    
    tabLabel->setObjectName("tabLabel"); // ✅ Name the label for easy retrieval later
    tabLabel->setWordWrap(true); // ✅ Allow text to wrap
    tabLabel->setFixedWidth(170); // ✅ Adjust width for better readability
    tabLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // ✅ Create the close button (❌)
    QPushButton *closeButton = new QPushButton(tabWidget);
    closeButton->setFixedSize(30, 30); // Adjust button size
    closeButton->setIcon(QIcon(":/resources/icons/cross.ico")); // Set icon
    closeButton->setStyleSheet("QPushButton { border: none; background: transparent; }"); // Hide button border
    
    // ✅ Handle tab closing when the button is clicked
    connect(closeButton, &QPushButton::clicked, this, [=]() {
        removeTabAt(newTabIndex);
    });

    // ✅ Add widgets to the tab layout
    tabLayout->addWidget(tabLabel);
    tabLayout->addWidget(closeButton);
    tabLayout->setStretch(0, 1);
    tabLayout->setStretch(1, 0);
    
    // ✅ Create the tab item and set widget
    QListWidgetItem *tabItem = new QListWidgetItem();
    tabItem->setSizeHint(QSize(155, 90)); // ✅ Adjust height only, keep default style
    tabList->addItem(tabItem);
    tabList->setItemWidget(tabItem, tabWidget);  // Attach tab layout

    // ✅ Create new InkCanvas instance
    InkCanvas *newCanvas = new InkCanvas(this);
    canvasStack->addWidget(newCanvas);

    pageMap[newCanvas] = 0;

    // ✅ Select the new tab
    tabList->setCurrentItem(tabItem);
    canvasStack->setCurrentWidget(newCanvas);
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