/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2021 Reion Wong <aj@cutefishos.com>                     *
 *   Copyright (C) 2009 Marco Martin <notmart@gmail.com>                   *
 *   Copyright (C) 2009 Matthieu Gallien <matthieu_gallien@yahoo.fr>       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "statusnotifierwatcher.h"
#include "statusnotifieritem_interface.h"
#include "statusnotifierwatcheradaptor.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDebug>

StatusNotifierWatcher::StatusNotifierWatcher(QObject *parent)
    : QObject(parent)
{
    // 检查服务是否已经存在
    QDBusConnection dbus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *iface = dbus.interface();
    
    // 总是创建adaptor和对象，这样DBus调用才能被处理
    new StatusNotifierWatcherAdaptor(this);
    dbus.registerObject(QStringLiteral("/StatusNotifierWatcher"), this);
    
    // 检查服务是否已经存在，避免重复注册
    if (iface && iface->isServiceRegistered(QStringLiteral("org.kde.StatusNotifierWatcher")).value()) {
        qWarning() << "org.kde.StatusNotifierWatcher service already registered by another instance";
        // 不注册服务，但继续初始化其他部分
    } else {
        // 注册服务
        if (!dbus.registerService(QStringLiteral("org.kde.StatusNotifierWatcher"))) {
            qWarning() << "Failed to register org.kde.StatusNotifierWatcher service";
        }
    }

    m_serviceWatcher = new QDBusServiceWatcher(this);
    m_serviceWatcher->setConnection(dbus);
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, &StatusNotifierWatcher::serviceUnregistered);
}

StatusNotifierWatcher::~StatusNotifierWatcher()
{
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.unregisterService(QStringLiteral("org.kde.StatusNotifierWatcher"));
}

QStringList StatusNotifierWatcher::RegisteredStatusNotifierItems() const
{
    return m_registeredServices;
}

bool StatusNotifierWatcher::IsStatusNotifierHostRegistered() const
{
    return !m_statusNotifierHostServices.isEmpty();
}

void StatusNotifierWatcher::RegisterStatusNotifierItem(const QString &serviceOrPath)
{
    QString service;
    QString path;
    if (serviceOrPath.startsWith(QLatin1Char('/'))) {
        service = message().service();
        path = serviceOrPath;
    } else {
        service = serviceOrPath;
        path = QStringLiteral("/StatusNotifierItem");
    }
    QString notifierItemId = service + path;
    if (m_registeredServices.contains(notifierItemId)) {
        return;
    }
    m_serviceWatcher->addWatchedService(service);
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(service).value()) {
        // check if the service has registered a SystemTray object
        org::kde::StatusNotifierItem trayclient(service, path, QDBusConnection::sessionBus());
        if (trayclient.isValid()) {
            qDebug() << "Registering" << notifierItemId << "to system tray";
            m_registeredServices.append(notifierItemId);
            emit StatusNotifierItemRegistered(notifierItemId);
        } else {
            m_serviceWatcher->removeWatchedService(service);
        }
    } else {
        m_serviceWatcher->removeWatchedService(service);
    }
}

void StatusNotifierWatcher::RegisterStatusNotifierHost(const QString &service)
{
    if (service.contains(QLatin1String("org.kde.StatusNotifierHost-")) && QDBusConnection::sessionBus().interface()->isServiceRegistered(service).value()
        && !m_statusNotifierHostServices.contains(service)) {
        qDebug() << "Registering" << service << "as system tray";

        m_statusNotifierHostServices.insert(service);
        m_serviceWatcher->addWatchedService(service);
        emit StatusNotifierHostRegistered();
    }
}

void StatusNotifierWatcher::serviceUnregistered(const QString &name)
{
    qDebug() << "Service " << name << "unregistered";
    m_serviceWatcher->removeWatchedService(name);

    QString match = name + QLatin1Char('/');
    QStringList::Iterator it = m_registeredServices.begin();
    while (it != m_registeredServices.end()) {
        if (it->startsWith(match)) {
            QString name = *it;
            it = m_registeredServices.erase(it);
            emit StatusNotifierItemUnregistered(name);
        } else {
            ++it;
        }
    }

    if (m_statusNotifierHostServices.contains(name)) {
        m_statusNotifierHostServices.remove(name);
        emit StatusNotifierHostUnregistered();
    }
}
