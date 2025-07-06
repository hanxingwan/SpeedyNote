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
#ifdef Q_OS_WIN
#include <windows.h>
#endif

MarkdownWindowManager::MarkdownWindowManager(InkCanvas *canvas, QObject *parent)
    : QObject(parent), canvas(canvas)
{
    // Connect to canvas signals if needed
}

MarkdownWindowManager::~MarkdownWindowManager() {
    clearAllWindows();
}

MarkdownWindow* MarkdownWindowManager::createMarkdownWindow(const QRect &rect) {
    if (!canvas) return nullptr;
    
    // Convert screen coordinates to canvas coordinates
    QRect canvasRect = convertScreenToCanvasRect(rect);
    
    // Create new markdown window with canvas coordinates
    MarkdownWindow *window = new MarkdownWindow(canvasRect, canvas);
    
    // Connect signals
    connect(window, &MarkdownWindow::deleteRequested, this, &MarkdownWindowManager::onWindowDeleteRequested);
    connect(window, &MarkdownWindow::contentChanged, this, [this]() {
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
    connect(window, &MarkdownWindow::windowMoved, this, [this](MarkdownWindow*) {
        // Mark canvas as edited since window was moved
        if (canvas) {
            canvas->setEdited(true);
        }
    });
    connect(window, &MarkdownWindow::windowResized, this, [this](MarkdownWindow*) {
        // Mark canvas as edited since window was resized
        if (canvas) {
            canvas->setEdited(true);
        }
    });
    
    // Add to current windows list
    currentWindows.append(window);
    
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

void MarkdownWindowManager::saveWindowsForPage(int pageNumber) {
    if (!canvas || currentWindows.isEmpty()) return;
    
    // Update page windows map
    pageWindows[pageNumber] = currentWindows;
    
    // Save to file
    saveWindowData(pageNumber, currentWindows);
}

void MarkdownWindowManager::loadWindowsForPage(int pageNumber) {
    if (!canvas) return;

    // This method is now only responsible for loading and showing the
    // windows for the specified page. Hiding of old windows is handled before this.

    QList<MarkdownWindow*> newPageWindows;

    // Check if we have windows for this page in memory
    if (pageWindows.contains(pageNumber)) {
        newPageWindows = pageWindows[pageNumber];
    } else {
        // Load from file
        newPageWindows = loadWindowData(pageNumber);

        // Update page windows map
        if (!newPageWindows.isEmpty()) {
            pageWindows[pageNumber] = newPageWindows;
        }
    }

    // Update the current window list and show the windows
    currentWindows = newPageWindows;
    for (MarkdownWindow *window : currentWindows) {
        window->show();
        window->updateScreenPosition();
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
        connect(window, &MarkdownWindow::deleteRequested, this, &MarkdownWindowManager::onWindowDeleteRequested);
        connect(window, &MarkdownWindow::contentChanged, this, [this]() {
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
        connect(window, &MarkdownWindow::windowMoved, this, [this](MarkdownWindow*) {
            // Mark canvas as edited since window was moved
            if (canvas) {
                canvas->setEdited(true);
            }
        });
        connect(window, &MarkdownWindow::windowResized, this, [this](MarkdownWindow*) {
            // Mark canvas as edited since window was resized
            if (canvas) {
                canvas->setEdited(true);
            }
        });
        
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
    if (!canvas) return screenRect;
    
    // Get pan offsets and zoom from canvas
    int panX = 0, panY = 0, zoom = 100;
    
    // Use QMetaObject to call methods dynamically
    QMetaObject::invokeMethod(canvas, "getPanOffsetX", Qt::DirectConnection, Q_RETURN_ARG(int, panX));
    QMetaObject::invokeMethod(canvas, "getPanOffsetY", Qt::DirectConnection, Q_RETURN_ARG(int, panY));
    QMetaObject::invokeMethod(canvas, "getZoom", Qt::DirectConnection, Q_RETURN_ARG(int, zoom));
    
    // Convert screen coordinates to canvas coordinates
    // Canvas = (Screen / (Zoom / 100)) + Pan
    qreal zoomFactor = zoom / 100.0;
    QRect canvasRect;
    canvasRect.setX((screenRect.x() / zoomFactor) + panX);
    canvasRect.setY((screenRect.y() / zoomFactor) + panY);
    canvasRect.setWidth(screenRect.width() / zoomFactor);
    canvasRect.setHeight(screenRect.height() / zoomFactor);
    
        return canvasRect;
}

void MarkdownWindowManager::updateAllWindowPositions() {
    // Update positions of all current windows
    for (MarkdownWindow *window : currentWindows) {
        if (window) {
            window->updateScreenPosition();
        }
    }
}