Beryllium
=========

[![Build Status](https://api.travis-ci.org/Softus/beryllium.svg?branch=master)](https://travis-ci.org/Softus/beryllium)

Introduction
============

Beryllium is a cross-platform video logging software written in c++.
It is capable of video capturing from multiple sources. With many other
features, including: real-time video broadcast via HTTP/RTP/UDP; DICOM
support, including worklist and saving images/videos in the storage
servers; basic video editing.

Requirements
============

* [Qt](http://qt-project.org/) 5.2 or higher;
* [GStreamer](http://gstreamer.freedesktop.org/) 1.6 or higher;
* [QtGstreamer](http://gstreamer.freedesktop.org/modules/qt-gstreamer.html) 1.2 or higher;
* [DCMTK](http://dcmtk.org/) 3.6.0 or higher;
* [MediaInfo](http://mediainfo.sourceforge.net/) 0.7.73 or higher.

Installation
============

Debian/Ubuntu/Mint
------------------

1. Install build dependecies

        sudo apt install libdcmtk2-dev libboost-dev libmediainfo-dev \
        libwrap0-dev libgudev-1.0-dev libgstreamer1.0-dev libv4l-dev \
        libgstreamer-plugins-base1.0-dev libavc1394-dev libraw1394-dev \
        libqt5x11extras5-dev libqt5gstreamer-1.0-0 libqt5gstreamer-dev \
        qttools5-dev-tools libqt5opengl5-dev qt5-default

2. Make Makefile

        qmake CONFIG+=dicom beryllium.pro
  
3. Make Beryllium

        lrelease *.ts
        make

4. Install Beryllium

        sudo make install

SUSE/Open SUSE
--------------

1. Install build dependecies

        sudo zypper install make rpm-build qt-devel qt-gstreamer-devel \
        libQtGlib-devel dcmtk-devel tcp_wrappers-devel libgudev-devel \
        mediainfo-devel gstreamer-devel gstreamer-plugins-qt5-devel \
        libqt5-qtbase-devel libavc-1394-devel  

2. Make Makefile

        qmake-qt5 CONFIG+=dicom beryllium.pro

3. Make Beryllium

        lrelease *.ts
        make

4. Install Beryllium

        sudo make install


SUSE/Open SUSE
--------------

1. Install build dependecies

        sudo zypper install make rpm-build qt-devel qt-gstreamer-devel \
        libQtGlib-devel dcmtk-devel tcp_wrappers-devel libgudev-devel \
        mediainfo-devel gstreamer-devel gstreamer-plugins-qt5-devel \
        libqt5-qtbase-devel libavc-1394-devel libv4l-devel

2. Make Makefile

        qmake-qt5 CONFIG+=dicom beryllium.pro

3. Make Beryllium

        lrelease *.ts
        make

4. Install Beryllium

        sudo make install

CentOS
--------------

1. Install build dependecies

        sudo yum install make rpm-build gstreamer-devel libv4l-devel \
        qt-devel libgudev1-devel libavc1394-devel libmediainfo-devel \
        dcmtk-devel openssl-devel

2. Make Makefile

        qmake-qt5 CONFIG+=dicom beryllium.pro

3. Make Beryllium

        lrelease *.ts
        make

4. Install Beryllium

        sudo make install

Fedora
--------------

1. Install build dependecies

        sudo dnf install make rpm-build gstreamer-devel libv4l-devel \
        qt-devel qt5-gstreamer-devel libgudev-devel libavc1394-devel \
        libmediainfo-devel dcmtk-devel openssl-devel


2. Make Makefile

        qmake-qt5 CONFIG+=dicom beryllium.pro

3. Make Beryllium

        lrelease *.ts
        make

4. Install Beryllium

        sudo make install


Windows (Visual Studio)
--------------

1. Install build dependecies

  * [pkg-config](http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/)
  * [gstreamer, sgtreamer-sdk](https://gstreamer.freedesktop.org/data/pkg/windows/)
  * [boost](https://sourceforge.net/projects/boost/files/boost/)

2. Build 3-rd party libraries
  * [QtGStreamer](https://github.com/detrout/qt-gstreamer.git)

        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -DQT_VERSION=5  -DBoost_INCLUDE_DIR=<path to boost> -G "Visual Studio <version>"
        cmake --build . --target install

3. Make Makefile

        qmake-qt5 -set INCLUDEDIR %BOOSTDIR% beryllium.pro

4. Make Beryllium

        lrelease *.ts
        nmake -f Makefile.Release

Note that both the GStreamer & Qt must be built with exactly the same
version of the MSVC. For example, if GStreamer is build with MSVC 2010,
the Qt version should must be any from 5.0 till 5.5.

Windows (MinGW)
--------------

1. Install build dependecies

  * [gstreamer, sgtreamer-sdk](https://gstreamer.freedesktop.org/data/pkg/windows/)
  * [boost](https://sourceforge.net/projects/boost/files/boost/)

2. Build 3-rd party libraries
  * [QtGStreamer](https://github.com/detrout/qt-gstreamer.git)

        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -DQT_VERSION=5  -DBoost_INCLUDE_DIR=<path to boost> -G "MinGW Makefiles"
        cmake --build . --target install

3. Make Makefile

        qmake-qt5 -set INCLUDEDIR %BOOSTDIR% beryllium.pro

4. Make Beryllium

        lrelease *.ts
        min32gw-make -f Makefile.Release

Note that both the GStreamer & Qt must be built with exactly the same
version of the GCC. For example, if GStreamer is build with GCC 4.7.3,
the Qt version should must be 5.0.
