/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: Stephan Kulow <coolo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfilefiltercombo.h"
#include "kfilefilter.h"
#include "kfilefiltercombo_debug.h"

#include <KLocalizedString>
#include <QDebug>
#include <QEvent>
#include <QLineEdit>
#include <QMimeDatabase>

#include <config-kiofilewidgets.h>

#include <algorithm>
#include <utility>

class KFileFilterComboPrivate
{
public:
    explicit KFileFilterComboPrivate(KFileFilterCombo *qq)
        : q(qq)
    {
    }

    void slotFilterChanged();

    KFileFilterCombo *const q;
    // when we have more than 3 mimefilters and no default-filter,
    // we don't show the comments of all mimefilters in one line,
    // instead we show "All supported files". We have to translate
    // that back to the list of mimefilters in currentFilter() tho.
    bool m_hasAllSupportedFiles = false;
    // true when setMimeFilter was called
    bool m_isMimeFilter = false;
    QString m_lastFilter;
    QString m_defaultFilter = i18nc("Default mime type filter that shows all file types", "*|All Files");

    QVector<KFileFilter> m_filters;
    bool m_allTypes;
};

KFileFilterCombo::KFileFilterCombo(QWidget *parent)
    : KComboBox(true, parent)
    , d(new KFileFilterComboPrivate(this))
{
    setTrapReturnKey(true);
    setInsertPolicy(QComboBox::NoInsert);
    connect(this, qOverload<int>(&QComboBox::activated), this, &KFileFilterCombo::filterChanged);
    // TODO KF6: remove this QOverload, only KUrlComboBox::returnPressed(const QString &) will remain
    connect(this, qOverload<const QString &>(&KComboBox::returnPressed), this, &KFileFilterCombo::filterChanged);
    connect(this, &KFileFilterCombo::filterChanged, this, [this]() {
        d->slotFilterChanged();
    });
    d->m_allTypes = false;
}

KFileFilterCombo::~KFileFilterCombo() = default;

void KFileFilterCombo::setFilter(const QString &filterString)
{
    clear();
    d->m_filters.clear();
    d->m_hasAllSupportedFiles = false;

    const QVector<KFileFilter> filters = KFileFilter::fromFilterString(filterString);

    if (!filters.isEmpty()) {
        d->m_filters = filters;
    } else {
        d->m_filters = KFileFilter::fromFilterString(d->m_defaultFilter);
    }

    for (const KFileFilter &filter : std::as_const(d->m_filters)) {
        addItem(filter.label());
    }

    d->m_lastFilter = currentText();
    d->m_isMimeFilter = false;
}

QString KFileFilterCombo::currentFilter() const
{
    QString f = currentText();
    if (f == itemText(currentIndex())) { // user didn't edit the text
        f = d->m_filters.value(currentIndex()).toFilterString();
        if (d->m_isMimeFilter || (currentIndex() == 0 && d->m_hasAllSupportedFiles)) {
            return f; // we have a MIME type as filter
        }
    }

    int tab = f.indexOf(QLatin1Char('|'));
    if (tab < 0) {
        return f;
    } else {
        return f.left(tab);
    }
}

bool KFileFilterCombo::showsAllTypes() const
{
    return d->m_allTypes;
}

QStringList KFileFilterCombo::filters() const
{
    QStringList result;

    for (const KFileFilter &filter : std::as_const(d->m_filters)) {
        result << filter.toFilterString();
    }

    return result;
}

void KFileFilterCombo::setCurrentFilter(const QString &filterString)
{
    auto it = std::find_if(d->m_filters.cbegin(), d->m_filters.cend(), [filterString](const KFileFilter &filter) {
        return filterString == filter.toFilterString();
    });

    if (it == d->m_filters.cend()) {
        qWarning() << "Could not find filter" << filterString;
        setCurrentIndex(-1);
        Q_EMIT filterChanged();
        return;
    }

    setCurrentIndex(std::distance(d->m_filters.cbegin(), it));
    Q_EMIT filterChanged();
}

void KFileFilterCombo::setMimeFilter(const QStringList &types, const QString &defaultType)
{
    clear();
    d->m_filters.clear();
    QString delim = QStringLiteral(", ");
    d->m_hasAllSupportedFiles = false;
    bool hasAllFilesFilter = false;
    QMimeDatabase db;

    d->m_allTypes = defaultType.isEmpty() && (types.count() > 1);

    // If there's MIME types that have the same comment, we will show the extension
    // in addition to the MIME type comment
    QHash<QString, int> allTypeComments;
    for (QStringList::ConstIterator it = types.begin(); it != types.end(); ++it) {
        const QMimeType type = db.mimeTypeForName(*it);
        if (!type.isValid()) {
            qCWarning(KIO_KFILEWIDGETS_KFILEFILTERCOMBO) << *it << "is not a valid MIME type";
            continue;
        }

        allTypeComments[type.comment()] += 1;
    }

    for (QStringList::ConstIterator it = types.begin(); it != types.end(); ++it) {
        // qDebug() << *it;
        const QMimeType type = db.mimeTypeForName(*it);
        if (!type.isValid()) {
            qCWarning(KIO_KFILEWIDGETS_KFILEFILTERCOMBO) << *it << "is not a valid MIME type";
            continue;
        }

        if (type.name().startsWith(QLatin1String("all/")) || type.isDefault()) {
            hasAllFilesFilter = true;
            continue;
        }

        KFileFilter filter;

        if (allTypeComments.value(type.comment()) > 1) {
            const QString label = i18nc("%1 is the mimetype name, %2 is the extensions", "%1 (%2)", type.comment(), type.suffixes().join(QLatin1String(", ")));
            filter = KFileFilter(label, {}, {*it});
        } else {
            filter = KFileFilter(*it);
        }

        d->m_filters.append(filter);
        addItem(filter.label());

        if (type.name() == defaultType) {
            setCurrentIndex(count() - 1);
        }
    }

    if (count() == 1) {
        d->m_allTypes = false;
    }

    if (d->m_allTypes) {
        QStringList allTypes;
        for (const KFileFilter &filter : std::as_const(d->m_filters)) {
            allTypes << filter.mimePatterns().join(QLatin1Char(' '));
        }

        KFileFilter allSupportedFilesFilter;

        if (count() <= 3) { // show the MIME type comments of at max 3 types
            QStringList allComments;
            for (const KFileFilter &filter : std::as_const(d->m_filters)) {
                allComments << filter.label();
            }

            allSupportedFilesFilter = KFileFilter(allComments.join(delim), {}, allTypes);
        } else {
            allSupportedFilesFilter = KFileFilter(i18n("All Supported Files"), {}, allTypes);
            d->m_hasAllSupportedFiles = true;
        }

        insertItem(0, allSupportedFilesFilter.label());
        d->m_filters.prepend(allSupportedFilesFilter);
        setCurrentIndex(0);
    }

    if (hasAllFilesFilter) {
        addItem(i18n("All Files"));
        d->m_filters.append(KFileFilter(i18n("All Files"), {}, {QStringLiteral("application/octet-stream")}));
    }

    d->m_lastFilter = currentText();
    d->m_isMimeFilter = true;
}

void KFileFilterComboPrivate::slotFilterChanged()
{
    m_lastFilter = q->currentText();
}

bool KFileFilterCombo::eventFilter(QObject *o, QEvent *e)
{
    if (o == lineEdit() && e->type() == QEvent::FocusOut) {
        if (currentText() != d->m_lastFilter) {
            Q_EMIT filterChanged();
        }
    }

    return KComboBox::eventFilter(o, e);
}

void KFileFilterCombo::setDefaultFilter(const QString &filter)
{
    d->m_defaultFilter = filter;
}

QString KFileFilterCombo::defaultFilter() const
{
    return d->m_defaultFilter;
}

bool KFileFilterCombo::isMimeFilter() const
{
    return d->m_isMimeFilter;
}

#include "moc_kfilefiltercombo.cpp"
