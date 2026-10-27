#pragma once

#include <QKeySequence>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QVariantList>

namespace jot {

/// Something the keyboard can do, and every way to do it.
struct KeyAction
{
    QString id;
    QString label;
    /// Where it works: "window" anywhere, "list" in the note list or sidebar, "note" while
    /// writing or in vi's normal mode, "visual" with text selected in vi's visual mode.
    QString scope;
    /// Shortcuts that always work, as QKeySequence::StandardKey values or text.
    QVariantList standard;
    /// The keys for it in vi mode, such as "j", "gg" or "Esc".
    QString vi;
    /// The key after the leader in vi mode.
    QString leader;
};

/// Turns vi mode's keys into actions from one table, so the shortcuts and their help cannot
/// disagree. Text fields and popups keep their keys; the note's vi keys are ViEditor's.
class KeyRouter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool viMode MEMBER m_viMode NOTIFY settingsChanged)
    Q_PROPERTY(QString leaderKey MEMBER m_leaderKey NOTIFY settingsChanged)
    /// Milliseconds the leader waits for the next key; 0 waits until Escape.
    Q_PROPERTY(int leaderTimeout MEMBER m_leaderTimeout NOTIFY settingsChanged)
    /// "leader", or the start of a longer vi key such as "g", while waiting for the key
    /// that finishes a command; else empty.
    Q_PROPERTY(QString pending READ pending NOTIFY pendingChanged)
    Q_PROPERTY(QVariantList actions READ actionList CONSTANT)

public:
    explicit KeyRouter(QObject *parent = nullptr);

    [[nodiscard]] static QList<KeyAction> actions();
    /// actions() for QML: maps with the fields of KeyAction, plus `keys`, the standard
    /// shortcuts as the platform writes them.
    [[nodiscard]] static QVariantList actionList();
    /// The window's actions for the command palette, as actionList() gives them, best match
    /// for `query` first, leaving out those in `unavailable` and the palette itself.
    Q_INVOKABLE static QVariantList commands(const QString &query, const QStringList &unavailable);
    /// The action in `scope` whose vi keys are `keys`, or empty. `partial` says whether
    /// `keys` begins a longer one.
    [[nodiscard]] static QString viAction(const QString &scope, const QString &keys,
                                          bool *partial = nullptr);

    [[nodiscard]] QQuickWindow *window() const { return m_window; }
    void setWindow(QQuickWindow *window);
    [[nodiscard]] QString pending() const { return m_pending; }
    [[nodiscard]] bool viMode() const { return m_viMode; }
    [[nodiscard]] QString leaderKey() const { return m_leaderKey; }

    /// Feeds one typed key in vi mode. Returns whether the key was used.
    Q_INVOKABLE bool press(const QString &text);
    /// Stops waiting for the rest of a command.
    Q_INVOKABLE void cancel();

Q_SIGNALS:
    void triggered(const QString &id);
    void windowChanged();
    void settingsChanged();
    void pendingChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    [[nodiscard]] static QVariantList toList(const QList<KeyAction> &actions);
    void setPending(const QString &pending);

    QPointer<QQuickWindow> m_window;
    bool m_viMode = false;
    QString m_leaderKey = QStringLiteral(",");
    int m_leaderTimeout = 5000;
    QString m_pending;
    QTimer m_timeout;
};

} // namespace jot
