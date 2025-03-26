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


    BackgroundStyle backgroundStyle = BackgroundStyle::None;
    QColor backgroundColor = Qt::transparent;
    int backgroundDensity = 40;  // pixels between lines


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

    void loadPdfPreviewAsync(int pageNumber);  // ✅ Load a quick preview of the PDF page
    // for notebook background below
    void setBackgroundStyle(BackgroundStyle style);
    void setBackgroundColor(const QColor &color);
    void setBackgroundDensity(int density);

    BackgroundStyle getBackgroundStyle() const { return backgroundStyle; }
    QColor getBackgroundColor() const { return backgroundColor; }
    int getBackgroundDensity() const { return backgroundDensity; }

    void saveBackgroundMetadata();  // ✅ Save background metadata
    

    

protected:
    void paintEvent(QPaintEvent *event) override;
    void tabletEvent(QTabletEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override; // Handle resizing
    

private:
    QPixmap buffer;            // Off-screen buffer
    QImage background;
    QPointF lastPoint;
    bool drawing;
    QColor penColor; // Added pen color property
    qreal penThickness; // Added pen thickness property
    ToolType currentTool;
    ToolType previousTool; // To restore tool after erasing
    QString saveFolder; // Folder to save images
    QPixmap backgroundImage;

    int zoomFactor;     // Zoom percentage (100 = normal)
    int panOffsetX;     // Horizontal pan offset
    int panOffsetY;     // Vertical pan offset

    void initializeBuffer();   // Helper to initialize the buffer
    void drawStroke(const QPointF &start, const QPointF &end, qreal pressure);    
    void eraseStroke(const QPointF &start, const QPointF &end, qreal pressure);
    

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


};

#endif // INKCANVAS_H