#include "VaultController.h"

#include "jot/Conflicts.h"
#include "jot/FrontMatter.h"
#include "jot/JoplinExport.h"
#include "jot/JoplinImport.h"
#include "jot/Layout.h"
#include "jot/Logging.h"
#include "jot/Merge.h"
#include "jot/NoteHistory.h"
#include "jot/NoteScan.h"
#include "jot/QuickAdd.h"
#include "jot/Rename.h"
#include "jot/Settings.h"
#include "jot/TaskLine.h"

#include <QBuffer>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QLocale>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

VaultController *s_instance = nullptr;

} // namespace

VaultController::VaultController(Settings &settings, QObject *parent)
    : QObject(parent), m_settings(settings), m_notebooks(settings), m_tagTree(settings)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(m_saveDelay);
    connect(&m_saveTimer, &QTimer::timeout, this, &VaultController::saveNow);
    connect(this, &VaultController::currentChanged, this, &VaultController::currentTitleChanged);
    // Saving changes the list's order and titles; a burst of saves reloads it once.
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(800);
    connect(&m_reloadTimer, &QTimer::timeout, this, &VaultController::reloadModels);
    m_notes.setOrder(settings.noteSort());
    connect(&m_notes, &NoteListModel::orderChanged, this,
            [this] { m_settings.setNoteSort(m_notes.order()); });
    connect(&settings, &Settings::noteSortChanged, this,
            [this] { m_notes.setOrder(m_settings.noteSort()); });
    m_tasks.setView(settings.tasksView());
    connect(&m_tasks, &TaskModel::filterChanged, this,
            [this] { m_settings.setTasksView(m_tasks.view()); });
    connect(&settings, &Settings::tasksViewChanged, this,
            [this] { m_tasks.setView(m_settings.tasksView()); });
    // Jot keeps running for days in the tray, so the trash is looked at more than at opening.
    m_expireTimer.setInterval(std::chrono::hours(6));
    connect(&m_expireTimer, &QTimer::timeout, this, &VaultController::expireTrash);
    m_expireTimer.start();
    connect(&settings, &Settings::trashChanged, this, &VaultController::expireTrash);
    connect(&settings, &Settings::trashChanged, this, &VaultController::updateTrashHits);
    connect(&m_notes, &NoteListModel::queryChanged, this, &VaultController::updateTrashHits);
    connect(&m_notes, &NoteListModel::scopeChanged, this, &VaultController::updateTrashHits);
}

VaultController::~VaultController()
{
    saveNow();
    if (s_instance == this)
        s_instance = nullptr;
}

VaultController *VaultController::instance()
{
    return s_instance;
}

void VaultController::setInstance(VaultController *controller)
{
    s_instance = controller;
}

VaultController *VaultController::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool VaultController::openLast()
{
    const QString path = m_settings.vaultPath();
    return !path.isEmpty() && QFileInfo(path).isDir() && openPath(path);
}

bool VaultController::openFolder(const QUrl &folder)
{
    return openPath(folder.toLocalFile());
}

bool VaultController::openPath(const QString &path)
{
    // Closing drops the note shown, so edits that cannot be written keep this vault open.
    saveNow();
    if (m_unsaved) {
        fail(tr("The note shown could not be saved, so the folder was not changed."));
        return false;
    }
    QString error;
    const std::optional<Vault> created = Vault::create(QDir(path).absolutePath(), &error);
    if (!created) {
        fail(error);
        return false;
    }
    auto vault = std::make_unique<Vault>(*created);
    auto index = std::make_unique<VaultIndex>(*vault, m_indexPath);
    if (!index->open(&error)) {
        fail(tr("Could not open the index for %1: %2").arg(path, error));
        return false;
    }

    close();
    m_vault = std::move(vault);
    m_index = std::move(index);
    m_vault->removeEmptyAttachmentFolders();
    m_index->refresh();
    m_watcher = std::make_unique<VaultWatcher>(*m_vault);
    connect(m_watcher.get(), &VaultWatcher::changed, this, &VaultController::notesChangedOnDisk);
    connect(m_watcher.get(), &VaultWatcher::foldersChanged, this, &VaultController::reloadModels);
    m_watcher->start();
    m_settings.setVaultPath(m_vault->root());
    qCInfo(lcVault) << "opened" << m_vault->root() << "with" << m_index->noteCount() << "notes";

    if (!m_error.isEmpty()) {
        m_error.clear();
        Q_EMIT errorChanged();
    }
    reloadModels();
    Q_EMIT openChanged();
    m_sync.setVault(m_vault->root(), [this] { saveNow(); });
    expireTrash();
    reloadTrash();
    return true;
}

void VaultController::close()
{
    saveNow();
    m_sync.setVault({}, {});
    // Clears the editor and any problem with the note, which belonged to this vault.
    m_current.clear();
    readCurrent();
    m_watcher.reset();
    m_index.reset();
    m_vault.reset();
    m_historyKept.clear();
    m_undo.clear();
    m_redo.clear();
    Q_EMIT historyChanged();
    m_trail.clear();
    Q_EMIT trailChanged();
    reloadTrash();
}

void VaultController::fail(const QString &error)
{
    qCWarning(lcVault) << "could not open the vault:" << error;
    m_error = error;
    Q_EMIT errorChanged();
}

void VaultController::notesChangedOnDisk(const QStringList &paths)
{
    // The title comes from the index, which only takes the change below, after the note is read.
    const QString title = currentTitle();
    if (paths.contains(m_current)) {
        if (m_unsaved) {
            QString error;
            if (const std::optional<QString> theirs =
                    notefile::read(m_vault->absolute(m_current), &error))
                takeOutsideEdit(*theirs);
        } else {
            readCurrent();
        }
    }
    m_index->update(paths);
    if (currentTitle() != title)
        Q_EMIT currentTitleChanged();
    reloadModels();
    // Other notes changing is what changes what links here; saving this one does not.
    m_backlinks.reload(m_vault.get(), m_index.get(), m_current);
}

void VaultController::takeOutsideEdit(const QString &theirs)
{
    const std::optional<QString> merged = mergeText(m_savedText, m_currentText, theirs);
    if (!merged) {
        keepConflict(theirs);
        return;
    }
    qCInfo(lcVault) << m_current << "changed on disk while being edited; merged the two";
    m_savedText = theirs;
    if (*merged == theirs) {
        // The file holds everything typed already.
        m_saveTimer.stop();
        m_currentText = theirs;
        setUnsaved(false);
    } else {
        m_currentText = *merged;
        m_saveTimer.start();
    }
    Q_EMIT currentChanged();
}

void VaultController::keepConflict(const QString &theirs)
{
    QString stem = Vault::titleFromPath(m_current);
    if (Vault::isDataFile(m_current))
        stem.truncate(stem.lastIndexOf(u'.'));
    const QString folder = m_current.contains(u'/') ? m_current.section(u'/', 0, -2) : QString();
    const QString title =
        tr("%1 (conflict %2)")
            .arg(stem, QDateTime::currentDateTime().toString(u"yyyy-MM-dd HHmm"_s));
    const QString path = m_vault->freePathLike(m_current, folder, title);
    QString error;
    if (notefile::write(m_vault->absolute(path), theirs, &error))
        qCWarning(lcVault) << m_current
                           << "changed on disk while being edited; kept the other "
                              "version as"
                           << path;
    else
        qCWarning(lcVault) << "could not keep the other version of" << m_current << error;
}

void VaultController::setUnsaved(bool unsaved)
{
    if (m_unsaved == unsaved)
        return;
    m_unsaved = unsaved;
    Q_EMIT unsavedChanged();
}

void VaultController::setProblem(const QString &problem)
{
    if (m_problem == problem)
        return;
    m_problem = problem;
    Q_EMIT problemChanged();
}

void VaultController::edit(const QString &edited)
{
    if (!m_vault || m_current.isEmpty())
        return;
    // The editor gives plain \n lines and drops a byte order mark; the file keeps its own.
    QString text = edited;
    text.replace(u"\r\n"_s, u"\n"_s);
    const QString before =
        m_byteOrderMark ? m_currentText.sliced(m_currentText.startsWith(QChar(0xfeff)) ? 1 : 0)
                        : m_currentText;
    text = keepLineEndings(before, text);
    if (m_byteOrderMark && !text.startsWith(QChar(0xfeff)))
        text.prepend(QChar(0xfeff));
    if (text == m_currentText)
        return;
    m_currentText = text;
    setUnsaved(true);
    m_saveTimer.start();
}

void VaultController::saveNow()
{
    m_saveTimer.stop();
    if (!m_unsaved || !m_vault || m_current.isEmpty())
        return;
    if (!m_vault->writable()) {
        qCWarning(lcVault) << "not saving" << m_current << "in a vault from a newer Jot";
        return;
    }
    keepHistory(m_current, m_savedText);
    QString error;
    if (m_watcher)
        m_watcher->expectWrite(m_current, m_currentText);
    if (!notefile::write(m_vault->absolute(m_current), m_currentText, &error)) {
        qCWarning(lcVault) << "could not save" << m_current << error;
        setProblem(tr("Not saved: %1. Trying again.").arg(error));
        // Tried again until it works, typing or not.
        m_saveTimer.start(5000);
        return;
    }
    m_saveTimer.setInterval(m_saveDelay);
    m_savedText = m_currentText;
    setUnsaved(false);
    setProblem({});
    const QString title = currentTitle();
    m_index->update({m_current});
    if (currentTitle() != title)
        Q_EMIT currentTitleChanged();
    scheduleModelReload();
}

void VaultController::keepHistory(const QString &path, const QString &before, bool now)
{
    const QDateTime time = QDateTime::currentDateTime();
    const auto kept = m_historyKept.constFind(path);
    if (!now && kept != m_historyKept.cend() && kept->msecsTo(time) < m_historyInterval)
        return;
    const QFileInfo file(m_vault->absolute(path));
    QString error;
    if (!NoteHistory(*m_vault).keep(path, before, file.exists() ? file.lastModified() : time,
                                    &error)) {
        qCWarning(lcVault) << "could not keep a version of" << path << error;
        return;
    }
    NoteHistory(*m_vault).prune(path, time);
    m_historyKept.insert(path, time);
}

void VaultController::forgetHistoryKept(const QString &path)
{
    m_historyKept.removeIf([&path](const auto &kept) {
        return kept.key() == path || kept.key().startsWith(path + u'/');
    });
}

QVariantList VaultController::historyOf(const QString &path) const
{
    QVariantList list;
    if (!m_vault)
        return list;
    const QList<NoteHistory::Version> versions = NoteHistory(*m_vault).versions(path);
    const QLocale locale;
    const QDate today = QDate::currentDate();
    // To the minute, or to the second beside a version from the same minute.
    const QString seconds = locale.timeFormat(QLocale::LongFormat).remove(u't').trimmed();
    const auto label = [&](const QDateTime &written, bool toTheSecond) {
        const QDate day = written.date();
        const QString date = day == today ? tr("Today")
                             : day == today.addDays(-1)
                                 ? tr("Yesterday")
                                 : locale.toString(day, QLocale::ShortFormat);
        const QString time = toTheSecond ? locale.toString(written.time(), seconds)
                                         : locale.toString(written.time(), QLocale::ShortFormat);
        return tr("%1, %2").arg(date, time);
    };
    for (qsizetype i = 0; i < versions.size(); ++i) {
        const QString shown = label(versions[i].written, false);
        const bool crowded =
            (i > 0 && label(versions[i - 1].written, false) == shown) ||
            (i + 1 < versions.size() && label(versions[i + 1].written, false) == shown);
        list.append(QVariantMap{{u"path"_s, versions[i].path},
                                {u"written"_s, versions[i].written},
                                {u"label"_s, crowded ? label(versions[i].written, true) : shown}});
    }
    return list;
}

QString VaultController::versionText(const QString &version) const
{
    if (!m_vault || QDir::cleanPath(version) != version ||
        !version.startsWith(Vault::historyFolderFor({})))
        return {};
    QString error;
    return notefile::read(m_vault->absolute(version), &error).value_or(QString());
}

QVariantMap VaultController::compareVersions(const QString &older, const QString &newer) const
{
    // Either side a version's path, or empty for the note as it is now.
    const auto textOf = [this](const QString &version) {
        return version.isEmpty() ? m_currentText : versionText(version);
    };
    const std::optional<QList<DiffLine>> lines = diffLines(textOf(older), textOf(newer));
    if (!lines)
        return {{u"error"_s, tr("The two are too long and different to compare.")}};
    QVariantList rows;
    int changes = 0;
    bool inChange = false;
    for (const DiffLine &line : *lines) {
        const bool same = line.kind == DiffLine::Kind::Same;
        if (!same && !inChange)
            ++changes;
        inChange = !same;
        rows.append(QVariantMap{{u"kind"_s, same                                   ? u"same"_s
                                            : line.kind == DiffLine::Kind::Removed ? u"removed"_s
                                                                                   : u"added"_s},
                                {u"text"_s, line.text}});
    }
    return {{u"lines"_s, rows}, {u"changes"_s, changes}};
}

bool VaultController::restoreVersion(const QString &version)
{
    if (!m_vault || m_current.isEmpty() || !m_vault->writable() ||
        QDir::cleanPath(version) != version ||
        !version.startsWith(Vault::historyFolderFor(m_current) + u'/'))
        return false;
    QString error;
    const std::optional<QString> text = notefile::read(m_vault->absolute(version), &error);
    if (!text) {
        qCWarning(lcVault) << "could not read" << version << error;
        return false;
    }
    return replaceCurrentText(*text);
}

bool VaultController::replaceCurrentText(const QString &text)
{
    if (!m_vault || !m_vault->writable())
        return false;
    saveNow();
    if (m_unsaved)
        return false;
    if (text == m_currentText)
        return true;
    // What is replaced becomes a version too, so this can be undone.
    keepHistory(m_current, m_currentText, true);
    const QString before = m_currentText;
    m_currentText = text;
    setUnsaved(true);
    saveNow();
    if (m_unsaved) {
        // Not written, so the note stays as it is on disk rather than retrying the restore.
        m_saveTimer.stop();
        m_currentText = before;
        setUnsaved(false);
        setProblem({});
        return false;
    }
    Q_EMIT currentChanged();
    reloadModels();
    return true;
}

QVariantList VaultController::currentConflicts() const
{
    if (!m_vault || m_current.isEmpty())
        return {};
    const QString folder = m_current.contains(u'/') ? m_current.section(u'/', 0, -2) : QString();
    QStringList siblings;
    for (const QString &name : QDir(m_vault->absolute(folder)).entryList(QDir::Files))
        siblings.append(folder.isEmpty() ? name : folder + u'/' + name);
    QVariantList copies;
    const QLocale locale;
    for (const QString &path : conflictCopiesOf(m_current, siblings)) {
        const std::optional<ConflictCopy> copy = readConflictCopy(path);
        copies.append(QVariantMap{
            {u"path"_s, path},
            {u"from"_s, copy->from},
            {u"when"_s, copy->when.isValid()
                            ? locale.toString(copy->when, u"MMMM d, "_s) +
                                  locale.toString(copy->when.time(), QLocale::ShortFormat)
                            : QString()}});
    }
    return copies;
}

QVariantMap VaultController::compareWith(const QString &copy) const
{
    if (!m_vault || readConflictCopy(copy).value_or(ConflictCopy{}).original != m_current)
        return {};
    QString error;
    const std::optional<QString> theirs = notefile::read(m_vault->absolute(copy), &error);
    if (!theirs)
        return {{u"error"_s, error}};
    const std::optional<QList<DiffLine>> lines = diffLines(m_currentText, *theirs);
    if (!lines)
        return {{u"error"_s, tr("The two are too long and different to show side by side.")}};
    // Lines taken out and put in at the same place sit side by side as one change.
    QVariantList rows;
    QStringList removed;
    QStringList added;
    const auto flush = [&] {
        for (qsizetype i = 0; i < std::max(removed.size(), added.size()); ++i) {
            const bool mine = i < removed.size();
            const bool other = i < added.size();
            rows.append(QVariantMap{{u"mine"_s, mine ? removed.at(i) : QString()},
                                    {u"theirs"_s, other ? added.at(i) : QString()},
                                    {u"change"_s, mine && other ? u"changed"_s
                                                  : mine        ? u"mine"_s
                                                                : u"theirs"_s}});
        }
        removed.clear();
        added.clear();
    };
    int differences = 0;
    for (const DiffLine &line : *lines) {
        if (line.kind == DiffLine::Kind::Same) {
            flush();
            rows.append(QVariantMap{
                {u"mine"_s, line.text}, {u"theirs"_s, line.text}, {u"change"_s, u"same"_s}});
            continue;
        }
        if (removed.isEmpty() && added.isEmpty())
            ++differences;
        (line.kind == DiffLine::Kind::Removed ? removed : added).append(line.text);
    }
    flush();
    return {{u"rows"_s, rows}, {u"differences"_s, differences}};
}

bool VaultController::keepThisVersion(const QString &copy)
{
    if (!m_vault || !m_vault->writable() ||
        readConflictCopy(copy).value_or(ConflictCopy{}).original != m_current)
        return false;
    const bool trashed = trashNote(copy, true);
    Q_EMIT currentConflictsChanged();
    return trashed;
}

bool VaultController::useOtherVersion(const QString &copy)
{
    if (!m_vault || !m_vault->writable() ||
        readConflictCopy(copy).value_or(ConflictCopy{}).original != m_current)
        return false;
    QString error;
    const std::optional<QString> theirs = notefile::read(m_vault->absolute(copy), &error);
    if (!theirs || !replaceCurrentText(*theirs))
        return false;
    trashNote(copy, true);
    Q_EMIT currentConflictsChanged();
    return true;
}

void VaultController::scheduleModelReload()
{
    m_reloadTimer.start();
}

QString VaultController::newNote(const QString &title)
{
    if (!m_vault)
        return {};
    saveNow();
    // In the notebook shown; a tag or "All notes" puts it at the top of the vault.
    const QString folder = m_notes.tag().isEmpty() ? m_notes.notebook() : QString();
    QString error;
    const std::optional<QString> path = m_vault->createNote(
        folder, title.isEmpty() ? tr("Untitled") : title, QDateTime::currentDateTimeUtc(), &error);
    if (!path) {
        qCWarning(lcVault) << "could not make a note:" << error;
        return {};
    }
    m_index->update({*path});
    reloadModels();
    setCurrent(*path);
    return *path;
}

QVariantList VaultController::dataKinds()
{
    QVariantList kinds;
    for (const auto &[kind, label] :
         {std::pair{u"json"_s, tr("JSON file")}, std::pair{u"csv"_s, tr("CSV file")},
          std::pair{u"tsv"_s, tr("TSV file")}, std::pair{u"yaml"_s, tr("YAML file")}})
        kinds << QVariantMap{{u"kind"_s, kind}, {u"label"_s, label}};
    return kinds;
}

QString VaultController::newFile(const QString &kind)
{
    if (!m_vault)
        return {};
    saveNow();
    const QString folder = m_notes.tag().isEmpty() ? m_notes.notebook() : QString();
    QString error;
    const std::optional<QString> path =
        m_vault->createDataFile(folder, tr("Untitled"), kind, &error);
    if (!path) {
        qCWarning(lcVault) << "could not make a file:" << error;
        setProblem(tr("Could not make a file: %1").arg(error));
        return {};
    }
    m_index->update({*path});
    reloadModels();
    setCurrent(*path);
    return *path;
}

bool VaultController::renameCurrent(const QString &title)
{
    const QString wanted = title.trimmed();
    if (!m_vault || m_current.isEmpty() || wanted.isEmpty() || wanted == currentTitle())
        return false;
    saveNow();
    const QString folder = m_current.contains(u'/') ? m_current.section(u'/', 0, -2) : QString();

    // A note with nothing in it yet, named "people.csv", becomes a CSV file. One with words in
    // it stays a note, so nothing it holds is lost.
    const QString typed = QFileInfo(wanted).suffix().toLower();
    if (!Vault::isDataFile(m_current) && Vault::kDataSuffixes.contains(typed)) {
        const NoteParts parts = splitFrontMatter(m_currentText);
        // Read line by line, as a comment or list the front matter reader skips is still kept.
        const QStringList lines = parts.frontMatter.toString().split(u'\n');
        const bool onlyMade = std::ranges::all_of(lines, [](const QString &line) {
            const QString kept = line.trimmed();
            return kept.isEmpty() || kept.startsWith(u"created:"_s) || kept.startsWith(u"title:"_s);
        });
        if (parts.body.trimmed().isEmpty() && onlyMade) {
            const QString base = Vault::fileNameFor(wanted.chopped(typed.size() + 1).trimmed());
            const QString to =
                m_vault->freePath(folder, base.isEmpty() ? tr("Untitled") : base, u'.' + typed);
            QString error;
            const auto expect = [this](const QString &path, const QString &text) {
                if (m_watcher)
                    m_watcher->expectWrite(path, text);
            };
            forgetHistoryKept(m_current);
            if (!renameNote(*m_vault, *m_index, m_current, to, &error, expect)) {
                qCWarning(lcVault) << "could not rename" << m_current << error;
                resyncAfterRename(m_current, to);
                setProblem(tr("Could not rename the note: %1").arg(error));
                return false;
            }
            m_trail.moved(m_current, to);
            m_current = to;
            readCurrent();
            m_currentText = Vault::starterData(typed);
            // The note it was is not an earlier version of the data file.
            m_savedText.clear();
            setUnsaved(true);
            saveNow();
            m_index->update({m_current});
            reloadModels();
            Q_EMIT currentChanged();
            return true;
        }
    }

    // A data file keeps its kind: a name typed without the extension gets it back.
    const bool data = Vault::isDataFile(m_current);
    const QString suffix = data ? u'.' + QFileInfo(m_current).suffix() : u".md"_s;
    QString name = Vault::fileNameFor(wanted);
    if (data && name.endsWith(suffix, Qt::CaseInsensitive))
        name.chop(suffix.size());
    QString to = (folder.isEmpty() ? QString() : folder + u'/') + name + suffix;
    if (to.compare(m_current, Qt::CaseSensitive) != 0 && QFileInfo::exists(m_vault->absolute(to)))
        to = m_vault->freePathLike(m_current, folder, data ? name : wanted);

    QString error;
    if (to != m_current) {
        const auto expect = [this](const QString &path, const QString &text) {
            if (m_watcher)
                m_watcher->expectWrite(path, text);
        };
        forgetHistoryKept(m_current);
        if (!renameNote(*m_vault, *m_index, m_current, to, &error, expect)) {
            qCWarning(lcVault) << "could not rename" << m_current << error;
            resyncAfterRename(m_current, to);
            setProblem(tr("Could not rename the note: %1").arg(error));
            return false;
        }
        m_trail.moved(m_current, to);
        m_current = to;
        readCurrent();
    }
    // The title key holds what the file name cannot; when the name can, it goes. A data file
    // has no front matter, so its name is all it has.
    const QVariantMap meta = readFrontMatter(splitFrontMatter(m_currentText).frontMatter);
    const bool fits = Vault::titleFromPath(to) == wanted;
    if (!data && (!fits || meta.contains(u"title"_s))) {
        const QString text =
            setFrontMatterKey(m_currentText, u"title"_s, fits ? QVariant() : QVariant(wanted));
        if (text != m_currentText) {
            m_currentText = text;
            setUnsaved(true);
            saveNow();
            Q_EMIT currentChanged();
        }
    }
    m_index->update({m_current});
    reloadModels();
    return true;
}

QString VaultController::newNotebook(const QString &parent)
{
    if (!m_vault)
        return {};
    QString error;
    const std::optional<QString> path = m_vault->createFolder(parent, tr("New notebook"), &error);
    if (!path) {
        qCWarning(lcVault) << "could not make a notebook:" << error;
        setProblem(tr("Could not make a notebook: %1").arg(error));
        return {};
    }
    // Its parent opens, so the new notebook shows.
    QStringList collapsed = m_settings.collapsedNotebooks();
    if (collapsed.removeOne(parent))
        m_settings.setCollapsedNotebooks(collapsed);
    if (m_watcher)
        m_watcher->start();
    reloadModels();
    return *path;
}

bool VaultController::setPinned(const QString &path, bool pinned)
{
    if (!m_vault || !m_vault->writable() || path.isEmpty())
        return false;
    QString error;
    std::optional<Layout> layout = Layout::read(*m_vault, &error);
    if (layout)
        layout->setPinned(path, pinned);
    if (!layout || !layout->write(*m_vault, &error)) {
        qCWarning(lcVault) << "could not pin" << path << error;
        setProblem(tr("Could not pin the note: %1").arg(error));
        return false;
    }
    reloadModels();
    return true;
}

bool VaultController::setNotebookEmoji(const QString &path, const QString &emoji)
{
    if (!m_vault || !m_vault->writable() || path.isEmpty())
        return false;
    QString error;
    std::optional<Layout> layout = Layout::read(*m_vault, &error);
    if (layout)
        layout->setEmoji(path, emoji);
    if (!layout || !layout->write(*m_vault, &error)) {
        qCWarning(lcVault) << "could not set the emoji of" << path << error;
        setProblem(tr("Could not change the notebook's emoji: %1").arg(error));
        return false;
    }
    reloadModels();
    return true;
}

bool VaultController::setViewEmoji(const QString &view, const QString &emoji)
{
    if (!m_vault || !m_vault->writable() || !Layout::kViews.contains(view))
        return false;
    QString error;
    std::optional<Layout> layout = Layout::read(*m_vault, &error);
    if (layout)
        layout->setViewEmoji(view, emoji);
    if (!layout || !layout->write(*m_vault, &error)) {
        qCWarning(lcVault) << "could not set the emoji of" << view << error;
        setProblem(tr("Could not change the emoji: %1").arg(error));
        return false;
    }
    reloadModels();
    return true;
}

bool VaultController::renameNotebook(const QString &path, const QString &name)
{
    const QString base = Vault::fileNameFor(name.trimmed()).remove(u'/');
    if (base.isEmpty() || base.startsWith(u'.'))
        return false;
    const QString parent = path.contains(u'/') ? path.section(u'/', 0, -2) : QString();
    return relocateNotebook(path, parent.isEmpty() ? base : parent + u'/' + base);
}

bool VaultController::moveNotebook(const QString &path, const QString &parent)
{
    // Where it is already, or into itself, is nothing to do rather than a problem.
    const QString current = path.contains(u'/') ? path.section(u'/', 0, -2) : QString();
    if (parent == current || parent == path || parent.startsWith(path + u'/'))
        return false;
    const QString name = path.section(u'/', -1);
    return relocateNotebook(path, parent.isEmpty() ? name : parent + u'/' + name);
}

bool VaultController::relocateNotebook(const QString &from, const QString &to)
{
    if (!m_vault || !m_index || from.isEmpty() || to == from)
        return false;
    saveNow();
    QString error;
    const auto expect = [this](const QString &path, const QString &text) {
        if (m_watcher)
            m_watcher->expectWrite(path, text);
    };
    forgetHistoryKept(from);
    const std::optional<QStringList> written =
        jot::moveNotebook(*m_vault, *m_index, from, to, &error, expect);
    if (!written) {
        qCWarning(lcVault) << "could not move" << from << "to" << to << error;
        // A failure after the folder moved leaves it moved, so everything follows it there
        // and the index is read again, rather than the editor saving to where it was.
        const bool movedAnyway =
            !QFileInfo::exists(m_vault->absolute(from)) && QFileInfo::exists(m_vault->absolute(to));
        if (movedAnyway) {
            m_index->refresh();
            followNotebook(from, to, {});
        } else {
            if (m_watcher)
                m_watcher->start();
            reloadModels();
        }
        // After following, which reads the note shown again and so clears any problem.
        setProblem(tr("Could not move the notebook: %1").arg(error));
        return false;
    }
    followNotebook(from, to, *written);
    return true;
}

void VaultController::followNotebook(const QString &from, const QString &to,
                                     const QStringList &written)
{
    // Whatever named a path inside it now names the same place in its new spot.
    const auto follow = [&](const QString &path) {
        return path == from || path.startsWith(from + u'/') ? to + path.mid(from.size()) : path;
    };
    QStringList collapsed;
    for (const QString &folded : m_settings.collapsedNotebooks())
        collapsed.append(follow(folded));
    collapsed.sort();
    m_settings.setCollapsedNotebooks(collapsed);
    m_notes.setNotebook(follow(m_notes.notebook()));
    m_trail.moved(from, to);
    if (m_watcher)
        m_watcher->start();
    afterMove(follow(m_current), written);
}

bool VaultController::moveNoteTo(const QString &path, const QString &notebook)
{
    if (!m_vault || !m_index || path.isEmpty())
        return false;
    const QString folder = path.contains(u'/') ? path.section(u'/', 0, -2) : QString();
    if (folder == notebook)
        return false;
    saveNow();
    const QString to = m_vault->freePathLike(path, notebook, Vault::titleFromPath(path));
    QString error;
    const auto expect = [this](const QString &note, const QString &text) {
        if (m_watcher)
            m_watcher->expectWrite(note, text);
    };
    forgetHistoryKept(path);
    const std::optional<QStringList> written =
        renameNote(*m_vault, *m_index, path, to, &error, expect);
    if (!written) {
        qCWarning(lcVault) << "could not move" << path << "to" << notebook << error;
        resyncAfterRename(path, to);
        setProblem(tr("Could not move the note: %1").arg(error));
        return false;
    }
    m_trail.moved(path, to);
    afterMove(m_current == path ? to : m_current, *written);
    return true;
}

void VaultController::afterMove(const QString &current, const QStringList &written)
{
    if (current != m_current || written.contains(current)) {
        m_current = current;
        readCurrent();
    }
    reloadModels();
}

QString VaultController::pasteImage()
{
    if (!m_vault || m_current.isEmpty())
        return {};
    const QImage image = QGuiApplication::clipboard()->image();
    if (image.isNull())
        return {};
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    const QString name =
        tr("Pasted %1").arg(QDateTime::currentDateTime().toString(u"yyyy-MM-dd HHmmss"_s)) +
        u".png"_s;
    QString error;
    const std::optional<QString> path = m_vault->addAttachment(m_current, name, png, &error);
    if (!path) {
        setProblem(tr("Could not keep the pasted image: %1").arg(error));
        return {};
    }
    return u"![](%1)"_s.arg(Vault::linkTarget(m_current, *path));
}

QString VaultController::attachFiles(const QList<QUrl> &files)
{
    if (!m_vault || m_current.isEmpty())
        return {};
    QStringList lines;
    QStringList refused;
    for (const QUrl &url : files) {
        QString error;
        const std::optional<QString> path =
            url.isLocalFile() ? m_vault->copyAttachment(m_current, url.toLocalFile(), &error)
                              : std::nullopt;
        if (!path) {
            refused.append(
                url.isLocalFile()
                    ? error
                    : tr("%1 is not a file on this computer").arg(url.toDisplayString()));
            continue;
        }
        const bool image = !QImageReader::imageFormat(m_vault->absolute(*path)).isEmpty();
        lines.append((image ? u"![](%1)"_s : u"[%2](%1)"_s)
                         .arg(Vault::linkTarget(m_current, *path), QFileInfo(*path).fileName()));
    }
    if (!refused.isEmpty())
        setProblem(tr("Not attached: %1").arg(refused.join(u"; "_s)));
    return lines.join(u'\n');
}

QVariantMap VaultController::placeBlock(const QString &text, int position,
                                        const QString &markdown) const
{
    position = std::clamp(position, 0, int(text.size()));
    const QString before = position > 0 && text.at(position - 1) != u'\n' ? u"\n"_s : QString();
    const QString after =
        position < text.size() && text.at(position) == u'\n' ? QString() : u"\n"_s;
    const QString inserted = before + markdown + after;
    return {{u"position"_s, position},
            {u"text"_s, inserted},
            {u"cursor"_s, position + int(inserted.size())}};
}

QString VaultController::scheduledFromWords(const QString &text) const
{
    return jot::scheduledFromWords(text, QDateTime::currentDateTime()).value_or(QString());
}

QVariantMap VaultController::taskWordsAt(const QString &text, int position) const
{
    const std::optional<TaskWords> at = jot::taskWordsAt(text, position);
    if (!at)
        return {};
    return {{u"start"_s, int(at->start)},
            {u"end"_s, int(at->end)},
            {u"words"_s, text.sliced(at->start, at->end - at->start)}};
}

QString VaultController::scheduledAs(const QString &text, const QString &when) const
{
    return jot::scheduledAs(text, when, QDateTime::currentDateTime()).value_or(QString());
}

QString VaultController::scheduleWords(const QString &text) const
{
    const TaskLine line = readTaskLine(text);
    return QStringList{QString(line.due).replace(u'T', u' '), line.repeat}
        .filter(QRegularExpression(u"\\S"_s))
        .join(u' ');
}

QVariantMap VaultController::readQuickTask(const QString &typed) const
{
    const QuickTask task = jot::readQuickTask(typed, QDateTime::currentDateTime());
    const QLocale locale;
    QString when;
    if (!task.due.isEmpty()) {
        const QDate day = QDate::fromString(task.due.left(10), Qt::ISODate);
        const QDate today = QDate::currentDate();
        when = day == today              ? tr("Today")
               : day == today.addDays(1) ? tr("Tomorrow")
                                         : locale.toString(day, u"dddd, MMMM d"_s);
        const QTime time = QTime::fromString(task.due.mid(11, 5), u"HH:mm"_s);
        if (time.isValid())
            when += u", "_s + locale.toString(time, QLocale::ShortFormat);
    }
    return {{u"title"_s, task.title},
            {u"due"_s, task.due},
            {u"when"_s, when},
            {u"repeat"_s, task.repeat},
            {u"labels"_s, readTaskLine(task.line).labels},
            {u"line"_s, task.line}};
}

QVariantMap VaultController::tickedLineAt(const QString &text, int position) const
{
    const TickEdit edit = jot::tickedLineAt(text, position, QDate::currentDate());
    return {{u"start"_s, int(edit.start)}, {u"end"_s, int(edit.end)}, {u"text"_s, edit.text}};
}

bool VaultController::addQuickTask(const QString &typed)
{
    if (!m_vault || !m_vault->writable())
        return false;
    const QuickTask task = jot::readQuickTask(typed, QDateTime::currentDateTime());
    if (task.title.isEmpty())
        return false;
    const QString path = inboxPath(*m_vault);
    if (QFileInfo::exists(m_vault->absolute(path))) {
        const bool added = rewriteNote(path, [&task](const QString &note) {
            return std::optional(withTaskAdded(note, task.line));
        });
        if (!added)
            setProblem(tr("Could not add the to-do to %1.").arg(path));
        return added;
    }
    QString error;
    const QString file = m_vault->absolute(path);
    if (!QDir().mkpath(QFileInfo(file).path()) ||
        !notefile::write(file, withTaskAdded({}, task.line), &error)) {
        qCWarning(lcVault) << "could not add a to-do to" << path << error;
        setProblem(tr("Could not add the to-do: %1").arg(error));
        return false;
    }
    if (m_watcher)
        m_watcher->start();
    m_index->update({path});
    reloadModels();
    return true;
}

int VaultController::movedCursor(const QString &shown, int cursor) const
{
    // The editor has plain \n lines and no byte order mark, which the note's text may have.
    QString now = m_currentText;
    if (now.startsWith(QChar(0xfeff)))
        now.remove(0, 1);
    now.replace(u"\r\n"_s, u"\n"_s);
    return movedPosition(shown, now, cursor);
}

QString VaultController::attachmentPath(const QString &target, bool wiki) const
{
    if (!m_vault || !m_index || m_current.isEmpty())
        return {};
    // A path written in a Markdown link names the file outright, so the disk answers it.
    if (!wiki && !target.contains(u':')) {
        const QString folder =
            m_current.contains(u'/') ? m_current.section(u'/', 0, -2) : QString();
        const QString decoded = QUrl::fromPercentEncoding(target.section(u'#', 0, 0).toUtf8());
        const QString path = QDir::cleanPath(folder.isEmpty() ? decoded : folder + u'/' + decoded);
        if (!path.startsWith(u"../"_s) && !Vault::isListed(path) &&
            QFileInfo(m_vault->absolute(path)).isFile())
            return m_vault->absolute(path);
    }
    const QString path = m_index->resolve(target, m_current);
    // Notes and data files open in Jot; anything else is an attachment.
    if (path.isEmpty() || Vault::isListed(path) || !(wiki || !target.contains(u':')))
        return {};
    const QString file = m_vault->absolute(path);
    return QFileInfo(file).isFile() ? file : QString();
}

QVariantMap VaultController::preview(const QString &target) const
{
    if (!m_index || m_current.isEmpty())
        return {};
    const QString path = m_index->resolve(target, m_current);
    const std::optional<NoteInfo> info = Vault::isListed(path) ? m_index->note(path) : std::nullopt;
    if (!info)
        return {};
    QString error;
    const QString text = notefile::read(m_vault->absolute(path), &error).value_or(QString());
    // Enough to recognize the note by, cut at a line so no Markdown is left half open.
    QString body =
        Vault::isDataFile(path) ? text.trimmed() : splitFrontMatter(text).body.toString().trimmed();
    constexpr qsizetype kLength = 600;
    if (body.size() > kLength) {
        const qsizetype cut = body.lastIndexOf(u'\n', kLength);
        body = body.left(cut > kLength / 2 ? cut : kLength) + u"\n..."_s;
    }
    return {{u"path"_s, path}, {u"title"_s, info->title}, {u"text"_s, body}};
}

void VaultController::setShowingTasks(bool showing)
{
    if (m_showingTasks == showing)
        return;
    m_showingTasks = showing;
    Q_EMIT showingTasksChanged();
    if (showing)
        setShowingTrash(false);
}

void VaultController::setShowingTrash(bool showing)
{
    if (showing)
        reloadTrash();
    if (m_showingTrash == showing)
        return;
    m_showingTrash = showing;
    Q_EMIT showingTrashChanged();
    if (showing)
        setShowingTasks(false);
}

void VaultController::reloadTrash()
{
    QList<TrashedNote> trashed = m_vault ? VaultTrash(*m_vault).list() : QList<TrashedNote>();
    // Read once here, keeping what was read before, so searching never reads the files.
    VaultTrash::readText(trashed, m_trashed);
    m_trashed = trashed;
    m_trash.setItems(m_trashed);
    if (!m_trash.contains(m_chosenTrashed))
        setChosenTrashed({});
    updateTrashHits();
}

void VaultController::updateTrashHits()
{
    // Within the notebook searched, as the notes found are; a tag's search has none there.
    const QString query = m_notes.query().trimmed();
    QList<TrashedNote> scope;
    if (m_settings.searchTrash() && !query.isEmpty() && m_notes.tag().isEmpty()) {
        const QString notebook = m_notes.notebook();
        for (const TrashedNote &note : std::as_const(m_trashed)) {
            if (notebook.isEmpty() || note.path.startsWith(notebook + u'/'))
                scope << note;
        }
    }
    m_trashHits.setItems(scope, query);
}

void VaultController::setChosenTrashed(const QString &location)
{
    if (m_chosenTrashed == location)
        return;
    m_chosenTrashed = location;
    Q_EMIT chosenTrashedChanged();
}

void VaultController::showTrashed(const QString &location)
{
    setShowingTrash(true);
    setChosenTrashed(m_trash.contains(location) ? location : QString());
}

QString VaultController::restoreTrashed(const QString &location)
{
    bool gone = false;
    const std::optional<QString> path = bringBack(location, &gone);
    if (!path)
        return {};
    setShowingTrash(false);
    setCurrent(*path);
    Change change;
    change.kind = Change::Kind::Trash;
    change.path = *path;
    change.trashed = false;
    remember(change);
    return *path;
}

std::optional<QString> VaultController::bringBack(const QString &location, bool *gone)
{
    const std::optional<TrashedNote> item = m_trash.item(location);
    *gone = !item;
    if (!m_vault || !m_index || !item) {
        if (*gone)
            setProblem(tr("That note is not in the trash any more."));
        return std::nullopt;
    }
    QString error;
    const std::optional<QString> path = VaultTrash(*m_vault).restore(*item, &error);
    if (!path) {
        qCWarning(lcVault) << "could not restore" << item->path << error;
        setProblem(tr("Could not bring the note back: %1").arg(error));
        reloadTrash();
        return std::nullopt;
    }
    m_index->update({*path});
    reloadModels();
    reloadTrash();
    // Back, though without what it says here.
    if (!error.isEmpty())
        setProblem(error);
    return path;
}

bool VaultController::deleteTrashed(const QString &location)
{
    const std::optional<TrashedNote> item = m_trash.item(location);
    if (!m_vault || !item)
        return false;
    QString error;
    const bool deleted = VaultTrash(*m_vault).remove(*item, &error);
    if (!deleted) {
        qCWarning(lcVault) << "could not delete" << item->path << error;
        setProblem(tr("Could not delete the note: %1").arg(error));
    }
    reloadTrash();
    return deleted;
}

bool VaultController::emptyTrash()
{
    if (!m_vault)
        return false;
    QString error;
    const bool emptied = VaultTrash(*m_vault).empty(&error);
    if (!emptied) {
        qCWarning(lcVault) << "could not empty the trash" << error;
        setProblem(tr("Could not empty the trash: %1").arg(error));
    }
    reloadTrash();
    return emptied;
}

void VaultController::expireTrash()
{
    if (!m_vault || m_settings.trashDays() <= 0)
        return;
    const int moved = VaultTrash(*m_vault).expire(QDateTime::currentDateTime(),
                                                  m_settings.trashDays(), m_discard);
    if (moved > 0) {
        qCInfo(lcVault) << "moved" << moved << "notes from the trash to the desktop's";
        reloadTrash();
    }
}

QString VaultController::trashedText(const QString &location) const
{
    if (!m_trash.item(location))
        return {};
    QString error;
    return notefile::read(location, &error).value_or(QString());
}

QString VaultController::trashedTitle(const QString &location) const
{
    const std::optional<TrashedNote> item = m_trash.item(location);
    return item ? Vault::titleFromPath(item->path) : QString();
}

bool VaultController::rewriteNote(
    const QString &path, const std::function<std::optional<QString>(const QString &)> &change,
    bool undoable)
{
    if (!m_vault || !m_vault->writable())
        return false;
    const auto record = [this, &path, undoable](const QString &before, const QString &after) {
        if (!undoable || before == after)
            return;
        Change text;
        text.path = path;
        text.before = before;
        text.after = after;
        remember(text);
    };
    if (path == m_current) {
        const std::optional<QString> changed = change(m_currentText);
        if (!changed)
            return false;
        const QString before = m_currentText;
        m_currentText = *changed;
        setUnsaved(true);
        saveNow();
        // The smallest edit that makes the change, in the editor's text, which has plain \n lines.
        const auto shown = [](QString text) { return text.replace(u"\r\n"_s, u"\n"_s); };
        const QString from = shown(before);
        const QString to = shown(*changed);
        qsizetype start = 0;
        while (start < from.size() && start < to.size() && from[start] == to[start])
            ++start;
        qsizetype end = 0;
        while (end < from.size() - start && end < to.size() - start &&
               from[from.size() - 1 - end] == to[to.size() - 1 - end])
            ++end;
        Q_EMIT currentPatched(int(start), int(from.size() - start - end),
                              to.mid(start, to.size() - start - end));
        Q_EMIT currentChanged();
        reloadModels();
        record(before, *changed);
        return true;
    }
    QString error;
    const std::optional<QString> text = notefile::read(m_vault->absolute(path), &error);
    const std::optional<QString> changed = text ? change(*text) : std::nullopt;
    if (!changed)
        return false;
    keepHistory(path, *text);
    if (m_watcher)
        m_watcher->expectWrite(path, *changed);
    if (!notefile::write(m_vault->absolute(path), *changed, &error)) {
        qCWarning(lcVault) << "could not change" << path << error;
        return false;
    }
    m_index->update({path});
    reloadModels();
    record(*text, *changed);
    return true;
}

bool VaultController::setTaskDone(const QString &path, int line, bool done, const QString &task)
{
    return rewriteNote(
        path,
        [line, done, task](const QString &text) {
            return jot::setTaskDone(text, line, done, task);
        },
        true);
}

bool VaultController::completeTask(const QString &path)
{
    return rewriteNote(
        path,
        [](const QString &text) {
            QString changed = setFrontMatterKey(text, u"task"_s, u"done"_s);
            changed = setFrontMatterKey(changed, u"done"_s,
                                        QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            return std::optional(changed);
        },
        true);
}

bool VaultController::reopenTask(const QString &path)
{
    return rewriteNote(
        path,
        [](const QString &text) {
            return std::optional(setFrontMatterKey(setFrontMatterKey(text, u"task"_s, u"open"_s),
                                                   u"done"_s, QVariant()));
        },
        true);
}

bool VaultController::undo()
{
    if (m_undo.isEmpty())
        return false;
    Change change = m_undo.takeLast();
    bool stale = false;
    const bool done = apply(change, true, &stale);
    if (done)
        m_redo.append(change);
    else if (!stale)
        m_undo.append(change);
    Q_EMIT historyChanged();
    return done;
}

bool VaultController::redo()
{
    if (m_redo.isEmpty())
        return false;
    Change change = m_redo.takeLast();
    bool stale = false;
    const bool done = apply(change, false, &stale);
    if (done)
        m_undo.append(change);
    else if (!stale)
        m_redo.append(change);
    Q_EMIT historyChanged();
    return done;
}

namespace {

// Whether undoing (`back`) or redoing `trashed` would move the note to the trash.
bool trashes(bool trashed, bool back)
{
    return trashed != back;
}

} // namespace

QString VaultController::undoTrashes() const
{
    if (m_undo.isEmpty() || m_undo.last().kind != Change::Kind::Trash ||
        !trashes(m_undo.last().trashed, true))
        return {};
    return Vault::titleFromPath(m_undo.last().path);
}

QString VaultController::redoTrashes() const
{
    if (m_redo.isEmpty() || m_redo.last().kind != Change::Kind::Trash ||
        !trashes(m_redo.last().trashed, false))
        return {};
    return Vault::titleFromPath(m_redo.last().path);
}

bool VaultController::apply(Change &change, bool back, bool *stale)
{
    if (change.kind == Change::Kind::Text) {
        return back ? replaceTaskText(change.path, change.after, change.before, stale)
                    : replaceTaskText(change.path, change.before, change.after, stale);
    }
    if (trashes(change.trashed, back)) {
        // Gone or moved since, it is not this change's to trash.
        *stale = !m_vault || !QFileInfo::exists(m_vault->absolute(change.path));
        if (*stale) {
            setProblem(tr("%1 has moved or gone since, so it was left as it is.")
                           .arg(Vault::titleFromPath(change.path)));
            return false;
        }
        const std::optional<QString> location =
            moveNoteToTrash(change.path, change.keepAttachments);
        if (location)
            change.location = *location;
        return location.has_value();
    }
    const std::optional<QString> path = bringBack(change.location, stale);
    if (path)
        change.path = *path;
    return path.has_value();
}

void VaultController::remember(const Change &change)
{
    m_undo.append(change);
    m_redo.clear();
    Q_EMIT historyChanged();
}

bool VaultController::replaceTaskText(const QString &path, const QString &from, const QString &to,
                                      bool *stale)
{
    // A note that cannot be read, as when it was renamed or deleted, has changed for good.
    *stale = true;
    const bool done = rewriteNote(path, [&from, &to, stale](const QString &text) {
        *stale = text != from;
        return *stale ? std::nullopt : std::optional(to);
    });
    if (done)
        return true;
    setProblem(*stale ? tr("That note has changed since, so the to-do was left as it is.")
                      : tr("Could not change %1.").arg(path));
    return false;
}

void VaultController::openAt(const QString &path, int line)
{
    setCurrent(path);
    if (line <= 0)
        return;
    QString text = m_currentText;
    text.remove(u'\r');
    if (text.startsWith(QChar(0xfeff)))
        text.remove(0, 1);
    Q_EMIT revealPosition(int(lineStart(text, line)));
}

void VaultController::resyncAfterRename(const QString &from, const QString &to)
{
    if (!QFileInfo::exists(m_vault->absolute(from)) && QFileInfo::exists(m_vault->absolute(to))) {
        m_trail.moved(from, to);
        m_current = to;
        readCurrent();
    }
    m_index->update({from, to});
    reloadModels();
}

bool VaultController::trashCurrent()
{
    return trashNote(m_current, false);
}

QStringList VaultController::notesUsingAttachments(const QString &path) const
{
    if (!m_index || path.isEmpty())
        return {};
    QStringList notes = m_index->sourcesLinkingInto(Vault::assetsFolderFor(path));
    notes.removeAll(path);
    notes.sort();
    return notes;
}

bool VaultController::trashNote(const QString &path, bool keepAttachments)
{
    const std::optional<QString> location = moveNoteToTrash(path, keepAttachments);
    if (!location)
        return false;
    Change change;
    change.kind = Change::Kind::Trash;
    change.path = path;
    change.location = *location;
    change.keepAttachments = keepAttachments;
    remember(change);
    return true;
}

std::optional<QString> VaultController::moveNoteToTrash(const QString &path, bool keepAttachments)
{
    if (!m_vault || !m_index || path.isEmpty())
        return std::nullopt;
    if (path == m_current)
        saveNow();
    QString error;
    QString location;
    if (!m_vault->trashNote(path, &error, keepAttachments, &location)) {
        qCWarning(lcVault) << "could not move" << path << "to the trash:" << error;
        setProblem(tr("Could not move the note to the trash: %1").arg(error));
        return std::nullopt;
    }
    forgetHistoryKept(path);
    m_index->update({path});
    if (path == m_current)
        setCurrent({});
    reloadModels();
    reloadTrash();
    return location;
}

bool VaultController::trashNotebook(const QString &path)
{
    if (!m_vault || !m_index || path.isEmpty())
        return false;
    const auto inside = [&path](const QString &other) {
        return other == path || other.startsWith(path + u'/');
    };
    if (inside(m_current))
        saveNow();
    QStringList notes;
    for (const NoteInfo &note : m_index->notesIn(path))
        notes.append(note.path);
    QString error;
    if (!m_vault->trashFolder(path, &error)) {
        qCWarning(lcVault) << "could not move" << path << "to the trash:" << error;
        setProblem(tr("Could not move the notebook to the trash: %1").arg(error));
        return false;
    }
    forgetHistoryKept(path);
    m_index->update(notes);
    // Nothing goes on naming what is gone: the note shown, the list's notebook, the folds.
    if (inside(m_current))
        setCurrent({});
    if (inside(m_notes.notebook()))
        m_notes.setNotebook({});
    QStringList collapsed = m_settings.collapsedNotebooks();
    collapsed.removeIf(inside);
    m_settings.setCollapsedNotebooks(collapsed);
    if (m_watcher)
        m_watcher->start();
    reloadModels();
    reloadTrash();
    return true;
}

int VaultController::notesInNotebook(const QString &path) const
{
    return m_index ? int(m_index->notesIn(path).size()) : 0;
}

bool VaultController::setTask(const QString &path, bool task)
{
    // Only the task key: a due date is the user's own, and comes back if it is a task again.
    return rewriteNote(path, [task](const QString &text) {
        return std::optional(
            setFrontMatterKey(text, u"task"_s, task ? QVariant(u"open"_s) : QVariant()));
    });
}

QString VaultController::titleOf(const QString &path) const
{
    const std::optional<NoteInfo> info = m_index ? m_index->note(path) : std::nullopt;
    return info ? info->title : Vault::titleFromPath(path);
}

bool VaultController::isTask(const QString &path) const
{
    const std::optional<NoteInfo> info = m_index ? m_index->note(path) : std::nullopt;
    return info && !info->task.isEmpty();
}

QString VaultController::linkTo(const QString &path) const
{
    if (!m_index || path.isEmpty())
        return {};
    // Wikilinks find notes by file name, so a name two notes share needs the path.
    const QString name = Vault::titleFromPath(path);
    const qsizetype sharing =
        std::ranges::count_if(m_index->notesIn({}), [&](const NoteInfo &note) {
            return Vault::titleFromPath(note.path).compare(name, Qt::CaseInsensitive) == 0;
        });
    return u"[["_s + Vault::wikilinkName(path, sharing > 1) + u"]]"_s;
}

void VaultController::showInFiles(const QString &path) const
{
    if (!m_vault)
        return;
    const QString folder = Vault::isListed(path) ? Vault::notebookOf(path) : path;
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_vault->absolute(folder)));
}

QStringList VaultController::notebookPaths() const
{
    QStringList paths;
    if (m_vault) {
        for (const VaultEntry &entry : m_vault->entries()) {
            if (entry.folder)
                paths.append(entry.path);
        }
    }
    return paths;
}

bool VaultController::follow(const QString &target, bool wiki)
{
    if (!m_vault)
        return false;
    static const QRegularExpression scheme(u"^[A-Za-z][A-Za-z0-9+.-]*:"_s);
    if (!wiki && scheme.match(target).hasMatch())
        return QDesktopServices::openUrl(QUrl(target));
    const QString path = m_index->resolve(target, m_current);
    if (Vault::isListed(path)) {
        setCurrent(path);
        return true;
    }
    if (!path.isEmpty())
        return QDesktopServices::openUrl(QUrl::fromLocalFile(m_vault->absolute(path)));
    if (!wiki)
        return false;
    // A wikilink to nothing yet makes the note, beside the one that links to it.
    const QString name = target.section(u'#', 0, 0).section(u'/', -1).trimmed();
    if (name.isEmpty())
        return false;
    const QString folder = m_current.contains(u'/') ? m_current.section(u'/', 0, -2) : QString();
    saveNow();
    QString error;
    const std::optional<QString> created =
        m_vault->createNote(folder, name, QDateTime::currentDateTimeUtc(), &error);
    if (!created)
        return false;
    m_index->update({*created});
    reloadModels();
    setCurrent(*created);
    return true;
}

QUrl VaultController::joplinProfile() const
{
    const QString database = joplinProfileDatabase();
    return database.isEmpty() ? QUrl() : QUrl::fromLocalFile(database);
}

QVariantMap VaultController::importJoplin(const QUrl &file, const QString &folder, bool dryRun)
{
    const auto failed = [](const QString &why) {
        return QVariantMap{{u"ok"_s, false}, {u"error"_s, why}};
    };
    if (!m_vault)
        return failed(tr("No notes folder is open."));
    QString error;
    const std::optional<JoplinExport> from = readJoplin(file.toLocalFile(), &error);
    if (!from)
        return failed(error);
    // What is typed so far is saved first, so the import cannot pass it by; if it cannot be
    // saved, nothing is imported, as reading the note again afterwards would lose it.
    if (!dryRun) {
        saveNow();
        if (m_unsaved)
            return failed(tr("Could not save the note being edited, so nothing was imported."));
    }
    const std::optional<JoplinImport> done =
        jot::importJoplin(*m_vault, *from, folder, dryRun, &error);
    if (!dryRun) {
        // Even a failed import may have written some notes, which the lists should show.
        m_index->refresh();
        if (m_watcher)
            m_watcher->start();
        reloadModels();
    }
    // The watcher was restarted, so it will not report what the import wrote, even one that
    // failed partway: the note shown is read again if its file changed, or the next keystroke
    // would save over the import. What was typed was saved first, so nothing is lost.
    if (!dryRun && !m_current.isEmpty()) {
        QString ignored;
        if (notefile::read(m_vault->absolute(m_current), &ignored).value_or(m_currentText) !=
            m_currentText)
            readCurrent();
    }
    if (!done) {
        qCWarning(lcVault) << "could not import" << file.toLocalFile() << error;
        return failed(error);
    }
    QStringList keptEdits;
    QStringList notBroughtBack;
    for (const auto &note : done->notes) {
        if (note.outcome == JoplinImport::Outcome::ChangedInJot)
            keptEdits.append(note.path);
        else if (note.outcome == JoplinImport::Outcome::Removed)
            notBroughtBack.append(note.path);
    }
    const QString local = file.toLocalFile();
    const QString source = local == joplinProfileDatabase()
                               ? tr("From the Joplin app on this computer")
                               : tr("From %1").arg(QFileInfo(local).fileName());
    return {{u"ok"_s, true},
            {u"source"_s, source},
            {u"created"_s, done->count(JoplinImport::Outcome::Created)},
            {u"updated"_s, done->count(JoplinImport::Outcome::Updated)},
            {u"unchanged"_s, done->count(JoplinImport::Outcome::Unchanged)},
            {u"notebooks"_s, int(done->notebooks.size())},
            {u"attachments"_s, done->attachments},
            {u"versions"_s, done->versions},
            {u"keptEdits"_s, keptEdits},
            {u"notBroughtBack"_s, notBroughtBack},
            {u"skipped"_s, done->skipped}};
}

void VaultController::reloadModels()
{
    // Whatever changed the lists may have changed the files, so a sync follows, and a conflict
    // copy may have come or gone.
    m_sync.localChanged();
    Q_EMIT currentConflictsChanged();
    m_notebookEmoji.clear();
    m_pinned.clear();
    if (m_vault) {
        QString error;
        const std::optional<Layout> layout = Layout::read(*m_vault, &error);
        if (!layout)
            qCWarning(lcVault) << error;
        const QHash<QString, QString> emojis =
            layout ? layout->emojis() : QHash<QString, QString>();
        for (auto it = emojis.cbegin(); it != emojis.cend(); ++it)
            m_notebookEmoji.insert(it.key(), it.value());
        m_pinned = layout ? layout->pinned() : QStringList();
        for (const QString &view : Layout::kViews)
            m_viewEmoji.insert(view, layout ? layout->viewEmoji(view) : Layout::defaultEmoji(view));
    }
    if (m_index) {
        m_notebooks.reload(m_vault.get(), m_index->noteCounts(), m_index->noteCount());
        m_tagTree.reload(m_index->tagCounts());
        m_notes.reload(m_index.get(), m_pinned);
        m_switcher.reload(m_index.get());
        m_linkSuggestions.reload(m_index.get());
        m_tasks.reload(m_index.get());
    } else {
        m_notebooks.reload(nullptr, {}, 0);
        m_tagTree.reload({});
        m_notes.reload(nullptr);
        m_switcher.reload(nullptr);
        m_linkSuggestions.reload(nullptr);
        m_backlinks.reload(nullptr, nullptr, {});
        m_tasks.reload(nullptr);
    }
    Q_EMIT notesChanged();
}

QString VaultController::root() const
{
    return m_vault ? m_vault->root() : QString();
}

QString VaultController::name() const
{
    return m_vault ? QFileInfo(m_vault->root()).fileName() : QString();
}

bool VaultController::writable() const
{
    return m_vault && m_vault->writable();
}

int VaultController::noteCount() const
{
    return m_index ? m_index->noteCount() : 0;
}

QVariantList VaultController::labels() const
{
    QVariantList labels;
    if (m_index) {
        for (const TagCount &each : m_index->labels())
            labels.append(QVariantMap{{u"label"_s, each.tag}, {u"count"_s, each.notes}});
    }
    return labels;
}

QStringList VaultController::tags() const
{
    return m_index ? m_index->tags() : QStringList();
}

void VaultController::setCurrent(const QString &path)
{
    if (m_current == path)
        return;
    saveNow();
    if (!m_travelling) {
        m_trail.visit(path, m_cursorPosition);
        m_openingCursor = 0;
        Q_EMIT trailChanged();
    }
    m_current = path;
    // Its attachments may have been deleted elsewhere since; an empty folder for them goes.
    if (m_vault && !path.isEmpty())
        m_vault->removeEmptyAttachmentFolder(path);
    Q_EMIT opening();
    readCurrent();
}

bool VaultController::goBack()
{
    return travel(-1);
}

bool VaultController::goForward()
{
    return travel(1);
}

bool VaultController::travel(int by)
{
    if (!m_vault)
        return false;
    const auto exists = [this](const QString &path) {
        return QFileInfo::exists(m_vault->absolute(path));
    };
    const std::optional<NoteTrail::Visit> to =
        by < 0 ? m_trail.back(m_cursorPosition, exists) : m_trail.forward(m_cursorPosition, exists);
    Q_EMIT trailChanged();
    if (!to)
        return false;
    m_openingCursor = to->cursor;
    m_travelling = true;
    setCurrent(to->path);
    m_travelling = false;
    return true;
}

QString VaultController::currentKind() const
{
    if (!Vault::isDataFile(m_current))
        return u"markdown"_s;
    const QString suffix = QFileInfo(m_current).suffix().toLower();
    return suffix == u"yml" ? u"yaml"_s : suffix;
}

QString VaultController::currentTitle() const
{
    if (!m_index || m_current.isEmpty())
        return {};
    const std::optional<NoteInfo> info = m_index->note(m_current);
    return info ? info->title : Vault::titleFromPath(m_current);
}

void VaultController::readCurrent()
{
    m_saveTimer.stop();
    m_saveTimer.setInterval(m_saveDelay);
    setUnsaved(false);
    setProblem({});
    m_currentText.clear();
    if (m_vault && !m_current.isEmpty()) {
        QString error;
        if (const std::optional<QString> text =
                notefile::read(m_vault->absolute(m_current), &error))
            m_currentText = *text;
        else
            setProblem(tr("Could not read the note: %1").arg(error));
    }
    m_backlinks.reload(m_vault.get(), m_index.get(), m_current);
    m_savedText = m_currentText;
    m_byteOrderMark = m_currentText.startsWith(QChar(0xfeff));
    Q_EMIT currentChanged();
    Q_EMIT currentConflictsChanged();
}

} // namespace jot
