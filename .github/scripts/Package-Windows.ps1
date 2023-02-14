[CmdletBinding()]
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [ValidateSet('x86', 'x64', 'x86+x64')]
    [string] $Target,
    [switch] $BuildInstaller = $false
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

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    Install-BuildDependencies -WingetFile "${ScriptHome}/.Wingetfile"

    Log-Information "Packaging ${ProductName}..."

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
            "${ProjectRoot}/release/${ProductName}-*-windows-*.exe"
        )
    }

    Remove-Item @RemoveArgs

    if ( ( $BuildInstaller ) ) {
        if ( $Target -eq 'x86+x64' ) {
            $IsccCandidates = Get-ChildItem -Recurse -Path '*.iss'

            if ( $IsccCandidates.length -gt 0 ) {
                $IsccFile = $IsccCandidates[0].FullName
            } else {
                $IsccFile = ''
            }
        } else {
            $IsccFile = "${ProjectRoot}/build_${Target}/installer-Windows.generated.iss"
        }

        if ( ! ( Test-Path -Path $IsccFile ) ) {
            throw 'InnoSetup install script not found. Run the build script or the CMake build and install procedures first.'
        }

        Log-Information 'Creating InnoSetup installer...'
        Push-Location -Stack BuildTemp
        Ensure-Location -Path "${ProjectRoot}/release"
        Invoke-External iscc ${IsccFile} /O. /F"${OutputName}-Installer"
        Pop-Location -Stack BuildTemp
    }

    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
    }

    Compress-Archive -Force @CompressArgs
}

Package
