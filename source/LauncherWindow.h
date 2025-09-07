#ifndef LAUNCHERWINDOW_H
#define LAUNCHERWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QWidget>
#include <QFrame>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QListWidget>
#include <QStackedWidget>

class MainWindow;
class RecentNotebooksManager;
class InkCanvas;

class LauncherWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LauncherWindow(QWidget *parent = nullptr);
    ~LauncherWindow();

private slots:
    void onTabChanged(int index);
    void onNewNotebookClicked();
    void onOpenPdfClicked();
    void onOpenNotebookClicked();
    void onRecentNotebookClicked();
    void onStarredNotebookClicked();
    void onNotebookRightClicked(const QPoint &pos);
    void refreshRecentNotebooks();
    void refreshStarredNotebooks();

private:
    void setupUi();
    void setupReturnTab();
    void setupNewTab();
    void setupOpenPdfTab();
    void setupOpenNotebookTab();
    void setupRecentTab();
    void setupStarredTab();
    void populateRecentGrid();
    void populateStarredGrid();
    QPushButton* createNotebookButton(const QString &path, bool isStarred = false);
    void openNotebook(const QString &path);
    void toggleStarredStatus(const QString &path);
    void removeFromRecent(const QString &path);
    QString getModernButtonStyle();
    QString getTabStyle();
    void applyModernStyling();
    MainWindow* findExistingMainWindow();
    void preserveWindowState(QWidget* targetWindow, bool isExistingWindow = false);
    QIcon loadThemedIcon(const QString& baseName);
    bool isDarkMode() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

    // UI Components
    QWidget *centralWidget;
    QSplitter *mainSplitter;
    QListWidget *tabList;
    QWidget *contentArea;
    QStackedWidget *contentStack;
    
    // Tab content widgets
    QWidget *returnTab;
    QWidget *newTab;
    QWidget *openPdfTab;
    QWidget *openNotebookTab;
    QWidget *recentTab;
    QWidget *starredTab;
    
    // Recent and Starred grids
    QScrollArea *recentScrollArea;
    QScrollArea *starredScrollArea;
    QWidget *recentGridWidget;
    QWidget *starredGridWidget;
    QGridLayout *recentGridLayout;
    QGridLayout *starredGridLayout;
    
    // Layout optimization
    int lastCalculatedWidth;
    
    // Managers
    RecentNotebooksManager *notebookManager;
    
    // Current right-clicked notebook path
    QString rightClickedPath;
    
    static const int GRID_COLUMNS = 4;
    static const int BUTTON_SIZE = 200;
    static const int COVER_HEIGHT = 150;
};

#endif // LAUNCHERWINDOW_H
