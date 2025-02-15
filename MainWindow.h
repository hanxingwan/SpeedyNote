#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "InkCanvas.h"
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QLineEdit>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QListWidget>
#include <QStackedWidget>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow();
    

private slots:
    void toggleBenchmark();
    void updateBenchmarkDisplay();
    void applyCustomColor(); // Added function for custom color input
    void updateThickness(int value); // New function for thickness control
    void changeTool(int index);
    void selectFolder(); // Select save folder
    void saveCanvas(); // Save canvas to file
    void saveAnnotated();
    void deleteCurrentPage();

    void loadPdf();
    void clearPdf();

    void saveCurrentPage();
    void switchPage(int pageNumber);
    void selectBackground();

    void updateZoom();
    void applyZoom();
    void updatePanRange();
    void updatePanX(int value);
    void updatePanY(int value);


    void forceUIRefresh();


    void switchTab(int index);
    void addNewTab();
    void removeTabAt(int index);
    void updateTabLabel();

private:
    InkCanvas *canvas;
    InkCanvas* currentCanvas();
    QPushButton *benchmarkButton;
    QLabel *benchmarkLabel;
    QTimer *benchmarkTimer;
    bool benchmarking;
    QPushButton *redButton;
    QPushButton *blueButton;
    QPushButton *yellowButton;
    QPushButton *blackButton;
    QPushButton *whiteButton;
    QLineEdit *customColorInput;
    QSlider *thicknessSlider; // Added thickness slider
    QComboBox *toolSelector;
    QPushButton *deletePageButton;
    QPushButton *selectFolderButton; // Button to select folder
    QPushButton *saveButton; // Button to save file
    QPushButton *saveAnnotatedButton;

    QPushButton *loadPdfButton;
    QPushButton *clearPdfButton;
    int currentPage;
    QSpinBox *pageInput;
    QPushButton *backgroundButton; // New button to set background

    QSlider *zoomSlider;
    QPushButton *dezoomButton;
    QPushButton *zoom50Button;
    QPushButton *zoom200Button;
    QLineEdit *zoomInput;
    QSlider *panXSlider;
    QSlider *panYSlider;


    QListWidget *tabList;          // Sidebar for tabs
    QStackedWidget *canvasStack;   // Holds multiple InkCanvas instances
    QPushButton *addTabButton;     // Button to add tabs
    

    void setupUi();

    
};

#endif // MAINWINDOW_H