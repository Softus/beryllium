Beryllium
=========

[![Buddy Pipeline](https://app.buddy.works/pbludov/beryllium/pipelines/pipeline/124169/badge.svg?token=bf26fe8fed990190f11227bb2aa0c7d1e71118737795eed7b5069fff7106a015)](https://app.buddy.works/pbludov/beryllium/pipelines/pipeline/124169)
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

* Build dependecies

        sudo apt install lsb-release debhelper fakeroot libdcmtk2-dev libboost-dev \
          libmediainfo-dev libwrap0-dev libqt5opengl5-dev libgstreamer1.0-dev \
          libgstreamer-plugins-base1.0-dev libqt5gstreamer-1.0-0 libqt5gstreamer-dev \
          libavc1394-dev libraw1394-dev libv4l-dev qttools5-dev-tools qt5-default libqt5x11extras5-dev

* Make Beryllium from the source

        lrelease *.ts
        qmake beryllium.pro
        make
        sudo make install

* Create Package

        cp docs/* debian/
        dpkg-buildpackage -us -uc -tc -I.git -I*.yml -rfakeroot

SUSE/Open SUSE
--------------

* Build dependecies

        sudo zypper install lsb-release rpm-build make libqt5-linguist libqt5-qtbase-devel \
          gstreamer-plugins-qt5-devel dcmtk-devel libmediainfo-devel libqt5-qtx11extras-devel \
          openssl-devel libavc1394-devel libv4l-devel

* Make Beryllium from the source

        lrelease-qt5 *.ts
        qmake-qt5 beryllium.pro
        make
        sudo make install

* Create Package

        tar czf ../beryllium.tar.gz --exclude=debian --exclude=*.yml *
        rpmbuild -ta ../beryllium.tar.gz

CentOS
------

* Build dependecies

        sudo yum install epel-release
        sudo yum update
        sudo yum install redhat-lsb rpm-build git make cmake gcc-c++ boost-devel \
          gstreamer1-plugins-base-devel qt5-qtdeclarative-devel gstreamer1-devel \
          libv4l-devel qt5-qtbase-devel qt5-linguist qt5-qtx11extras-devel libavc1394-devel \
          libmediainfo-devel openssl-devel

* Build 3-rd party libraries

        # qt-gstreamer
        git clone https://anongit.freedesktop.org/git/gstreamer/qt-gstreamer.git
        cd qt-gstreamer
        mkdir build
        cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=/usr -DQT_VERSION=5
        sudo cmake --build . --target install
        cd ../..

        # DCMTK (optional)
        git clone https://github.com/DCMTK/dcmtk.git -b DCMTK-3.6.3
        cd dcmtk
        mkdir build
        cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=/usr -DDCMTK_WITH_OPENSSL=OFF -DDCMTK_WITH_WRAP=OFF -DDCMTK_WITH_ICU=OFF -DDCMTK_WITH_ICONV=OFF
        sudo cmake --build . --target install
        cd ../..

* Make Beryllium from the source

        lrelease-qt5 *.ts
        qmake-qt5 beryllium.pro
        make
        sudo make install

* Create Package

        tar czf ../beryllium.tar.gz --exclude=debian --exclude=dcmtk --exclude=qt-gstreamer --exclude=*.yml *
        rpmbuild -ta ../beryllium.tar.gz

Fedora
------

* Build dependecies

        sudo dnf install redhat-lsb rpm-build make gstreamer1-devel libv4l-devel \
          qt5-qtbase-devel qt5-gstreamer-devel qt5-linguist qt5-qtx11extras-devel \
          libavc1394-devel libmediainfo-devel dcmtk-devel openssl-devel gcc-c++

* Make Beryllium from the source

        lrelease-qt5 *.ts
        qmake-qt5 beryllium.pro
        make
        sudo make install

* Create Package

        tar czf ../beryllium.tar.gz --exclude=debian --exclude=*.yml *
        rpmbuild -ta ../beryllium.tar.gz

Mageia
------

* Build dependecies

        sudo dnf install lsb-release rpm-build git make cmake gcc-c++ qttools5 \
          lib64avc1394-devel lib64mediainfo-devel lib64qt5-gstreamer-devel lib64boost-devel \
          lib64gstreamer1.0-devel lib64gstreamer-plugins-base1.0-devel lib64qt5base5-devel \
          lib64qt5x11extras-devel lib64v4l-devel

* Build 3-rd party libraries

        # DCMTK (optional)
        git clone https://github.com/DCMTK/dcmtk.git -b DCMTK-3.6.3
        cd dcmtk
        mkdir build
        cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=/usr -DDCMTK_WITH_OPENSSL=OFF -DDCMTK_WITH_WRAP=OFF -DDCMTK_WITH_ICU=OFF -DDCMTK_WITH_ICONV=OFF
        sudo cmake --build . --target install
        cd ../..

* Make Beryllium from the source

        lrelease *.ts
        qmake beryllium.pro
        make
        sudo make install

* Create Package

        tar czf ../beryllium.tar.gz --exclude=debian --exclude=dcmtk --exclude=*.yml *
        rpmbuild -ta ../beryllium.tar.gz

Windows (Visual Studio)
-----------------------

* Build dependecies

  * [CMake](https://cmake.org/download/)
  * [WiX Toolset](http://wixtoolset.org/releases/)
  * [pkg-config](http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/)
  * [GStreamer, GStreamer-sdk](https://gstreamer.freedesktop.org/data/pkg/windows/)
  * [Boost](https://sourceforge.net/projects/boost/files/boost/)
  * [Qt MSVC](https://download.qt.io/archive/qt/)
  * [QtGStreamer](https://anongit.freedesktop.org/git/gstreamer/qt-gstreamer.git)
  * [DCMTK (optional)](http://dcmtk.org/dcmtk.php.en)
  * [MediaInfo (optional)](http://mediaarea.net/ru/MediaInfo/Download/Source)

* Build 3-rd party libraries

        # MediaInfo (optional)
        cd libmediainfo/mediainfolib/project/cmake
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -G "Visual Studio <version>"
        cmake --build . --target install

        # DCMTK (optional)
        cd dcmtk
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -DDCMTK_WITH_OPENSSL=OFF -DDCMTK_WITH_WRAP=OFF -DDCMTK_WITH_ICU=OFF -DDCMTK_WITH_ICONV=OFF -G "Visual Studio <version>"
        cmake --build . --target install

        # QtGStreamer
        set BOOST_DIR=<the path to boost headers>
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -DQT_VERSION=5 -DBoost_INCLUDE_DIR=%BOOST_DIR% -G "Visual Studio <version>"
        cmake --build . --target install

* Make Beryllium from the source

        lrelease-qt5 *.ts
        qmake-qt5 INCLUDEDIR+=%BOOST_DIR%
        nmake -f Makefile.Release

* Create Package

        copy \usr\bin\*.dll release\
        copy <the path to qt folder>\bin\*.dll Release\
        xcopy /s <the path to qt folder>\plugins Release\

        set QT_RUNTIME=QtRuntime32.wxi
        wix\build.cmd

Note that both the GStreamer & Qt must be built with exactly the same
version of the MSVC. For example, if GStreamer is build with MSVC 2010,
the Qt version should must be any from 5.0 till 5.5.

Windows (MinGW)
---------------

* Build dependecies

  * [CMake](https://cmake.org/download/)
  * [WiX Toolset](http://wixtoolset.org/releases/)
  * [GStreamer, GStreamer-sdk](https://gstreamer.freedesktop.org/data/pkg/windows/)
  * [Boost](https://sourceforge.net/projects/boost/files/boost/)
  * [Qt 5.0.2 MinGW](https://download.qt.io/archive/qt/5.0/5.0.2/)
  * [QtGStreamer](https://anongit.freedesktop.org/git/gstreamer/qt-gstreamer.git)
  * [DCMTK](http://dcmtk.org/dcmtk.php.en)
  * [MediaInfo (optional)](http://mediaarea.net/ru/MediaInfo/Download/Source)

* Build 3-rd party libraries

        # MediaInfo (optional)
        cd libmediainfo/mediainfolib/project/cmake
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -G "MinGW Makefiles"
        cmake --build . --target install

        # DCMTK (optional)
        cd dcmtk
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -DDCMTK_WITH_OPENSSL=OFF -DDCMTK_WITH_WRAP=OFF -DDCMTK_WITH_ICU=OFF -DDCMTK_WITH_ICONV=OFF -G "MinGW Makefiles"
        cmake --build . --target install

        # QtGStreamer
        set BOOST_DIR=<the path to boost headers>
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -DQT_VERSION=5 -DBoost_INCLUDE_DIR=%BOOST_DIR% -G "MinGW Makefiles"
        cmake --build . --target install

* Make Beryllium from the source

        lrelease-qt5 *.ts
        qmake-qt5 INCLUDEDIR+=%BOOST_DIR%
        min32gw-make -f Makefile.Release

* Create Package

        copy \usr\bin\*.dll release\
        copy C:\Qt\Qt5.0.2\5.0.2\mingw47_32\bin\*.dll Release\
        xcopy /s C:\Qt\Qt5.0.2\5.0.2\mingw47_32\plugins Release\

        set QT_RUNTIME=qt-5.0.2_mingw-4.7.wxi
        wix\build.cmd

Note that both the GStreamer & Qt must be built with exactly the same
version of the GCC. For example, if GStreamer is build with GCC 4.7.3,
the Qt version should must be 5.0.
