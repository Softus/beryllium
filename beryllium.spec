Name: beryllium
Provides: beryllium
Version: 1.4.0
Release: 1%{?dist}
License: LGPL-2.1+
Source: %{name}.tar.gz
URL: http://softus.org/products/beryllium
Vendor: Softus Inc. <contact@softus.org>
Packager: Softus Inc. <contact@softus.org>
Summary: Beryllium DICOM edition.

%description
Beryllium DICOM edition.


Video and image capturing for medicine.

%global debug_package %{nil}

BuildRequires: make, gcc-c++

%{?fedora:BuildRequires: gstreamer1-devel, qt5-qtbase-devel, qt5-qtx11extras-devel, libv4l-devel, libavc1394-devel, qt5-gstreamer-devel, libmediainfo-devel, openssl-devel, dcmtk-devel}
%{?fedora:Requires: gstreamer1-plugins-base, gstreamer1-plugins-good, gstreamer1-plugins-bad-free}

%{?rhel:BuildRequires: gstreamer1-devel, qt5-qtbase-devel, qt5-qtx11extras-devel, libv4l-devel, libavc1394-devel, libmediainfo-devel, openssl-devel}
%{?rhel:Requires: gstreamer1-plugins-base, gstreamer1-plugins-good, gstreamer1-plugins-bad-free}

%{?suse_version:BuildRequires: libqt5-linguist, libqt5-qtbase-devel, libqt5-qtx11extras-devel, libv4l-devel, libavc1394-devel, gstreamer-plugins-qt5-devel, libmediainfo-devel, openssl-devel, dcmtk-devel}
%{?suse_version:Requires: gstreamer-plugins-base, gstreamer-plugins-good, gstreamer-plugins-bad}

%if 0%{?mageia}
%define qmake qmake
%define lrelease lrelease
BuildRequires: qttools5
%ifarch x86_64 amd64
BuildRequires: lib64v4l-devel, lib64avc1394-devel, lib64mediainfo-devel, lib64qt5-gstreamer-devel, lib64boost-devel, lib64gstreamer1.0-devel, lib64gstreamer-plugins-base1.0-devel, lib64qt5base5-devel, lib64qt5x11extras-devel
Requires: lib64gstreamer-plugins-base1.0_0
%else
BuildRequires: libv4l-devel,   libavc1394-devel,   libmediainfo-devel,   libqt5-gstreamer-devel,   libboost-devel,   libgstreamer1.0-devel,   libgstreamer-plugins-base1.0-devel,   libqt5base5-devel,   libqt5x11extras-devel
Requires: libgstreamer-plugins-base1.0_0
%endif
%else
%define qmake qmake-qt5
%define lrelease lrelease-qt5
%endif

%prep
%setup -c %{name}
 
%build
%{lrelease} *.ts
%{qmake} PREFIX=%{_prefix} QMAKE_CFLAGS+="%optflags" QMAKE_CXXFLAGS+="%optflags";
make %{?_smp_mflags};

%install
make install INSTALL_ROOT="%buildroot";

%files
%doc docs/*
%{_mandir}/man1/%{name}.1.*
%{_bindir}/beryllium
%{_datadir}/dbus-1/services
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/%{name}.png

%posttrans
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
/usr/bin/update-desktop-database &> /dev/null || :

%changelog
* Tue Apr 21 2015 Pavel Bludov <pbludov@gmail.com>
+ Version 1.2.6
- Change centos dependencies

* Wed Sep 11 2013 Pavel Bludov <pbludov@gmail.com>
+ Version 0.3.8
- First rpm spec for automated builds.
