#include "RecentNotebooksDialog.h"
#include "MainWindow.h" // For selectFolder functionality
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QPixmap>
#include <QScrollArea>

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
            // This logic is similar to MainWindow::selectFolder but directly sets the folder.
            InkCanvas *canvas = mainWindowRef->currentCanvas(); // Get current canvas from MainWindow
            if (canvas) {
                if (canvas->isEdited()){
                     mainWindowRef->saveCurrentPage(); // Use MainWindow's save method
                }
                canvas->setSaveFolder(notebookPath);
                
                // ✅ Show last accessed page dialog if available
                if (!mainWindowRef->showLastAccessedPageDialog(canvas)) {
                    // No last accessed page, start from page 1
                    mainWindowRef->switchPageWithDirection(1, 1); // Use MainWindow's direction-aware switchPage method
                    mainWindowRef->pageInput->setValue(1); // Update pageInput in MainWindow
                } else {
                    // Dialog handled page switching, update page input
                    mainWindowRef->pageInput->setValue(mainWindowRef->getCurrentPageForCanvas(canvas) + 1);
                }
                mainWindowRef->updateTabLabel(); // Update tab label in MainWindow
                mainWindowRef->updateBookmarkButtonState(); // ✅ Update bookmark button state after loading notebook
                
                // Update recent notebooks list and refresh cover page
                if (notebookManager) {
                    // Generate and save fresh cover preview
                    notebookManager->generateAndSaveCoverPreview(notebookPath, canvas);
                    // Add/update in recent list (this moves it to the top)
                    notebookManager->addRecentNotebook(notebookPath, canvas);
                }
            }
            accept(); // Close the dialog
        }
    }
} 