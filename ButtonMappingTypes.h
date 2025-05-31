#ifndef BUTTONMAPPINGTYPES_H
#define BUTTONMAPPINGTYPES_H

#include <QString>
#include <QStringList>
#include <QCoreApplication>

// Internal enum for dial modes (language-independent)
enum class InternalDialMode {
    None,
    PageSwitching,
    ZoomControl,
    ThicknessControl,
    ColorAdjustment,
    ToolSwitching,
    PresetSelection,
    PanAndPageScroll
};

// Internal enum for controller actions (language-independent)
enum class InternalControllerAction {
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
    ToggleSidebar,        // New: Hide/show sidebar
    Save,                 // New: Save current page
    StraightLineTool,     // New: Toggle straight line tool
    RopeTool,            // New: Toggle rope tool
    SetPenTool,          // New: Set tool to pen
    SetMarkerTool,       // New: Set tool to marker
    SetEraserTool        // New: Set tool to eraser
};

class ButtonMappingHelper {
    Q_DECLARE_TR_FUNCTIONS(ButtonMappingHelper)

public:
    // Get internal key from display string
    static QString dialModeToInternalKey(InternalDialMode mode);
    static QString actionToInternalKey(InternalControllerAction action);
    
    // Get enum from internal key
    static InternalDialMode internalKeyToDialMode(const QString &key);
    static InternalControllerAction internalKeyToAction(const QString &key);
    
    // Get translated display strings
    static QStringList getTranslatedDialModes();
    static QStringList getTranslatedActions();
    static QStringList getTranslatedButtons();
    
    // Get internal keys (for storage)
    static QStringList getInternalDialModeKeys();
    static QStringList getInternalActionKeys();
    static QStringList getInternalButtonKeys();
    
    // Convert between display string and internal key
    static QString displayToInternalKey(const QString &displayString, bool isDialMode);
    static QString internalKeyToDisplay(const QString &internalKey, bool isDialMode);
};

#endif // BUTTONMAPPINGTYPES_H 