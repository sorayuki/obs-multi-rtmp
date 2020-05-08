#!/bin/sh
set -ex

mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
