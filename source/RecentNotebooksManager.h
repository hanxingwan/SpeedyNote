#ifndef RECENTNOTEBOOKSMANAGER_H
#define RECENTNOTEBOOKSMANAGER_H

#include <QStringList>
#include <QSettings>
#include <QObject>

class InkCanvas; // Forward declaration

class RecentNotebooksManager : public QObject {
    Q_OBJECT

public:
    explicit RecentNotebooksManager(QObject *parent = nullptr);

    void addRecentNotebook(const QString& folderPath, InkCanvas* canvasForPreview = nullptr);
    void removeRecentNotebook(const QString& folderPath);
    QStringList getRecentNotebooks() const;
    QString getCoverImagePathForNotebook(const QString& folderPath) const;
    QString getNotebookDisplayName(const QString& folderPath) const;
    void generateAndSaveCoverPreview(const QString& folderPath, InkCanvas* optionalCanvas = nullptr);
    
    // Starred notebooks functionality
    void addStarred(const QString& folderPath);
    void removeStarred(const QString& folderPath);
    bool isStarred(const QString& folderPath) const;
    QStringList getStarredNotebooks() const;

private:
    void loadRecentNotebooks();
    void saveRecentNotebooks();
    void loadStarredNotebooks();
    void saveStarredNotebooks();
    QString getCoverImageDir() const;
    QString sanitizeFolderName(const QString& folderPath) const;
    QString getPdfPathFromNotebook(const QString& folderPath) const;
    QString getNotebookIdFromPath(const QString& folderPath) const;

    QStringList recentNotebookPaths;
    QStringList starredNotebookPaths;
    const int MAX_RECENT_NOTEBOOKS = 16;
    QSettings settings;
};

#endif // RECENTNOTEBOOKSMANAGER_H 