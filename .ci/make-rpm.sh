#!/bin/bash

DISTRO=${1-`lsb_release -is | tr " " "_"`}
RELEASE=${2-`lsb_release -rs`}

tar czf ../beryllium.tar.gz * \
  --exclude=builddir \
  --exclude=cache \
  --exclude=debian \
  --exclude=.ci \
  --exclude=.git \
  --exclude=*.yml \
  --exclude=*.sh

rpmbuild -D"dicom 1" -D"distro $DISTRO" -D"rev $RELEASE" -ta ../beryllium.tar.gz
