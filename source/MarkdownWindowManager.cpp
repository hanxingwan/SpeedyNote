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
    
    // Initialize content debounce timer to batch content updates
    contentDebounceTimer = new QTimer(this);
    contentDebounceTimer->setSingleShot(true);
    contentDebounceTimer->setInterval(500); // 500ms delay after last keystroke
    connect(contentDebounceTimer, &QTimer::timeout, this, &MarkdownWindowManager::flushPendingContentUpdates);
    
    // Initialize geometry debounce timer to batch move/resize updates
    geometryDebounceTimer = new QTimer(this);
    geometryDebounceTimer->setSingleShot(true);
    geometryDebounceTimer->setInterval(200); // 200ms delay after last move/resize
    connect(geometryDebounceTimer, &QTimer::timeout, this, &MarkdownWindowManager::flushPendingGeometryUpdates);

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
    qDebug() << "==========================================";
    qDebug() << "MarkdownWindowManager::createMarkdownWindow() - Creating new window";
    qDebug() << "  Input screen rect:" << rect;
    
    if (!canvas) {
        qDebug() << "  ERROR: No canvas!";
        return nullptr;
    }
    
    // ✅ Suppress geometry updates during window creation
    // The windowResized signal will fire during construction, but we don't want to queue it
    // because we're going to save immediately after creation anyway
    bool wasSuppressed = suppressDirtyMarking;
    suppressDirtyMarking = true;
    
    // Convert screen coordinates to canvas coordinates
    QRect canvasRect = convertScreenToCanvasRect(rect);
    qDebug() << "  Converted to canvas rect:" << canvasRect;
    
    // Apply bounds checking to ensure window is within canvas bounds
    QRect canvasBounds = canvas->getCanvasRect();
    
    // Ensure window stays within canvas bounds
    int maxX = canvasBounds.width() - canvasRect.width();
    int maxY = canvasBounds.height() - canvasRect.height();
    
    canvasRect.setX(qMax(0, qMin(canvasRect.x(), maxX)));
    canvasRect.setY(qMax(0, qMin(canvasRect.y(), maxY)));
    
    qDebug() << "  Bounds-checked canvas rect:" << canvasRect << "Canvas bounds:" << canvasBounds;
    
    // Create new markdown window with canvas coordinates
    MarkdownWindow *window = new MarkdownWindow(canvasRect, canvas);
    qDebug() << "  Created window:" << window;
    
    // Connect signals
    connectWindowSignals(window);
    
    // Add to current windows list
    currentWindows.append(window);
    
    qDebug() << "  Total windows after creation:" << currentWindows.size();
    
    // Show the window
    window->show();
    window->focusEditor();
    
    // ✅ Re-enable dirty marking after creation is complete
    suppressDirtyMarking = wasSuppressed;
    
    // Mark canvas as edited since we created a new markdown window
    if (canvas) {
        canvas->setEdited(true);
        
        // ✅ Mark current page as dirty so the new window will be saved
        int currentPage = canvas->getLastActivePage();
        dirtyPages.insert(currentPage);
        qDebug() << "  Marked page" << currentPage << "as dirty due to new window creation";
        
        // ✅ CRITICAL FIX: Add the new window to the page cache immediately
        // This ensures that when content updates happen before the first save,
        // the cache exists and can be updated properly
        if (pageWindows.contains(currentPage)) {
            // Cache exists, add this window to it
            pageWindows[currentPage].append(window);
            qDebug() << "  Added window to existing cache for page" << currentPage;
        } else {
            // No cache yet, create one with this window
            pageWindows[currentPage] = QList<MarkdownWindow*>{window};
            qDebug() << "  Created new cache for page" << currentPage;
        }
    }
    
    emit windowCreated(window);
    qDebug() << "  Window creation complete";
    return window;
}

void MarkdownWindowManager::removeMarkdownWindow(MarkdownWindow *window) {
    if (!window) return;
    
    // Remove from current windows
    currentWindows.removeAll(window);
    
    // ✅ Remove from pending updates if present
    pendingContentUpdates.remove(window);
    pendingGeometryUpdates.remove(window);
    pendingGeometryOriginalY.remove(window);
    
    // ✅ CRITICAL FIX: Remove the window from cache lists, but DON'T erase the entire page
    // The page cache needs to remain intact so saveWindowsForPage can use it correctly
    for (auto it = pageWindows.begin(); it != pageWindows.end(); ++it) {
        // Remove this specific window from the page's cache list
        it.value().removeAll(window);
        
        qDebug() << "  Removed window from page" << it.key() << "cache (now has" 
                 << it.value().size() << "windows)";
    }
    // ✅ CRASH FIX: Also remove from combinedTempWindows to prevent accessing deleted windows
    combinedTempWindows.removeAll(window);
    
    emit windowRemoved(window);
    
    // Delete the window
    window->deleteLater();
}

void MarkdownWindowManager::clearAllWindows() {
    // Stop transparency timer
    transparencyTimer->stop();
    currentlyFocusedWindow = nullptr;
    windowsAreTransparent = false;
    
    // ✅ MEMORY SAFETY FIX: Use a set to track which windows we've already deleted
    // In single-page mode, pageWindows and currentWindows contain the SAME objects,
    // so we need to avoid double-deletion
    QSet<MarkdownWindow*> deletedWindows;
    
    // Clear current windows
    for (MarkdownWindow *window : currentWindows) {
        if (!deletedWindows.contains(window)) {
            window->deleteLater();
            deletedWindows.insert(window);
        }
    }
    currentWindows.clear();
    
    // Clear page windows
    for (auto it = pageWindows.begin(); it != pageWindows.end(); ++it) {
        for (MarkdownWindow *window : it.value()) {
            if (!deletedWindows.contains(window)) {
                window->deleteLater();
                deletedWindows.insert(window);
            }
        }
    }
    pageWindows.clear();
}

void MarkdownWindowManager::clearAllCachedWindows() {
    // ✅ MEMORY LEAK FIX: Clean up all cached windows
    // This is used during canvas destruction to prevent memory leaks

    // Stop transparency timer
    if (transparencyTimer) {
        transparencyTimer->stop();
    }
    currentlyFocusedWindow = nullptr;
    windowsAreTransparent = false;

    // ✅ MEMORY SAFETY FIX: Use a set to track which windows we've already deleted
    // Multiple lists may contain the same objects, so we need to avoid double-deletion
    QSet<MarkdownWindow*> deletedWindows;

    // Clear combined temp windows (temporary clones used for pseudo-smooth scrolling)
    for (MarkdownWindow *window : combinedTempWindows) {
        if (window && !deletedWindows.contains(window)) {
            window->deleteLater();
            deletedWindows.insert(window);
        }
    }
    combinedTempWindows.clear();

    // Clear permanent page cache (windows loaded from disk and cached for reuse)
    for (auto it = pageWindows.begin(); it != pageWindows.end(); ++it) {
        for (MarkdownWindow *window : it.value()) {
            if (window && !deletedWindows.contains(window)) {
                window->deleteLater();
                deletedWindows.insert(window);
            }
        }
    }
    pageWindows.clear();

    // Clear current windows (the visible windows on screen)
    for (MarkdownWindow *window : currentWindows) {
        if (window && !deletedWindows.contains(window)) {
            window->deleteLater();
            deletedWindows.insert(window);
        }
    }
    currentWindows.clear();
}


void MarkdownWindowManager::clearCurrentPagePermanently(int pageNumber) {
    // ✅ This method safely clears current page windows and permanently deletes their data
    
    // Stop transparency timer
    transparencyTimer->stop();
    currentlyFocusedWindow = nullptr;
    windowsAreTransparent = false;
    
    // Clear current windows from memory (they're currently visible)
    for (MarkdownWindow *window : currentWindows) {
        // ✅ CRASH FIX: Also remove from combined temp windows to prevent dangling pointers
        combinedTempWindows.removeAll(window);
        window->deleteLater();
    }
    currentWindows.clear();
    // ✅ CRASH FIX: Clear combinedTempWindows to prevent dangling pointers
    combinedTempWindows.clear();
    
    // Remove this page from the pageWindows map (no need to delete widgets again)
    pageWindows.remove(pageNumber);
    
    // Delete the persistent data file for this page
    QString filePath = getWindowDataFilePath(pageNumber);
    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
}

void MarkdownWindowManager::saveWindowsForPage(int pageNumber) {
    qDebug() << "==========================================";
    qDebug() << "MarkdownWindowManager::saveWindowsForPage(" << pageNumber << ")";
    qDebug() << "  Canvas:" << canvas;
    qDebug() << "  currentWindows count:" << currentWindows.size();
    qDebug() << "  Page dirty:" << dirtyPages.contains(pageNumber);
    
    // ✅ Flush any pending content updates before saving
    if (!pendingContentUpdates.isEmpty()) {
        qDebug() << "  Flushing" << pendingContentUpdates.size() << "pending content updates before save";
        contentDebounceTimer->stop();
        flushPendingContentUpdates(); // Process immediately
    }
    
    // ✅ Flush any pending geometry updates before saving
    if (!pendingGeometryUpdates.isEmpty()) {
        qDebug() << "  Flushing" << pendingGeometryUpdates.size() << "pending geometry updates before save";
        geometryDebounceTimer->stop();
        flushPendingGeometryUpdates(); // Process immediately
    }

    // ✅ OPTIMIZATION: Only save if this page has been modified
    if (!dirtyPages.contains(pageNumber)) {
        qDebug() << "  SKIPPED: Page" << pageNumber << "has not been modified";
        return;
    }
    
    if (!canvas) {
        qDebug() << "  SKIPPED: canvas is null";
        return;
    }
    
    // ✅ FIX: Allow saving even if currentWindows is empty (e.g., after deleting all windows)
    // An empty list should be saved to disk to represent "no windows on this page"
    if (currentWindows.isEmpty()) {
        qDebug() << "  NOTE: Saving empty window list for page" << pageNumber;
    }
    
    // ✅ CRITICAL FIX: During combined mode, identify which windows belong to this page
    // and update both cache and disk correctly
    bool isCombinedMode = !combinedTempWindows.isEmpty();
    qDebug() << "  isCombinedMode:" << isCombinedMode;
    
    QList<MarkdownWindow*> windowsToSave;
    
    if (!isCombinedMode) {
        // Single-page mode: update permanent cache directly
        pageWindows[pageNumber] = currentWindows;
        windowsToSave = currentWindows;
        qDebug() << "  Updated pageWindows cache for page" << pageNumber;
    } else {
        // ✅ CRITICAL FIX: Combined mode - rebuild cache from currentWindows by page
        // Filter currentWindows to include only windows that belong to this specific page
        qDebug() << "  Combined mode: filtering windows for page" << pageNumber;
        
        int singlePageHeight = canvas->getBufferHeight() / 2;
        
        QList<MarkdownWindow*> pageSpecificWindows;
        QList<MarkdownWindow*> windowsForSaving;
        
        for (MarkdownWindow *window : currentWindows) {
            int windowPage = getPageNumberFromCanvasY(window->getCanvasRect().y());
            if (windowPage == pageNumber) {
                // ✅ Create a clone with PAGE-RELATIVE coordinates for both cache and disk
                MarkdownWindow *clonedWindow = new MarkdownWindow(window->getCanvasRect(), canvas);
                clonedWindow->setMarkdownContent(window->getMarkdownContent());
                
                QRect rect = clonedWindow->getCanvasRect();
                // ✅ CRITICAL FIX: Always adjust if window is in bottom half of canvas
                // This ensures page-relative coordinates regardless of current view
                // (combinedFirstPage/combinedSecondPage might have changed during debounce)
                if (rect.y() >= singlePageHeight) {
                    rect.moveTop(rect.y() - singlePageHeight);
                    clonedWindow->setCanvasRect(rect);
                    qDebug() << "    Adjusted window Y from" << window->getCanvasRect().y() 
                             << "to" << rect.y() << "(page-relative)";
                }
                
                // ✅ BOTH cache and disk store PAGE-RELATIVE coordinates
                // This allows InkCanvas to apply the correct offset when loading
                pageSpecificWindows.append(clonedWindow);
                windowsForSaving.append(clonedWindow);
                qDebug() << "    Window" << window << "belongs to page" << pageNumber 
                         << "(Y=" << window->getCanvasRect().y() << ")";
            }
        }
        
        // ✅ Clean up old cache entries before replacing
        // In combined mode, cache contains clones with page-relative coordinates,
        // which are different objects from currentWindows, so we can safely delete all old clones
        if (pageWindows.contains(pageNumber)) {
            for (MarkdownWindow *oldWindow : pageWindows[pageNumber]) {
                oldWindow->deleteLater();
            }
        }
        
        // ✅ Update cache with PAGE-RELATIVE coordinates (matching disk format)
        pageWindows[pageNumber] = pageSpecificWindows;
        qDebug() << "  Updated cache for page" << pageNumber << "with" 
                 << pageSpecificWindows.size() << "windows (page-relative coordinates)";
        
        // ✅ Save to disk (clones are now owned by cache, don't delete them)
        windowsToSave = windowsForSaving;
    }
    
    // Print what we're saving
    qDebug() << "  Saving" << windowsToSave.size() << "window(s) to page" << pageNumber;
    for (int i = 0; i < windowsToSave.size(); i++) {
        MarkdownWindow *w = windowsToSave[i];
        qDebug() << "  Window" << i << ":" << w;
        qDebug() << "    Content:" << w->getMarkdownContent().left(50) << "...";
        qDebug() << "    Canvas rect:" << w->getCanvasRect();
    }
    
    // Save to file
    saveWindowData(pageNumber, windowsToSave);
    
    // ✅ Clear dirty flag after successful save
    dirtyPages.remove(pageNumber);
    qDebug() << "  Cleared dirty flag for page" << pageNumber;
}

void MarkdownWindowManager::loadWindowsForPage(int pageNumber) {
    qDebug() << "==========================================";
    qDebug() << "MarkdownWindowManager::loadWindowsForPage(" << pageNumber << ")";
    
    if (!canvas) {
        qDebug() << "  ERROR: No canvas!";
        return;
    }

    // This method is now only responsible for loading and showing the
    // windows for the specified page. Hiding of old windows is handled before this.

    // ✅ MEMORY LEAK FIX: Clean up temporary combined windows when switching to regular page mode
    for (MarkdownWindow *window : combinedTempWindows) {
        if (window) {
            // Check if this window is in the permanent page cache
            bool isPermanent = false;
            for (const QList<MarkdownWindow*> &pageList : pageWindows.values()) {
                if (pageList.contains(window)) {
                    isPermanent = true;
                    break;
                }
            }

            // Only delete temporary cloned instances, not permanent cached ones
            if (!isPermanent) {
                // ✅ MEMORY SAFETY: Use deleteLater() for QWidget-derived objects
                window->deleteLater();
            }
        }
    }
    combinedTempWindows.clear();


    QList<MarkdownWindow*> newPageWindows;

    // Check if we have windows for this page in memory
    if (pageWindows.contains(pageNumber)) {
        qDebug() << "  Found windows in MEMORY cache for page" << pageNumber;
        newPageWindows = pageWindows[pageNumber];
        qDebug() << "  Cached windows count:" << newPageWindows.size();
        for (int i = 0; i < newPageWindows.size(); i++) {
            MarkdownWindow *w = newPageWindows[i];
            qDebug() << "    Cached window" << i << ":" << w;
            qDebug() << "      Content:" << w->getMarkdownContent().left(50) << "...";
            qDebug() << "      Canvas rect:" << w->getCanvasRect();
        }
    } else {
        qDebug() << "  No cache found, loading windows from DISK for page" << pageNumber;
        // Load from file
        newPageWindows = loadWindowData(pageNumber);

        // Update page windows map
        if (!newPageWindows.isEmpty()) {
            pageWindows[pageNumber] = newPageWindows;
            qDebug() << "  Updated cache with" << newPageWindows.size() << "windows";
        }
    }

    qDebug() << "  Total windows to display:" << newPageWindows.size();

    // ✅ Reset combined mode flags since we're loading a single page
    combinedFirstPage = -1;
    combinedSecondPage = -1;
    
    // Clear the currently focused window since we're loading a new page
    currentlyFocusedWindow = nullptr;
    windowsAreTransparent = false;

    // ✅ Suppress dirty marking during window positioning (loading, not user edits)
    suppressDirtyMarking = true;
    
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
    
    // ✅ Re-enable dirty marking now that loading is complete
    suppressDirtyMarking = false;
    qDebug() << "  Loading complete, dirty marking re-enabled";
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

    // ✅ Suppress dirty marking during window loading for combined views
    bool wasSuppressed = suppressDirtyMarking;
    suppressDirtyMarking = true;

    // ✅ PSEUDO-SMOOTH SCROLLING: Always create new window instances for combined views
    // The same notebook page appears at different Y-offsets in adjacent combined pages
    // (e.g., page 2 appears at bottom of display-page-1 and top of display-page-2)
    // We need separate window instances for each position, but reuse the cached data
    
    QList<MarkdownWindow*> windows;

    // Check if we have cached windows for this page that we can clone from
    if (pageWindows.contains(pageNumber) && !pageWindows[pageNumber].isEmpty()) {
        // Clone the cached windows to create new instances with same data
        const QList<MarkdownWindow*> &cachedWindows = pageWindows[pageNumber];
        for (MarkdownWindow *cachedWindow : cachedWindows) {
            // Serialize the cached window and create a new instance from it
            QVariantMap data = cachedWindow->serialize();

            // Create new window instance with same data
            MarkdownWindow *window = new MarkdownWindow(QRect(0, 0, 400, 300), canvas);
            window->deserialize(data);
            windows.append(window);
        }
    } else {
        // Load windows from file for the first time
        QList<MarkdownWindow*> loadedWindows = loadWindowData(pageNumber);

        // ✅ CRITICAL FIX: Store CLONES in permanent cache, not the loaded instances
        // The loaded instances will be returned and potentially Y-adjusted for combined views
        // We need the cache to always have the original unadjusted coordinates
        QList<MarkdownWindow*> permanentCache;
        for (MarkdownWindow *loadedWindow : loadedWindows) {
            // Clone for permanent cache
            QVariantMap data = loadedWindow->serialize();

            MarkdownWindow *cacheWindow = new MarkdownWindow(QRect(0, 0, 400, 300), canvas);
            cacheWindow->deserialize(data);
            permanentCache.append(cacheWindow);
        }

        // Store clones in cache for future use
        if (!permanentCache.isEmpty()) {
            pageWindows[pageNumber] = permanentCache;

            // ✅ MEMORY LEAK FIX: Limit cache size to prevent unbounded growth
            // Keep only the most recent 5 pages in cache
            const int MAX_CACHED_PAGES = 5;
            if (pageWindows.size() > MAX_CACHED_PAGES) {
                // Find and remove the oldest cache entry (simple heuristic: lowest page number far from current)
                int pageToRemove = -1;
                int maxDistance = 0;
                for (auto it = pageWindows.begin(); it != pageWindows.end(); ++it) {
                    int distance = qAbs(it.key() - pageNumber);
                    if (distance > maxDistance) {
                        maxDistance = distance;
                        pageToRemove = it.key();
                    }
                }

                if (pageToRemove >= 0) {
                    // Delete all cached windows for this page
                    QList<MarkdownWindow*> oldWindows = pageWindows[pageToRemove];
                    for (MarkdownWindow* oldWindow : oldWindows) {
                        if (oldWindow) {
                            // ✅ MEMORY SAFETY: Use deleteLater() for QWidget-derived objects
                            oldWindow->deleteLater();
                        }
                    }
                    pageWindows.remove(pageToRemove);
                }
            }
        }

        // Return the originally loaded windows (these will be Y-adjusted if needed)
        windows = loadedWindows;
    }

    // Apply bounds checking and setup connections for all windows
    for (MarkdownWindow *window : windows) {
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
    
    // ✅ Restore dirty marking state
    suppressDirtyMarking = wasSuppressed;
    
    return windows;
}

void MarkdownWindowManager::setCombinedWindows(const QList<MarkdownWindow*> &windows, int firstPage, int secondPage) {
    // ✅ CRASH FIX: Hide current windows BEFORE deleting any windows
    // Otherwise we might try to hide already-deleted windows
    for (MarkdownWindow *window : currentWindows) {
        window->hide();
    }

    // ✅ MEMORY LEAK FIX: Clean up old temporary combined windows
    // These are cloned instances created for pseudo-smooth scrolling that need deletion
    for (MarkdownWindow *window : combinedTempWindows) {
        if (window) {
            // Don't delete if it's in the new incoming windows list (will be reused)
            if (windows.contains(window)) {
                continue;
            }

            // Check if this window is in the permanent page cache
            bool isPermanent = false;
            for (const QList<MarkdownWindow*> &pageList : pageWindows.values()) {
                if (pageList.contains(window)) {
                    isPermanent = true;
                    break;
                }
            }

            // Only delete temporary cloned instances, not permanent cached ones
            if (!isPermanent) {
                // ✅ MEMORY SAFETY: Use deleteLater() for QWidget-derived objects
                window->deleteLater();
            }
        }
    }
    combinedTempWindows.clear();

    // ✅ Store which pages are being displayed in combined mode
    combinedFirstPage = firstPage;
    combinedSecondPage = secondPage;
    qDebug() << "MarkdownWindowManager::setCombinedWindows() - firstPage:" << firstPage << "secondPage:" << secondPage;
    
    // Clear the currently focused window since we're switching to combined view
    currentlyFocusedWindow = nullptr;
    windowsAreTransparent = false;
    
    // ✅ Suppress dirty marking during window positioning (loading, not user edits)
    suppressDirtyMarking = true;
    
    // Set new combined windows as current
    currentWindows = windows;

    // ✅ Track these combined windows for cleanup on next call
    combinedTempWindows = windows;
    
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
    
    // ✅ Re-enable dirty marking now that loading is complete
    suppressDirtyMarking = false;
    qDebug() << "  Combined loading complete, dirty marking re-enabled";
}

void MarkdownWindowManager::saveWindowsForPageSeparately(int pageNumber, const QList<MarkdownWindow*> &windows) {
    if (!canvas) return;
    
    qDebug() << "saveWindowsForPageSeparately(" << pageNumber << ")";
    qDebug() << "  Page dirty:" << dirtyPages.contains(pageNumber);
    
    // ✅ Flush any pending content updates before saving
    if (!pendingContentUpdates.isEmpty()) {
        qDebug() << "  Flushing" << pendingContentUpdates.size() << "pending content updates before save";
        contentDebounceTimer->stop();
        flushPendingContentUpdates(); // Process immediately
    }
    
    // ✅ Flush any pending geometry updates before saving
    if (!pendingGeometryUpdates.isEmpty()) {
        qDebug() << "  Flushing" << pendingGeometryUpdates.size() << "pending geometry updates before save";
        geometryDebounceTimer->stop();
        flushPendingGeometryUpdates(); // Process immediately
    }

    // ✅ OPTIMIZATION: Only save if this page has been modified
    if (!dirtyPages.contains(pageNumber)) {
        qDebug() << "  SKIPPED: Page" << pageNumber << "has not been modified";
        return;
    }
    
    // Update page windows map for this specific page
    // ✅ CRITICAL FIX: INVALIDATE the cache so next load reads fresh data from disk
    // This method is called after InkCanvas has already adjusted coordinates, so the cache
    // may have stale combined-mode coordinates that don't match what's on disk
    
    // Invalidate cache by deleting old clones and removing from map
    if (pageWindows.contains(pageNumber)) {
        qDebug() << "  Invalidating cache for page" << pageNumber;
        for (MarkdownWindow *oldWindow : pageWindows[pageNumber]) {
            // Only delete if it's not in currentWindows (to avoid deleting active windows)
            if (!currentWindows.contains(oldWindow)) {
                oldWindow->deleteLater();
            }
        }
        pageWindows.remove(pageNumber);
    }

    // Save to disk with the adjusted coordinates from InkCanvas
    saveWindowData(pageNumber, windows);
    
    // ✅ Clear dirty flag after successful save
    dirtyPages.remove(pageNumber);
    qDebug() << "  Cleared dirty flag for page" << pageNumber;
}

void MarkdownWindowManager::onWindowDeleteRequested(MarkdownWindow *window) {
    qDebug() << "==========================================";
    qDebug() << "MarkdownWindowManager::onWindowDeleteRequested(" << window << ")";
    qDebug() << "  Deleting window with content:" << window->getMarkdownContent().left(50) << "...";
    
    // ✅ CRITICAL: Flush any pending updates for this window BEFORE removing it
    // This prevents stale debounced updates from firing after deletion
    bool hadContentUpdate = pendingContentUpdates.contains(window);
    bool hadGeometryUpdate = pendingGeometryUpdates.contains(window);
    
    if (hadContentUpdate) {
        qDebug() << "  Clearing pending content update for deleted window";
        pendingContentUpdates.remove(window);
    }
    if (hadGeometryUpdate) {
        qDebug() << "  Clearing pending geometry update for deleted window";
        pendingGeometryUpdates.remove(window);
    }
    
    // ✅ CRITICAL: Restart timers if there are still other pending updates
    // (stopping would lose updates for other windows)
    if (hadContentUpdate && !pendingContentUpdates.isEmpty()) {
        qDebug() << "  Restarting content debounce timer for other pending updates";
        contentDebounceTimer->start();
    }
    if (hadGeometryUpdate && !pendingGeometryUpdates.isEmpty()) {
        qDebug() << "  Restarting geometry debounce timer for other pending updates";
        geometryDebounceTimer->start();
    }
    
    // ✅ CRITICAL FIX: In combined mode, we need to save BOTH pages
    // since the deletion affects the layout
    bool isCombinedMode = (combinedFirstPage >= 0 && combinedSecondPage >= 0);
    QSet<int> pagesToSave;
    
    if (isCombinedMode) {
        qDebug() << "  Combined mode detected: pages" << combinedFirstPage << "and" << combinedSecondPage;
        
        // Determine which page the window belongs to based on Y coordinate
        QRect windowRect = window->getCanvasRect();
        int windowPage = getPageNumberFromCanvasY(windowRect.y());
        qDebug() << "  Window Y coordinate:" << windowRect.y();
        qDebug() << "  Window belongs to page:" << windowPage;
        
        // Save the page the window was on
        pagesToSave.insert(windowPage);
        
        // ✅ Also save the other page to ensure cache consistency
        // (The new saveWindowsForPage logic will correctly rebuild both caches)
        if (windowPage == combinedFirstPage) {
            pagesToSave.insert(combinedSecondPage);
        } else {
            pagesToSave.insert(combinedFirstPage);
        }
    } else {
        // Single page mode - find which page from canvas
        if (canvas) {
            QWidget *parent = canvas->parentWidget();
            MainWindow *mainWindow = nullptr;
            while (parent) {
                mainWindow = qobject_cast<MainWindow*>(parent);
                if (mainWindow) break;
                parent = parent->parentWidget();
            }
            
            if (mainWindow) {
                int currentPage = mainWindow->getCurrentPageForCanvas(canvas);
                qDebug() << "  Single page mode: page" << currentPage;
                pagesToSave.insert(currentPage);
            }
        }
    }
    
    removeMarkdownWindow(window);
    
    // Mark canvas as edited since we deleted a markdown window
    if (canvas) {
        canvas->setEdited(true);
    }
    
    // ✅ Save all affected pages
    for (int pageNum : pagesToSave) {
        qDebug() << "  Marking page" << pageNum << "as dirty due to window deletion";
        dirtyPages.insert(pageNum);
        saveWindowsForPage(pageNum);
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
    qDebug() << "  saveWindowData() - Writing to file:" << filePath;
    
    if (filePath.isEmpty()) {
        qDebug() << "  ERROR: filePath is empty!";
        return;
    }
    
    QJsonArray windowsArray;
    for (int i = 0; i < windows.size(); i++) {
        MarkdownWindow *window = windows[i];
        QVariantMap windowData = window->serialize();
        qDebug() << "    Serializing window" << i << ":" << window;
        qDebug() << "      Content:" << windowData.value("content").toString().left(50) << "...";
        qDebug() << "      Canvas rect:" << QRect(windowData.value("canvas_x").toInt(),
                                                    windowData.value("canvas_y").toInt(),
                                                    windowData.value("canvas_width").toInt(),
                                                    windowData.value("canvas_height").toInt());
        windowsArray.append(QJsonObject::fromVariantMap(windowData));
    }
    
    QJsonDocument doc(windowsArray);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        qint64 bytesWritten = file.write(doc.toJson());
        file.close();
        qDebug() << "  SUCCESS: Wrote" << bytesWritten << "bytes to disk";
        
        #ifdef Q_OS_WIN
        // Set hidden attribute on Windows
        SetFileAttributesW(reinterpret_cast<const wchar_t *>(filePath.utf16()), FILE_ATTRIBUTE_HIDDEN);
        #endif
    } else {
        qDebug() << "  ERROR: Failed to open file for writing:" << file.errorString();
    }
}

QList<MarkdownWindow*> MarkdownWindowManager::loadWindowData(int pageNumber) {
    QList<MarkdownWindow*> windows;
    
    QString filePath = getWindowDataFilePath(pageNumber);
    qDebug() << "==========================================";
    qDebug() << "MarkdownWindowManager::loadWindowData(" << pageNumber << ")";
    qDebug() << "  Reading from file:" << filePath;
    
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        qDebug() << "  File doesn't exist or path is empty";
        return windows;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "  ERROR: Failed to open file for reading:" << file.errorString();
        return windows;
    }
    
    QByteArray data = file.readAll();
    file.close();
    qDebug() << "  Read" << data.size() << "bytes from disk";
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "  ERROR: Failed to parse markdown window data:" << error.errorString();
        return windows;
    }
    
    QJsonArray windowsArray = doc.array();
    qDebug() << "  Found" << windowsArray.size() << "windows in file";
    
    for (int i = 0; i < windowsArray.size(); i++) {
        QJsonObject windowObj = windowsArray[i].toObject();
        QVariantMap windowData = windowObj.toVariantMap();
        
        qDebug() << "  Loading window" << i;
        qDebug() << "    Content:" << windowData.value("content").toString().left(50) << "...";
        qDebug() << "    Canvas rect:" << QRect(windowData.value("canvas_x").toInt(),
                                                  windowData.value("canvas_y").toInt(),
                                                  windowData.value("canvas_width").toInt(),
                                                  windowData.value("canvas_height").toInt());
        
        // Create window with default rect (will be updated by deserialize)
        MarkdownWindow *window = new MarkdownWindow(QRect(0, 0, 300, 200), canvas);
        window->deserialize(windowData);
        
        qDebug() << "    Created window:" << window;
        
        // Connect signals
        connectWindowSignals(window);
        
        windows.append(window);
        window->show();
    }
    
    qDebug() << "  Loaded" << windows.size() << "windows total";
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

void MarkdownWindowManager::flushPendingContentUpdates() {
    // Process all pending content updates
    for (auto it = pendingContentUpdates.begin(); it != pendingContentUpdates.end(); ++it) {
        MarkdownWindow *window = it.key();
        QString newContent = it.value();
        
        // Get the page this window belongs to
        if (!canvas) continue;
        
        QRect windowRect = window->getCanvasRect();
        int windowPage = getPageNumberFromCanvasY(windowRect.y());
        
        qDebug() << "  [Debounced] Updating cache for window" << window << "on page:" << windowPage;
        qDebug() << "  Content:" << newContent.left(50) << "...";
        
        // Update the permanent cache
        bool cacheUpdated = false;
        if (pageWindows.contains(windowPage)) {
            QList<MarkdownWindow*> &cachedWindows = pageWindows[windowPage];
            
            // ✅ In combined mode, cache stores clones with page-relative coordinates
            // We need to match by comparing the window's position adjusted to page-relative
            bool isCombinedMode = (combinedFirstPage >= 0 && combinedSecondPage >= 0);
            QRect searchRect = windowRect;
            
            if (isCombinedMode && windowPage == combinedSecondPage) {
                // Window is on second page, adjust to page-relative for matching
                int singlePageHeight = canvas->getBufferHeight() / 2;
                searchRect.moveTop(windowRect.y() - singlePageHeight);
            }
            
            for (MarkdownWindow *cachedWindow : cachedWindows) {
                // Match by page-relative position
                if (cachedWindow->getCanvasRect() == searchRect) {
                    // Found matching cached clone - update its content
                    cachedWindow->setMarkdownContent(newContent);
                    qDebug() << "  Updated cached clone for page" << windowPage;
                    cacheUpdated = true;
                    break;
                }
            }
        }
        
        // ✅ CRITICAL FIX: If the cache doesn't exist yet (e.g., freshly created window),
        // we still need to mark the page as dirty so the content gets saved!
        if (!cacheUpdated) {
            qDebug() << "  Cache not found for page" << windowPage << "(likely a new window)";
        }
        
        // Always mark the page as dirty when content changes, regardless of cache state
        dirtyPages.insert(windowPage);
        qDebug() << "  Marked page" << windowPage << "as dirty";
    }
    
    // Clear pending updates
    pendingContentUpdates.clear();
}

void MarkdownWindowManager::flushPendingGeometryUpdates() {
    // ✅ Use tracked original Y positions to determine which page windows started on
    bool isCombinedMode = (combinedFirstPage >= 0 && combinedSecondPage >= 0);
    
    // Process all pending geometry updates (move/resize operations)
    for (MarkdownWindow *window : pendingGeometryUpdates) {
        if (!canvas) continue;
        
        QRect windowRect = window->getCanvasRect();
        int newPage = getPageNumberFromCanvasY(windowRect.y());
        
        qDebug() << "  [Debounced] Processing geometry change for window" << window << "on page:" << newPage;
        qDebug() << "  New geometry:" << windowRect;
        
        // ✅ CRITICAL FIX: Check if the window moved to a different page
        // If so, we need to remove it from the old page cache and add it to the new page cache
        int oldPage = -1;
        
        // In combined mode, use the tracked original Y position to determine original page
        if (isCombinedMode && pendingGeometryOriginalY.contains(window)) {
            int originalY = pendingGeometryOriginalY[window];
            oldPage = getPageNumberFromCanvasY(originalY);
            qDebug() << "  Original position: Y=" << originalY << "(page" << oldPage << ")";
        } else {
            // Single page mode or no tracked position: check cache
            for (auto it = pageWindows.begin(); it != pageWindows.end(); ++it) {
                if (it.value().contains(window)) {
                    oldPage = it.key();
                    break;
                }
            }
        }
        
        if (oldPage != -1 && oldPage != newPage) {
            qDebug() << "  Window moved from page" << oldPage << "to page" << newPage;
            
            // Remove from old page cache
            pageWindows[oldPage].removeAll(window);
            dirtyPages.insert(oldPage);
            qDebug() << "  Removed from page" << oldPage << "cache and marked dirty";
            
            // Add to new page cache
            if (pageWindows.contains(newPage)) {
                pageWindows[newPage].append(window);
            } else {
                pageWindows[newPage] = QList<MarkdownWindow*>{window};
            }
            dirtyPages.insert(newPage);
            qDebug() << "  Added to page" << newPage << "cache and marked dirty";
        } else {
            // Same page, just update geometry
            dirtyPages.insert(newPage);
            qDebug() << "  Marked page" << newPage << "as dirty";
            
            // Update permanent cache
            updatePermanentCacheForWindow(window, newPage);
        }
    }
    
    // Clear pending updates and tracked positions
    pendingGeometryUpdates.clear();
    pendingGeometryOriginalY.clear();
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
    // Note: Called on every keystroke - debug output is minimized to reduce spam
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

int MarkdownWindowManager::getPageNumberFromCanvasY(int canvasY) const {
    // Calculate which page a canvas Y coordinate belongs to
    // This is crucial for combined mode where multiple pages are shown at once
    
    if (!canvas) {
        return 0; // Fallback
    }
    
    // Check if we're in combined mode
    bool isCombinedMode = (combinedFirstPage >= 0 && combinedSecondPage >= 0);
    
    if (!isCombinedMode) {
        // Single page mode - use the current page from the cache
        // If we have current windows, they all belong to the same page
        if (!currentWindows.isEmpty() && pageWindows.size() > 0) {
            // Find which page the current windows belong to
            for (auto it = pageWindows.constBegin(); it != pageWindows.constEnd(); ++it) {
                if (it.value() == currentWindows || 
                    (!it.value().isEmpty() && !currentWindows.isEmpty() && 
                     it.value().first() == currentWindows.first())) {
                    return it.key();
                }
            }
        }
        return 0; // Fallback
    }
    
    // Combined mode - calculate which page based on Y coordinate
    int bufferHeight = canvas->getBufferHeight();
    int singlePageHeight = bufferHeight / 2;
    
    // Top half shows combinedFirstPage, bottom half shows combinedSecondPage
    if (canvasY < singlePageHeight) {
        return combinedFirstPage;
    } else {
        return combinedSecondPage;
    }
}

void MarkdownWindowManager::updatePermanentCacheForWindow(MarkdownWindow *modifiedWindow, int pageNumber) {
    // ✅ USER MODIFICATION FIX: Update permanent cache when user moves/resizes a window
    // This ensures changes persist even during combined mode
    if (!pageWindows.contains(pageNumber)) {
        return; // No cache for this page yet
    }

    // Find the permanent cached window that corresponds to this modified window
    // Match by content since that's the closest thing to a unique identifier
    QString content = modifiedWindow->getMarkdownContent();
    QList<MarkdownWindow*> &cachedWindows = pageWindows[pageNumber];

    for (MarkdownWindow *cachedWindow : cachedWindows) {
        if (cachedWindow && cachedWindow->getMarkdownContent() == content) {
            // Update the permanent cached window's position/size
            cachedWindow->setCanvasRect(modifiedWindow->getCanvasRect());
            break;
        }
    }
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
            
            // ✅ FIX: Determine which page this window belongs to based on its Y coordinate
            QRect windowRect = window->getCanvasRect();
            int windowPage = getPageNumberFromCanvasY(windowRect.y());
            
            // ✅ DEBOUNCE: Queue the content update instead of applying immediately
            // This prevents excessive cache updates and debug prints on every keystroke
            // The actual cache update happens 500ms after the last keystroke
            pendingContentUpdates[window] = window->getMarkdownContent();
            
            // Restart the debounce timer (resets if already running)
            contentDebounceTimer->stop();
            contentDebounceTimer->start();
            
            // Note: Minimal debug output to avoid spam on every keystroke
            // Actual cache update will be logged when debounce timer fires
        }
    });
    connect(window, &MarkdownWindow::windowMoved, this, [this, window](MarkdownWindow*) {
        // Note: Called on every pixel of movement during dragging - minimize debug output
        // qDebug() << "windowMoved signal triggered for window:" << window;
        
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
            
            // ✅ DEBOUNCE: Queue the geometry update instead of applying immediately
            // This prevents marking dirty on every pixel during dragging
            // The actual dirty marking and cache update happen 200ms after the last move
            if (!suppressDirtyMarking) {
                // ✅ Track original Y position when FIRST queued (for detecting page changes)
                if (!pendingGeometryUpdates.contains(window)) {
                    pendingGeometryOriginalY[window] = window->getCanvasRect().y();
                }
                pendingGeometryUpdates.insert(window);
                
                // Restart the debounce timer (resets if already running)
                geometryDebounceTimer->stop();
                geometryDebounceTimer->start();
            }
        }
    });
    connect(window, &MarkdownWindow::windowResized, this, [this, window](MarkdownWindow*) {
        // Note: Called on every pixel of resize during dragging - minimize debug output
        // qDebug() << "windowResized signal triggered for window:" << window;
        
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

            // ✅ DEBOUNCE: Queue the geometry update instead of applying immediately
            // This prevents marking dirty on every pixel during resizing
            // The actual dirty marking and cache update happen 200ms after the last resize
            if (!suppressDirtyMarking) {
                // ✅ Track original Y position when FIRST queued (for detecting page changes)
                if (!pendingGeometryUpdates.contains(window)) {
                    pendingGeometryOriginalY[window] = window->getCanvasRect().y();
                }
                pendingGeometryUpdates.insert(window);
                
                // Restart the debounce timer (resets if already running)
                geometryDebounceTimer->stop();
                geometryDebounceTimer->start();
            } else {
                qDebug() << "==========================================";
                qDebug() << "windowResized signal triggered for window:" << window;
                qDebug() << "  New canvas rect:" << window->getCanvasRect();
                qDebug() << "  Skipping dirty marking (loading operation)";
            }
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