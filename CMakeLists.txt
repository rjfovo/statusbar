cmake_minimum_required(VERSION 3.16...3.22)
project(statusbar LANGUAGES CXX)

# 设置Qt6标准项目配置
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt6包
find_package(Qt6 REQUIRED COMPONENTS Core Widgets DBus LinguistTools Quick QuickControls2 Concurrent)
# 应用Qt标准项目设置 
qt_standard_project_setup()

# 查找KF6包 - 可能需要根据实际模块调整名称
find_package(KF6WindowSystem REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB_LIBS REQUIRED xcb)

set(SRCS
    src/main.cpp
    src/statusbar.cpp
    src/controlcenterdialog.cpp
    src/brightness.cpp
    src/battery.cpp
    src/appearance.cpp
    src/processprovider.cpp
    src/activity.cpp
    src/capplications.cpp
    src/notifications.cpp
    src/backgroundhelper.cpp

    src/systemtray/statusnotifieritemsource.cpp
    src/systemtray/systemtraytypes.cpp
    src/systemtray/systemtraytypedefs.h
    src/systemtray/systemtraymodel.cpp
    src/systemtray/statusnotifierwatcher.cpp
    src/systemtray/statusnotifieritemhost.cpp

    src/libdbusmenuqt/dbusmenuimporter.cpp
    src/libdbusmenuqt/dbusmenushortcut_p.cpp
    src/libdbusmenuqt/dbusmenutypes_p.cpp
    src/libdbusmenuqt/utils.cpp

    src/appmenu/kdbusimporter.h
    src/appmenu/appmenu.h
    src/appmenu/appmenu.cpp
    src/appmenu/appmenudbus.h
    src/appmenu/appmenudbus.cpp
    src/appmenu/verticalmenu.h
    src/appmenu/verticalmenu.cpp
    src/appmenu/menuimporter.h
    src/appmenu/menuimporter.cpp
    src/appmenu/appmenumodel.h
    src/appmenu/appmenumodel.cpp
    src/appmenu/appmenuapplet.h
    src/appmenu/appmenuapplet.cpp
    src/appmenu/dbusmenu_interface.cpp
    src/appmenu/dbusmenu_interface.h

    src/poweractions.cpp

    qml.qrc
)

# DBus接口处理 - 使用Qt6版本的命令
set(statusnotifierwatcher_xml src/systemtray/org.kde.StatusNotifierWatcher.xml)
qt6_add_dbus_interface(SRCS ${statusnotifierwatcher_xml} statusnotifierwatcher_interface)

qt6_add_dbus_interface(SRCS src/systemtray/org.freedesktop.DBus.Properties.xml dbusproperties)

set(statusnotifieritem_xml src/systemtray/org.kde.StatusNotifierItem.xml)
set_source_files_properties(${statusnotifieritem_xml} PROPERTIES
   NO_NAMESPACE false
   INCLUDE "src/systemtray/systemtraytypes.h"
   CLASSNAME OrgKdeStatusNotifierItem
)
qt6_add_dbus_interface(SRCS ${statusnotifieritem_xml} statusnotifieritem_interface)

qt6_add_dbus_adaptor(SRCS src/systemtray/org.kde.StatusNotifierWatcher.xml
                     src/systemtray/statusnotifierwatcher.h StatusNotifierWatcher)

# libdbusmenuqt
set_source_files_properties(src/libdbusmenuqt/com.canonical.dbusmenu.xml PROPERTIES
   NO_NAMESPACE true
   INCLUDE "src/libdbusmenuqt/dbusmenutypes_p.h"
   CLASSNAME DBusMenuInterface
)

# appmenu - 使用Qt6版本的命令
qt6_add_dbus_adaptor(appmenu_SRCS src/appmenu/com.canonical.AppMenu.Registrar.xml
    src/appmenu/menuimporter.h MenuImporter menuimporteradaptor MenuImporterAdaptor)
    
qt6_add_dbus_adaptor(appmenu_SRCS src/appmenu/com.cutefish.cappmenu.xml
    src/appmenu/appmenudbus.h AppmenuDBus appmenuadaptor AppmenuAdaptor)

## statusbar dbus
qt6_add_dbus_adaptor(DBUS_SOURCES
                     src/com.cutefish.Statusbar.xml
                     src/statusbar.h StatusBar)

add_executable(cutefish-statusbar ${SRCS} ${appmenu_SRCS} ${DBUS_SOURCES})

target_include_directories(cutefish-statusbar PUBLIC ${XCB_LIBS_INCLUDE_DIRS})
target_link_libraries(cutefish-statusbar
  PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Quick
  Qt6::QuickControls2
  Qt6::Concurrent
  Qt6::DBus
  KF6::WindowSystem
  ${XCB_LIBS_LIBRARIES}
)

# 修复翻译处理部分
file(GLOB TS_FILES translations/*.ts)

# 方法1: 使用推荐的现代方法 - 修正参数顺序
qt6_add_translations(cutefish-statusbar TS_FILES ${TS_FILES})

# 或者方法2: 使用兼容方法（如果上面方法仍有问题）
# set(TRANSLATION_TARGET cutefish-statusbar)
# qt6_create_translation(QM_FILES ${TS_FILES} ${TRANSLATION_TARGET})

# 如果使用方法2，需要取消注释以下行：
# add_custom_target(translations ALL DEPENDS ${QM_FILES})
# add_dependencies(cutefish-statusbar translations)

install(TARGETS cutefish-statusbar RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# 安装翻译文件 - 需要先确保生成了QM文件
# 如果使用方法2，取消注释以下行：
# install(FILES ${QM_FILES} DESTINATION /usr/share/cutefish-statusbar/translations)

# 如果使用方法1，使用以下安装方式：
qt6_add_translations(cutefish-statusbar TS_FILES ${TS_FILES} QM_FILES_OUTPUT_VARIABLE QM_FILES)
install(FILES ${QM_FILES} DESTINATION /usr/share/cutefish-statusbar/translations)