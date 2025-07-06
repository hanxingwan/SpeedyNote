#ifndef MARKDOWNWINDOW_H
#define MARKDOWNWINDOW_H

#include <QWidget>
#include <QRect>
#include <QString>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>

class QMarkdownTextEdit;

class MarkdownWindow : public QWidget {
    Q_OBJECT

public:
    explicit MarkdownWindow(const QRect &rect, QWidget *parent = nullptr);
    ~MarkdownWindow();

    // Content management
    QString getMarkdownContent() const;
    void setMarkdownContent(const QString &content);
    
    // Position and size
    QRect getWindowRect() const;
    void setWindowRect(const QRect &rect);
    
    // Canvas coordinate support
    QRect getCanvasRect() const;
    void setCanvasRect(const QRect &canvasRect);
    void updateScreenPosition(); // Updates screen position from canvas coordinates
    
    // Serialization
    QVariantMap serialize() const;
    void deserialize(const QVariantMap &data);
    
    // Focus management
    void focusEditor();
    bool isEditorFocused() const;

signals:
    void deleteRequested(MarkdownWindow *window);
    void contentChanged();
    void windowMoved(MarkdownWindow *window);
    void windowResized(MarkdownWindow *window);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onDeleteClicked();
    void onMarkdownTextChanged();

private:
    // Resize handle detection
    enum ResizeHandle {
        None,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Top,
        Bottom,
        Left,
        Right
    };
    
    QMarkdownTextEdit *markdownEditor;
    QPushButton *deleteButton;
    QLabel *titleLabel;
    QVBoxLayout *mainLayout;
    QHBoxLayout *headerLayout;
    
    // Window dragging
    bool dragging = false;
    QPoint dragStartPosition;
    QPoint windowStartPosition;
    
    // Resizing
    bool resizing = false;
    QPoint resizeStartPosition;
    QRect resizeStartRect;
    ResizeHandle currentResizeHandle = None;
    
    // Canvas coordinate storage
    QRect canvasRect; // Position and size in canvas coordinates
    
    ResizeHandle getResizeHandle(const QPoint &pos) const;
    void updateCursor(const QPoint &pos);
    void setupUI();
    void applyStyle();
    void convertScreenToCanvasRect(const QRect &screenRect);
};

#endif // MARKDOWNWINDOW_H 