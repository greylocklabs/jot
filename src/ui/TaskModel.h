#pragma once

#include "jot/ToDos.h"
#include "jot/VaultIndex.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QTimer>

namespace jot {

/// The to-dos Tasks shows: one view of them, Today, Upcoming, Anytime, All or Done, with a tag or a
/// priority to narrow it, under headings for the day or the note they are in.
class TaskModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Given by Vault")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    /// "today", "upcoming", "anytime", "all" or "done".
    Q_PROPERTY(QString view READ view WRITE setView NOTIFY filterChanged)
    /// Only to-dos with this #tag or one nested in it, when set.
    Q_PROPERTY(QString tag READ tag WRITE setTag NOTIFY filterChanged)
    /// Only to-dos of this priority, 1 urgent to 4 low, or 0 for those with none; -1 for all.
    Q_PROPERTY(int priority READ priority WRITE setPriority NOTIFY filterChanged)
    /// How many are overdue or due today, whatever the view.
    Q_PROPERTY(int todayCount READ todayCount NOTIFY countChanged)

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        /// The to-do's words.
        TitleRole,
        /// The note it is in.
        NoteRole,
        LineRole,
        /// When it is due, as the list says it: a time today, a day otherwise; or empty.
        DueRole,
        /// The due day is past.
        OverdueRole,
        /// It repeats.
        RepeatsRole,
        /// The heading it is listed under: a day, or the note it is in.
        GroupRole,
        /// It is done.
        DoneRole,
        /// 1 urgent to 4 low, or 0 for none.
        PriorityRole,
        /// What the day popup starts from: the checkbox's words as written, markers and all.
        WordingRole,
    };

    explicit TaskModel(QObject *parent = nullptr);

    void reload(const VaultIndex *index);

    [[nodiscard]] QString view() const { return m_view; }
    void setView(const QString &view);
    [[nodiscard]] QString tag() const { return m_tag; }
    void setTag(const QString &tag);
    [[nodiscard]] int priority() const { return m_priority; }
    void setPriority(int priority);
    /// Orders open to-dos by `sort`, "due", "created", "title" or "priority", the other way
    /// round when `descending`. Only by due day do they come under day headings.
    void setSort(const QString &sort, bool descending);
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
    QString m_tag;
    int m_priority = -1;
    ToDoSort m_sort = ToDoSort::Due;
    bool m_descending = false;
    QDate m_today;
    QList<ToDo> m_rows;
    int m_todayCount = 0;
    /// Fills the list again just after midnight, when Today becomes another day.
    QTimer m_midnight;
};

} // namespace jot
