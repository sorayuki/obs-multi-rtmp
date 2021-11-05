Param(
    [Switch]$Help,
    [Switch]$Quiet,
    [Switch]$Verbose,
    [Switch]$NoChoco,
    [Switch]$SkipDependencyChecks,
    [Switch]$BuildInstaller,
    [Switch]$CombinedArchs,
    [String]$BuildDirectory = "build",
    [String]$BuildArch = (Get-CimInstance CIM_OperatingSystem).OSArchitecture,
    [String]$BuildConfiguration = "RelWithDebInfo"
)

##############################################################################
# Windows plugin build script
##############################################################################
#
# This script contains all steps necessary to:
#
#   * Build libobs and obs-frontend-api with all required dependencies
#   * Build your plugin
#   * Create 64-bit or 32-bit packages
#   * Create 64-bit or 32-bit installation packages
#   * Create combined installation packages
#
# Parameters:
#   -Help                   : Print usage help
#   -NoChco                 : Skip automatic dependency installation
#                             via Chocolatey
#   -SkipDependencyChecks   : Skips dependency checks
#   -BuildDirectory         : Directory to use for builds
#                             Default: Win64 on 64-bit systems
#                                      Win32 on 32-bit systems
#  -BuildArch               : Build architecture to use (32bit or 64bit)
#  -BuildConfiguration      : Build configuration to use
#                             Default: RelWithDebInfo
#  -BuildInstaller          : Build InnoSetup installer - Default: off"
#  -CombinedArchs           : Create combined packages and installer
#                             (64-bit and 32-bit) - Default: off"
#
# Environment Variables (optional):
#  WindowsDepsVersion       : Pre-compiled Windows dependencies version
#  WindowsQtVersion         : Pre-compiled Qt version
#  ObsVersion               : OBS Version
#
##############################################################################

$ErrorActionPreference = "Stop"

$_RunObsBuildScript = $true

$CheckoutDir = git rev-parse --show-toplevel

$DepsBuildDir = "${CheckoutDir}/../obs-build-dependencies"
$ObsBuildDir = "${CheckoutDir}/../obs-studio"

if (Test-Path ${CheckoutDir}/CI/include/build_environment.ps1) {
    . ${CheckoutDir}/CI/include/build_environment.ps1
}

. ${CheckoutDir}/CI/include/build_support_windows.ps1

## DEPENDENCY INSTALLATION ##
. ${CheckoutDir}/CI/windows/01_install_dependencies.ps1

## OBS LIBRARY BUILD ##
. ${CheckoutDir}/CI/windows/02_build_obs_libs.ps1

## PLUGIN BUILD ##
. ${CheckoutDir}/CI/windows/03_build_plugin.ps1

## PLUGIN PACKAGE AND NOTARIZE ##
. ${CheckoutDir}/CI/windows/04_package_plugin.ps1

## MAIN SCRIPT FUNCTIONS ##
function Build-Obs-Plugin-Main {
    Ensure-Directory ${CheckoutDir}
    Write-Step "Fetching version tags..."
    & git fetch origin --tags
    $GitBranch = git rev-parse --abbrev-ref HEAD
    $GitHash = git rev-parse --short HEAD
    $ErrorActionPreference = "SilentlyContiue"
    $GitTag = git describe --tags --abbrev=0
    $ErrorActionPreference = "Stop"

    if ($GitTag -eq $null) {
        $GitTag=$ProductVersion
    }

    $FileName = "${ProductName}-${GitTag}-${GitHash}"

    if(!($SkipDependencyChecks.isPresent)) {
        Install-Dependencies -NoChoco:$NoChoco
    }

    if($CombinedArchs.isPresent) {
        Build-OBS-Libs -BuildArch 64-bit
        Build-OBS-Libs -BuildArch 32-bit
        Build-OBS-Plugin -BuildArch 64-bit
        Build-OBS-Plugin -BuildArch 32-bit
    } else {
        Build-OBS-Libs
        Build-OBS-Plugin
    }

    Package-OBS-Plugin
}

function Print-Usage {
    Write-Host "build-windows.ps1 - Build script for ${ProductName}"
    $Lines = @(
        "Usage: ${MyInvocation.MyCommand.Name}",
        "-Help                    : Print this help",
        "-Quiet                   : Suppress most build process output"
        "-Verbose                 : Enable more verbose build process output"
        "-NoChoco                 : Skip automatic dependency installation via Chocolatey - Default: on",
        "-SkipDependencyChecks    : Skips dependency checks - Default: off",
        "-BuildDirectory          : Directory to use for builds - Default: build64 on 64-bit systems, build32 on 32-bit systems",
        "-BuildArch               : Build architecture to use (32bit or 64bit) - Default: local architecture",
        "-BuildConfiguration      : Build configuration to use - Default: RelWithDebInfo",
        "-BuildInstaller          : Build InnoSetup installer - Default: off",
        "-CombinedArchs           : Create combined packages and installer (64-bit and 32-bit) - Default: off"
    )
    $Lines | Write-Host
}

if($Help.isPresent) {
    Print-Usage
    exit 0
}

Build-Obs-Plugin-Main
