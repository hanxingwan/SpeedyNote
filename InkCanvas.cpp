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

#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>

#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QMessageBox>

#include <poppler-qt6.h>




InkCanvas::InkCanvas(QWidget *parent) 
    : QWidget(parent), drawing(false), penColor(Qt::black), penThickness(5.0), zoomFactor(100), panOffsetX(0), panOffsetY(0), currentTool(ToolType::Pen) {    
    setAttribute(Qt::WA_StaticContents);
    setTabletTracking(true);

    
    
    // Detect screen resolution and set canvas size
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QSize logicalSize = screen->availableGeometry().size() * 0.89;
        setMaximumSize(logicalSize); // Optional
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
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


void InkCanvas::loadPdfPreviewAsync(int pageNumber) {
    if (!pdfDocument || pageNumber < 0 || pageNumber >= pdfDocument->numPages()) return;

    QFutureWatcher<QPixmap> *watcher = new QFutureWatcher<QPixmap>(this);

    connect(watcher, &QFutureWatcher<QPixmap>::finished, this, [this, watcher]() {
        QPixmap result = watcher->result();
        if (!result.isNull()) {
            backgroundImage = result;
            update();  // trigger repaint
        }
        watcher->deleteLater();  // Clean up
    });

    QFuture<QPixmap> future = QtConcurrent::run([=]() -> QPixmap {
        std::unique_ptr<Poppler::Page> page(pdfDocument->page(pageNumber));
        if (!page) return QPixmap();

        QImage pdfImage = page->renderToImage(96, 96);
        if (pdfImage.isNull()) return QPixmap();

        QImage upscaled = pdfImage.scaled(pdfImage.width() * 3, pdfImage.height() * 3,
                                          Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return QPixmap::fromImage(upscaled);
    });

    watcher->setFuture(future);
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

    // 🟨 Optional notebook-style background rendering
    if (backgroundImage.isNull() && backgroundStyle != BackgroundStyle::None) {
        // QPixmap bg(size());
        painter.save();
        painter.fillRect(QRectF(0, 0, buffer.width(), buffer.height()), backgroundColor);  // Default: Qt::white or Qt::transparent

        // QPainter bgPainter(&bg);
        QPen linePen(QColor(100, 100, 100, 100));  // Subtle gray lines
        linePen.setWidthF(1.0);
        painter.setPen(linePen);

        qreal scaledDensity = backgroundDensity;

        if (devicePixelRatioF() > 1.0)
            scaledDensity *= devicePixelRatioF();  // Optional DPI handling

        // const int step = backgroundDensity;  // e.g., 40 px

        if (backgroundStyle == BackgroundStyle::Lines || backgroundStyle == BackgroundStyle::Grid) {
            for (int y = 0; y < buffer.height(); y += scaledDensity)
                painter.drawLine(0, y, buffer.width(), y);
        }
        if (backgroundStyle == BackgroundStyle::Grid) {
            for (int x = 0; x < buffer.width(); x += scaledDensity)
            painter.drawLine(x, 0, x, buffer.height());
        }

        // painter.drawPixmap(0, 0, buffer);
        painter.restore();
    }

    // ✅ Draw loaded image or PDF background if available
    if (!backgroundImage.isNull()) {
        painter.drawPixmap(0, 0, backgroundImage);
    }

    // ✅ Draw user's strokes from the buffer (transparent overlay)
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

    if (!edited){
        edited = true;
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

    if (!saveFolder.isEmpty()) {
        QDir().mkpath(saveFolder);
        loadNotebookId();  // ✅ Load notebook ID when save folder is set
    }

    QString bgMetaFile = saveFolder + "/.background_config.txt";  // This metadata is for background styles, not to be confused with pdf directories. 
    if (QFile::exists(bgMetaFile)) {
        QFile file(bgMetaFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.startsWith("style=")) {
                    QString val = line.mid(6);
                    if (val == "Grid") backgroundStyle = BackgroundStyle::Grid;
                    else if (val == "Lines") backgroundStyle = BackgroundStyle::Lines;
                    else backgroundStyle = BackgroundStyle::None;
                } else if (line.startsWith("color=")) {
                    backgroundColor = QColor(line.mid(6));
                } else if (line.startsWith("density=")) {
                    backgroundDensity = line.mid(8).toInt();
                }
            }
            file.close();
        }
    }

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
    QString filePath = saveFolder + QString("/%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));

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

    QString filePath = saveFolder + QString("/annotated_%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));

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

    QString fileName = saveFolder + QString("/%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));


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
        QString bgFileName = saveFolder + QString("/bg_%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));
        QString metadataFile = saveFolder + QString("/.%1_bgsize_%2.txt").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));

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
    QString fileName = saveFolder + QString("/%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));
    QString bgFileName = saveFolder + QString("/bg_%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));
    QString metadataFileName = saveFolder + QString("/.%1_bgsize_%2.txt").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));
    
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
    QString bgFileName = saveFolder + QString("/bg_%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));

    // Copy the file to the save folder
    QFile::copy(filePath, bgFileName);

    // Load the background image
    QImage bgImage(bgFileName);

    if (!bgImage.isNull()) {
        // Save background resolution in metadata file
        QString metadataFile = saveFolder + QString("/.%1_bgsize_%2.txt")
                                            .arg(notebookId)
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


// for background
void InkCanvas::setBackgroundStyle(BackgroundStyle style) {
    backgroundStyle = style;
    update();  // Trigger repaint
}

void InkCanvas::setBackgroundColor(const QColor &color) {
    backgroundColor = color;
    update();
}

void InkCanvas::setBackgroundDensity(int density) {
    backgroundDensity = density;
    update();
}

void InkCanvas::saveBackgroundMetadata() {
    if (saveFolder.isEmpty()) return;

    QString bgMetaFile = saveFolder + "/.background_config.txt";
    QFile file(bgMetaFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        QString styleStr = "None";
        if (backgroundStyle == BackgroundStyle::Grid) styleStr = "Grid";
        else if (backgroundStyle == BackgroundStyle::Lines) styleStr = "Lines";

        out << "style=" << styleStr << "\n";
        out << "color=" << backgroundColor.name().toUpper() << "\n";
        out << "density=" << backgroundDensity << "\n";
        file.close();
    }
}


void InkCanvas::exportNotebook(const QString &destinationFile) {
    if (destinationFile.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Export Error"), tr("No export file specified."));
        return;
    }

    if (saveFolder.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Export Error"), tr("No notebook loaded (saveFolder is empty)"));
        return;
    }

    QStringList files;

    // Collect all files from saveFolder
    QDir dir(saveFolder);
    QDirIterator it(saveFolder, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        QString relativePath = dir.relativeFilePath(filePath);
        files << relativePath;
    }

    if (files.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Export Error"), tr("No files found to export."));
        return;
    }

    // Generate temporary file list
    QString tempFileList = saveFolder + "/filelist.txt";
    QFile listFile(tempFileList);
    if (listFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&listFile);
        for (const QString &file : files) {
            out << file << "\n";
        }
        listFile.close();
    } else {
        QMessageBox::warning(nullptr, tr("Export Error"), tr("Failed to create temporary file list."));
        return;
    }

#ifdef _WIN32
    QString tarExe = QCoreApplication::applicationDirPath() + "/bsdtar.exe";
#else
    QString tarExe = "tar";
#endif

    QStringList args;
    args << "-cf" << QDir::toNativeSeparators(destinationFile);

    for (const QString &file : files) {
        args << QDir::toNativeSeparators(file);
    }

    QProcess process;
    process.setWorkingDirectory(saveFolder);

    process.start(tarExe, args);
    if (!process.waitForFinished()) {
        QMessageBox::warning(nullptr, tr("Export Error"), tr("Tar process failed to finish."));
        QFile::remove(tempFileList);
        return;
    }

    QFile::remove(tempFileList);

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QMessageBox::warning(nullptr, tr("Export Error"), tr("Tar process failed."));
        return;
    }

    QMessageBox::information(nullptr, tr("Export"), tr("Notebook exported successfully."));
}


void InkCanvas::importNotebook(const QString &packageFile) {

    // Ask user for destination working folder
    QString destFolder = QFileDialog::getExistingDirectory(nullptr, tr("Select Destination Folder for Imported Notebook"));

    if (destFolder.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Import Canceled"), tr("No destination folder selected."));
        return;
    }

    // Check if destination folder is empty (optional, good practice)
    QDir destDir(destFolder);
    if (!destDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, tr("Destination Not Empty"),
            tr("The selected folder is not empty. Files may be overwritten. Continue?"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

#ifdef _WIN32
    QString tarExe = QCoreApplication::applicationDirPath() + "/bsdtar.exe";
#else
    QString tarExe = "tar";
#endif

    // Extract package
    QStringList args;
    args << "-xf" << packageFile;

    QProcess process;
    process.setWorkingDirectory(destFolder);
    process.start(tarExe, args);
    process.waitForFinished();

    // Switch notebook folder
    setSaveFolder(destFolder);
    loadPage(0);

    QMessageBox::information(nullptr, tr("Import Complete"), tr("Notebook imported successfully."));
}


void InkCanvas::loadNotebookId() {
    QString idFile = saveFolder + "/.notebook_id.txt";
    if (QFile::exists(idFile)) {
        QFile file(idFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            notebookId = in.readLine().trimmed();
        }
    } else {
        // No ID file → create new random ID
        notebookId = QUuid::createUuid().toString(QUuid::WithoutBraces).replace("-", "");
        saveNotebookId();
    }
}

void InkCanvas::saveNotebookId() {
    QString idFile = saveFolder + "/.notebook_id.txt";
    QFile file(idFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << notebookId;
    }
}


void InkCanvas::importNotebookTo(const QString &packageFile, const QString &destFolder) {

    #ifdef _WIN32
        QString tarExe = QCoreApplication::applicationDirPath() + "/bsdtar.exe";
    #else
        QString tarExe = "tar";
    #endif
    
        QStringList args;
        args << "-xf" << packageFile;
    
        QProcess process;
        process.setWorkingDirectory(destFolder);
        process.start(tarExe, args);
        process.waitForFinished();
    
        setSaveFolder(destFolder);
        loadNotebookId();
        loadPage(0);

        QMessageBox::information(nullptr, tr("Import"), tr("Notebook imported successfully."));
    }
    