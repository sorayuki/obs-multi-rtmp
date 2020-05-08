call "ci\ci_includes.generated.cmd"

cd build32
cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_SYSTEM_VERSION=10.0 -DQTDIR="%QTDIR32%" -DLibObs_DIR="%OBSPath%\build32\libobs" -DLIBOBS_INCLUDE_DIR="%OBSPath%\libobs" -DLIBOBS_LIB="%OBSPath%\build32\libobs\%build_config%\obs.lib" -DOBS_FRONTEND_LIB="%OBSPath%\build32\UI\obs-frontend-api\%build_config%\obs-frontend-api.lib" ..
cd ..\build64
cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_SYSTEM_VERSION=10.0 -DQTDIR="%QTDIR64%" -DLibObs_DIR="%OBSPath%\build64\libobs" -DLIBOBS_INCLUDE_DIR="%OBSPath%\libobs" -DLIBOBS_LIB="%OBSPath%\build64\libobs\%build_config%\obs.lib" -DOBS_FRONTEND_LIB="%OBSPath%\build64\UI\obs-frontend-api\%build_config%\obs-frontend-api.lib" ..

REM Rename the main project file to something CI can pick up independently of the project's name
cd ..
ren "build32\%PluginName%.sln" "build32\main.sln"
ren "build64\%PluginName%.sln" "build64\main.sln"²