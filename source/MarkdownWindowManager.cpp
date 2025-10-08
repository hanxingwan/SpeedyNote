#include "MarkdownWindowManager.h"
#include "MarkdownWindow.h"
#include "InkCanvas.h"
#include "MainWindow.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QDebug>
#include <QTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

MarkdownWindowManager::MarkdownWindowManager(InkCanvas *canvas, QObject *parent)
    : QObject(parent), canvas(canvas)
{
    // Initialize transparency timer
    transparencyTimer = new QTimer(this);
    transparencyTimer->setSingleShot(true);
    transparencyTimer->setInterval(10000); // 10 seconds
    connect(transparencyTimer, &QTimer::timeout, this, &MarkdownWindowManager::onTransparencyTimerTimeout);

    // Connect to canvas signals to update window positions when canvas changes
    if (canvas) {
        connect(canvas, &InkCanvas::panChanged, this, &MarkdownWindowManager::updateAllWindowPositions);
        connect(canvas, &InkCanvas::zoomChanged, this, &MarkdownWindowManager::updateAllWindowPositions);
    }
}

MarkdownWindowManager::~MarkdownWindowManager() {
    clearAllWindows();
}

MarkdownWindow* MarkdownWindowManager::createMarkdownWindow(const QRect &rect) {
    if (!canvas) return nullptr;
    
    // qDebug() << "MarkdownWindowManager::createMarkdownWindow() - Creating new window";
    // qDebug() << "  Input screen rect:" << rect;
    
    // Convert screen coordinates to canvas coordinates
    QRect canvasRect = convertScreenToCanvasRect(rect);
    // qDebug() << "  Converted to canvas rect:" << canvasRect;
    
    // Apply bounds checking to ensure window is within canvas bounds
    QRect canvasBounds = canvas->getCanvasRect();
    
    // Ensure window stays within canvas bounds
    int maxX = canvasBounds.width() - canvasRect.width();
    int maxY = canvasBounds.height() - canvasRect.height();
    
    canvasRect.setX(qMax(0, qMin(canvasRect.x(), maxX)));
    canvasRect.setY(qMax(0, qMin(canvasRect.y(), maxY)));
    
            // qDebug() << "  Bounds-checked canvas rect:" << canvasRect << "Canvas bounds:" << canvasBounds;
    
    // Create new markdown window with canvas coordinates
    MarkdownWindow *window = new MarkdownWindow(canvasRect, canvas);
    
    // Connect signals
    connectWindowSignals(window);
    
    // Add to current windows list
    currentWindows.append(window);
    
    // qDebug() << "Total windows after creation:" << currentWindows.size();
    
    // Show the window
    window->show();
    window->focusEditor();
    
    // Mark canvas as edited since we created a new markdown window
    if (canvas) {
        canvas->setEdited(true);
    }
    
    emit windowCreated(window);
    return window;
}

void MarkdownWindowManager::removeMarkdownWindow(MarkdownWindow *window) {
    if (!window) return;
    
    // Remove from current windows
    currentWindows.removeAll(window);
    
    // Remove from all page windows
    for (auto it = pageWindows.begin(); it != pageWindows.end(); ++it) {
        it.value().removeAll(window);
    }
    
    emit windowRemoved(window);
    
    // Delete the window
    window->deleteLater();
}

void MarkdownWindowManager::clearAllWindows() {
    // Stop transparency timer
    transparencyTimer->stop();
    currentlyFocusedWindow = nullptr;
    windowsAreTransparent = false;
    
    // Clear current windows
    for (MarkdownWindow *window : currentWindows) {
        window->deleteLater();
    }
    currentWindows.clear();
    
    // Clear page windows
    for (auto it = pageWindows.begin(); it != pageWindows.end(); ++it) {
        for (MarkdownWindow *window : it.value()) {
            window->deleteLater();
        }
    }
    pageWindows.clear();
}

void MarkdownWindowManager::clearCurrentPagePermanently(int pageNumber) {
    // ✅ This method safely clears current page windows and permanently deletes their data
    
    // Stop transparency timer
    transparencyTimer->stop();
    currentlyFocusedWindow = nullptr;
    windowsAreTransparent = false;
    
    // Clear current windows from memory (they're currently visible)
    for (MarkdownWindow *window : currentWindows) {
        window->deleteLater();
    }
    currentWindows.clear();
    
    // Remove this page from the pageWindows map (no need to delete widgets again)
    pageWindows.remove(pageNumber);
    
    // Delete the persistent data file for this page
    QString filePath = getWindowDataFilePath(pageNumber);
    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
}

void MarkdownWindowManager::saveWindowsForPage(int pageNumber) {
    if (!canvas || currentWindows.isEmpty()) return;
    
    // Update page windows map
    pageWindows[pageNumber] = currentWindows;
    
    // Save to file
    saveWindowData(pageNumber, currentWindows);
}

void MarkdownWindowManager::loadWindowsForPage(int pageNumber) {
    if (!canvas) return;

    // qDebug() << "MarkdownWindowManager::loadWindowsForPage(" << pageNumber << ")";

    // This method is now only responsible for loading and showing the
    // windows for the specified page. Hiding of old windows is handled before this.

    QList<MarkdownWindow*> newPageWindows;

    // Check if we have windows for this page in memory
    if (pageWindows.contains(pageNumber)) {
        // qDebug() << "Found windows in memory for page" << pageNumber;
        newPageWindows = pageWindows[pageNumber];
    } else {
        // qDebug() << "Loading windows from file for page" << pageNumber;
        // Load from file
        newPageWindows = loadWindowData(pageNumber);

        // Update page windows map
        if (!newPageWindows.isEmpty()) {
            pageWindows[pageNumber] = newPageWindows;
        }
    }

    // qDebug() << "Loaded" << newPageWindows.size() << "windows for page" << pageNumber;

    // Clear the currently focused window since we're loading a new page
    currentlyFocusedWindow = nullptr;
    windowsAreTransparent = false;

    // Update the current window list and show the windows
    currentWindows = newPageWindows;
    for (MarkdownWindow *window : currentWindows) {
        // Validate that the window is within canvas bounds
        if (!window->isValidForCanvas()) {
            // qDebug() << "Warning: Markdown window at" << window->getCanvasRect() 
            //          << "is outside canvas bounds" << canvas->getCanvasRect();
            // Optionally adjust the window position to fit within bounds
            QRect canvasBounds = canvas->getCanvasRect();
            QRect windowRect = window->getCanvasRect();
            
            // Clamp the window position to canvas bounds
            int newX = qMax(0, qMin(windowRect.x(), canvasBounds.width() - windowRect.width()));
            int newY = qMax(0, qMin(windowRect.y(), canvasBounds.height() - windowRect.height()));
            
            if (newX != windowRect.x() || newY != windowRect.y()) {
                QRect adjustedRect(newX, newY, windowRect.width(), windowRect.height());
                window->setCanvasRect(adjustedRect);
                // qDebug() << "Adjusted window position to" << adjustedRect;
            }
        }
        
        // Ensure canvas connections are set up for loaded windows
        window->ensureCanvasConnections();
        
        window->show();
        window->updateScreenPositionImmediate();
        // Start windows as semi-transparent when arriving at a new page
        window->setTransparent(true);
    }
    
    // Mark that windows are currently transparent
    windowsAreTransparent = true;
    
    // Start transparency timer if there are windows
    // When timer expires, windows will remain transparent (or become transparent again)
    if (!currentWindows.isEmpty()) {
        // qDebug() << "Starting transparency timer for" << currentWindows.size() << "windows";
        transparencyTimer->stop();
        transparencyTimer->start();
    } else {
        // qDebug() << "No windows to show, not starting timer";
    }
}

void MarkdownWindowManager::deleteWindowsForPage(int pageNumber) {
    // Remove windows from memory
    if (pageWindows.contains(pageNumber)) {
        QList<MarkdownWindow*> windows = pageWindows[pageNumber];
        for (MarkdownWindow *window : windows) {
            window->deleteLater();
        }
        pageWindows.remove(pageNumber);
    }
    
    // Delete file
    QString filePath = getWindowDataFilePath(pageNumber);
    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
    
    // Clear current windows if they belong to this page
    if (pageWindows.value(pageNumber) == currentWindows) {
        currentWindows.clear();
    }
}

void MarkdownWindowManager::setSelectionMode(bool enabled) {
    selectionMode = enabled;
    
    // Change cursor for canvas
    if (canvas) {
        canvas->setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
    }
}

QList<MarkdownWindow*> MarkdownWindowManager::getCurrentPageWindows() const {
    return currentWindows;
}

QList<MarkdownWindow*> MarkdownWindowManager::loadWindowsForPageSeparately(int pageNumber) {
    if (!canvas) return QList<MarkdownWindow*>();

    // Load windows from file without affecting current windows
    QList<MarkdownWindow*> pageWindows = loadWindowData(pageNumber);
    
    // Apply bounds checking and setup connections
    for (MarkdownWindow *window : pageWindows) {
        // Validate that the window is within canvas bounds
        if (!window->isValidForCanvas()) {
            QRect canvasBounds = canvas->getCanvasRect();
            QRect windowRect = window->getCanvasRect();
            
            // Clamp the window position to canvas bounds
            int newX = qMax(0, qMin(windowRect.x(), canvasBounds.width() - windowRect.width()));
            int newY = qMax(0, qMin(windowRect.y(), canvasBounds.height() - windowRect.height()));
            
            if (newX != windowRect.x() || newY != windowRect.y()) {
                QRect adjustedRect(newX, newY, windowRect.width(), windowRect.height());
                window->setCanvasRect(adjustedRect);
            }
        }
        
        // Ensure canvas connections are set up for loaded windows
        window->ensureCanvasConnections();
        
        // Connect window signals
        connectWindowSignals(window);
        
        // DON'T show the window here - let setCombinedWindows handle visibility
        window->updateScreenPositionImmediate();
        // Start windows as semi-transparent (they'll be shown by setCombinedWindows)
        window->setTransparent(true);
    }
    
    return pageWindows;
}

void MarkdownWindowManager::setCombinedWindows(const QList<MarkdownWindow*> &windows) {
    // Hide current windows first
    for (MarkdownWindow *window : currentWindows) {
        window->hide();
    }
    
    // Clear the currently focused window since we're switching to combined view
    currentlyFocusedWindow = nullptr;
    windowsAreTransparent = false;
    
    // Set new combined windows as current
    currentWindows = windows;
    
    // Show all combined windows
    for (MarkdownWindow *window : currentWindows) {
        window->show();
        window->updateScreenPositionImmediate();
        // Start windows as semi-transparent when switching to combined view
        window->setTransparent(true);
    }
    
    // Mark that windows are currently transparent
    windowsAreTransparent = true;
    
    // Start transparency timer if there are windows
    // When timer expires, windows will remain transparent (or become transparent again)
    if (!currentWindows.isEmpty()) {
        transparencyTimer->stop();
        transparencyTimer->start();
    }
}

void MarkdownWindowManager::saveWindowsForPageSeparately(int pageNumber, const QList<MarkdownWindow*> &windows) {
    if (!canvas) return;
    
    // Update page windows map for this specific page
    pageWindows[pageNumber] = windows;
    
    // Save to file
    saveWindowData(pageNumber, windows);
}

void MarkdownWindowManager::onWindowDeleteRequested(MarkdownWindow *window) {
    removeMarkdownWindow(window);
    
    // Mark canvas as edited since we deleted a markdown window
    if (canvas) {
        canvas->setEdited(true);
    }
    
    // Save current state
    if (canvas) {
        MainWindow *mainWindow = qobject_cast<MainWindow*>(canvas->parent());
        if (mainWindow) {
            int currentPage = mainWindow->getCurrentPageForCanvas(canvas);
            saveWindowsForPage(currentPage);
        }
    }
}

QString MarkdownWindowManager::getWindowDataFilePath(int pageNumber) const {
    QString saveFolder = getSaveFolder();
    QString notebookId = getNotebookId();
    
    if (saveFolder.isEmpty() || notebookId.isEmpty()) {
        return QString();
    }
    
    return saveFolder + QString("/.%1_markdown_%2.json").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));
}

void MarkdownWindowManager::saveWindowData(int pageNumber, const QList<MarkdownWindow*> &windows) {
    QString filePath = getWindowDataFilePath(pageNumber);
    if (filePath.isEmpty()) return;
    
    QJsonArray windowsArray;
    for (MarkdownWindow *window : windows) {
        QVariantMap windowData = window->serialize();
        windowsArray.append(QJsonObject::fromVariantMap(windowData));
    }
    
    QJsonDocument doc(windowsArray);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        
        #ifdef Q_OS_WIN
        // Set hidden attribute on Windows
        SetFileAttributesW(reinterpret_cast<const wchar_t *>(filePath.utf16()), FILE_ATTRIBUTE_HIDDEN);
        #endif
    }
}

QList<MarkdownWindow*> MarkdownWindowManager::loadWindowData(int pageNumber) {
    QList<MarkdownWindow*> windows;
    
    QString filePath = getWindowDataFilePath(pageNumber);
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return windows;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return windows;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse markdown window data:" << error.errorString();
        return windows;
    }
    
    QJsonArray windowsArray = doc.array();
    for (const QJsonValue &value : windowsArray) {
        QJsonObject windowObj = value.toObject();
        QVariantMap windowData = windowObj.toVariantMap();
        
        // Create window with default rect (will be updated by deserialize)
        MarkdownWindow *window = new MarkdownWindow(QRect(0, 0, 300, 200), canvas);
        window->deserialize(windowData);
        
        // Connect signals
        connectWindowSignals(window);
        
        windows.append(window);
        window->show();
    }
    
    return windows;
}

QString MarkdownWindowManager::getSaveFolder() const {
    return canvas ? canvas->getSaveFolder() : QString();
}

QString MarkdownWindowManager::getNotebookId() const {
    if (!canvas) return QString();
    
    QString saveFolder = canvas->getSaveFolder();
    if (saveFolder.isEmpty()) return QString();
    
    QString idFile = saveFolder + "/.notebook_id.txt";
    if (QFile::exists(idFile)) {
        QFile file(idFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString notebookId = in.readLine().trimmed();
            file.close();
            return notebookId;
        }
    }
    
    return "notebook"; // Default fallback
}

QRect MarkdownWindowManager::convertScreenToCanvasRect(const QRect &screenRect) const {
    if (!canvas) {
        // qDebug() << "MarkdownWindowManager::convertScreenToCanvasRect() - No canvas, returning screen rect:" << screenRect;
        return screenRect;
    }
    
    // Use the new coordinate conversion methods from InkCanvas
    QRect canvasRect = canvas->mapWidgetToCanvas(screenRect);
    // qDebug() << "MarkdownWindowManager::convertScreenToCanvasRect() - Screen rect:" << screenRect << "-> Canvas rect:" << canvasRect;
    // qDebug() << "  Canvas size:" << canvas->getCanvasSize() << "Zoom:" << canvas->getZoomFactor() << "Pan:" << canvas->getPanOffset();
    return canvasRect;
}

void MarkdownWindowManager::updateAllWindowPositions() {
    // Update positions of all current windows
    for (MarkdownWindow *window : currentWindows) {
        if (window) {
            window->updateScreenPositionImmediate();
        }
    }
}

void MarkdownWindowManager::resetTransparencyTimer() {
    // qDebug() << "MarkdownWindowManager::resetTransparencyTimer() called";
    transparencyTimer->stop();
    
    // DON'T make all windows opaque here - only the interacted window should be opaque
    // The caller is responsible for setting the specific window's transparency
    
    // Start the timer again
    transparencyTimer->start();
    // qDebug() << "Transparency timer started for 10 seconds";
}

void MarkdownWindowManager::setWindowsTransparent(bool transparent) {
    if (windowsAreTransparent == transparent) return;
    
    // qDebug() << "MarkdownWindowManager::setWindowsTransparent(" << transparent << ")";
    // qDebug() << "Current windows count:" << currentWindows.size();
    // qDebug() << "Currently focused window:" << currentlyFocusedWindow;
    
    windowsAreTransparent = transparent;
    
    // Apply transparency logic:
    // - If transparent=true: make all windows except focused one transparent
    // - If transparent=false: make all windows opaque
    for (MarkdownWindow *window : currentWindows) {
        if (transparent) {
            // When making transparent, only make non-focused windows transparent
            if (window != currentlyFocusedWindow) {
                // qDebug() << "Setting unfocused window" << window << "transparent: true";
                window->setTransparent(true);
            } else {
                // qDebug() << "Keeping focused window" << window << "opaque";
                window->setTransparent(false);
            }
        } else {
            // When making opaque, make all windows opaque
            // qDebug() << "Setting window" << window << "transparent: false";
            window->setTransparent(false);
        }
    }
}

void MarkdownWindowManager::hideAllWindows() {
    // Stop transparency timer when hiding windows
    transparencyTimer->stop();
    currentlyFocusedWindow = nullptr;
    windowsAreTransparent = false;
    
    // Hide all current windows
    for (MarkdownWindow *window : currentWindows) {
        window->hide();
        window->setTransparent(false); // Reset transparency state
    }
}

void MarkdownWindowManager::setWindowsFrameOnlyMode(bool enabled) {
    // Set frame-only mode for all current windows
    for (MarkdownWindow* window : currentWindows) {
        if (window) {
            window->setFrameOnlyMode(enabled);
        }
    }
}

void MarkdownWindowManager::onWindowFocusChanged(MarkdownWindow *window, bool focused) {
    // qDebug() << "MarkdownWindowManager::onWindowFocusChanged(" << window << ", " << focused << ")";
    
    if (focused) {
        // A window gained focus - make only this window opaque and reset timer
        // qDebug() << "Window gained focus, setting as currently focused";
        currentlyFocusedWindow = window;
        
        // Make only this window opaque
        window->setTransparent(false);
        
        // Update state since at least one window is now opaque
        windowsAreTransparent = false;
        
        // Reset timer
        resetTransparencyTimer();
    } else {
        // A window lost focus
        // qDebug() << "Window lost focus";
        if (currentlyFocusedWindow == window) {
            // qDebug() << "Clearing currently focused window";
            currentlyFocusedWindow = nullptr;
        }
        
        // Check if any window still has focus
        bool anyWindowFocused = false;
        for (MarkdownWindow *w : currentWindows) {
            if (w->isEditorFocused()) {
                // qDebug() << "Found another focused window:" << w;
                anyWindowFocused = true;
                currentlyFocusedWindow = w;
                // Make the newly focused window opaque
                w->setTransparent(false);
                // Update state since at least one window is now opaque
                windowsAreTransparent = false;
                break;
            }
        }
        
        if (!anyWindowFocused) {
            // qDebug() << "No window has focus, starting transparency timer";
            // No window has focus, start transparency timer
            resetTransparencyTimer();
        } else {
            // qDebug() << "Another window still has focus, resetting timer";
            // Another window has focus, reset timer
            resetTransparencyTimer();
        }
    }
}

void MarkdownWindowManager::onWindowContentChanged(MarkdownWindow *window) {
    // qDebug() << "MarkdownWindowManager::onWindowContentChanged(" << window << ")";
    
    // Content changed, make this window the focused one and reset transparency timer
    currentlyFocusedWindow = window;
    
    // Make only this window opaque
    window->setTransparent(false);
    
    // Update state since at least one window is now opaque
    windowsAreTransparent = false;
    
    // Reset transparency timer
    resetTransparencyTimer();
}

void MarkdownWindowManager::onTransparencyTimerTimeout() {
    // qDebug() << "MarkdownWindowManager::onTransparencyTimerTimeout() - Timer expired!";
    // Make all windows except the focused one semi-transparent
    setWindowsTransparent(true);
}

void MarkdownWindowManager::connectWindowSignals(MarkdownWindow *window) {
    // qDebug() << "MarkdownWindowManager::connectWindowSignals() - Connecting signals for window" << window;
    
    // Connect existing signals
    connect(window, &MarkdownWindow::deleteRequested, this, &MarkdownWindowManager::onWindowDeleteRequested);
    connect(window, &MarkdownWindow::contentChanged, this, [this, window]() {
        onWindowContentChanged(window);
        
        // Mark canvas as edited since markdown content changed
        if (canvas) {
            canvas->setEdited(true);
            
            // Get current page and save windows
            MainWindow *mainWindow = qobject_cast<MainWindow*>(canvas->parent());
            if (mainWindow) {
                int currentPage = mainWindow->getCurrentPageForCanvas(canvas);
                saveWindowsForPage(currentPage);
            }
        }
    });
    connect(window, &MarkdownWindow::windowMoved, this, [this, window](MarkdownWindow*) {
        // qDebug() << "Window moved:" << window;
        
        // Window was moved, make it temporarily opaque and reset transparency timer
        // DON'T set currentlyFocusedWindow - moving doesn't mean editing
        
        // Make only this window opaque
        window->setTransparent(false);
        
        // Update state since at least one window is now opaque
        windowsAreTransparent = false;
        
        // Reset transparency timer - after 10 seconds this window will become transparent again
        resetTransparencyTimer();
        
        // Mark canvas as edited since window was moved
        if (canvas) {
            canvas->setEdited(true);
        }
    });
    connect(window, &MarkdownWindow::windowResized, this, [this, window](MarkdownWindow*) {
        // qDebug() << "Window resized:" << window;
        
        // Window was resized, make it temporarily opaque and reset transparency timer
        // DON'T set currentlyFocusedWindow - resizing doesn't mean editing
        
        // Make only this window opaque
        window->setTransparent(false);
        
        // Update state since at least one window is now opaque
        windowsAreTransparent = false;
        
        // Reset transparency timer - after 10 seconds this window will become transparent again
        resetTransparencyTimer();
        
        // Mark canvas as edited since window was resized
        if (canvas) {
            canvas->setEdited(true);
        }
    });
    
    // Connect new transparency-related signals
    connect(window, &MarkdownWindow::focusChanged, this, &MarkdownWindowManager::onWindowFocusChanged);
    connect(window, &MarkdownWindow::editorFocusChanged, this, &MarkdownWindowManager::onWindowFocusChanged);
    connect(window, &MarkdownWindow::windowInteracted, this, [this, window](MarkdownWindow*) {
        // qDebug() << "Window interacted:" << window;
        
        // Window was clicked/interacted with, make it temporarily opaque and reset transparency timer
        // DON'T set currentlyFocusedWindow - clicking window border/header doesn't mean editing
        // (The editor focus signals will set currentlyFocusedWindow if user clicks inside the editor)
        
        // Make only this window opaque
        window->setTransparent(false);
        
        // Update state since at least one window is now opaque
        windowsAreTransparent = false;
        
        // Reset transparency timer - after 10 seconds this window will become transparent again
        resetTransparencyTimer();
    });
    
    // qDebug() << "All signals connected for window" << window;
}