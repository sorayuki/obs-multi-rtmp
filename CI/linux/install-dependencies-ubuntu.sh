#!/bin/sh
set -ex

sudo add-apt-repository -y ppa:obsproject/obs-studio
sudo apt-get -qq update

sudo apt-get install -y \
	libc-dev-bin \
	libc6-dev git \
	build-essential \
	checkinstall \
	cmake \
	obs-studio \
	qtbase5-dev

# Dirty hack
sudo wget -O /usr/include/obs/obs-frontend-api.h https://raw.githubusercontent.com/obsproject/obs-studio/25.0.0/UI/obs-frontend-api/obs-frontend-api.h

sudo ldconfig
