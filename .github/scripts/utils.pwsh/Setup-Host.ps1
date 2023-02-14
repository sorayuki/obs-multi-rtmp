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

    if ( ! ( Test-Path function:Expand-ArchiveExt ) ) {
        . $PSScriptRoot/Expand-ArchiveExt.ps1
    }

    Install-BuildDependencies -WingetFile "${ScriptHome}/.Wingetfile"

    if ( $script:Target -eq '' ) { $script:Target = $script:HostArchitecture }

    $script:QtVersion = $BuildSpec.platformConfig."windows-${script:Target}".qtVersion
    $script:VisualStudioVersion = $BuildSpec.platformConfig."windows-${script:Target}".visualStudio
    $script:PlatformSDK = $BuildSpec.platformConfig."windows-${script:Target}".platformSDK

    if ( ! ( ( $script:SkipAll ) -or ( $script:SkipDeps ) ) ) {
        ('prebuilt', "qt${script:QtVersion}") | ForEach-Object {
            $_Dependency = $_
            $_Version = $BuildSpec.dependencies."${_Dependency}".version
            $_BaseUrl = $BuildSpec.dependencies."${_Dependency}".baseUrl
            $_Label = $BuildSpec.dependencies."${_Dependency}".label
            $_Hash = $BuildSpec.dependencies."${_Dependency}".hashes."windows-${script:Target}"

            if ( $BuildSpec.dependencies."${_Dependency}".PSobject.Properties.Name -contains "pdb-hashes" ) {
                $_PdbHash = $BuildSpec.dependencies."${_Dependency}".'pdb-hashes'."$windows-${script:Target}"
            }

            if ( $_Version -eq '' ) {
                throw "No ${_Dependency} spec found in ${script:BuildSpecFile}."
            }

            Log-Information "Setting up ${_Label}..."

            Push-Location -Stack BuildTemp
            Ensure-Location -Path "$(Resolve-Path -Path "${ProjectRoot}/..")/obs-build-dependencies"

            switch -wildcard ( $_Dependency ) {
                prebuilt {
                    $_Filename = "windows-deps-${_Version}-${script:Target}.zip"
                    $_Uri = "${_BaseUrl}/${_Version}/${_Filename}"
                    $_Target = "plugin-deps-${_Version}-qt${script:QtVersion}-${script:Target}"
                    $script:DepsVersion = ${_Version}
                }
                "qt*" {
                    $_Filename = "windows-deps-qt${script:QtVersion}-${_Version}-${script:Target}.zip"
                    $_Uri = "${_BaseUrl}/${_Version}/${_Filename}"
                    $_Target = "plugin-deps-${_Version}-qt${script:QtVersion}-${script:Target}"
                }
            }

            if ( ! ( Test-Path -Path $_Filename ) ) {
                $Params = @{
                    UserAgent = 'NativeHost'
                    Uri = $_Uri
                    OutFile = $_Filename
                    UseBasicParsing = $true
                    ErrorAction = 'Stop'
                }

                Invoke-WebRequest @Params
                Log-Status "Downloaded ${_Label} for ${script:Target}."
            } else {
                Log-Status "Found downloaded ${_Label}."
            }

            $_FileHash = Get-FileHash -Path $_Filename -Algorithm SHA256

            if ( $_FileHash.Hash.ToLower() -ne $_Hash ) {
                throw "Checksum of downloaded ${_Label} does not match specification. Expected '${_Hash}', 'found $(${_FileHash}.Hash.ToLower())'"
            }
            Log-Status "Checksum of downloaded ${_Label} matches."

            if ( ! ( ( $script:SkipAll ) -or ( $script:SkipUnpack ) ) ) {
                Push-Location -Stack BuildTemp
                Ensure-Location -Path $_Target

                Expand-ArchiveExt -Path "../${_Filename}" -DestinationPath . -Force

                Pop-Location -Stack BuildTemp
            }
            Pop-Location -Stack BuildTemp
        }
    }
}

function Get-HostArchitecture {
    $Host64Bit = [System.Environment]::Is64BitOperatingSystem
    $HostArchitecture = ('x86', 'x64')[$Host64Bit]

    return $HostArchitecture
}

$script:HostArchitecture = Get-HostArchitecture
