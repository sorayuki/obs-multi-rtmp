function Setup-Host {
    if ( ! ( Test-Path function:Log-Output ) ) {
        . $PSScriptRoot/Logger.ps1
    }

    if ( ! ( Test-Path function:Ensure-Location ) ) {
        . $PSScriptRoot/Ensure-Location.ps1
    }

    if ( ! ( Test-Path function:Install-BuildDependencies ) ) {
        . $PSScriptRoot/Install-BuildDependencies.ps1
    }

    Install-BuildDependencies -WingetFile "${ScriptHome}/.Wingetfile"

    if ( $script:Target -eq '' ) { $script:Target = $script:HostArchitecture }

    Push-Location -Stack BuildTemp
    Log-Information 'Setting up obs-deps...'
    $DepsVersion = $BuildSpec.dependencies.'obs-deps'."windows-${script:Target}".version
    $DepsHash = $BuildSpec.dependencies.'obs-deps'."windows-${script:Target}".hash

    if ( ${DepsVersion} -eq '' ) {
        throw 'No obs-deps version found in buildspec.json.'
    }
    Log-Status 'Found obs-deps specification.'

    Ensure-Location -Path "$(Resolve-Path -Path "${ProjectRoot}/..")/obs-build-dependencies"

    if ( ! ( Test-Path -Path "windows-deps-${DepsVersion}-${script:Target}.zip" ) ) {
        $Params = @{
            UserAgent = 'NativeHost'
            Uri = "https://github.com/obsproject/obs-deps/releases/download/win-${DepsVersion}/windows-deps-${DepsVersion}-${script:Target}.zip"
            OutFile = "windows-deps-${DepsVersion}-${script:Target}.zip"
            UseBasicParsing = $true
            ErrorAction = 'Stop'
        }

        Invoke-WebRequest @Params
        Log-Status "Downloaded obs-deps for ${script:Target}."
    } else {
        Log-Status 'Found downloaded obs-deps.'
    }

    $FileHash = Get-FileHash -Path "windows-deps-${DepsVersion}-${script:Target}.zip" -Algorithm SHA256

    if ( $FileHash.Hash.ToLower() -ne $DepsHash ) {
        throw "Checksum mismatch of obs-deps download. Expected '${DepsHash}', found '$(${FileHash}.Hash.ToLower())'"
    }
    Log-Status 'Checksum of downloaded obs-deps matches.'

    Ensure-Location -Path "plugin-deps-${Target}"

    if ( ! ( Test-Path function:Expand-ArchiveExt ) ) {
        . $PSScriptRoot/Expand-ArchiveExt.ps1
    }

    Log-Information 'Extracting obs-deps...'
    Expand-ArchiveExt -Path "../windows-deps-${DepsVersion}-${script:Target}.zip" -DestinationPath . -Force
    Pop-Location -Stack BuildTemp

    Push-Location -Stack BuildTemp
    Log-Information 'Setting up Qt...'
    $QtVersion = $BuildSpec.dependencies.'obs-deps-qt'."windows-${script:Target}".version
    $QtHash = $BuildSpec.dependencies.'obs-deps-qt'."windows-${script:Target}".hash

    if ( ${QtVersion} -eq '' ) {
        throw 'No Qt version found in buildspec.json.'
    }
    Log-Status 'Found Qt specification.'

    Ensure-Location -Path "$(Resolve-Path -Path "${ProjectRoot}/..")/obs-build-dependencies"

    if ( ! ( Test-Path -Path "windows-deps-qt-${DepsVersion}-${script:Target}.zip" ) ) {
        $Params = @{
            UserAgent = 'NativeHost'
            Uri = "https://cdn-fastly.obsproject.com/downloads/windows-deps-qt-${DepsVersion}-${script:Target}.zip"
            OutFile = "windows-deps-qt-${DepsVersion}-${script:Target}.zip"
            UseBasicParsing = $true
            ErrorAction = 'Stop'
        }

        Invoke-WebRequest @Params
        Log-Status "Downloaded Qt for ${script:Target}."
    } else {
        Log-Status 'Found downloaded Qt.'
    }

    $FileHash = Get-FileHash -Path "windows-deps-qt-${DepsVersion}-${script:Target}.zip" -Algorithm SHA256

    if ( $FileHash.Hash.ToLower() -ne ${QtHash} ) {
        throw "Checksum mismatch of Qt download. Expected '${QtHash}', found '$(${FileHash}.Hash.ToLower())'"
    }
    Log-Status 'Checksum of downloaded Qt matches.'

    Ensure-Location -Path "plugin-deps-${Target}"

    Log-Information 'Extracting Qt...'
    Expand-ArchiveExt -Path "../windows-deps-qt-${DepsVersion}-${script:Target}.zip" -DestinationPath . -Force
    Pop-Location -Stack BuildTemp
}

function Get-HostArchitecture {
    $Host64Bit = [System.Environment]::Is64BitOperatingSystem
    $HostArchitecture = ('x86', 'x64')[$Host64Bit]

    return $HostArchitecture
}

$script:HostArchitecture = Get-HostArchitecture
