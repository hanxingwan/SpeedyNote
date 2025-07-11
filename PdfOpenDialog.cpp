#include "PdfOpenDialog.h"
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>
#include <QScreen>
#include <QGuiApplication>

PdfOpenDialog::PdfOpenDialog(const QString &pdfPath, QWidget *parent)
    : QDialog(parent), result(Cancel), pdfPath(pdfPath)
{
    setWindowTitle(tr("Open PDF with SpeedyNote"));
    setWindowIcon(QIcon(":/resources/icons/mainicon.png"));
    setModal(true);
    
    // Remove setFixedSize and use proper size management instead
    // Calculate size based on DPI
    QScreen *screen = QGuiApplication::primaryScreen();
    qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
    
    // Set minimum and maximum sizes instead of fixed size
    int baseWidth = 500;
    int baseHeight = 200;
    
    // Scale sizes appropriately for DPI
    int scaledWidth = static_cast<int>(baseWidth * qMax(1.0, dpr * 0.8));
    int scaledHeight = static_cast<int>(baseHeight * qMax(1.0, dpr * 0.8));
    
    setMinimumSize(scaledWidth, scaledHeight);
    setMaximumSize(scaledWidth + 100, scaledHeight + 50); // Allow some flexibility
    
    // Set size policy to prevent unwanted resizing
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    setupUI();
    
    // Ensure the dialog is properly sized after UI setup
    adjustSize();
    
    // Center the dialog on parent or screen
    if (parent) {
        move(parent->geometry().center() - rect().center());
    } else {
        move(screen->geometry().center() - rect().center());
    }
}

void PdfOpenDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Set layout size constraint to prevent unwanted resizing
    mainLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    
    // Icon and title
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);
    
    QLabel *iconLabel = new QLabel();
    QPixmap icon = QIcon(":/resources/icons/mainicon.png").pixmap(48, 48);
    iconLabel->setPixmap(icon);
    iconLabel->setFixedSize(48, 48);
    iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    QLabel *titleLabel = new QLabel(tr("Open PDF with SpeedyNote"));
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    
    mainLayout->addLayout(headerLayout);
    
    // PDF file info
    QFileInfo fileInfo(pdfPath);
    QString fileName = fileInfo.fileName();
    QString folderName = fileInfo.baseName(); // Filename without extension
    
    QLabel *messageLabel = new QLabel(tr("PDF File: %1\n\nHow would you like to open this PDF?").arg(fileName));
    messageLabel->setWordWrap(true);
    messageLabel->setStyleSheet("font-size: 12px; color: #666;");
    messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    mainLayout->addWidget(messageLabel);
    
    // Buttons
    QVBoxLayout *buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(10);
    
    // Create new folder button
    QPushButton *createFolderBtn = new QPushButton(tr("Create New Notebook Folder (\"%1\")").arg(folderName));
    createFolderBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    createFolderBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    createFolderBtn->setMinimumHeight(40);
    createFolderBtn->setStyleSheet(R"(
        QPushButton {
            text-align: left;
            padding: 10px;
            border: 1px solid palette(mid);
            border-radius: 5px;
            background: palette(button);
        }
        QPushButton:hover {
            background: palette(light);
            border-color: palette(dark);
        }
        QPushButton:pressed {
            background: palette(midlight);
        }
    )");
    connect(createFolderBtn, &QPushButton::clicked, this, &PdfOpenDialog::onCreateNewFolder);
    
    // Use existing folder button
    QPushButton *existingFolderBtn = new QPushButton(tr("Use Existing Notebook Folder..."));
    existingFolderBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon));
    existingFolderBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    existingFolderBtn->setMinimumHeight(40);
    existingFolderBtn->setStyleSheet(R"(
        QPushButton {
            text-align: left;
            padding: 10px;
            border: 1px solid palette(mid);
            border-radius: 5px;
            background: palette(button);
        }
        QPushButton:hover {
            background: palette(light);
            border-color: palette(dark);
        }
        QPushButton:pressed {
            background: palette(midlight);
        }
    )");
    connect(existingFolderBtn, &QPushButton::clicked, this, &PdfOpenDialog::onUseExistingFolder);
    
    buttonLayout->addWidget(createFolderBtn);
    buttonLayout->addWidget(existingFolderBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    // Cancel button
    QHBoxLayout *cancelLayout = new QHBoxLayout();
    cancelLayout->addStretch();
    
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton));
    cancelBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    cancelBtn->setMinimumSize(80, 30);
    cancelBtn->setStyleSheet(R"(
        QPushButton {
            padding: 8px 20px;
            border: 1px solid palette(mid);
            border-radius: 3px;
            background: palette(button);
        }
        QPushButton:hover {
            background: palette(light);
        }
        QPushButton:pressed {
            background: palette(midlight);
        }
    )");
    connect(cancelBtn, &QPushButton::clicked, this, &PdfOpenDialog::onCancel);
    
    cancelLayout->addWidget(cancelBtn);
    mainLayout->addLayout(cancelLayout);
}

void PdfOpenDialog::resizeEvent(QResizeEvent *event)
{
    // Handle resize events smoothly to prevent jitter during window dragging
    if (event->size() != event->oldSize()) {
        // Only process if size actually changed
        QDialog::resizeEvent(event);
        
        // Ensure the dialog stays within reasonable bounds
        QSize newSize = event->size();
        QSize minSize = minimumSize();
        QSize maxSize = maximumSize();
        
        // Clamp the size to prevent unwanted resizing
        newSize = newSize.expandedTo(minSize).boundedTo(maxSize);
        
        if (newSize != event->size()) {
            // If size needs to be adjusted, do it without triggering another resize event
            resize(newSize);
        }
    } else {
        // If size hasn't changed, just call parent implementation
        QDialog::resizeEvent(event);
    }
}

void PdfOpenDialog::onCreateNewFolder()
{
    QFileInfo fileInfo(pdfPath);
    QString suggestedFolderName = fileInfo.baseName();
    
    // Get the directory where the PDF is located
    QString pdfDir = fileInfo.absolutePath();
    QString newFolderPath = pdfDir + "/" + suggestedFolderName;
    
    // Check if folder already exists
    if (QDir(newFolderPath).exists()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, 
            tr("Folder Exists"), 
            tr("A folder named \"%1\" already exists in the same directory as the PDF.\n\nDo you want to use this existing folder?").arg(suggestedFolderName),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
        );
        
        if (reply == QMessageBox::Yes) {
            selectedFolder = newFolderPath;
            result = UseExistingFolder;
            accept();
            return;
        } else if (reply == QMessageBox::Cancel) {
            return; // Stay in dialog
        }
        // If No, continue to create with different name
        
        // Try to create with incremented name
        int counter = 1;
        do {
            newFolderPath = pdfDir + "/" + suggestedFolderName + QString("_%1").arg(counter);
            counter++;
        } while (QDir(newFolderPath).exists() && counter < 100);
        
        if (counter >= 100) {
            QMessageBox::warning(this, tr("Error"), tr("Could not create a unique folder name."));
            return;
        }
    }
    
    // Create the new folder
    QDir dir;
    if (dir.mkpath(newFolderPath)) {
        selectedFolder = newFolderPath;
        result = CreateNewFolder;
        accept();
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to create folder: %1").arg(newFolderPath));
    }
}

void PdfOpenDialog::onUseExistingFolder()
{
    QString folder = QFileDialog::getExistingDirectory(
        this, 
        tr("Select Existing Notebook Folder"), 
        QFileInfo(pdfPath).absolutePath()
    );
    
    if (!folder.isEmpty()) {
        selectedFolder = folder;
        result = UseExistingFolder;
        accept();
    }
}

void PdfOpenDialog::onCancel()
{
    result = Cancel;
    reject();
} 