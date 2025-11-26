/******************************************************************
 * Copyright 2021 Reion Wong <aj@cutefishos.com>
 * Copyright 2016 Kai Uwe Broulik <kde@privat.broulik.de>
 * Copyright 2016 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************/

#include "appmenumodel.h"

#include <QAction>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QGuiApplication>
#include <QMenu>
#include <QTimer>
#include <QDebug>


#include "../libdbusmenuqt/dbusmenuimporter.h"

#include <xcb/xcb.h>
#include <xcb/xproto.h>

/*
 * Helper: get xcb_connection_t from Qt6
 */
static xcb_connection_t *qt_x11_connection()
{
    auto x11 = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11)
        return nullptr;
    return x11->connection();
}

/*
 * Read a string property from a window (like object path / service name)
 */
static QByteArray getWindowPropertyString(xcb_window_t id, const QByteArray &name)
{
    xcb_connection_t *c = qt_x11_connection();
    QByteArray value;

    if (!c)
        return value;

    const xcb_intern_atom_cookie_t atomCookie =
            xcb_intern_atom(c, false, name.size(), name.constData());
    std::unique_ptr<xcb_intern_atom_reply_t, void(*)(xcb_intern_atom_reply_t*)> atomReply(
            xcb_intern_atom_reply(c, atomCookie, nullptr),
            [](xcb_intern_atom_reply_t *p){ if (p) free(p); });

    if (!atomReply)
        return value;

    static const uint32_t MAX_PROP_SIZE = 10000;
    xcb_get_property_cookie_t propertyCookie =
            xcb_get_property(c, false, id, atomReply->atom, XCB_ATOM_STRING, 0, MAX_PROP_SIZE);

    std::unique_ptr<xcb_get_property_reply_t, void(*)(xcb_get_property_reply_t*)> propertyReply(
            xcb_get_property_reply(c, propertyCookie, nullptr),
            [](xcb_get_property_reply_t *p){ if (p) free(p); });

    if (!propertyReply)
        return value;

    if (propertyReply->type == XCB_ATOM_STRING &&
        propertyReply->format == 8 &&
        propertyReply->value_len > 0) {

        const char *data = reinterpret_cast<const char*>(xcb_get_property_value(propertyReply.get()));
        int len = propertyReply->value_len;
        if (data)
            value = QByteArray(data, data[len - 1] ? len : len - 1);
    }

    return value;
}

/*
 * Get the current active window using _NET_ACTIVE_WINDOW on the root window.
 * Returns 0 when unavailable.
 */
static xcb_window_t getActiveWindow()
{
    xcb_connection_t *c = qt_x11_connection();
    if (!c)
        return 0;

    xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(c));
    xcb_screen_t *screen = it.data;
    if (!screen)
        return 0;

    xcb_window_t root = screen->root;
    const QByteArray atomName = QByteArrayLiteral("_NET_ACTIVE_WINDOW");

    xcb_intern_atom_cookie_t atomCookie =
            xcb_intern_atom(c, false, atomName.size(), atomName.constData());
    std::unique_ptr<xcb_intern_atom_reply_t, void(*)(xcb_intern_atom_reply_t*)> atomReply(
            xcb_intern_atom_reply(c, atomCookie, nullptr),
            [](xcb_intern_atom_reply_t *p){ if (p) free(p); });

    if (!atomReply)
        return 0;

    xcb_get_property_cookie_t propCookie =
            xcb_get_property(c, false, root, atomReply->atom, XCB_ATOM_WINDOW, 0, 1);

    std::unique_ptr<xcb_get_property_reply_t, void(*)(xcb_get_property_reply_t*)> propReply(
            xcb_get_property_reply(c, propCookie, nullptr),
            [](xcb_get_property_reply_t *p){ if (p) free(p); });

    if (!propReply)
        return 0;

    if (propReply->value_len == 0)
        return 0;

    // value is window id (CARD32)
    uint32_t *val = reinterpret_cast<uint32_t*>(xcb_get_property_value(propReply.get()));
    if (!val)
        return 0;

    return static_cast<xcb_window_t>(*val);
}

/* ------------------------------------------------------------------ */

class CDBusMenuImporter : public DBusMenuImporter
{
public:
    CDBusMenuImporter(const QString &service, const QString &path, QObject *parent)
        : DBusMenuImporter(service, path, parent)
    {
    }

protected:
    QIcon iconForName(const QString &name) override
    {
        return QIcon::fromTheme(name);
    }
};

AppMenuModel::AppMenuModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_serviceWatcher(new QDBusServiceWatcher(this))
{
    connect(this, &AppMenuModel::modelNeedsUpdate, this, [this] {
        if (!m_updatePending) {
            m_updatePending = true;
            QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
        }
    });

    // Polling timer to detect active window changes (替代 KWindowSystem 信号)
    QTimer *pollTimer = new QTimer(this);
    pollTimer->setInterval(400); // 400ms, 可调
    connect(pollTimer, &QTimer::timeout, this, &AppMenuModel::onActiveWindowChanged);
    pollTimer->start();

    // 初始触发一次
    onActiveWindowChanged();

    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    // if our current DBus connection gets lost, close the menu
    // we'll select the new menu when the focus changes
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &serviceName) {
        if (serviceName == m_serviceName) {
            setMenuAvailable(false);
            emit modelNeedsUpdate();
        }
    });
}

AppMenuModel::~AppMenuModel() = default;

bool AppMenuModel::menuAvailable() const
{
    return m_menuAvailable;
}

void AppMenuModel::setMenuAvailable(bool set)
{
    if (m_menuAvailable != set) {
        m_menuAvailable = set;
        setVisible(true);
        emit menuAvailableChanged();
    }
}

bool AppMenuModel::visible() const
{
    return m_visible;
}

void AppMenuModel::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

int AppMenuModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (!m_menuAvailable || !m_menu) {
        return 0;
    }

    return m_menu->actions().count();
}

void AppMenuModel::update()
{
    beginResetModel();
    endResetModel();
    m_updatePending = false;
}

void AppMenuModel::onActiveWindowChanged()
{
    // read active window via EWMH
    const xcb_window_t active = getActiveWindow();
    if (!active) {
        // no active window
        setVisible(false);
        return;
    }

    const QString objectPath = QString::fromUtf8(getWindowPropertyString(active, QByteArrayLiteral("_KDE_NET_WM_APPMENU_OBJECT_PATH")));
    const QString serviceName = QString::fromUtf8(getWindowPropertyString(active, QByteArrayLiteral("_KDE_NET_WM_APPMENU_SERVICE_NAME")));

    if (!objectPath.isEmpty() && !serviceName.isEmpty()) {
        setMenuAvailable(true);
        updateApplicationMenu(serviceName, objectPath);
        setVisible(true);
        emit modelNeedsUpdate();
    } else {
        // no appmenu for active window
        setVisible(false);
    }
}

QHash<int, QByteArray> AppMenuModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[MenuRole] = QByteArrayLiteral("activeMenu");
    roleNames[ActionRole] = QByteArrayLiteral("activeActions");
    return roleNames;
}

QVariant AppMenuModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row < 0 || !m_menuAvailable || !m_menu) {
        return QVariant();
    }

    const auto actions = m_menu->actions();
    if (row >= actions.count()) {
        return QVariant();
    }

    if (role == MenuRole) { // TODO this should be Qt::DisplayRole
        return actions.at(row)->text();
    } else if (role == ActionRole) {
        return QVariant::fromValue((void *)actions.at(row));
    }

    return QVariant();
}

void AppMenuModel::updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath)
{
    if (m_serviceName == serviceName && m_menuObjectPath == menuObjectPath) {
        if (m_importer) {
            QMetaObject::invokeMethod(m_importer, "updateMenu", Qt::QueuedConnection);
        }
        return;
    }

    m_serviceName = serviceName;
    m_serviceWatcher->setWatchedServices(QStringList({m_serviceName}));

    m_menuObjectPath = menuObjectPath;

    if (m_importer) {
        m_importer->deleteLater();
    }

    m_importer = new CDBusMenuImporter(serviceName, menuObjectPath, this);
    QMetaObject::invokeMethod(m_importer, "updateMenu", Qt::QueuedConnection);

    connect(m_importer.data(), &DBusMenuImporter::menuUpdated, this, [=](QMenu *menu) {
        m_menu = m_importer->menu();
        if (m_menu.isNull() || menu != m_menu) {
            return;
        }

        // cache first layer of sub menus, which we'll be popping up
        const auto actions = m_menu->actions();
        for (QAction *a : actions) {
            // signal dataChanged when the action changes
            connect(a, &QAction::changed, this, [this, a] {
                if (m_menuAvailable && m_menu) {
                    const int actionIdx = m_menu->actions().indexOf(a);
                    if (actionIdx > -1) {
                        const QModelIndex modelIdx = index(actionIdx, 0);
                        emit dataChanged(modelIdx, modelIdx);
                    }
                }
            });

            connect(a, &QAction::destroyed, this, &AppMenuModel::modelNeedsUpdate);

            if (a->menu()) {
                m_importer->updateMenu(a->menu());
            }
        }

        setMenuAvailable(true);
        emit modelNeedsUpdate();
    });

    connect(m_importer.data(), &DBusMenuImporter::actionActivationRequested, this, [this](QAction *action) {
        // TODO submenus
        if (!m_menuAvailable || !m_menu) {
            return;
        }

        const auto actions = m_menu->actions();
        auto it = std::find(actions.begin(), actions.end(), action);
        if (it != actions.end()) {
            Q_EMIT requestActivateIndex(it - actions.begin());
        }
    });
}
