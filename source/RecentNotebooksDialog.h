#ifndef RECENTNOTEBOOKSDIALOG_H
#define RECENTNOTEBOOKSDIALOG_H

#include <QDialog>
#include <QGridLayout>
#include "RecentNotebooksManager.h"

class QPushButton;
class QLabel;
class MainWindow; // Forward declaration

class RecentNotebooksDialog : public QDialog {
    Q_OBJECT

public:
    explicit RecentNotebooksDialog(MainWindow *mainWindow, RecentNotebooksManager *manager, QWidget *parent = nullptr);

private slots:
    void onNotebookClicked();

private:
    void setupUi();
    void populateGrid();

    RecentNotebooksManager *notebookManager;
    QGridLayout *gridLayout;
    MainWindow *mainWindowRef; // To call selectFolder-like functionality
};

#endif // RECENTNOTEBOOKSDIALOG_H 