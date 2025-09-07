#include "RecentNotebooksManager.h"
#include "InkCanvas.h"
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QApplication>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDataStream>
#include <QDateTime>
#include <QCryptographicHash>
#include <QTimer>
#include <QPointer>

RecentNotebooksManager::RecentNotebooksManager(QObject *parent)
    : QObject(parent), settings("SpeedyNote", "App") {
    QDir().mkpath(getCoverImageDir()); // Ensure cover image directory exists
    loadRecentNotebooks();
    loadStarredNotebooks();
}

void RecentNotebooksManager::addRecentNotebook(const QString& folderPath, InkCanvas* canvasForPreview) {
    if (folderPath.isEmpty()) return;

    // ✅ Use display path to avoid duplicates between .spn files and temp folders
    QString displayPath = folderPath;
    if (canvasForPreview) {
        displayPath = canvasForPreview->getDisplayPath();
    }
    
    // ✅ Normalize path separators to prevent duplicates
    displayPath = QDir::toNativeSeparators(QFileInfo(displayPath).absoluteFilePath());

    // ✅ More aggressive deduplication - remove any entries that might be related
    QString pdfPath;
    if (canvasForPreview) {
        pdfPath = canvasForPreview->getPdfPath();
    }
    
    // Remove entries that match either the display path or are related to the same PDF or notebook ID
    QStringList toRemove;
    QString currentNotebookId;
    if (canvasForPreview) {
        currentNotebookId = canvasForPreview->getNotebookId();
    }
    
    for (const QString& existingPath : recentNotebookPaths) {
        bool shouldRemove = false;
        
        // ✅ Normalize existing path for comparison
        QString normalizedExistingPath = QDir::toNativeSeparators(QFileInfo(existingPath).absoluteFilePath());
        
        if (normalizedExistingPath == displayPath) {
            shouldRemove = true;
        } else if (!pdfPath.isEmpty()) {
            // Check if this existing entry is related to the same PDF
            QString existingPdfPath = getPdfPathFromNotebook(existingPath);
            if (!existingPdfPath.isEmpty() && 
                QFileInfo(existingPdfPath).absoluteFilePath() == QFileInfo(pdfPath).absoluteFilePath()) {
                shouldRemove = true;
            }
        }
        
        // Also check notebook ID if available
        if (!shouldRemove && !currentNotebookId.isEmpty()) {
            QString existingNotebookId = getNotebookIdFromPath(existingPath);
            if (!existingNotebookId.isEmpty() && existingNotebookId == currentNotebookId) {
                shouldRemove = true;
            }
        }
        
        if (shouldRemove) {
            toRemove.append(existingPath);
        }
    }
    
    for (const QString& pathToRemove : toRemove) {
        recentNotebookPaths.removeAll(pathToRemove);
    }
    
    recentNotebookPaths.prepend(displayPath);

    // Trim the list if it exceeds the maximum size
    while (recentNotebookPaths.size() > MAX_RECENT_NOTEBOOKS) {
        recentNotebookPaths.removeLast();
    }

    // ✅ Generate thumbnail immediately, but also schedule a delayed one for better content
    generateAndSaveCoverPreview(displayPath, canvasForPreview);
    saveRecentNotebooks();
    
    // ✅ Schedule a delayed thumbnail generation to capture content after page loads
    if (canvasForPreview) {
        // Use QPointer to safely handle canvas deletion
        QPointer<InkCanvas> canvasPtr(canvasForPreview);
        QTimer::singleShot(500, this, [this, displayPath, canvasPtr]() {
            if (canvasPtr && !canvasPtr.isNull()) { // Safe check for canvas validity
                generateAndSaveCoverPreview(displayPath, canvasPtr.data());
            }
        });
    }
}

QStringList RecentNotebooksManager::getRecentNotebooks() const {
    return recentNotebookPaths;
}

void RecentNotebooksManager::removeRecentNotebook(const QString& folderPath) {
    if (folderPath.isEmpty()) return;
    
    // ✅ Normalize path for comparison
    QString normalizedPath = QDir::toNativeSeparators(QFileInfo(folderPath).absoluteFilePath());
    
    // Remove all matching entries (there might be multiple due to different path formats)
    if (recentNotebookPaths.removeAll(normalizedPath) > 0) {
        saveRecentNotebooks();
    }
    
    // Also try to remove the original path in case normalization changed it
    if (recentNotebookPaths.removeAll(folderPath) > 0) {
        saveRecentNotebooks();
    }
}

void RecentNotebooksManager::loadRecentNotebooks() {
    QStringList rawPaths = settings.value("recentNotebooks").toStringList();
    
    // ✅ Normalize all loaded paths to prevent separator inconsistencies
    recentNotebookPaths.clear();
    for (const QString& path : rawPaths) {
        if (!path.isEmpty()) {
            QString normalizedPath = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
            recentNotebookPaths.append(normalizedPath);
        }
    }
}

void RecentNotebooksManager::saveRecentNotebooks() {
    settings.setValue("recentNotebooks", recentNotebookPaths);
}

QString RecentNotebooksManager::sanitizeFolderName(const QString& folderPath) const {
    // ✅ Create a more unique cache key to prevent collisions
    QString baseName = QFileInfo(folderPath).baseName();
    QString absolutePath = QFileInfo(folderPath).absoluteFilePath();
    
    // Create a hash of the full path to ensure uniqueness
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(absolutePath.toUtf8());
    QString pathHash = hash.result().toHex().left(8); // First 8 chars of hash
    
    return QString("%1_%2").arg(baseName.replace(QRegularExpression("[^a-zA-Z0-9_]"), "_")).arg(pathHash);
}

QString RecentNotebooksManager::getCoverImageDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/RecentCovers/";
}

void RecentNotebooksManager::generateAndSaveCoverPreview(const QString& folderPath, InkCanvas* optionalCanvas) {
    QString coverDir = getCoverImageDir();
    QString coverFileName = sanitizeFolderName(folderPath) + "_cover.png";
    QString coverFilePath = coverDir + coverFileName;

    QImage coverImage(400, 300, QImage::Format_ARGB32_Premultiplied);
    coverImage.fill(Qt::white); // Default background
    QPainter painter(&coverImage);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // ✅ Try canvas grab first, but with better validation
    bool canvasGrabSuccessful = false;
    if (optionalCanvas && optionalCanvas->width() > 0 && optionalCanvas->height() > 0) {
        int logicalCanvasWidth = optionalCanvas->width();
        int logicalCanvasHeight = optionalCanvas->height();

        double targetAspectRatio = 4.0 / 3.0;
        double canvasAspectRatio = (double)logicalCanvasWidth / logicalCanvasHeight;

        int grabWidth, grabHeight;
        int xOffset = 0, yOffset = 0;

        if (canvasAspectRatio > targetAspectRatio) { // Canvas is wider than target 4:3
            grabHeight = logicalCanvasHeight;
            grabWidth = qRound(logicalCanvasHeight * targetAspectRatio);
            xOffset = (logicalCanvasWidth - grabWidth) / 2;
        } else { // Canvas is taller than or equal to target 4:3
            grabWidth = logicalCanvasWidth;
            grabHeight = qRound(logicalCanvasWidth / targetAspectRatio);
            yOffset = (logicalCanvasHeight - grabHeight) / 2;
        }

        if (grabWidth > 0 && grabHeight > 0) {
            QRect grabRect(xOffset, yOffset, grabWidth, grabHeight);
            QPixmap capturedView = optionalCanvas->grab(grabRect);
            
            // ✅ Check if the captured view is not just blank/white
            QImage testImage = capturedView.toImage();
            bool isBlank = true;
            if (!testImage.isNull() && testImage.width() > 0 && testImage.height() > 0) {
                // Sample a few pixels to see if it's not just white
                for (int i = 0; i < qMin(100, testImage.width() * testImage.height()); i += 10) {
                    int x = (i % testImage.width());
                    int y = (i / testImage.width()) % testImage.height();
                    QRgb pixel = testImage.pixel(x, y);
                    if (pixel != qRgb(255, 255, 255)) { // Not white
                        isBlank = false;
                        break;
                    }
                }
            }
            
            if (!isBlank) {
                painter.drawPixmap(coverImage.rect(), capturedView, capturedView.rect());
                canvasGrabSuccessful = true;
            }
        }
    }
    
         // ✅ If canvas grab failed or was blank, try saved files
     if (!canvasGrabSuccessful) {
         // ✅ Fallback: check for saved pages using new JSON metadata system
        QString notebookIdStr;
        QString actualFolderPath = folderPath;
        
        // ✅ Handle .spn packages - extract to get notebook ID
        if (folderPath.endsWith(".spn", Qt::CaseInsensitive)) {
            // For .spn files, we need to extract temporarily to read metadata
            // This is a bit expensive but necessary for thumbnails
            QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                             "/speedynote_thumb_" + QString::number(QDateTime::currentMSecsSinceEpoch());
            QDir().mkpath(tempDir);
            
            // Simple extraction for thumbnail purposes (we'll clean up after)
            QFile spnFile(folderPath);
            if (spnFile.open(QIODevice::ReadOnly)) {
                QDataStream in(&spnFile);
                
                // Skip header
                QString header;
                in >> header;
                if (header == "Contents") {
                    int fileCount;
                    in >> fileCount;
                    
                    for (int i = 0; i < fileCount; ++i) {
                        QString fileName;
                        QByteArray fileData;
                        in >> fileName >> fileData;
                        
                        QString filePath = tempDir + "/" + fileName;
                        QDir().mkpath(QFileInfo(filePath).absolutePath());
                        
                        QFile outFile(filePath);
                        if (outFile.open(QIODevice::WriteOnly)) {
                            outFile.write(fileData);
                            outFile.close();
                        }
                    }
                }
                spnFile.close();
                actualFolderPath = tempDir;
            }
        }
        
        // ✅ Try new JSON metadata system first
        QString jsonFile = actualFolderPath + "/.speedynote_metadata.json";
        if (QFile::exists(jsonFile)) {
            QFile file(jsonFile);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QByteArray data = file.readAll();
                file.close();
                
                QJsonParseError error;
                QJsonDocument doc = QJsonDocument::fromJson(data, &error);
                
                if (error.error == QJsonParseError::NoError) {
                    QJsonObject obj = doc.object();
                    notebookIdStr = obj["notebook_id"].toString();
                }
            }
        }
        
        // ✅ Fallback to old system for backwards compatibility
        if (notebookIdStr.isEmpty()) {
            QFile idFile(actualFolderPath + "/.notebook_id.txt");
            if (idFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&idFile);
                notebookIdStr = in.readLine().trimmed();
                idFile.close();
            }
        }

        QString firstPagePath, firstAnnotatedPagePath;
        if (!notebookIdStr.isEmpty()) {
            firstPagePath = actualFolderPath + QString("/%1_00000.png").arg(notebookIdStr);
            firstAnnotatedPagePath = actualFolderPath + QString("/annotated_%1_00000.png").arg(notebookIdStr);
        }

        QImage pageImage;
        if (!firstAnnotatedPagePath.isEmpty() && QFile::exists(firstAnnotatedPagePath)) {
            pageImage.load(firstAnnotatedPagePath);
        } else if (!firstPagePath.isEmpty() && QFile::exists(firstPagePath)) {
            pageImage.load(firstPagePath);
        }

        if (!pageImage.isNull()) {
            painter.drawImage(coverImage.rect(), pageImage, pageImage.rect());
        } else {
            // If no image found, draw a placeholder
            painter.fillRect(coverImage.rect(), Qt::darkGray);
            painter.setPen(Qt::white);
            painter.drawText(coverImage.rect(), Qt::AlignCenter, tr("No Page 0 Preview"));
        }
        
        // ✅ Clean up temporary directory if we extracted an .spn file
        if (actualFolderPath != folderPath && actualFolderPath.contains("/speedynote_thumb_")) {
            QDir tempDir(actualFolderPath);
            tempDir.removeRecursively();
        }
    }
    coverImage.save(coverFilePath, "PNG");
}

QString RecentNotebooksManager::getCoverImagePathForNotebook(const QString& folderPath) const {
    QString coverDir = getCoverImageDir();
    QString coverFileName = sanitizeFolderName(folderPath) + "_cover.png";
    QString coverFilePath = coverDir + coverFileName;
    if (QFile::exists(coverFilePath)) {
        return coverFilePath;
    }
    return ""; // Return empty if no cover exists
}

QString RecentNotebooksManager::getPdfPathFromNotebook(const QString& folderPath) const {
    QString actualFolderPath = folderPath;
    bool needsCleanup = false;
    
    // ✅ Handle .spn packages - extract temporarily to read PDF path
    if (folderPath.endsWith(".spn", Qt::CaseInsensitive)) {
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                         "/speedynote_pdf_check_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(tempDir);
        
        QFile spnFile(folderPath);
        if (spnFile.open(QIODevice::ReadOnly)) {
            QDataStream in(&spnFile);
            QString header;
            in >> header;
            if (header == "Contents") {
                int fileCount;
                in >> fileCount;
                
                for (int i = 0; i < fileCount; ++i) {
                    QString fileName;
                    QByteArray fileData;
                    in >> fileName >> fileData;
                    
                    if (fileName == ".speedynote_metadata.json") {
                        QString filePath = tempDir + "/" + fileName;
                        QDir().mkpath(QFileInfo(filePath).absolutePath()); // Ensure directory exists
                        QFile outFile(filePath);
                        if (outFile.open(QIODevice::WriteOnly)) {
                            outFile.write(fileData);
                            outFile.close();
                        }
                        break;
                    }
                }
            }
            spnFile.close();
            actualFolderPath = tempDir;
            needsCleanup = true;
        }
    }
    
    QString pdfPath;
    
    // ✅ Check for PDF metadata in JSON first
    QString jsonFile = actualFolderPath + "/.speedynote_metadata.json";
    if (QFile::exists(jsonFile)) {
        QFile file(jsonFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray data = file.readAll();
            file.close();
            
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(data, &error);
            
            if (error.error == QJsonParseError::NoError) {
                QJsonObject obj = doc.object();
                pdfPath = obj["pdf_path"].toString();
            }
        }
    }
    
    // ✅ Fallback to old system
    if (pdfPath.isEmpty()) {
        QString metadataFile = actualFolderPath + "/.pdf_path.txt";
        if (QFile::exists(metadataFile)) {
            QFile file(metadataFile);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                pdfPath = in.readLine().trimmed();
                file.close();
            }
        }
    }
    
    // ✅ Clean up temp directory if we extracted an .spn file
    if (needsCleanup && actualFolderPath.contains("/speedynote_pdf_check_")) {
        QDir tempDir(actualFolderPath);
        tempDir.removeRecursively();
    }
    
    return pdfPath;
}

QString RecentNotebooksManager::getNotebookIdFromPath(const QString& folderPath) const {
    QString actualFolderPath = folderPath;
    bool needsCleanup = false;
    
    // ✅ Handle .spn packages - extract temporarily to read notebook ID
    if (folderPath.endsWith(".spn", Qt::CaseInsensitive)) {
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                         "/speedynote_id_check_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(tempDir);
        
        QFile spnFile(folderPath);
        if (spnFile.open(QIODevice::ReadOnly)) {
            QDataStream in(&spnFile);
            QString header;
            in >> header;
            if (header == "Contents") {
                int fileCount;
                in >> fileCount;
                
                for (int i = 0; i < fileCount; ++i) {
                    QString fileName;
                    QByteArray fileData;
                    in >> fileName >> fileData;
                    
                    if (fileName == ".speedynote_metadata.json") {
                        QString filePath = tempDir + "/" + fileName;
                        QDir().mkpath(QFileInfo(filePath).absolutePath()); // Ensure directory exists
                        QFile outFile(filePath);
                        if (outFile.open(QIODevice::WriteOnly)) {
                            outFile.write(fileData);
                            outFile.close();
                        }
                        break;
                    }
                }
            }
            spnFile.close();
            actualFolderPath = tempDir;
            needsCleanup = true;
        }
    }
    
    QString notebookId;
    
    // ✅ Try new JSON metadata system first
    QString jsonFile = actualFolderPath + "/.speedynote_metadata.json";
    if (QFile::exists(jsonFile)) {
        QFile file(jsonFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray data = file.readAll();
            file.close();
            
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(data, &error);
            
            if (error.error == QJsonParseError::NoError) {
                QJsonObject obj = doc.object();
                notebookId = obj["notebook_id"].toString();
            }
        }
    }
    
    // ✅ Fallback to old system
    if (notebookId.isEmpty()) {
        QFile idFile(actualFolderPath + "/.notebook_id.txt");
        if (idFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&idFile);
            notebookId = in.readLine().trimmed();
            idFile.close();
        }
    }
    

    
    // ✅ Clean up temp directory if we extracted an .spn file
    if (needsCleanup && actualFolderPath.contains("/speedynote_id_check_")) {
        QDir tempDir(actualFolderPath);
        tempDir.removeRecursively();
    }
    
    return notebookId;
}

QString RecentNotebooksManager::getNotebookDisplayName(const QString& folderPath) const {
    QString pdfPath = getPdfPathFromNotebook(folderPath);
    if (!pdfPath.isEmpty()) {
        return QFileInfo(pdfPath).fileName();
    }
    
    // Fallback to folder name
    return QFileInfo(folderPath).fileName();
}

// Starred notebooks functionality
void RecentNotebooksManager::addStarred(const QString& folderPath) {
    if (folderPath.isEmpty()) return;
    
    // ✅ Normalize path to prevent duplicates
    QString normalizedPath = QDir::toNativeSeparators(QFileInfo(folderPath).absoluteFilePath());
    
    if (!starredNotebookPaths.contains(normalizedPath)) {
        starredNotebookPaths.append(normalizedPath);
        saveStarredNotebooks();
    }
}

void RecentNotebooksManager::removeStarred(const QString& folderPath) {
    if (folderPath.isEmpty()) return;
    
    // ✅ Normalize path for comparison
    QString normalizedPath = QDir::toNativeSeparators(QFileInfo(folderPath).absoluteFilePath());
    
    if (starredNotebookPaths.removeAll(normalizedPath) > 0) {
        saveStarredNotebooks();
    }
}

bool RecentNotebooksManager::isStarred(const QString& folderPath) const {
    if (folderPath.isEmpty()) return false;
    
    // ✅ Normalize path for comparison
    QString normalizedPath = QDir::toNativeSeparators(QFileInfo(folderPath).absoluteFilePath());
    
    return starredNotebookPaths.contains(normalizedPath);
}

QStringList RecentNotebooksManager::getStarredNotebooks() const {
    return starredNotebookPaths;
}

void RecentNotebooksManager::loadStarredNotebooks() {
    QStringList rawPaths = settings.value("starredNotebooks").toStringList();
    
    // ✅ Normalize all loaded paths to prevent separator inconsistencies
    starredNotebookPaths.clear();
    for (const QString& path : rawPaths) {
        if (!path.isEmpty()) {
            QString normalizedPath = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
            starredNotebookPaths.append(normalizedPath);
        }
    }
}

void RecentNotebooksManager::saveStarredNotebooks() {
    settings.setValue("starredNotebooks", starredNotebookPaths);
} 