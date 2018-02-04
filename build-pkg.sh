#!/bin/bash

# Localizations
lrelease *.ts

nodicom=0
debug=0
opts=""

for arg in "$@"; do
  case $arg in
  'nodicom')
    nodicom=1
     ;;
  'debug')
    debug=1
     ;;
  *) echo "Unknown argument $arg"
     echo "Valid arguments are 'debug', 'nodicom'"
     ;;
  esac
done

if [ $nodicom = 1 ]; then
rm -fr src/dicom*
  # Remove all non-free code
  for f in $(grep -l WITH_DICOM src)
    do unifdef -o $f -UWITH_DICOM $f
  done
  # Beryllium DICOM edition => Beryllium free edition
  sed -i "s/DICOM/free/g" debian/control beryllium.spec
  sed -i "1 i CONFIG+=nodicom\n" beryllium.pro
fi

if [ $debug = 1 ]; then
  sed -i "1 i CONFIG+=debug\n" beryllium.pro
fi

distro=$((lsb_release -is || echo windows) | awk '{print tolower($1)}')
rev=$(lsb_release -rs || (cmd.exe /c ver | gawk 'match($0,/[0-9]\.[0-9]/){print substr($0,RSTART,RLENGTH)}'))
case $distro in
ubuntu | debian)  echo "Building DEB package"
    rm -f ../*.deb ../*.tar.gz ../*.dsc ../*.changes
    cp docs/* debian/
    dpkg-buildpackage -I.git -I*.sh -rfakeroot
    cd ..
    for fname in *.deb *.tar.gz *.dsc *.changes;
    do
        mv $fname $distro-$rev-$fname;
    done
    ;;
opensuse | fedora | suse | centos)  echo "Building RPM package"
    rm -f ../*.rpm ../*.tar.gz
    tar czf ../beryllium.tar.gz * --exclude=.git --exclude=*.sh && rpmbuild -D"dicom $dicom" -D"debug $debug" -D"distro $distro" -D"rev $rev" -ta ../beryllium.tar.gz
    mv ~/rpmbuild/RPMS/*-beryllium-*.rpm ..
    ;;
windows)  echo "Building MSI package"
    qmake && nmake -f Makefile.Release && cp -r c:/tmp/release . && cmd.exe "/c" "wix\build.cmd"
    ;;
*) echo "$distro is not supported yet"
    ;;
esac

