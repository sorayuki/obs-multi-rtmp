#!/usr/bin/bash

set -eux

: "${DESTDIR:=/usr}"
: "${QTDIR:=}"
: "${OBS_INCLUDE_DIR:=/usr/include/obs}"

ver=$(grep 'obs-multi-rtmp VERSION' CMakeLists.txt)
ver=($ver)
ver=${ver[2]}
ver=${ver:0:-1}

obs_ver=$(obs -V)
obs_ver=${obs_ver//-/ }
obs_ver=($obs_ver)
obs_ver=${obs_ver[2]}

rm -rf build dist
curl -o /tmp/obs-frontend-api.h https://raw.githubusercontent.com/obsproject/obs-studio/$obs_ver/UI/obs-frontend-api/obs-frontend-api.h
sudo mkdir -p $OBS_INCLUDE_DIR/UI/obs-frontend-api
sudo mv /tmp/obs-frontend-api.h $OBS_INCLUDE_DIR/UI/obs-frontend-api

cmake  -DCMAKE_INSTALL_PREFIX=$DESTDIR -DQTDIR=$QTDIR -DOBS_INCLUDE_DIR=$OBS_INCLUDE_DIR -B build .
cmake --build build --config Release
# sudo cmake --install build --config Release
