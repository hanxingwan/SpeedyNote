#ifndef INKCANVAS_H
#define INKCANVAS_H

#include <QWidget>
#include <QTabletEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QElapsedTimer>
#include <deque>
#include <QColor>
#include "ToolType.h"
#include <QImage>
#include <poppler-qt6.h>
#include <QCache>
#include <QTimer>
#include <QMenu>
#include <QClipboard>
#include <QFutureWatcher>
#include <QMutex>
#include "MarkdownWindowManager.h"
#include "PictureWindowManager.h"
#include "ToolType.h"
#include "ButtonMappingTypes.h"
#include "SpnPackageManager.h"
#include "PdfRelinkDialog.h"

class MarkdownWindowManager;
class PictureWindowManager;
class MarkdownWindow;
class PictureWindow;

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
    void ropeSelectionCompleted(const QPoint &position); // Signal emitted when rope tool selection is completed
    void pdfLinkClicked(int targetPage); // Signal emitted when a PDF link is clicked
    void pdfTextSelected(const QString &text); // Signal emitted when PDF text is selected
    void pdfLoaded(); // Signal emitted when a PDF is loaded
    void markdownSelectionModeChanged(bool enabled); // Signal emitted when markdown selection mode changes
    void annotatedImageSaved(const QString &filePath); // ✅ Signal emitted when annotated image is saved
    void autoScrollRequested(int direction); // Signal for autoscrolling to next/prev page
    void earlySaveRequested(); // Signal for proactive save before autoscroll threshold

public:
    explicit InkCanvas(QWidget *parent = nullptr);
    virtual ~InkCanvas();

    void startBenchmark();
    void stopBenchmark();
    int getProcessedRate();
    void setPenColor(const QColor &color); // Added function to set pen color
    void setPenThickness(qreal thickness); // Added function to set pen thickness
    void adjustAllToolThicknesses(qreal zoomRatio); // Adjust all tool thicknesses for zoom changes
    void setTool(ToolType tool);
    void setSaveFolder(const QString &folderPath); // Function to set save folder
    void saveToFile(int pageNumber); // Function to save canvas to file
    void saveCurrentPage();  // ✅ New function (see below)
    void loadPage(int pageNumber);
    void deletePage(int pageNumber);
    void clearCurrentPage(); // Clear all content (drawing + pictures) from current page
    void setBackground(const QString &filePath, int pageNumber);
    void saveAnnotated(int pageNumber);

    void setZoom(int zoomLevel);
    int getZoom() const;
    void updatePanOffsets(int xOffset, int yOffset);

    int getPanOffsetX() const;  // Getter for panOffsetX
    int getPanOffsetY() const;  // Getter for panOffsetY

    void setPanX(int xOffset);  // Setter for panOffsetX
    void setPanY(int yOffset);  // Setter for panOffsetY

    void loadPdf(const QString &pdfPath);
    void loadPdfPage(int pageNumber);

    bool isPdfLoadedFunc() const;
    int getTotalPdfPages() const;
    Poppler::Document* getPdfDocument() const;
    
    void clearPdf();
    void clearPdfNoDelete();

    QString getSaveFolder() const { return saveFolder; }
    QString getDisplayPath() const; // ✅ Get display path (.spn package or folder)
    void syncSpnPackage(); // ✅ Sync changes back to .spn file

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

    // saveBackgroundMetadata moved to unified JSON metadata section

    int getBufferWidth() const { return buffer.width(); }
    QPixmap getBuffer() const { return buffer; } // Get buffer for concurrent saving



    bool isEdited() const { return edited; }  // ✅ Check if the canvas has been edited
    void setEdited(bool state) { edited = state; }  // ✅ Set the edited state

    void setPDFRenderDPI(int dpi) { pdfRenderDPI = dpi; }  // ✅ Set PDF render DPI

    void clearPdfCache() { 
        QMutexLocker locker(&pdfCacheMutex);
        pdfCache.clear(); 
    }
    void clearNoteCache() { 
        {
            QMutexLocker locker(&noteCacheMutex);
            noteCache.clear(); 
        }
        currentCachedNotePage = -1;
        if (noteCacheTimer && noteCacheTimer->isActive()) {
            noteCacheTimer->stop();
        }
        
        // Cancel and clean up any active note watchers
        for (QFutureWatcher<void>* watcher : activeNoteWatchers) {
            if (watcher && !watcher->isFinished()) {
                watcher->cancel();
            }
            watcher->deleteLater();
        }
        activeNoteWatchers.clear();
    }

    // Touch gesture support
    void setTouchGesturesEnabled(bool enabled) { touchGesturesEnabled = enabled; }
    bool areTouchGesturesEnabled() const { return touchGesturesEnabled; }

    // Rope tool selection actions
    void deleteRopeSelection(); // Delete the current rope tool selection
    void cancelRopeSelection(); // Cancel the current rope tool selection
    void copyRopeSelection(); // Copy the current rope tool selection
    void copyRopeSelectionToClipboard(); // Copy the current rope tool selection to clipboard
    
    // Markdown integration
    MarkdownWindowManager* getMarkdownManager() const { return markdownManager; }
    PictureWindowManager* getPictureManager() const { return pictureManager; }
    void setMarkdownSelectionMode(bool enabled);
    bool isMarkdownSelectionMode() const;
    
    // Picture integration
    void setPictureSelectionMode(bool enabled);
    bool isPictureSelectionMode() const;
    void setPictureWindowEditMode(bool enabled);

    // PDF text selection and link functionality
    void setPdfTextSelectionEnabled(bool enabled) { 
        pdfTextSelectionEnabled = enabled; 
        
        // Enable mouse tracking for PDF text selection
        setMouseTracking(enabled);
        
        // Change cursor when entering/exiting text selection mode
        if (enabled && isPdfLoaded) {
            setCursor(Qt::IBeamCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        
        // Clear any existing selection when disabling
        if (!enabled) {
            clearPdfTextSelection();
        }
        
        update(); // Refresh the display
    }
    bool isPdfTextSelectionEnabled() const { return pdfTextSelectionEnabled; }
    void clearPdfTextSelection(); // Clear current PDF text selection
    QString getSelectedPdfText() const; // Get currently selected PDF text

    // Canvas coordinate system support
    QSize getCanvasSize() const { return buffer.size(); }
    QRect getCanvasRect() const { return QRect(0, 0, buffer.width(), buffer.height()); }
    qreal getZoomFactor() const { return zoomFactor / 100.0; }
    QPointF getPanOffset() const { return QPointF(panOffsetX, panOffsetY); }
    
    // Coordinate conversion methods
    QPointF mapWidgetToCanvas(const QPointF &widgetPoint) const;
    QPointF mapCanvasToWidget(const QPointF &canvasPoint) const;
    QRect mapWidgetToCanvas(const QRect &widgetRect) const;
    QRect mapCanvasToWidget(const QRect &canvasRect) const;
    
    // Autoscroll threshold getter for MainWindow
    int getAutoscrollThreshold() const;
    
    // Background image getter for MainWindow
    QPixmap getBackgroundImage() const { return backgroundImage; }

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
    
    // Separate thickness values for each tool
    qreal penToolThickness = 5.0;    // Default pen thickness
    qreal markerToolThickness = 5.0; // Default marker thickness (will be scaled by 8x in drawing)
    qreal eraserToolThickness = 5.0; // Default eraser thickness (will be scaled by 6x in drawing)
    QString saveFolder;
    QString actualPackagePath; // ✅ Store .spn package path for display purposes
    QString tempWorkingDir; // ✅ Temporary directory for .spn packages
    bool isSpnPackage = false; // ✅ Flag to indicate if working with .spn package
    QString notebookId;
    QPixmap backgroundImage;
    bool straightLineMode = false;  // Flag for straight line mode
    bool ropeToolMode = false; // Flag for rope tool mode
    QPixmap selectionBuffer; // Buffer for the selected area in rope tool mode (physical pixels, masked)
    QRect selectionRect; // Bounding rectangle of the current selection in LOGICAL WIDGET coordinates
    QRectF exactSelectionRectF; // Floating-point version for smoother movement
    QPolygonF lassoPathPoints; // Points of the lasso selection in LOGICAL WIDGET coordinates
    bool selectingWithRope = false; // True if currently drawing the lasso
    bool movingSelection = false; // True if currently moving the selection
    bool selectionJustCopied = false; // True if selection was just copied and hasn't been moved yet
    bool selectionAreaCleared = false; // True if the selection area has been cleared from the buffer
    QPainterPath selectionMaskPath; // Path used to clear the selection area from buffer
    QRectF selectionBufferRect; // Buffer rectangle for the selection area
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
    mutable QMutex pdfCacheMutex; // Thread safety for pdfCache
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

    // ✅ Unified JSON metadata
    QString pdfPath;
    int lastAccessedPage = 0;
    BackgroundStyle backgroundStyle = BackgroundStyle::None;
    QColor backgroundColor = Qt::white;
    int backgroundDensity = 20;
    QStringList bookmarks; // ✅ Add bookmarks to JSON metadata

public:
    // ✅ Metadata management
    void loadNotebookId();
    
    // Combined canvas window management
    void saveCombinedWindowsForPage(int pageNumber); // Save windows for combined canvas pages
    
    // Autoscroll threshold detection (for touch gestures and external pan changes)
    void checkAutoscrollThreshold(int oldPanY, int newPanY);
    void saveNotebookId();
    void saveBackgroundMetadata();
    
    // ✅ New unified JSON metadata system
    void loadNotebookMetadata();
    void saveNotebookMetadata();
    void setLastAccessedPage(int pageNumber);
    int getLastAccessedPage() const;
    QString getPdfPath() const; // ✅ Get PDF path from JSON metadata
    QString getNotebookId() const; // ✅ Get notebook ID from JSON metadata
    bool handleMissingPdf(QWidget *parent = nullptr); // ✅ Handle missing PDF file
    
    // ✅ Bookmark management through JSON
    void addBookmark(const QString &bookmark);
    void removeBookmark(const QString &bookmark);
    QStringList getBookmarks() const;
    void setBookmarks(const QStringList &bookmarkList);
    
    // ✅ Cache management
    void invalidateCurrentPageCache(); // Invalidate cache for current page when modified
    
private:
    // Combined canvas window management
    void loadCombinedWindowsForPage(int pageNumber); // Load windows for combined canvas pages
    QList<MarkdownWindow*> loadMarkdownWindowsForPage(int pageNumber); // Load markdown windows without affecting current
    QList<PictureWindow*> loadPictureWindowsForPage(int pageNumber); // Load picture windows without affecting current
    
    // PDF text selection helpers for combined canvas
    void loadPdfTextBoxesForSinglePage(int pageNumber); // Load text boxes for single page
    void loadPdfTextBoxesForCombinedCanvas(int pageNumber, int singlePageHeight); // Load text boxes for combined canvas
    // ✅ Migration from old txt files to JSON
    void migrateOldMetadataFiles();
    
    // ✅ Clipboard image paste functionality
    QString pasteImageFromClipboard(); // Returns path to saved clipboard image or empty string on failure

    int pdfRenderDPI = 192;  // Default to 288 DPI

    // Touch gesture support
    bool touchGesturesEnabled = false;
    QPointF lastTouchPos;
    qreal lastPinchScale = 1.0;
    bool isPanning = false;
    int activeTouchPoints = 0;

    // Background style members (moved to unified JSON metadata section above)

    bool pdfTextSelectionEnabled = false;
    
    // PDF text selection members
    bool pdfTextSelecting = false; // True when actively selecting text
    QPointF pdfSelectionStart; // Start point of text selection (logical widget coordinates)
    QPointF pdfSelectionEnd; // End point of text selection (logical widget coordinates)
    QList<Poppler::TextBox*> currentPdfTextBoxes; // Text boxes for current page(s)
    QList<Poppler::TextBox*> selectedTextBoxes; // Currently selected text boxes
    QList<int> currentPdfTextBoxPageNumbers; // Page number for each text box (for combined canvas)
    std::unique_ptr<Poppler::Page> currentPdfPageForText; // Current PDF page for text operations
    std::unique_ptr<Poppler::Page> currentPdfPageForTextSecond; // Second PDF page for combined canvas
    
    // PDF text selection throttling (60 FPS)
    QTimer* pdfTextSelectionTimer = nullptr; // Timer for throttling text selection updates
    QPointF pendingSelectionStart; // Pending selection start point
    QPointF pendingSelectionEnd; // Pending selection end point
    bool hasPendingSelection = false; // True if there's a pending selection update
    
    // Intelligent PDF cache system
    QTimer* pdfCacheTimer = nullptr; // Timer for delayed adjacent page caching
    int currentCachedPage = -1; // Currently displayed page for cache management
    int pendingCacheTargetPage = -1; // Target page for pending cache operation (to validate timer relevance)
    QList<QFutureWatcher<void>*> activePdfWatchers; // Track active PDF cache watchers for cleanup
    
    // Intelligent note page cache system
    QCache<int, QPixmap> noteCache; // Cache for note pages (PNG files)
    mutable QMutex noteCacheMutex; // Thread safety for noteCache
    QTimer* noteCacheTimer = nullptr; // Timer for delayed adjacent note page caching
    int currentCachedNotePage = -1; // Currently displayed note page for cache management
    int pendingNoteCacheTargetPage = -1; // Target page for pending note cache operation (to validate timer relevance)
    QList<QFutureWatcher<void>*> activeNoteWatchers; // Track active note cache watchers for cleanup
    
    // Markdown integration
    MarkdownWindowManager* markdownManager = nullptr;
    PictureWindowManager* pictureManager = nullptr;
    bool markdownSelectionMode = false;
    QPoint markdownSelectionStart;
    QPoint markdownSelectionEnd;
    bool markdownSelecting = false;
    
    // Picture selection state
    bool pictureSelectionMode = false;
    QPoint pictureSelectionStart;
    bool pictureSelecting = false;
    bool pictureWindowEditMode = false;
    
    // Picture window interaction state
    PictureWindow* activePictureWindow = nullptr;
    bool pictureDragging = false;
    bool pictureResizing = false;
    QPoint pictureInteractionStartPos;
    QRect pictureStartRect;
    QRect picturePreviousRect; // Track previous position to clear ghost images
    QRect picturePreviewRect;  // Store preview position during drag/resize for performance
    int pictureResizeHandle = 0; // Store as int to avoid incomplete type issues
    
    // Picture interaction helper methods
    void handlePictureMouseMove(QMouseEvent *event);
    void handlePictureMouseRelease(QMouseEvent *event);
    
    // Helper methods for PDF text selection
    void loadPdfTextBoxes(int pageNumber); // Load text boxes for a page
    QPointF mapWidgetToPdfCoordinates(const QPointF &widgetPoint); // Map widget coordinates to PDF coordinates
    QPointF mapPdfToWidgetCoordinates(const QPointF &pdfPoint, int pageNumber = -1); // Map PDF coordinates to widget coordinates
    void updatePdfTextSelection(const QPointF &start, const QPointF &end); // Update text selection
    void handlePdfLinkClick(const QPointF &clickPoint); // Handle PDF link clicks
    void showPdfTextSelectionMenu(const QPoint &position); // Show context menu for PDF text selection
    QList<Poppler::TextBox*> getTextBoxesInSelection(const QPointF &start, const QPointF &end); // Get text boxes in selection area
    
    // Intelligent PDF cache helper methods
    void renderPdfPageToCache(int pageNumber); // Render a single page and add to cache
    void checkAndCacheAdjacentPages(int targetPage); // Check and cache adjacent pages if needed
    bool isValidPageNumber(int pageNumber) const; // Check if page number is valid
    
    // Intelligent note cache helper methods
    void loadNotePageToCache(int pageNumber); // Load a single note page and add to cache
    void checkAndCacheAdjacentNotePages(int targetPage); // Check and cache adjacent note pages if needed
    QString getNotePageFilePath(int pageNumber) const; // Get file path for note page
    
private slots:
    void processPendingTextSelection(); // Process pending text selection updates (throttled to 60 FPS)
    void cacheAdjacentPages(); // Cache adjacent pages after delay
    void cacheAdjacentNotePages(); // Cache adjacent note pages after delay
};

#endif // INKCANVAS_H