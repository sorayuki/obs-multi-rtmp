function Set-GitConfig {
    <#
        .SYNOPSIS
            Sets a git config value.
        .DESCRIPTION
            Allows setting single or multiple config values in a PowerShell-friendly fashion.
        .EXAMPLE
            Set-GitConfig advice.detachedHead false
    #>

    if ( $args.Count -lt 2 ) {
        throw 'Set-GitConfig called without required arguments <OPTION> <VALUE>.'
    }

    Invoke-External git config @args
}

function Invoke-GitCheckout {
    <#
        .SYNOPSIS
            Checks out a specified git repository.
        .DESCRIPTION
            Wraps the git executable with PowerShell syntax to check out
            a specified Git repository with a given commit hash and branch,
            or a GitHub pull request ID.
        .EXAMPLE
            Invoke-GitCheckout -Uri "My-Repo-Uri" -Commit "My-Commit-Hash"
            Invoke-GitCheckout -Uri "My-Repo-Uri" -Commit "My-Commit-Hash" -Branch "main"
            Invoke-GitCheckout -Uri "My-Repo-Uri" -Commit "My-Commit-Hash" -PullRequest 250
    #>

    param(
        [Parameter(Mandatory)]
        [string] $Uri,
        [Parameter(Mandatory)]
        [string] $Commit,
        [string] $Path,
        [string] $Branch = "master",
        [string] $PullRequest
    )

    if ( ! ( $Uri -like "*github.com*" ) -and ( $PullRequest -ne "" ) ) {
        throw 'Fetching pull requests is only supported with GitHub-based repositories.'
    }

    if ( ! ( Test-Path function:Log-Information ) ) {
        . $PSScriptRoot/Logger.ps1
    }

    if ( ! ( Test-Path function:Invoke-External ) ) {
        . $PSScriptRoot/Invoke-External.ps1
    }

    $RepositoryName = [System.IO.Path]::GetFileNameWithoutExtension($Uri)

    if ( $Path -eq "" ) {
        $Path = "$(Get-Location | Convert-Path)\${RepositoryName}"
    }

    Push-Location -Stack GitCheckoutTemp

    if ( Test-Path $Path/.git ) {
        Write-Information "Repository ${RepositoryName} found in ${Path}"

        Set-Location $Path

        Set-GitConfig advice.detachedHead false
        Set-GitConfig remote.origin.url $Uri
        Set-GitConfig remote.origin.tapOpt --no-tags

        $Ref = "+refs/heads/{0}:refs/remotes/origin/{0}" -f $Branch

        Set-GitConfig --replace-all remote.origin.fetch $Ref

        if ( $PullRequest -ne "" ) {
            try {
                Invoke-External git show-ref --quiet --verify refs/heads/pr-$PullRequest
            } catch {
                Invoke-External git fetch origin $("pull/{0}/head:pull-{0}" -f $PullRequest)
            } finally {
                Invoke-External git checkout -f "pull-${PullRequest}"
            }
        }

        try {
            $null = Invoke-External git rev-parse -q --verify "${Commit}^{commit}"
        } catch {
            Invoke-External git fetch origin
        }

        Invoke-External git checkout -f $Commit -- | Log-Information
    } else {
        Invoke-External git clone $Uri $Path

        Set-Location $Path

        Set-GitConfig advice.detachedHead false

        if ( $PullRequest -ne "" ) {
            $Ref = "pull/{0}/head:pull-{0}" -f $PullRequest
            $Branch = "pull-${PullRequest}"
            Invoke-External git fetch origin $Ref
            Invoke-External git checkout $Branch
        }

        Invoke-External git checkout -f $Commit
    }

    Log-Information "Checked out commit ${Commit} on branch ${Branch}"

    if ( Test-Path ${Path}/.gitmodules ) {
        Invoke-External git submodule foreach --recursive git submodule sync
        Invoke-External git submodule update --init --recursive
    }

    Pop-Location -Stack GitCheckoutTemp
}
