#include "MarkdownWindow.h"
#include "../markdown/qmarkdowntextedit.h"
#include "InkCanvas.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QVariantMap>
#include <QApplication>
#include <QStyle>
#include <QDebug>

MarkdownWindow::MarkdownWindow(const QRect &rect, QWidget *parent)
    : QWidget(parent)
{
    // Initialize canvas coordinates - assume the rect passed is in canvas coordinates
    canvasRect = rect;
    // qDebug() << "MarkdownWindow::MarkdownWindow() - Initial canvas rect:" << canvasRect;
    
    setupUI();
    applyStyle();
    
    // ✅ PERFORMANCE: Set up throttle timer for pan updates
    updateThrottleTimer = new QTimer(this);
    updateThrottleTimer->setSingleShot(true);
    updateThrottleTimer->setInterval(16); // ~60 FPS maximum update rate
    connect(updateThrottleTimer, &QTimer::timeout, this, [this]() {
        if (hasPendingUpdate) {
            hasPendingUpdate = false;
            updateScreenPositionImmediate();
        }
    });
    
    // Set initial screen position based on canvas coordinates
    updateScreenPositionImmediate();
    
    // Enable mouse tracking for resize handles
    setMouseTracking(true);
    
    // Set window flags for proper behavior
    setWindowFlags(Qt::Widget);
    setAttribute(Qt::WA_DeleteOnClose, false); // We'll handle deletion manually
    
    // Connect to canvas pan/zoom changes to update position
    if (InkCanvas *inkCanvas = qobject_cast<InkCanvas*>(parentWidget())) {
        connect(inkCanvas, &InkCanvas::panChanged, this, &MarkdownWindow::updateScreenPosition);
        connect(inkCanvas, &InkCanvas::zoomChanged, this, &MarkdownWindow::updateScreenPosition);
    }
    
    // Connect to parent widget resize events to handle layout changes
    if (parentWidget()) {
        parentWidget()->installEventFilter(this);
    }
}

MarkdownWindow::~MarkdownWindow() = default;

void MarkdownWindow::setupUI() {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(0);
    
    // Header with title and delete button
    headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(4);
    
    titleLabel = new QLabel("Markdown", this);
    titleLabel->setStyleSheet("font-weight: bold; color: #333;");
    
    deleteButton = new QPushButton("×", this);
    deleteButton->setFixedSize(16, 16);
    deleteButton->setStyleSheet(R"(
        QPushButton {
            background-color: #ff4444;
            color: white;
            border: none;
            border-radius: 8px;
            font-weight: bold;
            font-size: 10px;
        }
        QPushButton:hover {
            background-color: #ff6666;
        }
        QPushButton:pressed {
            background-color: #cc2222;
        }
    )");
    
    connect(deleteButton, &QPushButton::clicked, this, &MarkdownWindow::onDeleteClicked);
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(deleteButton);
    
    // Markdown editor
    markdownEditor = new QMarkdownTextEdit(this);
    markdownEditor->setPlainText(""); // Start with empty content
    
    connect(markdownEditor, &QMarkdownTextEdit::textChanged, this, &MarkdownWindow::onMarkdownTextChanged);
    
    // Connect editor focus events using event filter
    markdownEditor->installEventFilter(this);
    
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(markdownEditor);
}

void MarkdownWindow::applyStyle() {
    // Detect dark mode using the same method as MainWindow
    bool isDarkMode = palette().color(QPalette::Window).lightness() < 128;
    
    QString backgroundColor = isDarkMode ? "#2b2b2b" : "white";
    QString borderColor = isDarkMode ? "#555555" : "#cccccc";
    QString headerBackgroundColor = isDarkMode ? "#3c3c3c" : "#f0f0f0";
    QString focusBorderColor = isDarkMode ? "#6ca9dc" : "#4a90e2";
    
    setStyleSheet(QString(R"(
        MarkdownWindow {
            background-color: %1;
            border: 2px solid %2;
            border-radius: 4px;
        }
        MarkdownWindow:focus {
            border-color: %3;
        }
    )").arg(backgroundColor, borderColor, focusBorderColor));
    
    // Style the header
    if (headerLayout && headerLayout->parentWidget()) {
        headerLayout->parentWidget()->setStyleSheet(QString(R"(
            background-color: %1;
            border-bottom: 1px solid %2;
        )").arg(headerBackgroundColor, borderColor));
    }
    
    // Set the markdown editor to have an explicit opaque background
    if (markdownEditor) {
        QString editorBg = isDarkMode ? "#2b2b2b" : "white";
        markdownEditor->setStyleSheet(QString(R"(
            QMarkdownTextEdit {
                background-color: %1;
                border: none;
            }
        )").arg(editorBg));
    }
}

QString MarkdownWindow::getMarkdownContent() const {
    return markdownEditor ? markdownEditor->toPlainText() : QString();
}

void MarkdownWindow::setMarkdownContent(const QString &content) {
    if (markdownEditor) {
        markdownEditor->setPlainText(content);
    }
}

QRect MarkdownWindow::getWindowRect() const {
    return geometry();
}

void MarkdownWindow::setWindowRect(const QRect &rect) {
    setGeometry(rect);
}

QRect MarkdownWindow::getCanvasRect() const {
    return canvasRect;
}

void MarkdownWindow::setCanvasRect(const QRect &rect) {
    canvasRect = rect;
    updateScreenPosition();
}

void MarkdownWindow::updateScreenPosition() {
    // ✅ PERFORMANCE: Throttle updates during frequent pan operations
    if (!updateThrottleTimer->isActive()) {
        // If not currently throttling, update immediately and start throttling
        updateScreenPositionImmediate();
        updateThrottleTimer->start();
    } else {
        // If throttling is active, just mark that we have a pending update
        hasPendingUpdate = true;
    }
}

void MarkdownWindow::updateScreenPositionImmediate() {
    // Prevent recursive updates
    if (isUpdatingPosition) return;
    isUpdatingPosition = true;
    
    // Debug: Log when this is called due to external changes (not during mouse movement)
    if (!dragging && !resizing) {
        // qDebug() << "MarkdownWindow::updateScreenPositionImmediate() called for window" << this << "Canvas rect:" << canvasRect;
    }
    
    // Get the canvas parent to access coordinate conversion methods
    if (QWidget *canvas = parentWidget()) {
        // Try to cast to InkCanvas to use the new coordinate methods
        if (InkCanvas *inkCanvas = qobject_cast<InkCanvas*>(canvas)) {
            // Use the new coordinate conversion methods
            QRect screenRect = inkCanvas->mapCanvasToWidget(canvasRect);
            // qDebug() << "MarkdownWindow::updateScreenPositionImmediate() - Canvas rect:" << canvasRect << "-> Screen rect:" << screenRect;
            // qDebug() << "  Canvas size:" << inkCanvas->getCanvasSize() << "Zoom:" << inkCanvas->getZoomFactor() << "Pan:" << inkCanvas->getPanOffset();
            setGeometry(screenRect);
        } else {
            // Fallback: use canvas coordinates directly
            // qDebug() << "MarkdownWindow::updateScreenPositionImmediate() - No InkCanvas parent, using canvas rect directly:" << canvasRect;
            setGeometry(canvasRect);
        }
    } else {
        // No parent, use canvas coordinates directly
        // qDebug() << "MarkdownWindow::updateScreenPositionImmediate() - No parent, using canvas rect directly:" << canvasRect;
        setGeometry(canvasRect);
    }
    
    isUpdatingPosition = false;
}

QVariantMap MarkdownWindow::serialize() const {
    QVariantMap data;
    data["canvas_x"] = canvasRect.x();
    data["canvas_y"] = canvasRect.y();
    data["canvas_width"] = canvasRect.width();
    data["canvas_height"] = canvasRect.height();
    data["content"] = getMarkdownContent();
    
    // Add canvas size information for debugging and consistency checks
    if (InkCanvas *inkCanvas = qobject_cast<InkCanvas*>(parentWidget())) {
        QSize canvasSize = inkCanvas->getCanvasSize();
        data["canvas_buffer_width"] = canvasSize.width();
        data["canvas_buffer_height"] = canvasSize.height();
        data["zoom_factor"] = inkCanvas->getZoomFactor();
        QPointF panOffset = inkCanvas->getPanOffset();
        data["pan_x"] = panOffset.x();
        data["pan_y"] = panOffset.y();
    }
    
    return data;
}

void MarkdownWindow::deserialize(const QVariantMap &data) {
    int x = data.value("canvas_x", 0).toInt();
    int y = data.value("canvas_y", 0).toInt();
    int width = data.value("canvas_width", 300).toInt();
    int height = data.value("canvas_height", 200).toInt();
    QString content = data.value("content", "# New Markdown Window").toString();
    
    QRect loadedRect = QRect(x, y, width, height);
    // qDebug() << "MarkdownWindow::deserialize() - Loaded canvas rect:" << loadedRect;
    // qDebug() << "  Original canvas size:" << data.value("canvas_buffer_width", -1).toInt() << "x" << data.value("canvas_buffer_height", -1).toInt();
    
    // Apply bounds checking to loaded coordinates
    if (InkCanvas *inkCanvas = qobject_cast<InkCanvas*>(parentWidget())) {
        QRect canvasBounds = inkCanvas->getCanvasRect();
        
        // Ensure window stays within canvas bounds
        int maxX = canvasBounds.width() - loadedRect.width();
        int maxY = canvasBounds.height() - loadedRect.height();
        
        loadedRect.setX(qMax(0, qMin(loadedRect.x(), maxX)));
        loadedRect.setY(qMax(0, qMin(loadedRect.y(), maxY)));
        
        if (loadedRect != QRect(x, y, width, height)) {
            // qDebug() << "  Bounds-corrected canvas rect:" << loadedRect << "Canvas bounds:" << canvasBounds;
        }
    }
    
    canvasRect = loadedRect;
    setMarkdownContent(content);
    
    // Ensure canvas connections are set up
    ensureCanvasConnections();
    
    updateScreenPositionImmediate();
}

void MarkdownWindow::focusEditor() {
    if (markdownEditor) {
        markdownEditor->setFocus();
    }
}

bool MarkdownWindow::isEditorFocused() const {
    return markdownEditor && markdownEditor->hasFocus();
}

void MarkdownWindow::onDeleteClicked() {
    emit deleteRequested(this);
}

void MarkdownWindow::onMarkdownTextChanged() {
    // Note: This is called on every keystroke, so we don't print debug messages here
    // to avoid spamming the console. The actual content updates are debounced.
    emit contentChanged();
}

void MarkdownWindow::setTransparent(bool transparent) {
    if (isTransparentState == transparent) return;
    
    // qDebug() << "MarkdownWindow::setTransparent(" << transparent << ") for window" << this;
    isTransparentState = transparent;
    
    // Apply transparency effect by changing the background style
    if (transparent) {
        // qDebug() << "Setting window background to semi-transparent";
        applyTransparentStyle();
    } else {
        // qDebug() << "Setting window background to opaque";
        applyStyle(); // Restore normal style
    }
}

void MarkdownWindow::applyTransparentStyle() {
    // Detect dark mode using the same method as MainWindow
    bool isDarkMode = palette().color(QPalette::Window).lightness() < 128;
    
    QString backgroundColor = isDarkMode ? "rgba(43, 43, 43, 0.3)" : "rgba(255, 255, 255, 0.3)"; // 30% opacity
    QString borderColor = isDarkMode ? "#555555" : "#cccccc";
    QString headerBackgroundColor = isDarkMode ? "rgba(60, 60, 60, 0.3)" : "rgba(240, 240, 240, 0.3)"; // 30% opacity
    QString focusBorderColor = isDarkMode ? "#6ca9dc" : "#4a90e2";
    
    setStyleSheet(QString(R"(
        MarkdownWindow {
            background-color: %1;
            border: 2px solid %2;
            border-radius: 4px;
        }
        MarkdownWindow:focus {
            border-color: %3;
        }
    )").arg(backgroundColor, borderColor, focusBorderColor));
    
    // Style the header with transparency
    if (headerLayout && headerLayout->parentWidget()) {
        headerLayout->parentWidget()->setStyleSheet(QString(R"(
            background-color: %1;
            border-bottom: 1px solid %2;
        )").arg(headerBackgroundColor, borderColor));
    }
    
    // Make the markdown editor background semi-transparent too
    if (markdownEditor) {
        QString editorBg = isDarkMode ? "rgba(43, 43, 43, 0.3)" : "rgba(255, 255, 255, 0.3)";
        markdownEditor->setStyleSheet(QString(R"(
            QMarkdownTextEdit {
                background-color: %1;
                border: none;
            }
        )").arg(editorBg));
    }
}

bool MarkdownWindow::isTransparent() const {
    return isTransparentState;
}

void MarkdownWindow::setFrameOnlyMode(bool enabled) {
    if (frameOnlyMode == enabled) return;
    
    frameOnlyMode = enabled;
    
    if (enabled) {
        // Hide all child widgets to show only the frame
        if (markdownEditor) markdownEditor->hide();
        if (titleLabel) titleLabel->hide();
        if (deleteButton) deleteButton->hide();
    } else {
        // Restore all child widgets
        if (markdownEditor) markdownEditor->show();
        if (titleLabel) titleLabel->show();
        if (deleteButton) deleteButton->show();
    }
    
    update(); // Trigger repaint to show/hide content
}

bool MarkdownWindow::isFrameOnlyMode() const {
    return frameOnlyMode;
}

bool MarkdownWindow::isValidForCanvas() const {
    if (InkCanvas *inkCanvas = qobject_cast<InkCanvas*>(parentWidget())) {
        QRect canvasBounds = inkCanvas->getCanvasRect();
        
        // Check if the window is at least partially within the canvas bounds
        return canvasBounds.intersects(canvasRect);
    }
    
    // If no canvas parent, assume valid
    return true;
}

QString MarkdownWindow::getCoordinateInfo() const {
    QString info;
    info += QString("Canvas Coordinates: (%1, %2) %3x%4\n")
            .arg(canvasRect.x()).arg(canvasRect.y())
            .arg(canvasRect.width()).arg(canvasRect.height());
    
    QRect screenRect = geometry();
    info += QString("Screen Coordinates: (%1, %2) %3x%4\n")
            .arg(screenRect.x()).arg(screenRect.y())
            .arg(screenRect.width()).arg(screenRect.height());
    
    if (InkCanvas *inkCanvas = qobject_cast<InkCanvas*>(parentWidget())) {
        QSize canvasSize = inkCanvas->getCanvasSize();
        info += QString("Canvas Size: %1x%2\n")
                .arg(canvasSize.width()).arg(canvasSize.height());
        
        info += QString("Zoom Factor: %1\n").arg(inkCanvas->getZoomFactor());
        
        QPointF panOffset = inkCanvas->getPanOffset();
        info += QString("Pan Offset: (%1, %2)\n")
                .arg(panOffset.x()).arg(panOffset.y());
        
        info += QString("Valid for Canvas: %1\n").arg(isValidForCanvas() ? "Yes" : "No");
    } else {
        info += "No InkCanvas parent found\n";
    }
    
    return info;
}

void MarkdownWindow::ensureCanvasConnections() {
    // Connect to canvas pan/zoom changes to update position
    if (InkCanvas *inkCanvas = qobject_cast<InkCanvas*>(parentWidget())) {
        // Disconnect any existing connections to avoid duplicates
        disconnect(inkCanvas, &InkCanvas::panChanged, this, &MarkdownWindow::updateScreenPosition);
        disconnect(inkCanvas, &InkCanvas::zoomChanged, this, &MarkdownWindow::updateScreenPosition);
        
        // Reconnect
        connect(inkCanvas, &InkCanvas::panChanged, this, &MarkdownWindow::updateScreenPosition);
        connect(inkCanvas, &InkCanvas::zoomChanged, this, &MarkdownWindow::updateScreenPosition);
        
        // Install event filter to handle canvas resize events (for layout changes)
        inkCanvas->installEventFilter(this);
        
        // qDebug() << "MarkdownWindow::ensureCanvasConnections() - Connected to canvas signals for window" << this;
    } else {
        // qDebug() << "MarkdownWindow::ensureCanvasConnections() - No InkCanvas parent found for window" << this;
    }
}

void MarkdownWindow::focusInEvent(QFocusEvent *event) {
    // qDebug() << "MarkdownWindow::focusInEvent() - Window" << this << "gained focus";
    QWidget::focusInEvent(event);
    emit focusChanged(this, true);
}

void MarkdownWindow::focusOutEvent(QFocusEvent *event) {
    // qDebug() << "MarkdownWindow::focusOutEvent() - Window" << this << "lost focus";
    QWidget::focusOutEvent(event);
    emit focusChanged(this, false);
}

bool MarkdownWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == markdownEditor) {
        if (event->type() == QEvent::FocusIn) {
            // qDebug() << "MarkdownWindow::eventFilter() - Editor gained focus in window" << this;
            emit editorFocusChanged(this, true);
        } else if (event->type() == QEvent::FocusOut) {
            // qDebug() << "MarkdownWindow::eventFilter() - Editor lost focus in window" << this;
            emit editorFocusChanged(this, false);
        }
    } else if (obj == parentWidget() && event->type() == QEvent::Resize) {
        // Canvas widget was resized (likely due to layout changes like showing/hiding sidebars)
        // Update our screen position to maintain canvas position
        QTimer::singleShot(0, this, [this]() {
            updateScreenPosition();
        });
    }
    return QWidget::eventFilter(obj, event);
}

void MarkdownWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Emit signal to indicate window interaction
        emit windowInteracted(this);
        
        ResizeHandle handle = getResizeHandle(event->pos());
        
        if (handle != None) {
            // Start resizing
            resizing = true;
            isUserInteracting = true;
            currentResizeHandle = handle;
            resizeStartPosition = event->globalPosition().toPoint();
            resizeStartRect = geometry();
        } else if (event->pos().y() < 24) { // Header area
            // Start dragging
            dragging = true;
            isUserInteracting = true;
            dragStartPosition = event->globalPosition().toPoint();
            windowStartPosition = pos();
        }
    }
    
    QWidget::mousePressEvent(event);
}

void MarkdownWindow::mouseMoveEvent(QMouseEvent *event) {
    if (resizing) {
        QPoint delta = event->globalPosition().toPoint() - resizeStartPosition;
        QRect newRect = resizeStartRect;
        
        // Apply resize based on the stored handle
        switch (currentResizeHandle) {
            case TopLeft:
                newRect.setTopLeft(newRect.topLeft() + delta);
                break;
            case TopRight:
                newRect.setTopRight(newRect.topRight() + delta);
                break;
            case BottomLeft:
                newRect.setBottomLeft(newRect.bottomLeft() + delta);
                break;
            case BottomRight:
                newRect.setBottomRight(newRect.bottomRight() + delta);
                break;
            case Top:
                newRect.setTop(newRect.top() + delta.y());
                break;
            case Bottom:
                newRect.setBottom(newRect.bottom() + delta.y());
                break;
            case Left:
                newRect.setLeft(newRect.left() + delta.x());
                break;
            case Right:
                newRect.setRight(newRect.right() + delta.x());
                break;
            default:
                break;
        }
        
        // Enforce minimum size
        newRect.setSize(newRect.size().expandedTo(QSize(200, 150)));
        
        // Keep resized window within canvas bounds
        if (InkCanvas *inkCanvas = qobject_cast<InkCanvas*>(parentWidget())) {
            // Convert to canvas coordinates to check bounds
            QRect tempCanvasRect = inkCanvas->mapWidgetToCanvas(newRect);
            QRect canvasBounds = inkCanvas->getCanvasRect();
            
            // Apply canvas bounds constraints
            int maxCanvasX = canvasBounds.width() - tempCanvasRect.width();
            int maxCanvasY = canvasBounds.height() - tempCanvasRect.height();
            
            tempCanvasRect.setX(qMax(0, qMin(tempCanvasRect.x(), maxCanvasX)));
            tempCanvasRect.setY(qMax(0, qMin(tempCanvasRect.y(), maxCanvasY)));
            
            // Also ensure the window doesn't get resized beyond canvas bounds
            if (tempCanvasRect.right() > canvasBounds.width()) {
                tempCanvasRect.setWidth(canvasBounds.width() - tempCanvasRect.x());
            }
            if (tempCanvasRect.bottom() > canvasBounds.height()) {
                tempCanvasRect.setHeight(canvasBounds.height() - tempCanvasRect.y());
            }
            
            // Convert back to screen coordinates for the constrained geometry
            newRect = inkCanvas->mapCanvasToWidget(tempCanvasRect);
        }
        
        // Update the widget geometry directly during resizing
        setGeometry(newRect);
        
        // Convert screen coordinates back to canvas coordinates
        convertScreenToCanvasRect(newRect);
        emit windowResized(this);
    } else if (dragging) {
        QPoint delta = event->globalPosition().toPoint() - dragStartPosition;
        QPoint newPos = windowStartPosition + delta;
        
        // Keep window within canvas bounds (not just parent widget bounds)
        if (InkCanvas *inkCanvas = qobject_cast<InkCanvas*>(parentWidget())) {
            // Convert current position to canvas coordinates to check bounds
            QRect tempScreenRect(newPos, size());
            QRect tempCanvasRect = inkCanvas->mapWidgetToCanvas(tempScreenRect);
            QRect canvasBounds = inkCanvas->getCanvasRect();
            
            // Apply canvas bounds constraints
            int maxCanvasX = canvasBounds.width() - tempCanvasRect.width();
            int maxCanvasY = canvasBounds.height() - tempCanvasRect.height();
            
            tempCanvasRect.setX(qMax(0, qMin(tempCanvasRect.x(), maxCanvasX)));
            tempCanvasRect.setY(qMax(0, qMin(tempCanvasRect.y(), maxCanvasY)));
            
            // Convert back to screen coordinates for the constrained position
            QRect constrainedScreenRect = inkCanvas->mapCanvasToWidget(tempCanvasRect);
            newPos = constrainedScreenRect.topLeft();
            
            // Debug: Log when position is constrained
            if (tempCanvasRect != inkCanvas->mapWidgetToCanvas(QRect(windowStartPosition + delta, size()))) {
                // qDebug() << "MarkdownWindow: Constraining position to canvas bounds. Canvas rect:" << tempCanvasRect;
            }
        } else {
            // Fallback: Keep window within parent bounds
            if (parentWidget()) {
                QRect parentRect = parentWidget()->rect();
                newPos.setX(qMax(0, qMin(newPos.x(), parentRect.width() - width())));
                newPos.setY(qMax(0, qMin(newPos.y(), parentRect.height() - height())));
            }
        }
        
        // Update the widget position directly during dragging
        move(newPos);
        
        // Convert screen position back to canvas coordinates
        QRect newScreenRect(newPos, size());
        convertScreenToCanvasRect(newScreenRect);
        emit windowMoved(this);
    } else {
        // Update cursor for resize handles
        updateCursor(event->pos());
    }
    
    QWidget::mouseMoveEvent(event);
}

void MarkdownWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        resizing = false;
        dragging = false;
        isUserInteracting = false;
        currentResizeHandle = None;
    }
    
    QWidget::mouseReleaseEvent(event);
}

void MarkdownWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    
    // Emit windowResized if this is a user-initiated resize or if we're not updating position due to canvas changes
    if (isUserInteracting || !isUpdatingPosition) {
        emit windowResized(this);
    }
}

void MarkdownWindow::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    
    // Draw resize handles
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QColor handleColor = hasFocus() ? QColor(74, 144, 226) : QColor(180, 180, 180);
    painter.setPen(QPen(handleColor, 2));
    painter.setBrush(QBrush(handleColor));
    
    // Draw corner handles
    int handleSize = 6;
    painter.drawEllipse(0, 0, handleSize, handleSize); // Top-left
    painter.drawEllipse(width() - handleSize, 0, handleSize, handleSize); // Top-right
    painter.drawEllipse(0, height() - handleSize, handleSize, handleSize); // Bottom-left
    painter.drawEllipse(width() - handleSize, height() - handleSize, handleSize, handleSize); // Bottom-right
}

MarkdownWindow::ResizeHandle MarkdownWindow::getResizeHandle(const QPoint &pos) const {
    const int handleSize = 8;
    const QRect rect = this->rect();
    
    // Check corner handles first
    if (QRect(0, 0, handleSize, handleSize).contains(pos))
        return TopLeft;
    if (QRect(rect.width() - handleSize, 0, handleSize, handleSize).contains(pos))
        return TopRight;
    if (QRect(0, rect.height() - handleSize, handleSize, handleSize).contains(pos))
        return BottomLeft;
    if (QRect(rect.width() - handleSize, rect.height() - handleSize, handleSize, handleSize).contains(pos))
        return BottomRight;
    
    // Check edge handles
    if (QRect(0, 0, rect.width(), handleSize).contains(pos))
        return Top;
    if (QRect(0, rect.height() - handleSize, rect.width(), handleSize).contains(pos))
        return Bottom;
    if (QRect(0, 0, handleSize, rect.height()).contains(pos))
        return Left;
    if (QRect(rect.width() - handleSize, 0, handleSize, rect.height()).contains(pos))
        return Right;
    
    return None;
}

void MarkdownWindow::updateCursor(const QPoint &pos) {
    ResizeHandle handle = getResizeHandle(pos);
    
    switch (handle) {
        case TopLeft:
        case BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case TopRight:
        case BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case Top:
        case Bottom:
            setCursor(Qt::SizeVerCursor);
            break;
        case Left:
        case Right:
            setCursor(Qt::SizeHorCursor);
            break;
        default:
            if (pos.y() < 24) { // Header area
                setCursor(Qt::SizeAllCursor);
            } else {
                setCursor(Qt::ArrowCursor);
            }
            break;
    }
}

void MarkdownWindow::convertScreenToCanvasRect(const QRect &screenRect) {
    // Get the canvas parent to access coordinate conversion methods
    if (QWidget *canvas = parentWidget()) {
        // Try to cast to InkCanvas to use the new coordinate methods
        if (InkCanvas *inkCanvas = qobject_cast<InkCanvas*>(canvas)) {
            // Use the new coordinate conversion methods
            QRect oldCanvasRect = canvasRect;
            QRect newCanvasRect = inkCanvas->mapWidgetToCanvas(screenRect);
            
            // Store the converted canvas coordinates without bounds checking
            // Bounds checking should only happen during user interaction (dragging/resizing)
            canvasRect = newCanvasRect;
            // Only log if there was a significant change (and not during mouse movement)
            if ((canvasRect.topLeft() - oldCanvasRect.topLeft()).manhattanLength() > 10 && !dragging && !resizing) {
                // qDebug() << "MarkdownWindow::convertScreenToCanvasRect() - Screen rect:" << screenRect << "-> Canvas rect:" << canvasRect;
                // qDebug() << "  Old canvas rect:" << oldCanvasRect;
            }
        } else {
            // Fallback: use screen coordinates directly
            // qDebug() << "MarkdownWindow::convertScreenToCanvasRect() - No InkCanvas parent, using screen rect directly:" << screenRect;
            canvasRect = screenRect;
        }
    } else {
        // No parent, use screen coordinates directly
        // qDebug() << "MarkdownWindow::convertScreenToCanvasRect() - No parent, using screen rect directly:" << screenRect;
        canvasRect = screenRect;
    }
}