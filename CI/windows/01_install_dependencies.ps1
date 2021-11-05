Param(
    [Switch]$Help = $(if (Test-Path variable:Help) { $Help }),
    [Switch]$Quiet = $(if (Test-Path variable:Quiet) { $Quiet }),
    [Switch]$Verbose = $(if (Test-Path variable:Verbose) { $Verbose }),
    [Switch]$NoChoco = $(if (Test-Path variable:NoChoco) { $true } else { $false }),
    [String]$BuildArch = $(if (Test-Path variable:BuildArch) { "${BuildArch}" } else { (Get-CimInstance CIM_OperatingSystem).OSArchitecture }),
    [String]$ProductName = $(if (Test-Path variable:ProductName) { "${ProductName}" } else { "obs-plugin" })
)

##############################################################################
# Windows dependency management function
##############################################################################
#
# This script file can be included in build scripts for Windows or run
# directly
#
##############################################################################

$ErrorActionPreference = "Stop"

Function Install-obs-deps {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$Version
    )

    Write-Status "Setup for pre-built Windows OBS dependencies v${Version}"
    Ensure-Directory $DepsBuildDir

    if (!(Test-Path $DepsBuildDir/dependencies${Version})) {
        Write-Status "Setting up pre-built Windows OBS dependencies v${Version}"

        Write-Step "Download..."
        $ProgressPreference = $(if ($Quiet.isPresent) { "SilentlyContinue" } else { "Continue" })
        Invoke-WebRequest -Uri "https://cdn-fastly.obsproject.com/downloads/dependencies${Version}.zip" -UseBasicParsing -OutFile "dependencies${Version}.zip"
        $ProgressPreference = 'Continue'

        Write-Step "Unpack..."

        Expand-Archive -Path "dependencies${Version}.zip"
    } else {
        Write-Step "Found existing pre-built dependencies..."

    }
}

function Install-qt-deps {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$Version
    )

    Write-Status "Setup for pre-built dependency Qt v${Version}"
    Ensure-Directory $DepsBuildDir

    if (!(Test-Path $DepsBuildDir/Qt_${Version})) {
        Write-Status "Setting up OBS dependency Qt v${Version}"

        Write-Step "Download..."
        $ProgressPreference = $(if ($Quiet.isPresent) { 'SilentlyContinue' } else { 'Continue' })
        Invoke-WebRequest -Uri "https://cdn-fastly.obsproject.com/downloads/Qt_${Version}.7z" -UseBasicParsing -OutFile "Qt_${Version}.7z"
        $ProgressPreference = 'Continue'
        
        Write-Step "Unpack..."

        # TODO: Replace with zip and properly package Qt to share directory with other deps
        & 7z x Qt_${Version}.7z
        & mv ${Version} "Qt_${Version}"
    } else {
        Write-Step "Found existing pre-built Qt..."
    }
}

function Install-obs-studio {
    Param(
        [parameter(Mandatory=$true)]
        [string]$Version
    )

    $CheckoutRef = "$(if (!(Test-Path variable:OBSBranch)) { ${OBSBranch} } else { "tags/${OBSVersion}" })"

    Write-Status "Setup for OBS Studio v${CheckoutRef}"
    Ensure-Directory ${ObsBuildDir}

    if (!(Test-Path "${ObsBuildDir}/.git")) {
        & git clone --recursive https://github.com/obsproject/obs-studio "${pwd}"
        & git fetch origin --tags
        & git checkout ${CheckoutRef} -b obs-plugin-build
    } else {
        $BranchExists = &git show-ref --verify --quiet refs/heads/obs-plugin-build

        if ($BranchExists -Eq $false) {
            & git checkout ${CheckoutRef} -b obs-plugin-build
        } else {
            & git checkout obs-plugin-build
        }
    }
}

function Install-Dependencies {
    if(!($NoChoco.isPresent)) {
        Install-Windows-Dependencies
    }

    $BuildDependencies = @(
        @('obs-deps', $WindowsDepsVersion),
        @('qt-deps', $WindowsQtVersion),
        @('obs-studio', $OBSVersion)
    )

    Foreach($Dependency in ${BuildDependencies}) {
        $DependencyName = $Dependency[0]
        $DependencyVersion = $Dependency[1]

        $FunctionName = "Install-${DependencyName}"
        & $FunctionName -Version $DependencyVersion
    }

    Ensure-Directory ${CheckoutDir}
}

function Install-Dependencies-Standalone {
    $CheckoutDir = git rev-parse --show-toplevel

    if (Test-Path ${CheckoutDir}/CI/include/build_environment.ps1) {
        . ${CheckoutDir}/CI/include/build_environment.ps1
    }

    $DepsBuildDir = "${CheckoutDir}/../obs-build-dependencies"
    $ObsBuildDir = "${CheckoutDir}/../obs-studio"

    . ${CheckoutDir}/CI/include/build_support_windows.ps1

    Write-Status "Setting up plugin build dependencies"
    Install-Dependencies
}

function Print-Usage {
    $Lines = @(
        "Usage: ${MyInvocation.MyCommand.Name}",
        "-Help                    : Print this help",
        "-Quiet                   : Suppress most build process output",
        "-Verbose                 : Enable more verbose build process output",
        "-NoChoco                 : Skip automatic dependency installation via Chocolatey - Default: on"
        "-BuildArch               : Build architecture to use (32bit or 64bit) - Default: local architecture"
    )

    $Lines | Write-Host
}

if(!(Test-Path variable:_RunObsBuildScript)) {
    if ($Help.isPresent) {
        Print-Usage
        exit 0
    }

    Install-Dependencies-Standalone
}
