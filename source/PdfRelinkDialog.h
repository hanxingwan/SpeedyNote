#ifndef PDFRELINKDIALOG_H
#define PDFRELINKDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>

class PdfRelinkDialog : public QDialog
{
    Q_OBJECT

public:
    enum Result {
        Cancel,
        RelinkPdf,
        ContinueWithoutPdf
    };

    explicit PdfRelinkDialog(const QString &missingPdfPath, QWidget *parent = nullptr);
    
    Result getResult() const { return result; }
    QString getNewPdfPath() const { return newPdfPath; }

private slots:
    void onRelinkPdf();
    void onContinueWithoutPdf();
    void onCancel();

private:
    void setupUI();
    
    Result result = Cancel;
    QString originalPdfPath;
    QString newPdfPath;
};

#endif // PDFRELINKDIALOG_H 