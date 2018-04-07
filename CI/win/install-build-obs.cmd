@echo off
SETLOCAL EnableDelayedExpansion

REM Check if obs-studio build exists.
REM If the obs-studio directory does exist, check if the last OBS tag built
REM matches the latest OBS tag.
REM If the tags match, do not build obs-studio.
REM If the tags do not match, build obs-studio.
REM If the obs-studio directory doesn't exist, build obs-studio.
echo Checking for obs-studio build...

set OBSLatestTagPrePull=0
set OBSLatestTagPostPull=0
echo Latest tag pre-pull: %OBSLatestTagPrePull%
echo Latest tag post-pull: %OBSLatestTagPostPull%

REM Set up the build flag as undefined.
set "BuildOBS="

REM Check the last tag successfully built by CI.
if exist C:\projects\obs-studio-last-tag-built.txt (
  set /p OBSLastTagBuilt=<C:\projects\obs-studio-last-tag-built.txt
) else (
  set OBSLastTagBuilt=0
)

REM If obs-studio directory exists, run git pull and get the latest tag number.
if exist C:\projects\obs-studio\ (
  echo obs-studio directory exists
  echo   Updating tag info
  cd C:\projects\obs-studio\
  git describe --tags --abbrev=0 > C:\projects\latest-obs-studio-tag-pre-pull.txt
  set /p OBSLatestTagPrePull=<C:\projects\latest-obs-studio-tag-pre-pull.txt
  git checkout master
  git pull
  git describe --tags --abbrev=0 > C:\projects\latest-obs-studio-tag-post-pull.txt
  set /p OBSLatestTagPostPull=<C:\projects\latest-obs-studio-tag-post-pull.txt
  set /p OBSLatestTag=<C:\projects\latest-obs-studio-tag-post-pull.txt
  echo %OBSLatestTagPostPull%> C:\projects\latest-obs-studio-tag.txt
)

REM Check the obs-studio tags for mismatches.
REM If a new tag was pulled, set the build flag.
if not %OBSLatestTagPrePull%==%OBSLatestTagPostPull% (
  echo Latest tag pre-pull: %OBSLatestTagPrePull%
  echo Latest tag post-pull: %OBSLatestTagPostPull%
  echo Tags do not match.  Need to rebuild OBS.
  set BuildOBS=true
)

REM If the latest git tag doesn't match the last built tag, set the build flag.
if not %OBSLatestTagPostPull%==%OBSLastTagBuilt% (
  echo Last built OBS tag: %OBSLastTagBuilt%
  echo Latest tag post-pull: %OBSLatestTagPostPull%
  echo Tags do not match.  Need to rebuild OBS.
  set BuildOBS=true
)

REM If obs-studio directory does not exist, clone the git repo, get the latest
REM tag number, and set the build flag.
if not exist C:\projects\obs-studio (
  echo obs-studio directory does not exist
  git clone --recursive https://github.com/obsproject/obs-studio
  cd C:\projects\obs-studio\
  git describe --tags --abbrev=0 > C:\projects\obs-studio-latest-tag.txt
  set /p OBSLatestTag=<C:\projects\obs-studio-latest-tag.txt
  set BuildOBS=true
)

REM Some debug info
echo:
echo Latest tag pre-pull: %OBSLatestTagPrePull%
echo Latest tag post-pull: %OBSLatestTagPostPull%
echo Latest tag: %OBSLatestTag%
echo Last built OBS tag: %OBSLastTagBuilt%

if defined BuildOBS (
  echo BuildOBS: true
) else (
  echo BuildOBS: false
)
echo:

REM If the build flag is set, build obs-studio.
if defined BuildOBS (
  echo Building obs-studio...
  echo   git checkout %OBSLatestTag%
  git checkout %OBSLatestTag%
  echo:
  echo   Removing previous build dirs...
  if exist build rmdir /s /q C:\projects\obs-studio\build
  if exist build32 rmdir /s /q C:\projects\obs-studio\build32
  if exist build64 rmdir /s /q C:\projects\obs-studio\build64
  echo   Making new build dirs...
  mkdir build
  mkdir build32
  mkdir build64
  echo   Running cmake for obs-studio %OBSLatestTag% 32-bit...
  cd ./build32
  cmake -G "Visual Studio 14 2015" -DCOPIED_DEPENDENCIES=false -DCOPY_DEPENDENCIES=true ..
  echo:
  echo:
  echo   Running cmake for obs-studio %OBSLatestTag% 64-bit...
  cd ../build64
  cmake -G "Visual Studio 14 2015 Win64" -DCOPIED_DEPENDENCIES=false -DCOPY_DEPENDENCIES=true ..
  echo:
  echo:
  echo   Building obs-studio %OBSLatestTag% 32-bit ^(Build Config: %build_config%^)...
  call msbuild /m /p:Configuration=%build_config% C:\projects\obs-studio\build32\obs-studio.sln /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"
  echo   Building obs-studio %OBSLatestTag% 64-bit ^(Build Config: %build_config%^)...
  call msbuild /m /p:Configuration=%build_config% C:\projects\obs-studio\build64\obs-studio.sln /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"
  cd ..
  git describe --tags --abbrev=0 > C:\projects\obs-studio-last-tag-built.txt
  set /p OBSLastTagBuilt=<C:\projects\obs-studio-last-tag-built.txt
) else (
  echo Last OBS tag built is:  %OBSLastTagBuilt%
  echo No need to rebuild OBS.
)
