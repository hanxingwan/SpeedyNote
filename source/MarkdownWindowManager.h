#ifndef MARKDOWNWINDOWMANAGER_H
#define MARKDOWNWINDOWMANAGER_H

#include <QObject>
#include <QWidget>
#include <QMap>
#include <QList>
#include <QSet>
#include <QRect>
#include <QString>
#include <QTimer>

class MarkdownWindow;
class InkCanvas;

class MarkdownWindowManager : public QObject {
    Q_OBJECT

public:
    explicit MarkdownWindowManager(InkCanvas *canvas, QObject *parent = nullptr);
    ~MarkdownWindowManager();

    // Window management
    MarkdownWindow* createMarkdownWindow(const QRect &rect);
    void removeMarkdownWindow(MarkdownWindow *window);
    void clearAllWindows();
    void clearCurrentPagePermanently(int pageNumber); // ✅ Safely clear and permanently delete current page windows
    void clearAllCachedWindows(); // Clear all cached windows (for destructor cleanup)
    
    // Page lifecycle
    void saveWindowsForPage(int pageNumber);
    void loadWindowsForPage(int pageNumber);
    void deleteWindowsForPage(int pageNumber);
    
    // Selection mode
    void setSelectionMode(bool enabled);
    bool isSelectionMode() const { return selectionMode; }
    
    // Get all windows for current page
    QList<MarkdownWindow*> getCurrentPageWindows() const;
    
    // Combined canvas support
    QList<MarkdownWindow*> loadWindowsForPageSeparately(int pageNumber);
    void setCombinedWindows(const QList<MarkdownWindow*> &windows, int firstPage = -1, int secondPage = -1);
    void saveWindowsForPageSeparately(int pageNumber, const QList<MarkdownWindow*> &windows);
    
    // Get canvas
    InkCanvas* getCanvas() const { return canvas; }
    
    // Transparency management
    void resetTransparencyTimer();
    void setWindowsTransparent(bool transparent);
    void hideAllWindows(); // Hide all windows and stop transparency timer
    
    // Frame-only mode (for performance during touch panning)
    void setWindowsFrameOnlyMode(bool enabled);

signals:
    void windowCreated(MarkdownWindow *window);
    void windowRemoved(MarkdownWindow *window);

private slots:
    void onWindowDeleteRequested(MarkdownWindow *window);
    void onWindowFocusChanged(MarkdownWindow *window, bool focused);
    void onWindowContentChanged(MarkdownWindow *window);
    void onTransparencyTimerTimeout();

private:
    InkCanvas *canvas;
    bool selectionMode = false;
    
    // Map of page number to list of markdown windows
    QMap<int, QList<MarkdownWindow*>> pageWindows;
    
    // Current page windows (for quick access)
    QList<MarkdownWindow*> currentWindows;

    // Track temporary combined windows for cleanup
    QList<MarkdownWindow*> combinedTempWindows; // ✅ Track temporary combined windows for cleanup
    QList<MarkdownWindow*> orphanedCacheWindows; // ✅ Track orphaned cache windows awaiting cleanup
    
    // Track combined mode page numbers
    int combinedFirstPage = -1;  // First page in combined mode (-1 if not in combined mode)
    int combinedSecondPage = -1; // Second page in combined mode (-1 if not in combined mode)
    
    // Track which pages have been modified (dirty flags)
    QSet<int> dirtyPages; // Pages that need to be saved to disk
    bool suppressDirtyMarking = false; // Temporarily disable dirty marking during loading operations
    
    // Content change debouncing to avoid updating cache on every keystroke
    QTimer *contentDebounceTimer; // Delays cache updates after content changes
    QMap<MarkdownWindow*, QString> pendingContentUpdates; // Windows with pending content updates
    
    // Geometry change debouncing to avoid marking dirty on every pixel of dragging/resizing
    QTimer *geometryDebounceTimer; // Delays marking dirty after move/resize operations
    QSet<MarkdownWindow*> pendingGeometryUpdates; // Windows with pending geometry updates
    QMap<MarkdownWindow*, int> pendingGeometryOriginalY; // Track original Y position before movement
    
    // Transparency timer system
    QTimer *transparencyTimer;
    MarkdownWindow *currentlyFocusedWindow = nullptr;
    bool windowsAreTransparent = false;
    
    // Helper methods
    QString getWindowDataFilePath(int pageNumber) const;
    void saveWindowData(int pageNumber, const QList<MarkdownWindow*> &windows);
    QList<MarkdownWindow*> loadWindowData(int pageNumber);
    QString getSaveFolder() const;
    QString getNotebookId() const;
    QRect convertScreenToCanvasRect(const QRect &screenRect) const;
    void connectWindowSignals(MarkdownWindow *window);
    void updatePermanentCacheForWindow(MarkdownWindow *modifiedWindow, int pageNumber);
    int getPageNumberFromCanvasY(int canvasY) const; // Calculate which page a Y coordinate belongs to
    void flushPendingContentUpdates(); // Process queued content updates immediately
    void flushPendingGeometryUpdates(); // Process queued geometry updates immediately
    
public slots:
    void updateAllWindowPositions(); // Update all window positions when canvas pan/zoom changes
};

#endif // MARKDOWNWINDOWMANAGER_H 