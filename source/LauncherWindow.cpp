#include "LauncherWindow.h"
#include "MainWindow.h"
#include "RecentNotebooksManager.h"
#include "SpnPackageManager.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QTabWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QListWidget>
#include <QListWidgetItem>
#include <QFrame>
#include <QPixmap>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QPointer>
#include <QDesktopServices>
#include <QUrl>
#include <QResizeEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QScroller>
#include <QHash>

LauncherWindow::LauncherWindow(QWidget *parent)
    : QMainWindow(parent), notebookManager(nullptr), lastCalculatedWidth(0)
{
    setupUi();
    applyModernStyling();
    
    // Set window properties
    setWindowTitle(tr("SpeedyNote - Launcher"));
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QSize logicalSize = screen->availableGeometry().size() * 0.89;
        resize(logicalSize);
    }
    
    // Set window icon
    setWindowIcon(QIcon(":/resources/icons/mainicon.png"));
    
    // Use singleton instance of RecentNotebooksManager
    notebookManager = RecentNotebooksManager::getInstance(this);
    
    // Connect to thumbnail update signal to invalidate pixmap cache
    connect(notebookManager, &RecentNotebooksManager::thumbnailUpdated,
            this, [this](const QString& folderPath, const QString& coverImagePath) {
                invalidatePixmapCacheForPath(coverImagePath);
                // Don't repopulate entire grids here - that causes memory leaks!
                // The cache invalidation is enough; thumbnails will refresh on next launcher open
            });
    
    // Don't populate grids in constructor - showEvent will handle it
    // This prevents double population (constructor + showEvent)
}

LauncherWindow::~LauncherWindow()
{
    // Clean up QScroller instances to prevent memory leaks
    if (recentScrollArea && recentScrollArea->viewport()) {
        QScroller::ungrabGesture(recentScrollArea->viewport());
    }
    if (starredScrollArea && starredScrollArea->viewport()) {
        QScroller::ungrabGesture(starredScrollArea->viewport());
    }
    
    // Clear grids and pixmap cache before destruction
    clearRecentGrid();
    clearStarredGrid();
    clearPixmapCache();
}


void LauncherWindow::setupUi()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Create main splitter
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // Create sidebar with tabs
    tabList = new QListWidget();
    tabList->setObjectName("sidebarTabList"); // Give it a specific name for styling
    tabList->setFixedWidth(250);
    tabList->setSpacing(4);
    
    // Add tab items with explicit sizing
    QListWidgetItem *returnItem = new QListWidgetItem(loadThemedIcon("cross"), tr("Return"));
    QListWidgetItem *newItem = new QListWidgetItem(loadThemedIcon("addtab"), tr("New"));
    QListWidgetItem *openPdfItem = new QListWidgetItem(loadThemedIcon("pdf"), tr("Open PDF"));
    QListWidgetItem *openNotebookItem = new QListWidgetItem(loadThemedIcon("folder"), tr("Open Notebook"));
    QListWidgetItem *recentItem = new QListWidgetItem(loadThemedIcon("recent"), tr("Recent"));
    QListWidgetItem *starredItem = new QListWidgetItem(loadThemedIcon("star"), tr("Starred"));
    
    // Set explicit size hints for touch-friendly interface
    QSize itemSize(230, 60); // Width, Height - much taller for touch
    returnItem->setSizeHint(itemSize);
    newItem->setSizeHint(itemSize);
    openPdfItem->setSizeHint(itemSize);
    openNotebookItem->setSizeHint(itemSize);
    recentItem->setSizeHint(itemSize);
    starredItem->setSizeHint(itemSize);
    
    // Set font for each item
    QFont itemFont;
    itemFont.setPointSize(14);
    itemFont.setWeight(QFont::Medium);
    returnItem->setFont(itemFont);
    newItem->setFont(itemFont);
    openPdfItem->setFont(itemFont);
    openNotebookItem->setFont(itemFont);
    recentItem->setFont(itemFont);
    starredItem->setFont(itemFont);
    
    tabList->addItem(returnItem);
    tabList->addItem(newItem);
    tabList->addItem(openPdfItem);
    tabList->addItem(openNotebookItem);
    tabList->addItem(recentItem);
    tabList->addItem(starredItem);
    
    tabList->setCurrentRow(4); // Start with Recent tab (now index 4)
    
    // Create content area
    contentStack = new QStackedWidget();
    
    // Setup individual tabs
    setupReturnTab();
    setupNewTab();
    setupOpenPdfTab();
    setupOpenNotebookTab();
    setupRecentTab();
    setupStarredTab();
    
    contentStack->addWidget(returnTab);
    contentStack->addWidget(newTab);
    contentStack->addWidget(openPdfTab);
    contentStack->addWidget(openNotebookTab);
    contentStack->addWidget(recentTab);
    contentStack->addWidget(starredTab);
    contentStack->setCurrentIndex(4); // Start with Recent tab (now index 4)
    
    // Add to splitter
    mainSplitter->addWidget(tabList);
    mainSplitter->addWidget(contentStack);
    mainSplitter->setStretchFactor(1, 1);
    
    // Main layout
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->addWidget(mainSplitter);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Connect tab selection with custom handling
    connect(tabList, &QListWidget::currentRowChanged, this, &LauncherWindow::onTabChanged);
}

void LauncherWindow::setupReturnTab()
{
    returnTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(returnTab);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);
    
    // Title
    QLabel *titleLabel = new QLabel(tr("Return to Previous Document"));
    titleLabel->setObjectName("titleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    // Description
    QLabel *descLabel = new QLabel(tr("Click the Return tab to go back to your previous document"));
    descLabel->setObjectName("descLabel");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);
    
    layout->addStretch();
}

void LauncherWindow::setupNewTab()
{
    newTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(newTab);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);
    
    // Title
    QLabel *titleLabel = new QLabel(tr("Create New Notebook"));
    titleLabel->setObjectName("titleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    // Description
    QLabel *descLabel = new QLabel(tr("Start a new notebook with a blank canvas"));
    descLabel->setObjectName("descLabel");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);
    
    // Create button
    QPushButton *createBtn = new QPushButton(tr("Create New Notebook"));
    createBtn->setObjectName("primaryButton");
    createBtn->setFixedSize(250, 50);
    connect(createBtn, &QPushButton::clicked, this, &LauncherWindow::onNewNotebookClicked);
    layout->addWidget(createBtn, 0, Qt::AlignCenter);
    
    layout->addStretch();
}

void LauncherWindow::setupOpenPdfTab()
{
    openPdfTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(openPdfTab);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);
    
    // Title
    QLabel *titleLabel = new QLabel(tr("Open PDF"));
    titleLabel->setObjectName("titleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    // Description
    QLabel *descLabel = new QLabel(tr("Select a PDF file to create a notebook for annotation"));
    descLabel->setObjectName("descLabel");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);
    
    // Open button
    QPushButton *openBtn = new QPushButton(tr("Browse for PDF"));
    openBtn->setObjectName("primaryButton");
    openBtn->setFixedSize(250, 50);
    connect(openBtn, &QPushButton::clicked, this, &LauncherWindow::onOpenPdfClicked);
    layout->addWidget(openBtn, 0, Qt::AlignCenter);
    
    layout->addStretch();
}

void LauncherWindow::setupOpenNotebookTab()
{
    openNotebookTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(openNotebookTab);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);
    
    // Title
    QLabel *titleLabel = new QLabel(tr("Open Notebook"));
    titleLabel->setObjectName("titleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    // Description
    QLabel *descLabel = new QLabel(tr("Select an existing SpeedyNote notebook (.spn) to open"));
    descLabel->setObjectName("descLabel");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);
    
    // Open button
    QPushButton *openBtn = new QPushButton(tr("Browse for Notebook"));
    openBtn->setObjectName("primaryButton");
    openBtn->setFixedSize(250, 50);
    connect(openBtn, &QPushButton::clicked, this, &LauncherWindow::onOpenNotebookClicked);
    layout->addWidget(openBtn, 0, Qt::AlignCenter);
    
    layout->addStretch();
}

void LauncherWindow::setupRecentTab()
{
    recentTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(recentTab);
    layout->setContentsMargins(20, 20, 20, 20);
    
    // Title
    QLabel *titleLabel = new QLabel(tr("Recent Notebooks"));
    titleLabel->setObjectName("titleLabel");
    layout->addWidget(titleLabel);
    
    // Scroll area for grid
    recentScrollArea = new QScrollArea();
    recentScrollArea->setWidgetResizable(true);
    recentScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    recentScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // Enable touch scrolling
    recentScrollArea->setAttribute(Qt::WA_AcceptTouchEvents, true);
    
    // Enable kinetic scrolling for touch devices
    QScroller::grabGesture(recentScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    
    recentGridWidget = new QWidget();
    recentGridLayout = new QGridLayout(recentGridWidget);
    recentGridLayout->setSpacing(20);
    recentGridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    
    recentScrollArea->setWidget(recentGridWidget);
    layout->addWidget(recentScrollArea);
}

void LauncherWindow::setupStarredTab()
{
    starredTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(starredTab);
    layout->setContentsMargins(20, 20, 20, 20);
    
    // Title
    QLabel *titleLabel = new QLabel(tr("Starred Notebooks"));
    titleLabel->setObjectName("titleLabel");
    layout->addWidget(titleLabel);
    
    // Scroll area for grid
    starredScrollArea = new QScrollArea();
    starredScrollArea->setWidgetResizable(true);
    starredScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    starredScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // Enable touch scrolling
    starredScrollArea->setAttribute(Qt::WA_AcceptTouchEvents, true);
    
    // Enable kinetic scrolling for touch devices
    QScroller::grabGesture(starredScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    
    starredGridWidget = new QWidget();
    starredGridLayout = new QGridLayout(starredGridWidget);
    starredGridLayout->setSpacing(20);
    starredGridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    
    starredScrollArea->setWidget(starredGridWidget);
    layout->addWidget(starredScrollArea);
}

void LauncherWindow::populateRecentGrid()
{
    // Clear existing widgets more thoroughly
    QLayoutItem *child;
    while ((child = recentGridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            // Disconnect all signals to prevent memory leaks
            child->widget()->disconnect();
            child->widget()->deleteLater(); // Use deleteLater for safer deletion
        }
        delete child;
    }
    
    QStringList recentPaths = notebookManager->getRecentNotebooks();
    
    // Calculate adaptive columns based on available width
    int availableWidth = recentScrollArea->viewport()->width();
    
    // Only recalculate if width changed significantly or first time
    if (abs(availableWidth - lastCalculatedWidth) > 50 || lastCalculatedWidth == 0) {
        lastCalculatedWidth = availableWidth;
    } else {
        availableWidth = lastCalculatedWidth; // Use cached width
    }
    
    int buttonWidth = BUTTON_SIZE + 20; // Button size plus some spacing
    int adaptiveColumns = qMax(2, qMin(6, availableWidth / buttonWidth)); // Between 2-6 columns
    
    int row = 0, col = 0;
    
    for (const QString &path : recentPaths) {
        if (path.isEmpty()) continue;
        
        QPushButton *button = createNotebookButton(path, false);
        recentGridLayout->addWidget(button, row, col);
        
        col++;
        if (col >= adaptiveColumns) {
            col = 0;
            row++;
        }
    }
}

void LauncherWindow::populateStarredGrid()
{
    // Clear existing widgets more thoroughly
    QLayoutItem *child;
    while ((child = starredGridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            // Disconnect all signals to prevent memory leaks
            child->widget()->disconnect();
            child->widget()->deleteLater(); // Use deleteLater for safer deletion
        }
        delete child;
    }
    
    QStringList starredPaths = notebookManager->getStarredNotebooks();
    
    // Calculate adaptive columns based on available width
    int availableWidth = starredScrollArea->viewport()->width();
    
    // Use the same cached width as recent grid for consistency
    if (lastCalculatedWidth > 0) {
        availableWidth = lastCalculatedWidth;
    }
    
    int buttonWidth = BUTTON_SIZE + 20; // Button size plus some spacing
    int adaptiveColumns = qMax(2, qMin(6, availableWidth / buttonWidth)); // Between 2-6 columns
    
    int row = 0, col = 0;
    
    for (const QString &path : starredPaths) {
        if (path.isEmpty()) continue;
        
        QPushButton *button = createNotebookButton(path, true);
        starredGridLayout->addWidget(button, row, col);
        
        col++;
        if (col >= adaptiveColumns) {
            col = 0;
            row++;
        }
    }
}

QPushButton* LauncherWindow::createNotebookButton(const QString &path, bool isStarred)
{
    QPushButton *button = new QPushButton();
    button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
    button->setObjectName("notebookButton");
    button->setProperty("notebookPath", path);
    button->setProperty("isStarred", isStarred);
    
    // Enable context menu
    button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(button, &QPushButton::customContextMenuRequested, this, &LauncherWindow::onNotebookRightClicked);
    
    // Main click handler
    if (isStarred) {
        connect(button, &QPushButton::clicked, this, &LauncherWindow::onStarredNotebookClicked);
    } else {
        connect(button, &QPushButton::clicked, this, &LauncherWindow::onRecentNotebookClicked);
    }
    
    // Create layout for button content
    QVBoxLayout *buttonLayout = new QVBoxLayout(button);
    buttonLayout->setContentsMargins(10, 10, 10, 10);
    buttonLayout->setSpacing(8);
    
    // Cover image
    QLabel *coverLabel = new QLabel();
    coverLabel->setFixedSize(BUTTON_SIZE - 20, COVER_HEIGHT);
    coverLabel->setAlignment(Qt::AlignCenter);
    
    // Apply theme-appropriate styling for cover
    QSettings settings("SpeedyNote", "App");
    bool isDarkMode = settings.value("useDarkMode", false).toBool();
    QString coverBg = isDarkMode ? "#2b2b2b" : "white";
    QString coverBorder = isDarkMode ? "#555555" : "#ddd";
    coverLabel->setStyleSheet(QString("border: 1px solid %1; border-radius: 0px; background: %2;").arg(coverBorder).arg(coverBg));
    
    // Set scaling mode to fill the entire area
    coverLabel->setScaledContents(true);
    
    QString coverPath = notebookManager->getCoverImagePathForNotebook(path);
    if (!coverPath.isEmpty()) {
        // Use cached pixmap if available to prevent memory leaks
        QString cacheKey = QString("%1_%2x%3").arg(coverPath).arg(coverLabel->width()).arg(coverLabel->height());
        
        QPixmap finalPixmap;
        if (pixmapCache.contains(cacheKey)) {
            // Use cached version
            finalPixmap = pixmapCache.value(cacheKey);
        } else {
            // Load and process new pixmap
            QPixmap coverPixmap(coverPath);
            if (!coverPixmap.isNull()) {
                // Scale to fill the entire label area, cropping if necessary
                QPixmap scaledPixmap = coverPixmap.scaled(coverLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                
                // If the scaled pixmap is larger than the label, crop it to center
                if (scaledPixmap.size() != coverLabel->size()) {
                    int x = (scaledPixmap.width() - coverLabel->width()) / 2;
                    int y = (scaledPixmap.height() - coverLabel->height()) / 2;
                    scaledPixmap = scaledPixmap.copy(x, y, coverLabel->width(), coverLabel->height());
                }
                
                finalPixmap = scaledPixmap;
                
                // Cache the processed pixmap (but limit cache size to prevent unbounded growth)
                if (pixmapCache.size() < 20) { // Limit cache to 20 items to reduce memory usage
                    pixmapCache.insert(cacheKey, finalPixmap);
                }
            }
        }
        
        if (!finalPixmap.isNull()) {
            coverLabel->setPixmap(finalPixmap);
        } else {
            coverLabel->setText(tr("No Preview"));
            QString textColor = isDarkMode ? "#cccccc" : "#666";
            coverLabel->setStyleSheet(coverLabel->styleSheet() + QString(" color: %1;").arg(textColor));
        }
    } else {
        coverLabel->setText(tr("No Preview"));
        QString textColor = isDarkMode ? "#cccccc" : "#666";
        coverLabel->setStyleSheet(coverLabel->styleSheet() + QString(" color: %1;").arg(textColor));
    }
    
    buttonLayout->addWidget(coverLabel);
    
    // Title
    QLabel *titleLabel = new QLabel(notebookManager->getNotebookDisplayName(path));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(false); // Disable word wrap to keep single line
    titleLabel->setMaximumHeight(20); // Reduce height to enforce single line
    titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed); // Allow text to be clipped
    
    // Set elide mode to show ellipsis when text is too long
    QFontMetrics fontMetrics(titleLabel->font());
    QString elidedText = fontMetrics.elidedText(titleLabel->text(), Qt::ElideRight, BUTTON_SIZE - 20);
    titleLabel->setText(elidedText);
    titleLabel->setStyleSheet("font-weight: bold;"); // Let system handle color
    buttonLayout->addWidget(titleLabel);
    
    return button;
}

void LauncherWindow::onNewNotebookClicked()
{
    // Try to find existing MainWindow first
    MainWindow *existingMainWindow = findExistingMainWindow();
    MainWindow *targetMainWindow = nullptr;
    
    if (existingMainWindow) {
        // Use existing MainWindow and add new tab
        targetMainWindow = existingMainWindow;
        targetMainWindow->show();
        targetMainWindow->raise();
        targetMainWindow->activateWindow();
        
        // Always create a new tab for the new document
        targetMainWindow->addNewTab();
    } else {
        // Create a new MainWindow with a blank notebook
        targetMainWindow = new MainWindow();
        
        // Connect to handle when MainWindow closes
        connect(targetMainWindow, &MainWindow::destroyed, this, [this]() {
            // Only show launcher if no other MainWindows exist
            if (!findExistingMainWindow()) {
                show();
                refreshRecentNotebooks();
            }
        });
    }
    
    // Preserve window state
    preserveWindowState(targetMainWindow, existingMainWindow != nullptr);
    
    // Hide the launcher
    hide();
}

void LauncherWindow::onOpenPdfClicked()
{
    QString pdfPath = QFileDialog::getOpenFileName(this, 
        tr("Open PDF File"), 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("PDF Files (*.pdf)"));
    
    if (!pdfPath.isEmpty()) {
        // Try to find existing MainWindow first
        MainWindow *existingMainWindow = findExistingMainWindow();
        MainWindow *targetMainWindow = nullptr;
        
        if (existingMainWindow) {
            // Use existing MainWindow and add new tab
            targetMainWindow = existingMainWindow;
            targetMainWindow->show();
            targetMainWindow->raise();
            targetMainWindow->activateWindow();
            
            // Always create a new tab for the new document
            targetMainWindow->addNewTab();
        } else {
            // Create new MainWindow
            targetMainWindow = new MainWindow();
            
            // Connect to handle when MainWindow closes
            connect(targetMainWindow, &MainWindow::destroyed, this, [this]() {
                if (!findExistingMainWindow()) {
                    show();
                    refreshRecentNotebooks();
                }
            });
        }
        
        // Preserve window state
        preserveWindowState(targetMainWindow, existingMainWindow != nullptr);
        
        // Hide launcher
        hide();
        
        // Use the same approach as file explorer integration - call openPdfFile directly
        // This will show the proper dialog and handle PDF linking correctly
        QTimer::singleShot(100, [targetMainWindow, pdfPath]() {
            targetMainWindow->openPdfFile(pdfPath);
        });
    }
}

void LauncherWindow::onOpenNotebookClicked()
{
    QString spnPath = QFileDialog::getOpenFileName(this, 
        tr("Open SpeedyNote Notebook"), 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("SpeedyNote Files (*.spn)"));
    
    if (!spnPath.isEmpty()) {
        openNotebook(spnPath);
    }
}

MainWindow* LauncherWindow::findExistingMainWindow()
{
    // Find existing MainWindow among all top-level widgets
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        MainWindow *mainWindow = qobject_cast<MainWindow*>(widget);
        if (mainWindow) {
            return mainWindow;
        }
    }
    return nullptr;
}

void LauncherWindow::preserveWindowState(QWidget* targetWindow, bool isExistingWindow)
{
    if (!targetWindow) return;
    
    if (isExistingWindow) {
        // For existing windows, just show them without changing their size/position
        if (targetWindow->isMaximized()) {
            targetWindow->showMaximized();
        } else if (targetWindow->isFullScreen()) {
            targetWindow->showFullScreen();
        } else {
            targetWindow->show();
        }
    } else {
        // For new windows, apply launcher's window state
        if (isMaximized()) {
            targetWindow->showMaximized();
        } else if (isFullScreen()) {
            targetWindow->showFullScreen();
        } else {
            targetWindow->resize(size());
            targetWindow->move(pos());
            targetWindow->show();
        }
    }
}

void LauncherWindow::onRecentNotebookClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (button) {
        QString path = button->property("notebookPath").toString();
        openNotebook(path);
    }
}

void LauncherWindow::onStarredNotebookClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (button) {
        QString path = button->property("notebookPath").toString();
        openNotebook(path);
    }
}

void LauncherWindow::openNotebook(const QString &path)
{
    if (path.isEmpty()) return;
    
    // Try to find existing MainWindow first
    MainWindow *existingMainWindow = findExistingMainWindow();
    MainWindow *targetMainWindow = nullptr;
    
    if (existingMainWindow) {
        // Use existing MainWindow and add new tab
        targetMainWindow = existingMainWindow;
        targetMainWindow->show();
        targetMainWindow->raise();
        targetMainWindow->activateWindow();
        
        // Always create a new tab for the new document
        targetMainWindow->addNewTab();
    } else {
        // Create new MainWindow
        targetMainWindow = new MainWindow();
        
        // Connect to handle when MainWindow closes
        connect(targetMainWindow, &MainWindow::destroyed, this, [this]() {
            // Only show launcher if no other MainWindows exist
            if (!findExistingMainWindow()) {
                show();
                refreshRecentNotebooks();
                refreshStarredNotebooks();
            }
        });
    }
    
    // Preserve window state when showing MainWindow
    preserveWindowState(targetMainWindow, existingMainWindow != nullptr);
    
    // Hide launcher
    hide();
    
    // Open the notebook
    if (path.endsWith(".spn", Qt::CaseInsensitive)) {
        targetMainWindow->openSpnPackage(path);
    } else {
        // Handle folder-based notebooks
        InkCanvas *canvas = targetMainWindow->currentCanvas();
        if (canvas) {
            canvas->setSaveFolder(path);
            if (!targetMainWindow->showLastAccessedPageDialog(canvas)) {
                targetMainWindow->switchPageWithDirection(1, 1);
                targetMainWindow->pageInput->setValue(1);
            } else {
                targetMainWindow->pageInput->setValue(targetMainWindow->getCurrentPageForCanvas(canvas) + 1);
            }
            targetMainWindow->updateTabLabel();
            targetMainWindow->updateBookmarkButtonState();
        }
    }
}

void LauncherWindow::onNotebookRightClicked(const QPoint &pos)
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    QString path = button->property("notebookPath").toString();
    bool isStarred = button->property("isStarred").toBool();
    rightClickedPath = path;
    
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    
    // Star/Unstar action
    QAction *starAction;
    if (isStarred) {
        starAction = menu->addAction(loadThemedIcon("star_reversed"), tr("Remove from Starred"));
    } else {
        starAction = menu->addAction(loadThemedIcon("star"), tr("Add to Starred"));
    }
    connect(starAction, &QAction::triggered, this, [this]() {
        toggleStarredStatus(rightClickedPath);
    });
    
    menu->addSeparator();
    
    // Delete from Recent (only for recent notebooks, not starred)
    if (!isStarred) {
        QAction *deleteAction = menu->addAction(loadThemedIcon("cross"), tr("Remove from Recent"));
        connect(deleteAction, &QAction::triggered, this, [this]() {
            removeFromRecent(rightClickedPath);
        });
        
        menu->addSeparator();
    }
    
    // Open in file explorer
    QAction *explorerAction = menu->addAction(loadThemedIcon("folder"), tr("Show in Explorer"));
    connect(explorerAction, &QAction::triggered, this, [path]() {
        QString dirPath;
        if (path.endsWith(".spn", Qt::CaseInsensitive)) {
            dirPath = QFileInfo(path).absolutePath();
        } else {
            dirPath = path;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    });
    
    menu->popup(button->mapToGlobal(pos));
}

void LauncherWindow::toggleStarredStatus(const QString &path)
{
    if (notebookManager->isStarred(path)) {
        notebookManager->removeStarred(path);
    } else {
        notebookManager->addStarred(path);
    }
    
    // Refresh both grids
    refreshRecentNotebooks();
    refreshStarredNotebooks();
}

void LauncherWindow::removeFromRecent(const QString &path)
{
    // Remove from recent notebooks
    notebookManager->removeRecentNotebook(path);
    
    // Refresh the recent grid to show the change
    refreshRecentNotebooks();
}

void LauncherWindow::refreshRecentNotebooks()
{
    // Always update the data, but only refresh UI if visible
    if (isVisible()) {
        // Just repopulate - don't clear cache as it might be needed
        populateRecentGrid();
        
        // Simple UI update
        if (recentScrollArea) {
            recentScrollArea->update();
        }
        update();
    }
    // If hidden, the data is still updated and showEvent will refresh UI
}

void LauncherWindow::refreshStarredNotebooks()
{
    // Always update the data, but only refresh UI if visible
    if (isVisible()) {
        // Just repopulate - don't clear cache as it might be needed
        populateStarredGrid();
        
        // Simple UI update
        if (starredScrollArea) {
            starredScrollArea->update();
        }
        update();
    }
    // If hidden, the data is still updated and showEvent will refresh UI
}

void LauncherWindow::applyModernStyling()
{
    // Detect dark mode
    bool isDarkMode = false;
    QSettings settings("SpeedyNote", "App");
    if (settings.contains("useDarkMode")) {
        isDarkMode = settings.value("useDarkMode", false).toBool();
    } else {
        // Auto-detect system dark mode
        QPalette palette = QApplication::palette();
        QColor windowColor = palette.color(QPalette::Window);
        isDarkMode = windowColor.lightness() < 128;
    }
    
    // Apply theme-appropriate styling
    QString mainBg = isDarkMode ? "#2b2b2b" : "#f8f9fa";
    QString cardBg = isDarkMode ? "#3c3c3c" : "#ffffff";
    QString borderColor = isDarkMode ? "#555555" : "#e9ecef";
    QString textColor = isDarkMode ? "#ffffff" : "#212529";
    QString secondaryTextColor = isDarkMode ? "#cccccc" : "#6c757d";
    QString hoverBorderColor = isDarkMode ? "#0078d4" : "#007bff";
    QString selectedBg = isDarkMode ? "#0078d4" : "#007bff";
    QString hoverBg = isDarkMode ? "#404040" : "#e9ecef";
    QString scrollBg = isDarkMode ? "#2b2b2b" : "#f8f9fa";
    QString scrollHandle = isDarkMode ? "#666666" : "#ced4da";
    QString scrollHandleHover = isDarkMode ? "#777777" : "#adb5bd";
    
    // Main window styling
    setStyleSheet(QString(R"(
        QMainWindow {
            background-color: %1;
        }
        
        QListWidget#sidebarTabList {
            background-color: %2;
            border: none;
            border-right: 1px solid %3;
            outline: none;
            font-size: 14px;
            padding: 10px 0px;
        }
        
        QListWidget#sidebarTabList::item {
            margin: 4px 8px;
            padding-left: 20px;
            border-radius: 8px;
        }
        
        QListWidget#sidebarTabList::item:selected {
            background-color: %4;
            color: white;
        }
        
        QListWidget#sidebarTabList::item:hover:!selected {
            background-color: %5;
        }
        
        QLabel#titleLabel {
            font-size: 24px;
            font-weight: bold;
            margin-bottom: 10px;
        }
        
        QLabel#descLabel {
            font-size: 14px;
            margin-bottom: 20px;
        }
        
        QPushButton#primaryButton {
            background-color: %4;
            border: none;
            border-radius: 8px;
            color: white;
            font-size: 16px;
            font-weight: bold;
            padding: 15px 30px;
        }
        
        QPushButton#primaryButton:hover {
            background-color: %6;
        }
        
        QPushButton#primaryButton:pressed {
            background-color: %7;
        }
        
        QPushButton#notebookButton {
            background-color: %2;
            border: 1px solid %3;
            border-radius: 12px;
            padding: 0px;
        }
        
        QPushButton#notebookButton:hover {
            border-color: %8;
        }
        
        QPushButton#notebookButton:pressed {
            background-color: %5;
        }
        
        QScrollArea {
            border: none;
            background-color: transparent;
        }
        
        QScrollBar:vertical {
            background-color: %9;
            width: 12px;
            border-radius: 6px;
        }
        
        QScrollBar::handle:vertical {
            background-color: %10;
            border-radius: 6px;
            min-height: 30px;
        }
        
        QScrollBar::handle:vertical:hover {
            background-color: %11;
        }
        
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            border: none;
            background: none;
        }
    )").arg(mainBg)                                    // %1
       .arg(cardBg)                                    // %2
       .arg(borderColor)                               // %3
       .arg(selectedBg)                                // %4
       .arg(hoverBg)                                   // %5
       .arg(isDarkMode ? "#005a9e" : "#0056b3")        // %6
       .arg(isDarkMode ? "#004578" : "#004085")        // %7
       .arg(hoverBorderColor)                          // %8
       .arg(scrollBg)                                  // %9
       .arg(scrollHandle)                              // %10
       .arg(scrollHandleHover));                       // %11
}

void LauncherWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    
    // Only recalculate grids if the size actually changed significantly
    if (event->oldSize().isValid() && 
        abs(event->size().width() - event->oldSize().width()) > 50) {
        // Recalculate grids when window is resized significantly
        // Use direct calls instead of timer to avoid timer accumulation
        populateRecentGrid();
        populateStarredGrid();
    }
}

void LauncherWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    // Rebuild UI when becoming visible since hideEvent clears everything
    // Use direct calls instead of timer to avoid timer accumulation
    populateRecentGrid();
    populateStarredGrid();
    
    // Simple UI update
    update();
}

void LauncherWindow::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
    
    // Clear grids when hiding to free UI widgets immediately
    // This prevents widget accumulation when launcher is opened/closed repeatedly
    clearRecentGrid();
    clearStarredGrid();
    
    // Don't clear pixmap cache - keep thumbnails cached to avoid reload/memory churn
    // The cache is size-limited (50 items) so it won't grow unbounded
    
    // Reset width cache to force recalculation next time
    lastCalculatedWidth = 0;
}

bool LauncherWindow::isDarkMode() const
{
    QSettings settings("SpeedyNote", "App");
    if (settings.contains("useDarkMode")) {
        return settings.value("useDarkMode", false).toBool();
    } else {
        // Auto-detect system dark mode
        QPalette palette = QApplication::palette();
        QColor windowColor = palette.color(QPalette::Window);
        return windowColor.lightness() < 128;
    }
}

QIcon LauncherWindow::loadThemedIcon(const QString& baseName)
{
    QString path = isDarkMode()
        ? QString(":/resources/icons/%1_reversed.png").arg(baseName)
        : QString(":/resources/icons/%1.png").arg(baseName);
    return QIcon(path);
}

void LauncherWindow::onTabChanged(int index)
{
    // Handle direct actions for certain tabs
    switch (index) {
        case 0: // Return tab
            {
                MainWindow *existingMainWindow = findExistingMainWindow();
                if (existingMainWindow) {
                    // Show and return to existing MainWindow (preserve its existing state)
                    preserveWindowState(existingMainWindow, true);
                    hide();
                } else {
                    // No existing window, stay on launcher
                    QMessageBox::information(this, tr("No Document"), 
                        tr("There is no previous document to return to."));
                }
                // Reset to Recent tab to allow clicking Return again
                QTimer::singleShot(50, this, [this]() {
                    tabList->setCurrentRow(4);
                });
            }
            break;
            
        case 1: // New tab - direct action
            onNewNotebookClicked();
            // Reset to Recent tab to allow clicking New again
            QTimer::singleShot(50, this, [this]() {
                tabList->setCurrentRow(4);
            });
            break;
            
        case 2: // Open PDF tab - direct action
            onOpenPdfClicked();
            // Reset to Recent tab to allow clicking Open PDF again
            QTimer::singleShot(50, this, [this]() {
                tabList->setCurrentRow(4);
            });
            break;
            
        case 3: // Open Notebook tab - direct action
            onOpenNotebookClicked();
            // Reset to Recent tab to allow clicking Open Notebook again
            QTimer::singleShot(50, this, [this]() {
                tabList->setCurrentRow(4);
            });
            break;
            
        case 4: // Recent tab - show content
        case 5: // Starred tab - show content
        default:
            // Show the corresponding content page
            contentStack->setCurrentIndex(index);
            break;
    }
}

void LauncherWindow::clearRecentGrid()
{
    if (!recentGridLayout) return;
    
    // Clear all widgets from the recent grid
    QLayoutItem *child;
    while ((child = recentGridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->disconnect();
            child->widget()->deleteLater();
        }
        delete child;
    }
}

void LauncherWindow::clearStarredGrid()
{
    if (!starredGridLayout) return;
    
    // Clear all widgets from the starred grid
    QLayoutItem *child;
    while ((child = starredGridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->disconnect();
            child->widget()->deleteLater();
        }
        delete child;
    }
}

void LauncherWindow::clearPixmapCache()
{
    // Clear the pixmap cache to free memory
    pixmapCache.clear();
}

void LauncherWindow::invalidatePixmapCacheForPath(const QString &path)
{
    // Remove all cache entries that match this path (different sizes)
    QStringList keysToRemove;
    for (auto it = pixmapCache.constBegin(); it != pixmapCache.constEnd(); ++it) {
        if (it.key().startsWith(path + "_")) {
            keysToRemove.append(it.key());
        }
    }
    
    for (const QString &key : keysToRemove) {
        pixmapCache.remove(key);
    }
}
