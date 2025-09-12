OutFile "obs-multi-rtmp-setup.exe"

Unicode true
RequestExecutionLevel user

SetDatablockOptimize on
SetCompress auto
SetCompressor /SOLID lzma

Name "obs-multi-rtmp"
Caption "Multiple RTMP Output Plugin for OBS Studio"
Icon "${NSISDIR}\Contrib\Graphics\Icons\win-install.ico"

Var /Global DefInstDir
Function .onInit
    ReadEnvStr $0 "ALLUSERSPROFILE"
    StrCpy $DefInstDir "$0\obs-studio\plugins"
    StrCpy $INSTDIR "$DefInstDir"

    IfFileExists "$DefInstDir\obs-multi-rtmp\*.*" AskUninst DontAskUninst
    AskUninst:
        MessageBox MB_YESNO|MB_ICONQUESTION "安装还是卸载obs-multi-rtmp？ 是=安装，否=卸载$\r$\n$\r$\nobs-multi-rtmpを入れるか外れるか？はい=入れる、いいえ=外れる$\r$\n$\r$\nInstall or remove obs-multi-rtmp? Yes = Install, No = Remove" IDYES NotDoUninst IDNO DoUninst
    DoUninst:
        RMDir /r "$DefInstDir"
        MessageBox MB_OK|MB_ICONINFORMATION "完成$\r$\n$\r$\n完了$\r$\n$\r$\nDone"
        Quit
    NotDoUninst:
    DontAskUninst:
FunctionEnd

Function onDirPageLeave
StrCmp "$INSTDIR" "$DefInstDir" DirNotModified DirModified
DirModified:
MessageBox MB_OK|MB_ICONSTOP "Please don't change the install directory."
Abort
DirNotModified:
FunctionEnd

Page directory "" "" onDirPageLeave
Page instfiles

Section
SetOutPath "$INSTDIR"
File /r "release\Release\obs-multi-rtmp"
SectionEnd

