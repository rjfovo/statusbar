/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     cutefishos <cutefishos@foxmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "activity.h"

#include <QFile>
#include <QCursor>
#include <QDebug>
#include <QDirIterator>
#include <QSettings>
#include <QRegularExpression>
#include <QGuiApplication>                 // <-- Qt6: 用来访问 nativeInterface

#include <NETWM>
#include <KWindowEffects>
#include <KX11Extras>                      // KF6 X11 API
#include <KWindowInfo>
#include <KWindowSystem>

static const QStringList blockList = {
    "cutefish-launcher",
    "cutefish-statusbar"
};

Activity::Activity(QObject *parent)
    : QObject(parent)
    , m_cApps(CApplications::self())
{
#ifdef KWS_X11
    onActiveWindowChanged();

    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged,
            this, &Activity::onActiveWindowChanged);

    connect(KWindowSystem::self(),
            static_cast<void (KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>
            (&KWindowSystem::windowChanged),
            this, &Activity::onActiveWindowChanged);
#endif
}

bool Activity::launchPad() const
{
    return m_launchPad;
}

QString Activity::title() const
{
    return m_title;
}

QString Activity::icon() const
{
    return m_icon;
}

void Activity::close()
{
#ifdef KWS_X11
    if (auto x11 = QGuiApplication::instance()->nativeInterface<QNativeInterface::QX11Application>()) {
        NETRootInfo(x11->connection(), NET::CloseWindow)
            .closeWindowRequest(KWindowSystem::activeWindow());
    }
#endif
}

void Activity::minimize()
{
#ifdef KWS_X11
    KX11Extras::minimizeWindow(KWindowSystem::activeWindow());
#endif
}

void Activity::restore()
{
#ifdef KWS_X11
    KX11Extras::clearState(KWindowSystem::activeWindow(), NET::Max);
#endif
}

void Activity::maximize()
{
#ifdef KWS_X11
    KX11Extras::setState(KWindowSystem::activeWindow(), NET::Max);
#endif
}

void Activity::toggleMaximize()
{
#ifdef KWS_X11
    KWindowInfo info(KWindowSystem::activeWindow(), NET::WMState);
    bool isWindow = !info.hasState(NET::SkipTaskbar) ||
                    info.windowType(NET::UtilityMask) != NET::Utility ||
                    info.windowType(NET::DesktopMask) != NET::Desktop;

    if (!isWindow)
        return;

    bool isMax = info.hasState(NET::Max);
    isMax ? restore() : maximize();
#endif
}

void Activity::move()
{
#ifdef KWS_X11
    WId winId = KWindowSystem::activeWindow();
    KWindowInfo info(winId, NET::WMState | NET::WMGeometry | NET::WMDesktop);
    bool isWindow = !info.hasState(NET::SkipTaskbar) ||
                    info.windowType(NET::UtilityMask) != NET::Utility ||
                    info.windowType(NET::DesktopMask) != NET::Desktop;

    if (!isWindow) return;

    if (!info.isOnCurrentDesktop()) {
        KX11Extras::setCurrentDesktop(info.desktop());
        KX11Extras::forceActiveWindow(winId);
    }

    if (auto x11 = QGuiApplication::instance()->nativeInterface<QNativeInterface::QX11Application>()) {
        NETRootInfo ri(x11->connection(), NET::WMMoveResize);
        ri.moveResizeRequest(
            winId,
            QCursor::pos().x(),
            QCursor::pos().y(),
            NET::Move
        );
    }
#endif
}

bool Activity::isAcceptableWindow(quint64 wid)
{
#ifdef KWS_X11
    QFlags<NET::WindowTypeMask> ignoreList;
    ignoreList |= NET::DesktopMask;
    ignoreList |= NET::DockMask;
    ignoreList |= NET::SplashMask;
    ignoreList |= NET::ToolbarMask;
    ignoreList |= NET::MenuMask;
    ignoreList |= NET::PopupMenuMask;
    ignoreList |= NET::NotificationMask;

    KWindowInfo info(wid, NET::WMWindowType | NET::WMState,
                     NET::WM2TransientFor | NET::WM2WindowClass);

    if (!info.valid())
        return false;

    if (NET::typeMatchesMask(info.windowType(NET::AllTypesMask), ignoreList))
        return false;

    if (info.hasState(NET::SkipTaskbar) || info.hasState(NET::SkipPager))
        return false;

    WId root = 0;
    if (auto x11 = QGuiApplication::instance()->nativeInterface<QNativeInterface::QX11Application>()) {
        root = x11->appRootWindow ? static_cast<WId>(x11->appRootWindow()) : 0;
        // Note: some Qt builds expose appRootWindow via that native interface; if not available,
        // root will remain 0 and transientFor checks fall back accordingly.
    }

    WId trans = info.transientFor();
    if (trans == 0 || trans == wid || trans == root)
        return true;

    info = KWindowInfo(trans, NET::WMWindowType);

    QFlags<NET::WindowTypeMask> normal;
    normal |= NET::NormalMask;
    normal |= NET::DialogMask;
    normal |= NET::UtilityMask;

    return !NET::typeMatchesMask(info.windowType(NET::AllTypesMask), normal);
#else
    Q_UNUSED(wid)
    return false;
#endif
}

void Activity::onActiveWindowChanged()
{
#ifdef KWS_X11
    KWindowInfo info(KWindowSystem::activeWindow(),
                     NET::WMState | NET::WMVisibleName | NET::WMWindowType,
                     NET::WM2WindowClass);

    m_launchPad = (info.windowClassClass() == "cutefish-launcher");
    emit launchPadChanged();

    if (NET::typeMatchesMask(info.windowType(NET::AllTypesMask), NET::DesktopMask)) {
        m_title = tr("Desktop");
        m_icon.clear();
        emit titleChanged();
        emit iconChanged();
        return;
    }

    if (!isAcceptableWindow(KWindowSystem::activeWindow()) ||
        blockList.contains(info.windowClassClass())) {
        clearTitle();
        clearIcon();
        return;
    }

    m_pid = info.pid();
    m_windowClass = info.windowClassClass().toLower();

    CAppItem *item = m_cApps->matchItem(m_pid, m_windowClass);
    if (item) {
        m_title = item->localName;
        emit titleChanged();

        if (m_icon != item->icon) {
            m_icon = item->icon;
            emit iconChanged();
        }
    } else {
        QString t = info.visibleName();
        if (t != m_title) {
            m_title = t;
            emit titleChanged();
            clearIcon();
        }
    }
#endif
}

void Activity::clearTitle()
{
    m_title.clear();
    emit titleChanged();
}

void Activity::clearIcon()
{
    m_icon.clear();
    emit iconChanged();
}
