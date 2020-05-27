$ErrorActionPreference = "Stop"
Set-PSDebug -Trace 1

$obs_bin_url = $Env:OBS_BIN_URL.Replace("#TARGET#", "$Env:TARGET")

$obs_bin_url_x86 = $obs_bin_url.Replace("#ARCH#", "x86")
$obs_bin_filename_x86 = $obs_bin_url_x86.Split("/")[-1]
$obs_bin_dirname_x86 =  $obs_bin_filename_x86 -replace "\.[^.]+$", ""

$obs_bin_url_x64 = $obs_bin_url.Replace("#ARCH#", "x64")
$obs_bin_filename_x64 = $obs_bin_url_x64.Split("/")[-1]
$obs_bin_dirname_x64 = $obs_bin_filename_x64 -replace "\.[^.]+$", ""

$obs_src_url = $Env:OBS_SRC_URL.Replace("#TARGET#", "$Env:TARGET")
$obs_src_filename = $obs_src_url.Split("/")[-1]

Start-FileDownload $obs_bin_url_x86
Start-FileDownload $obs_bin_url_x64
Start-FileDownload $obs_src_url

unzip -q $obs_bin_filename_x86 -d ../$obs_bin_dirname_x86
unzip -q $obs_bin_filename_x64 -d ../$obs_bin_dirname_x64
unzip -q $obs_src_filename -d ..
