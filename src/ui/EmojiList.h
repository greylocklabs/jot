#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

namespace jot {

/// The emoji the notebook emoji picker offers, found by their Unicode names.
class EmojiList : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Emoji)
    QML_SINGLETON

public:
    using QObject::QObject;

    /// `{emoji, name}` for each emoji whose name has a word starting with each word of
    /// `query`, in the list's order; every emoji for an empty query.
    Q_INVOKABLE static QVariantList matching(const QString &query);
};

} // namespace jot
