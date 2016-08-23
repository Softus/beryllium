Summary: Beryllium DICOM edition.
Name: beryllium
Provides: beryllium
Version: 1.3.6
Release: 1
License: LGPL-2.1+
Source: %{name}.tar.gz
#Source: %{name}-%{version}.tar.bz2
URL: http://dc.baikal.ru/products/beryllium
Vendor: Beryllium team <beryllium@dc.baikal.ru>
Packager: Beryllium team <beryllium@dc.baikal.ru>

BuildRequires: make
BuildRequires: libavc-1394-devel, libgudev-devel, libv4l-devel

%if %distro == fedora
BuildRequires: gstreamer1-devel, qt-devel, qt5-gstreamer-devel
Requires: gstreamer1, qt5
Requires: gstreamer1-plugins-base, gstreamer1-plugins-good
Requires: gstreamer1-plugins-bad-free, gnonlin
%endif

%if %distro == centos
BuildRequires: gstreamer-devel, qt-devel
Requires: gstreamer >= 0.10.35, qt4 >= 4.6.0
Requires: gstreamer-plugins-base >= 0.10.31, gstreamer-plugins-good >= 0.10.23
Requires: gstreamer-plugins-bad >= 0.10.19, gstreamer-plugins-ugly >= 0.10.18
Requires: gstreamer-ffmpeg, gnonlin
%endif

%if %distro == opensuse
BuildRequires: gstreamer-devel, libqt5-qtbase-devel, gstreamer-plugins-qt5-devel
Requires: gstreamer-plugins-base, gstreamer-plugins-good
Requires: gstreamer-plugins-bad, gstreamer-plugins-ugly
Requires: gstreamer-plugin-gnonlin
%endif

%if %dicom == 1
BuildRequires: libmediainfo-devel, dcmtk-devel, tcp_wrappers-devel, openssl-devel

%if %distro == centos
Requires: dcmtk, openssl, libmediainfo, libzen
%endif

%if %distro == opensuse
Requires: dcmtk, libmediainfo0, libzen0
%endif

%endif

%description
Beryllium DICOM edition.


Video and image capturing for medicine.

%define _rpmfilename %{distro}-%{rev}-%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm

%prep
%setup -c %{name}
 
%build
qmake PREFIX=%{_prefix} QMAKE_CFLAGS+="%optflags" QMAKE_CXXFLAGS+="%optflags";
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
