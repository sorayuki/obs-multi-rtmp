call "%~dp0..\ci_includes.generated.cmd"

REM cd build32
REM cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_SYSTEM_VERSION=10.0 -DQTDIR="%QTDIR32%" -DLibObs_DIR="%OBSPath%\build32\libobs" -DLIBOBS_INCLUDE_DIR="%OBSPath%\libobs" -DLIBOBS_LIB="%OBSPath%\build32\libobs\%build_config%\obs.lib" -DOBS_FRONTEND_LIB="%OBSPath%\build32\UI\obs-frontend-api\%build_config%\obs-frontend-api.lib" ..
REM ren "%PluginName%.sln" "main.sln"

REM cd ..\build64
REM cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_SYSTEM_VERSION=10.0 -DQTDIR="%QTDIR64%" -DLibObs_DIR="%OBSPath%\build64\libobs" -DLIBOBS_INCLUDE_DIR="%OBSPath%\libobs" -DLIBOBS_LIB="%OBSPath%\build64\libobs\%build_config%\obs.lib" -DOBS_FRONTEND_LIB="%OBSPath%\build64\UI\obs-frontend-api\%build_config%\obs-frontend-api.lib" ..
ren "%PluginName%.sln" "main.sln"
