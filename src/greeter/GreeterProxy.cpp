/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
***************************************************************************/

#include "GreeterProxy.h"

#include "Configuration.h"
#include "Messages.h"
#include "SessionModel.h"
#include "SocketWriter.h"

#include <QLocalSocket>

namespace SDE {
    class GreeterProxyPrivate {
    public:
        SessionModel *sessionModel { nullptr };
        QLocalSocket *socket { nullptr };
        bool canPowerOff { false };
        bool canReboot { false };
        bool canSuspend { false };
        bool canHibernate { false };
        bool canHybridSleep { false };
    };

    GreeterProxy::GreeterProxy(const QString &socket, QObject *parent) : QObject(parent), d(new GreeterProxyPrivate()) {
        d->socket = new QLocalSocket(this);
        // connect signals
        connect(d->socket, SIGNAL(connected()), this, SLOT(connected()));
        connect(d->socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
        connect(d->socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
        connect(d->socket, SIGNAL(error(QLocalSocket::LocalSocketError)), this, SLOT(error()));

        // connect to server
        d->socket->connectToServer(socket);
    }

    GreeterProxy::~GreeterProxy() {
        delete d->socket;
        delete d;
    }

    void GreeterProxy::setSessionModel(SessionModel *model) {
        d->sessionModel = model;
    }

    bool GreeterProxy::canPowerOff() const {
        return d->canPowerOff;
    }

    bool GreeterProxy::canReboot() const {
        return d->canReboot;
    }

    bool GreeterProxy::canSuspend() const {
        return d->canSuspend;
    }

    bool GreeterProxy::canHibernate() const {
        return d->canHibernate;
    }

    bool GreeterProxy::canHybridSleep() const {
        return d->canHybridSleep;
    }

    void GreeterProxy::powerOff() {
        SocketWriter(d->socket) << quint32(GreeterMessages::PowerOff);
    }

    void GreeterProxy::reboot() {
        SocketWriter(d->socket) << quint32(GreeterMessages::Reboot);
    }

    void GreeterProxy::suspend() {
        SocketWriter(d->socket) << quint32(GreeterMessages::Suspend);
    }

    void GreeterProxy::hibernate() {
        SocketWriter(d->socket) << quint32(GreeterMessages::Hibernate);
    }

    void GreeterProxy::hybridSleep() {
        SocketWriter(d->socket) << quint32(GreeterMessages::HybridSleep);
    }

    void GreeterProxy::login(const QString &user, const QString &password, const int sessionIndex) const {
        if (!d->sessionModel) {
            // log error
            qCritical() << "GREETER: Session model is not set.";

            // return
            return;
        }

        // get model index
        QModelIndex index = d->sessionModel->index(sessionIndex, 0);

        // send command to the daemon
        SocketWriter(d->socket) << quint32(GreeterMessages::Login) << user << password << d->sessionModel->data(index, SessionModel::FileRole).toString();
    }

    void GreeterProxy::connected() {
        // log connection
        qDebug() << "GREETER: Connected to the daemon.";

        // send connected message
        SocketWriter(d->socket) << quint32(GreeterMessages::Connect);
    }

    void GreeterProxy::disconnected() {
        // log disconnection
        qDebug() << "GREETER: Disconnected from the daemon.";
    }

    void GreeterProxy::error() {
        qCritical() << "GREETER: Socket error: " << d->socket->errorString();
    }

    void GreeterProxy::readyRead() {
        // input stream
        QDataStream input(d->socket);

        // read message
        quint32 message;
        input >> message;

        switch (DaemonMessages(message)) {
            case DaemonMessages::Capabilities: {
                // log message
                qDebug() << "GREETER: Message received from daemon: Capabilities";

                // read capabilities
                quint32 capabilities;
                input >> capabilities;

                // parse capabilities
                d->canPowerOff = capabilities & quint32(Capabilities::PowerOff);
                d->canReboot = capabilities & quint32(Capabilities::Reboot);
                d->canSuspend = capabilities & quint32(Capabilities::Suspend);
                d->canHibernate = capabilities & quint32(Capabilities::Hibernate);
                d->canHybridSleep = capabilities & quint32(Capabilities::HybridSleep);
            }
            break;
            case DaemonMessages::LoginSucceeded: {
                // log message
                qDebug() << "GREETER: Message received from daemon: LoginSucceeded";

                // emit signal
                emit loginSucceeded();
            }
            break;
            case DaemonMessages::LoginFailed: {
                // log message
                qDebug() << "GREETER: Message received from daemon: LoginFailed";

                // emit signal
                emit loginFailed();
            }
            break;
            default: {
                // log message
                qWarning() << "GREETER: Unknown message received from daemon.";
            }
        }
    }
}
