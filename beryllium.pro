# Copyright (C) 2013-2018 Softus Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; version 2.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

isEmpty(PREFIX): PREFIX   = /usr/local
DEFINES += PREFIX=$$PREFIX
CONFIG  += c++11

win32 {
    QT           += gui-private
    LIBS         += -ladvapi32 -lnetapi32 -luser32 -lwsock32
    INCLUDEPATH  += c:/usr/include
    QMAKE_LIBDIR += c:/usr/lib

    USERNAME    = $$(USERNAME)
    OS_DISTRO   = Windows
    OS_REVISION = $$system($$quote('cmd.exe /q /c "for /f "tokens=4-5 delims=. " %i in (\'ver\') do echo %i.%j"'))
} linux {
    LIBS       += -lX11

    USERNAME    = $$(USER)
    OS_DISTRO   = $$system($$quote('lsb_release -is | cut -d" " -f1'))
    OS_REVISION = $$system(lsb_release -rs)
} macx {
    USERNAME    = $$(LOGNAME)
    OS_DISTRO   = MacOSX
    OS_REVISION = $$system(sw_vers -productVersion)
}

DEFINES += OS_DISTRO=$$OS_DISTRO OS_REVISION=$$OS_REVISION USERNAME=$$USERNAME

# Tell qmake to use pkg-config to find QtGStreamer.
CONFIG += link_pkgconfig
PKGCONFIG += gio-2.0

OPTIONAL_MODULES = dbus opengl widgets x11extras
for (mod, OPTIONAL_MODULES): qtHaveModule($$mod) {
    QT += $$mod
    DEFINES += WITH_QT_$$upper($$mod)
}

OPTIONAL_LIBS = libavc1394 libraw1394 libv4l2
for (mod, OPTIONAL_LIBS) {
  modVer = $$system(pkg-config --silence-errors --modversion $$mod)
  !isEmpty(modVer) {
    message("Found $$mod version $$modVer")
    PKGCONFIG += $$mod
    DEFINES += WITH_$$upper($$replace(mod, \W, _))
  }
}

PKGCONFIG += Qt5GLib-2.0 Qt5GStreamer-1.0 Qt5GStreamerUi-1.0 \
    gstreamer-1.0 gstreamer-base-1.0 gstreamer-pbutils-1.0

TARGET   = beryllium
TEMPLATE = app

# Produce nice compilation output
# CONFIG += silent

SOURCES += \
    src/aboutdialog.cpp \
    src/archivewindow.cpp \
    src/beryllium.cpp \
    src/darkthemestyle.cpp \
    src/hotkeyedit.cpp \
    src/mainwindow.cpp \
    src/mandatoryfieldgroup.cpp \
    src/patientdatadialog.cpp \
    src/smartshortcut.cpp \
    src/sound.cpp \
    src/thumbnaillist.cpp \
    src/touch/clickfilter.cpp \
    src/touch/slidingstackedwidget.cpp \
    src/typedetect.cpp \
    src/videoeditor.cpp \
    src/videoencodingprogressdialog.cpp \
    src/settingsdialog.cpp \
    src/pipeline.cpp \
    src/videowidget.cpp \
    src/settings/videosources.cpp \
    src/settings/videosourcedetails.cpp \
    src/settings/videorecord.cpp \
    src/settings/studies.cpp \
    src/settings/storage.cpp \
    src/settings/physicians.cpp \
    src/settings/mandatoryfields.cpp \
    src/settings/hotkeys.cpp \
    src/settings/debug.cpp \
    src/settings/confirmations.cpp \
    src/settings/elementproperties.cpp

win32 {
    SOURCES += \
        src/smartshortcut_win.cpp
} linux {
    SOURCES += \
        src/smartshortcut_x11.cpp
} macx {
    SOURCES += \
        src/smartshortcut_mac.cpp
}

contains(QT, dbus): {
    SOURCES += \
        src/dbusconnect.cpp \
        src/mainwindowdbusadaptor.cpp
    HEADERS += \
        src/dbusconnect.h \
        src/mainwindowdbusadaptor.h
}

HEADERS += \
    src/aboutdialog.h \
    src/archivewindow.h \
    src/comboboxwithpopupsignal.h \
    src/darkthemestyle.h \
    src/defaults.h \
    src/hotkeyedit.h \
    src/mainwindow.h \
    src/mandatoryfieldgroup.h \
    src/patientdatadialog.h \
    src/product.h \
    src/qwaitcursor.h \
    src/smartshortcut.h \
    src/sound.h \
    src/thumbnaillist.h \
    src/touch/clickfilter.h \
    src/touch/slidingstackedwidget.h \
    src/touch/slidingstackedwidget_p.h \
    src/typedetect.h \
    src/videoeditor.h \
    src/videoencodingprogressdialog.h \
    src/settingsdialog.h \
    src/pipeline.h \
    src/platform.h \
    src/videowidget.h \
    src/settings/videosources.h \
    src/settings/videosourcedetails.h \
    src/settings/confirmations.h \
    src/settings/debug.h \
    src/settings/hotkeys.h \
    src/settings/mandatoryfields.h \
    src/settings/physicians.h \
    src/settings/storage.h \
    src/settings/studies.h \
    src/settings/videorecord.h \
    src/settings/elementproperties.h

FORMS   +=

RESOURCES    += \
    beryllium.qrc
TRANSLATIONS += beryllium_ru.ts

linux {
    INSTALLS          += target
    target.path        = $$PREFIX/bin

    shortcut.files     = beryllium.desktop
    shortcut.path      = $$PREFIX/share/applications
    icon.files         = pixmaps/beryllium.png
    icon.path          = $$PREFIX/share/icons
    man.files          = beryllium.1
    man.path           = $$PREFIX/share/man/man1
    translations.files = beryllium_ru.qm
    translations.path  = $$PREFIX/share/beryllium/translations
    sound.files        = sound/*
    sound.path         = $$PREFIX/share/beryllium/sound
    INSTALLS          += translations sound shortcut icon man

    contains(QT, dbus): {
        dbus.files     = org.softus.beryllium.service
        dbus.path      = $$PREFIX/share/dbus-1/services
        INSTALLS      += dbus
    }
}

include (src/dicom/dicom.pri)
include (libqxt/libqxt.pri)
