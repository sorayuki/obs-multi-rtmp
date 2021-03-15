#!/bin/bash

set -eux

: "${INSTALL:=0}"
: "${DESTDIR:=/Library/Application Support}"
: "${QTDIR:=/usr/local/opt/qt}"
: "${OBS_BIN_DIR:=/Applications/OBS.app}"

obs_ver=$("$OBS_BIN_DIR/Contents/MacOS/obs" -V)
obs_ver=${obs_ver//-/ }
obs_ver=($obs_ver)
obs_ver=${obs_ver[2]}
: "${OBS_SRC_DIR:=../obs-studio-$obs_ver}"

lib_path="dist/$DESTDIR/obs-studio/plugins/obs-multi-rtmp/bin/obs-multi-rtmp.so"

ver=$(grep 'obs-multi-rtmp VERSION' CMakeLists.txt)
ver=($ver)
ver=${ver[2]}
ver_len=${#ver}
ver_len=$((ver_len - 1))
ver=${ver:0:$ver_len}

rm -fr build dist *.pkg

cmake -DCMAKE_INSTALL_PREFIX="$DESTDIR" -DQTDIR="$QTDIR" -DOBS_BIN_DIR="$OBS_BIN_DIR/Contents/Frameworks" -DOBS_SRC_DIR="$OBS_SRC_DIR" -B build .
cmake --build build --config Release
cmake --install build --config Release --prefix "dist/$DESTDIR"

install_name_tool \
    -change "$QTDIR/lib/QtWidgets.framework/Versions/5/QtWidgets" \
        @executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets \
    -change "$QTDIR/lib/QtGui.framework/Versions/5/QtGui" \
        @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui \
    -change "$QTDIR/lib/QtCore.framework/Versions/5/QtCore" \
        @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore \
    "$lib_path"
otool -L "$lib_path"

sed "s/#VERSION#/$ver/g" installer_script/obs-multi-rtmp.pkgproj.in > build/obs-multi-rtmp.pkgproj
packagesbuild -F ./ build/obs-multi-rtmp.pkgproj
mv obs-multi-rtmp.pkg "obs-multi-rtmp_$ver.pkg"

[ $INSTALL -gt 0 ] && sudo cmake --install build --config Release || :
