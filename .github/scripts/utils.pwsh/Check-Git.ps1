function Check-Git {
    <#
        .SYNOPSIS
            Ensures available git executable on host system.
        .DESCRIPTION
            Checks whether a git command is available on the host system. If none is found,
            Git is installed via winget.
        .EXAMPLE
            Check-Git
    #>

    if ( ! ( Test-Path function:Log-Info ) ) {
        . $PSScriptRoot/Logger.ps1
    }

    Log-Information 'Checking for Git executable...'

    if ( ! ( Get-Command git ) ) {
        Log-Warning 'No Git executable found. Will try to install via winget.'
        winget install git
    } else {
        Log-Debug "Git found at $(Get-Command git)."
        Log-Status "Git found."
    }
}
