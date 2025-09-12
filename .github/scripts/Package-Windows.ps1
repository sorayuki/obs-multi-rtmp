[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
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

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
			"${ProjectRoot}/release/${ProductName}-*-windows-*.exe"
        )
    }

    Remove-Item @RemoveArgs

    Log-Group "Archiving ${ProductName}..."
    # $CompressArgs = @{
    #     Path = (Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Exclude "${OutputName}*.*")
    #     CompressionLevel = 'Optimal'
    #     DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
    #     Verbose = ($Env:CI -ne $null)
    # }
    # Compress-Archive -Force @CompressArgs
    
    if (-not ([AppDomain]::CurrentDomain.GetAssemblies() | Where-Object { $_.GetType('System.IO.Compression.ZipFile', $false) })) {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
    }
    $zipf = New-Object System.IO.Compression.ZipArchive -ArgumentList ([System.IO.File]::OpenWrite("${ProjectRoot}/release/${OutputName}.zip"), [System.IO.Compression.ZipArchiveMode]::Create)
    function AddDirectory{
        param( [string] $DestDir, [string] $SourceDir )
        Log-Information "Adding directory: ${SourceDir} to ${DestDir}"
        $Entries = Get-ChildItem -Path $SourceDir -Recurse
        foreach( $Entry in $Entries ) {
            $RelativePath = $Entry.FullName.Substring($SourceDir.Length).TrimStart('\')
            $ZipEntryPath = "$DestDir/${RelativePath}"
            if ( $Entry.PSIsContainer -eq $false ) {
                Log-Information "Adding file entry: ${ZipEntryPath}"
                $stream = $zipf.CreateEntry($ZipEntryPath, [System.IO.Compression.CompressionLevel]::Optimal)
                $fileStream = [System.IO.File]::OpenRead($Entry.FullName)
                $entryStream = $stream.Open()
                $fileStream.CopyTo($entryStream)
                $entryStream.Close()
                $fileStream.Close()
            }
        }
    }
    AddDirectory "obs-plugins/64bit" "${ProjectRoot}/release/${Configuration}/${ProductName}/bin/64bit"
    AddDirectory "data/obs-plugins/${ProductName}" "${ProjectRoot}/release/${Configuration}/${ProductName}/data"
    $zipf.Dispose()
    Log-Group

    $NsiFile = "${ProjectRoot}/installer.nsi"
    Log-Information 'Creating NSIS installer...'

    Push-Location -Stack BuildTemp
    Ensure-Location -Path "${ProjectRoot}/release"
    Invoke-External "makensis.exe" ${NsiFile}
    Copy-Item -Path "${ProjectRoot}/obs-multi-rtmp-setup.exe" -Destination "${OutputName}-Installer.exe"
    Pop-Location -Stack BuildTemp

    Log-Group
}

Package
