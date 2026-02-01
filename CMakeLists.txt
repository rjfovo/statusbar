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

# 查找libcutefish
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB_LIBS REQUIRED xcb)

# libcutefish没有提供CMake配置文件，我们直接链接库
# 设置libcutefish库路径
set(LIBCUTEFISH_LIBRARIES cutefishnm_qmlplugins cutefishsystem_qmlplugins)

# 添加libcutefish库的搜索路径
set(LIBCUTEFISH_LIB_DIRS 
    ${CMAKE_SOURCE_DIR}/../libcutefish/obj-x86_64-linux-gnu/networkmanagement
    ${CMAKE_SOURCE_DIR}/../libcutefish/obj-x86_64-linux-gnu/system
)

# 添加库搜索路径
link_directories(${LIBCUTEFISH_LIB_DIRS})

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

# 首先创建一个QML模块库
qt_add_qml_module(cutefish-statusbar-qml
    URI "cutefish.statusbar"
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cutefish/statusbar
    QML_FILES
        # 如果有QML文件，可以在这里添加
    SOURCES
        ${SRCS}
        ${appmenu_SRCS}
        ${DBUS_SOURCES}
)

target_include_directories(cutefish-statusbar-qml PUBLIC ${XCB_LIBS_INCLUDE_DIRS})
target_link_libraries(cutefish-statusbar-qml
  PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Quick
  Qt6::QuickControls2
  Qt6::Concurrent
  Qt6::DBus
  KF6::WindowSystem
  ${LIBCUTEFISH_LIBRARIES}
  ${XCB_LIBS_LIBRARIES}
)

# 然后创建可执行文件
add_executable(cutefish-statusbar src/main.cpp)

target_include_directories(cutefish-statusbar PUBLIC ${XCB_LIBS_INCLUDE_DIRS})
target_link_libraries(cutefish-statusbar
  PRIVATE
  cutefish-statusbar-qml
  Qt6::Core
  Qt6::Widgets
  Qt6::Quick
  Qt6::QuickControls2
  Qt6::Concurrent
  Qt6::DBus
  KF6::WindowSystem
  ${LIBCUTEFISH_LIBRARIES}
  ${XCB_LIBS_LIBRARIES}
)

# 修复翻译处理部分
file(GLOB TS_FILES translations/*.ts)

# 为QML模块添加翻译
qt6_add_translations(cutefish-statusbar-qml TS_FILES ${TS_FILES})

install(TARGETS cutefish-statusbar RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# 安装QML库文件到系统库目录
install(TARGETS cutefish-statusbar-qml
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# 获取Qt6的QML安装路径
if (NOT DEFINED INSTALL_QMLDIR OR INSTALL_QMLDIR STREQUAL "")
    set(INSTALL_QMLDIR "${CMAKE_INSTALL_LIBDIR}/qt6/qml")
endif()

# 安装QML模块
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cutefish/statusbar
        DESTINATION ${INSTALL_QMLDIR}/cutefish)

# 修复qrc文件中的绝对路径问题
# 在安装后修复qrc文件，将绝对路径替换为相对路径
install(CODE "
    message(STATUS \"Fixing qrc file paths...\")
    file(GLOB_RECURSE QRC_FILES \"\${CMAKE_INSTALL_PREFIX}/${INSTALL_QMLDIR}/cutefish/statusbar/*.qrc\")
    foreach(QRC_FILE \${QRC_FILES})
        file(READ \${QRC_FILE} QRC_CONTENT)
        # 替换绝对路径为相对路径
        # 匹配格式: <file alias=\"/cutefish/statusbar\">/workspace/cutefish_project/cutefish/code/statusbar/obj-x86_64-linux-gnu/cutefish/statusbar</file>
        # 注意：CMake的正则表达式需要正确转义
        string(REGEX REPLACE \"<file alias=\\\"/cutefish/statusbar\\\">[^<]*</file>\"
               \"<file alias=\\\"/cutefish/statusbar\\\">cutefish/statusbar</file>\"
               QRC_CONTENT_FIXED \${QRC_CONTENT})
        if(NOT \"\${QRC_CONTENT}\" STREQUAL \"\${QRC_CONTENT_FIXED}\")
            file(WRITE \${QRC_FILE} \${QRC_CONTENT_FIXED})
            message(STATUS \"Fixed qrc file: \${QRC_FILE}\")
        else()
            message(STATUS \"No changes needed for qrc file: \${QRC_FILE}\")
        endif()
    endforeach()
")
