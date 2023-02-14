function Install-BuildDependencies {
    <#
        .SYNOPSIS
            Installs required build dependencies.
        .DESCRIPTION
            Additional packages might be needed for successful builds. This module contains additional
            dependencies available for installation via winget and, if possible, adds their locations
            to the environment path for future invocation.
        .EXAMPLE
            Install-BuildDependencies
    #>

    param(
        [string] $WingetFile = "$PSScriptRoot/.Wingetfile"
    )

    if ( ! ( Test-Path function:Log-Warning ) ) {
        . $PSScriptRoot/Logger.ps1
    }
    
    $Host64Bit = [System.Environment]::Is64BitOperatingSystem

    $Paths = $Env:Path -split [System.IO.Path]::PathSeparator

    $WingetOptions = @('install', '--accept-package-agreements', '--accept-source-agreements')

    if ( $script:Quiet ) {
        $WingetOptions += '--silent'
    }

    Get-Content $WingetFile | ForEach-Object {
        $_, $Package, $_, $Path, $_, $Binary = ([regex]::Split($_, " (?=(?:[^']|'[^']*')*$)")) -replace ',', '' -replace "'",''

        (${Env:ProgramFiles(x86)}, $Env:ProgramFiles) | ForEach-Object {
            $Prefix = $_
            $FullPath = "${Prefix}\${Path}"
            if ( ( Test-Path $FullPath  ) -and ! ( $Paths -contains $FullPath ) ) {
                $Paths += $FullPath
                $Env:Path = $Paths -join [System.IO.Path]::PathSeparator
            }
        }

        Log-Debug "Checking for command ${Binary}"
        $Found = Get-Command -ErrorAction SilentlyContinue $Binary

        if ( $Found ) {
            Log-Status "Found dependency ${Binary} as $($Found.Source)"
        } else {
            Log-Status "Installing package ${Package}"

            try {
                $Params = $WingetOptions + $Package

                winget @Params
            } catch {
                throw "Error while installing winget package ${Package}: $_"
            }
        }
    }
}
