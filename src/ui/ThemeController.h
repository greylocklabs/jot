#pragma once

#include "greylock/ThemeSpec.h"

#include <QColor>
#include <QEasingCurve>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantList>

class QFileSystemWatcher;

namespace jot {

/// The `Theme` singleton QML reads every visual value from. Theme-driven values
/// come from the active theme file and change together; spacing, type sizes and
/// metrics are constants, because they are product decisions.
class ThemeController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Theme)
    QML_SINGLETON

    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY changed)
    /// Every color token as `{name, color}`, for the gallery.
    Q_PROPERTY(QVariantList swatches READ swatches NOTIFY changed)

    Q_PROPERTY(QColor bg READ bg NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor surfaceAlt READ surfaceAlt NOTIFY changed)
    Q_PROPERTY(QColor hairline READ hairline NOTIFY changed)
    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(QColor text READ text NOTIFY changed)
    Q_PROPERTY(QColor textMuted READ textMuted NOTIFY changed)
    Q_PROPERTY(QColor textFaint READ textFaint NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor accentText READ accentText NOTIFY changed)
    Q_PROPERTY(QColor danger READ danger NOTIFY changed)
    Q_PROPERTY(QColor edge READ edge NOTIFY changed)
    Q_PROPERTY(QColor accentEdge READ accentEdge NOTIFY changed)

    Q_PROPERTY(int radiusSm READ radiusSm NOTIFY changed)
    Q_PROPERTY(int radiusMd READ radiusMd NOTIFY changed)
    Q_PROPERTY(int radiusLg READ radiusLg NOTIFY changed)
    Q_PROPERTY(int radiusXl READ radiusXl NOTIFY changed)
    Q_PROPERTY(int stickerEdge READ stickerEdge NOTIFY changed)

    /// The shadow under menus, popovers and tooltips; `shadowColor` carries its opacity.
    Q_PROPERTY(QColor shadowColor READ shadowColor NOTIFY changed)
    Q_PROPERTY(int shadowBlur READ shadowBlur NOTIFY changed)
    Q_PROPERTY(int shadowOffset READ shadowOffset NOTIFY changed)

    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY changed)
    Q_PROPERTY(QString displayFontFamily READ displayFontFamily NOTIFY changed)
    Q_PROPERTY(QString monoFontFamily READ monoFontFamily NOTIFY changed)
    /// The font notes are written in: the theme's [editor] family, or its usual one.
    Q_PROPERTY(QString editorFontFamily READ editorFontFamily NOTIFY changed)
    /// Font weights for strong, bold and heavy text.
    Q_PROPERTY(int weightStrong READ weightStrong NOTIFY changed)
    Q_PROPERTY(int weightBold READ weightBold NOTIFY changed)
    Q_PROPERTY(int weightHeavy READ weightHeavy NOTIFY changed)

    Q_PROPERTY(int durFast READ durFast NOTIFY changed)
    Q_PROPERTY(int durMed READ durMed NOTIFY changed)
    Q_PROPERTY(int durSlow READ durSlow NOTIFY changed)
    /// For color and opacity changes, which should never overshoot.
    Q_PROPERTY(int easing READ easing CONSTANT)
    /// For things arriving or settling, with `bounce` as the overshoot.
    Q_PROPERTY(int easingBounce READ easingBounce NOTIFY changed)
    Q_PROPERTY(qreal bounce READ bounce NOTIFY changed)

    Q_PROPERTY(int space1 READ space1 CONSTANT)
    Q_PROPERTY(int space2 READ space2 CONSTANT)
    Q_PROPERTY(int space3 READ space3 CONSTANT)
    Q_PROPERTY(int space4 READ space4 CONSTANT)
    Q_PROPERTY(int space5 READ space5 CONSTANT)
    Q_PROPERTY(int space6 READ space6 CONSTANT)
    Q_PROPERTY(int space7 READ space7 CONSTANT)

    Q_PROPERTY(int textXs READ textXs CONSTANT)
    Q_PROPERTY(int textSm READ textSm CONSTANT)
    Q_PROPERTY(int textMd READ textMd CONSTANT)
    Q_PROPERTY(int textBase READ textBase CONSTANT)
    Q_PROPERTY(int textLg READ textLg CONSTANT)
    Q_PROPERTY(int textXl READ textXl CONSTANT)
    Q_PROPERTY(int textDisplay READ textDisplay CONSTANT)
    Q_PROPERTY(int text2xl READ text2xl CONSTANT)

    /// A destructive button armed for its second click: filled with danger,
    /// on an edge of it, with ink that reads on it.
    Q_PROPERTY(QColor dangerEdge READ dangerEdge NOTIFY changed)
    Q_PROPERTY(QColor dangerText READ dangerText NOTIFY changed)

    /// How long the leader key waits before showing what can follow it, in ms.
    Q_PROPERTY(int whichKeyDelay READ whichKeyDelay CONSTANT)
    /// How faint a control that cannot be used yet is drawn.
    Q_PROPERTY(qreal fadedOpacity READ fadedOpacity CONSTANT)
    /// Vi's block cursor, faint enough to read the character under it.
    Q_PROPERTY(qreal blockCursorOpacity READ blockCursorOpacity CONSTANT)
    /// The widest a search box opens, on a large window.
    Q_PROPERTY(int searchWidth READ searchWidth CONSTANT)
    /// The title bar, which is also the toolbar.
    Q_PROPERTY(int titleBarHeight READ titleBarHeight CONSTANT)
    /// The grab strip along a frameless window's edges. Anything scrollable at
    /// an edge keeps its handle clear of it.
    Q_PROPERTY(int resizeBorder READ resizeBorder CONSTANT)
    /// How far the logo sits above centre, so its face, not its binding, lines up
    /// with the title beside it.
    Q_PROPERTY(int logoLift READ logoLift CONSTANT)
    /// A lighter line for glyphs with several strokes, or beside text.
    Q_PROPERTY(qreal fineGlyphStroke READ fineGlyphStroke CONSTANT)
    Q_PROPERTY(int sidebarWidth READ sidebarWidth CONSTANT)
    /// The middle column of notes, between the sidebar and the note.
    Q_PROPERTY(int noteListWidth READ noteListWidth CONSTANT)
    /// The narrowest the note gets, however wide the columns beside it were dragged.
    Q_PROPERTY(int editorMinWidth READ editorMinWidth CONSTANT)
    /// The widest a note's text runs, so lines stay easy to read on a wide window.
    Q_PROPERTY(int editorWidth READ editorWidth CONSTANT)
    /// The tallest an image shows inline in a note.
    Q_PROPERTY(int imageHeight READ imageHeight CONSTANT)
    /// How far each notebook steps in from the one holding it.
    Q_PROPERTY(int treeIndent READ treeIndent CONSTANT)
    /// A row in a list such as the sidebar's notebooks.
    Q_PROPERTY(int listRowHeight READ listRowHeight CONSTANT)
    /// A two-way switch such as the format row's Formatted and Source.
    Q_PROPERTY(int switchHeight READ switchHeight CONSTANT)
    /// The dot on a button that says something is waiting, such as an update.
    Q_PROPERTY(int badgeSize READ badgeSize CONSTANT)
    /// The small line icons beside list text, and their stroke.
    Q_PROPERTY(int smallGlyphSize READ smallGlyphSize CONSTANT)
    Q_PROPERTY(qreal smallGlyphStroke READ smallGlyphStroke CONSTANT)

public:
    /// The process-wide instance, which QML also receives.
    static ThemeController *instance();
    static ThemeController *create(QQmlEngine *, QJSEngine *);

    /// Applies an installed theme by id, or a theme file by path, and watches a
    /// file for edits. On failure the current theme stays and errors are returned.
    QStringList load(const QString &idOrPath);

    [[nodiscard]] const greylock::ThemeSpec &spec() const { return m_spec; }

    Q_INVOKABLE QColor tint(const QColor &color, qreal alpha) const;
    /// The font notes are written in for a Settings choice: "mono" is the theme's mono
    /// family, "serif" its display one, and anything else its editor one. A theme that
    /// leaves a family empty gets the system's monospace or serif font for it.
    Q_INVOKABLE QString noteFontFamily(const QString &choice) const;
    Q_INVOKABLE double contrast(const QColor &a, const QColor &b) const;

    QVariantList swatches() const;

    QString name() const { return m_spec.name; }
    bool dark() const { return m_spec.dark; }
    QStringList warnings() const { return m_warnings; }

    QColor bg() const { return m_spec.colors.background; }
    QColor surface() const { return m_spec.colors.surface; }
    QColor surfaceAlt() const { return m_spec.colors.surfaceAlt; }
    QColor hairline() const { return m_spec.colors.hairline; }
    QColor border() const { return m_spec.colors.border; }
    QColor text() const { return m_spec.colors.text; }
    QColor textMuted() const { return m_spec.colors.textMuted; }
    QColor textFaint() const { return m_spec.colors.textFaint; }
    QColor accent() const { return m_spec.colors.accent; }
    QColor accentText() const { return m_spec.colors.accentText; }
    QColor danger() const { return m_spec.colors.danger; }
    QColor edge() const { return m_spec.colors.edge; }
    QColor accentEdge() const { return m_spec.colors.accentEdge; }

    int radiusSm() const { return m_spec.shape.radiusSmall; }
    int radiusMd() const { return m_spec.shape.radius; }
    int radiusLg() const { return m_spec.shape.radiusLarge; }
    int radiusXl() const { return m_spec.shape.radiusXLarge; }
    int stickerEdge() const { return m_spec.shape.stickerEdge; }

    QColor shadowColor() const;
    int shadowBlur() const { return m_spec.shadow.blur; }
    int shadowOffset() const { return m_spec.shadow.offset; }

    QString fontFamily() const { return m_spec.type.family; }
    QString displayFontFamily() const { return m_spec.type.displayFamily; }
    QString monoFontFamily() const { return m_spec.type.monoFamily; }
    QString editorFontFamily() const
    {
        return m_spec.editor.family.isEmpty() ? m_spec.type.family : m_spec.editor.family;
    }
    int weightStrong() const { return m_spec.type.weightStrong; }
    int weightBold() const { return m_spec.type.weightBold; }
    int weightHeavy() const { return m_spec.type.weightHeavy; }

    int durFast() const { return m_spec.motion.durationFast; }
    int durMed() const { return m_spec.motion.duration; }
    int durSlow() const { return m_spec.motion.durationSlow; }
    int easing() const { return QEasingCurve::OutCubic; }
    int easingBounce() const;
    qreal bounce() const { return m_spec.motion.bounce; }

    int space1() const { return 2; }
    int space2() const { return 4; }
    int space3() const { return 8; }
    int space4() const { return 12; }
    int space5() const { return 16; }
    int space6() const { return 24; }
    int space7() const { return 32; }

    int textXs() const { return 11; }
    int textSm() const { return 12; }
    int textMd() const { return 13; }
    int textBase() const { return 14; }
    int textLg() const { return 15; }
    int textXl() const { return 20; }
    int textDisplay() const { return 22; }
    int text2xl() const { return 28; }

    QColor dangerEdge() const;
    QColor dangerText() const;

    int whichKeyDelay() const { return 300; }
    qreal fadedOpacity() const { return 0.5; }
    qreal blockCursorOpacity() const { return 0.45; }
    int searchWidth() const { return 560; }
    int titleBarHeight() const { return 58; }
    int resizeBorder() const { return 6; }
    qreal fineGlyphStroke() const { return 1.6; }
    int logoLift() const { return 1; }
    int sidebarWidth() const { return 240; }
    int noteListWidth() const { return 320; }
    int editorMinWidth() const { return 240; }
    int treeIndent() const { return 14; }
    int editorWidth() const { return 760; }
    int imageHeight() const { return 360; }
    int listRowHeight() const { return 28; }
    int switchHeight() const { return 26; }
    int badgeSize() const { return 10; }
    int smallGlyphSize() const { return 10; }
    qreal smallGlyphStroke() const { return 1.6; }

Q_SIGNALS:
    void changed();

private:
    explicit ThemeController(QObject *parent = nullptr);
    void reloadWatchedFile();

    greylock::ThemeSpec m_spec;
    QStringList m_warnings;
    QString m_watchedPath;
    QFileSystemWatcher *m_watcher;
};

} // namespace jot
