#pragma once

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

// Joplin databases made for tests, so no binary fixtures sit in the repo. Each table has only
// some of the columns Joplin's has, as an older or newer Joplin's would differ, and the reader
// must manage with what is there.

using namespace Qt::StringLiterals;

/// Makes a Joplin database at `path` with its tables, empty, then runs `rows`, SQL inserts.
/// Gives the first error, or empty when all went in.
inline QString makeJoplinDatabase(const QString &path, const QStringList &rows)
{
    QString error;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"joplin-builder"_s);
        db.setDatabaseName(path);
        if (!db.open())
            return db.lastError().text();
        QSqlQuery query(db);
        const QStringList tables = {
            u"CREATE TABLE notes (id TEXT, parent_id TEXT DEFAULT '', title TEXT DEFAULT '', "
            "body TEXT DEFAULT '', user_created_time INT DEFAULT 0, user_updated_time INT "
            "DEFAULT 0, is_todo INT DEFAULT 0, todo_due INT DEFAULT 0, todo_completed INT DEFAULT "
            "0, is_conflict INT DEFAULT 0, latitude NUMERIC DEFAULT 0, author TEXT DEFAULT '', "
            "source_url TEXT DEFAULT '', markup_language INT DEFAULT 1, encryption_applied INT "
            "DEFAULT 0, deleted_time INT DEFAULT 0)"_s,
            u"CREATE TABLE folders (id TEXT, parent_id TEXT DEFAULT '', title TEXT DEFAULT '', "
            "icon TEXT DEFAULT '', encryption_applied INT DEFAULT 0, deleted_time INT DEFAULT 0)"_s,
            u"CREATE TABLE tags (id TEXT, title TEXT DEFAULT '')"_s,
            u"CREATE TABLE note_tags (id TEXT, note_id TEXT, tag_id TEXT)"_s,
            u"CREATE TABLE resources (id TEXT, title TEXT DEFAULT '', mime TEXT DEFAULT '', "
            "filename TEXT DEFAULT '', file_extension TEXT DEFAULT '', encryption_applied INT "
            "DEFAULT 0, encryption_blob_encrypted INT DEFAULT 0)"_s,
            u"CREATE TABLE revisions (id TEXT, parent_id TEXT DEFAULT '', item_type INT DEFAULT 1, "
            "item_id TEXT, item_updated_time INT, title_diff TEXT DEFAULT '', body_diff TEXT "
            "DEFAULT '', metadata_diff TEXT DEFAULT '', encryption_applied INT DEFAULT 0)"_s,
        };
        for (const QString &statement : tables + rows) {
            if (!query.exec(statement)) {
                error = statement + u": "_s + query.lastError().text();
                break;
            }
        }
    }
    QSqlDatabase::removeDatabase(u"joplin-builder"_s);
    return error;
}
