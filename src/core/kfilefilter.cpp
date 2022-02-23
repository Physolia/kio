/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfilefilter.h"

#include <QDebug>
#include <QMetaType>
#include <QMimeDatabase>
#include <algorithm>
#include <qchar.h>

class KFileFilterPrivate : public QSharedData
{
public:
    KFileFilterPrivate()
    {
    }

    KFileFilterPrivate(const KFileFilterPrivate &other)
        : QSharedData(other)
        , m_label(other.m_label)
        , m_filePatterns(other.m_filePatterns)
        , m_mimePatterns(other.m_mimePatterns)
    {
    }

    QString m_label;
    QStringList m_filePatterns;
    QStringList m_mimePatterns;
};

QVector<KFileFilter> KFileFilter::fromFilterString(const QString &filterString)
{
    int pos = filterString.indexOf(QLatin1Char('/'));

    // Check for an un-escaped '/', if found
    // interpret as a MIME filter.

    if (pos > 0 && filterString[pos - 1] != QLatin1Char('\\')) {
        const QStringList filters = filterString.split(QLatin1Char(' '), Qt::SkipEmptyParts);

        QVector<KFileFilter> result;
        result.reserve(filters.size());

        std::transform(filters.begin(), filters.end(), std::back_inserter(result), [](const QString &mimeType) {
            return KFileFilter(mimeType);
        });

        return result;
    }

    // Strip the escape characters from
    // escaped '/' characters.

    QString escapeRemoved(filterString);
    for (pos = 0; (pos = escapeRemoved.indexOf(QLatin1String("\\/"), pos)) != -1; ++pos) {
        escapeRemoved.remove(pos, 1);
    }

    const QStringList filters = escapeRemoved.split(QLatin1Char('\n'));

    QVector<KFileFilter> result;

    for (const QString &filter : filters) {
        int separatorPos = filter.indexOf(QLatin1Char('|'));

        QString label;
        QStringList patterns;

        if (separatorPos != -1) {
            label = filter.mid(separatorPos + 1);
            patterns = filter.left(separatorPos).split(QLatin1Char(' '));
        } else {
            patterns = filter.split(QLatin1Char(' '));
        }

        result << KFileFilter(label, patterns, {});
    }

    return result;
}

KFileFilter::KFileFilter()
    : d(new KFileFilterPrivate)
{
}

KFileFilter::KFileFilter(const QString &label, const QStringList &filePatterns, const QStringList &mimePatterns)
    : d(new KFileFilterPrivate)
{
    d->m_filePatterns = filePatterns;
    d->m_mimePatterns = mimePatterns;

    if (!label.isEmpty()) {
        d->m_label = label;
    } else {
        QStringList items = d->m_filePatterns;

        QMimeDatabase db;

        for (const QString &mimeType : std::as_const(d->m_mimePatterns)) {
            items << db.mimeTypeForName(mimeType).comment();
        }

        d->m_label = items.join(QLatin1Char(' '));
    }
}

KFileFilter::KFileFilter(const QString &mimeType)
    : d(new KFileFilterPrivate)
{
    static QMimeDatabase db;
    const QMimeType type = db.mimeTypeForName(mimeType);
    d->m_label = type.comment();
    d->m_mimePatterns = QStringList{mimeType};
}

KFileFilter::~KFileFilter() = default;

KFileFilter::KFileFilter(const KFileFilter &other)
    : d(other.d)
{
}

KFileFilter &KFileFilter::operator=(const KFileFilter &other)
{
    if (this != &other) {
        d = other.d;
    }

    return *this;
}

QString KFileFilter::label() const
{
    return d->m_label;
}

QStringList KFileFilter::filePatterns() const
{
    return d->m_filePatterns;
}

QStringList KFileFilter::mimePatterns() const
{
    return d->m_mimePatterns;
}

bool KFileFilter::operator==(const KFileFilter &other) const
{
    return d->m_label == other.d->m_label && d->m_filePatterns == other.d->m_filePatterns && d->m_mimePatterns == other.d->m_mimePatterns;
}

QString KFileFilter::toFilterString() const
{
    if (!d->m_mimePatterns.isEmpty()) {
        return d->m_mimePatterns.join(QLatin1Char(' '));
    }

    if (!d->m_label.isEmpty()) {
        return d->m_filePatterns.join(QLatin1Char(' ')) + QLatin1Char('|') + d->m_label;

    } else {
        return d->m_filePatterns.join(QLatin1Char(' '));
    }
}

Q_DECLARE_METATYPE(KFileFilter);
