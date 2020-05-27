#!/usr/bin/bash

set -eux

: "${INSTALL:=0}"
: "${DESTDIR:=/usr}"
: "${QTDIR:=}"
: "${OBS_SRC_DIR:=/usr/include/obs}"

ver=$(grep 'obs-multi-rtmp VERSION' CMakeLists.txt)
ver=($ver)
ver=${ver[2]}
ver=${ver:0:-1}

rm -fr build dist

cmake  -DCMAKE_INSTALL_PREFIX=$DESTDIR -DQTDIR=$QTDIR -DOBS_SRC_DIR=$OBS_SRC_DIR -B build .
cmake --build build --config Release
cmake --install build --config Release --prefix dist/$DESTDIR
[ $INSTALL -gt 0 ] && sudo cmake --install build --config Release
