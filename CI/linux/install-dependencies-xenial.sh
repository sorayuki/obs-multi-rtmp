#!/bin/sh
set -ex

add-apt-repository -y ppa:obsproject/obs-studio
apt-get -qq update

apt-get install -y \
	libc-dev-bin \
	libc6-dev git \
	build-essential \
	checkinstall \
	cmake \
	obs-studio \
    qtbase5-dev

# Dirty hack
wget -O /usr/include/obs/obs-frontend-api.h https://raw.githubusercontent.com/obsproject/obs-studio/master/UI/obs-frontend-api/obs-frontend-api.h

ldconfig
