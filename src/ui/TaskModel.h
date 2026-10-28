#pragma once

#include "jot/ToDos.h"
#include "jot/VaultIndex.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QTimer>

namespace jot {

/// The to-dos Tasks shows: one view of them, Today, Upcoming, Anytime or All, with a label
/// or a tag to narrow it, under headings for the day or the note they are in.
class TaskModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Given by Vault")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    /// "today", "upcoming", "anytime" or "all".
    Q_PROPERTY(QString view READ view WRITE setView NOTIFY filterChanged)
    /// Only to-dos with this @label, when set.
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY filterChanged)
    /// Only to-dos with this #tag or one nested in it, when set.
    Q_PROPERTY(QString tag READ tag WRITE setTag NOTIFY filterChanged)
    /// How many are overdue or due today, whatever the view.
    Q_PROPERTY(int todayCount READ todayCount NOTIFY countChanged)

public:
    enum Role {
        /// "note" for a note that is a task, "task" for a checkbox in a note.
        KindRole = Qt::UserRole + 1,
        PathRole,
        /// The to-do's words, or a task note's title.
        TitleRole,
        /// The note a checkbox is in; empty for a task note.
        NoteRole,
        LineRole,
        /// When it is due, as the list says it: a time today, a day otherwise; or empty.
        DueRole,
        /// The due day is past.
        OverdueRole,
        LabelsRole,
        /// It repeats.
        RepeatsRole,
        /// The heading it is listed under: a day, or the note it is in.
        GroupRole,
    };

    explicit TaskModel(QObject *parent = nullptr);

    void reload(const VaultIndex *index);

    [[nodiscard]] QString view() const { return m_view; }
    void setView(const QString &view);
    [[nodiscard]] QString label() const { return m_label; }
    void setLabel(const QString &label);
    [[nodiscard]] QString tag() const { return m_tag; }
    void setTag(const QString &tag);
    [[nodiscard]] int todayCount() const { return m_todayCount; }

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void countChanged();
    void filterChanged();

private:
    void refill();

    const VaultIndex *m_index = nullptr;
    QString m_view = QStringLiteral("today");
    QString m_label;
    QString m_tag;
    QDate m_today;
    QList<ToDo> m_rows;
    int m_todayCount = 0;
    /// Fills the list again just after midnight, when Today becomes another day.
    QTimer m_midnight;
};

} // namespace jot
