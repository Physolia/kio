/*
    SPDX-FileCopyrightText: 2000-2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000-2002 George Staikos <staikos@kde.org>
    SPDX-FileCopyrightText: 2000-2002 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2001, 2002 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007 Nick Shaforostoff <shafff@ukr.net>
    SPDX-FileCopyrightText: 2007-2018 Daniel Nicoletti <dantti12@gmail.com>
    SPDX-FileCopyrightText: 2008, 2009 Andreas Hartmetz <ahartmetz@gmail.com>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef HTTP_H
#define HTTP_H

#include <KIO/WorkerBase>

#include <QNetworkReply>
#include <QSslError>

#include "httpmethod_p.h"

class QDomNodeList;

class HTTPProtocol : public QObject, public KIO::WorkerBase
{
    Q_OBJECT
public:
    HTTPProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app);
    ~HTTPProtocol() override;

    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult put(const QUrl &url, int _mode, KIO::JobFlags flags) override;
    KIO::WorkerResult mimetype(const QUrl &url) override;
    KIO::WorkerResult special(const QByteArray &data) override;
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult mkdir(const QUrl &url, int _permissions) override;
    KIO::WorkerResult rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) override;
    KIO::WorkerResult copy(const QUrl &src, const QUrl &dest, int, KIO::JobFlags flags) override;
    KIO::WorkerResult del(const QUrl &url, bool _isfile) override;
    KIO::WorkerResult fileSystemFreeSpace(const QUrl &url) override;

Q_SIGNALS:
    void errorOut(KIO::Error error);

private:
    struct Response {
        int httpCode;
        QByteArray data;
        int kioCode = 0;
    };

    /**
     * Handles file -> webdav put requests.
     */
    [[nodiscard]] KIO::WorkerResult copyPut(const QUrl &src, const QUrl &dest, KIO::JobFlags flags);

    void handleSslErrors(QNetworkReply *reply, const QList<QSslError> errors);
    void handleRedirection(KIO::HTTP_METHOD method, const QUrl &originalUrl, QNetworkReply *reply);

    [[nodiscard]] KIO::WorkerResult davStatList(const QUrl &url, bool stat);
    void davParsePropstats(const QDomNodeList &propstats, KIO::UDSEntry &entry);
    QDateTime parseDateTime(const QString &input, const QString &type);
    void davParseActiveLocks(const QDomNodeList &activeLocks, uint &lockCount);
    int codeFromResponse(const QString &response);
    bool davDestinationExists(const QUrl &url);
    QByteArray getData();
    QString getContentType();

    void setSslMetaData();

    [[nodiscard]] KIO::WorkerResult post(const QUrl &url, qint64 size);
    [[nodiscard]] Response makeRequest(const QUrl &url, KIO::HTTP_METHOD method, QIODevice *inputData, const QMap<QByteArray, QByteArray> &extraHeaders = {});

    [[nodiscard]] Response makeDavRequest(const QUrl &url, KIO::HTTP_METHOD, QByteArray &inputData, const QMap<QByteArray, QByteArray> &extraHeaders = {});
    [[nodiscard]] Response makeRequest(const QUrl &url, KIO::HTTP_METHOD, QByteArray &inputData, const QMap<QByteArray, QByteArray> &extraHeaders = {});

    [[nodiscard]] KIO::WorkerResult davError(KIO::HTTP_METHOD method, const QUrl &url, const Response &response);
    [[nodiscard]] KIO::WorkerResult davError(QString &errorMsg, KIO::HTTP_METHOD method, int code, const QUrl &_url, const QByteArray &responseData);
    [[nodiscard]] KIO::WorkerResult sendHttpError(const QUrl &url, KIO::HTTP_METHOD method, const Response &response);
    [[nodiscard]] KIO::WorkerResult davGeneric(const QUrl &url, KIO::HTTP_METHOD method, qint64 size = -1);
    QString davProcessLocks();
    static QByteArray methodToString(KIO::HTTP_METHOD method);

    KIO::MetaData sslMetaData;
    KIO::Error lastError = (KIO::Error)KJob::NoError;
    QString m_hostName;
};

#endif
