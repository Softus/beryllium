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

isEmpty(nodicom) {
    QT         += network
    DEFINES    += WITH_DICOM

    # libmediainfo.pc adds UNICODE, but dcmtk isn't compatible with wchar,
    # so we can't use pkgconfig for this library.
    LIBS       += -lmediainfo -lzen

    # DCMTK isn't pgk-config compatible at all.
    LIBS       += -ldcmnet -ldcmdata -loflog -lofstd
    win32 {
        LIBS += -lzlibstatic -lnetapi32 -liphlpapi -lcharset -lws2_32
    } linux {
        LIBS += -lssl -lz
    } macx {
        LIBS += -lcharset -liconv -lz
    }

    SOURCES   += \
        src/dicom/dcmclient.cpp \
        src/dicom/dcmconverter.cpp \
        src/dicom/detailsdialog.cpp \
        src/dicom/dicomdevicesettings.cpp \
        src/dicom/dicommppsmwlsettings.cpp \
        src/dicom/dicomserverdetails.cpp \
        src/dicom/dicomserversettings.cpp \
        src/dicom/dicomstoragesettings.cpp \
        src/dicom/transcyrillic.cpp \
        src/dicom/worklistcolumnsettings.cpp \
        src/dicom/worklist.cpp \
        src/dicom/worklistquerysettings.cpp

    HEADERS += \
        src/dicom/dcmclient.h \
        src/dicom/dcmconverter.h \
        src/dicom/detailsdialog.h \
        src/dicom/dicomdevicesettings.h \
        src/dicom/dicommppsmwlsettings.h \
        src/dicom/dicomserverdetails.h \
        src/dicom/dicomserversettings.h \
        src/dicom/dicomstoragesettings.h \
        src/dicom/transcyrillic.h \
        src/dicom/worklistcolumnsettings.h \
        src/dicom/worklist.h \
        src/dicom/worklistquerysettings.h
}
