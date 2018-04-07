#!/bin/sh
set -ex

cd /root/$PROJECT_NAME

mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
