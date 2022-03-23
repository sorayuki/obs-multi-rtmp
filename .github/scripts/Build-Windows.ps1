[CmdletBinding()]
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [ValidateSet('x86', 'x64')]
    [string] $Target,
    [ValidateSet('Visual Studio 17 2022', 'Visual Studio 16 2019')]
    [string] $CMakeGenerator = 'Visual Studio 16 2019'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $PSVersionTable.PSVersion -lt '7.0.0' ) {
    Write-Warning 'The obs-deps PowerShell build script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Build {
    trap {
        Pop-Location -Stack BuildTemp
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach($Utility in $UtilityFunctions) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    Setup-Host

    (Get-Content -Path ${ProjectRoot}/CMakeLists.txt -Raw) `
        -replace "project\((.*) VERSION (.*)\)", "project(${ProductName} VERSION ${ProductVersion})" `
        | Out-File -Path ${ProjectRoot}/CMakeLists.txt

    Setup-Obs

    Push-Location -Stack BuildTemp

    Ensure-Location $ProjectRoot

    $CmakeArgs = @(
        '-G', $CmakeGenerator
        '-DCMAKE_SYSTEM_VERSION=10.0.18363.657'
        "-DCMAKE_GENERATOR_PLATFORM=$(if (${script:Target} -eq "x86") { "Win32" } else { "x64" })"
        "-DCMAKE_BUILD_TYPE=${Configuration}"
        "-DCMAKE_PREFIX_PATH=$(Resolve-Path -Path "${ProjectRoot}/../obs-build-dependencies/plugin-deps-${Target}")"
    )

    Log-Information "Configuring ${ProductName}..."
    Invoke-External cmake -S . -B build_${script:Target} @CmakeArgs

    $CmakeArgs = @(
        '--config', "${Configuration}"
    )

    if ( $VerbosePreference -eq 'Continue' ) {
        $CmakeArgs+=('--verbose')
    }

    Log-Information "Building ${ProductName}..."
    Invoke-External cmake --build "build_${script:Target}" @CmakeArgs
    Log-Information "Install ${ProductName}..."
    Invoke-External cmake --install "build_${script:Target}" --prefix "${ProjectRoot}/release" @CmakeArgs

    Pop-Location -Stack BuildTemp
}

Build
