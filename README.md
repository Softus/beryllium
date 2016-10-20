Beryllium
=========

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
        libssl-dev libwrap0-dev libgudev-1.0-dev libgstreamer1.0-dev \
        libqt5gstreamer-dev libgstreamer-plugins-base1.0-dev \
        libqt5opengl5-dev libavc1394-dev libraw1394-dev libv4l-dev

2. Make Makefile

        qmake CONFIG+=dicom beryllium.pro
  
3. Make Beryllium

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

        make

4. Install Beryllium

        sudo make install
