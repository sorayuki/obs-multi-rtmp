#!/bin/bash

set -eux

: "${OBS_BIN:=/Applications/OBS.app/Contents/MacOS/obs}"
: "${OBS_SRC_DIR:=../}"
: "${OBS_SRC_URL:=https://github.com/obsproject/obs-studio/archive/#OBS_VER#.tar.gz}"

brew install packages obs

obs_ver=$("$OBS_BIN" -V)
obs_ver=${obs_ver//-/ }
obs_ver=($obs_ver)
obs_ver=${obs_ver[2]}

cd "$OBS_SRC_DIR"
curl -fsSLOJ "${OBS_SRC_URL//#OBS_VER#/$obs_ver}"
tar xf "obs-studio-$obs_ver.tar.gz"
