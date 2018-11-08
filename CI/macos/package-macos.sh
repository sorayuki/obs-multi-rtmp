#!/bin/sh

set -e

echo "-- Preparing package build"
export QT_CELLAR_PREFIX="$(find /usr/local/Cellar/qt -d 1 | tail -n 1)"

export GIT_HASH=$(git rev-parse --short HEAD)

export VERSION="$GIT_HASH-$TRAVIS_BRANCH"
export LATEST_VERSION="$TRAVIS_BRANCH"
if [ -n "${TRAVIS_TAG}" ]; then
	export VERSION="$TRAVIS_TAG"
	export LATEST_VERSION="$TRAVIS_TAG"
fi

export FILENAME="$PROJECT_NAME-$VERSION.pkg"
export LATEST_FILENAME="$PROJECT_NAME-latest-$LATEST_VERSION.pkg"

echo "-- Modifying $PROJECT_NAME.so"
install_name_tool \
	-change /usr/local/opt/qt/lib/QtWidgets.framework/Versions/5/QtWidgets @rpath/QtWidgets \
	-change /usr/local/opt/qt/lib/QtGui.framework/Versions/5/QtGui @rpath/QtGui \
	-change /usr/local/opt/qt/lib/QtCore.framework/Versions/5/QtCore @rpath/QtCore \
	./build/$PROJECT_NAME.so

# Check if replacement worked
echo "-- Dependencies for obs-websocket"
otool -L ./build/obs-websocket.so

echo "-- Actual package build"
packagesbuild ./CI/installer/installer-macOS.pkgproj

echo "-- Renaming $PROJECT_NAME.pkg to $FILENAME"
mv ./release/$PROJECT_NAME.pkg ./release/$FILENAME
cp ./release/$FILENAME ./release/$LATEST_FILENAME
