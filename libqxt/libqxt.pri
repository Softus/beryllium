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

INCLUDEPATH += $$PWD
DEFINES     += BUILD_QXT_CORE QXT_STATIC

HEADERS += $$PWD/qxtglobal.h \
    $$PWD/qxtmetaobject.h \
    $$PWD/qxtnamespace.h \
    $$PWD/qxtcheckcombobox.h \
    $$PWD/qxtcheckcombobox_p.h \
    $$PWD/qxtconfirmationmessage.h \
    $$PWD/qxtlineedit.h \
    $$PWD/qxtspanslider.h \
    $$PWD/qxtspanslider_p.h

SOURCES += $$PWD/qxtcheckcombobox.cpp \
    $$PWD/qxtconfirmationmessage.cpp \
    $$PWD/qxtlineedit.cpp \
    $$PWD/qxtspanslider.cpp
