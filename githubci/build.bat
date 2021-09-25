set OBSBIN_VER=26.1.1
set OBSSRC_VER=26.1.2

curl -o obs-bin-x86.zip "https://cdn-fastly.obsproject.com/downloads/OBS-Studio-%OBSBIN_VER%-Full-x86.zip"
curl -o obs-bin-x64.zip "https://cdn-fastly.obsproject.com/downloads/OBS-Studio-%OBSBIN_VER%-Full-x64.zip"
git clone --depth=1 -b %OBSSRC_VER% "https://github.com/obsproject/obs-studio.git" obs-src
call obs-src\CI\install-qt-win.cmd
cmake -E make_directory obs-bin
pushd obs-bin
cmake -E tar zxf ..\obs-bin-x86.zip
cmake -E tar zxf ..\obs-bin-x64.zip
popd

set QTDIR32=-DQTDIR="C:/QtDep/5.15.2/msvc2019"
set QTDIR64=-DQTDIR="C:/QtDep/5.15.2/msvc2019_64"

rem install mode
cmake %QTDIR32% -G "Visual Studio 16 2019" -A Win32 -B build_x86 -S . -DCMAKE_INSTALL_PREFIX=dist
cmake --build build_x86 --config Release
cmake --install build_x86 --config Release

cmake %QTDIR64% -G "Visual Studio 16 2019" -A x64 -B build_x64 -S . -DCMAKE_INSTALL_PREFIX=dist
cmake --build build_x64 --config Release
cmake --install build_x64 --config Release

cmake -E make_directory installer
pushd dist
copy ..\installer_script\installer.nsi .
"C:\Program Files (x86)\NSIS\makensis" installer.nsi
copy obs-multi-rtmp-setup.exe ..\installer
popd

rem portable mode
cmake %QTDIR32% -G "Visual Studio 16 2019" -A Win32 -B build_x86 -S . -DPORTABLE_MODE=ON -DCMAKE_INSTALL_PREFIX=portable
cmake --build build_x86 --config Release
cmake --install build_x86 --config Release

cmake %QTDIR64% -G "Visual Studio 16 2019" -A x64 -B build_x64 -S . -DPORTABLE_MODE=ON -DCMAKE_INSTALL_PREFIX=portable
cmake --build build_x64 --config Release
cmake --install build_x64 --config Release
