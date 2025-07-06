#ifndef MARKDOWNWINDOWMANAGER_H
#define MARKDOWNWINDOWMANAGER_H

#include <QObject>
#include <QWidget>
#include <QMap>
#include <QList>
#include <QRect>
#include <QString>

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
    
    // Page lifecycle
    void saveWindowsForPage(int pageNumber);
    void loadWindowsForPage(int pageNumber);
    void deleteWindowsForPage(int pageNumber);
    
    // Selection mode
    void setSelectionMode(bool enabled);
    bool isSelectionMode() const { return selectionMode; }
    
    // Get all windows for current page
    QList<MarkdownWindow*> getCurrentPageWindows() const;
    
    // Get canvas
    InkCanvas* getCanvas() const { return canvas; }

signals:
    void windowCreated(MarkdownWindow *window);
    void windowRemoved(MarkdownWindow *window);

private slots:
    void onWindowDeleteRequested(MarkdownWindow *window);

private:
    InkCanvas *canvas;
    bool selectionMode = false;
    
    // Map of page number to list of markdown windows
    QMap<int, QList<MarkdownWindow*>> pageWindows;
    
    // Current page windows (for quick access)
    QList<MarkdownWindow*> currentWindows;
    
    // Helper methods
    QString getWindowDataFilePath(int pageNumber) const;
    void saveWindowData(int pageNumber, const QList<MarkdownWindow*> &windows);
    QList<MarkdownWindow*> loadWindowData(int pageNumber);
    QString getSaveFolder() const;
    QString getNotebookId() const;
    QRect convertScreenToCanvasRect(const QRect &screenRect) const;
    
public slots:
    void updateAllWindowPositions(); // Update all window positions when canvas pan/zoom changes
};

#endif // MARKDOWNWINDOWMANAGER_H 