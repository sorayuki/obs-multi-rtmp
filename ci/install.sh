#!/usr/bin/bash

set -eux

: "${OBS_SRC_DIR:=/usr/include/obs}"

sudo add-apt-repository -y ppa:obsproject/obs-studio
sudo apt-get -q update
sudo apt-get install -y obs-studio

obs_ver=$(obs -V)
obs_ver=${obs_ver//-/ }
obs_ver=($obs_ver)
obs_ver=${obs_ver[2]}

curl -o /tmp/obs-frontend-api.h https://raw.githubusercontent.com/obsproject/obs-studio/$obs_ver/UI/obs-frontend-api/obs-frontend-api.h
sudo mkdir -p $OBS_SRC_DIR/UI/obs-frontend-api
sudo mv /tmp/obs-frontend-api.h $OBS_SRC_DIR/UI/obs-frontend-api

$obs_x86 = "OBS-Studio-${TARGET}-Full-x86"
$obs_x64 = "OBS-Studio-${TARGET}-Full-x64"
$obs_src = "obs-studio-${TARGET}"

curl -fsLOS ${QT_URL}${QT_FILENAME}.7z
curl -fsLOS ${env:OBS_BASE_BIN_URL}/${env:TARGET}/${obs_x86}.zip
curl -fsLOS ${env:OBS_BASE_BIN_URL}/${env:TARGET}/${obs_x64}.zip

7z x "${QT_FILENAME}.7z" -o ..
qt_dirname=${QT_FILENAME:3}
mv ${QT_FILENAME:3} ../${QT_FILENAME}

unzip -q ${obs_x86}.zip -d ../${obs_x86}
unzip -q ${obs_x64}.zip -d ../${obs_x64}
unzip -q ${obs_src}.zip -d ..
