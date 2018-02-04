Beryllium
=========

[![Buddy Pipeline](https://app.buddy.works/pbludov/beryllium/pipelines/pipeline/124168/badge.svg?token=bf26fe8fed990190f11227bb2aa0c7d1e71118737795eed7b5069fff7106a015)](https://app.buddy.works/pbludov/beryllium/pipelines/pipeline/124168)
[![Build Status](https://api.travis-ci.org/Softus/beryllium.svg?branch=master)](https://travis-ci.org/Softus/beryllium)
[![Build status](https://ci.appveyor.com/api/projects/status/ae09iex64d3bfgar?svg=true)](https://ci.appveyor.com/project/pbludov/beryllium)
[![PPA](https://img.shields.io/badge/PPA-available-green.svg)](https://launchpad.net/~softus-team/+archive/ubuntu/ppa)

Introduction
============

Beryllium is a cross-platform video logging software written in c++.
It is capable of video capturing from multiple sources. With many other
features, including: real-time video broadcast via HTTP/RTP/UDP; DICOM
support, including worklist and saving images/videos in the storage
servers; basic video editing.

Requirements
============

* [Qt](http://qt-project.org/) 5.0.2 or higher;
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

        qmake beryllium.pro
  
3. Make Beryllium

        lrelease *.ts
        make

4. Install Beryllium

        sudo make install

5. Create Package

        cp docs/* debian/
        dpkg-buildpackage -us -uc -I.git -I*.sh -rfakeroot

SUSE/Open SUSE
--------------

1. Install build dependecies

        sudo zypper install make rpm-build qt-devel qt-gstreamer-devel \
        libQtGlib-devel dcmtk-devel tcp_wrappers-devel libgudev-devel \
        mediainfo-devel gstreamer-devel gstreamer-plugins-qt5-devel \
        libqt5-qtbase-devel libavc-1394-devel libv4l-devel

2. Make Makefile

        qmake-qt5 beryllium.pro

3. Make Beryllium

        lrelease *.ts
        make

4. Install Beryllium

        sudo make install

5. Create Package

        distro=$( lsb_release -is | awk '{print tolower($1)}' )
        rev=$( lsb_release -rs )
        tar czf ../beryllium.tar.gz * --exclude=.git --exclude=*.sh && rpmbuild -D"dicom 1 -D"distro $distro" -D"rev $rev" -ta ../beryllium.tar.gz

CentOS
------

1. Install build dependecies

        sudo yum install make rpm-build gstreamer-devel libv4l-devel \
        qt-devel libgudev1-devel libavc1394-devel libmediainfo-devel \
        dcmtk-devel openssl-devel

2. Make Makefile

        qmake-qt5 beryllium.pro

3. Make Beryllium

        lrelease-qt5 *.ts
        make

4. Install Beryllium

        sudo make install

5. Create Package

        distro=$( lsb_release -is | awk '{print tolower($1)}' )
        rev=$( lsb_release -rs )
        tar czf ../beryllium.tar.gz * --exclude=.git --exclude=*.sh && rpmbuild -D"dicom 1 -D"distro $distro" -D"rev $rev" -ta ../beryllium.tar.gz

Fedora
------

1. Install build dependecies

        sudo dnf install make rpm-build gstreamer-devel libv4l-devel \
        qt-devel qt5-gstreamer-devel qt5-linguist libgudev-devel libavc1394-devel \
        libmediainfo-devel dcmtk-devel openssl-devel tcp_wrappers-devel gcc-c++ 

2. Make Makefile

        qmake-qt5 beryllium.pro

3. Make Beryllium

        lrelease-qt5 *.ts
        make

4. Install Beryllium

        sudo make install

5. Create Package

        distro=$( lsb_release -is | awk '{print tolower($1)}' )
        rev=$( lsb_release -rs )
        tar czf ../beryllium.tar.gz * --exclude=.git --exclude=*.sh && rpmbuild -D"dicom 1" -D"distro $distro" -D"rev $rev" -ta ../beryllium.tar.gz

Windows (Visual Studio)
-----------------------

1. Install build dependecies

  * [CMake](https://cmake.org/download/)
  * [WiX Toolset](http://wixtoolset.org/releases/)
  * [pkg-config](http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/)
  * [GStreamer, GStreamer-sdk](https://gstreamer.freedesktop.org/data/pkg/windows/)
  * [Boost](https://sourceforge.net/projects/boost/files/boost/)
  * [Qt 5.5 MSVC](https://download.qt.io/archive/qt/5.5/)
  * [QtGStreamer](https://github.com/detrout/qt-gstreamer.git)
  * [DCMTK (optional)](http://dcmtk.org/dcmtk.php.en)
  * [MediaInfo (optional)](http://mediaarea.net/ru/MediaInfo/Download/Source)

2. Build 3-rd party libraries

        # MediaInfo (optional)
        cd libmediainfo/mediainfolib/project/cmake
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -G "Visual Studio <version>"
        cmake --build . --target install

        # DCMTK (optional)
        cd dcmtk
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -G "Visual Studio <version>"
        cmake --build . --target install

        # QtGStreamer
        set BOOST_DIR=<the path to boost headers>
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -DQT_VERSION=5  -DBoost_INCLUDE_DIR=%BOOST_DIR% -G "Visual Studio <version>"
        cmake --build . --target install

3. Make Makefile

        qmake-qt5 INCLUDEDIR+=%BOOST_DIR%

4. Make Beryllium

        lrelease-qt5 *.ts
        nmake -f Makefile.Release

5. Create Package

        copy \usr\bin\*.dll release\
        copy C:\Qt\Qt5.5.1\5.5.1\msvc2010\bin\*.dll Release\
        xcopy /s C:\Qt5.5.1\5.5.1\msvc2010\plugins Release\

        set QT_RUNTIME=QtRuntime32.wxi
        wix\build.cmd

Note that both the GStreamer & Qt must be built with exactly the same
version of the MSVC. For example, if GStreamer is build with MSVC 2010,
the Qt version should must be any from 5.0 till 5.5.

Windows (MinGW)
---------------

1. Install build dependecies

  * [CMake](https://cmake.org/download/)
  * [WiX Toolset](http://wixtoolset.org/releases/)
  * [GStreamer, GStreamer-sdk](https://gstreamer.freedesktop.org/data/pkg/windows/)
  * [Boost](https://sourceforge.net/projects/boost/files/boost/)
  * [Qt 5.0.2 MinGW](https://download.qt.io/archive/qt/5.0/5.0.2/)
  * [DCMTK](http://dcmtk.org/dcmtk.php.en)
  * [MediaInfo (optional)](http://mediaarea.net/ru/MediaInfo/Download/Source)
  * [QtGStreamer (optional)](https://github.com/detrout/qt-gstreamer.git)

2. Build 3-rd party libraries

        # MediaInfo (optional)
        cd libmediainfo/mediainfolib/project/cmake
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -G "MinGW Makefiles"
        cmake --build . --target install

        # DCMTK (optional)
        cd dcmtk
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -G "MinGW Makefiles"
        cmake --build . --target install

        # QtGStreamer
        set BOOST_DIR=<the path to boost headers>
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -DQT_VERSION=5  -DBoost_INCLUDE_DIR=%BOOST_DIR% -G "MinGW Makefiles"
        cmake --build . --target install

3. Make Makefile

        qmake-qt5 INCLUDEDIR+=%BOOST_DIR%

4. Make Beryllium

        lrelease-qt5 *.ts
        min32gw-make -f Makefile.Release

5. Create Package

        copy \usr\bin\*.dll release\
        copy C:\Qt\Qt5.0.2\5.0.2\mingw47_32\bin\*.dll Release\
        xcopy /s C:\Qt\Qt5.0.2\5.0.2\mingw47_32\plugins Release\

        set QT_RUNTIME=qt-5.0.2_mingw-4.7.wxi
        wix\build.cmd


Note that both the GStreamer & Qt must be built with exactly the same
version of the GCC. For example, if GStreamer is build with GCC 4.7.3,
the Qt version should must be 5.0.
