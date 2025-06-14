#ifndef INKCANVAS_H
#define INKCANVAS_H

#include <QWidget>
#include <QTabletEvent>
#include <QPainter>
#include <QPixmap>
#include <QElapsedTimer>
#include <deque>
#include <QColor>
#include "ToolType.h"
#include <QImage>
#include <poppler-qt6.h>
#include <QCache>
#include <QTimer>

enum class BackgroundStyle {
    None,
    Grid,
    Lines
};

class InkCanvas : public QWidget {
    Q_OBJECT

signals:
    void zoomChanged(int newZoom);
    void panChanged(int panX, int panY);
    void touchGestureEnded(); // Signal emitted when touch gestures end

public:
    explicit InkCanvas(QWidget *parent = nullptr);
    virtual ~InkCanvas();

    void startBenchmark();
    void stopBenchmark();
    int getProcessedRate();
    void setPenColor(const QColor &color); // Added function to set pen color
    void setPenThickness(qreal thickness); // Added function to set pen thickness
    void setTool(ToolType tool);
    void setSaveFolder(const QString &folderPath); // Function to set save folder
    void saveToFile(int pageNumber); // Function to save canvas to file
    void saveCurrentPage();  // ✅ New function (see below)
    void loadPage(int pageNumber);
    void deletePage(int pageNumber);
    void setBackground(const QString &filePath, int pageNumber);
    void saveAnnotated(int pageNumber);

    void setZoom(int zoomLevel);
    int getZoom() const;
    QSize getCanvasSize() const;
    void updatePanOffsets(int xOffset, int yOffset);

    int getPanOffsetX() const;  // Getter for panOffsetX
    int getPanOffsetY() const;  // Getter for panOffsetY

    void setPanX(int xOffset);  // Setter for panOffsetX
    void setPanY(int yOffset);  // Setter for panOffsetY

    void loadPdf(const QString &pdfPath);
    void loadPdfPage(int pageNumber);

    bool isPdfLoadedFunc() const;
    int getTotalPdfPages() const;
    
    void clearPdf();
    void clearPdfNoDelete();

    QString getSaveFolder() const;

    void setLastActivePage(int page) { lastActivePage = page; }
    int getLastActivePage() const { return lastActivePage; }
    // ✅ Zoom & Pan Management
    void setLastZoomLevel(int zoom) { lastZoomLevel = zoom; }
    int getLastZoomLevel() const { return lastZoomLevel; }

    void setLastPanX(int pan) { lastPanX = pan; }
    int getLastPanX() const { return lastPanX; }

    void setLastPanY(int pan) { lastPanY = pan; }
    int getLastPanY() const { return lastPanY; }
    QColor getPenColor(); // Added getter for pen color
    qreal getPenThickness(); // Added getter for pen thickness
    ToolType getCurrentTool(); // Added getter for tool type

    // Straight line mode toggle
    void setStraightLineMode(bool enabled) { straightLineMode = enabled; }
    bool isStraightLineMode() const { return straightLineMode; }

    // Rope tool mode toggle
    void setRopeToolMode(bool enabled) { ropeToolMode = enabled; }
    bool isRopeToolMode() const { return ropeToolMode; }

    void loadPdfPreviewAsync(int pageNumber);  // ✅ Load a quick preview of the PDF page
    // for notebook background below
    void setBackgroundStyle(BackgroundStyle style);
    void setBackgroundColor(const QColor &color);
    void setBackgroundDensity(int density);

    BackgroundStyle getBackgroundStyle() const { return backgroundStyle; }
    QColor getBackgroundColor() const { return backgroundColor; }
    int getBackgroundDensity() const { return backgroundDensity; }

    void saveBackgroundMetadata();  // ✅ Save background metadata

    int getBufferWidth() const { return buffer.width(); }
    QPixmap getBuffer() const { return buffer; } // Get buffer for concurrent saving

    void exportNotebook(const QString &destinationFile);
    void importNotebook(const QString &packageFile);

    void importNotebookTo(const QString &packageFile, const QString &destFolder);

    bool isEdited() const { return edited; }  // ✅ Check if the canvas has been edited
    void setEdited(bool state) { edited = state; }  // ✅ Set the edited state

    void setPDFRenderDPI(int dpi) { pdfRenderDPI = dpi; }  // ✅ Set PDF render DPI

    void clearPdfCache() { pdfCache.clear(); }

    // Touch gesture support
    void setTouchGesturesEnabled(bool enabled) { touchGesturesEnabled = enabled; }
    bool areTouchGesturesEnabled() const { return touchGesturesEnabled; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void tabletEvent(QTabletEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override; // Handle resizing
    
    // Touch event handling
    bool event(QEvent *event) override;

private:
    QPointF mapLogicalWidgetToPhysicalBuffer(const QPointF& logicalWidgetPoint);
    QRect mapRectBufferToWidgetLogical(const QRectF& physicalBufferRect);

    QPixmap buffer;            // Off-screen buffer
    QImage background;
    QPointF lastPoint;
    QPointF straightLineStartPoint;  // Stores the start point for straight line mode
    bool drawing;
    QColor penColor; // Added pen color property
    qreal penThickness; // Added pen thickness property
    ToolType currentTool;
    ToolType previousTool; // To restore tool after erasing
    QString saveFolder; // Folder to save images
    QPixmap backgroundImage;
    bool straightLineMode = false;  // Flag for straight line mode
    bool ropeToolMode = false; // Flag for rope tool mode
    QPixmap selectionBuffer; // Buffer for the selected area in rope tool mode (physical pixels, masked)
    QRect selectionRect; // Bounding rectangle of the current selection in LOGICAL WIDGET coordinates
    QRectF exactSelectionRectF; // Floating-point version for smoother movement
    QPolygonF lassoPathPoints; // Points of the lasso selection in LOGICAL WIDGET coordinates
    bool selectingWithRope = false; // True if currently drawing the lasso
    bool movingSelection = false; // True if currently moving the selection
    QPointF lastMovePoint; // Last point during selection movement (logical widget coordinates)

    int zoomFactor;     // Zoom percentage (100 = normal)
    int panOffsetX;     // Horizontal pan offset
    int panOffsetY;     // Vertical pan offset
    
    // Internal floating-point zoom for smoother gestures
    qreal internalZoomFactor = 100.0;

    void initializeBuffer();   // Helper to initialize the buffer
    void drawStroke(const QPointF &start, const QPointF &end, qreal pressure);    
    void eraseStroke(const QPointF &start, const QPointF &end, qreal pressure);
    QRectF calculatePreviewRect(const QPointF &start, const QPointF &oldEnd, const QPointF &newEnd);
    

    QCache<int, QPixmap> pdfCache; // Caches 5 pages of the PDF
    std::unique_ptr<Poppler::Document> pdfDocument;
    int currentPdfPage;
    bool isPdfLoaded = false;
    int totalPdfPages = 0;

    int lastActivePage = 0;  // ✅ Stores last active page per tab
    int lastZoomLevel = 100;  // ✅ Default zoom level
    int lastPanX = 0;  // ✅ Default pan X
    int lastPanY = 0;  // ✅ Default pan Y

    // QImage *pdfImage = nullptr;  // ✅ Image for the current PDF page

    bool edited = false;  // ✅ Track if the canvas has been edited

    bool benchmarking;
    std::deque<qint64> processedTimestamps;
    QElapsedTimer benchmarkTimer;

    QString notebookId;

    void loadNotebookId();
    void saveNotebookId();

    int pdfRenderDPI = 288;  // Default to 288 DPI

    // Touch gesture support
    bool touchGesturesEnabled = false;
    QPointF lastTouchPos;
    qreal lastPinchScale = 1.0;
    bool isPanning = false;
    int activeTouchPoints = 0;

    // Background style members
    BackgroundStyle backgroundStyle = BackgroundStyle::None;
    QColor backgroundColor = Qt::transparent;
    int backgroundDensity = 40;  // pixels between lines
};

#endif // INKCANVAS_H