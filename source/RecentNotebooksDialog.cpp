#include "RecentNotebooksDialog.h"
#include "MainWindow.h" // For selectFolder functionality
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QPixmap>
#include <QScrollArea>
#include <QTimer>
#include <QPointer>

RecentNotebooksDialog::RecentNotebooksDialog(MainWindow *mainWindow, RecentNotebooksManager *manager, QWidget *parent)
    : QDialog(parent), notebookManager(manager), mainWindowRef(mainWindow) {
    setupUi();
    populateGrid();
    setWindowTitle(tr("Recent Notebooks"));
    setMinimumSize(800, 600);
}

void RecentNotebooksDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    QWidget *gridContainer = new QWidget();
    gridLayout = new QGridLayout(gridContainer);
    gridLayout->setSpacing(15);

    scrollArea->setWidget(gridContainer);
    mainLayout->addWidget(scrollArea);
    setLayout(mainLayout);
}

void RecentNotebooksDialog::populateGrid() {
    QStringList recentPaths = notebookManager->getRecentNotebooks();
    int row = 0;
    int col = 0;
    const int numCols = 4;

    for (const QString &path : recentPaths) {
        if (path.isEmpty()) continue;

        QPushButton *button = new QPushButton(this);
        button->setFixedSize(180, 180); // Larger buttons
        button->setProperty("notebookPath", path); // Store path for slot
        connect(button, &QPushButton::clicked, this, &RecentNotebooksDialog::onNotebookClicked);

        QVBoxLayout *buttonLayout = new QVBoxLayout(button);
        buttonLayout->setContentsMargins(5,5,5,5);

        QLabel *coverLabel = new QLabel(this);
        coverLabel->setFixedSize(170, 127); // 4:3 aspect ratio for cover
        coverLabel->setAlignment(Qt::AlignCenter);
        QString coverPath = notebookManager->getCoverImagePathForNotebook(path);
        if (!coverPath.isEmpty()) {
            QPixmap coverPixmap(coverPath);
            coverLabel->setPixmap(coverPixmap.scaled(coverLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            coverLabel->setText(tr("No Preview"));
        }
        coverLabel->setStyleSheet("border: 1px solid gray;");

        QLabel *nameLabel = new QLabel(notebookManager->getNotebookDisplayName(path), this);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setWordWrap(true);
        nameLabel->setFixedHeight(40); // Fixed height for name label

        buttonLayout->addWidget(coverLabel);
        buttonLayout->addWidget(nameLabel);
        button->setLayout(buttonLayout);

        gridLayout->addWidget(button, row, col);

        col++;
        if (col >= numCols) {
            col = 0;
            row++;
        }
    }
}

void RecentNotebooksDialog::onNotebookClicked() {
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (button) {
        QString notebookPath = button->property("notebookPath").toString();
        if (!notebookPath.isEmpty() && mainWindowRef) {
            // ✅ Handle .spn files vs regular folders properly
            if (notebookPath.endsWith(".spn", Qt::CaseInsensitive)) {
                // For .spn files, use the proper opening method
                mainWindowRef->openSpnPackage(notebookPath);
            } else {
                // For regular folders, use the existing logic
                InkCanvas *canvas = mainWindowRef->currentCanvas();
                if (canvas) {
                    if (canvas->isEdited()){
                         mainWindowRef->saveCurrentPage();
                    }
                    canvas->setSaveFolder(notebookPath);
                    
                    // ✅ Show last accessed page dialog if available
                    if (!mainWindowRef->showLastAccessedPageDialog(canvas)) {
                        // No last accessed page, start from page 1
                        mainWindowRef->switchPageWithDirection(1, 1);
                        mainWindowRef->pageInput->setValue(1);
                    } else {
                        // Dialog handled page switching, update page input
                        mainWindowRef->pageInput->setValue(mainWindowRef->getCurrentPageForCanvas(canvas) + 1);
                    }
                    mainWindowRef->updateTabLabel();
                    mainWindowRef->updateBookmarkButtonState();
                    
                    // ✅ Update recent notebooks list AFTER page is loaded to ensure proper thumbnail generation
                    if (notebookManager) {
                        // Use QPointer to safely handle canvas deletion
                        QPointer<InkCanvas> canvasPtr(canvas);
                        QTimer::singleShot(100, this, [this, notebookPath, canvasPtr]() {
                            if (notebookManager && canvasPtr && !canvasPtr.isNull()) {
                                // Generate and save fresh cover preview
                                notebookManager->generateAndSaveCoverPreview(notebookPath, canvasPtr.data());
                                // Add/update in recent list (this moves it to the top)
                                notebookManager->addRecentNotebook(notebookPath, canvasPtr.data());
                            }
                        });
                    }
                }
            }
            accept(); // Close the dialog
        }
    }
} 