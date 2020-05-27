Set-PSDebug -Trace 1
$ErrorActionPreference = "Stop"

$ver = (Select-String 'obs-multi-rtmp VERSION' CMakeLists.txt -Raw)
$ver = $ver.Split(' ')[2]
$ver = $ver.Remove($ver.Length - 1)

Remove-Item build_x86 build_x64 dist *.zip -Recurse -ErrorAction Ignore

cmake -DQTDIR="../Qt_5.10.1/msvc2017" -DOBS_BIN_DIR="../OBS-Studio-25.0.8-Full-x86" -DOBS_SRC_DIR="../obs-studio-25.0.8" -G "Visual Studio 16 2019" -A Win32 -B build_x86 -DCMAKE_INSTALL_PREFIX=dist .
cmake --build build_x86 --config Release
cmake --install build_x86 --config Release

cmake -DQTDIR="../Qt_5.10.1/msvc2017_64" -DOBS_BIN_DIR="../OBS-Studio-25.0.8-Full-x64" -DOBS_SRC_DIR="../obs-studio-25.0.8" -G "Visual Studio 16 2019" -A x64 -B build_x64 -DCMAKE_INSTALL_PREFIX=dist .
cmake --build build_x64 --config Release
cmake --install build_x64 --config Release

cd dist
cmake -E tar cf "../obs-multi-rtmp_Windows_$ver.zip" --format=zip .
cd ..
