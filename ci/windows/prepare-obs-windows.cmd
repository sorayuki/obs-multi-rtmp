@echo off
SETLOCAL EnableDelayedExpansion

REM If obs-studio directory does not exist, clone the git repo
if not exist %OBSPath% (
	echo obs-studio directory does not exist
	git clone https://github.com/obsproject/obs-studio %OBSPath%
	cd /D %OBSPath%\
	git describe --tags --abbrev=0 --exclude="*-rc*" > "%OBSPath%\obs-studio-latest-tag.txt"
    set /p OBSLatestTag=<"%OBSPath%\obs-studio-latest-tag.txt"
)

REM Prepare OBS Studio builds

echo Running CMake...
cd /D %OBSPath%
echo   git checkout %OBSLatestTag%
git checkout %OBSLatestTag%
echo:

if not exist build32 mkdir build32
if not exist build64 mkdir build64

echo   Running cmake for obs-studio %OBSLatestTag% 32-bit...
cd build32
cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_SYSTEM_VERSION=10.0 -DQTDIR="%QTDIR32%" -DDepsPath="%DepsPath32%" -DBUILD_CAPTIONS=true -DDISABLE_PLUGINS=true -DCOPIED_DEPENDENCIES=false -DCOPY_DEPENDENCIES=true ..
echo:
echo:

echo   Running cmake for obs-studio %OBSLatestTag% 64-bit...
cd ..\build64
cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_SYSTEM_VERSION=10.0 -DQTDIR="%QTDIR64%" -DDepsPath="%DepsPath64%" -DBUILD_CAPTIONS=true -DDISABLE_PLUGINS=true -DCOPIED_DEPENDENCIES=false -DCOPY_DEPENDENCIES=true ..
echo:
echo:

dir "%OBSPath%\libobs"
