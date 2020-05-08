#!/bin/sh

OSTYPE=$(uname)

if [ "${OSTYPE}" != "Darwin" ]; then
    echo "[Error] macOS obs-studio build script can be run on Darwin-type OS only."
    exit 1
fi

HAS_CMAKE=$(type cmake 2>/dev/null)
HAS_GIT=$(type git 2>/dev/null)

if [ "${HAS_CMAKE}" = "" ]; then
    echo "[Error] CMake not installed - please run 'install-dependencies-macos.sh' first."
    exit 1
fi

if [ "${HAS_GIT}" = "" ]; then
    echo "[Error] Git not installed - please install Xcode developer tools or via Homebrew."
    exit 1
fi

echo "=> Downloading and unpacking OBS dependencies"
wget --quiet --retry-connrefused --waitretry=1 https://obs-nightly.s3.amazonaws.com/osx-deps-2018-08-09.tar.gz
tar -xf ./osx-deps-2018-08-09.tar.gz -C /tmp

# Build obs-studio
cd ..
echo "=> Cloning obs-studio from GitHub.."
git clone https://github.com/obsproject/obs-studio
cd obs-studio
OBSLatestTag=$(git describe --tags --abbrev=0)
git checkout $OBSLatestTag
mkdir build && cd build
echo "=> Building obs-studio.."
cmake .. \
	-DBUILD_CAPTIONS=true \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=10.11 \
	-DDISABLE_PLUGINS=true \
    -DENABLE_SCRIPTING=0 \
	-DDepsPath=/tmp/obsdeps \
	-DCMAKE_PREFIX_PATH=/usr/local/opt/qt/lib/cmake \
&& make -j4
