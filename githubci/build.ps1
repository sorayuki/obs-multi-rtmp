$OBSSRC_VER = "28.0.0-beta1"

# fetch obs source code
if ([System.IO.File]::Exists("OBSSRC-INSTALLED") -eq $false) {
    git clone --depth=1 -b $OBSSRC_VER "https://github.com/obsproject/obs-studio.git" obs-src
    [System.IO.File]::WriteAllText("OBSSRC-INSTALLED", "")
}

# fetch obs binary
if ([System.IO.File]::Exists("OBSBIN-INSTALLED") -eq $false) {
    Invoke-WebRequest -Uri "https://github.com/obsproject/obs-studio/releases/download/28.0.0-beta1/OBS-Studio-28.0-beta1-Full-x64.zip" -UseBasicParsing -OutFile "obs-bin.zip"
    Expand-Archive -Path "obs-bin.zip" -DestinationPath "obs-bin"
    [System.IO.File]::WriteAllText("OBSBIN-INSTALLED", "")
}

$_RunObsBuildScript = $true
$BuildArch = "x64"
$CheckoutDir = Resolve-Path -Path "$PSScriptRoot\..\obs-src"
$DepsBuildDir = "${CheckoutDir}/../obs-build-dependencies"

cd "$PSScriptRoot\..\"

. ".\obs-src\CI\include\build_support_windows.ps1"
. ".\obs-src\CI\windows\01_install_dependencies.ps1"

# install qt
if ([System.IO.File]::Exists("QT-INSTALLED") -eq $false) {
    Install-qt-deps -Version $WindowsDepsVersion
    [System.IO.File]::WriteAllText("QT-INSTALLED", "")
}

cd "$PSScriptRoot\..\"
$depdir = ""
Foreach($dir in [System.IO.Directory]::EnumerateDirectories("obs-build-dependencies", "windows-deps-*")) {
    $depdir = $dir
    break
}
if ($depdir -ceq "") {
    echo "Fail to find Qt dir"
    break script
}

Remove-Item -Recurse -Force output
mkdir output
Foreach($portable in @("ON", "OFF")) {
    Foreach($dir in @("build_x64", "dist")) {
        Remove-Item -Recurse -Force $dir
    }
    cmake -DQTDIR="${depdir}" -G "Visual Studio 17 2022" -A x64 -B build_x64 -S . -DPORTABLE_MODE="${portable}" -DCMAKE_INSTALL_PREFIX=dist
    if ($LASTEXITCODE -gt 0) {
        echo "Configurare failed"
        break script
    }
    cmake --build build_x64 --config Release
    if ($LASTEXITCODE -gt 0) {
        echo "Build failed"
        break script
    }
    cmake --install build_x64 --config Release
    if ($portable -eq "OFF") {
        cd dist
        copy "..\installer_script\installer.nsi" ".\installer.nsi"
        & "C:\Program Files (x86)\NSIS\makensis.exe" "installer.nsi"
        copy obs-multi-rtmp-setup.exe ..\output\obs-multi-rtmp-setup.exe
        cd ..
    } else {
        cd dist
        Compress-Archive -Path * -DestinationPath "..\output\obs-multi-rtmp-portable.zip"
        cd ..
    }
}
