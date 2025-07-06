#include "MarkdownWindow.h"
#include "markdown/qmarkdowntextedit.h"
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

MarkdownWindow::MarkdownWindow(const QRect &rect, QWidget *parent)
    : QWidget(parent)
{
    // Initialize canvas coordinates - assume the rect passed is in canvas coordinates
    canvasRect = rect;
    
    setupUI();
    applyStyle();
    
    // Set initial screen position based on canvas coordinates
    updateScreenPosition();
    
    // Enable mouse tracking for resize handles
    setMouseTracking(true);
    
    // Set window flags for proper behavior
    setWindowFlags(Qt::Widget);
    setAttribute(Qt::WA_DeleteOnClose, false); // We'll handle deletion manually
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
    markdownEditor->setPlainText("# Enter your markdown here\n\nType your notes...");
    
    connect(markdownEditor, &QMarkdownTextEdit::textChanged, this, &MarkdownWindow::onMarkdownTextChanged);
    
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
    // Get the canvas parent to access pan/zoom information
    if (QWidget *canvas = parentWidget()) {
        // Try to cast to InkCanvas to get pan offsets
        if (canvas->objectName() == "InkCanvas" || canvas->inherits("InkCanvas")) {
            // Get pan offsets using QObject property system or direct method calls
            int panX = 0, panY = 0;
            int zoom = 100;
            
            // Use QMetaObject to call methods dynamically
            QMetaObject::invokeMethod(canvas, "getPanOffsetX", Qt::DirectConnection, Q_RETURN_ARG(int, panX));
            QMetaObject::invokeMethod(canvas, "getPanOffsetY", Qt::DirectConnection, Q_RETURN_ARG(int, panY));
            QMetaObject::invokeMethod(canvas, "getZoom", Qt::DirectConnection, Q_RETURN_ARG(int, zoom));
            
            // Convert canvas coordinates to screen coordinates
            // Screen = (Canvas - Pan) * (Zoom / 100)
            qreal zoomFactor = zoom / 100.0;
            QRect screenRect;
            screenRect.setX((canvasRect.x() - panX) * zoomFactor);
            screenRect.setY((canvasRect.y() - panY) * zoomFactor);
            screenRect.setWidth(canvasRect.width() * zoomFactor);
            screenRect.setHeight(canvasRect.height() * zoomFactor);
            
            setGeometry(screenRect);
        } else {
            // Fallback: use canvas coordinates directly
            setGeometry(canvasRect);
        }
    } else {
        // No parent, use canvas coordinates directly
        setGeometry(canvasRect);
    }
}

QVariantMap MarkdownWindow::serialize() const {
    QVariantMap data;
    data["canvas_x"] = canvasRect.x();
    data["canvas_y"] = canvasRect.y();
    data["canvas_width"] = canvasRect.width();
    data["canvas_height"] = canvasRect.height();
    data["content"] = getMarkdownContent();
    return data;
}

void MarkdownWindow::deserialize(const QVariantMap &data) {
    int x = data.value("canvas_x", 0).toInt();
    int y = data.value("canvas_y", 0).toInt();
    int width = data.value("canvas_width", 300).toInt();
    int height = data.value("canvas_height", 200).toInt();
    QString content = data.value("content", "# New Markdown Window").toString();
    
    canvasRect = QRect(x, y, width, height);
    setMarkdownContent(content);
    updateScreenPosition();
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
    emit contentChanged();
}

void MarkdownWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        ResizeHandle handle = getResizeHandle(event->pos());
        
        if (handle != None) {
            // Start resizing
            resizing = true;
            currentResizeHandle = handle;
            resizeStartPosition = event->globalPosition().toPoint();
            resizeStartRect = geometry();
        } else if (event->pos().y() < 24) { // Header area
            // Start dragging
            dragging = true;
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
        
        // Convert screen coordinates back to canvas coordinates
        convertScreenToCanvasRect(newRect);
        updateScreenPosition();
        emit windowResized(this);
    } else if (dragging) {
        QPoint delta = event->globalPosition().toPoint() - dragStartPosition;
        QPoint newPos = windowStartPosition + delta;
        
        // Keep window within parent bounds
        if (parentWidget()) {
            QRect parentRect = parentWidget()->rect();
            newPos.setX(qMax(0, qMin(newPos.x(), parentRect.width() - width())));
            newPos.setY(qMax(0, qMin(newPos.y(), parentRect.height() - height())));
        }
        
        // Convert screen position back to canvas coordinates
        QRect newScreenRect(newPos, size());
        convertScreenToCanvasRect(newScreenRect);
        updateScreenPosition();
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
        currentResizeHandle = None;
    }
    
    QWidget::mouseReleaseEvent(event);
}

void MarkdownWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    emit windowResized(this);
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
    // Get the canvas parent to access pan/zoom information
    if (QWidget *canvas = parentWidget()) {
        // Try to cast to InkCanvas to get pan offsets
        if (canvas->objectName() == "InkCanvas" || canvas->inherits("InkCanvas")) {
            // Get pan offsets using QObject property system or direct method calls
            int panX = 0, panY = 0;
            int zoom = 100;
            
            // Use QMetaObject to call methods dynamically
            QMetaObject::invokeMethod(canvas, "getPanOffsetX", Qt::DirectConnection, Q_RETURN_ARG(int, panX));
            QMetaObject::invokeMethod(canvas, "getPanOffsetY", Qt::DirectConnection, Q_RETURN_ARG(int, panY));
            QMetaObject::invokeMethod(canvas, "getZoom", Qt::DirectConnection, Q_RETURN_ARG(int, zoom));
            
            // Convert screen coordinates to canvas coordinates
            // Canvas = (Screen / (Zoom / 100)) + Pan
            qreal zoomFactor = zoom / 100.0;
            canvasRect.setX((screenRect.x() / zoomFactor) + panX);
            canvasRect.setY((screenRect.y() / zoomFactor) + panY);
            canvasRect.setWidth(screenRect.width() / zoomFactor);
            canvasRect.setHeight(screenRect.height() / zoomFactor);
        } else {
            // Fallback: use screen coordinates directly
            canvasRect = screenRect;
        }
    } else {
        // No parent, use screen coordinates directly
        canvasRect = screenRect;
    }
}