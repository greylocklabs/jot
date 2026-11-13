#pragma once

#include "KeyRouter.h"

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTimer>
#include <QVariantList>

class QTextDocument;

namespace jot {

/// Vi's normal, insert and visual modes for a TextEdit, kept basic. Moves are sent as the
/// TextEdit's own keys, so they behave as its arrows do.
class ViEditor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickItem *target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(jot::KeyRouter *router MEMBER m_router NOTIFY routerChanged)
    /// Off, every key goes to the TextEdit as usual.
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /// While true, as when notes are offered for a wikilink, insert mode leaves every key,
    /// Escape too, to the TextEdit.
    Q_PROPERTY(bool busy MEMBER m_busy NOTIFY busyChanged)
    /// "normal", "insert" or "visual".
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    /// The start of a longer key, such as "d" before "dd", or empty.
    Q_PROPERTY(QString pending READ pending NOTIFY pendingChanged)
    /// Where the cursor is, counted from 1 as vi does: the line, and the character on it.
    Q_PROPERTY(int line READ line NOTIFY placeChanged)
    Q_PROPERTY(int column READ column NOTIFY placeChanged)
    /// How far through the file the cursor's line is: 0 on the first line, 100 on the last.
    Q_PROPERTY(int percent READ percent NOTIFY placeChanged)
    /// The lines the selection touches, or 0 with nothing selected.
    Q_PROPERTY(int pickedLines READ pickedLines NOTIFY placeChanged)
    /// After f, F, t or T, where the character gone to is found further on in the same
    /// direction, to highlight while the next press of one of them steps on to it.
    Q_PROPERTY(QVariantList matches READ matches NOTIFY matchesChanged)

public:
    explicit ViEditor(QObject *parent = nullptr);

    [[nodiscard]] QQuickItem *target() const { return m_target; }
    void setTarget(QQuickItem *target);
    [[nodiscard]] bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    [[nodiscard]] QString mode() const { return m_mode; }
    void setMode(const QString &mode);
    [[nodiscard]] QString pending() const { return m_pending; }
    [[nodiscard]] int line() const;
    [[nodiscard]] int column() const;
    [[nodiscard]] int percent() const;
    [[nodiscard]] int pickedLines() const;
    [[nodiscard]] QVariantList matches() const { return m_matches; }
    /// Runs a command typed after `:`, as vim does: a line number, `$`, `w`, `q`, `wq`,
    /// `s/old/new/` on the line or `%s/old/new/g` in the whole note, or `noh`. Gives what to
    /// say in the status line, empty when there is nothing to say.
    Q_INVOKABLE QString runCommand(const QString &command);

Q_SIGNALS:
    /// Escape in normal mode: the keyboard is done with the note.
    void leaveRequested();
    /// The text from `start` to `end` was yanked, to flash it.
    void yanked(int start, int end);
    /// `:` in normal mode, or Space when `:` is the leader: a command is to be typed.
    void commandLineRequested();
    /// `:noh`: find highlights are to go.
    void highlightsCleared();
    void targetChanged();
    void routerChanged();
    void enabledChanged();
    void busyChanged();
    void modeChanged();
    void pendingChanged();
    void placeChanged();
    void matchesChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    /// Hides a find's matches once the cursor moves some other way.
    void dropMatches();

private:
    /// Handles a key in normal or visual mode; says whether it was used.
    bool key(const QKeyEvent *event);
    /// Does the action `id`; says whether it was one.
    bool run(const QString &id);
    /// Moves the cursor as the TextEdit's own keys do, selecting in visual mode.
    bool move(const QString &id);
    void sendKey(int key, Qt::KeyboardModifiers modifiers);
    void setPending(const QString &pending);
    /// Goes on through the note to `target`, as f, F, t or T (`kind`) do, or cuts or changes up
    /// to it after d or c (`op`). Repeated, t and T look past a match right beside the cursor.
    bool find(QChar op, QChar kind, QChar target, bool repeat);
    /// Shows where `target` is found further on from the cursor in `kind`'s direction, or
    /// nothing when `target` is null.
    void setMatches(QChar kind, QChar target);
    /// Ends stepping through a find's matches and hides them.
    void endSteps();
    /// Whether keys type text, so the desktop's input method should see them.
    [[nodiscard]] bool typing() const;
    void updateInputMethod();

    [[nodiscard]] QTextDocument *document() const;
    [[nodiscard]] int cursor() const;
    void setCursor(int position);
    [[nodiscard]] bool writable() const;
    /// Puts `text` where vi's yank and delete keep it, which is the clipboard.
    void keep(const QString &text, bool lines);
    void paste(bool after);

    QPointer<QQuickItem> m_target;
    KeyRouter *m_router = nullptr;
    bool m_enabled = false;
    bool m_busy = false;
    bool m_sending = false;
    QString m_mode = QStringLiteral("normal");
    QString m_pending;
    QTimer m_timeout;
    /// What dd or yy last kept, which p pastes as whole lines while the clipboard holds it.
    QString m_lines;
    /// The last f, F, t or T and its character, for ; and , to repeat.
    QChar m_findKind;
    QChar m_findTarget;
    QVariantList m_matches;
    /// After a find, until another key or move: f, F, t and T step through its matches.
    bool m_stepping = false;
    /// Moving the cursor itself, as a find does, which leaves its matches shown.
    bool m_finding = false;
};

} // namespace jot
