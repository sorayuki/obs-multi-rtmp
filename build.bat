echo y | rd /s dist
del release.zip

set QTDIR32=-DQTDIR="F:/dev/qt/5.10.1/msvc2017"
set QTDIR64=-DQTDIR="F:/dev/qt/5.10.1/msvc2017_64"

cmake %QTDIR32% -G "Visual Studio 16 2019" -A Win32 -B build_x86 -S . -DCMAKE_INSTALL_PREFIX=dist
cmake --build build_x86 --config Release
cmake --install build_x86 --config Release

cmake %QTDIR64% -G "Visual Studio 16 2019" -A x64 -B build_x64 -S . -DCMAKE_INSTALL_PREFIX=dist
cmake --build build_x64 --config Release
cmake --install build_x64 --config Release

if not exist dist\nul exit /b
cd dist
if exist ..\dist\nul
cmake -E tar cf ..\release.zip --format=zip .
cd ..
