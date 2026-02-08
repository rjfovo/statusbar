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

#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QIcon>
#include <QDir>
#include <QDebug>

#include "statusbar.h"
#include "controlcenterdialog.h"
#include "systemtray/systemtraymodel.h"
#include "appmenu/appmenumodel.h"
#include "appmenu/appmenuapplet.h"
#include "poweractions.h"
#include "notifications.h"
#include "backgroundhelper.h"

#include "appearance.h"
#include "brightness.h"
#include "battery.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

    // Set icon theme for Qt6
    // In Qt6, we need to ensure icon theme is properly set
    // First set the search paths
    QStringList iconThemePaths;
    iconThemePaths << "/usr/share/icons";
    iconThemePaths << QDir::homePath() + "/.local/share/icons";
    iconThemePaths << "/usr/local/share/icons";
    QIcon::setThemeSearchPaths(iconThemePaths);
    
    // Try to set icon theme in order of preference
    QStringList preferredThemes = {"cutefish", "Crule", "Crule-dark", "breeze", "Adwaita", "hicolor"};
    QString themeSet = "hicolor"; // default fallback
    
    for (const QString &theme : preferredThemes) {
        QString themePath = QString("/usr/share/icons/%1").arg(theme);
        if (QDir(themePath).exists()) {
            themeSet = theme;
            break;
        }
    }
    
    QIcon::setThemeName(themeSet);
    qDebug() << "StatusBar: Icon theme set to:" << QIcon::themeName() << "from search paths:" << QIcon::themeSearchPaths();
    
    // Ensure QIcon image provider is available for QML
    // This is needed for image://icontheme/ URLs to work
    if (QIcon::themeName().isEmpty()) {
        qWarning() << "StatusBar: No icon theme set! image://icontheme/ URLs will not work.";
    }

    const char *uri = "Cutefish.StatusBar";
    qmlRegisterType<SystemTrayModel>(uri, 1, 0, "SystemTrayModel");
    qmlRegisterType<ControlCenterDialog>(uri, 1, 0, "ControlCenterDialog");
    qmlRegisterType<Appearance>(uri, 1, 0, "Appearance");
    qmlRegisterType<Brightness>(uri, 1, 0, "Brightness");
    qmlRegisterType<Battery>(uri, 1, 0, "Battery");
    qmlRegisterType<AppMenuModel>(uri, 1, 0, "AppMenuModel");
    qmlRegisterType<AppMenuApplet>(uri, 1, 0, "AppMenuApplet");
    qmlRegisterType<PowerActions>(uri, 1, 0, "PowerActions");
    qmlRegisterType<Notifications>(uri, 1, 0, "Notifications");
    qmlRegisterType<BackgroundHelper>(uri, 1, 0, "BackgroundHelper");

    QString qmFilePath = QString("%1/%2.qm").arg("/usr/share/cutefish-statusbar/translations/").arg(QLocale::system().name());
    if (QFile::exists(qmFilePath)) {
        QTranslator *translator = new QTranslator(QApplication::instance());
        if (translator->load(qmFilePath)) {
            QGuiApplication::installTranslator(translator);
        } else {
            translator->deleteLater();
        }
    }

    StatusBar bar;

    if (!QDBusConnection::sessionBus().registerService("com.cutefish.Statusbar")) {
        return -1;
    }

    if (!QDBusConnection::sessionBus().registerObject("/Statusbar", &bar)) {
        return -1;
    }

    return app.exec();
}
