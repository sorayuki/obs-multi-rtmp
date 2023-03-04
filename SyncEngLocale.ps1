function ReadLocaleAsArray {
    param (
        $fileName
    )
    return Get-Content $fileName | Foreach-Object {
        $x = $_.Split("=")
        return @{Key=$x[0]; Value=$x[1]}
    }
}

function ReadLocale {
    param (
        $fileName
    )
    $r = @{}
    Get-Content $fileName | Foreach-Object {
        $x = $_.Split("=")
        $r[$x[0]] = $x[1]
    }
    return $r
}

$en_locale = ReadLocaleAsArray ".\data\locale\en-US.ini"

Get-ChildItem ".\data\locale" -File | ForEach-Object {
    $cur_locale = ReadLocale (".\data\locale" + "\" + $_.Name)
    $en_locale `
    | ForEach-Object {
        if ($cur_locale.ContainsKey($_.Key)) {
            return $_.Key + "=" + $cur_locale[$_.Key]
        } else {
            return $_.Key + "=" + $_.Value
        }
    } `
    | Set-Content -Path (".\data\locale" + "\" + $_.Name)
}