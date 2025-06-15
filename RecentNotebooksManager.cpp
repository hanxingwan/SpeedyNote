#include "RecentNotebooksManager.h"
#include "InkCanvas.h"
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QApplication>
#include <QRegularExpression>
#include <QTextStream>
#include <QTextCodec>

RecentNotebooksManager::RecentNotebooksManager(QObject *parent)
    : QObject(parent), settings("SpeedyNote", "App") {
    QDir().mkpath(getCoverImageDir()); // Ensure cover image directory exists
    loadRecentNotebooks();
}

void RecentNotebooksManager::addRecentNotebook(const QString& folderPath, InkCanvas* canvasForPreview) {
    if (folderPath.isEmpty()) return;

    // Remove if already exists to move it to the front
    recentNotebookPaths.removeAll(folderPath);
    recentNotebookPaths.prepend(folderPath);

    // Trim the list if it exceeds the maximum size
    while (recentNotebookPaths.size() > MAX_RECENT_NOTEBOOKS) {
        recentNotebookPaths.removeLast();
    }

    generateAndSaveCoverPreview(folderPath, canvasForPreview);
    saveRecentNotebooks();
}

QStringList RecentNotebooksManager::getRecentNotebooks() const {
    return recentNotebookPaths;
}

void RecentNotebooksManager::loadRecentNotebooks() {
    recentNotebookPaths = settings.value("recentNotebooks").toStringList();
}

void RecentNotebooksManager::saveRecentNotebooks() {
    settings.setValue("recentNotebooks", recentNotebookPaths);
}

QString RecentNotebooksManager::sanitizeFolderName(const QString& folderPath) const {
    return QFileInfo(folderPath).baseName().replace(QRegularExpression("[^a-zA-Z0-9_]"), "_");
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
            painter.drawPixmap(coverImage.rect(), capturedView, capturedView.rect());
        } else {
            // Fallback if calculated grab dimensions are invalid, draw placeholder or fill
            // This case should ideally not be hit if canvas width/height > 0
            painter.fillRect(coverImage.rect(), Qt::lightGray);
            painter.setPen(Qt::black);
            painter.drawText(coverImage.rect(), Qt::AlignCenter, tr("Preview Error"));
        }
    } else {
        // Fallback: check for a saved "annotated_..._00000.png" or the first page
        // This part remains the same as it deals with pre-rendered images.
        QString notebookIdStr;
        InkCanvas tempCanvas(nullptr); // Temporary canvas to access potential notebookId property logic if it were there
                                     // However, notebookId is specific to an instance with a saveFolder.
                                     // For a generic fallback, we might need a different way or assume a common naming if notebookId is unknown.
                                     // For now, let's assume a simple naming or improve this fallback if needed.
        
        // Attempt to get notebookId if folderPath is a valid notebook with an ID file
        QFile idFile(folderPath + "/.notebook_id.txt");
        if (idFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&idFile);
            in.setCodec("UTF-8"); // Set UTF-8 codec for Qt5
            notebookIdStr = in.readLine().trimmed();
            idFile.close();
        }

        QString firstPagePath, firstAnnotatedPagePath;
        if (!notebookIdStr.isEmpty()) {
            firstPagePath = folderPath + QString("/%1_00000.png").arg(notebookIdStr);
            firstAnnotatedPagePath = folderPath + QString("/annotated_%1_00000.png").arg(notebookIdStr);
        } else {
            // Fallback if no ID, try a generic name (less ideal)
            // This situation should be rare if notebooks are managed properly
            // For robustness, one might iterate files or use a known default page name
            // For this example, we'll stick to a pattern that might not always work without an ID
            // Consider that `InkCanvas(nullptr).property("notebookId")` was problematic as `notebookId` is instance specific.
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

QString RecentNotebooksManager::getNotebookDisplayName(const QString& folderPath) const {
    // Check for PDF metadata first
    QString metadataFile = folderPath + "/.pdf_path.txt";
    if (QFile::exists(metadataFile)) {
        QFile file(metadataFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            in.setCodec("UTF-8"); // Set UTF-8 codec for Qt5
            QString pdfPath = in.readLine().trimmed();
            file.close();
            if (!pdfPath.isEmpty()) {
                return QFileInfo(pdfPath).fileName();
            }
        }
    }
    // Fallback to folder name
    return QFileInfo(folderPath).fileName();
} 