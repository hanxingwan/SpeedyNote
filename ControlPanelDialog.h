#ifndef CONTROLPANELDIALOG_H
#define CONTROLPANELDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QColorDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QColor>

#include "InkCanvas.h" // Needed for BackgroundStyle enum
#include "MainWindow.h"

class ControlPanelDialog : public QDialog {
    Q_OBJECT

public:
    explicit ControlPanelDialog(MainWindow *mainWindow, InkCanvas *targetCanvas, QWidget *parent = nullptr);

private slots:
    void applyChanges();
    void chooseColor();

private:
    InkCanvas *canvas;

    QTabWidget *tabWidget;
    QWidget *backgroundTab;

    QComboBox *styleCombo;
    QPushButton *colorButton;
    QSpinBox *densitySpin;

    QPushButton *applyButton;
    QPushButton *okButton;
    QPushButton *cancelButton;

    QColor selectedColor;

    void createBackgroundTab();
    void loadFromCanvas();

    MainWindow *mainWindowRef;
    InkCanvas *canvasRef;
    QWidget *performanceTab;
    QWidget *toolbarTab;
    void createToolbarTab();
    void createPerformanceTab();

    QWidget *controllerMappingTab;

    // Mapping comboboxes for hold and press
    QMap<QString, QComboBox*> holdMappingCombos;
    QMap<QString, QComboBox*> pressMappingCombos;

    void createButtonMappingTab();
};

#endif // CONTROLPANELDIALOG_H
