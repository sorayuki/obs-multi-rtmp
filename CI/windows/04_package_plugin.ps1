Param(
    [Switch]$Help = $(if (Test-Path variable:Help) { $Help }),
    [Switch]$Quiet = $(if (Test-Path variable:Quiet) { $Quiet }),
    [Switch]$Verbose = $(if (Test-Path variable:Verbose) { $Verbose }),
    [Switch]$BuildInstaller = $(if ($BuildInstaller.isPresent) { $true }),
    [String]$ProductName = $(if (Test-Path variable:ProductName) { "${ProductName}" } else { "obs-plugin" }),
    [Switch]$CombinedArchs = $(if ($CombinedArchs.isPresent) { $true }),
    [String]$BuildDirectory = $(if (Test-Path variable:BuildDirectory) { "${BuildDirectory}" } else { "build" }),
    [String]$BuildArch = $(if (Test-Path variable:BuildArch) { "${BuildArch}" } else { (Get-WmiObject Win32_OperatingSystem).OSArchitecture}),
    [String]$BuildConfiguration = $(if (Test-Path variable:BuildConfiguration) { "${BuildConfiguration}" } else { "RelWithDebInfo" })
)

##############################################################################
# Windows libobs plugin package function
##############################################################################
#
# This script file can be included in build scripts for Windows or run
# directly with the -Standalone switch
#
##############################################################################

$ErrorActionPreference = "Stop"

function Package-OBS-Plugin {
    Param(
        [String]$BuildDirectory = $(if (Test-Path variable:BuildDirectory) { "${BuildDirectory}" }),
        [String]$BuildArch = $(if (Test-Path variable:BuildArch) { "${BuildArch}" }),
        [String]$BuildConfiguration = $(if (Test-Path variable:BuildConfiguration) { "${BuildConfiguration}" })
    )

    Write-Status "Package plugin ${ProductName}"
    Ensure-Directory ${CheckoutDir}

    if ($CombinedArchs.isPresent) {
        if (!(Test-Path ${CheckoutDir}/release/obs-plugins/64bit)) {
            cmake --build ${BuildDirectory}64 --config ${BuildConfiguration} -t install
        }

        if (!(Test-Path ${CheckoutDir}/release/obs-plugins/32bit)) {
            cmake --build ${BuildDirectory}32 --config ${BuildConfiguration} -t install
        }

        $CompressVars = @{
            Path = "${CheckoutDir}/release/*"
            CompressionLevel = "Optimal"
            DestinationPath = "${FileName}-Windows.zip"
        }

        Write-Step "Creating zip archive..."

        Compress-Archive -Force @CompressVars
        if(($BuildInstaller.isPresent) -And (Test-CommandExists "iscc")) {
            Write-Step "Creating installer..."
            & iscc ${CheckoutDir}/installer/installer-Windows.generated.iss /O. /F"${FileName}-Windows-Installer"
        }
    } elseif ($BuildArch -eq "64-bit") {
        cmake --build ${BuildDirectory}64 --config ${BuildConfiguration} -t install

        $CompressVars = @{
            Path = "${CheckoutDir}/release/*"
            CompressionLevel = "Optimal"
            DestinationPath = "${FileName}-Win64.zip"
        }

        Write-Step "Creating zip archive..."

        Compress-Archive -Force @CompressVars

        if(($BuildInstaller.isPresent) -And (Test-CommandExists "iscc")) {
            Write-Step "Creating installer..."
            & iscc ${CheckoutDir}/installer/installer-Windows.generated.iss /O. /F"${FileName}-Win64-Installer"
        }
    } elseif ($BuildArch -eq "32-bit") {
        cmake --build ${BuildDirectory}32 --config ${BuildConfiguration} -t install

        $CompressVars = @{
            Path = "${CheckoutDir}/release/*"
            CompressionLevel = "Optimal"
            DestinationPath = "${FileName}-Win32.zip"
        }

        Write-Step "Creating zip archive..."

        Compress-Archive -Force @CompressVars

        if(($BuildInstaller.isPresent) -And (Test-CommandExists "iscc")) {
            Write-Step "Creating installer..."
            & iscc ${CheckoutDir}/installer/installer-Windows.generated.iss /O. /F"${FileName}-Win32-Installer"
        }
    }
}

function Package-Plugin-Standalone {
    $CheckoutDir = git rev-parse --show-toplevel

    if (Test-Path ${CheckoutDir}/CI/include/build_environment.ps1) {
        . ${CheckoutDir}/CI/include/build_environment.ps1
    }

    . ${CheckoutDir}/CI/include/build_support_windows.ps1

    Ensure-Directory ${CheckoutDir}
    $GitBranch = git rev-parse --abbrev-ref HEAD
    $GitHash = git rev-parse --short HEAD
    $ErrorActionPreference = "SilentlyContiue"
    $GitTag = git describe --tags --abbrev=0
    $ErrorActionPreference = "Stop"

    if ($GitTag -eq $null) {
        $GitTag=$ProductVersion
    }

    $FileName = "${ProductName}-${GitTag}-${GitHash}"

    Package-OBS-Plugin
}

function Print-Usage {
    $Lines = @(
        "Usage: ${MyInvocation.MyCommand.Name}",
        "-Help                    : Print this help",
        "-Quiet                   : Suppress most build process output",
        "-Verbose                 : Enable more verbose build process output",
        "-CombinedArchs           : Create combined architecture package",
        "-BuildDirectory          : Directory to use for builds - Default: build64 on 64-bit systems, build32 on 32-bit systems",
        "-BuildArch               : Build architecture to use (32bit or 64bit) - Default: local architecture",
        "-BuildConfiguration      : Build configuration to use - Default: RelWithDebInfo"
    )

    $Lines | Write-Host
}


if(!(Test-Path variable:_RunObsBuildScript)) {
    if($Help.isPresent) {
        Print-Usage
        exit 0
    }

    Package-Plugin-Standalone
}
