#include "InkCanvas.h"
#include "ToolType.h"
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QColor>
#include <cmath>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
// #include <QInputDevice>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QImageReader>
#include <QCache>
#include "MainWindow.h"
#include <QInputDevice>
#include <QTabletEvent>

#include <poppler-qt6.h>




InkCanvas::InkCanvas(QWidget *parent) 
    : QWidget(parent), drawing(false), penColor(Qt::black), penThickness(5.0), zoomFactor(100), panOffsetX(0), panOffsetY(0), currentTool(ToolType::Pen) {    
    setAttribute(Qt::WA_StaticContents);
    setTabletTracking(true);
    
    // Detect screen resolution and set canvas size
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QSize logicalSize = screen->availableGeometry().size() * 0.9;
        setFixedSize(logicalSize);
    } else {
        setFixedSize(1920, 1080); // Fallback size
    }
    
    initializeBuffer();
    QCache<int, QPixmap> pdfCache(10);
    pdfCache.setMaxCost(10);  // ✅ Ensures the cache holds at most 5 pages
    // No need to set auto-delete, QCache will handle deletion automatically
}

InkCanvas::~InkCanvas() {
    // Cleanup if needed
}



void InkCanvas::initializeBuffer() {
    QScreen *screen = QGuiApplication::primaryScreen();
    qreal dpr = screen ? screen->devicePixelRatio() : 1.0;

    // Get logical screen size
    QSize logicalSize = screen ? screen->size() : QSize(1440, 900);
    QSize pixelSize = logicalSize * dpr;

    buffer = QPixmap(pixelSize);
    buffer.fill(Qt::transparent);

    setMaximumSize(pixelSize); // 🔥 KEY LINE to make full canvas drawable
}

void InkCanvas::loadPdf(const QString &pdfPath) {
    pdfDocument = Poppler::Document::load(pdfPath);
    if (pdfDocument && !pdfDocument->isLocked()) {
        totalPdfPages = pdfDocument->numPages();
        isPdfLoaded = true;
        totalPdfPages = pdfDocument->numPages();
        loadPdfPage(0); // Load first page
        // ✅ Save the PDF path in the metadata file
        if (!saveFolder.isEmpty()) {
            QString metadataFile = saveFolder + "/.pdf_path.txt";
            QFile file(metadataFile);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << pdfPath;  // Store the absolute path of the PDF
                file.close();
            }
        }
    }
}

void InkCanvas::clearPdf() {
    pdfDocument.reset();
    pdfDocument = nullptr;
    isPdfLoaded = false;
    totalPdfPages = 0;
    pdfCache.clear();

    // ✅ Remove the PDF path file when clearing the PDF
    if (!saveFolder.isEmpty()) {
        QString metadataFile = saveFolder + "/.pdf_path.txt";
        QFile::remove(metadataFile);
    }
}

void InkCanvas::clearPdfNoDelete() {
    pdfDocument.reset();
    pdfDocument = nullptr;
    isPdfLoaded = false;
    totalPdfPages = 0;
    pdfCache.clear();
}

void InkCanvas::loadPdfPage(int pageNumber) {
    if (!pdfDocument) return;

    if (pdfCache.contains(pageNumber)) {
        backgroundImage = *pdfCache.object(pageNumber);
        loadPage(pageNumber);  // Load annotations
        update();
        return;
    }

    // ✅ Ensure the cache holds only 10 pages max
    if (pdfCache.count() >= 10) {
        auto oldestKey = pdfCache.keys().first();
        pdfCache.remove(oldestKey);
    }

    // ✅ Render the page and store it in the cache
    if (pageNumber >= 0 && pageNumber < pdfDocument->numPages()) {
        std::unique_ptr<Poppler::Page> page(pdfDocument->page(pageNumber));
        if (page) {
            QImage pdfImage = page->renderToImage(288, 288);
            if (!pdfImage.isNull()) {
                QPixmap cachedPixmap = QPixmap::fromImage(pdfImage);
                pdfCache.insert(pageNumber, new QPixmap(cachedPixmap));

                backgroundImage = cachedPixmap;
            }
        }
    } else {
        backgroundImage = QPixmap();  // Clear background when out of range
    }
    loadPage(pageNumber);  // Load existing canvas annotations
    update();
}


void InkCanvas::loadPdfPreview(int pageNumber) {
    if (!pdfDocument) return;

    if (pageNumber >= 0 && pageNumber < pdfDocument->numPages()) {
        std::unique_ptr<Poppler::Page> page(pdfDocument->page(pageNumber));
        if (page) {
            QImage pdfImage = page->renderToImage(72, 72);  // ✅ Render at low DPI
            
            if (!pdfImage.isNull()) {
                // ✅ Scale the preview to match the final page size (4x upscale)
                QImage upscaledPreview = pdfImage.scaled(pdfImage.width() * 4, pdfImage.height() * 4, 
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
                backgroundImage = QPixmap::fromImage(upscaledPreview);
                update();
            }
        }
    }
}

void InkCanvas::startBenchmark() {
    benchmarking = true;
    processedTimestamps.clear();
    benchmarkTimer.start();
}

void InkCanvas::stopBenchmark() {
    benchmarking = false;
}

int InkCanvas::getProcessedRate() {
    qint64 now = benchmarkTimer.elapsed();
    while (!processedTimestamps.empty() && now - processedTimestamps.front() > 1000) {
        processedTimestamps.pop_front();
    }
    return static_cast<int>(processedTimestamps.size());
}


void InkCanvas::resizeEvent(QResizeEvent *event) {
    // Only resize the buffer if the new size is larger
    if (event->size().width() > buffer.width() || event->size().height() > buffer.height()) {
        QPixmap newBuffer(event->size());
        newBuffer.fill(Qt::transparent);

        QPainter painter(&newBuffer);
        painter.drawPixmap(0, 0, buffer);
        buffer = newBuffer;
    }

    QWidget::resizeEvent(event);
}

void InkCanvas::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    
    // Scale the painter to match zoom factor
    painter.scale(zoomFactor / 100.0, zoomFactor / 100.0);

    // Pan offset needs to be reversed because painter works in transformed coordinates
    painter.translate(-panOffsetX, -panOffsetY);

    if (!backgroundImage.isNull()) {
        painter.drawPixmap(0, 0, backgroundImage);
    }
    painter.drawPixmap(0, 0, buffer);
}

void InkCanvas::tabletEvent(QTabletEvent *event) {


    if (event->pointerType() == QPointingDevice::PointerType::Eraser) {
        if (event->type() == QEvent::TabletPress) {
            previousTool = currentTool;
            currentTool = ToolType::Eraser;
        } else if (event->type() == QEvent::TabletRelease) {
            currentTool = previousTool;
        }
    }

    if (event->type() == QEvent::TabletPress) {
        drawing = true;
        lastPoint = event->posF();
    } else if (event->type() == QEvent::TabletMove && drawing) {
        if (currentTool == ToolType::Eraser) {
            eraseStroke(lastPoint, event->posF(), event->pressure());
        } else {
            drawStroke(lastPoint, event->posF(), event->pressure());
        }
        lastPoint = event->posF();
        processedTimestamps.push_back(benchmarkTimer.elapsed());
    } else if (event->type() == QEvent::TabletRelease) {
        drawing = false;
    }
    event->accept();
}

void InkCanvas::mousePressEvent(QMouseEvent *event) {
    // Ignore mouse/touch input on canvas
    event->ignore();
}

void InkCanvas::mouseMoveEvent(QMouseEvent *event) {
    // Ignore mouse/touch input on canvas
    event->ignore();
}

void InkCanvas::mouseReleaseEvent(QMouseEvent *event) {
    // Ignore mouse/touch input on canvas
    event->ignore();
}

void InkCanvas::drawStroke(const QPointF &start, const QPointF &end, qreal pressure) {
    if (buffer.isNull()) {
        initializeBuffer();
    }

    if (!edited){
        edited = true;
    }

    QPainter painter(&buffer);
    painter.setRenderHint(QPainter::Antialiasing);

    qreal thickness = penThickness;

    qreal updatePadding = (currentTool == ToolType::Marker) ? thickness * 4.0 : 10;

    if (currentTool == ToolType::Marker) {
        thickness *= 8.0;
        QColor markerColor = penColor;
        markerColor.setAlpha(4); // Semi-transparent for marker effect
        QPen pen(markerColor, thickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
    } else { // Default Pen
        qreal scaledThickness = thickness * pressure;  // **Linear pressure scaling**
        QPen pen(penColor, scaledThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
    }

    // Convert screen position to buffer position
    QPointF bufferStart = ((start) / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
    QPointF bufferEnd = ((end) / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);

    painter.drawLine(bufferStart, bufferEnd);

    QRectF updateRect = QRectF(bufferStart, bufferEnd)
                        .normalized()
                        .adjusted(-updatePadding, -updatePadding, updatePadding, updatePadding);

    QRect scaledUpdateRect = QRect(
        ((updateRect.topLeft() - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0)).toPoint(),
        ((updateRect.bottomRight() - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0)).toPoint()
    );
    update(scaledUpdateRect);
}

void InkCanvas::eraseStroke(const QPointF &start, const QPointF &end, qreal pressure) {
    if (buffer.isNull()) {
        initializeBuffer();
    }

    QPainter painter(&buffer);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);

    qreal eraserThickness = penThickness * 6.0;
    QPen eraserPen(Qt::transparent, eraserThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(eraserPen);

    // Convert screen position to buffer position
    QPointF bufferStart = ((start) / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
    QPointF bufferEnd = ((end) / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);

    painter.drawLine(bufferStart, bufferEnd);

    QRectF updateRect = QRectF(bufferStart, bufferEnd)
                        .normalized()
                        .adjusted(-10, -10, 10, 10);

    QRect scaledUpdateRect = QRect(
        ((updateRect.topLeft() - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0)).toPoint(),
        ((updateRect.bottomRight() - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0)).toPoint()
    );
    update(scaledUpdateRect);
}

void InkCanvas::setPenColor(const QColor &color) {
    penColor = color;
}

void InkCanvas::setPenThickness(qreal thickness) {
    penThickness = thickness;
}

void InkCanvas::setTool(ToolType tool) {
    currentTool = tool;
}

void InkCanvas::setSaveFolder(const QString &folderPath) {
    saveFolder = folderPath;
    clearPdfNoDelete(); 

    // ✅ Check if the folder has a saved PDF path
    QString metadataFile = saveFolder + "/.pdf_path.txt";
    if (!QFile::exists(metadataFile)) {
        return;
    }


    QFile file(metadataFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream in(&file);
    QString storedPdfPath = in.readLine().trimmed();
    file.close();

    if (storedPdfPath.isEmpty()) {
        return;
    }


    // ✅ Ensure the stored PDF file still exists before loading
    if (!QFile::exists(storedPdfPath)) {
        return;
    }
    loadPdf(storedPdfPath);
}

void InkCanvas::saveToFile(int pageNumber) {
    if (saveFolder.isEmpty()) {
        return;
    }
    QString filePath = saveFolder + QString("/%1_%2.png").arg(QFileInfo(saveFolder).baseName()).arg(pageNumber, 5, 10, QChar('0'));

    // ✅ If no file exists and the buffer is empty, do nothing
    
    if (!edited) {
        return;
    }


    QImage image(buffer.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.drawPixmap(0, 0, buffer);
    image.save(filePath, "PNG");
    edited = false;
}

void InkCanvas::saveAnnotated(int pageNumber) {
    if (saveFolder.isEmpty()) return;

    QString filePath = saveFolder + QString("/annotated_%1_%2.png").arg(QFileInfo(saveFolder).baseName()).arg(pageNumber, 5, 10, QChar('0'));

    // Use the buffer size to ensure correct resolution
    QImage image(buffer.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    
    if (!backgroundImage.isNull()) {
        painter.drawPixmap(0, 0, backgroundImage.scaled(buffer.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }
    
    painter.drawPixmap(0, 0, buffer);
    image.save(filePath, "PNG");
}


void InkCanvas::loadPage(int pageNumber) {
    if (saveFolder.isEmpty()) return;

    QString fileName = saveFolder + QString("/%1_%2.png").arg(QFileInfo(saveFolder).baseName()).arg(pageNumber, 5, 10, QChar('0'));


    if (QFile::exists(fileName)) {
        buffer.load(fileName);
    } else {
        initializeBuffer(); // Clear the canvas if no file exists
    }
    // Load PDF page as a background if applicable
    if (isPdfLoaded && pdfDocument && pageNumber >= 0 && pageNumber < pdfDocument->numPages()) {
        // std::unique_ptr<Poppler::Page> pdfPage(pdfDocument->page(pageNumber));

        backgroundImage = *pdfCache.object(pageNumber);
        // Resize canvas to match PDF page size
        if (backgroundImage.size() != buffer.size()) {
            QPixmap newBuffer(backgroundImage.size());
            newBuffer.fill(Qt::transparent);

            // Copy existing drawings
            QPainter painter(&newBuffer);
            painter.drawPixmap(0, 0, buffer);

            buffer = newBuffer;
            setMaximumSize(backgroundImage.width(), backgroundImage.height());
        } 
        
    } else {
        QString bgFileName = saveFolder + QString("/bg_%1_%2.png")
                                    .arg(QFileInfo(saveFolder).baseName())
                                    .arg(pageNumber, 5, 10, QChar('0'));

        QString metadataFile = saveFolder + QString("/.%1_bgsize_%2.txt")
                                        .arg(QFileInfo(saveFolder).baseName())
                                        .arg(pageNumber, 5, 10, QChar('0'));

        int bgWidth = 0, bgHeight = 0;
        if (QFile::exists(metadataFile)) {
            QFile file(metadataFile);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                in >> bgWidth >> bgHeight;
                file.close();
            }
        }

        if (QFile::exists(bgFileName)) {
            QImage bgImage(bgFileName);
            backgroundImage = QPixmap::fromImage(bgImage);

            // Resize canvas **only if background resolution is different**
            if (bgWidth > 0 && bgHeight > 0 && (bgWidth != width() || bgHeight != height())) {
                // Create a new buffer
                QPixmap newBuffer(bgWidth, bgHeight);
                newBuffer.fill(Qt::transparent);

                // Copy existing drawings to the new buffer
                QPainter painter(&newBuffer);
                painter.drawPixmap(0, 0, buffer);

                // Assign the new buffer
                buffer = newBuffer;
                setMaximumSize(bgWidth, bgHeight);
            }
        } else {
            backgroundImage = QPixmap(); // No background for this page
        }
    }

    // Check if a background exists for this page
    

    update();
    adjustSize();            // Force the widget to update its size
    parentWidget()->update();
}

void InkCanvas::deletePage(int pageNumber) {
    if (saveFolder.isEmpty()) {
        return;
    }
    QString fileName = saveFolder + QString("/%1_%2.png").arg(QFileInfo(saveFolder).baseName()).arg(pageNumber, 5, 10, QChar('0'));
    QString bgFileName = saveFolder + QString("/bg_%1_%2.png").arg(QFileInfo(saveFolder).baseName()).arg(pageNumber, 5, 10, QChar('0'));
    QString metadataFileName = saveFolder + QString("/.%1_bgsize_%2.txt")  
                                         .arg(QFileInfo(saveFolder).baseName())
                                         .arg(pageNumber, 5, 10, QChar('0'));
    
    #ifdef Q_OS_WIN
    // Remove hidden attribute before deleting in Windows
    SetFileAttributesW(reinterpret_cast<const wchar_t *>(metadataFileName.utf16()), FILE_ATTRIBUTE_NORMAL);
    #endif
    
    QFile::remove(fileName);
    QFile::remove(bgFileName);
    QFile::remove(metadataFileName);
}

void InkCanvas::setBackground(const QString &filePath, int pageNumber) {
    if (saveFolder.isEmpty()) {
        return; // No save folder set
    }

    // Construct full path inside save folder
    QString bgFileName = saveFolder + QString("/bg_%1_%2.png").arg(QFileInfo(saveFolder).baseName()).arg(pageNumber, 5, 10, QChar('0'));

    // Copy the file to the save folder
    QFile::copy(filePath, bgFileName);

    // Load the background image
    QImage bgImage(bgFileName);

    if (!bgImage.isNull()) {
        // Save background resolution in metadata file
        QString metadataFile = saveFolder + QString("/.%1_bgsize_%2.txt")
                                            .arg(QFileInfo(saveFolder).baseName())
                                            .arg(pageNumber, 5, 10, QChar('0'));
        QFile file(metadataFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << bgImage.width() << " " << bgImage.height();
            file.close();
        }

        #ifdef Q_OS_WIN
        // Set hidden attribute for metadata on Windows
        SetFileAttributesW(reinterpret_cast<const wchar_t *>(metadataFile.utf16()), FILE_ATTRIBUTE_HIDDEN);
        #endif    

        // Only resize if the background size is different
        if (bgImage.width() != width() || bgImage.height() != height()) {
            // Create a new buffer with the new size
            QPixmap newBuffer(bgImage.width(), bgImage.height());
            newBuffer.fill(Qt::transparent);

            // Copy existing drawings
            QPainter painter(&newBuffer);
            painter.drawPixmap(0, 0, buffer);

            // Assign new buffer and update canvas size
            buffer = newBuffer;
            setMaximumSize(bgImage.width(), bgImage.height());
        }

        backgroundImage = QPixmap::fromImage(bgImage);
        
        update();
        adjustSize();
        parentWidget()->update();
    }

    update();  // Refresh canvas
}


void InkCanvas::setZoom(int zoomLevel) {
    zoomFactor = qMax(20, qMin(zoomLevel, 400)); // Limit zoom to 50%-400%
    update();
}

void InkCanvas::updatePanOffsets(int xOffset, int yOffset) {
    panOffsetX = xOffset;
    panOffsetY = yOffset;
    update();
}

int InkCanvas::getPanOffsetX() const {
    return panOffsetX;
}

int InkCanvas::getPanOffsetY() const {
    return panOffsetY;
}

int InkCanvas::getZoom() const {
    return zoomFactor;
}

QSize InkCanvas::getCanvasSize() const{
    return buffer.size();
}

void InkCanvas::setPanX(int value) {
    panOffsetX = value;
    update();
}

void InkCanvas::setPanY(int value) {
    panOffsetY = value;
    update();
}

bool InkCanvas::isPdfLoadedFunc() const {
    return isPdfLoaded;
}

int InkCanvas::getTotalPdfPages() const {
    return totalPdfPages;
}

QString InkCanvas::getSaveFolder() const {
    return saveFolder;
}

void InkCanvas::saveCurrentPage() {
    MainWindow *mainWin = qobject_cast<MainWindow*>(parentWidget());  // ✅ Get main window
    if (!mainWin) return;
    
    int currentPage = mainWin->getCurrentPageForCanvas(this);  // ✅ Get correct page
    saveToFile(currentPage);
}

QColor InkCanvas::getPenColor(){
    return penColor;
}

qreal InkCanvas::getPenThickness(){
    return penThickness;
}

ToolType InkCanvas::getCurrentTool() {
    return currentTool;
}