Summary: Beryllium DICOM edition.
Name: beryllium
Provides: beryllium
Version: 1.4.0
Release: 1
License: LGPL-2.1+
Source: %{name}.tar.gz
#Source: %{name}-%{version}.tar.bz2
URL: http://softus.org/products/beryllium
Vendor: Softus Inc. <contact@softus.org>
Packager: Softus Inc. <contact@softus.org>

BuildRequires: make, libv4l-devel, libavc1394-devel

%if %distro == fedora
BuildRequires: gstreamer1-devel, qt5-qtbase-devel, qt5-qtx11extras-devel, libgudev1-devel
BuildRequires: qt5-gstreamer1-devel
Requires: gstreamer1, qt5
Requires: gstreamer1-plugins-base, gstreamer1-plugins-good, gstreamer1-plugins-bad-free
%endif

%if %distro == centos
BuildRequires: gstreamer1-devel, qt5-qtbase-devel, qt5-qtx11extras-devel, libgudev1-devel
Requires: gstreamer1, qt5
Requires: gstreamer1-plugins-base, gstreamer1-plugins-good, gstreamer1-plugins-bad-free
%endif

%if %distro == opensuse
BuildRequires: libqt5-linguist, libqt5-qtbase-devel, libqt5-qtx11extras-devel, libgudev-1_0-devel
BuildRequires: gstreamer-plugins-qt5-devel
Requires: gstreamer-plugins-base, gstreamer-plugins-good
Requires: gstreamer-plugins-bad, gstreamer-plugins-ugly
Requires: gstreamer-plugins-qt5, gstreamer-plugin-gnonlin
%endif

%if %dicom == 1
BuildRequires: libmediainfo-devel, dcmtk-devel, openssl-devel
Requires: dcmtk

%if %distro == fedora
Requires: libmediainfo, libzen
%endif

%if %distro == centos
Requires: libmediainfo, libzen
%endif

%if %distro == opensuse
Requires: libmediainfo0, libzen0
%endif

%endif

%description
Beryllium DICOM edition.


Video and image capturing for medicine.

%global debug_package %{nil}

%define _rpmfilename %{distro}-%{rev}-%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm

%prep
%setup -c %{name}
 
%build
qmake-qt5 PREFIX=%{_prefix} QMAKE_CFLAGS+="%optflags" QMAKE_CXXFLAGS+="%optflags";
lrelease-qt5 *.ts
make -j 2 %{?_smp_mflags};

%install
make install INSTALL_ROOT="%buildroot";

%files
%doc docs/*
%{_mandir}/man1/%{name}.1.gz
%{_bindir}/beryllium
%{_datadir}/dbus-1/services
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/%{name}.png

%changelog
* Tue Apr 21 2015 Pavel Bludov <pbludov@gmail.com>
+ Version 1.2.6
- Change centos dependencies

* Wed Sep 11 2013 Pavel Bludov <pbludov@gmail.com>
+ Version 0.3.8
- First rpm spec for automated builds.
