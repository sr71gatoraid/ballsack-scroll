# SPDX-FileCopyrightText: 2026 KWin AutoScroll contributors
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.24)

project(kwin-smooth-autoscroll VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.7.0")
set(KF_MIN_VERSION "6.0.0")
set(KWIN_MIN_VERSION "6.7.5")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(CTest)
include(ECMDeprecationSettings)
include(ECMQtDeclareLoggingCategory)
include(KDEInstallDirs6)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Gui Svg Widgets)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Config
    ConfigWidgets
    CoreAddons
    I18n
    KCMUtils
    WidgetsAddons
)
find_package(KWin ${KWIN_MIN_VERSION} REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_subdirectory(src)
add_subdirectory(kcm)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
