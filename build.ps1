param(
    [string]$OBS_BIN_DIR32 = "../OBS-Studio-25.0.8-Full-x86",
    [string]$OBS_BIN_DIR64 = "../OBS-Studio-25.0.8-Full-x64",
    [string]$OBS_SRC_DIR = "../obs-studio-25.0.8",
    [string]$QTROOT = "C:/QtDep"
)

$ErrorActionPreference = "Stop"
Set-PSDebug -Trace 1

# find qt directory
$QTDIR32 = [System.IO.Directory]::GetDirectories("c:/QtDep/", "msvc2017", 1)[0].Replace("\", "/")
$QTDIR64 = [System.IO.Directory]::GetDirectories("c:/QtDep/", "msvc2017_64", 1)[0].Replace("\", "/")

$ver = (Select-String -Pattern "obs-multi-rtmp VERSION" -Path CMakeLists.txt -Raw)
$ver = $ver.Split(" ")[2]
$ver = $ver.Remove($ver.Length - 1)

Remove-Item -Path build_x86, build_x64, dist, *.zip -Recurse -ErrorAction Ignore

cmake -DQTDIR="$QTDIR32" -DOBS_BIN_DIR="$OBS_BIN_DIR32" -DOBS_SRC_DIR="$OBS_SRC_DIR" -G "Visual Studio 16 2019" -A Win32 -B build_x86 -DCMAKE_INSTALL_PREFIX=dist .
cmake --build build_x86 --config Release
cmake --install build_x86 --config Release

cmake -DQTDIR="$QTDIR64" -DOBS_BIN_DIR="$OBS_BIN_DIR64" -DOBS_SRC_DIR="$OBS_SRC_DIR" -G "Visual Studio 16 2019" -A x64 -B build_x64 -DCMAKE_INSTALL_PREFIX=dist .
cmake --build build_x64 --config Release
cmake --install build_x64 --config Release

cd dist
cmake -E tar cf "../obs-multi-rtmp_Windows_$ver.zip" --format=zip .
cd ..
