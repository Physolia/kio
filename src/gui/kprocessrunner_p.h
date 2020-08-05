/* This file is part of the KDE libraries
    Copyright (c) 2020 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KPROCESSRUNNER_P_H
#define KPROCESSRUNNER_P_H

#include "applicationlauncherjob.h"
#include "kiogui_export.h"

#include <KProcess>

#include <QObject>
#include <memory>
#include <KStartupInfo>

namespace KIOGuiPrivate {
/**
 * @internal DO NOT USE
 */
bool KIOGUI_EXPORT checkStartupNotify(const KService *service, bool *silent_arg,
                                      QByteArray *wmclass_arg);
}

/**
 * @internal  (exported for KRun, currently)
 * This class runs a KService or a shell command, using QProcess internally.
 * It creates a startup notification and finishes it on success or on error (for the taskbar)
 * It also shows an error message if necessary (e.g. "program not found").
 */
class KIOGUI_EXPORT KProcessRunner : public QObject
{
    Q_OBJECT

public:
    /**
     * Run a KService (application desktop file) to open @p urls.
     * @param service the service to run
     * @param urls the list of URLs, can be empty
     * @param flags various flags
     * @param suggestedFileName see KRun::setSuggestedFileName
     * @param asn Application startup notification id, if any (otherwise "").

     */
    static KProcessRunner *fromApplication(const KService::Ptr &service,
                                           const QList<QUrl> &urls,
                                           KIO::ApplicationLauncherJob::RunFlags flags = {},
                                           const QString &suggestedFileName = {},
                                           const QByteArray &asn = {});

    /**
     * Run a shell command
     * @param cmd must be a shell command. No need to append "&" to it.
     * @param desktopName name of the desktop file, if known.
     * @param execName the name of the executable, if known.
     * @param iconName icon for the startup notification
     * @param asn Application startup notification id, if any (otherwise "").
     * @param workingDirectory the working directory for the started process. The default
     *                         (if passing an empty string) is the user's document path.
     *                         This allows a command like "kwrite file.txt" to find file.txt from the right place.
     */
    static KProcessRunner *fromCommand(const QString &cmd,
                                       const QString &desktopName,
                                       const QString &execName,
                                       const QString &iconName,
                                       const QByteArray &asn = {},
                                       const QString &workingDirectory = {});

    /**
     * Blocks until the process has started. Only exists for KRun via Command/ApplicationLauncherJob, will disappear in KF6.
     */
    virtual bool waitForStarted(int timeout = 30000) = 0;

    virtual ~KProcessRunner();

    static int instanceCount(); // for the unittest

Q_SIGNALS:
    /**
     * @brief Emitted on error. In that case, finished() is not emitted.
     * @param errorString the error message
     */
    void error(const QString &errorString);

    /**
     * @brief emitted when the process was successfully started
     * @param pid PID of the process that was started
     */
    void processStarted(qint64 pid);

protected:
    KProcessRunner(const QString &executable);
    virtual void startProcess() = 0;
    void setPid(qint64 pid);
    void terminateStartupNotification();

    std::unique_ptr<KProcess> m_process;
    QString m_executable; // can be a full path
    QString m_desktopName;
    qint64 m_pid = 0;

private:
    static KProcessRunner *makeInstance(const QString &executable);
    void emitDelayedError(const QString &errorMsg);
    void init(const KService::Ptr &service, const QString &userVisibleName,
              const QString &iconName, const QByteArray &asn);

    KStartupInfoId m_startupId;

    Q_DISABLE_COPY(KProcessRunner)
};

class ForkingProcessRunner : public KProcessRunner
{
    Q_OBJECT

public:
    explicit ForkingProcessRunner(const QString &executable);

    void startProcess() override;
    bool waitForStarted(int timeout) override;

private Q_SLOTS:
    void slotProcessExited(int, QProcess::ExitStatus);
    void slotProcessError(QProcess::ProcessError error);
    void slotProcessStarted();
};

#endif
