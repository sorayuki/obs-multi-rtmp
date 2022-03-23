function Setup-Obs {
    if ( ! ( Test-Path function:Log-Output ) ) {
        . $PSScriptRoot/Logger.ps1
    }

    if ( ! ( Test-Path function:Check-Git ) ) {
        . $PSScriptRoot/Check-Git.ps1
    }

    Check-Git

    if ( ! ( Test-Path function:Ensure-Location ) ) {
        . $PSScriptRoot/Ensure-Location.ps1
    }

    if ( ! ( Test-Path function:Invoke-GitCheckout ) ) {
        . $PSScriptRoot/Invoke-GitCheckout.ps1
    }

    if ( ! ( Test-Path function:Invoke-External ) ) {
        . $PSScriptRoot/Invoke-External.ps1
    }

    Log-Information 'Setting up OBS Studio...'

    $ObsVersion = $BuildSpec.dependencies.'obs-studio'.version
    $ObsRepository = $BuildSpec.dependencies.'obs-studio'.repository
    $ObsBranch = $BuildSpec.dependencies.'obs-studio'.branch
    $ObsHash = $BuildSpec.dependencies.'obs-studio'.hash

    if ( $ObsVersion -eq '' ) {
        throw 'No obs-studio version found in buildspec.json.'
    }

    Push-Location -Stack BuildTemp
    Ensure-Location -Path "$(Resolve-Path -Path "${ProjectRoot}/../")/obs-studio"

    Invoke-GitCheckout -Uri $ObsRepository -Commit $ObsHash -Path . -Branch $ObsBranch

    Log-Information 'Configuring OBS Studio...'

    $NumProcessors = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors

    if ( $NumProcessors -gt 1 ) {
        $env:UseMultiToolTask = $true
        $env:EnforceProcessCountAcrossBuilds = $true
    }

    $CmakeArgs = @(
        '-G', $script:CmakeGenerator
        '-DCMAKE_SYSTEM_VERSION=10.0.18363.657'
        "-DCMAKE_GENERATOR_PLATFORM=$(if (${script:Target} -eq "x86") { "Win32" } else { "x64" })"
        "-DCMAKE_BUILD_TYPE=${script:Configuration}"
        '-DENABLE_PLUGINS=OFF'
        '-DENABLE_UI=OFF'
        '-DENABLE_SCRIPTING=OFF'
        "-DCMAKE_INSTALL_PREFIX=$(Resolve-Path -Path "${ProjectRoot}/../obs-build-dependencies/plugin-deps-${Target}")"
        "-DCMAKE_PREFIX_PATH=$(Resolve-Path -Path "${ProjectRoot}/../obs-build-dependencies/plugin-deps-${Target}")"
    )

    Invoke-External cmake -S . -B plugin_${script:Target} @CmakeArgs

    Log-Information 'Building libobs and obs-frontend-api...'

    $CmakeArgs = @(
        '--config', "$( if ( $script:Configuration -eq '' ) { 'RelWithDebInfo' } else { $script:Configuration })"
    )

    if ( $VerbosePreference -eq 'Continue' ) {
        $CmakeArgs+=('--verbose')
    }

    Invoke-External cmake --build plugin_${script:Target} @CmakeArgs -t obs-frontend-api
    Invoke-External cmake --install plugin_${script:Target} @CmakeArgs --component obs_libraries

    Pop-Location -Stack BuildTemp
}
