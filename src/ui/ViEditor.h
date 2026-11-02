#pragma once

#include "KeyRouter.h"

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTimer>

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

public:
    explicit ViEditor(QObject *parent = nullptr);

    [[nodiscard]] QQuickItem *target() const { return m_target; }
    void setTarget(QQuickItem *target);
    [[nodiscard]] bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    [[nodiscard]] QString mode() const { return m_mode; }
    void setMode(const QString &mode);
    [[nodiscard]] QString pending() const { return m_pending; }

Q_SIGNALS:
    /// Escape in normal mode: the keyboard is done with the note.
    void leaveRequested();
    void targetChanged();
    void routerChanged();
    void enabledChanged();
    void busyChanged();
    void modeChanged();
    void pendingChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Handles a key in normal or visual mode; says whether it was used.
    bool key(const QKeyEvent *event);
    /// Does the action `id`; says whether it was one.
    bool run(const QString &id);
    /// Moves the cursor as the TextEdit's own keys do, selecting in visual mode.
    bool move(const QString &id);
    void sendKey(int key, Qt::KeyboardModifiers modifiers);
    void setPending(const QString &pending);
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
};

} // namespace jot
