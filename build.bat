rd /s /q build_x86 build_x64 dist
del release.zip

set QTDIR32=-DQTDIR="../Qt_5.10.1/msvc2017"
set QTDIR64=-DQTDIR="../Qt_5.10.1/msvc2017_64"

cmake -DQTDIR="../Qt_5.10.1/msvc2017" -DOBS_BIN_DIR="../OBS-Studio-25.0.8-Full-x86" -DOBS_SRC_DIR="../obs-studio-25.0.8" -G "Visual Studio 16 2019" -A Win32 -B build_x86 -DCMAKE_INSTALL_PREFIX=dist . || exit /b 1
cmake --build build_x86 --config Release || exit /b 1
cmake --install build_x86 --config Release || exit /b 1

cmake -DQTDIR="../Qt_5.10.1/msvc2017_64" -DOBS_BIN_DIR="../OBS-Studio-25.0.8-Full-x64" -DOBS_SRC_DIR="../obs-studio-25.0.8" -G "Visual Studio 16 2019" -A x64 -B build_x64 -DCMAKE_INSTALL_PREFIX=dist . || exit /b 1
cmake --build build_x64 --config Release || exit /b 1
cmake --install build_x64 --config Release || exit /b 1

cd dist
cmake -E tar cf ..\release.zip --format=zip . || exit /b 1
cd ..
