#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "InkCanvas.h"
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QLineEdit>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QDial>
#include <QSoundEffect>
#include <QFont>
#include <QQueue>
// #include "HandwritingLineEdit.h"

enum DialMode {
    PageSwitching,
    ZoomControl,
    ThicknessControl,
    ColorAdjustment,
    ToolSwitching,
    PresetSelection
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow();
    int getCurrentPageForCanvas(InkCanvas *canvas); 

    bool lowResPreviewEnabled = true;

    void setLowResPreviewEnabled(bool enabled);
    bool isLowResPreviewEnabled() const;

    bool areBenchmarkControlsVisible() const;
    void setBenchmarkControlsVisible(bool visible);

    bool scrollOnTopEnabled = false;
    bool isScrollOnTopEnabled() const;
    void setScrollOnTopEnabled(bool enabled);

    

private slots:
    void toggleBenchmark();
    void updateBenchmarkDisplay();
    void applyCustomColor(); // Added function for custom color input
    void updateThickness(int value); // New function for thickness control
    void changeTool(int index);
    void selectFolder(); // Select save folder
    void saveCanvas(); // Save canvas to file
    void saveAnnotated();
    void deleteCurrentPage();

    void loadPdf();
    void clearPdf();

    void saveCurrentPage();
    void switchPage(int pageNumber);
    void selectBackground();

    void updateZoom();
    void applyZoom();
    void updatePanRange();
    void updatePanX(int value);
    void updatePanY(int value);


    void forceUIRefresh();


    void switchTab(int index);
    void addNewTab();
    void removeTabAt(int index);
    void updateTabLabel();
    void toggleZoomSlider();
    void toggleThicknessSlider(); // Added function to toggle thickness slider
    void toggleFullscreen();
    void showJumpToPageDialog();

    void toggleDial();  // ✅ Show/Hide dial
    void handleDialInput(int angle);  // ✅ Handle touch input
    void onDialReleased();
    // void processPageSwitch();
    bool eventFilter(QObject *watched, QEvent *event) override;
    void initializeDialSound();

    void changeDialMode(DialMode mode);
    void handleDialZoom(int angle);
    void handleDialThickness(int angle); // Added function to handle thickness control
    void onZoomReleased();
    void onThicknessReleased(); // Added function to handle thickness control
    void handleDialColor(int angle); // Added function to handle color adjustment
    void onColorReleased(); // Added function to handle color adjustment
    // void updateCustomColor();
    void updateSelectedChannel(int index);
    void updateDialDisplay();
    // void handleModeSelection(int angle);

    void handleToolSelection(int angle);
    void onToolReleased();
    void cycleColorChannel();

    void handlePresetSelection(int angle);
    void onPresetReleased();
    void addColorPreset();
    qreal getDevicePixelRatio(); 

    bool isDarkMode();
    QIcon loadThemedIcon(const QString& baseName);

    



    

    

    

private:
    InkCanvas *canvas;
    InkCanvas* currentCanvas();
    QPushButton *benchmarkButton;
    QLabel *benchmarkLabel;
    QTimer *benchmarkTimer;
    bool benchmarking;
    QPushButton *redButton;
    QPushButton *blueButton;
    QPushButton *yellowButton;
    QPushButton *greenButton;
    QPushButton *blackButton;
    QPushButton *whiteButton;
    QLineEdit *customColorInput;
    QPushButton *customColorButton;
    QPushButton *thicknessButton; // Added thickness button
    QSlider *thicknessSlider; // Added thickness slider
    QFrame *thicknessFrame; // Added thickness frame
    QComboBox *toolSelector;
    QPushButton *deletePageButton;
    QPushButton *selectFolderButton; // Button to select folder
    QPushButton *saveButton; // Button to save file
    QPushButton *saveAnnotatedButton;
    QPushButton *fullscreenButton;
    QPushButton *openControlPanelButton;

    QPushButton *loadPdfButton;
    QPushButton *clearPdfButton;
    QPushButton *toggleTabBarButton;

    QMap<InkCanvas*, int> pageMap;
    

    QSpinBox *pageInput;
    QPushButton *backgroundButton; // New button to set background

    QSlider *zoomSlider;
    QPushButton *zoomButton;
    QFrame *zoomFrame;
    QPushButton *dezoomButton;
    QPushButton *zoom50Button;
    QPushButton *zoom200Button;
    QWidget *zoomContainer;
    QLineEdit *zoomInput;
    QSlider *panXSlider;
    QSlider *panYSlider;


    QListWidget *tabList;          // Sidebar for tabs
    QStackedWidget *canvasStack;   // Holds multiple InkCanvas instances
    QPushButton *addTabButton;     // Button to add tabs
    QPushButton *jumpToPageButton; // Button to jump to a specific page

    QWidget *dialContainer = nullptr;  // ✅ Floating dial container
    QDial *pageDial = nullptr;  // ✅ The dial itself
    QDial *modeDial = nullptr;  // ✅ Mode dial
    QPushButton *dialToggleButton;  // ✅ Toggle button
    bool fastForwardMode = false;  // ✅ Toggle fast forward
    QPushButton *fastForwardButton;  // ✅ Fast forward button    
    int lastAngle = 0;
    int startAngle = 0;
    bool tracking = false;
    int accumulatedRotation = 0;
    QSoundEffect *dialClickSound = nullptr;

    int grossTotalClicks = 0;

    /*
    enum DialMode {
        PageSwitching,
        ZoomControl,
        PenThickness,
        ColorAdjustment
    };

    DialMode currentDialMode = PageSwitching;  // ✅ Default mode

    QComboBox *dialModeSelector;  // ✅ Mode selector
    QComboBox *channelSelector;  // ✅ Channel selector
    int selectedChannel = 0;  // ✅ Default channel
    */
    
    DialMode currentDialMode = PageSwitching;  // ✅ Default mode
    QComboBox *dialModeSelector;  // ✅ Mode selector
    QComboBox *channelSelector;  // ✅ Channel selector
    int selectedChannel = 0;  // ✅ Default channel
    QPushButton *colorPreview;  // ✅ Color preview

    QLabel *dialDisplay = nullptr;  // ✅ Display for dial mode

    QFrame *dialColorPreview;
    QLabel *dialIconView;
    QFont pixelFont;  // ✅ Font for pixel effect
    // QLabel *modeIndicator;  // ✅ Indicator for mode selection
    // QLabel *dialNeedle;

    QPushButton *btnPageSwitch;
    QPushButton *btnZoom;
    QPushButton *btnThickness;
    QPushButton *btnColor;
    QPushButton *btnTool;
    QPushButton *btnPresets;
    int tempClicks = 0;

    QPushButton *dialHiddenButton;  // ✅ Invisible tap button over OLED display

    QQueue<QColor> colorPresets;  // ✅ FIFO queue for color presets
    QPushButton *addPresetButton;  // ✅ Button to add current color to queue
    int currentPresetIndex = 0;  // ✅ Track selected preset

    qreal initialDpr = getDevicePixelRatio(); // Get initial device pixel ratio

    QWidget *sidebarContainer;  // Container for sidebar
    

    void setupUi();

    
};

#endif // MAINWINDOW_H