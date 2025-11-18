#ifndef MARKDOWNNOTESSIDEBAR_H
#define MARKDOWNNOTESSIDEBAR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QList>
#include <QString>
#include "MarkdownNoteEntry.h"

class MarkdownNotesSidebar : public QWidget {
    Q_OBJECT

public:
    explicit MarkdownNotesSidebar(QWidget *parent = nullptr);
    ~MarkdownNotesSidebar();
    
    // Note management
    void addNote(const MarkdownNoteData &data);
    void removeNote(const QString &noteId);
    void updateNote(const MarkdownNoteData &data);
    void clearNotes();
    
    // Load notes for specific page(s)
    void loadNotesForPages(const QList<MarkdownNoteData> &notes);
    
    // Get all current notes
    QList<MarkdownNoteData> getAllNotes() const;
    
    // Find note by ID
    MarkdownNoteEntry* findNoteEntry(const QString &noteId);

signals:
    void noteContentChanged(const QString &noteId, const MarkdownNoteData &data);
    void noteDeleted(const QString &noteId);
    void highlightLinkClicked(const QString &highlightId);

private slots:
    void onNoteContentChanged(const QString &noteId);
    void onNoteDeleted(const QString &noteId);
    void onHighlightLinkClicked(const QString &highlightId);

private:
    void setupUI();
    void applyStyle();
    
    QVBoxLayout *mainLayout;
    QScrollArea *scrollArea;
    QWidget *scrollContent;
    QVBoxLayout *scrollLayout;
    QLabel *emptyLabel;
    
    QList<MarkdownNoteEntry*> noteEntries;
    bool isDarkMode = false;
};

#endif // MARKDOWNNOTESSIDEBAR_H

