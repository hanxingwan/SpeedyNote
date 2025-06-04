#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "InkCanvas.h"
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QLineEdit>
#include <QSlider>
#include <QScrollBar>
#include <QComboBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QDial>
#include <QSoundEffect>
#include <QFont>
#include <QQueue>
#include "SDLControllerManager.h"
#include "ButtonMappingTypes.h"
#include "RecentNotebooksManager.h"
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QTabletEvent>

// #include "HandwritingLineEdit.h"

enum DialMode {
    None,
    PageSwitching,
    ZoomControl,
    ThicknessControl,
    ColorAdjustment,
    ToolSwitching,
    PresetSelection,
    PanAndPageScroll
};

enum class ControllerAction {
    None,
    ToggleFullscreen,
    ToggleDial,
    Zoom50,
    ZoomOut,
    Zoom200,
    AddPreset,
    DeletePage,
    FastForward,
    OpenControlPanel,
    RedColor,
    BlueColor,
    YellowColor,
    GreenColor,
    BlackColor,
    WhiteColor,
    CustomColor,
    ToggleSidebar,
    Save,
    StraightLineTool,
    RopeTool,
    SetPenTool,
    SetMarkerTool,
    SetEraserTool
};

static QString actionToString(ControllerAction action) {
    switch (action) {
        case ControllerAction::ToggleFullscreen: return "Toggle Fullscreen";
        case ControllerAction::ToggleDial: return "Toggle Dial";
        case ControllerAction::Zoom50: return "Zoom 50%";
        case ControllerAction::ZoomOut: return "Zoom Out";
        case ControllerAction::Zoom200: return "Zoom 200%";
        case ControllerAction::AddPreset: return "Add Preset";
        case ControllerAction::DeletePage: return "Delete Page";
        case ControllerAction::FastForward: return "Fast Forward";
        case ControllerAction::OpenControlPanel: return "Open Control Panel";
        case ControllerAction::RedColor: return "Red";
        case ControllerAction::BlueColor: return "Blue";
        case ControllerAction::YellowColor: return "Yellow";
        case ControllerAction::GreenColor: return "Green";
        case ControllerAction::BlackColor: return "Black";
        case ControllerAction::WhiteColor: return "White";
        case ControllerAction::CustomColor: return "Custom Color";
        case ControllerAction::ToggleSidebar: return "Toggle Sidebar";
        case ControllerAction::Save: return "Save";
        case ControllerAction::StraightLineTool: return "Straight Line Tool";
        case ControllerAction::RopeTool: return "Rope Tool";
        case ControllerAction::SetPenTool: return "Set Pen Tool";
        case ControllerAction::SetMarkerTool: return "Set Marker Tool";
        case ControllerAction::SetEraserTool: return "Set Eraser Tool";
        default: return "None";
    }
}

static ControllerAction stringToAction(const QString &str) {
    // Convert internal key to ControllerAction enum
    InternalControllerAction internalAction = ButtonMappingHelper::internalKeyToAction(str);
    
    switch (internalAction) {
        case InternalControllerAction::None: return ControllerAction::None;
        case InternalControllerAction::ToggleFullscreen: return ControllerAction::ToggleFullscreen;
        case InternalControllerAction::ToggleDial: return ControllerAction::ToggleDial;
        case InternalControllerAction::Zoom50: return ControllerAction::Zoom50;
        case InternalControllerAction::ZoomOut: return ControllerAction::ZoomOut;
        case InternalControllerAction::Zoom200: return ControllerAction::Zoom200;
        case InternalControllerAction::AddPreset: return ControllerAction::AddPreset;
        case InternalControllerAction::DeletePage: return ControllerAction::DeletePage;
        case InternalControllerAction::FastForward: return ControllerAction::FastForward;
        case InternalControllerAction::OpenControlPanel: return ControllerAction::OpenControlPanel;
        case InternalControllerAction::RedColor: return ControllerAction::RedColor;
        case InternalControllerAction::BlueColor: return ControllerAction::BlueColor;
        case InternalControllerAction::YellowColor: return ControllerAction::YellowColor;
        case InternalControllerAction::GreenColor: return ControllerAction::GreenColor;
        case InternalControllerAction::BlackColor: return ControllerAction::BlackColor;
        case InternalControllerAction::WhiteColor: return ControllerAction::WhiteColor;
        case InternalControllerAction::CustomColor: return ControllerAction::CustomColor;
        case InternalControllerAction::ToggleSidebar: return ControllerAction::ToggleSidebar;
        case InternalControllerAction::Save: return ControllerAction::Save;
        case InternalControllerAction::StraightLineTool: return ControllerAction::StraightLineTool;
        case InternalControllerAction::RopeTool: return ControllerAction::RopeTool;
        case InternalControllerAction::SetPenTool: return ControllerAction::SetPenTool;
        case InternalControllerAction::SetMarkerTool: return ControllerAction::SetMarkerTool;
        case InternalControllerAction::SetEraserTool: return ControllerAction::SetEraserTool;
    }
    return ControllerAction::None;
}

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

    bool colorButtonsVisible = true;
    bool areColorButtonsVisible() const;
    void setColorButtonsVisible(bool visible);

    bool scrollOnTopEnabled = false;
    bool isScrollOnTopEnabled() const;
    void setScrollOnTopEnabled(bool enabled);

    bool touchGesturesEnabled = false;
    bool areTouchGesturesEnabled() const;
    void setTouchGesturesEnabled(bool enabled);

    SDLControllerManager *controllerManager = nullptr;
    QThread *controllerThread = nullptr;

    QString getHoldMapping(const QString &buttonName);
    QString getPressMapping(const QString &buttonName);

    void saveButtonMappings();
    void loadButtonMappings();

    void setHoldMapping(const QString &buttonName, const QString &dialMode);
    void setPressMapping(const QString &buttonName, const QString &action);
    DialMode dialModeFromString(const QString &mode);

    void importNotebookFromFile(const QString &packageFile);

    int getPdfDPI() const { return pdfRenderDPI; }
    void setPdfDPI(int dpi);

    // void loadUserSettings();  // New
    void savePdfDPI(int dpi); // New

    // Background settings persistence
    void saveDefaultBackgroundSettings(BackgroundStyle style, QColor color, int density);
    void loadDefaultBackgroundSettings(BackgroundStyle &style, QColor &color, int &density);
    
    void migrateOldButtonMappings();
    QString migrateOldDialModeString(const QString &oldString);
    QString migrateOldActionString(const QString &oldString);

    InkCanvas* currentCanvas(); // Made public for RecentNotebooksDialog
    void saveCurrentPage(); // Made public for RecentNotebooksDialog
    void switchPage(int pageNumber); // Made public for RecentNotebooksDialog
    void updateTabLabel(); // Made public for RecentNotebooksDialog
    QSpinBox *pageInput; // Made public for RecentNotebooksDialog
    
    // New: Keyboard mapping methods (made public for ControlPanelDialog)
    void addKeyboardMapping(const QString &keySequence, const QString &action);
    void removeKeyboardMapping(const QString &keySequence);
    QMap<QString, QString> getKeyboardMappings() const;

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

    void updateZoom();
    void onZoomSliderChanged(int value); // Handle manual zoom slider changes
    void applyZoom();
    void updatePanRange();
    void updatePanX(int value);
    void updatePanY(int value);

    void selectBackground(); // Added back

    void forceUIRefresh();

    void switchTab(int index);
    void addNewTab();
    void removeTabAt(int index);
    void toggleZoomSlider();
    void toggleThicknessSlider(); // Added function to toggle thickness slider
    void toggleFullscreen();
    void showJumpToPageDialog();

    void toggleDial();  // ✅ Show/Hide dial
    void handleDialInput(int angle);  // ✅ Handle touch input
    void onDialReleased();
    // void processPageSwitch();
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

    void handleDialPanScroll(int angle);  // Add missing function declaration
    void onPanScrollReleased();           // Add missing function declaration

    // Touch gesture handlers
    void handleTouchZoomChange(int newZoom);
    void handleTouchPanChange(int panX, int panY);
    void handleTouchGestureEnd(); // Add handler for touch gesture completion
    
    // Color button state management
    void updateColorButtonStates();
    void selectColorButton(QPushButton* selectedButton);
    void updateStraightLineButtonState();
    void updateRopeToolButtonState(); // New slot for rope tool button
    void updateDialButtonState();     // New method for dial toggle button
    void updateFastForwardButtonState(); // New method for fast forward toggle button

    QColor getContrastingTextColor(const QColor &backgroundColor);
    void updateCustomColorButtonStyle(const QColor &color);

    void openRecentNotebooksDialog(); // Added slot

    void showPendingTooltip(); // Show tooltip with throttling

private:
    InkCanvas *canvas;
    QPushButton *benchmarkButton;
    QLabel *benchmarkLabel;
    QTimer *benchmarkTimer;
    bool benchmarking;

    QPushButton *exportNotebookButton;
    QPushButton *importNotebookButton;

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
    QPushButton *openRecentNotebooksButton; // Added button

    QPushButton *loadPdfButton;
    QPushButton *clearPdfButton;
    QPushButton *toggleTabBarButton;

    QMap<InkCanvas*, int> pageMap;
    

    QPushButton *backgroundButton; // New button to set background
    QPushButton *straightLineToggleButton; // Button to toggle straight line mode
    QPushButton *ropeToolButton; // Button to toggle rope tool mode

    QSlider *zoomSlider;
    QPushButton *zoomButton;
    QFrame *zoomFrame;
    QPushButton *dezoomButton;
    QPushButton *zoom50Button;
    QPushButton *zoom200Button;
    QWidget *zoomContainer;
    QLineEdit *zoomInput;
    QScrollBar *panXSlider;
    QScrollBar *panYSlider;


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

    DialMode currentDialMode = PageSwitching; ✅ Default mode

    QComboBox *dialModeSelector; ✅ Mode selector
    QComboBox *channelSelector; ✅ Channel selector
    int selectedChannel = 0; ✅ Default channel
    */
    
    DialMode currentDialMode = PanAndPageScroll; // ✅ Default mode
    DialMode temporaryDialMode = None;

    QComboBox *dialModeSelector; // ✅ Mode selector
    QComboBox *channelSelector; // ✅ Channel selector
    int selectedChannel = 0; // ✅ Default channel
    QPushButton *colorPreview; // ✅ Color preview

    QLabel *dialDisplay = nullptr; // ✅ Display for dial mode

    QFrame *dialColorPreview;
    QLabel *dialIconView;
    QFont pixelFont; // ✅ Font for pixel effect
    // QLabel *modeIndicator; ✅ Indicator for mode selection
    // QLabel *dialNeedle;

    QPushButton *btnPageSwitch;
    QPushButton *btnZoom;
    QPushButton *btnThickness;
    QPushButton *btnColor;
    QPushButton *btnTool;
    QPushButton *btnPresets;
    QPushButton *btnPannScroll;
    int tempClicks = 0;

    QPushButton *dialHiddenButton; // ✅ Invisible tap button over OLED display

    QQueue<QColor> colorPresets; // ✅ FIFO queue for color presets
    QPushButton *addPresetButton; // ✅ Button to add current color to queue
    int currentPresetIndex = 0; // ✅ Track selected preset

    qreal initialDpr = 1.0; // Will be set in constructor

    QWidget *sidebarContainer;  // Container for sidebar
    QWidget *controlBar;        // Control toolbar

    void setTemporaryDialMode(DialMode mode);
    void clearTemporaryDialMode();

    bool controlBarVisible = true;  // Track controlBar visibility state
    void toggleControlBar();        // Function to toggle controlBar visibility
    bool sidebarWasVisibleBeforeFullscreen = true;  // Track sidebar state before fullscreen

    int accumulatedRotationAfterLimit = 0; 

    int pendingPageFlip = 0;  // -1 for previous, +1 for next, 0 for no flip. This is used for mode PanAndPageScroll

    // Add in MainWindow class:
    QMap<QString, QString> buttonHoldMapping;
    QMap<QString, QString> buttonPressMapping;
    QMap<QString, ControllerAction> buttonPressActionMapping;
    
    // New: Keyboard mapping support
    QMap<QString, QString> keyboardMappings;  // keySequence -> action internal key
    QMap<QString, ControllerAction> keyboardActionMapping;  // keySequence -> action enum

    // Tooltip handling for pen input
    QTimer *tooltipTimer;
    QWidget *lastHoveredWidget;
    QPoint pendingTooltipPos;

    void handleButtonHeld(const QString &buttonName);
    void handleButtonReleased(const QString &buttonName);

    void handleControllerButton(const QString &buttonName);
    
    // New: Keyboard mapping methods
    void handleKeyboardShortcut(const QString &keySequence);
    void saveKeyboardMappings();
    void loadKeyboardMappings();

    void ensureTabHasUniqueSaveFolder(InkCanvas* canvas);

    RecentNotebooksManager *recentNotebooksManager; // Added manager instance

    int pdfRenderDPI = 288;  // Default to 288 DPI

    void setupUi();

    void loadUserSettings();

    bool scrollbarsVisible = false;
    QTimer *scrollbarHideTimer = nullptr;
    
    // Event filter for scrollbar hover detection and dial container drag
    bool eventFilter(QObject *obj, QEvent *event) override;
    
    // Update scrollbar positions based on container size
    void updateScrollbarPositions();
    
    // Handle edge proximity detection for scrollbar visibility
    void handleEdgeProximity(InkCanvas* canvas, const QPoint& pos);
    
    // Responsive toolbar management
    bool isToolbarTwoRows = false;
    QVBoxLayout *controlLayoutVertical = nullptr;
    QHBoxLayout *controlLayoutSingle = nullptr;
    QHBoxLayout *controlLayoutFirstRow = nullptr;
    QHBoxLayout *controlLayoutSecondRow = nullptr;
    void updateToolbarLayout();
    void createSingleRowLayout();
    void createTwoRowLayout();
    
    // Add timer for delayed layout updates
    QTimer *layoutUpdateTimer = nullptr;
    
    // Separator line for 2-row layout
    QFrame *separatorLine = nullptr;
    
protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;  // New: Handle keyboard shortcuts
    void tabletEvent(QTabletEvent *event) override; // Handle pen hover for tooltips
};

#endif // MAINWINDOW_H
