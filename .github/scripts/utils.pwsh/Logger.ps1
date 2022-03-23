function Log-Debug {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory,ValueFromPipeline)]
        [ValidateNotNullOrEmpty()]
        [string[]] $Message
    )

    Process {
        foreach($m in $Message) {
            Write-Debug $m
        }
    }
}

function Log-Verbose {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory,ValueFromPipeline)]
        [ValidateNotNullOrEmpty()]
        [string[]] $Message
    )

    Process {
        foreach($m in $Message) {
            Write-Verbose $m
        }
    }
}

function Log-Warning {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory,ValueFromPipeline)]
        [ValidateNotNullOrEmpty()]
        [string[]] $Message
    )

    Process {
        foreach($m in $Message) {
            Write-Warning $m
        }
    }
}

function Log-Error {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory,ValueFromPipeline)]
        [ValidateNotNullOrEmpty()]
        [string[]] $Message
    )

    Process {
        foreach($m in $Message) {
            Write-Error $m
        }
    }
}

function Log-Information {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory,ValueFromPipeline)]
        [ValidateNotNullOrEmpty()]
        [string[]] $Message
    )

    Process {
        if ( ! ( $script:Quiet ) ) {
            $StageName = $( if ( $script:StageName -ne $null ) { $script:StageName } else { '' })
            $Icon = ' =>'

            foreach($m in $Message) {
                Write-Host -NoNewLine -ForegroundColor Blue "  ${StageName} $($Icon.PadRight(5)) "
                Write-Host "${m}"
            }
        }
    }
}

function Log-Status {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory,ValueFromPipeline)]
        [ValidateNotNullOrEmpty()]
        [string[]] $Message
    )

    Process {
        if ( ! ( $script:Quiet ) ) {
            $StageName = $( if ( $StageName -ne $null ) { $StageName } else { '' })
            $Icon = '  >'

            foreach($m in $Message) {
                Write-Host -NoNewLine -ForegroundColor Green "  ${StageName} $($Icon.PadRight(5)) "
                Write-Host "${m}"
            }
        }
    }
}

function Log-Output {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory,ValueFromPipeline)]
        [ValidateNotNullOrEmpty()]
        [string[]] $Message
    )

    Process {
        if ( ! ( $script:Quiet ) ) {
            $StageName = $( if ( $script:StageName -ne $null ) { $script:StageName } else { '' })
            $Icon = ''

            foreach($m in $Message) {
                Write-Output "  ${StageName} $($Icon.PadRight(5)) ${m}"
            }
        }
    }
}

$Columns = (Get-Host).UI.RawUI.WindowSize.Width - 5
