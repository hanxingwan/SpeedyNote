#include "KeyCaptureDialog.h"
#include <QApplication>

KeyCaptureDialog::KeyCaptureDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Capture Key Sequence"));
    setFixedSize(350, 150);
    setModal(true);
    
    // Create UI elements
    instructionLabel = new QLabel(tr("Press the key combination you want to use:"), this);
    instructionLabel->setWordWrap(true);
    
    capturedLabel = new QLabel(tr("(No key captured yet)"), this);
    capturedLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 8px; border: 1px solid #ccc; }");
    capturedLabel->setAlignment(Qt::AlignCenter);
    
    // Buttons
    clearButton = new QPushButton(tr("Clear"), this);
    okButton = new QPushButton(tr("OK"), this);
    cancelButton = new QPushButton(tr("Cancel"), this);
    
    okButton->setDefault(true);
    okButton->setEnabled(false);  // Disabled until a key is captured
    
    // Layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(instructionLabel);
    mainLayout->addWidget(capturedLabel);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(clearButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connections
    connect(clearButton, &QPushButton::clicked, this, &KeyCaptureDialog::clearSequence);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    // Set focus to capture keys immediately
    setFocus();
}

void KeyCaptureDialog::keyPressEvent(QKeyEvent *event) {
    // Build key sequence string
    QStringList modifiers;
    
    if (event->modifiers() & Qt::ControlModifier) modifiers << "Ctrl";
    if (event->modifiers() & Qt::ShiftModifier) modifiers << "Shift";
    if (event->modifiers() & Qt::AltModifier) modifiers << "Alt";
    if (event->modifiers() & Qt::MetaModifier) modifiers << "Meta";
    
    // Don't capture modifier keys alone
    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift ||
        event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        return;
    }
    
    // Don't capture Escape (let it close the dialog)
    if (event->key() == Qt::Key_Escape) {
        QDialog::keyPressEvent(event);
        return;
    }
    
    QString keyString = QKeySequence(event->key()).toString();
    
    if (!modifiers.isEmpty()) {
        capturedSequence = modifiers.join("+") + "+" + keyString;
    } else {
        capturedSequence = keyString;
    }
    
    updateDisplay();
    event->accept();
}

void KeyCaptureDialog::clearSequence() {
    capturedSequence.clear();
    updateDisplay();
    setFocus();  // Return focus to dialog for key capture
}

void KeyCaptureDialog::updateDisplay() {
    if (capturedSequence.isEmpty()) {
        capturedLabel->setText(tr("(No key captured yet)"));
        okButton->setEnabled(false);
    } else {
        capturedLabel->setText(capturedSequence);
        okButton->setEnabled(true);
    }
} 