#!/bin/sh
set -ex

# OBS Studio deps
brew update
brew install ffmpeg
brew install libav

# qt 5.9.2
brew install https://raw.githubusercontent.com/Homebrew/homebrew-core/2b121c9a96e58a5da14228630cb71d5bead7137e/Formula/qt.rb

# Build obs-studio
cd ..
git clone --recursive https://github.com/jp9000/obs-studio
cd obs-studio
git checkout 21.0.0
mkdir build && cd build
cmake .. \
  -DCMAKE_PREFIX_PATH=/usr/local/opt/qt/lib/cmake \
&& make -j4

# Packages app
cd ..
curl -L -O  http://s.sudre.free.fr/Software/files/Packages.dmg -f --retry 5 -C -
hdiutil attach ./Packages.dmg
sudo installer -pkg /Volumes/Packages\ 1.2.2/packages/Packages.pkg -target /
