#include "InkCanvas.h"
#include "ToolType.h"
#include "MarkdownWindowManager.h"
#include "MarkdownWindow.h" // Include the full definition
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
#include <QTouchEvent>
#include <QtMath>
#include <QPainterPath>
#include <QTimer>
#include <QAction>

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
    
    // Set theme-aware default pen color
    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent);
    if (mainWindow) {
        penColor = mainWindow->getDefaultPenColor();
    }    
    setAttribute(Qt::WA_StaticContents);
    setTabletTracking(true);
    setAttribute(Qt::WA_AcceptTouchEvents);  // Enable touch events

    // Enable immediate updates for smoother animation
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    
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
    
    // Initialize PDF text selection throttling timer (60 FPS = ~16.67ms)
    pdfTextSelectionTimer = new QTimer(this);
    pdfTextSelectionTimer->setSingleShot(true);
    pdfTextSelectionTimer->setInterval(16); // ~60 FPS
    connect(pdfTextSelectionTimer, &QTimer::timeout, this, &InkCanvas::processPendingTextSelection);
    
    // Initialize PDF cache timer (will be created on-demand)
    pdfCacheTimer = nullptr;
    currentCachedPage = -1;
    
    // Initialize note page cache system
    noteCache.setMaxCost(15); // Cache up to 15 note pages (more than PDF since they're smaller)
    noteCacheTimer = nullptr;
    currentCachedNotePage = -1;
    
    // Initialize markdown manager
    markdownManager = new MarkdownWindowManager(this, this);
    
    // Connect pan/zoom signals to update markdown window positions
    connect(this, &InkCanvas::panChanged, markdownManager, &MarkdownWindowManager::updateAllWindowPositions);
    connect(this, &InkCanvas::zoomChanged, markdownManager, &MarkdownWindowManager::updateAllWindowPositions);
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
        // Enable anti-aliasing rendering hints for better text quality
        pdfDocument->setRenderHint(Poppler::Document::Antialiasing, true);
        pdfDocument->setRenderHint(Poppler::Document::TextAntialiasing, true);
        pdfDocument->setRenderHint(Poppler::Document::TextHinting, true);
        pdfDocument->setRenderHint(Poppler::Document::TextSlightHinting, true);
        
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
        // Emit signal that PDF was loaded
        emit pdfLoaded();
    }
}

void InkCanvas::clearPdf() {
    pdfDocument.reset();
    pdfDocument = nullptr;
    isPdfLoaded = false;
    totalPdfPages = 0;
    pdfCache.clear();
    
    // Reset cache system state
    currentCachedPage = -1;
    if (pdfCacheTimer && pdfCacheTimer->isActive()) {
        pdfCacheTimer->stop();
    }
    
    // Cancel and clean up any active PDF watchers
    for (QFutureWatcher<void>* watcher : activePdfWatchers) {
        if (watcher && !watcher->isFinished()) {
            watcher->cancel();
        }
        watcher->deleteLater();
    }
    activePdfWatchers.clear();

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
    
    // Reset cache system state
    currentCachedPage = -1;
    if (pdfCacheTimer && pdfCacheTimer->isActive()) {
        pdfCacheTimer->stop();
    }
    
    // Cancel and clean up any active PDF watchers
    for (QFutureWatcher<void>* watcher : activePdfWatchers) {
        if (watcher && !watcher->isFinished()) {
            watcher->cancel();
        }
        watcher->deleteLater();
    }
    activePdfWatchers.clear();
}

void InkCanvas::loadPdfPage(int pageNumber) {
    if (!pdfDocument) return;

    // Update current page tracker
    currentCachedPage = pageNumber;

    // Check if target page is already cached
    if (pdfCache.contains(pageNumber)) {
        // Display the cached page immediately
        backgroundImage = *pdfCache.object(pageNumber);
        loadPage(pageNumber);  // Load annotations
        loadPdfTextBoxes(pageNumber); // Load text boxes for PDF text selection
        update();
        
        // Check and cache adjacent pages after delay
        checkAndCacheAdjacentPages(pageNumber);
        return;
    }

    // Target page not in cache - render it immediately
    renderPdfPageToCache(pageNumber);
    
    // Display the newly rendered page
    if (pdfCache.contains(pageNumber)) {
        backgroundImage = *pdfCache.object(pageNumber);
    } else {
        backgroundImage = QPixmap();  // Clear background if rendering failed
    }
    
    loadPage(pageNumber);  // Load existing canvas annotations
    loadPdfTextBoxes(pageNumber); // Load text boxes for PDF text selection
    update();
    
    // Cache adjacent pages after delay
    checkAndCacheAdjacentPages(pageNumber);
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

        // Render with document-level anti-aliasing settings
        QImage pdfImage = page->renderToImage(96, 96);
        if (pdfImage.isNull()) return QPixmap();

        QImage upscaled = pdfImage.scaled(pdfImage.width() * (pdfRenderDPI / 96),
                                  pdfImage.height() * (pdfRenderDPI / 96),
                                  Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation);

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
    
    // Save the painter state before transformations
    painter.save();
    
    // Calculate the scaled canvas size
    qreal scaledCanvasWidth = buffer.width() * (internalZoomFactor / 100.0);
    qreal scaledCanvasHeight = buffer.height() * (internalZoomFactor / 100.0);
    
    // Calculate centering offsets
    qreal centerOffsetX = 0;
    qreal centerOffsetY = 0;
    
    // Center horizontally if canvas is smaller than widget
    if (scaledCanvasWidth < width()) {
        centerOffsetX = (width() - scaledCanvasWidth) / 2.0;
    }
    
    // Center vertically if canvas is smaller than widget
    if (scaledCanvasHeight < height()) {
        centerOffsetY = (height() - scaledCanvasHeight) / 2.0;
    }
    
    // Apply centering offset first
    painter.translate(centerOffsetX, centerOffsetY);
    
    // Use internal zoom factor for smoother animation
    painter.scale(internalZoomFactor / 100.0, internalZoomFactor / 100.0);

    // Pan offset needs to be reversed because painter works in transformed coordinates
    painter.translate(-panOffsetX, -panOffsetY);

    // Set clipping rectangle to canvas bounds to prevent painting outside
    painter.setClipRect(0, 0, buffer.width(), buffer.height());

    // 🟨 Notebook-style background rendering
    if (backgroundImage.isNull()) {
        painter.save();
        
        // Always apply background color regardless of style
        painter.fillRect(QRectF(0, 0, buffer.width(), buffer.height()), backgroundColor);

        // Only draw grid/lines if not "None" style
        if (backgroundStyle != BackgroundStyle::None) {
        QPen linePen(QColor(100, 100, 100, 100));  // Subtle gray lines
        linePen.setWidthF(1.0);
        painter.setPen(linePen);

        qreal scaledDensity = backgroundDensity;

        if (devicePixelRatioF() > 1.0)
            scaledDensity *= devicePixelRatioF();  // Optional DPI handling

        if (backgroundStyle == BackgroundStyle::Lines || backgroundStyle == BackgroundStyle::Grid) {
            for (int y = 0; y < buffer.height(); y += scaledDensity)
                painter.drawLine(0, y, buffer.width(), y);
        }
        if (backgroundStyle == BackgroundStyle::Grid) {
            for (int x = 0; x < buffer.width(); x += scaledDensity)
            painter.drawLine(x, 0, x, buffer.height());
            }
        }

        painter.restore();
    }

    // ✅ Draw loaded image or PDF background if available
    if (!backgroundImage.isNull()) {
        painter.drawPixmap(0, 0, backgroundImage);
    }

    // ✅ Draw user's strokes from the buffer (transparent overlay)
    painter.drawPixmap(0, 0, buffer);
    
    // Draw straight line preview if in straight line mode and drawing
    // Skip preview for eraser tool
    if (straightLineMode && drawing && currentTool != ToolType::Eraser) {
        // Save the current state before applying the eraser mode
        painter.save();
        
        // Store current pressure - ensure minimum is 0.5 for consistent preview
        qreal pressure = qMax(0.5, painter.device()->devicePixelRatioF() > 1.0 ? 0.8 : 1.0);
        
        // Set up the pen based on tool type
        if (currentTool == ToolType::Marker) {
            qreal thickness = penThickness * 8.0;
            QColor markerColor = penColor;
            // Increase alpha for better visibility in straight line mode
            // Using a higher value (80) instead of the regular 4 to compensate for single-pass drawing
            markerColor.setAlpha(80);
            QPen pen(markerColor, thickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
        } else { // Default Pen
            // Match the exact same thickness calculation as in drawStroke
            qreal scaledThickness = penThickness * pressure;
            QPen pen(penColor, scaledThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
        }
        
        // Use the same coordinate transformation logic as in drawStroke
        // to ensure the preview line appears at the exact same position
        QPointF bufferStart, bufferEnd;
        
        // Convert screen coordinates to buffer coordinates using the same calculations as drawStroke
        qreal scaledCanvasWidth = buffer.width() * (zoomFactor / 100.0);
        qreal scaledCanvasHeight = buffer.height() * (zoomFactor / 100.0);
        qreal centerOffsetX = (scaledCanvasWidth < width()) ? (width() - scaledCanvasWidth) / 2.0 : 0;
        qreal centerOffsetY = (scaledCanvasHeight < height()) ? (height() - scaledCanvasHeight) / 2.0 : 0;
        
        QPointF adjustedStart = straightLineStartPoint - QPointF(centerOffsetX, centerOffsetY);
        QPointF adjustedEnd = lastPoint - QPointF(centerOffsetX, centerOffsetY);
        
        bufferStart = (adjustedStart / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
        bufferEnd = (adjustedEnd / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
        
        // Draw the preview line using the same coordinates that will be used for the final line
        painter.drawLine(bufferStart, bufferEnd);
        
        // Restore the original painter state
        painter.restore();
    }

    // Draw selection rectangle if in rope tool mode and selecting, moving, or has a selection
    if (ropeToolMode && (selectingWithRope || movingSelection || (!selectionBuffer.isNull() && !selectionRect.isEmpty()))) {
        painter.save(); // Save painter state for overlays
        painter.resetTransform(); // Reset transform to draw directly in logical widget coordinates
        
        if (selectingWithRope && !lassoPathPoints.isEmpty()) {
            QPen selectionPen(Qt::DashLine);
            selectionPen.setColor(Qt::blue);
            selectionPen.setWidthF(1.5); // Width in logical pixels
            painter.setPen(selectionPen);
            painter.drawPolygon(lassoPathPoints); // lassoPathPoints are logical widget coordinates
        } else if (!selectionBuffer.isNull() && !selectionRect.isEmpty()) {
            // selectionRect is in logical widget coordinates.
            // selectionBuffer is in buffer coordinates, we need to handle scaling correctly
            QPixmap scaledBuffer = selectionBuffer;
            
            // Calculate the current zoom factor
            qreal currentZoom = internalZoomFactor / 100.0;
            
            // Scale the selection buffer to match the current zoom level
            if (currentZoom != 1.0) {
                QSize scaledSize = QSize(
                    qRound(scaledBuffer.width() * currentZoom),
                    qRound(scaledBuffer.height() * currentZoom)
                );
                scaledBuffer = scaledBuffer.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            
            // Draw it at the logical position
            // Use exactSelectionRectF for smoother movement if available
            QPointF topLeft = exactSelectionRectF.isEmpty() ? selectionRect.topLeft() : exactSelectionRectF.topLeft();
            painter.drawPixmap(topLeft, scaledBuffer);

            QPen selectionBorderPen(Qt::DashLine);
            selectionBorderPen.setColor(Qt::darkCyan);
            selectionBorderPen.setWidthF(1.5); // Width in logical pixels
            painter.setPen(selectionBorderPen);
            
            // Use exactSelectionRectF for drawing the selection border if available
            if (!exactSelectionRectF.isEmpty()) {
                painter.drawRect(exactSelectionRectF);
            } else {
                painter.drawRect(selectionRect);
            }
        }
        painter.restore(); // Restore painter state to what it was for drawing the main buffer
    }
    

    

    
    // Restore the painter state
    painter.restore();
    
    // Fill the area outside the canvas with the widget's background color
    QRect widgetRect = rect();
    QRectF canvasRect(
        centerOffsetX - panOffsetX * (internalZoomFactor / 100.0),
        centerOffsetY - panOffsetY * (internalZoomFactor / 100.0),
        buffer.width() * (internalZoomFactor / 100.0),
        buffer.height() * (internalZoomFactor / 100.0)
    );
    
    // Create regions for areas outside the canvas
    QRegion outsideRegion(widgetRect);
    outsideRegion -= QRegion(canvasRect.toRect());
    
    // Fill the outside region with the background color
    painter.setClipRegion(outsideRegion);
    painter.fillRect(widgetRect, palette().window().color());
    
    // Reset clipping for overlay elements that should appear on top
    painter.setClipping(false);
    
    // Draw markdown selection overlay
    if (markdownSelectionMode && markdownSelecting) {
        painter.save();
        QPen selectionPen(Qt::DashLine);
        selectionPen.setColor(Qt::green);
        selectionPen.setWidthF(2.0);
        painter.setPen(selectionPen);
        
        QBrush selectionBrush(QColor(0, 255, 0, 30)); // Semi-transparent green
        painter.setBrush(selectionBrush);
        
        QRect selectionRect = QRect(markdownSelectionStart, markdownSelectionEnd).normalized();
        painter.drawRect(selectionRect);
        painter.restore();
    }
    
    // Draw PDF text selection overlay on top of everything
    if (pdfTextSelectionEnabled && isPdfLoaded) {
        painter.save(); // Save painter state for PDF text overlay
        painter.resetTransform(); // Reset transform to draw directly in logical widget coordinates
        
        // Draw selection rectangle if actively selecting
        if (pdfTextSelecting) {
            QPen selectionPen(Qt::DashLine);
            selectionPen.setColor(QColor(0, 120, 215)); // Blue selection rectangle
            selectionPen.setWidthF(2.0); // Make it more visible
            painter.setPen(selectionPen);
            
            QBrush selectionBrush(QColor(0, 120, 215, 30)); // Semi-transparent blue fill
            painter.setBrush(selectionBrush);
            
            QRectF selectionRect(pdfSelectionStart, pdfSelectionEnd);
            selectionRect = selectionRect.normalized();
            painter.drawRect(selectionRect);
        }
        
        // Draw highlights for selected text boxes
        if (!selectedTextBoxes.isEmpty()) {
            QColor highlightColor = QColor(255, 255, 0, 100); // Semi-transparent yellow
            painter.setBrush(highlightColor);
            painter.setPen(Qt::NoPen);
            
            // Draw highlight rectangles for selected text boxes
            for (const Poppler::TextBox* textBox : selectedTextBoxes) {
                if (textBox) {
                    // Convert PDF coordinates to widget coordinates
                    QRectF pdfRect = textBox->boundingBox();
                    QPointF topLeft = mapPdfToWidgetCoordinates(pdfRect.topLeft());
                    QPointF bottomRight = mapPdfToWidgetCoordinates(pdfRect.bottomRight());
                    QRectF widgetRect(topLeft, bottomRight);
                    widgetRect = widgetRect.normalized();
                    
                    painter.drawRect(widgetRect);
                }
            }
        }
        
        painter.restore(); // Restore painter state
    }
}

void InkCanvas::tabletEvent(QTabletEvent *event) {
    // ✅ PRIORITY: Handle PDF text selection with stylus when enabled
    // This redirects stylus input to text selection instead of drawing
    if (pdfTextSelectionEnabled && isPdfLoaded) {
        if (event->type() == QEvent::TabletPress) {
            pdfTextSelecting = true;
            pdfSelectionStart = event->position();
            pdfSelectionEnd = pdfSelectionStart;
            
            // Clear any existing selected text boxes without resetting pdfTextSelecting
            selectedTextBoxes.clear();
            
            setCursor(Qt::IBeamCursor); // Ensure cursor is correct
            update(); // Refresh display
            event->accept();
            return;
        } else if (event->type() == QEvent::TabletMove && pdfTextSelecting) {
            pdfSelectionEnd = event->position();
            
            // Store pending selection for throttled processing (60 FPS throttling)
            pendingSelectionStart = pdfSelectionStart;
            pendingSelectionEnd = pdfSelectionEnd;
            hasPendingSelection = true;
            
            // Start timer if not already running (throttled to 60 FPS)
            if (!pdfTextSelectionTimer->isActive()) {
                pdfTextSelectionTimer->start();
            }
            
            // NOTE: No direct update() call here - let the timer handle updates at 60 FPS
            event->accept();
            return;
        } else if (event->type() == QEvent::TabletRelease && pdfTextSelecting) {
            pdfSelectionEnd = event->position();
            
            // Process any pending selection immediately on release
            if (pdfTextSelectionTimer && pdfTextSelectionTimer->isActive()) {
                pdfTextSelectionTimer->stop();
                if (hasPendingSelection) {
                    updatePdfTextSelection(pendingSelectionStart, pendingSelectionEnd);
                    hasPendingSelection = false;
                }
            } else {
                // Update selection with final position
                updatePdfTextSelection(pdfSelectionStart, pdfSelectionEnd);
            }
            
            pdfTextSelecting = false;
            
            // Check for link clicks if no text was selected
            QString selectedText = getSelectedPdfText();
            if (selectedText.isEmpty()) {
                handlePdfLinkClick(event->position());
            } else {
                // Show context menu for text selection
                QPoint globalPos = mapToGlobal(event->position().toPoint());
                showPdfTextSelectionMenu(globalPos);
            }
            
            event->accept();
            return;
        }
    }
    
    // ✅ NORMAL STYLUS BEHAVIOR: Only reached when PDF text selection is OFF
    // Hardware eraser detection
    static bool hardwareEraserActive = false;
    bool wasUsingHardwareEraser = false;
    
    // Track hardware eraser state
    if (event->pointerType() == QPointingDevice::PointerType::Eraser) {
        // Hardware eraser is being used
        wasUsingHardwareEraser = true;
        
        if (event->type() == QEvent::TabletPress) {
            // Start of eraser stroke - save current tool and switch to eraser
            hardwareEraserActive = true;
            previousTool = currentTool;
            currentTool = ToolType::Eraser;
        }
    }
    
    // Maintain hardware eraser state across move events
    if (hardwareEraserActive && event->type() != QEvent::TabletRelease) {
        wasUsingHardwareEraser = true;
    }

    // Determine if we're in eraser mode (either hardware eraser or tool set to eraser)
    bool isErasing = (currentTool == ToolType::Eraser);

    if (event->type() == QEvent::TabletPress) {
        drawing = true;
        lastPoint = event->posF(); // Logical widget coordinates
        if (straightLineMode) {
            straightLineStartPoint = lastPoint;
        }
        if (ropeToolMode) {
            if (!selectionBuffer.isNull() && selectionRect.contains(lastPoint.toPoint())) {
                // Start moving an existing selection (or continue if already moving)
                movingSelection = true;
                selectingWithRope = false;
                lastMovePoint = lastPoint;
                // Initialize the exact floating-point rect if it's empty
                if (exactSelectionRectF.isEmpty()) {
                    exactSelectionRectF = QRectF(selectionRect);
                }
                
                // If this selection was just copied, clear the area now that user is moving it
                if (selectionJustCopied) {
                    QPainter painter(&buffer);
                    painter.setCompositionMode(QPainter::CompositionMode_Clear);
                    QPointF bufferDest = mapLogicalWidgetToPhysicalBuffer(selectionRect.topLeft());
                    QRect clearRect(bufferDest.toPoint(), selectionBuffer.size());
                    // Only clear the part that's within buffer bounds
                    QRect bufferBounds(0, 0, buffer.width(), buffer.height());
                    QRect clippedClearRect = clearRect.intersected(bufferBounds);
                    if (!clippedClearRect.isEmpty()) {
                        painter.fillRect(clippedClearRect, Qt::transparent);
                    }
                    painter.end();
                    selectionJustCopied = false; // Clear the flag
                }
                
                // If the selection area hasn't been cleared from the buffer yet, clear it now
                if (!selectionAreaCleared && !selectionMaskPath.isEmpty()) {
                    QPainter painter(&buffer);
                    painter.setCompositionMode(QPainter::CompositionMode_Clear);
                    painter.fillPath(selectionMaskPath, Qt::transparent);
                    painter.end();
                    selectionAreaCleared = true;
                }
                // selectionBuffer already has the content.
                // The original area in 'buffer' was already cleared when selection was made.
            } else {
                // Start a new selection or cancel existing one
                if (!selectionBuffer.isNull()) { // If there's an active selection, a tap outside cancels it
                    selectionBuffer = QPixmap();
                    selectionRect = QRect();
                    lassoPathPoints.clear();
                    movingSelection = false;
                    selectingWithRope = false;
                    selectionJustCopied = false;
                    selectionAreaCleared = false;
                    selectionMaskPath = QPainterPath();
                    selectionBufferRect = QRectF();
                    update(); // Full update to remove old selection visuals
                    drawing = false; // Consumed this press for cancel
                    return;
                }
                selectingWithRope = true;
                movingSelection = false;
                selectionJustCopied = false;
                selectionAreaCleared = false;
                selectionMaskPath = QPainterPath();
                selectionBufferRect = QRectF();
                lassoPathPoints.clear();
                lassoPathPoints << lastPoint; // Start the lasso path
                selectionRect = QRect();
                selectionBuffer = QPixmap();
            }
        }
    } else if (event->type() == QEvent::TabletMove && drawing) {
        if (ropeToolMode) {
            if (selectingWithRope) {
                QRectF oldPreviewBoundingRect = lassoPathPoints.boundingRect();
                lassoPathPoints << event->posF();
                lastPoint = event->posF();
                QRectF newPreviewBoundingRect = lassoPathPoints.boundingRect();
                // Update the area of the selection rectangle preview (logical widget coordinates)
                update(oldPreviewBoundingRect.united(newPreviewBoundingRect).toRect().adjusted(-5,-5,5,5));
            } else if (movingSelection) {
                QPointF delta = event->posF() - lastMovePoint; // Delta in logical widget coordinates
                QRect oldWidgetSelectionRect = selectionRect;
                
                // Update the exact floating-point rectangle first
                exactSelectionRectF.translate(delta);
                
                // Convert back to integer rect, but only when the position actually changes
                QRect newRect = exactSelectionRectF.toRect();
                if (newRect != selectionRect) {
                    selectionRect = newRect;
                    // Update the combined area of the old and new selection positions (logical widget coordinates)
                    update(oldWidgetSelectionRect.united(selectionRect).adjusted(-2,-2,2,2));
                } else {
                    // Even if the integer position didn't change, we still need to update
                    // to make movement feel smoother, especially with slow movements
                    update(selectionRect.adjusted(-2,-2,2,2));
                }
                
                lastMovePoint = event->posF();
                if (!edited){
                    edited = true;
                    // Invalidate cache for current page since it's been modified
                    invalidateCurrentPageCache();
                }
            }
        } else if (straightLineMode && !isErasing) {
            // For straight line mode with non-eraser tools, just update the last position
            // and trigger a repaint of only the affected area for preview
            static QElapsedTimer updateTimer;
            static bool timerInitialized = false;
            
            if (!timerInitialized) {
                updateTimer.start();
                timerInitialized = true;
            }
            
            // Throttle updates based on time for high CPU usage tools
            bool shouldUpdate = true;
            
            // Apply throttling for marker which can be CPU intensive
            if (currentTool == ToolType::Marker) {
                shouldUpdate = updateTimer.elapsed() > 16; // Only update every 16ms (approx 60fps)
            }
            
            if (shouldUpdate) {
                QPointF oldLastPoint = lastPoint;
                lastPoint = event->posF();
                
                // Calculate affected rectangle that needs updating
                QRectF updateRect = calculatePreviewRect(straightLineStartPoint, oldLastPoint, lastPoint);
                update(updateRect.toRect());
                
                // Reset timer
                updateTimer.restart();
            } else {
                // Just update the last point without redrawing
                lastPoint = event->posF();
            }
        } else if (straightLineMode && isErasing) {
            // For eraser in straight line mode, continuously erase from start to current point
            // This gives immediate visual feedback and smoother erasing experience
            
            // Store current point
            QPointF currentPoint = event->posF();
            
            // Clear previous stroke by redrawing with transparency
            QRectF updateRect = QRectF(straightLineStartPoint, lastPoint).normalized().adjusted(-20, -20, 20, 20);
            update(updateRect.toRect());
            
            // Erase from start point to current position
            eraseStroke(straightLineStartPoint, currentPoint, event->pressure());
            
            // Update last point
            lastPoint = currentPoint;
            
            // Only track benchmarking when enabled
            if (benchmarking) {
                processedTimestamps.push_back(benchmarkTimer.elapsed());
            }
        } else {
            // Normal drawing mode OR eraser regardless of straight line mode
            if (isErasing) {
            eraseStroke(lastPoint, event->posF(), event->pressure());
        } else {
            drawStroke(lastPoint, event->posF(), event->pressure());
        }
        lastPoint = event->posF();
            
            // Only track benchmarking when enabled
            if (benchmarking) {
        processedTimestamps.push_back(benchmarkTimer.elapsed());
            }
        }
    } else if (event->type() == QEvent::TabletRelease) {
        if (straightLineMode && !isErasing) {
            // Draw the final line on release with the current pressure
            qreal pressure = event->pressure();
            
            // Always use at least a minimum pressure
            pressure = qMax(pressure, 0.5);
            
            drawStroke(straightLineStartPoint, event->posF(), pressure);
            
            // Only track benchmarking when enabled
            if (benchmarking) {
                processedTimestamps.push_back(benchmarkTimer.elapsed());
            }
            
            // Force repaint to clear the preview line
            update();
            if (!edited){
                edited = true;
                // Invalidate cache for current page since it's been modified
                invalidateCurrentPageCache();
            }
        } else if (straightLineMode && isErasing) {
            // For erasing in straight line mode, most of the work is done during movement
            // Just ensure one final erasing pass from start to end point
            qreal pressure = qMax(event->pressure(), 0.5);
            
            // Final pass to ensure the entire line is erased
            eraseStroke(straightLineStartPoint, event->posF(), pressure);
            
            // Force update to clear any remaining artifacts
            update();
            if (!edited){
                edited = true;
                // Invalidate cache for current page since it's been modified
                invalidateCurrentPageCache();
            }
        }
        
        drawing = false;
        
        // Reset tool state if we were using the hardware eraser
        if (wasUsingHardwareEraser) {
            currentTool = previousTool;
            hardwareEraserActive = false;  // Reset hardware eraser tracking
        }

        if (ropeToolMode) {
            if (selectingWithRope) {
                if (lassoPathPoints.size() > 2) { // Need at least 3 points for a polygon
                    lassoPathPoints << lassoPathPoints.first(); // Close the polygon
                    
                    if (!lassoPathPoints.boundingRect().isEmpty()) {
                        // 1. Create a QPolygonF in buffer coordinates using proper transformation
                        QPolygonF bufferLassoPath;
                        for (const QPointF& p_widget_logical : qAsConst(lassoPathPoints)) {
                            bufferLassoPath << mapLogicalWidgetToPhysicalBuffer(p_widget_logical);
                        }

                        // 2. Get the bounding box of this path on the buffer
                        QRectF bufferPathBoundingRect = bufferLassoPath.boundingRect();

                        // 3. Copy that part of the main buffer
                        QPixmap originalPiece = buffer.copy(bufferPathBoundingRect.toRect());

                        // 4. Create the selectionBuffer (same size as originalPiece) and fill transparent
                        selectionBuffer = QPixmap(originalPiece.size());
                        selectionBuffer.fill(Qt::transparent);
                        
                        // 5. Create a mask from the lasso path
                        QPainterPath maskPath;
                        // The lasso path for the mask needs to be relative to originalPiece.topLeft()
                        maskPath.addPolygon(bufferLassoPath.translated(-bufferPathBoundingRect.topLeft()));
                        
                        // 6. Paint the originalPiece onto selectionBuffer, using the mask
                        QPainter selectionPainter(&selectionBuffer);
                        selectionPainter.setClipPath(maskPath);
                        selectionPainter.drawPixmap(0,0, originalPiece);
                        selectionPainter.end();

                        // 7. DON'T clear the selected area from the main buffer yet
                        // The area will only be cleared when the user actually starts moving the selection
                        // This way, if the user cancels without moving, the original content remains
                        selectionAreaCleared = false;
                        
                        // Store the mask path and buffer rect for later clearing when movement starts
                        selectionMaskPath = maskPath.translated(bufferPathBoundingRect.topLeft());
                        selectionBufferRect = bufferPathBoundingRect;
                        
                        // 8. Calculate the correct selectionRect in logical widget coordinates
                        QRectF logicalSelectionRect = mapRectBufferToWidgetLogical(bufferPathBoundingRect);
                        selectionRect = logicalSelectionRect.toRect();
                        exactSelectionRectF = logicalSelectionRect;
                        
                        // Update the area of the selection on screen
                        update(logicalSelectionRect.adjusted(-2,-2,2,2).toRect());
                        
                        // Emit signal for context menu at the center of the selection with a delay
                        // This allows the user to immediately start moving the selection if desired
                        QPoint menuPosition = selectionRect.center();
                        QTimer::singleShot(500, this, [this, menuPosition]() {
                            // Only show menu if selection still exists and hasn't been moved
                            if (!selectionBuffer.isNull() && !selectionRect.isEmpty() && !movingSelection) {
                                emit ropeSelectionCompleted(menuPosition);
                            }
                        });
                    }
                }
                lassoPathPoints.clear(); // Ready for next selection, or move
                selectingWithRope = false;
                // Now, if the user presses inside selectionRect, movingSelection will become true.
            } else if (movingSelection) {
                if (!selectionBuffer.isNull() && !selectionRect.isEmpty()) {
                    QPainter painter(&buffer);
                    // Explicitly set composition mode to draw on top of existing content
                    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
                    // Use exact floating-point position if available for more precise placement
                    QPointF topLeft = exactSelectionRectF.isEmpty() ? selectionRect.topLeft() : exactSelectionRectF.topLeft();
                    // Use proper coordinate transformation to get buffer coordinates
                    QPointF bufferDest = mapLogicalWidgetToPhysicalBuffer(topLeft);
                    painter.drawPixmap(bufferDest.toPoint(), selectionBuffer);
                    painter.end();
                    
                    // Update the pasted area
                    QRectF bufferPasteRect(bufferDest, selectionBuffer.size());
                    update(mapRectBufferToWidgetLogical(bufferPasteRect).adjusted(-2,-2,2,2));
                    
                    // Clear selection after pasting, making it permanent
                    selectionBuffer = QPixmap();
                    selectionRect = QRect();
                    exactSelectionRectF = QRectF();
                    movingSelection = false;
                    selectionJustCopied = false;
                    selectionAreaCleared = false;
                    selectionMaskPath = QPainterPath();
                    selectionBufferRect = QRectF();
                }
            }
        }
        

    }
    event->accept();
}

// Helper function to calculate area that needs updating for preview
QRectF InkCanvas::calculatePreviewRect(const QPointF &start, const QPointF &oldEnd, const QPointF &newEnd) {
    // Calculate centering offsets - use the same calculation as in paintEvent
    qreal scaledCanvasWidth = buffer.width() * (zoomFactor / 100.0);
    qreal scaledCanvasHeight = buffer.height() * (zoomFactor / 100.0);
    qreal centerOffsetX = (scaledCanvasWidth < width()) ? (width() - scaledCanvasWidth) / 2.0 : 0;
    qreal centerOffsetY = (scaledCanvasHeight < height()) ? (height() - scaledCanvasHeight) / 2.0 : 0;
    
    // Calculate the buffer coordinates for our points (same as in drawStroke)
    QPointF adjustedStart = start - QPointF(centerOffsetX, centerOffsetY);
    QPointF adjustedOldEnd = oldEnd - QPointF(centerOffsetX, centerOffsetY);
    QPointF adjustedNewEnd = newEnd - QPointF(centerOffsetX, centerOffsetY);
    
    QPointF bufferStart = (adjustedStart / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
    QPointF bufferOldEnd = (adjustedOldEnd / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
    QPointF bufferNewEnd = (adjustedNewEnd / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
    
    // Now convert from buffer coordinates to screen coordinates
    QPointF screenStart = (bufferStart - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0) + QPointF(centerOffsetX, centerOffsetY);
    QPointF screenOldEnd = (bufferOldEnd - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0) + QPointF(centerOffsetX, centerOffsetY);
    QPointF screenNewEnd = (bufferNewEnd - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0) + QPointF(centerOffsetX, centerOffsetY);
    
    // Create rectangles for both the old and new lines
    QRectF oldLineRect = QRectF(screenStart, screenOldEnd).normalized();
    QRectF newLineRect = QRectF(screenStart, screenNewEnd).normalized();
    
    // Calculate padding based on pen thickness and device pixel ratio
    qreal dpr = devicePixelRatioF();
    qreal padding;
    
    if (currentTool == ToolType::Eraser) {
        padding = penThickness * 6.0 * dpr;
    } else if (currentTool == ToolType::Marker) {
        padding = penThickness * 8.0 * dpr;
    } else {
        padding = penThickness * dpr;
    }
    
    // Ensure minimum padding
    padding = qMax(padding, 15.0);
    
    // Combine rectangles with appropriate padding
    QRectF combinedRect = oldLineRect.united(newLineRect);
    return combinedRect.adjusted(-padding, -padding, padding, padding);
}

void InkCanvas::mousePressEvent(QMouseEvent *event) {
    // Handle markdown selection when enabled
    if (markdownSelectionMode && event->button() == Qt::LeftButton) {
        markdownSelecting = true;
        markdownSelectionStart = event->pos();
        markdownSelectionEnd = markdownSelectionStart;
        event->accept();
        return;
    }
    
    // Handle PDF text selection when enabled (mouse/touch fallback - stylus handled in tabletEvent)
    if (pdfTextSelectionEnabled && isPdfLoaded) {
        if (event->button() == Qt::LeftButton) {
            pdfTextSelecting = true;
            pdfSelectionStart = event->position();
            pdfSelectionEnd = pdfSelectionStart;
            
            // Clear any existing selected text boxes without resetting pdfTextSelecting
            selectedTextBoxes.clear();
            
            setCursor(Qt::IBeamCursor); // Ensure cursor is correct
            update(); // Refresh display
            event->accept();
            return;
        }
    }
    
    // Ignore mouse/touch input on canvas for drawing (drawing only works with tablet/stylus)
    event->ignore();
}

void InkCanvas::mouseMoveEvent(QMouseEvent *event) {
    // Handle markdown selection when enabled
    if (markdownSelectionMode && markdownSelecting) {
        markdownSelectionEnd = event->pos();
        update(); // Refresh to show selection rectangle
        event->accept();
        return;
    }
    
    // Handle PDF text selection when enabled (mouse/touch fallback - stylus handled in tabletEvent)
    if (pdfTextSelectionEnabled && isPdfLoaded && pdfTextSelecting) {
        pdfSelectionEnd = event->position();
        
        // Store pending selection for throttled processing
        pendingSelectionStart = pdfSelectionStart;
        pendingSelectionEnd = pdfSelectionEnd;
        hasPendingSelection = true;
        
        // Start timer if not already running (throttled to 60 FPS)
        if (!pdfTextSelectionTimer->isActive()) {
            pdfTextSelectionTimer->start();
        }
        
        event->accept();
        return;
    }
    
    // Update cursor based on mode when not actively selecting
    if (pdfTextSelectionEnabled && isPdfLoaded && !pdfTextSelecting) {
        setCursor(Qt::IBeamCursor);
    }
    
    event->ignore();
}

void InkCanvas::mouseReleaseEvent(QMouseEvent *event) {
    // Handle markdown selection when enabled
    if (markdownSelectionMode && markdownSelecting && event->button() == Qt::LeftButton) {
        markdownSelecting = false;
        
        // Create markdown window if selection is valid
        QRect selectionRect = QRect(markdownSelectionStart, markdownSelectionEnd).normalized();
        if (selectionRect.width() > 50 && selectionRect.height() > 50 && markdownManager) {
            markdownManager->createMarkdownWindow(selectionRect);
        }
        
        // Exit selection mode
        setMarkdownSelectionMode(false);
        
        // Force screen update to clear the green selection overlay
        update();
        
        event->accept();
        return;
    }
    
    // Handle PDF text selection when enabled (mouse/touch fallback - stylus handled in tabletEvent)
    if (pdfTextSelectionEnabled && isPdfLoaded && pdfTextSelecting) {
        if (event->button() == Qt::LeftButton) {
            pdfSelectionEnd = event->position();
            
            // Process any pending selection immediately on release
            if (pdfTextSelectionTimer && pdfTextSelectionTimer->isActive()) {
                pdfTextSelectionTimer->stop();
                if (hasPendingSelection) {
                    updatePdfTextSelection(pendingSelectionStart, pendingSelectionEnd);
                    hasPendingSelection = false;
                }
            } else {
                // Update selection with final position
                updatePdfTextSelection(pdfSelectionStart, pdfSelectionEnd);
            }
            
            pdfTextSelecting = false;
            
            // Check for link clicks if no text was selected
            QString selectedText = getSelectedPdfText();
            if (selectedText.isEmpty()) {
                handlePdfLinkClick(event->position());
            } else {
                // Show context menu for text selection
                QPoint globalPos = mapToGlobal(event->position().toPoint());
                showPdfTextSelectionMenu(globalPos);
            }
            
            event->accept();
            return;
        }
    }
    

    
    event->ignore();
}

void InkCanvas::drawStroke(const QPointF &start, const QPointF &end, qreal pressure) {
    if (buffer.isNull()) {
        initializeBuffer();
    }

    if (!edited){
        edited = true;
        // Invalidate cache for current page since it's been modified
        invalidateCurrentPageCache();
    }

    QPainter painter(&buffer);
    painter.setRenderHint(QPainter::Antialiasing);

    qreal thickness = penThickness;

    qreal updatePadding = (currentTool == ToolType::Marker) ? thickness * 4.0 : 10;

    if (currentTool == ToolType::Marker) {
        thickness *= 8.0;
        QColor markerColor = penColor;
        // Adjust alpha based on whether we're in straight line mode
        if (straightLineMode) {
            // For straight line mode, use higher alpha to make it more visible
            markerColor.setAlpha(40);
        } else {
            // For regular drawing, use lower alpha for the usual marker effect
            markerColor.setAlpha(4);
        }
        QPen pen(markerColor, thickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
    } else { // Default Pen
        qreal scaledThickness = thickness * pressure;  // **Linear pressure scaling**
        QPen pen(penColor, scaledThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
    }

    // Calculate centering offsets
    qreal scaledCanvasWidth = buffer.width() * (zoomFactor / 100.0);
    qreal scaledCanvasHeight = buffer.height() * (zoomFactor / 100.0);
    qreal centerOffsetX = (scaledCanvasWidth < width()) ? (width() - scaledCanvasWidth) / 2.0 : 0;
    qreal centerOffsetY = (scaledCanvasHeight < height()) ? (height() - scaledCanvasHeight) / 2.0 : 0;
    
    // Convert screen position to buffer position, accounting for centering
    QPointF adjustedStart = start - QPointF(centerOffsetX, centerOffsetY);
    QPointF adjustedEnd = end - QPointF(centerOffsetX, centerOffsetY);
    
    QPointF bufferStart = (adjustedStart / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
    QPointF bufferEnd = (adjustedEnd / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);

    painter.drawLine(bufferStart, bufferEnd);

    QRectF updateRect = QRectF(bufferStart, bufferEnd)
                        .normalized()
                        .adjusted(-updatePadding, -updatePadding, updatePadding, updatePadding);

    QRect scaledUpdateRect = QRect(
        ((updateRect.topLeft() - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0) + QPointF(centerOffsetX, centerOffsetY)).toPoint(),
        ((updateRect.bottomRight() - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0) + QPointF(centerOffsetX, centerOffsetY)).toPoint()
    );
    update(scaledUpdateRect);
}

void InkCanvas::eraseStroke(const QPointF &start, const QPointF &end, qreal pressure) {
    if (buffer.isNull()) {
        initializeBuffer();
    }

    if (!edited){
        edited = true;
        // Invalidate cache for current page since it's been modified
        invalidateCurrentPageCache();
    }

    QPainter painter(&buffer);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);

    qreal eraserThickness = penThickness * 6.0;
    QPen eraserPen(Qt::transparent, eraserThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(eraserPen);

    // Calculate centering offsets
    qreal scaledCanvasWidth = buffer.width() * (zoomFactor / 100.0);
    qreal scaledCanvasHeight = buffer.height() * (zoomFactor / 100.0);
    qreal centerOffsetX = (scaledCanvasWidth < width()) ? (width() - scaledCanvasWidth) / 2.0 : 0;
    qreal centerOffsetY = (scaledCanvasHeight < height()) ? (height() - scaledCanvasHeight) / 2.0 : 0;
    
    // Convert screen position to buffer position, accounting for centering
    QPointF adjustedStart = start - QPointF(centerOffsetX, centerOffsetY);
    QPointF adjustedEnd = end - QPointF(centerOffsetX, centerOffsetY);
    
    QPointF bufferStart = (adjustedStart / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
    QPointF bufferEnd = (adjustedEnd / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);

    painter.drawLine(bufferStart, bufferEnd);

    qreal updatePadding = eraserThickness / 2.0 + 5.0; // Half the eraser thickness plus some extra padding
    QRectF updateRect = QRectF(bufferStart, bufferEnd)
                        .normalized()
                        .adjusted(-updatePadding, -updatePadding, updatePadding, updatePadding);

    QRect scaledUpdateRect = QRect(
        ((updateRect.topLeft() - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0) + QPointF(centerOffsetX, centerOffsetY)).toPoint(),
        ((updateRect.bottomRight() - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0) + QPointF(centerOffsetX, centerOffsetY)).toPoint()
    );
    update(scaledUpdateRect);
}

void InkCanvas::setPenColor(const QColor &color) {
    penColor = color;
}

void InkCanvas::setPenThickness(qreal thickness) {
    // Set thickness for the current tool
    switch (currentTool) {
        case ToolType::Pen:
            penToolThickness = thickness;
            break;
        case ToolType::Marker:
            markerToolThickness = thickness;
            break;
        case ToolType::Eraser:
            eraserToolThickness = thickness;
            break;
    }
    
    // Update the current thickness for efficient drawing
    penThickness = thickness;
}

void InkCanvas::adjustAllToolThicknesses(qreal zoomRatio) {
    // Adjust thickness for all tools to maintain visual consistency
    penToolThickness *= zoomRatio;
    markerToolThickness *= zoomRatio;
    eraserToolThickness *= zoomRatio;
    
    // Update the current thickness based on the current tool
    switch (currentTool) {
        case ToolType::Pen:
            penThickness = penToolThickness;
            break;
        case ToolType::Marker:
            penThickness = markerToolThickness;
            break;
        case ToolType::Eraser:
            penThickness = eraserToolThickness;
            break;
    }
}

void InkCanvas::setTool(ToolType tool) {
    currentTool = tool;
    
    // Switch to the thickness for the new tool
    switch (currentTool) {
        case ToolType::Pen:
            penThickness = penToolThickness;
            break;
        case ToolType::Marker:
            penThickness = markerToolThickness;
            break;
        case ToolType::Eraser:
            penThickness = eraserToolThickness;
            break;
    }
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
    
    // Save markdown windows for this page
    if (markdownManager) {
        markdownManager->saveWindowsForPage(pageNumber);
    }
    
    // Update note cache with the saved buffer
    noteCache.insert(pageNumber, new QPixmap(buffer));
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

    // Hide any markdown windows from the previous page BEFORE loading the new page content.
    // This ensures the correct repaint area and stops the transparency timer.
    if (markdownManager) {
        markdownManager->hideAllWindows();
    }

    // Update current note page tracker
    currentCachedNotePage = pageNumber;

    // Check if note page is already cached
    bool loadedFromCache = false;
    if (noteCache.contains(pageNumber)) {
        // Use cached note page immediately
        buffer = *noteCache.object(pageNumber);
        loadedFromCache = true;
    } else {
        // Load note page from disk and cache it
        loadNotePageToCache(pageNumber);
        
        // Use the newly cached page or initialize buffer if loading failed
        if (noteCache.contains(pageNumber)) {
            buffer = *noteCache.object(pageNumber);
            loadedFromCache = true;
    } else {
        initializeBuffer(); // Clear the canvas if no file exists
        loadedFromCache = false;
    }
    }
    
    // Reset edited state when loading a new page
    edited = false;
    
    // Handle background image loading (PDF or custom background)
    if (isPdfLoaded && pdfDocument && pageNumber >= 0 && pageNumber < pdfDocument->numPages()) {
        // Use PDF as background (should already be cached by loadPdfPage)
        if (pdfCache.contains(pageNumber)) {
        backgroundImage = *pdfCache.object(pageNumber);
            
            // Resize canvas to match PDF page size if needed
        if (backgroundImage.size() != buffer.size()) {
            QPixmap newBuffer(backgroundImage.size());
            newBuffer.fill(Qt::transparent);

            // Copy existing drawings
            QPainter painter(&newBuffer);
            painter.drawPixmap(0, 0, buffer);

            buffer = newBuffer;
            setMaximumSize(backgroundImage.width(), backgroundImage.height());
        
                // Update cache with resized buffer
                noteCache.insert(pageNumber, new QPixmap(buffer));
            }
        }
    } else {
        // Handle custom background images
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
                
                // Update cache with resized buffer
                noteCache.insert(pageNumber, new QPixmap(buffer));
            }
        } else {
            backgroundImage = QPixmap(); // No background for this page
            // Only apply device pixel ratio fix if buffer was NOT loaded from cache
            // This prevents resizing cached buffers that might already be correctly sized
            if (!loadedFromCache) {
                QScreen *screen = QGuiApplication::primaryScreen();
                qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
                QSize logicalSize = screen ? screen->size() : QSize(1440, 900);
                QSize expectedPixelSize = logicalSize * dpr;
                
                if (buffer.size() != expectedPixelSize) {
                    // Buffer is wrong size, need to resize it properly
                    QPixmap newBuffer(expectedPixelSize);
                    newBuffer.fill(Qt::transparent);
                    
                    // Copy existing drawings if buffer was smaller
                    if (!buffer.isNull()) {
                        QPainter painter(&newBuffer);
                        painter.drawPixmap(0, 0, buffer);
                    }
                    
                    buffer = newBuffer;
                    setMaximumSize(expectedPixelSize);
                }
            }
        }
    }

    // Cache adjacent note pages after delay for faster navigation
    checkAndCacheAdjacentNotePages(pageNumber);

    update();
    adjustSize();            // Force the widget to update its size
    parentWidget()->update();
    
    // Load markdown windows AFTER canvas is fully rendered and sized
    // Use a single-shot timer to ensure the canvas is fully updated
    QTimer::singleShot(0, this, [this, pageNumber]() {
        if (markdownManager) {
            markdownManager->loadWindowsForPage(pageNumber);
        }
    });
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

    // Remove deleted page from note cache
    noteCache.remove(pageNumber);

    // Delete markdown windows for this page
    if (markdownManager) {
        markdownManager->deleteWindowsForPage(pageNumber);
    }

    if (pdfDocument){
        loadPdfPage(pageNumber);
    }
    else{
        loadPage(pageNumber);
    }

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
    int newZoom = qMax(10, qMin(zoomLevel, 400)); // Limit zoom to 10%-400%
    if (zoomFactor != newZoom) {
        zoomFactor = newZoom;
        internalZoomFactor = zoomFactor; // Sync internal zoom
        update();
        emit zoomChanged(zoomFactor);
    }
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

void InkCanvas::setPanX(int value) {
    if (panOffsetX != value) {
        panOffsetX = value;
        update();
        emit panChanged(panOffsetX, panOffsetY);
    }
}

void InkCanvas::setPanY(int value) {
    if (panOffsetY != value) {
        panOffsetY = value;
        update();
        emit panChanged(panOffsetX, panOffsetY);
    }
}

bool InkCanvas::isPdfLoadedFunc() const {
    return isPdfLoaded;
}

int InkCanvas::getTotalPdfPages() const {
    return totalPdfPages;
}

Poppler::Document* InkCanvas::getPdfDocument() const {
    return pdfDocument.get();
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

bool InkCanvas::event(QEvent *event) {
    if (!touchGesturesEnabled) {
        return QWidget::event(event);
    }

    if (event->type() == QEvent::TouchBegin || 
        event->type() == QEvent::TouchUpdate || 
        event->type() == QEvent::TouchEnd) {
        
        QTouchEvent *touchEvent = static_cast<QTouchEvent*>(event);
        const QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->points();
        
        activeTouchPoints = touchPoints.count();

        if (activeTouchPoints == 1) {
            // Single finger pan
            const QTouchEvent::TouchPoint &touchPoint = touchPoints.first();
            
            if (event->type() == QEvent::TouchBegin) {
                isPanning = true;
                lastTouchPos = touchPoint.position();
            } else if (event->type() == QEvent::TouchUpdate && isPanning) {
                QPointF delta = touchPoint.position() - lastTouchPos;
                
                // Make panning more responsive by using floating-point calculations
                qreal scaledDeltaX = delta.x() / (internalZoomFactor / 100.0);
                qreal scaledDeltaY = delta.y() / (internalZoomFactor / 100.0);
                
                // Calculate new pan positions with sub-pixel precision
                qreal newPanX = panOffsetX - scaledDeltaX;
                qreal newPanY = panOffsetY - scaledDeltaY;
                
                // Clamp pan values when canvas is smaller than viewport
                qreal scaledCanvasWidth = buffer.width() * (internalZoomFactor / 100.0);
                qreal scaledCanvasHeight = buffer.height() * (internalZoomFactor / 100.0);
                
                // If canvas is smaller than widget, lock pan to 0 (centered)
                if (scaledCanvasWidth < width()) {
                    newPanX = 0;
                }
                if (scaledCanvasHeight < height()) {
                    newPanY = 0;
                }
                
                // Emit signal with integer values for compatibility
                emit panChanged(qRound(newPanX), qRound(newPanY));
                
                lastTouchPos = touchPoint.position();
            }
        } else if (activeTouchPoints == 2) {
            // Two finger pinch zoom
            isPanning = false;
            
            const QTouchEvent::TouchPoint &touch1 = touchPoints[0];
            const QTouchEvent::TouchPoint &touch2 = touchPoints[1];
            
            // Calculate distance between touch points with higher precision
            qreal currentDist = QLineF(touch1.position(), touch2.position()).length();
            qreal startDist = QLineF(touch1.pressPosition(), touch2.pressPosition()).length();
            
            if (event->type() == QEvent::TouchBegin) {
                lastPinchScale = 1.0;
                // Store the starting internal zoom
                internalZoomFactor = zoomFactor;
            } else if (event->type() == QEvent::TouchUpdate && startDist > 0) {
                qreal scale = currentDist / startDist;
                
                // Use exponential scaling for more natural feel
                qreal scaleChange = scale / lastPinchScale;
                
                // Apply scale with higher sensitivity
                internalZoomFactor *= scaleChange;
                internalZoomFactor = qBound(10.0, internalZoomFactor, 400.0);
                
                // Calculate zoom center (midpoint between two fingers)
                QPointF center = (touch1.position() + touch2.position()) / 2.0;
                
                // Account for centering offset
                qreal scaledCanvasWidth = buffer.width() * (zoomFactor / 100.0);
                qreal scaledCanvasHeight = buffer.height() * (zoomFactor / 100.0);
                qreal centerOffsetX = (scaledCanvasWidth < width()) ? (width() - scaledCanvasWidth) / 2.0 : 0;
                qreal centerOffsetY = (scaledCanvasHeight < height()) ? (height() - scaledCanvasHeight) / 2.0 : 0;
                
                // Adjust center point for centering offset
                QPointF adjustedCenter = center - QPointF(centerOffsetX, centerOffsetY);
                
                // Always update zoom for smooth animation
                int newZoom = qRound(internalZoomFactor);
                
                // Calculate pan adjustment to keep the zoom centered at pinch point
                QPointF bufferCenter = adjustedCenter / (zoomFactor / 100.0) + QPointF(panOffsetX, panOffsetY);
                
                // Update zoom factor before emitting
                qreal oldZoomFactor = zoomFactor;
                zoomFactor = newZoom;
                
                // Emit zoom change even for small changes
                emit zoomChanged(newZoom);
                
                // Adjust pan to keep center point fixed with sub-pixel precision
                qreal newPanX = bufferCenter.x() - adjustedCenter.x() / (internalZoomFactor / 100.0);
                qreal newPanY = bufferCenter.y() - adjustedCenter.y() / (internalZoomFactor / 100.0);
                
                // After zoom, check if we need to center
                qreal newScaledCanvasWidth = buffer.width() * (internalZoomFactor / 100.0);
                qreal newScaledCanvasHeight = buffer.height() * (internalZoomFactor / 100.0);
                
                if (newScaledCanvasWidth < width()) {
                    newPanX = 0;
                }
                if (newScaledCanvasHeight < height()) {
                    newPanY = 0;
                }
                
                emit panChanged(qRound(newPanX), qRound(newPanY));
                
                lastPinchScale = scale;
                
                // Request update only for visible area
                update();
            }
        } else {
            // More than 2 fingers - ignore
            isPanning = false;
        }
        
        if (event->type() == QEvent::TouchEnd) {
            isPanning = false;
            lastPinchScale = 1.0;
            activeTouchPoints = 0;
            // Sync internal zoom with actual zoom
            internalZoomFactor = zoomFactor;
            
            // Emit signal that touch gesture has ended
            emit touchGestureEnded();
        }
        
        event->accept();
        return true;
    }
    
    return QWidget::event(event);
}

// Helper function to map LOGICAL widget coordinates to PHYSICAL buffer coordinates
QPointF InkCanvas::mapLogicalWidgetToPhysicalBuffer(const QPointF& logicalWidgetPoint) {
    // Use the same coordinate transformation logic as drawStroke for consistency
    qreal scaledCanvasWidth = buffer.width() * (zoomFactor / 100.0);
    qreal scaledCanvasHeight = buffer.height() * (zoomFactor / 100.0);
    qreal centerOffsetX = (scaledCanvasWidth < width()) ? (width() - scaledCanvasWidth) / 2.0 : 0;
    qreal centerOffsetY = (scaledCanvasHeight < height()) ? (height() - scaledCanvasHeight) / 2.0 : 0;
    
    // Convert widget position to buffer position, accounting for centering
    QPointF adjustedPoint = logicalWidgetPoint - QPointF(centerOffsetX, centerOffsetY);
    QPointF bufferPoint = (adjustedPoint / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
    
    return bufferPoint;
}

// Helper function to map a PHYSICAL buffer RECT to LOGICAL widget RECT for updates
QRect InkCanvas::mapRectBufferToWidgetLogical(const QRectF& physicalBufferRect) {
    // Use the same coordinate transformation logic as drawStroke for consistency
    qreal scaledCanvasWidth = buffer.width() * (zoomFactor / 100.0);
    qreal scaledCanvasHeight = buffer.height() * (zoomFactor / 100.0);
    qreal centerOffsetX = (scaledCanvasWidth < width()) ? (width() - scaledCanvasWidth) / 2.0 : 0;
    qreal centerOffsetY = (scaledCanvasHeight < height()) ? (height() - scaledCanvasHeight) / 2.0 : 0;
    
    // Convert buffer coordinates back to widget coordinates
    QRectF widgetRect = QRectF(
        (physicalBufferRect.topLeft() - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0) + QPointF(centerOffsetX, centerOffsetY),
        physicalBufferRect.size() * (zoomFactor / 100.0)
    );
    
    return widgetRect.toRect();
}

// Rope tool selection actions
void InkCanvas::deleteRopeSelection() {
    if (!selectionBuffer.isNull() && !selectionRect.isEmpty()) {
        // If the selection area hasn't been cleared from the buffer yet, clear it now for deletion
        if (!selectionAreaCleared && !selectionMaskPath.isEmpty()) {
            QPainter painter(&buffer);
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.fillPath(selectionMaskPath, Qt::transparent);
            painter.end();
        }
        
        // Clear the selection state
        selectionBuffer = QPixmap();
        selectionRect = QRect();
        exactSelectionRectF = QRectF();
        movingSelection = false;
        selectingWithRope = false;
        selectionJustCopied = false;
        selectionAreaCleared = false;
        selectionMaskPath = QPainterPath();
        selectionBufferRect = QRectF();
        
        // Mark as edited since we deleted content
        if (!edited) {
            edited = true;
            // Invalidate cache for current page since it's been modified
            invalidateCurrentPageCache();
        }
        
        // Update the entire canvas to remove selection visuals
        update();
    }
}

void InkCanvas::cancelRopeSelection() {
    if (!selectionBuffer.isNull() && !selectionRect.isEmpty()) {
        // Paste the selection back to its current location (where user moved it)
        QPainter painter(&buffer);
        // Explicitly set composition mode to draw on top of existing content
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        // Use the current selection position
        QPointF currentTopLeft = exactSelectionRectF.isEmpty() ? selectionRect.topLeft() : exactSelectionRectF.topLeft();
        QPointF bufferDest = mapLogicalWidgetToPhysicalBuffer(currentTopLeft);
        painter.drawPixmap(bufferDest.toPoint(), selectionBuffer);
        painter.end();
        
        // Store selection buffer size for update calculation before clearing it
        QSize selectionSize = selectionBuffer.size();
        QRect updateRect = QRect(currentTopLeft.toPoint(), selectionSize);
        
        // Clear the selection state
        selectionBuffer = QPixmap();
        selectionRect = QRect();
        exactSelectionRectF = QRectF();
        movingSelection = false;
        selectingWithRope = false;
        selectionJustCopied = false;
        selectionAreaCleared = false;
        selectionMaskPath = QPainterPath();
        selectionBufferRect = QRectF();
        
        // Update the restored area
        update(updateRect.adjusted(-5, -5, 5, 5));
    }
}

void InkCanvas::copyRopeSelection() {
    if (!selectionBuffer.isNull() && !selectionRect.isEmpty()) {
        // Get current selection position
        QPointF currentTopLeft = exactSelectionRectF.isEmpty() ? selectionRect.topLeft() : exactSelectionRectF.topLeft();
        
        // Calculate new position (right next to the original), but ensure it stays within canvas bounds
        QPointF newTopLeft = currentTopLeft + QPointF(selectionRect.width() + 5, 0); // 5 pixels gap
        
        // Check if the new position would extend beyond buffer boundaries and adjust if needed
        QPointF currentBufferDest = mapLogicalWidgetToPhysicalBuffer(currentTopLeft);
        QPointF newBufferDest = mapLogicalWidgetToPhysicalBuffer(newTopLeft);
        
        // Ensure the copy stays within buffer bounds
        if (newBufferDest.x() + selectionBuffer.width() > buffer.width()) {
            // If it would extend beyond right edge, place it to the left of the original
            newTopLeft = currentTopLeft - QPointF(selectionRect.width() + 5, 0);
            newBufferDest = mapLogicalWidgetToPhysicalBuffer(newTopLeft);
            
            // If it still doesn't fit on the left, place it below the original
            if (newBufferDest.x() < 0) {
                newTopLeft = currentTopLeft + QPointF(0, selectionRect.height() + 5);
                newBufferDest = mapLogicalWidgetToPhysicalBuffer(newTopLeft);
                
                // If it doesn't fit below either, place it above
                if (newBufferDest.y() + selectionBuffer.height() > buffer.height()) {
                    newTopLeft = currentTopLeft - QPointF(0, selectionRect.height() + 5);
                    newBufferDest = mapLogicalWidgetToPhysicalBuffer(newTopLeft);
                    
                    // If none of the positions work, just offset slightly and let it extend
                    if (newBufferDest.y() < 0) {
                        newTopLeft = currentTopLeft + QPointF(10, 10);
                        newBufferDest = mapLogicalWidgetToPhysicalBuffer(newTopLeft);
                    }
                }
            }
        }
        if (newBufferDest.y() + selectionBuffer.height() > buffer.height()) {
            // If it would extend beyond bottom edge, place it above the original
            newTopLeft = currentTopLeft - QPointF(0, selectionRect.height() + 5);
            newBufferDest = mapLogicalWidgetToPhysicalBuffer(newTopLeft);
        }
        
        // First, paste the selection back to its current location to restore the original
        QPainter painter(&buffer);
        // Explicitly set composition mode to draw on top of existing content
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawPixmap(currentBufferDest.toPoint(), selectionBuffer);
        
        // Then, paste the copy to the new location
        // We need to handle the case where the copy extends beyond buffer boundaries
        QRect targetRect(newBufferDest.toPoint(), selectionBuffer.size());
        QRect bufferBounds(0, 0, buffer.width(), buffer.height());
        QRect clippedRect = targetRect.intersected(bufferBounds);
        
        if (!clippedRect.isEmpty()) {
            // Calculate which part of the selectionBuffer to draw
            QRect sourceRect = QRect(
                clippedRect.x() - targetRect.x(),
                clippedRect.y() - targetRect.y(),
                clippedRect.width(),
                clippedRect.height()
            );
            
            painter.drawPixmap(clippedRect, selectionBuffer, sourceRect);
        }
        
        // For the selection buffer, we keep the FULL content, even if parts extend beyond canvas
        // This way when the user drags it back, the full content is preserved
        QPixmap newSelectionBuffer = selectionBuffer; // Keep the full original content
        
        // DON'T clear the copied area immediately - leave both original and copy on the canvas
        // The copy will only be cleared when the user actually starts moving the selection
        // This way, if the user cancels without moving, both original and copy remain permanently
        painter.end();
        
        // Update the selection buffer and position
        selectionBuffer = newSelectionBuffer;
        selectionRect = QRect(newTopLeft.toPoint(), selectionRect.size());
        exactSelectionRectF = QRectF(newTopLeft, selectionRect.size());
        selectionJustCopied = true; // Mark that this selection was just copied
        
        // Mark as edited since we added content
        if (!edited) {
            edited = true;
            // Invalidate cache for current page since it's been modified
            invalidateCurrentPageCache();
        }
        
        // Update the entire affected area (original + copy + gap)
        QRect updateArea = QRect(currentTopLeft.toPoint(), selectionRect.size())
                          .united(selectionRect)
                          .adjusted(-10, -10, 10, 10);
        update(updateArea);
    }
}

// PDF text selection implementation
void InkCanvas::clearPdfTextSelection() {
    // Clear selection state
    selectedTextBoxes.clear();
    pdfTextSelecting = false;
    
    // Cancel any pending throttled updates
    if (pdfTextSelectionTimer && pdfTextSelectionTimer->isActive()) {
        pdfTextSelectionTimer->stop();
    }
    hasPendingSelection = false;
    
    // Refresh display
    update();
}

QString InkCanvas::getSelectedPdfText() const {
    if (selectedTextBoxes.isEmpty()) {
        return QString();
    }
    
    // Pre-allocate string with estimated size for efficiency
    QString selectedText;
    selectedText.reserve(selectedTextBoxes.size() * 20); // Estimate ~20 chars per text box
    
    // Build text with space separators
    for (const Poppler::TextBox* textBox : selectedTextBoxes) {
        if (textBox && !textBox->text().isEmpty()) {
            if (!selectedText.isEmpty()) {
                selectedText += " ";
            }
            selectedText += textBox->text();
        }
    }
    
    return selectedText;
}

void InkCanvas::loadPdfTextBoxes(int pageNumber) {
    // Clear existing text boxes
    qDeleteAll(currentPdfTextBoxes);
    currentPdfTextBoxes.clear();
    
    if (!pdfDocument || pageNumber < 0 || pageNumber >= pdfDocument->numPages()) {
        return;
    }
    
    // Get the page for text operations
    currentPdfPageForText = std::unique_ptr<Poppler::Page>(pdfDocument->page(pageNumber));
    if (!currentPdfPageForText) {
        return;
    }
    
    // Load text boxes for the page
    auto textBoxVector = currentPdfPageForText->textList();
    currentPdfTextBoxes.clear();
    for (auto& textBox : textBoxVector) {
        currentPdfTextBoxes.append(textBox.release()); // Transfer ownership to QList
    }
}

QPointF InkCanvas::mapWidgetToPdfCoordinates(const QPointF &widgetPoint) {
    if (!currentPdfPageForText || backgroundImage.isNull()) {
        return QPointF();
    }
    
    // Use the same zoom factor as in paintEvent (internalZoomFactor for smooth animations)
    qreal zoom = internalZoomFactor / 100.0;
    
    // Calculate the scaled canvas size (same as in paintEvent)
    qreal scaledCanvasWidth = buffer.width() * zoom;
    qreal scaledCanvasHeight = buffer.height() * zoom;
    
    // Calculate centering offsets (same as in paintEvent)
    qreal centerOffsetX = 0;
    qreal centerOffsetY = 0;
    
    // Center horizontally if canvas is smaller than widget
    if (scaledCanvasWidth < width()) {
        centerOffsetX = (width() - scaledCanvasWidth) / 2.0;
    }
    
    // Center vertically if canvas is smaller than widget
    if (scaledCanvasHeight < height()) {
        centerOffsetY = (height() - scaledCanvasHeight) / 2.0;
    }
    
    // Reverse the transformations applied in paintEvent
    QPointF adjustedPoint = widgetPoint;
    adjustedPoint -= QPointF(centerOffsetX, centerOffsetY);
    adjustedPoint /= zoom;
    adjustedPoint += QPointF(panOffsetX, panOffsetY);
    
    // Convert to PDF coordinates
    // Since Poppler renders the PDF in Qt coordinate system (top-left origin),
    // we don't need to flip the Y coordinate
    QSizeF pdfPageSize = currentPdfPageForText->pageSizeF();
    QSizeF imageSize = backgroundImage.size();
    
    // Scale from image coordinates to PDF coordinates
    qreal scaleX = pdfPageSize.width() / imageSize.width();
    qreal scaleY = pdfPageSize.height() / imageSize.height();
    
    QPointF pdfPoint;
    pdfPoint.setX(adjustedPoint.x() * scaleX);
    pdfPoint.setY(adjustedPoint.y() * scaleY); // No Y-axis flipping needed
    
    return pdfPoint;
}

QPointF InkCanvas::mapPdfToWidgetCoordinates(const QPointF &pdfPoint) {
    if (!currentPdfPageForText || backgroundImage.isNull()) {
        return QPointF();
    }
    
    // Convert from PDF coordinates to image coordinates
    QSizeF pdfPageSize = currentPdfPageForText->pageSizeF();
    QSizeF imageSize = backgroundImage.size();
    
    // Scale from PDF coordinates to image coordinates
    qreal scaleX = imageSize.width() / pdfPageSize.width();
    qreal scaleY = imageSize.height() / pdfPageSize.height();
    
    QPointF imagePoint;
    imagePoint.setX(pdfPoint.x() * scaleX);
    imagePoint.setY(pdfPoint.y() * scaleY); // No Y-axis flipping needed
    
    // Use the same zoom factor as in paintEvent (internalZoomFactor for smooth animations)
    qreal zoom = internalZoomFactor / 100.0;
    
    // Apply the same transformations as in paintEvent
    QPointF widgetPoint = imagePoint;
    widgetPoint -= QPointF(panOffsetX, panOffsetY);
    widgetPoint *= zoom;
    
    // Calculate the scaled canvas size (same as in paintEvent)
    qreal scaledCanvasWidth = buffer.width() * zoom;
    qreal scaledCanvasHeight = buffer.height() * zoom;
    
    // Calculate centering offsets (same as in paintEvent)
    qreal centerOffsetX = 0;
    qreal centerOffsetY = 0;
    
    // Center horizontally if canvas is smaller than widget
    if (scaledCanvasWidth < width()) {
        centerOffsetX = (width() - scaledCanvasWidth) / 2.0;
    }
    
    // Center vertically if canvas is smaller than widget
    if (scaledCanvasHeight < height()) {
        centerOffsetY = (height() - scaledCanvasHeight) / 2.0;
    }
    
    widgetPoint += QPointF(centerOffsetX, centerOffsetY);
    
    return widgetPoint;
}

void InkCanvas::updatePdfTextSelection(const QPointF &start, const QPointF &end) {
    // Early return if PDF is not loaded or no text boxes available
    if (!isPdfLoaded || currentPdfTextBoxes.isEmpty()) {
        return;
    }
    
    // Clear previous selection efficiently
    selectedTextBoxes.clear();
    
    // Create normalized selection rectangle in widget coordinates
    QRectF widgetSelectionRect(start, end);
    widgetSelectionRect = widgetSelectionRect.normalized();
    
    // Convert to PDF coordinate space
    QPointF pdfTopLeft = mapWidgetToPdfCoordinates(widgetSelectionRect.topLeft());
    QPointF pdfBottomRight = mapWidgetToPdfCoordinates(widgetSelectionRect.bottomRight());
    QRectF pdfSelectionRect(pdfTopLeft, pdfBottomRight);
    pdfSelectionRect = pdfSelectionRect.normalized();
    
    // Reserve space for efficiency if we expect many selections
    selectedTextBoxes.reserve(qMin(currentPdfTextBoxes.size(), 50));
    
    // Find intersecting text boxes with optimized loop
    for (const Poppler::TextBox* textBox : currentPdfTextBoxes) {
        if (textBox && textBox->boundingBox().intersects(pdfSelectionRect)) {
            selectedTextBoxes.append(const_cast<Poppler::TextBox*>(textBox));
        }
    }
    
    // Only emit signal and update if we have selected text
    if (!selectedTextBoxes.isEmpty()) {
        QString selectedText = getSelectedPdfText();
        if (!selectedText.isEmpty()) {
            emit pdfTextSelected(selectedText);
        }
    }
    
    // Trigger repaint to show selection
    update();
}

QList<Poppler::TextBox*> InkCanvas::getTextBoxesInSelection(const QPointF &start, const QPointF &end) {
    QList<Poppler::TextBox*> selectedBoxes;
    
    if (!currentPdfPageForText) {
        // qDebug() << "PDF text selection: No current page for text";
        return selectedBoxes;
    }
    
    // Convert widget coordinates to PDF coordinates
    QPointF pdfStart = mapWidgetToPdfCoordinates(start);
    QPointF pdfEnd = mapWidgetToPdfCoordinates(end);
    
    // qDebug() << "PDF text selection: Widget coords" << start << "to" << end;
    // qDebug() << "PDF text selection: PDF coords" << pdfStart << "to" << pdfEnd;
    
    // Create selection rectangle in PDF coordinates
    QRectF selectionRect(pdfStart, pdfEnd);
    selectionRect = selectionRect.normalized();
    
    // qDebug() << "PDF text selection: Selection rect in PDF coords:" << selectionRect;
    
    // Find text boxes that intersect with the selection
    int intersectionCount = 0;
    for (Poppler::TextBox* textBox : currentPdfTextBoxes) {
        if (textBox) {
            QRectF textBoxRect = textBox->boundingBox();
            bool intersects = textBoxRect.intersects(selectionRect);
            
            if (intersects) {
                selectedBoxes.append(textBox);
                intersectionCount++;
                // qDebug() << "PDF text selection: Text box intersects:" << textBox->text() 
                //          << "at" << textBoxRect;
            }
        }
    }
    
    // qDebug() << "PDF text selection: Found" << intersectionCount << "intersecting text boxes";
    
    return selectedBoxes;
}

void InkCanvas::handlePdfLinkClick(const QPointF &position) {
    if (!isPdfLoaded || !currentPdfPageForText) {
        return;
    }
    
    // Convert widget coordinates to PDF coordinates
    QPointF pdfPoint = mapWidgetToPdfCoordinates(position);
    
    // Get PDF page size for reference
    QSizeF pdfPageSize = currentPdfPageForText->pageSizeF();
    
    // Convert to normalized coordinates (0.0 to 1.0) to match Poppler's link coordinate system
    QPointF normalizedPoint(pdfPoint.x() / pdfPageSize.width(), pdfPoint.y() / pdfPageSize.height());
    
    // Get links for the current page
    auto links = currentPdfPageForText->links();
    
    for (const auto& link : links) {
        QRectF linkArea = link->linkArea();
        
        // Normalize the rectangle to handle negative width/height
        QRectF normalizedLinkArea = linkArea.normalized();
        
        // Check if the normalized rectangle contains the normalized point
        if (normalizedLinkArea.contains(normalizedPoint)) {
            // Handle different types of links
            if (link->linkType() == Poppler::Link::Goto) {
                Poppler::LinkGoto* gotoLink = static_cast<Poppler::LinkGoto*>(link.get());
                if (gotoLink && gotoLink->destination().pageNumber() >= 0) {
                    int targetPage = gotoLink->destination().pageNumber() - 1; // Convert to 0-based
                    emit pdfLinkClicked(targetPage);
                    return;
                }
            }
            // Add other link types as needed (URI, etc.)
        }
    }
}

void InkCanvas::showPdfTextSelectionMenu(const QPoint &position) {
    QString selectedText = getSelectedPdfText();
    if (selectedText.isEmpty()) {
        return; // No text selected, don't show menu
    }
    
    // Create context menu
    QMenu *contextMenu = new QMenu(this);
    contextMenu->setAttribute(Qt::WA_DeleteOnClose);
    
    // Add Copy action
    QAction *copyAction = contextMenu->addAction(tr("Copy"));
    copyAction->setIcon(QIcon(":/resources/icons/copy.png")); // You may need to add this icon
    connect(copyAction, &QAction::triggered, this, [selectedText]() {
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setText(selectedText);
    });
    
    // Add separator
    contextMenu->addSeparator();
    
    // Add Cancel action
    QAction *cancelAction = contextMenu->addAction(tr("Cancel"));
    cancelAction->setIcon(QIcon(":/resources/icons/cross.png"));
    connect(cancelAction, &QAction::triggered, this, [this]() {
        clearPdfTextSelection();
    });
    
    // Show the menu at the specified position
    contextMenu->popup(position);
}

void InkCanvas::processPendingTextSelection() {
    if (!hasPendingSelection) {
        return;
    }
    
    // Process the pending selection update
    updatePdfTextSelection(pendingSelectionStart, pendingSelectionEnd);
    hasPendingSelection = false;
}

// Intelligent PDF Cache System Implementation

bool InkCanvas::isValidPageNumber(int pageNumber) const {
    return (pageNumber >= 0 && pageNumber < totalPdfPages);
}

void InkCanvas::renderPdfPageToCache(int pageNumber) {
    if (!pdfDocument || !isValidPageNumber(pageNumber)) {
        return;
    }
    
    // Check if already cached
    if (pdfCache.contains(pageNumber)) {
        return;
    }
    
    // Ensure the cache holds only 10 pages max
    if (pdfCache.count() >= 10) {
        auto oldestKey = pdfCache.keys().first();
        pdfCache.remove(oldestKey);
    }
    
    // Render the page and store it in the cache
    std::unique_ptr<Poppler::Page> page(pdfDocument->page(pageNumber));
    if (page) {
        // Render with document-level anti-aliasing settings
        QImage pdfImage = page->renderToImage(pdfRenderDPI, pdfRenderDPI);
        if (!pdfImage.isNull()) {
            QPixmap cachedPixmap = QPixmap::fromImage(pdfImage);
            pdfCache.insert(pageNumber, new QPixmap(cachedPixmap));
        }
    }
}

void InkCanvas::checkAndCacheAdjacentPages(int targetPage) {
    if (!pdfDocument || !isValidPageNumber(targetPage)) {
        return;
    }
    
    // Calculate adjacent pages
    int prevPage = targetPage - 1;
    int nextPage = targetPage + 1;
    
    // Check what needs to be cached
    bool needPrevPage = isValidPageNumber(prevPage) && !pdfCache.contains(prevPage);
    bool needCurrentPage = !pdfCache.contains(targetPage);
    bool needNextPage = isValidPageNumber(nextPage) && !pdfCache.contains(nextPage);
    
    // If all pages are cached, nothing to do
    if (!needPrevPage && !needCurrentPage && !needNextPage) {
        return;
    }
    
    // Stop any existing timer
    if (pdfCacheTimer && pdfCacheTimer->isActive()) {
        pdfCacheTimer->stop();
    }
    
    // Create timer if it doesn't exist
    if (!pdfCacheTimer) {
        pdfCacheTimer = new QTimer(this);
        pdfCacheTimer->setSingleShot(true);
        connect(pdfCacheTimer, &QTimer::timeout, this, &InkCanvas::cacheAdjacentPages);
    }
    
    // Store the target page for validation when timer fires
    pendingCacheTargetPage = targetPage;
    
    // Start 1-second delay timer
    pdfCacheTimer->start(1000);
}

void InkCanvas::cacheAdjacentPages() {
    if (!pdfDocument || currentCachedPage < 0) {
        return;
    }
    
    // Check if the user has moved to a different page since the timer was started
    // If so, skip caching adjacent pages as they're no longer relevant
    if (pendingCacheTargetPage != currentCachedPage) {
        return; // User switched pages, don't cache adjacent pages for old page
    }
    
    int targetPage = currentCachedPage;
    int prevPage = targetPage - 1;
    int nextPage = targetPage + 1;
    
    // Create list of pages to cache asynchronously
    QList<int> pagesToCache;
    
    // Add previous page if needed
    if (isValidPageNumber(prevPage) && !pdfCache.contains(prevPage)) {
        pagesToCache.append(prevPage);
    }
    
    // Add next page if needed
    if (isValidPageNumber(nextPage) && !pdfCache.contains(nextPage)) {
        pagesToCache.append(nextPage);
    }
    
    // Cache pages asynchronously
    for (int pageNum : pagesToCache) {
        QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
        
        // Track the watcher for cleanup
        activePdfWatchers.append(watcher);
        
        connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
            // Remove from active list and delete
            activePdfWatchers.removeOne(watcher);
            watcher->deleteLater();
        });
        
        // Capture pageNum by value to avoid issues with lambda capture
        QFuture<void> future = QtConcurrent::run([this, pageNum]() {
            renderPdfPageToCache(pageNum);
        });
        
        watcher->setFuture(future);
    }
}

// Intelligent Note Cache System Implementation

QString InkCanvas::getNotePageFilePath(int pageNumber) const {
    if (saveFolder.isEmpty() || notebookId.isEmpty()) {
        return QString();
    }
    return saveFolder + QString("/%1_%2.png").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));
}

void InkCanvas::loadNotePageToCache(int pageNumber) {
    // Check if already cached
    if (noteCache.contains(pageNumber)) {
        return;
    }
    
    QString filePath = getNotePageFilePath(pageNumber);
    if (filePath.isEmpty()) {
        return;
    }
    
    // Ensure the cache doesn't exceed its limit
    if (noteCache.count() >= 15) {
        // QCache will automatically remove least recently used items
        // but we can be explicit about it
        auto keys = noteCache.keys();
        if (!keys.isEmpty()) {
            noteCache.remove(keys.first());
        }
    }
    
    // Load note page from disk if it exists
    if (QFile::exists(filePath)) {
        QPixmap notePixmap;
        if (notePixmap.load(filePath)) {
            noteCache.insert(pageNumber, new QPixmap(notePixmap));
        }
    }
    // If file doesn't exist, we don't cache anything - loadPage will handle initialization
}

void InkCanvas::checkAndCacheAdjacentNotePages(int targetPage) {
    if (saveFolder.isEmpty()) {
        return;
    }
    
    // Calculate adjacent pages
    int prevPage = targetPage - 1;
    int nextPage = targetPage + 1;
    
    // Check what needs to be cached (we don't have a max page limit for notes)
    bool needPrevPage = (prevPage >= 0) && !noteCache.contains(prevPage);
    bool needCurrentPage = !noteCache.contains(targetPage);
    bool needNextPage = !noteCache.contains(nextPage); // No upper limit check for notes
    
    // If all nearby pages are cached, nothing to do
    if (!needPrevPage && !needCurrentPage && !needNextPage) {
        return;
    }
    
    // Stop any existing timer
    if (noteCacheTimer && noteCacheTimer->isActive()) {
        noteCacheTimer->stop();
    }
    
    // Create timer if it doesn't exist
    if (!noteCacheTimer) {
        noteCacheTimer = new QTimer(this);
        noteCacheTimer->setSingleShot(true);
        connect(noteCacheTimer, &QTimer::timeout, this, &InkCanvas::cacheAdjacentNotePages);
    }
    
    // Store the target page for validation when timer fires
    pendingNoteCacheTargetPage = targetPage;
    
    // Start 1-second delay timer
    noteCacheTimer->start(1000);
}

void InkCanvas::cacheAdjacentNotePages() {
    if (saveFolder.isEmpty() || currentCachedNotePage < 0) {
        return;
    }
    
    // Check if the user has moved to a different page since the timer was started
    // If so, skip caching adjacent pages as they're no longer relevant
    if (pendingNoteCacheTargetPage != currentCachedNotePage) {
        return; // User switched pages, don't cache adjacent pages for old page
    }
    
    int targetPage = currentCachedNotePage;
    int prevPage = targetPage - 1;
    int nextPage = targetPage + 1;
    
    // Create list of note pages to cache asynchronously
    QList<int> notePagesToCache;
    
    // Add previous page if needed (check for >= 0 since notes can start from page 0)
    if (prevPage >= 0 && !noteCache.contains(prevPage)) {
        notePagesToCache.append(prevPage);
    }
    
    // Add next page if needed (no upper limit check for notes)
    if (!noteCache.contains(nextPage)) {
        notePagesToCache.append(nextPage);
    }
    
    // Cache note pages asynchronously
    for (int pageNum : notePagesToCache) {
        QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
        
        // Track the watcher for cleanup
        activeNoteWatchers.append(watcher);
        
        connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
            // Remove from active list and delete
            activeNoteWatchers.removeOne(watcher);
            watcher->deleteLater();
        });
        
        // Capture pageNum by value to avoid issues with lambda capture
        QFuture<void> future = QtConcurrent::run([this, pageNum]() {
            loadNotePageToCache(pageNum);
        });
        
        watcher->setFuture(future);
    }
}

void InkCanvas::invalidateCurrentPageCache() {
    if (currentCachedNotePage >= 0) {
        noteCache.remove(currentCachedNotePage);
    }
}

// Markdown integration methods
void InkCanvas::setMarkdownSelectionMode(bool enabled) {
    markdownSelectionMode = enabled;
    
    if (markdownManager) {
        markdownManager->setSelectionMode(enabled);
    }
    
    if (!enabled) {
        markdownSelecting = false;
    }
    
    // Update cursor
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
    
    // Notify signal
    emit markdownSelectionModeChanged(enabled);
}

bool InkCanvas::isMarkdownSelectionMode() const {
    return markdownSelectionMode;
}

// Canvas coordinate conversion methods
QPointF InkCanvas::mapWidgetToCanvas(const QPointF &widgetPoint) const {
    // Use the same coordinate transformation logic as mapLogicalWidgetToPhysicalBuffer
    qreal scaledCanvasWidth = buffer.width() * (zoomFactor / 100.0);
    qreal scaledCanvasHeight = buffer.height() * (zoomFactor / 100.0);
    qreal centerOffsetX = (scaledCanvasWidth < width()) ? (width() - scaledCanvasWidth) / 2.0 : 0;
    qreal centerOffsetY = (scaledCanvasHeight < height()) ? (height() - scaledCanvasHeight) / 2.0 : 0;
    
    // Convert widget position to canvas position
    QPointF adjustedPoint = widgetPoint - QPointF(centerOffsetX, centerOffsetY);
    QPointF canvasPoint = (adjustedPoint / (zoomFactor / 100.0)) + QPointF(panOffsetX, panOffsetY);
    
    return canvasPoint;
}

QPointF InkCanvas::mapCanvasToWidget(const QPointF &canvasPoint) const {
    // Reverse the transformation from mapWidgetToCanvas
    qreal scaledCanvasWidth = buffer.width() * (zoomFactor / 100.0);
    qreal scaledCanvasHeight = buffer.height() * (zoomFactor / 100.0);
    qreal centerOffsetX = (scaledCanvasWidth < width()) ? (width() - scaledCanvasWidth) / 2.0 : 0;
    qreal centerOffsetY = (scaledCanvasHeight < height()) ? (height() - scaledCanvasHeight) / 2.0 : 0;
    
    // Convert canvas position to widget position
    QPointF widgetPoint = (canvasPoint - QPointF(panOffsetX, panOffsetY)) * (zoomFactor / 100.0) + QPointF(centerOffsetX, centerOffsetY);
    
    return widgetPoint;
}

QRect InkCanvas::mapWidgetToCanvas(const QRect &widgetRect) const {
    QPointF topLeft = mapWidgetToCanvas(widgetRect.topLeft());
    QPointF bottomRight = mapWidgetToCanvas(widgetRect.bottomRight());
    
    return QRect(topLeft.toPoint(), bottomRight.toPoint());
}

QRect InkCanvas::mapCanvasToWidget(const QRect &canvasRect) const {
    QPointF topLeft = mapCanvasToWidget(canvasRect.topLeft());
    QPointF bottomRight = mapCanvasToWidget(canvasRect.bottomRight());
    
    return QRect(topLeft.toPoint(), bottomRight.toPoint());
}





