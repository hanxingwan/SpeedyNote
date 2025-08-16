#include "PictureWindowManager.h"
#include "PictureWindow.h"
#include "InkCanvas.h"
#include "MainWindow.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QDebug>
#include <QFileInfo>
#include <QUuid>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QSet>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

PictureWindowManager::PictureWindowManager(InkCanvas *canvas, QObject *parent)
    : QObject(parent), canvas(canvas), selectionMode(false)
{
    // Connect to canvas signals to update window positions when canvas changes
    if (canvas) {
        connect(canvas, &InkCanvas::panChanged, this, &PictureWindowManager::updateAllWindowPositions);
        connect(canvas, &InkCanvas::zoomChanged, this, &PictureWindowManager::updateAllWindowPositions);
    }
}

PictureWindowManager::~PictureWindowManager() {
    clearAllWindows();
    // ✅ Clean up any remaining unused images on destruction
    cleanupUnusedImages();
}

PictureWindow* PictureWindowManager::createPictureWindow(const QRect &rect, const QString &imagePath) {
    // qDebug() << "PictureWindowManager::createPictureWindow() called";
    // qDebug() << "  Rect:" << rect;
    // qDebug() << "  Image path:" << imagePath;
    
    if (!canvas) {
        // qDebug() << "  ERROR: Canvas is null!";
        return nullptr;
    }
    
    if (imagePath.isEmpty()) {
        // qDebug() << "  ERROR: Image path is empty!";
        return nullptr;
    }
    
    // Convert screen coordinates to canvas coordinates
    QRect canvasRect = convertScreenToCanvasRect(rect);
    // qDebug() << "  Canvas rect:" << canvasRect;
    
    // Apply bounds checking to ensure window is within canvas bounds
    QRect canvasBounds = canvas->getCanvasRect();
    // qDebug() << "  Canvas bounds:" << canvasBounds;
    
    // Ensure window stays within canvas bounds
    int maxX = canvasBounds.width() - canvasRect.width();
    int maxY = canvasBounds.height() - canvasRect.height();
    
    canvasRect.setX(qMax(0, qMin(canvasRect.x(), maxX)));
    canvasRect.setY(qMax(0, qMin(canvasRect.y(), maxY)));
    // qDebug() << "  Bounds-checked canvas rect:" << canvasRect;
    
    // Create new picture window with canvas coordinates
    PictureWindow *window = new PictureWindow(canvasRect, imagePath, canvas);
    // qDebug() << "  PictureWindow created successfully";
    
    // Connect signals
    connectWindowSignals(window);
    
    // Add to current windows list
    currentWindows.append(window);
    // qDebug() << "  Added to current windows list. Total windows:" << currentWindows.size();
    
    // Window is invisible - will be rendered by canvas
    // qDebug() << "  Picture window created (invisible, rendered by canvas)";
    
    // Mark canvas as edited since we created a new picture window
    if (canvas) {
        canvas->setEdited(true);
        canvas->update(); // Trigger repaint to show the new picture
    }
    
    emit windowCreated(window);
    // qDebug() << "  windowCreated signal emitted";
    return window;
}

void PictureWindowManager::removePictureWindow(PictureWindow *window) {
    if (!window) return;
    
    // ✅ Delete the associated image file before removing the window
    QString imagePath = window->getImagePath();
    if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
        // Only delete if the image is inside the notebook folder (don't delete user's original files)
        QString saveFolder = getSaveFolder();
        if (!saveFolder.isEmpty() && imagePath.startsWith(saveFolder)) {
            QFile::remove(imagePath);
            // qDebug() << "Deleted image file:" << imagePath;
        }
    }
    
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

void PictureWindowManager::clearAllWindows() {
    // Clear current windows
    for (PictureWindow *window : currentWindows) {
        if (window) {
            // ✅ Delete the associated image file before removing the window
            QString imagePath = window->getImagePath();
            if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
                // Only delete if the image is inside the notebook folder (don't delete user's original files)
                QString saveFolder = getSaveFolder();
                if (!saveFolder.isEmpty() && imagePath.startsWith(saveFolder)) {
                    QFile::remove(imagePath);
                    // qDebug() << "Cleared all image file:" << imagePath;
                }
            }
        }
        window->deleteLater();
    }
    currentWindows.clear();
    
    // Clear page windows
    for (auto it = pageWindows.begin(); it != pageWindows.end(); ++it) {
        for (PictureWindow *window : it.value()) {
            if (window) {
                // ✅ Delete the associated image file before removing the window
                QString imagePath = window->getImagePath();
                if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
                    // Only delete if the image is inside the notebook folder (don't delete user's original files)
                    QString saveFolder = getSaveFolder();
                    if (!saveFolder.isEmpty() && imagePath.startsWith(saveFolder)) {
                        QFile::remove(imagePath);
                        // qDebug() << "Cleared all page image file:" << imagePath;
                    }
                }
            }
            window->deleteLater();
        }
    }
    pageWindows.clear();
}

void PictureWindowManager::saveWindowsForPage(int pageNumber) {
    if (!canvas) return;
    
    // Update page windows map (even if empty - this is important for cleared pages)
    pageWindows[pageNumber] = currentWindows;
    
    // Save to file (even if empty - this clears the file for cleared pages)
    savePictureData(pageNumber, currentWindows);
}

void PictureWindowManager::loadWindowsForPage(int pageNumber) {
    if (!canvas) return;

    QList<PictureWindow*> newPageWindows;

    // Check if we have windows for this page in memory
    if (pageWindows.contains(pageNumber)) {
        newPageWindows = pageWindows[pageNumber];
    } else {
        // Load from file
        newPageWindows = loadPictureData(pageNumber);

        // Update page windows map
        if (!newPageWindows.isEmpty()) {
            pageWindows[pageNumber] = newPageWindows;
        }
    }

    // Update the current window list and show the windows
    currentWindows = newPageWindows;
    for (PictureWindow *window : currentWindows) {
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
        
        // Update screen position (even though window is invisible)
        window->updateScreenPosition();
    }
    
    // Trigger canvas repaint to show loaded pictures
    if (canvas && !currentWindows.isEmpty()) {
        canvas->update();
    }
}

void PictureWindowManager::deleteWindowsForPage(int pageNumber) {
    // Remove windows from memory
    if (pageWindows.contains(pageNumber)) {
        QList<PictureWindow*> windows = pageWindows[pageNumber];
        for (PictureWindow *window : windows) {
            if (window) {
                // ✅ Delete the associated image file before removing the window
                QString imagePath = window->getImagePath();
                if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
                    // Only delete if the image is inside the notebook folder (don't delete user's original files)
                    QString saveFolder = getSaveFolder();
                    if (!saveFolder.isEmpty() && imagePath.startsWith(saveFolder)) {
                        QFile::remove(imagePath);
                        // qDebug() << "Deleted page image file:" << imagePath;
                    }
                }
            }
            window->deleteLater();
        }
        pageWindows.remove(pageNumber);
    }
    
    // Delete metadata file
    QString filePath = getPictureDataFilePath(pageNumber);
    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
    
    // Clear current windows if they belong to this page
    if (pageWindows.value(pageNumber) == currentWindows) {
        currentWindows.clear();
    }
}

void PictureWindowManager::hideAllWindows() {
    // Hide all current windows
    for (PictureWindow *window : currentWindows) {
        window->hide();
    }
}

void PictureWindowManager::setSelectionMode(bool enabled) {
    selectionMode = enabled;
    
    // Change cursor for canvas
    if (canvas) {
        canvas->setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
    }
}

QList<PictureWindow*> PictureWindowManager::getCurrentPageWindows() const {
    return currentWindows;
}

void PictureWindowManager::exitAllEditModes() {
    for (PictureWindow *window : currentWindows) {
        if (window && window->isInEditMode()) {
            window->forceExitEditMode();
        }
    }
}

void PictureWindowManager::renderPicturesToCanvas(QPainter &painter) const {
    if (!canvas) return;
    
    // Render all current picture windows to the canvas
    for (PictureWindow *window : currentWindows) {
        if (window) {
            // Get the canvas coordinates of the picture window
            QRect canvasRect = window->getCanvasRect();
            
            // Render the picture window content directly in canvas coordinates
            window->renderToCanvas(painter, canvasRect);
        }
    }
}

void PictureWindowManager::renderPicturesToCanvas(QPainter &painter, const QRect &updateRect) const {
    if (!canvas) return;
    
    // Only render picture windows that intersect with the update rectangle
    for (PictureWindow *window : currentWindows) {
        if (window) {
            // Get the canvas coordinates of the picture window
            QRect canvasRect = window->getCanvasRect();
            
            // Only render if the picture intersects with the update area
            if (canvasRect.intersects(updateRect)) {
                // Simple cached rendering - no complex clipping
                window->renderToCanvas(painter, canvasRect);
            }
        }
    }
}

PictureWindow* PictureWindowManager::hitTest(const QPoint &canvasPos) const {
    // Check if the canvas position hits any picture window (in reverse order for top-most)
    for (int i = currentWindows.size() - 1; i >= 0; --i) {
        PictureWindow *window = currentWindows[i];
        if (window && window->getCanvasRect().contains(canvasPos)) {
            return window;
        }
    }
    return nullptr;
}

QString PictureWindowManager::copyImageToNotebook(const QString &sourcePath, int pageNumber) {
    // qDebug() << "PictureWindowManager::copyImageToNotebook() called";
    // qDebug() << "  Source path:" << sourcePath;
    // qDebug() << "  Page number:" << pageNumber;
    
    QString saveFolder = getSaveFolder();
    // qDebug() << "  Save folder:" << saveFolder;
    
    if (saveFolder.isEmpty()) {
        // qDebug() << "  ERROR: Save folder is empty!";
        return QString();
    }
    
    if (!QFile::exists(sourcePath)) {
        // qDebug() << "  ERROR: Source file does not exist!";
        return QString();
    }
    
    // Generate unique filename
    QString targetFileName = generateImageFileName(sourcePath, pageNumber);
    QString targetPath = saveFolder + "/" + targetFileName;
    // qDebug() << "  Target filename:" << targetFileName;
    // qDebug() << "  Target path:" << targetPath;
    
    // Copy the file
    if (QFile::exists(targetPath)) {
        // qDebug() << "  Target file already exists, removing it first";
        QFile::remove(targetPath); // Remove existing file if it exists
    }
    
    QFileInfo sourceInfo(sourcePath);
    // qDebug() << "  Source file size:" << sourceInfo.size() << "bytes";
    
    if (QFile::copy(sourcePath, targetPath)) {
        QFileInfo targetInfo(targetPath);
        // qDebug() << "  SUCCESS: File copied successfully!";
        // qDebug() << "  Target file size:" << targetInfo.size() << "bytes";
        
        #ifdef Q_OS_WIN
        // Set hidden attribute on Windows (optional, for consistency with other metadata files)
        // SetFileAttributesW(reinterpret_cast<const wchar_t *>(targetPath.utf16()), FILE_ATTRIBUTE_HIDDEN);
        #endif
        
        return targetPath;
    } else {
        // qDebug() << "  ERROR: Failed to copy file!";
        return QString();
    }
}

void PictureWindowManager::cleanupUnusedImages() {
    // ✅ Clean up image files that are no longer referenced by any picture windows
    QString saveFolder = getSaveFolder();
    if (saveFolder.isEmpty()) return;
    
    // Get all image files in the notebook folder
    QDir notebookDir(saveFolder);
    QStringList imageFilters;
    imageFilters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.gif" << "*.tiff" << "*.webp";
    QStringList imageFiles = notebookDir.entryList(imageFilters, QDir::Files);
    
    // Get all currently referenced image paths
    QSet<QString> referencedImages;
    
    // Check current windows
    for (PictureWindow *window : currentWindows) {
        if (window) {
            QString imagePath = window->getImagePath();
            if (!imagePath.isEmpty()) {
                referencedImages.insert(QFileInfo(imagePath).fileName());
            }
        }
    }
    
    // Check all page windows
    for (auto it = pageWindows.begin(); it != pageWindows.end(); ++it) {
        for (PictureWindow *window : it.value()) {
            if (window) {
                QString imagePath = window->getImagePath();
                if (!imagePath.isEmpty()) {
                    referencedImages.insert(QFileInfo(imagePath).fileName());
                }
            }
        }
    }
    
    // Delete unreferenced image files (only those that match our naming pattern)
    QString notebookId = getNotebookId();
    for (const QString &fileName : imageFiles) {
        if (fileName.startsWith(notebookId + "_img_") && !referencedImages.contains(fileName)) {
            QString fullPath = saveFolder + "/" + fileName;
            QFile::remove(fullPath);
            // qDebug() << "Cleaned up unused image file:" << fileName;
        }
    }
}

void PictureWindowManager::clearCurrentPageWindows() {
    if (!canvas) return;
    
    int currentPage = canvas->getLastActivePage();
    
    // Delete all current picture windows immediately
    for (PictureWindow *window : currentWindows) {
        if (window) {
            // ✅ Delete the associated image file before removing the window
            QString imagePath = window->getImagePath();
            if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
                // Only delete if the image is inside the notebook folder (don't delete user's original files)
                QString saveFolder = getSaveFolder();
                if (!saveFolder.isEmpty() && imagePath.startsWith(saveFolder)) {
                    QFile::remove(imagePath);
                    // qDebug() << "Cleared image file:" << imagePath;
                }
            }
            
            // Remove from all page windows maps first
            for (auto it = pageWindows.begin(); it != pageWindows.end(); ++it) {
                it.value().removeAll(window);
            }
            
            // Delete immediately to avoid timing issues
            delete window;
        }
    }
    currentWindows.clear();
    
    // Update the pageWindows map to reflect the cleared state
    pageWindows[currentPage] = QList<PictureWindow*>(); // Explicitly empty list
    
    // Mark canvas as edited since we cleared pictures
    canvas->setEdited(true);
    
    // ✅ CACHE FIX: Invalidate note cache when pictures are cleared
    // This ensures that when switching pages and coming back, the cleared pictures don't reappear
    canvas->invalidateCurrentPageCache();
    
    canvas->update(); // Trigger repaint to remove all pictures
    
    // Save the cleared state to disk
    saveWindowsForPage(currentPage);
}

void PictureWindowManager::onWindowDeleteRequested(PictureWindow *window) {
    removePictureWindow(window);
    
    // Mark canvas as edited since we deleted a picture window
    if (canvas) {
        canvas->setEdited(true);
        
        // ✅ CACHE FIX: Invalidate note cache when pictures are deleted
        // This ensures that when switching pages and coming back, the deleted pictures don't reappear
        canvas->invalidateCurrentPageCache();
    }
    
    // Save current state
    if (canvas) {
        int currentPage = canvas->getLastActivePage();
        saveWindowsForPage(currentPage);
    }
}

QString PictureWindowManager::getPictureDataFilePath(int pageNumber) const {
    QString saveFolder = getSaveFolder();
    QString notebookId = getNotebookId();
    
    if (saveFolder.isEmpty() || notebookId.isEmpty()) {
        return QString();
    }
    
    return saveFolder + QString("/.%1_pictures_%2.json").arg(notebookId).arg(pageNumber, 5, 10, QChar('0'));
}

void PictureWindowManager::savePictureData(int pageNumber, const QList<PictureWindow*> &windows) {
    QString filePath = getPictureDataFilePath(pageNumber);
    if (filePath.isEmpty()) return;
    
    QJsonArray windowsArray;
    for (PictureWindow *window : windows) {
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

QList<PictureWindow*> PictureWindowManager::loadPictureData(int pageNumber) {
    QList<PictureWindow*> windows;
    
    QString filePath = getPictureDataFilePath(pageNumber);
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
        qWarning() << "Failed to parse picture window data:" << error.errorString();
        return windows;
    }
    
    QJsonArray windowsArray = doc.array();
    for (const QJsonValue &value : windowsArray) {
        QJsonObject windowObj = value.toObject();
        QVariantMap windowData = windowObj.toVariantMap();
        
        // Get image path from data
        QString imagePath = windowData.value("image_path", "").toString();
        if (imagePath.isEmpty() || !QFile::exists(imagePath)) {
            continue; // Skip windows with missing images
        }
        
        // Create window with default rect (will be updated by deserialize)
        PictureWindow *window = new PictureWindow(QRect(0, 0, 200, 150), imagePath, canvas);
        window->deserialize(windowData);
        
        // Connect signals
        connectWindowSignals(window);
        
        windows.append(window);
        // Window is invisible - will be rendered by canvas
    }
    
    return windows;
}

QString PictureWindowManager::getSaveFolder() const {
    QString result = canvas ? canvas->getSaveFolder() : QString();
    // qDebug() << "PictureWindowManager::getSaveFolder() returning:" << result;
    return result;
}

QString PictureWindowManager::getNotebookId() const {
    if (!canvas) return QString();
    
    // Try to get notebook ID from JSON metadata first
    QString notebookId = canvas->getNotebookId();
    if (!notebookId.isEmpty()) {
        return notebookId;
    }
    
    // Fallback to old method
    QString saveFolder = canvas->getSaveFolder();
    if (saveFolder.isEmpty()) return QString();
    
    QString idFile = saveFolder + "/.notebook_id.txt";
    if (QFile::exists(idFile)) {
        QFile file(idFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString fallbackId = in.readLine().trimmed();
            file.close();
            return fallbackId;
        }
    }
    
    return "notebook"; // Default fallback
}

QString PictureWindowManager::generateImageFileName(const QString &originalPath, int pageNumber) {
    QFileInfo fileInfo(originalPath);
    QString extension = fileInfo.suffix().toLower();
    QString notebookId = getNotebookId();
    
    // Create a hash of the original file path and content for uniqueness
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(originalPath.toUtf8());
    
    // Add file content to hash to ensure uniqueness even for files with same path
    QFile originalFile(originalPath);
    if (originalFile.open(QIODevice::ReadOnly)) {
        hash.addData(originalFile.read(1024)); // Hash first 1KB for performance
        originalFile.close();
    }
    
    QString hashString = hash.result().toHex().left(8); // Use first 8 characters of hash
    
    return QString("%1_img_p%2_%3.%4")
           .arg(notebookId)
           .arg(pageNumber, 5, 10, QChar('0'))
           .arg(hashString)
           .arg(extension);
}

void PictureWindowManager::connectWindowSignals(PictureWindow *window) {
    // Connect existing signals
    connect(window, &PictureWindow::deleteRequested, this, &PictureWindowManager::onWindowDeleteRequested);
    connect(window, &PictureWindow::windowMoved, this, [this, window](PictureWindow*) {
        // ✅ PERFORMANCE: Only mark as edited and save - don't trigger extra updates
        // The canvas handles its own updates more efficiently during movement
        if (canvas) {
            canvas->setEdited(true);
            
            // ✅ CACHE FIX: Invalidate note cache when pictures are moved programmatically
            // (This is a backup - the main fix is in handlePictureMouseRelease)
            canvas->invalidateCurrentPageCache();
            
            // Get current page and save windows
            int currentPage = canvas->getLastActivePage();
            saveWindowsForPage(currentPage);
        }
    });
    connect(window, &PictureWindow::windowResized, this, [this, window](PictureWindow*) {
        // ✅ PERFORMANCE: Only mark as edited and save - don't trigger extra updates
        // The canvas handles its own updates more efficiently during resize
        if (canvas) {
            canvas->setEdited(true);
            
            // ✅ CACHE FIX: Invalidate note cache when pictures are resized programmatically
            // (This is a backup - the main fix is in handlePictureMouseRelease)
            canvas->invalidateCurrentPageCache();
            
            // Get current page and save windows
            int currentPage = canvas->getLastActivePage();
            saveWindowsForPage(currentPage);
        }
    });
    connect(window, &PictureWindow::windowInteracted, this, [this, window](PictureWindow*) {
        // Window was clicked/interacted with
        // Could be used for future features like selection, focus management, etc.
    });
    connect(window, &PictureWindow::editModeChanged, this, [this, window](PictureWindow*, bool enabled) {
        // Handle edit mode changes to disable/enable canvas pan
        if (canvas) {
            canvas->setPictureWindowEditMode(enabled);
            // Only update the picture window's area instead of full canvas
            QRect canvasRect = window->getCanvasRect();
            QRect widgetRect = canvas->mapCanvasToWidget(canvasRect);
            canvas->update(widgetRect.adjusted(-10, -10, 10, 10)); // Larger padding for edit mode borders
            // qDebug() << "PictureWindowManager: Picture window edit mode changed to:" << enabled;
        }
    });
}

void PictureWindowManager::updateAllWindowPositions() {
    // Update positions of all current windows
    for (PictureWindow *window : currentWindows) {
        if (window) {
            window->updateScreenPosition();
        }
    }
}

QRect PictureWindowManager::convertScreenToCanvasRect(const QRect &screenRect) const {
    if (!canvas) {
        return screenRect;
    }
    
    // Use the coordinate conversion methods from InkCanvas
    QRect canvasRect = canvas->mapWidgetToCanvas(screenRect);
    return canvasRect;
}
