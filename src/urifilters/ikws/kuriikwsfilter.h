/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>
    SPDX-FileCopyrightText: 2002, 2003 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KURIIKWSFILTER_H
#define KURIIKWSFILTER_H

#include "kurifilterplugin_p.h"
#include <QVariant>
#include <kurifilter.h>

class KAutoWebSearch : public KUriFilterPlugin
{
    Q_OBJECT
public:
    explicit KAutoWebSearch(QObject *parent);
    ~KAutoWebSearch() override;
    bool filterUri(KUriFilterData &) const override;

private:
    void populateProvidersList(QList<KUriFilterSearchProvider *> &searchProviders, const KUriFilterData &, bool allproviders = false) const;
};

#endif
