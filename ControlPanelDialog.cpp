#include "ControlPanelDialog.h"
#include "ButtonMappingTypes.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>

ControlPanelDialog::ControlPanelDialog(MainWindow *mainWindow, InkCanvas *targetCanvas, QWidget *parent)
    : QDialog(parent), canvas(targetCanvas), selectedColor(canvas->getBackgroundColor()), mainWindowRef(mainWindow) {

    setWindowTitle(tr("Canvas Control Panel"));
    resize(400, 200);

    tabWidget = new QTabWidget(this);

    // === Tabs ===
    createBackgroundTab();
    tabWidget->addTab(backgroundTab, tr("Background"));
    if (mainWindowRef) {
        createPerformanceTab();
        tabWidget->addTab(performanceTab, tr("Performance"));
        createToolbarTab();
    }
    createButtonMappingTab();
    createKeyboardMappingTab();
    // === Buttons ===
    applyButton = new QPushButton(tr("Apply"));
    okButton = new QPushButton(tr("OK"));
    cancelButton = new QPushButton(tr("Cancel"));

    connect(applyButton, &QPushButton::clicked, this, &ControlPanelDialog::applyChanges);
    connect(okButton, &QPushButton::clicked, this, [=]() {
        applyChanges();
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, &ControlPanelDialog::reject);

    // === Layout ===
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabWidget);
    mainLayout->addLayout(buttonLayout);

    loadFromCanvas();
}


void ControlPanelDialog::createBackgroundTab() {
    backgroundTab = new QWidget(this);

    QLabel *styleLabel = new QLabel(tr("Background Style:"));
    styleCombo = new QComboBox();
    styleCombo->addItem(tr("None"), static_cast<int>(BackgroundStyle::None));
    styleCombo->addItem(tr("Grid"), static_cast<int>(BackgroundStyle::Grid));
    styleCombo->addItem(tr("Lines"), static_cast<int>(BackgroundStyle::Lines));

    QLabel *colorLabel = new QLabel(tr("Background Color:"));
    colorButton = new QPushButton();
    connect(colorButton, &QPushButton::clicked, this, &ControlPanelDialog::chooseColor);

    QLabel *densityLabel = new QLabel(tr("Density:"));
    densitySpin = new QSpinBox();
    densitySpin->setRange(10, 200);
    densitySpin->setSuffix(" px");
    densitySpin->setSingleStep(5);

    QGridLayout *layout = new QGridLayout(backgroundTab);
    layout->addWidget(styleLabel, 0, 0);
    layout->addWidget(styleCombo, 0, 1);
    layout->addWidget(colorLabel, 1, 0);
    layout->addWidget(colorButton, 1, 1);
    layout->addWidget(densityLabel, 2, 0);
    layout->addWidget(densitySpin, 2, 1);
    // layout->setColumnStretch(1, 1); // Stretch the second column
    layout->setRowStretch(3, 1); // Stretch the last row
}

void ControlPanelDialog::chooseColor() {
    QColor chosen = QColorDialog::getColor(selectedColor, this, tr("Select Background Color"));
    if (chosen.isValid()) {
        selectedColor = chosen;
        colorButton->setStyleSheet(QString("background-color: %1").arg(selectedColor.name()));
    }
}

void ControlPanelDialog::applyChanges() {
    if (!canvas) return;

    BackgroundStyle style = static_cast<BackgroundStyle>(
        styleCombo->currentData().toInt()
    );

    canvas->setBackgroundStyle(style);
    canvas->setBackgroundColor(selectedColor);
    canvas->setBackgroundDensity(densitySpin->value());
    canvas->update();
    canvas->saveBackgroundMetadata();
    
    // ✅ Save these settings as defaults for new tabs
    if (mainWindowRef) {
        mainWindowRef->saveDefaultBackgroundSettings(style, selectedColor, densitySpin->value());
    }

    // ✅ Apply button mappings back to MainWindow with internal keys
    if (mainWindowRef) {
        for (const QString &buttonKey : holdMappingCombos.keys()) {
            QString displayString = holdMappingCombos[buttonKey]->currentText();
            QString internalKey = ButtonMappingHelper::displayToInternalKey(displayString, true);  // true = isDialMode
            mainWindowRef->setHoldMapping(buttonKey, internalKey);
        }
        for (const QString &buttonKey : pressMappingCombos.keys()) {
            QString displayString = pressMappingCombos[buttonKey]->currentText();
            QString internalKey = ButtonMappingHelper::displayToInternalKey(displayString, false);  // false = isAction
            mainWindowRef->setPressMapping(buttonKey, internalKey);
        }

        // ✅ Save to persistent settings
        mainWindowRef->saveButtonMappings();
    }
}

void ControlPanelDialog::loadFromCanvas() {
    styleCombo->setCurrentIndex(static_cast<int>(canvas->getBackgroundStyle()));
    densitySpin->setValue(canvas->getBackgroundDensity());
    selectedColor = canvas->getBackgroundColor();

    colorButton->setStyleSheet(QString("background-color: %1").arg(selectedColor.name()));

    if (mainWindowRef) {
        for (const QString &buttonKey : holdMappingCombos.keys()) {
            QString internalKey = mainWindowRef->getHoldMapping(buttonKey);
            QString displayString = ButtonMappingHelper::internalKeyToDisplay(internalKey, true);  // true = isDialMode
            int index = holdMappingCombos[buttonKey]->findText(displayString);
            if (index >= 0) holdMappingCombos[buttonKey]->setCurrentIndex(index);
        }

        for (const QString &buttonKey : pressMappingCombos.keys()) {
            QString internalKey = mainWindowRef->getPressMapping(buttonKey);
            QString displayString = ButtonMappingHelper::internalKeyToDisplay(internalKey, false);  // false = isAction
            int index = pressMappingCombos[buttonKey]->findText(displayString);
            if (index >= 0) pressMappingCombos[buttonKey]->setCurrentIndex(index);
        }
    }
}


void ControlPanelDialog::createPerformanceTab() {
    performanceTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(performanceTab);

    QCheckBox *previewToggle = new QCheckBox(tr("Enable Low-Resolution PDF Previews"));
    previewToggle->setChecked(mainWindowRef->isLowResPreviewEnabled());

    connect(previewToggle, &QCheckBox::toggled, mainWindowRef, &MainWindow::setLowResPreviewEnabled);

    QLabel *note = new QLabel(tr("Disabling this may improve dial smoothness on low-end devices."));
    note->setWordWrap(true);
    note->setStyleSheet("color: gray; font-size: 10px;");

    QLabel *dpiLabel = new QLabel(tr("PDF Rendering DPI:"));
    QComboBox *dpiSelector = new QComboBox();
    dpiSelector->addItems({"96", "192", "288", "384", "480"});
    dpiSelector->setCurrentText(QString::number(mainWindowRef->getPdfDPI()));

    connect(dpiSelector, &QComboBox::currentTextChanged, this, [=](const QString &value) {
        mainWindowRef->setPdfDPI(value.toInt());
    });

    QLabel *notePDF = new QLabel(tr("Adjust how the PDF is rendered. Higher DPI means better quality but slower performance. DO NOT CHANGE THIS OPTION WHEN MULTIPLE TABS ARE OPEN. THIS MAY LEAD TO UNDEFINED BEHAVIOR!"));
    notePDF->setWordWrap(true);
    notePDF->setStyleSheet("color: gray; font-size: 10px;");


    layout->addWidget(previewToggle);
    layout->addWidget(note);
    layout->addWidget(dpiLabel);
    layout->addWidget(dpiSelector);
    layout->addWidget(notePDF);

    layout->addStretch();

    // return performanceTab;
}

void ControlPanelDialog::createToolbarTab(){
    toolbarTab = new QWidget(this);
    QVBoxLayout *toolbarLayout = new QVBoxLayout(toolbarTab);

    // ✅ Checkbox to show/hide benchmark controls
    QCheckBox *benchmarkVisibilityCheckbox = new QCheckBox(tr("Show Benchmark Controls"), toolbarTab);
    benchmarkVisibilityCheckbox->setChecked(mainWindowRef->areBenchmarkControlsVisible());
    toolbarLayout->addWidget(benchmarkVisibilityCheckbox);
    QLabel *benchmarkNote = new QLabel(tr("This will show/hide the benchmark controls on the toolbar. Press the clock button to start/stop the benchmark."));
    benchmarkNote->setWordWrap(true);
    benchmarkNote->setStyleSheet("color: gray; font-size: 10px;");
    toolbarLayout->addWidget(benchmarkNote);

    // ✅ Checkbox to show/hide color buttons
    QCheckBox *colorButtonsVisibilityCheckbox = new QCheckBox(tr("Show Color Buttons"), toolbarTab);
    colorButtonsVisibilityCheckbox->setChecked(mainWindowRef->areColorButtonsVisible());
    toolbarLayout->addWidget(colorButtonsVisibilityCheckbox);
    QLabel *colorButtonsNote = new QLabel(tr("This will show/hide the color buttons on the toolbar"));
    colorButtonsNote->setWordWrap(true);
    colorButtonsNote->setStyleSheet("color: gray; font-size: 10px;");
    toolbarLayout->addWidget(colorButtonsNote);

    QCheckBox *scrollOnTopCheckBox = new QCheckBox(tr("Scroll on Top after Page Switching"), toolbarTab);
    scrollOnTopCheckBox->setChecked(mainWindowRef->isScrollOnTopEnabled());
    toolbarLayout->addWidget(scrollOnTopCheckBox);
    QLabel *scrollNote = new QLabel(tr("Enabling this will make the page scroll to the top after switching to a new page."));
    scrollNote->setWordWrap(true);
    scrollNote->setStyleSheet("color: gray; font-size: 10px;");
    toolbarLayout->addWidget(scrollNote);
    
    // ✅ Checkbox to enable/disable touch gestures
    QCheckBox *touchGesturesCheckbox = new QCheckBox(tr("Enable Touch Gestures"), toolbarTab);
    touchGesturesCheckbox->setChecked(mainWindowRef->areTouchGesturesEnabled());
    toolbarLayout->addWidget(touchGesturesCheckbox);
    QLabel *touchGesturesNote = new QLabel(tr("Enable pinch to zoom and touch panning on the canvas. When disabled, only pen input is accepted."));
    touchGesturesNote->setWordWrap(true);
    touchGesturesNote->setStyleSheet("color: gray; font-size: 10px;");
    toolbarLayout->addWidget(touchGesturesNote);
    
    toolbarLayout->addStretch();
    toolbarTab->setLayout(toolbarLayout);
    tabWidget->addTab(toolbarTab, tr("Features"));


    // Connect the checkbox
    connect(benchmarkVisibilityCheckbox, &QCheckBox::toggled, mainWindowRef, &MainWindow::setBenchmarkControlsVisible);
    connect(colorButtonsVisibilityCheckbox, &QCheckBox::toggled, mainWindowRef, &MainWindow::setColorButtonsVisible);
    connect(scrollOnTopCheckBox, &QCheckBox::toggled, mainWindowRef, &MainWindow::setScrollOnTopEnabled);
    connect(touchGesturesCheckbox, &QCheckBox::toggled, mainWindowRef, &MainWindow::setTouchGesturesEnabled);
}


void ControlPanelDialog::createButtonMappingTab() {
    QWidget *buttonTab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(buttonTab);

    QStringList buttonKeys = ButtonMappingHelper::getInternalButtonKeys();
    QStringList buttonDisplayNames = ButtonMappingHelper::getTranslatedButtons();
    QStringList dialModes = ButtonMappingHelper::getTranslatedDialModes();
    QStringList actions = ButtonMappingHelper::getTranslatedActions();

    for (int i = 0; i < buttonKeys.size(); ++i) {
        const QString &buttonKey = buttonKeys[i];
        const QString &buttonDisplayName = buttonDisplayNames[i];
        
        QHBoxLayout *h = new QHBoxLayout();
        h->addWidget(new QLabel(buttonDisplayName));  // Use translated button name

        QComboBox *holdCombo = new QComboBox();
        holdCombo->addItems(dialModes);  // Add translated dial mode names
        holdMappingCombos[buttonKey] = holdCombo;
        h->addWidget(new QLabel(tr("Hold:")));
        h->addWidget(holdCombo);

        QComboBox *pressCombo = new QComboBox();
        pressCombo->addItems(actions);  // Add translated action names
        pressMappingCombos[buttonKey] = pressCombo;
        h->addWidget(new QLabel(tr("Press:")));
        h->addWidget(pressCombo);

        layout->addLayout(h);
    }

    layout->addStretch();
    buttonTab->setLayout(layout);
    tabWidget->addTab(buttonTab, tr("Button Mapping"));
}

void ControlPanelDialog::createKeyboardMappingTab() {
    keyboardTab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(keyboardTab);
    
    // Instructions
    QLabel *instructionLabel = new QLabel(tr("Configure custom keyboard shortcuts for application actions:"), keyboardTab);
    instructionLabel->setWordWrap(true);
    layout->addWidget(instructionLabel);
    
    // Table to show current mappings
    keyboardTable = new QTableWidget(0, 2, keyboardTab);
    keyboardTable->setHorizontalHeaderLabels({tr("Key Sequence"), tr("Action")});
    keyboardTable->horizontalHeader()->setStretchLastSection(true);
    keyboardTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    keyboardTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(keyboardTable);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    addKeyboardMappingButton = new QPushButton(tr("Add Mapping"), keyboardTab);
    removeKeyboardMappingButton = new QPushButton(tr("Remove Mapping"), keyboardTab);
    
    buttonLayout->addWidget(addKeyboardMappingButton);
    buttonLayout->addWidget(removeKeyboardMappingButton);
    buttonLayout->addStretch();
    
    layout->addLayout(buttonLayout);
    
    // Connections
    connect(addKeyboardMappingButton, &QPushButton::clicked, this, &ControlPanelDialog::addKeyboardMapping);
    connect(removeKeyboardMappingButton, &QPushButton::clicked, this, &ControlPanelDialog::removeKeyboardMapping);
    
    // Load current mappings
    if (mainWindowRef) {
        QMap<QString, QString> mappings = mainWindowRef->getKeyboardMappings();
        keyboardTable->setRowCount(mappings.size());
        int row = 0;
        for (auto it = mappings.begin(); it != mappings.end(); ++it) {
            keyboardTable->setItem(row, 0, new QTableWidgetItem(it.key()));
            QString displayAction = ButtonMappingHelper::internalKeyToDisplay(it.value(), false);
            keyboardTable->setItem(row, 1, new QTableWidgetItem(displayAction));
            row++;
        }
    }
    
    tabWidget->addTab(keyboardTab, tr("Keyboard Shortcuts"));
}

void ControlPanelDialog::addKeyboardMapping() {
    // Step 1: Capture key sequence
    KeyCaptureDialog captureDialog(this);
    if (captureDialog.exec() != QDialog::Accepted) {
        return;
    }
    
    QString keySequence = captureDialog.getCapturedKeySequence();
    if (keySequence.isEmpty()) {
        return;
    }
    
    // Check if key sequence already exists
    if (mainWindowRef && mainWindowRef->getKeyboardMappings().contains(keySequence)) {
        QMessageBox::warning(this, tr("Key Already Mapped"), 
            tr("The key sequence '%1' is already mapped. Please choose a different key combination.").arg(keySequence));
        return;
    }
    
    // Step 2: Choose action
    QStringList actions = ButtonMappingHelper::getTranslatedActions();
    bool ok;
    QString selectedAction = QInputDialog::getItem(this, tr("Select Action"), 
        tr("Choose the action to perform when '%1' is pressed:").arg(keySequence), 
        actions, 0, false, &ok);
    
    if (!ok || selectedAction.isEmpty()) {
        return;
    }
    
    // Convert display name to internal key
    QString internalKey = ButtonMappingHelper::displayToInternalKey(selectedAction, false);
    
    // Add the mapping
    if (mainWindowRef) {
        mainWindowRef->addKeyboardMapping(keySequence, internalKey);
        
        // Update table
        int row = keyboardTable->rowCount();
        keyboardTable->insertRow(row);
        keyboardTable->setItem(row, 0, new QTableWidgetItem(keySequence));
        keyboardTable->setItem(row, 1, new QTableWidgetItem(selectedAction));
    }
}

void ControlPanelDialog::removeKeyboardMapping() {
    int currentRow = keyboardTable->currentRow();
    if (currentRow < 0) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select a mapping to remove."));
        return;
    }
    
    QTableWidgetItem *keyItem = keyboardTable->item(currentRow, 0);
    if (!keyItem) return;
    
    QString keySequence = keyItem->text();
    
    // Confirm removal
    int ret = QMessageBox::question(this, tr("Remove Mapping"), 
        tr("Are you sure you want to remove the keyboard shortcut '%1'?").arg(keySequence),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // Remove from MainWindow
        if (mainWindowRef) {
            mainWindowRef->removeKeyboardMapping(keySequence);
        }
        
        // Remove from table
        keyboardTable->removeRow(currentRow);
    }
}
