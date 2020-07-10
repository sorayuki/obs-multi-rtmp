#!/usr/bin/bash

set -eux

: "${OBS_INCLUDE_DIR:=/usr/include/obs}"
: "${OBS_API_SRC_URL:=https://raw.githubusercontent.com/obsproject/obs-studio/#OBS_VER#/UI/obs-frontend-api/obs-frontend-api.h}"

sudo add-apt-repository -y ppa:obsproject/obs-studio
sudo apt-get update
sudo apt-get install -y mesa-common-dev obs-studio

obs_ver=$(obs -V)
obs_ver=${obs_ver//-/ }
obs_ver=($obs_ver)
obs_ver=${obs_ver[2]}

curl -fsSLo /tmp/obs-frontend-api.h "${OBS_API_SRC_URL//#OBS_VER#/$obs_ver}"
sudo mkdir -p "$OBS_INCLUDE_DIR/UI/obs-frontend-api"
sudo mv /tmp/obs-frontend-api.h "$OBS_INCLUDE_DIR/UI/obs-frontend-api"
