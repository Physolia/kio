/*
    SPDX-License-Identifier: LGPL-2.0-or-later
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2019-2021 Harald Sitter <sitter@kde.org>
*/

#include "workerbase.h"

#include <commands_p.h>
#include <slavebase.h>

namespace KIO
{
// Bridges new worker API to legacy slave API. Overrides all SlaveBase virtual functions and redirects them at the
// fronting WorkerBase implementation. The WorkerBase implementation then returns Result objects which we translate
// back to the appropriate signal calls (error, finish, opened, etc.).
// When starting the dispatchLoop it actually runs inside the SlaveBase, so the SlaveBase is in the driver seat
// until KF6 when we can fully remove the SlaveBase in favor of the WorkerBase (means moving the dispatch and
// dispatchLoop functions into the WorkerBase and handling the signaling in the dispatch function rather than
// this intermediate Bridge object).
class WorkerSlaveBaseBridge : public SlaveBase
{
    void finalize(const WorkerResult &result)
    {
        if (!result.success) {
            error(result.error, result.errorString);
            return;
        }
        finished();
    }

    void maybeError(const WorkerResult &result)
    {
        if (!result.success) {
            error(result.error, result.errorString);
        }
    }

public:
    using SlaveBase::SlaveBase;

#warning for all the setFoo functions check the return types in the .h once we know what to do with them. they are void currently

    void setHost(const QString &host, quint16 port, const QString &user, const QString &pass) final
    {
#warning setHost is not actually ever called by anything (not even dispatch!) fixme
        base->setHost(host, port, user, pass);
    }

    void setSubUrl(const QUrl &url) final
    {
#warning setSubUrl is not actually ever called by anything (not even dispatch!) fixme
        finalize(base->setSubUrl(url));
    }

    void openConnection() final
    {
        const WorkerResult result = base->openConnection();
        if (!result.success) {
            error(result.error, result.errorString);
            return;
        }
        connected();
    }

    void closeConnection() final
    {
        base->closeConnection(); // not allowed to error but also not finishing
    }

    void get(const QUrl &url) final
    {
        finalize(base->get(url));
    }

    void open(const QUrl &url, QIODevice::OpenMode mode) final
    {
        const WorkerResult result = base->open(url, mode);
        if (!result.success) {
            error(result.error, result.errorString);
            return;
        }
        opened();
    }

    void read(KIO::filesize_t size) final
    {
        maybeError(base->read(size));
    }

    void write(const QByteArray &data) final
    {
        maybeError(base->write(data));
    }

    void seek(KIO::filesize_t offset) final
    {
        maybeError(base->seek(offset));
    }

    void close() final
    {
        finalize(base->close());
    }

    void put(const QUrl &url, int permissions, JobFlags flags) final
    {
        finalize(base->put(url, permissions, flags));
    }

    void stat(const QUrl &url) final
    {
        finalize(base->stat(url));
    }

    void mimetype(const QUrl &url) final
    {
        finalize(base->mimetype(url));
    }

    void listDir(const QUrl &url) final
    {
        finalize(base->listDir(url));
    }

    void mkdir(const QUrl &url, int permissions) final
    {
        finalize(base->mkdir(url, permissions));
    }

    void rename(const QUrl &src, const QUrl &dest, JobFlags flags) final
    {
        finalize(base->rename(src, dest, flags));
    }

    void symlink(const QString &target, const QUrl &dest, JobFlags flags) final
    {
        finalize(base->symlink(target, dest, flags));
    }

    void chmod(const QUrl &url, int permissions) final
    {
        finalize(base->chmod(url, permissions));
    }

    void chown(const QUrl &url, const QString &owner, const QString &group) final
    {
        finalize(base->chown(url, owner, group));
    }

    void setModificationTime(const QUrl &url, const QDateTime &mtime) final
    {
#warning setModificationTime is not actually ever called by anything (not even dispatch!) fixme
        finalize(base->setModificationTime(url, mtime));
    }

    void copy(const QUrl &src, const QUrl &dest, int permissions, JobFlags flags) final
    {
        finalize(base->copy(src, dest, permissions, flags));
    }

    void del(const QUrl &url, bool isfile) final
    {
        finalize(base->del(url, isfile));
    }

    void setLinkDest(const QUrl &url, const QString &target) final
    {
#warning setLinkDest is not actually ever called by anything (not even dispatch!) fixme
        finalize(base->setLinkDest(url, target));
    }

    void special(const QByteArray &data) final
    {
        // FIXME is it really non conclusive? special is a bit awkward in general I suspect
        maybeError(base->special(data));
    }

    void multiGet(const QByteArray &data) final
    {
        finalize(base->multiGet(data));
    }

    void slave_status() final
    {
        base->slave_status(); // this only requests an update and isn't able to error or finish whatsoever
    }

    void reparseConfiguration() final
    {
        // TODO it's a bit awkward that usually you aren't meant to call the base virtual but in this case you almost must lest remoteEncoding breaks
        base->reparseConfiguration();
        SlaveBase::reparseConfiguration();
    }

    WorkerBase *base = nullptr;

protected:
    void virtual_hook(int id, void *data) override
    {
        switch (id) {
#warning what do with AppConnectionMade actually it doesn't seem handled or used anywhere
        case SlaveBase::GetFileSystemFreeSpace:
            finalize(base->fileSystemFreeSpace(*static_cast<QUrl *>(data)));
            return;
        case SlaveBase::Truncate:
            maybeError(base->truncate(*static_cast<KIO::filesize_t *>(data)));
            break;
        }

#warning do we really still need and want this? I suppose this is so we can add more functions. but the ones we had in kf5 are super undocumented ... what gives
#warning also why do we not simply detatch the business logic from the interface behind the scenes. there's no reason we couldn't drive WorkerBase1 and WorkerBase2 depending on which is implemented
        maybeError(base->virtual_hook(id, data));
    }
};

class WorkerBasePrivate
{
public:
    WorkerBasePrivate(const QByteArray &protocol, const QByteArray &poolSocket, const QByteArray &appSocket, WorkerBase *base)
        : bridge(protocol, poolSocket, appSocket)
    {
        bridge.base = base;
    }

    WorkerSlaveBaseBridge bridge;

    inline QString protocolName() const
    {
        return bridge.protocolName();
    }
};

WorkerBase::WorkerBase(const QByteArray &protocol, const QByteArray &poolSocket, const QByteArray &appSocket)
    : d(new WorkerBasePrivate(protocol, poolSocket, appSocket, this))
{
}

WorkerBase::~WorkerBase() = default;

void WorkerBase::dispatchLoop()
{
    d->bridge.dispatchLoop();
}

void WorkerBase::connectSlave(const QString &address)
{
    d->bridge.connectSlave(address);
}

void WorkerBase::disconnectSlave()
{
    d->bridge.disconnectSlave();
}

void WorkerBase::setMetaData(const QString &key, const QString &value)
{
    d->bridge.setMetaData(key, value);
}

QString WorkerBase::metaData(const QString &key) const
{
    return d->bridge.metaData(key);
}

MetaData WorkerBase::allMetaData() const
{
    return d->bridge.allMetaData();
}

bool WorkerBase::hasMetaData(const QString &key) const
{
    return d->bridge.hasMetaData(key);
}

QMap<QString, QVariant> WorkerBase::mapConfig() const
{
    return d->bridge.mapConfig();
}

bool WorkerBase::configValue(const QString &key, bool defaultValue) const
{
    return d->bridge.configValue(key, defaultValue);
}

int WorkerBase::configValue(const QString &key, int defaultValue) const
{
    return d->bridge.configValue(key, defaultValue);
}

QString WorkerBase::configValue(const QString &key, const QString &defaultValue) const
{
    return d->bridge.configValue(key, defaultValue);
}

KConfigGroup *WorkerBase::config()
{
    return d->bridge.config();
}

void WorkerBase::sendMetaData()
{
    d->bridge.sendMetaData();
}

void WorkerBase::sendAndKeepMetaData()
{
    d->bridge.sendAndKeepMetaData();
}

KRemoteEncoding *WorkerBase::remoteEncoding()
{
    return d->bridge.remoteEncoding();
}

void WorkerBase::data(const QByteArray &data)
{
    d->bridge.data(data);
}

void WorkerBase::dataReq()
{
    d->bridge.dataReq();
}

void WorkerBase::needSubUrlData()
{
    d->bridge.needSubUrlData();
}

void WorkerBase::slaveStatus(const QString &host, bool connected)
{
    d->bridge.slaveStatus(host, connected);
}

void WorkerBase::canResume()
{
    d->bridge.canResume();
}

void WorkerBase::totalSize(KIO::filesize_t _bytes)
{
    d->bridge.totalSize(_bytes);
}

void WorkerBase::processedSize(KIO::filesize_t _bytes)
{
    d->bridge.processedSize(_bytes);
}

void WorkerBase::written(KIO::filesize_t _bytes)
{
    d->bridge.written(_bytes);
}

void WorkerBase::position(KIO::filesize_t _pos)
{
    d->bridge.position(_pos);
}

void WorkerBase::truncated(KIO::filesize_t _length)
{
    d->bridge.truncated(_length);
}

void WorkerBase::speed(unsigned long _bytes_per_second)
{
    d->bridge.speed(_bytes_per_second);
}

void WorkerBase::redirection(const QUrl &_url)
{
    d->bridge.redirection(_url);
}

void WorkerBase::errorPage()
{
    d->bridge.errorPage();
}

void WorkerBase::mimeType(const QString &_type)
{
    d->bridge.mimeType(_type);
}

void WorkerBase::exit()
{
    d->bridge.exit();
}

void WorkerBase::warning(const QString &_msg)
{
    d->bridge.warning(_msg);
}

void WorkerBase::infoMessage(const QString &_msg)
{
    d->bridge.infoMessage(_msg);
}

void WorkerBase::statEntry(const UDSEntry &entry)
{
    d->bridge.statEntry(entry);
}

void WorkerBase::listEntry(const UDSEntry &entry)
{
    d->bridge.listEntry(entry);
}

void WorkerBase::listEntries(const UDSEntryList &list)
{
    d->bridge.listEntries(list);
}

void WorkerBase::setHost(QString const &, quint16, QString const &, QString const &)
{
}

WorkerResult WorkerBase::openConnection()
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CONNECT));
}

void WorkerBase::closeConnection()
{
} // No response!

WorkerResult WorkerBase::stat(QUrl const &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_STAT));
}

WorkerResult WorkerBase::put(QUrl const &, int, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_PUT));
}

WorkerResult WorkerBase::special(const QByteArray &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SPECIAL));
}

WorkerResult WorkerBase::listDir(QUrl const &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_LISTDIR));
}

WorkerResult WorkerBase::get(QUrl const &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_GET));
}

WorkerResult WorkerBase::open(QUrl const &, QIODevice::OpenMode)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_OPEN));
}

WorkerResult WorkerBase::read(KIO::filesize_t)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_READ));
}

WorkerResult WorkerBase::write(const QByteArray &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_WRITE));
}

WorkerResult WorkerBase::seek(KIO::filesize_t)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SEEK));
}

WorkerResult WorkerBase::truncate(KIO::filesize_t)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_TRUNCATE));
}

WorkerResult WorkerBase::close()
{
#warning the documentation for close is rubbish its entirely unclear how this function should behave WRT error/finish
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CLOSE));
}

WorkerResult WorkerBase::mimetype(QUrl const &url)
{
    return get(url);
}

WorkerResult WorkerBase::rename(QUrl const &, QUrl const &, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_RENAME));
}

WorkerResult WorkerBase::symlink(QString const &, QUrl const &, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SYMLINK));
}

WorkerResult WorkerBase::copy(QUrl const &, QUrl const &, int, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_COPY));
}

WorkerResult WorkerBase::del(QUrl const &, bool)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_DEL));
}

WorkerResult WorkerBase::setLinkDest(const QUrl &, const QString &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SETLINKDEST));
}

WorkerResult WorkerBase::mkdir(QUrl const &, int)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_MKDIR));
}

WorkerResult WorkerBase::chmod(QUrl const &, int)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CHMOD));
}

WorkerResult WorkerBase::setModificationTime(QUrl const &, const QDateTime &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SETMODIFICATIONTIME));
}

WorkerResult WorkerBase::chown(QUrl const &, const QString &, const QString &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CHOWN));
}

WorkerResult WorkerBase::setSubUrl(QUrl const &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SUBURL));
}

WorkerResult WorkerBase::multiGet(const QByteArray &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_MULTI_GET));
}

WorkerResult WorkerBase::fileSystemFreeSpace(const QUrl &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_FILESYSTEMFREESPACE));
}

void WorkerBase::slave_status()
{
    slaveStatus(QString(), false);
}

void WorkerBase::reparseConfiguration()
{
    // base implementation called by bridge
}

int WorkerBase::openPasswordDialog(AuthInfo &info, const QString &errorMsg)
{
    return d->bridge.openPasswordDialogV2(info, errorMsg);
}

int WorkerBase::messageBox(MessageBoxType type, const QString &text, const QString &caption, const QString &buttonYes, const QString &buttonNo)
{
    return messageBox(text, type, caption, buttonYes, buttonNo, QString());
}

int WorkerBase::messageBox(const QString &text,
                           MessageBoxType type,
                           const QString &caption,
                           const QString &_buttonYes,
                           const QString &_buttonNo,
                           const QString &dontAskAgainName)
{
    // FIXME messageboxtype enum compat is totally implicit. could Q_enum maybe and ensure the types are compatible at runtime?
    // or simply pull the slavebase enum into our scope with `using`. The definition would still be in slavebase for now
    // but for the caller it's also available as KIO::WorkerBase::MessageBoxType. a bit meh to have ot include slavebase.h though
    return d->bridge.messageBox(text, static_cast<SlaveBase::MessageBoxType>(type), caption, _buttonYes, _buttonNo, dontAskAgainName);
}

bool WorkerBase::canResume(KIO::filesize_t offset)
{
    return d->bridge.canResume(offset);
}

int WorkerBase::waitForAnswer(int expected1, int expected2, QByteArray &data, int *pCmd)
{
    return d->bridge.waitForAnswer(expected1, expected2, data, pCmd);
}

int WorkerBase::readData(QByteArray &buffer)
{
    return d->bridge.readData(buffer);
}

void WorkerBase::setTimeoutSpecialCommand(int timeout, const QByteArray &data)
{
    d->bridge.setTimeoutSpecialCommand(timeout, data);
}

bool WorkerBase::checkCachedAuthentication(AuthInfo &info)
{
    return d->bridge.checkCachedAuthentication(info);
}

bool WorkerBase::cacheAuthentication(const AuthInfo &info)
{
    return d->bridge.cacheAuthentication(info);
}

// TODO should we really keep these functions. seems silly to have a very limited amount of these. if anything we maybe should have a MetaDataInterface object
// that can be obtained from the Base and used to conveniently query ALL well known keys
int WorkerBase::connectTimeout()
{
    return d->bridge.connectTimeout();
}

int WorkerBase::proxyConnectTimeout()
{
    return d->bridge.proxyConnectTimeout();
}

int WorkerBase::responseTimeout()
{
    return d->bridge.responseTimeout();
}

int WorkerBase::readTimeout()
{
    return d->bridge.readTimeout();
}

bool WorkerBase::wasKilled() const
{
    return d->bridge.wasKilled();
}

WorkerResult WorkerBase::virtual_hook(int id, void *data)
{
    Q_UNUSED(data);
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), id));
}

void WorkerBase::lookupHost(const QString &host)
{
    return d->bridge.lookupHost(host);
}

int WorkerBase::waitForHostInfo(QHostInfo &info)
{
    return d->bridge.waitForHostInfo(info);
}

PrivilegeOperationStatus WorkerBase::requestPrivilegeOperation(const QString &operationDetails)
{
    return d->bridge.requestPrivilegeOperation(operationDetails);
}

void WorkerBase::addTemporaryAuthorization(const QString &action)
{
    d->bridge.addTemporaryAuthorization(action);
}

} // namespace KIO
