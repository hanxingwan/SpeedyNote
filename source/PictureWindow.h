#ifndef PICTUREWINDOW_H
#define PICTUREWINDOW_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRect>
#include <QPixmap>
#include <QVariantMap>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QTimer>
#include <QTouchEvent>
#include <QTime>

class InkCanvas;

class PictureWindow : public QWidget {
    Q_OBJECT

public:
    enum ResizeHandle {
        None,
        TopLeft, TopRight, BottomLeft, BottomRight,
        Top, Bottom, Left, Right
    };
    explicit PictureWindow(const QRect &rect, const QString &imagePath, QWidget *parent = nullptr);
    ~PictureWindow();

    // Content management
    QString getImagePath() const;
    void setImagePath(const QString &imagePath);
    
    // Geometry management (canvas coordinates)
    QRect getCanvasRect() const;
    void setCanvasRect(const QRect &rect);
    
    // Serialization
    QVariantMap serialize() const;
    void deserialize(const QVariantMap &data);
    
    // Validation
    bool isValidForCanvas() const;
    
    // Coordinate system
    void updateScreenPosition();
    void updateScreenPositionImmediate(); // ✅ PERFORMANCE: Immediate update without throttling
    
    // Debug info
    QString getCoordinateInfo() const;
    
    // Canvas connections
    void ensureCanvasConnections();
    
    // Edit mode control
    bool isInEditMode() const { return editMode; }
    void forceExitEditMode();
    void enterEditMode(); // Make public for canvas interaction
    void onDeleteClicked(); // Make public for canvas interaction
    
    // Canvas rendering
    void renderToCanvas(QPainter &painter, const QRect &canvasRect) const;
    QPixmap getScaledPixmap() const { return scaledPixmap; }
    
    // Cached rendering - simple approach
    mutable QPixmap cachedRendering;
    mutable QRect cachedRect;
    mutable bool cachedEditMode;
    void invalidateCache() const;
    
    // Methods for handling interactions when rendered to canvas
    bool isClickOnDeleteButton(const QPoint &canvasPos) const;
    ResizeHandle getResizeHandleAtCanvasPos(const QPoint &canvasPos) const;
    bool isClickOnHeader(const QPoint &canvasPos) const;
    bool isClickOnPictureBody(const QPoint &canvasPos) const;
    
    // Aspect ratio management
    bool getMaintainAspectRatio() const { return maintainAspectRatio; }
    double getAspectRatio() const { return aspectRatio; }

    void setFrameOnlyMode(bool enabled);
    bool isFrameOnlyMode() const;

signals:
    void deleteRequested(PictureWindow *window);
    void windowMoved(PictureWindow *window);
    void windowResized(PictureWindow *window);
    void windowInteracted(PictureWindow *window);
    void editModeChanged(PictureWindow *window, bool enabled);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private slots:
    void exitEditMode();

private:

    void setupUI();
    void applyStyle();
    void loadImage();
    ResizeHandle getResizeHandle(const QPoint &pos) const;
    void updateCursor(const QPoint &pos);
    void convertScreenToCanvasRect(const QRect &screenRect);
    void handleTouchEvent(QTouchEvent *event);

    // UI elements
    QVBoxLayout *mainLayout;
    QHBoxLayout *headerLayout;
    QLabel *titleLabel;
    QPushButton *deleteButton;
    QLabel *imageLabel;
    
    // Image data
    QString imagePath;
    QPixmap originalPixmap;
    QPixmap scaledPixmap;
    
    // Coordinate system
    QRect canvasRect;  // Position in canvas coordinates
    bool isUpdatingPosition;
    QSize lastScaledSize;  // ✅ PERFORMANCE: Track last scaled size to avoid unnecessary rescaling
    
    // Mouse interaction
    bool dragging;
    bool resizing;
    bool isUserInteracting;
    ResizeHandle currentResizeHandle;
    QPoint dragStartPosition;
    QPoint windowStartPosition;
    QPoint resizeStartPosition;
    QRect resizeStartRect;
    
    // Aspect ratio preservation
    bool maintainAspectRatio;
    double aspectRatio;
    
    // Edit mode
    bool editMode;
    QTimer *longPressTimer;
    QPoint longPressStartPos;
    bool wasLongPress;
    QTime lastClickTime;
    
    // ✅ PERFORMANCE: Throttling for pan updates
    QTimer *updateThrottleTimer;
    bool hasPendingUpdate;
    
    // Touch interaction
    QPoint touchStartPos;
    QPoint touchStartGlobalPos;

    bool frameOnlyMode;
};

#endif // PICTUREWINDOW_H
