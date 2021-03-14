OutFile "obs-multi-rtmp-setup.exe"

Unicode true
RequestExecutionLevel user

SetDatablockOptimize on
SetCompress auto
SetCompressor /FINAL /SOLID lzma

Name "obs-multi-rtmp"
Caption "Multiple RTMP Output Plugin for OBS Studio"
Icon "${NSISDIR}\Contrib\Graphics\Icons\win-install.ico"

Function .onInit
    ReadEnvStr $0 "ALLUSERSPROFILE"
    StrCpy $INSTDIR "$0\obs-studio\plugins\obs-multi-rtmp"
FunctionEnd

Page directory
Page instfiles

Section
SetOutPath "$INSTDIR\bin\32bit"
File "plugins\obs-multi-rtmp\bin\32bit\obs-multi-rtmp.dll"

SetOutPath "$INSTDIR\bin\64bit"
File "plugins\obs-multi-rtmp\bin\64bit\obs-multi-rtmp.dll"

SetOutPath "$INSTDIR\data\locale"
File "plugins\obs-multi-rtmp\data\locale\*.ini"
SectionEnd

Section "Uninstaller"
RMDir /r /REBOOTOK "$INSTDIR\plugins\obs-multi-rtmp"
SectionEnd
