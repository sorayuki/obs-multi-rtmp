Set-PSDebug -Trace 1
$ErrorActionPreference = "Stop"

$obs_x86 = "OBS-Studio-$env:TARGET-Full-x86"
$obs_x64 = "OBS-Studio-$env:TARGET-Full-x64"
$obs_src = "obs-studio-$env:TARGET"

Start-FileDownload "$env:QT_BASE_URL/$env:QT_FILENAME.7z"
Start-FileDownload "$env:OBS_BASE_BIN_URL/$env:TARGET/$obs_x86.zip"
Start-FileDownload "$env:OBS_BASE_BIN_URL/$env:TARGET/$obs_x64.zip"
Start-FileDownload "$env:OBS_BASE_SRC_URL/$env:TARGET.zip"

7z x "$env:QT_FILENAME.7z" -o ..
Move-Item ../$env:QT_FILENAME.Substring(3) ../$env:QT_FILENAME

unzip -q "$obs_x86.zip" -d ../$obs_x86
unzip -q "$obs_x64.zip" -d ../$obs_x64
unzip -q "$obs_src.zip" -d ..
