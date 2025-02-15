
#include "MainWindow.h"
#include "InkCanvas.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScreen>
#include <QGuiApplication>
#include <QLineEdit>
#include "ToolType.h" // Include the header file where ToolType is defined
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QSpinBox>

MainWindow::MainWindow(QWidget *parent) 
    : QMainWindow(parent), benchmarking(false), currentPage(0) {

    setWindowTitle("SpeedyNote Alpha 0.1");

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
}


void MainWindow::setupUi() {

    loadPdfButton = new QPushButton("📋", this);
    clearPdfButton = new QPushButton("📋❌", this);
    loadPdfButton->setFixedSize(30, 30);
    clearPdfButton->setFixedSize(50, 30);
    connect(loadPdfButton, &QPushButton::clicked, this, &MainWindow::loadPdf);
    connect(clearPdfButton, &QPushButton::clicked, this, &MainWindow::clearPdf);

    benchmarkButton = new QPushButton("Start Benchmark", this);
    benchmarkButton->setFixedSize(100, 30); // Make the benchmark button smaller
    benchmarkLabel = new QLabel("Sample Rate: N/A", this);
    benchmarkLabel->setFixedHeight(20);  // Make the benchmark bar smaller

    selectFolderButton = new QPushButton("📁", this);
    selectFolderButton->setFixedSize(30, 30);
    connect(selectFolderButton, &QPushButton::clicked, this, &MainWindow::selectFolder);
    
    
    saveButton = new QPushButton("💾", this);
    saveButton->setFixedSize(30, 30);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveCurrentPage);
    
    saveAnnotatedButton = new QPushButton("💾𝓐", this);
    saveAnnotatedButton->setFixedSize(40, 30);
    connect(saveAnnotatedButton, &QPushButton::clicked, this, &MainWindow::saveAnnotated);

    redButton = new QPushButton("🔴", this);
    redButton->setFixedSize(30, 30);
    connect(redButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#EE0000")); });
    
    blueButton = new QPushButton("🔵", this);
    blueButton->setFixedSize(30, 30);
    connect(blueButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#66CCFF")); });

    yellowButton = new QPushButton("🟡", this);
    yellowButton->setFixedSize(30, 30);
    connect(yellowButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#FFEE00")); });

    blackButton = new QPushButton("⚫", this);
    blackButton->setFixedSize(30, 30);
    connect(blackButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#000000")); });

    whiteButton = new QPushButton("⚪", this);
    whiteButton->setFixedSize(30, 30);
    connect(whiteButton, &QPushButton::clicked, [this]() { currentCanvas()->setPenColor(QColor("#FFFFFF")); });
    
    customColorInput = new QLineEdit(this);
    customColorInput->setPlaceholderText("Custom HEX");
    customColorInput->setFixedSize(85, 30);
    connect(customColorInput, &QLineEdit::returnPressed, this, &MainWindow::applyCustomColor);

    thicknessSlider = new QSlider(Qt::Horizontal, this);
    thicknessSlider->setRange(1, 27);
    thicknessSlider->setValue(5);
    connect(thicknessSlider, &QSlider::valueChanged, this, &MainWindow::updateThickness);

    toolSelector = new QComboBox(this);
    toolSelector->addItem("🖋️");
    toolSelector->addItem("🖍️");
    toolSelector->addItem("◻️");
    toolSelector->setFixedWidth(60);
    connect(toolSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::changeTool);

    backgroundButton = new QPushButton("🖼️", this);
    backgroundButton->setFixedSize(30, 30);
    connect(backgroundButton, &QPushButton::clicked, this, &MainWindow::selectBackground);

    pageInput = new QSpinBox(this);
    pageInput->setMinimum(1);
    pageInput->setMaximum(9999);
    pageInput->setValue(1);
    connect(pageInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::switchPage);

    
    deletePageButton = new QPushButton("🗑️", this);
    deletePageButton->setFixedSize(30, 30);
    connect(deletePageButton, &QPushButton::clicked, this, &MainWindow::deleteCurrentPage);

    zoomSlider = new QSlider(Qt::Horizontal, this);
    zoomSlider->setRange(20, 400);
    zoomSlider->setValue(50);
    connect(zoomSlider, &QSlider::valueChanged, this, &MainWindow::updateZoom);

    zoom50Button = new QPushButton("0.5x", this);
    zoom50Button->setFixedSize(45, 30);
    connect(zoom50Button, &QPushButton::clicked, [this]() { zoomSlider->setValue(50); });

    dezoomButton = new QPushButton("1x", this);
    dezoomButton->setFixedSize(45, 30);
    connect(dezoomButton, &QPushButton::clicked, [this]() { zoomSlider->setValue(100); });

    zoom200Button = new QPushButton("2x", this);
    zoom200Button->setFixedSize(45, 30);
    connect(zoom200Button, &QPushButton::clicked, [this]() { zoomSlider->setValue(200); });

    panXSlider = new QSlider(Qt::Horizontal, this);
    panYSlider = new QSlider(Qt::Vertical, this);
    panYSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    connect(panXSlider, &QSlider::valueChanged, this, &MainWindow::updatePanX);
    
    connect(panYSlider, &QSlider::valueChanged, this, &MainWindow::updatePanY);




    // 🌟 Left Side: Tabs List
    tabList = new QListWidget(this);
    tabList->setFixedWidth(100);  // Adjust width as needed
    tabList->setSelectionMode(QAbstractItemView::SingleSelection);


    // 🌟 Add Button for New Tab
    addTabButton = new QPushButton("+ Add Tab");
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
    controlLayout->addWidget(saveButton);
    controlLayout->addWidget(saveAnnotatedButton);
    controlLayout->addWidget(redButton);
    controlLayout->addWidget(blueButton);
    controlLayout->addWidget(yellowButton);
    controlLayout->addWidget(blackButton);
    controlLayout->addWidget(whiteButton);
    controlLayout->addWidget(customColorInput);
    controlLayout->addWidget(thicknessSlider);
    controlLayout->addWidget(benchmarkButton);
    controlLayout->addWidget(benchmarkLabel);
    

    QHBoxLayout *controlLayout2 = new QHBoxLayout;

    controlLayout2->addWidget(toolSelector);
    controlLayout2->addWidget(backgroundButton);
    controlLayout2->addWidget(zoom50Button);
    controlLayout2->addWidget(dezoomButton);
    controlLayout2->addWidget(zoom200Button);
    controlLayout2->addWidget(zoomSlider);
    controlLayout2->addWidget(pageInput);
    controlLayout2->addWidget(deletePageButton);
    

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
    mainLayout->addLayout(controlLayout2);
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
        benchmarkButton->setText("Stop Benchmark");
    } else {
        currentCanvas()->stopBenchmark();
        benchmarkTimer->stop();
        benchmarkButton->setText("Start Benchmark");
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
    qreal thickness = 200.0 * std::pow(2.0, (value - 10) / 5.0) / currentCanvas()->getZoom(); // Exponential scaling
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
    currentCanvas()->saveToFile(currentPage);
}


void MainWindow::switchPage(int pageNumber) {
    currentPage = pageNumber - 1;
    if (currentCanvas()->isPdfLoadedFunc() && pageNumber < currentCanvas()->getTotalPdfPages()) {
        // If a PDF is loaded, load its corresponding page
        currentCanvas()->loadPdfPage(currentPage);
        currentCanvas()->loadPage(currentPage);
    } else {
        // Otherwise, load a regular saved page
        currentCanvas()->loadPage(currentPage);
    }
    currentCanvas()->setLastActivePage(currentPage);
    updateZoom(); // Update zoom and pan after page change
    currentCanvas()->setLastPanX(panXSlider->maximum()); 
    currentCanvas()->setLastPanY(panYSlider->maximum());
}

void MainWindow::deleteCurrentPage() {
    currentCanvas()->deletePage(currentPage);
}

void MainWindow::saveCurrentPage() {
    currentCanvas()->saveToFile(currentPage);
}

void MainWindow::selectBackground() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select Background Image", "", "Images (*.png *.jpg *.jpeg)");
    if (!filePath.isEmpty()) {
        currentCanvas()->setBackground(filePath, currentPage);
    }
}

void MainWindow::saveAnnotated() {
    currentCanvas()->saveAnnotated(currentPage);
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
            pageInput->blockSignals(true);  // ✅ Prevent triggering unwanted switchPage calls
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
    tabLabel->setFixedWidth(95); // ✅ Adjust width for better readability
    tabLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // ✅ Create the close button (❌)
    QPushButton *closeButton = new QPushButton("❌", tabWidget);
    closeButton->setFixedSize(15, 15); // Adjust button size
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
    tabItem->setSizeHint(QSize(95, 60)); // ✅ Adjust height only, keep default style
    tabList->addItem(tabItem);
    tabList->setItemWidget(tabItem, tabWidget);  // Attach tab layout

    // ✅ Create new InkCanvas instance
    InkCanvas *newCanvas = new InkCanvas(this);
    canvasStack->addWidget(newCanvas);

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
