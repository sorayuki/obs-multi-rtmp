#!/usr/bin/bash

set -eux

: "${OBS_SRC_DIR:=/usr/include/obs}"

sudo add-apt-repository -y ppa:obsproject/obs-studio
sudo apt-get -q update
sudo apt-get install -qy mesa-common-dev obs-studio

obs_ver=$(obs -V)
obs_ver=${obs_ver//-/ }
obs_ver=($obs_ver)
obs_ver=${obs_ver[2]}

curl -o /tmp/obs-frontend-api.h https://raw.githubusercontent.com/obsproject/obs-studio/$obs_ver/UI/obs-frontend-api/obs-frontend-api.h
sudo mkdir -p $OBS_SRC_DIR/UI/obs-frontend-api
sudo mv /tmp/obs-frontend-api.h $OBS_SRC_DIR/UI/obs-frontend-api
