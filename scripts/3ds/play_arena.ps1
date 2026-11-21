# play_arena.ps1 - opens a playable 12-car ARENA battle in Azahar and cleans up its temporary scenario file.
#   powershell -ExecutionPolicy Bypass -File scripts\3ds\play_arena.ps1 [-Extras 11]
param([int]$Extras = 11)
$ErrorActionPreference = 'Stop'
$sd     = "$env:APPDATA\Azahar\sdmc"
$azahar = 'C:\Program Files\Azahar\azahar.exe'
$scn    = "$sd\dr3_scenario.txt"
$log    = "$sd\drally_3ds.log"

Get-Process azahar -ErrorAction SilentlyContinue | Stop-Process -Force

$saved = @{}
foreach ($k in @('chaos_mode.txt', 'chaos_extras.txt')) {
    $f = "$sd\$k"
    $saved[$k] = if (Test-Path $f) { Get-Content $f -Raw } else { $null }
}

Set-Content "$sd\chaos_mode.txt" '4' -Encoding ascii
Set-Content "$sd\chaos_extras.txt" ([string]$Extras) -Encoding ascii
Set-Content $scn "until ready spam Return every 150 timeout 40000`nquickrace track 4`nuntil event race_start spam Return every 150 timeout 30000`nsleep 86400000" -Encoding ascii
Remove-Item $log -Force -ErrorAction SilentlyContinue

$p = Start-Process -FilePath $azahar -ArgumentList "`"$sd\dRally-3ds.3dsx`"" -PassThru
try {
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep 1
        if ((Test-Path $log) -and (Select-String -Path $log -Pattern 'event race_start' -Quiet)) { break }
        if ($p.HasExited) { break }
    }
}
finally {
    Remove-Item $scn -Force -ErrorAction SilentlyContinue
    foreach ($k in $saved.Keys) {
        if ($null -eq $saved[$k]) { Remove-Item "$sd\$k" -Force -ErrorAction SilentlyContinue }
        else { Set-Content "$sd\$k" $saved[$k] -NoNewline -Encoding ascii }
    }
}
Write-Host "ARENA battle started in Azahar (PID $($p.Id)). Scenario file removed and mode switches restored."
