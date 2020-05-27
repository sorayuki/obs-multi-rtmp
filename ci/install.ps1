param(
    [string]$OBS_VER = "25.0.8",
    [string]$OBS_BIN_URL = "https://github.com/obsproject/obs-studio/releases/download/#OBS_VER#/OBS-Studio-#OBS_VER#-Full-#ARCH#.zip",
    [string]$OBS_SRC_URL = "https://github.com/obsproject/obs-studio/archive/#OBS_VER#.zip"
)

$ErrorActionPreference = "Stop"
Set-PSDebug -Trace 1

$obs_bin_url = $OBS_BIN_URL.Replace("#OBS_VER#", "$OBS_VER")

$obs_bin_url_x86 = $obs_bin_url.Replace("#ARCH#", "x86")
$obs_bin_filename_x86 = $obs_bin_url_x86.Split("/")[-1]
$obs_bin_dirname_x86 =  $obs_bin_filename_x86 -replace "\.[^.]+$", ""

$obs_bin_url_x64 = $obs_bin_url.Replace("#ARCH#", "x64")
$obs_bin_filename_x64 = $obs_bin_url_x64.Split("/")[-1]
$obs_bin_dirname_x64 = $obs_bin_filename_x64 -replace "\.[^.]+$", ""

$obs_src_url = $OBS_SRC_URL.Replace("#OBS_VER#", "$OBS_VER")
$obs_src_filename = "obs-studio-$($obs_src_url.Split("/")[-1])"

If ($Env:APPVEYOR -ne "True") {
    function Start-FileDownload {
        param([string]$URL, [string]$FileName)
        Invoke-WebRequest -OutFile $FileName -TimeoutSec 300000 -Uri $URL
    }
}

Start-FileDownload $obs_bin_url_x86 -FileName ../$obs_bin_filename_x86
Start-FileDownload $obs_bin_url_x64 -FileName ../$obs_bin_filename_x64
Start-FileDownload $obs_src_url -FileName ../$obs_src_filename

unzip -oqd ../$obs_bin_dirname_x86 ../$obs_bin_filename_x86
unzip -oqd ../$obs_bin_dirname_x64 ../$obs_bin_filename_x64
unzip -oqd .. ../$obs_src_filename
