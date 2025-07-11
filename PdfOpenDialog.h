#ifndef PDFOPENDIALOG_H
#define PDFOPENDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QIcon>
#include <QPixmap>
#include <QFileInfo>
#include <QResizeEvent>

class PdfOpenDialog : public QDialog {
    Q_OBJECT

public:
    enum Result {
        CreateNewFolder,
        UseExistingFolder,
        Cancel
    };

    explicit PdfOpenDialog(const QString &pdfPath, QWidget *parent = nullptr);
    
    Result getResult() const { return result; }
    QString getSelectedFolder() const { return selectedFolder; }

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onCreateNewFolder();
    void onUseExistingFolder();
    void onCancel();

private:
    Result result;
    QString pdfPath;
    QString selectedFolder;
    
    void setupUI();
};

#endif // PDFOPENDIALOG_H 