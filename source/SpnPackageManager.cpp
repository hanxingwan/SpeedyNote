#include "SpnPackageManager.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>
#include <QTemporaryDir>
#include <QDataStream>
#include <QBuffer>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QUuid>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QFile>
#include <QByteArray>
#include <QDataStream>
#include <QVersionNumber>

const QString SpnPackageManager::SPN_EXTENSION = ".spn";
const QString SpnPackageManager::TEMP_PREFIX = "speedynote_";

bool SpnPackageManager::isSpnPackage(const QString &path)
{
    return path.endsWith(SPN_EXTENSION, Qt::CaseInsensitive) && QFile::exists(path) && QFileInfo(path).isFile();
}

QString SpnPackageManager::extractSpnToTemp(const QString &spnPath)
{
    if (!isSpnPackage(spnPath)) {
        qWarning() << "Not a valid .spn package:" << spnPath;
        return QString();
    }
    
    // Create unique temporary directory
    QString tempBaseName = TEMP_PREFIX + QFileInfo(spnPath).baseName() + "_" + 
                          QCryptographicHash::hash(spnPath.toUtf8(), QCryptographicHash::Md5).toHex().left(8);
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + tempBaseName;
    
    // Clean up existing temp dir if it exists
    if (QDir(tempDir).exists()) {
        QDir(tempDir).removeRecursively();
    }
    
    // Create temp directory
    if (!QDir().mkpath(tempDir)) {
        qWarning() << "Failed to create temp directory:" << tempDir;
        return QString();
    }
    
    // Extract .spn file to temp directory
    if (!unpackSpnToDirectory(spnPath, tempDir)) {
        QDir(tempDir).removeRecursively();
        return QString();
    }
    
    return tempDir;
}

bool SpnPackageManager::updateSpnFromTemp(const QString &spnPath, const QString &tempDir)
{
    if (!QDir(tempDir).exists()) {
        qWarning() << "Temp directory doesn't exist:" << tempDir;
        return false;
    }
    
    return packDirectoryToSpn(tempDir, spnPath);
}

bool SpnPackageManager::convertFolderToSpn(const QString &folderPath, QString &spnPath)
{
    QFileInfo folderInfo(folderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        return false;
    }
    
    // Create .spn package path
    QString baseName = folderInfo.fileName();
    QString parentDir = folderInfo.absolutePath();
    spnPath = parentDir + "/" + baseName + SPN_EXTENSION;
    
    // Check if .spn already exists
    if (QFile::exists(spnPath)) {
        qWarning() << "SPN package already exists:" << spnPath;
        return false;
    }
    
    // Pack folder into .spn file
    if (!packDirectoryToSpn(folderPath, spnPath)) {
        return false;
    }
    
    // Remove the old folder
    QDir oldDir(folderPath);
    if (!oldDir.removeRecursively()) {
        qWarning() << "Failed to remove old folder:" << folderPath;
        // Continue anyway, the .spn package is created
    }
    
    return true;
}

bool SpnPackageManager::createSpnPackage(const QString &spnPath, const QString &notebookName)
{
    if (QFile::exists(spnPath)) {
        return false; // Already exists
    }
    
    // Create a temporary directory with basic structure
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return false;
    }
    
    // Create basic .speedynote_metadata.json
    QJsonObject metadata = createSpnHeader(notebookName);
    QJsonDocument doc(metadata);
    
    QFile metaFile(tempDir.path() + "/.speedynote_metadata.json");
    if (metaFile.open(QIODevice::WriteOnly)) {
        metaFile.write(doc.toJson());
        metaFile.close();
    }
    
    // Pack the temp directory into .spn file
    return packDirectoryToSpn(tempDir.path(), spnPath);
}

QString SpnPackageManager::getSpnDisplayName(const QString &spnPath)
{
    if (!isSpnPackage(spnPath)) {
        return QFileInfo(spnPath).fileName();
    }
    
    QString baseName = QFileInfo(spnPath).baseName(); // Remove .spn extension
    return baseName;
}

bool SpnPackageManager::isValidSpnPackage(const QString &spnPath)
{
    if (!isSpnPackage(spnPath)) {
        return false;
    }
    
    // Try to extract to temp and check for SpeedyNote metadata
    QString tempDir = extractSpnToTemp(spnPath);
    if (tempDir.isEmpty()) {
        return false;
    }
    
    // Check for SpeedyNote metadata
    QString metadataFile = tempDir + "/.speedynote_metadata.json";
    QString oldIdFile = tempDir + "/.notebook_id.txt";
    
    bool isValid = QFile::exists(metadataFile) || QFile::exists(oldIdFile);
    
    // Clean up temp directory
    cleanupTempDir(tempDir);
    
    return isValid;
}

QString SpnPackageManager::getSuggestedSpnName(const QString &pdfPath)
{
    QFileInfo pdfInfo(pdfPath);
    return pdfInfo.baseName() + SPN_EXTENSION;
}

QString SpnPackageManager::getTempDirForSpn(const QString &spnPath)
{
    QString tempBaseName = TEMP_PREFIX + QFileInfo(spnPath).baseName() + "_" + 
                          QCryptographicHash::hash(spnPath.toUtf8(), QCryptographicHash::Md5).toHex().left(8);
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + tempBaseName;
}

void SpnPackageManager::cleanupTempDir(const QString &tempDir)
{
    if (!tempDir.isEmpty() && QDir(tempDir).exists()) {
        QDir(tempDir).removeRecursively();
    }
}

bool SpnPackageManager::packDirectoryToSpn(const QString &dirPath, const QString &spnPath)
{
    QFile spnFile(spnPath);
    if (!spnFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to create .spn file:" << spnPath;
        return false;
    }
    
    QDataStream stream(&spnFile);
    stream.setVersion(QDataStream::Qt_6_0);
    
    // Write header
    stream << QString("SPEEDYNOTE_PACKAGE");
    stream << quint32(1); // Version
    
    // Get all files in directory (including hidden files)
    QDir sourceDir(dirPath);
    QDirIterator it(dirPath, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    
    QStringList files;
    while (it.hasNext()) {
        QString filePath = it.next();
        QString relativePath = sourceDir.relativeFilePath(filePath);
        files.append(relativePath);
    }
    
    // Write file count
    stream << quint32(files.size());
    
    // Write each file
    for (const QString &relativePath : files) {
        QString fullPath = sourceDir.absoluteFilePath(relativePath);
        QFile file(fullPath);
        
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to read file:" << fullPath;
            continue;
        }
        
        QByteArray fileData = file.readAll();
        file.close();
        
        // Write file path and data
        stream << relativePath;
        stream << quint64(fileData.size());
        stream.writeRawData(fileData.constData(), fileData.size());
    }
    
    spnFile.close();
    return true;
}

bool SpnPackageManager::unpackSpnToDirectory(const QString &spnPath, const QString &dirPath)
{
    QFile spnFile(spnPath);
    if (!spnFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open .spn file:" << spnPath;
        return false;
    }
    
    QDataStream stream(&spnFile);
    stream.setVersion(QDataStream::Qt_6_0);
    
    // Read and verify header
    QString header;
    quint32 version;
    stream >> header >> version;
    
    if (header != "SPEEDYNOTE_PACKAGE" || version != 1) {
        qWarning() << "Invalid .spn file format:" << spnPath;
        return false;
    }
    
    // Read file count
    quint32 fileCount;
    stream >> fileCount;
    
    // Extract each file
    for (quint32 i = 0; i < fileCount; ++i) {
        QString relativePath;
        quint64 fileSize;
        stream >> relativePath >> fileSize;
        
        // Create directory structure if needed
        QString fullPath = dirPath + "/" + relativePath;
        QFileInfo fileInfo(fullPath);
        QDir().mkpath(fileInfo.absolutePath());
        
        // Read file data
        QByteArray fileData(fileSize, 0);
        stream.readRawData(fileData.data(), fileSize);
        
        // Write file
        QFile outFile(fullPath);
        if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(fileData);
            outFile.close();
        } else {
            qWarning() << "Failed to write file:" << fullPath;
        }
    }
    
    spnFile.close();
    return true;
}

QJsonObject SpnPackageManager::createSpnHeader(const QString &notebookName)
{
    QJsonObject metadata;
    metadata["notebook_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces).replace("-", "");
    metadata["version"] = "1.0";
    metadata["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metadata["last_modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    if (!notebookName.isEmpty()) {
        metadata["name"] = notebookName;
    }
    
    // Default values
    metadata["pdf_path"] = "";
    metadata["last_accessed_page"] = 0;
    metadata["background_style"] = "None";
    metadata["background_color"] = "#ffffff";
    metadata["background_density"] = 20;
    metadata["bookmarks"] = QJsonArray();
    
    return metadata;
} 