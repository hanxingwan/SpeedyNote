#include "ButtonMappingTypes.h"

QString ButtonMappingHelper::dialModeToInternalKey(InternalDialMode mode) {
    switch (mode) {
        case InternalDialMode::None: return "none";
        case InternalDialMode::PageSwitching: return "page_switching";
        case InternalDialMode::ZoomControl: return "zoom_control";
        case InternalDialMode::ThicknessControl: return "thickness_control";
        case InternalDialMode::ColorAdjustment: return "color_adjustment";
        case InternalDialMode::ToolSwitching: return "tool_switching";
        case InternalDialMode::PresetSelection: return "preset_selection";
        case InternalDialMode::PanAndPageScroll: return "pan_and_page_scroll";
    }
    return "none";
}

QString ButtonMappingHelper::actionToInternalKey(InternalControllerAction action) {
    switch (action) {
        case InternalControllerAction::None: return "none";
        case InternalControllerAction::ToggleFullscreen: return "toggle_fullscreen";
        case InternalControllerAction::ToggleDial: return "toggle_dial";
        case InternalControllerAction::Zoom50: return "zoom_50";
        case InternalControllerAction::ZoomOut: return "zoom_out";
        case InternalControllerAction::Zoom200: return "zoom_200";
        case InternalControllerAction::AddPreset: return "add_preset";
        case InternalControllerAction::DeletePage: return "delete_page";
        case InternalControllerAction::FastForward: return "fast_forward";
        case InternalControllerAction::OpenControlPanel: return "open_control_panel";
        case InternalControllerAction::RedColor: return "red_color";
        case InternalControllerAction::BlueColor: return "blue_color";
        case InternalControllerAction::YellowColor: return "yellow_color";
        case InternalControllerAction::GreenColor: return "green_color";
        case InternalControllerAction::BlackColor: return "black_color";
        case InternalControllerAction::WhiteColor: return "white_color";
        case InternalControllerAction::CustomColor: return "custom_color";
        case InternalControllerAction::ToggleSidebar: return "toggle_sidebar";
        case InternalControllerAction::Save: return "save";
        case InternalControllerAction::StraightLineTool: return "straight_line_tool";
        case InternalControllerAction::RopeTool: return "rope_tool";
        case InternalControllerAction::SetPenTool: return "set_pen_tool";
        case InternalControllerAction::SetMarkerTool: return "set_marker_tool";
        case InternalControllerAction::SetEraserTool: return "set_eraser_tool";
    }
    return "none";
}

InternalDialMode ButtonMappingHelper::internalKeyToDialMode(const QString &key) {
    if (key == "none") return InternalDialMode::None;
    if (key == "page_switching") return InternalDialMode::PageSwitching;
    if (key == "zoom_control") return InternalDialMode::ZoomControl;
    if (key == "thickness_control") return InternalDialMode::ThicknessControl;
    if (key == "color_adjustment") return InternalDialMode::ColorAdjustment;
    if (key == "tool_switching") return InternalDialMode::ToolSwitching;
    if (key == "preset_selection") return InternalDialMode::PresetSelection;
    if (key == "pan_and_page_scroll") return InternalDialMode::PanAndPageScroll;
    return InternalDialMode::None;
}

InternalControllerAction ButtonMappingHelper::internalKeyToAction(const QString &key) {
    if (key == "none") return InternalControllerAction::None;
    if (key == "toggle_fullscreen") return InternalControllerAction::ToggleFullscreen;
    if (key == "toggle_dial") return InternalControllerAction::ToggleDial;
    if (key == "zoom_50") return InternalControllerAction::Zoom50;
    if (key == "zoom_out") return InternalControllerAction::ZoomOut;
    if (key == "zoom_200") return InternalControllerAction::Zoom200;
    if (key == "add_preset") return InternalControllerAction::AddPreset;
    if (key == "delete_page") return InternalControllerAction::DeletePage;
    if (key == "fast_forward") return InternalControllerAction::FastForward;
    if (key == "open_control_panel") return InternalControllerAction::OpenControlPanel;
    if (key == "red_color") return InternalControllerAction::RedColor;
    if (key == "blue_color") return InternalControllerAction::BlueColor;
    if (key == "yellow_color") return InternalControllerAction::YellowColor;
    if (key == "green_color") return InternalControllerAction::GreenColor;
    if (key == "black_color") return InternalControllerAction::BlackColor;
    if (key == "white_color") return InternalControllerAction::WhiteColor;
    if (key == "custom_color") return InternalControllerAction::CustomColor;
    if (key == "toggle_sidebar") return InternalControllerAction::ToggleSidebar;
    if (key == "save") return InternalControllerAction::Save;
    if (key == "straight_line_tool") return InternalControllerAction::StraightLineTool;
    if (key == "rope_tool") return InternalControllerAction::RopeTool;
    if (key == "set_pen_tool") return InternalControllerAction::SetPenTool;
    if (key == "set_marker_tool") return InternalControllerAction::SetMarkerTool;
    if (key == "set_eraser_tool") return InternalControllerAction::SetEraserTool;
    return InternalControllerAction::None;
}

QStringList ButtonMappingHelper::getTranslatedDialModes() {
    return {
        tr("None"),
        tr("Page Switching"),
        tr("Zoom Control"),
        tr("Thickness Control"),
        tr("Color Adjustment"),
        tr("Tool Switching"),
        tr("Preset Selection"),
        tr("Pan and Page Scroll")
    };
}

QStringList ButtonMappingHelper::getTranslatedActions() {
    return {
        tr("None"),
        tr("Toggle Fullscreen"),
        tr("Toggle Dial"),
        tr("Zoom 50%"),
        tr("Zoom Out"),
        tr("Zoom 200%"),
        tr("Add Preset"),
        tr("Delete Page"),
        tr("Fast Forward"),
        tr("Open Control Panel"),
        tr("Red"),
        tr("Blue"),
        tr("Yellow"),
        tr("Green"),
        tr("Black"),
        tr("White"),
        tr("Custom Color"),
        tr("Toggle Sidebar"),
        tr("Save"),
        tr("Straight Line Tool"),
        tr("Rope Tool"),
        tr("Set Pen Tool"),
        tr("Set Marker Tool"),
        tr("Set Eraser Tool")
    };
}

QStringList ButtonMappingHelper::getTranslatedButtons() {
    return {
        tr("Left Shoulder"),
        tr("Right Shoulder"),
        tr("Paddle 2"),
        tr("Paddle 4"),
        tr("Y Button"),
        tr("A Button"),
        tr("B Button"),
        tr("X Button"),
        tr("Left Stick"),
        tr("Start Button"),
        tr("Guide Button")
    };
}

QStringList ButtonMappingHelper::getInternalDialModeKeys() {
    return {
        "none",
        "page_switching",
        "zoom_control",
        "thickness_control",
        "color_adjustment",
        "tool_switching",
        "preset_selection",
        "pan_and_page_scroll"
    };
}

QStringList ButtonMappingHelper::getInternalActionKeys() {
    return {
        "none",
        "toggle_fullscreen",
        "toggle_dial",
        "zoom_50",
        "zoom_out",
        "zoom_200",
        "add_preset",
        "delete_page",
        "fast_forward",
        "open_control_panel",
        "red_color",
        "blue_color",
        "yellow_color",
        "green_color",
        "black_color",
        "white_color",
        "custom_color",
        "toggle_sidebar",
        "save",
        "straight_line_tool",
        "rope_tool",
        "set_pen_tool",
        "set_marker_tool",
        "set_eraser_tool"
    };
}

QStringList ButtonMappingHelper::getInternalButtonKeys() {
    return {
        "LEFTSHOULDER",
        "RIGHTSHOULDER", 
        "PADDLE2",
        "PADDLE4",
        "Y",
        "A",
        "B",
        "X",
        "LEFTSTICK",
        "START",
        "GUIDE"
    };
}

QString ButtonMappingHelper::displayToInternalKey(const QString &displayString, bool isDialMode) {
    if (isDialMode) {
        QStringList translated = getTranslatedDialModes();
        QStringList internal = getInternalDialModeKeys();
        int index = translated.indexOf(displayString);
        return (index >= 0) ? internal[index] : "none";
    } else {
        QStringList translated = getTranslatedActions();
        QStringList internal = getInternalActionKeys();
        int index = translated.indexOf(displayString);
        return (index >= 0) ? internal[index] : "none";
    }
}

QString ButtonMappingHelper::internalKeyToDisplay(const QString &internalKey, bool isDialMode) {
    if (isDialMode) {
        QStringList translated = getTranslatedDialModes();
        QStringList internal = getInternalDialModeKeys();
        int index = internal.indexOf(internalKey);
        return (index >= 0) ? translated[index] : tr("None");
    } else {
        QStringList translated = getTranslatedActions();
        QStringList internal = getInternalActionKeys();
        int index = internal.indexOf(internalKey);
        return (index >= 0) ? translated[index] : tr("None");
    }
} 