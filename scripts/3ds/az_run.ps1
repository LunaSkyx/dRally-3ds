# az_run.ps1 - run the 3DS build unattended in the Azahar emulator and report what happened.
#
#   powershell -ExecutionPolicy Bypass -File scripts\3ds\az_run.ps1 -Scenario scripts\3ds\scenarios\explore.txt
#   ... -Build chaos        # build\3ds_chaos\dRally_3ds_chaos.3dsx instead of build\3ds\dRally_3ds.3dsx
#   ... -TimeoutSec 300     # hard stop (default 240)
#
# What it does (doc/scenario_harness.md has the full picture):
#   1. copies the .3dsx next to the game data on the emulated SD card (%APPDATA%\Azahar\sdmc)
#   2. writes the scenario to sdmc:/dr3_scenario.txt  (the build is inert without that file)
#   3. starts azahar.exe, waits for "[scn] done" in sdmc:/drally_3ds.log, then kills the emulator
#   4. ALWAYS removes dr3_scenario.txt again, so a manual run afterwards is not scripted
#   5. copies log + screenshots to logs\az_runs\<label>\ (BMP -> PNG) and prints the [scn] lines
#   ... -Mode duel -Extras 18   # race mode (standard|duel|dm|random|arena) and extra cars: written to the SD switch
#                               # files chaos_mode.txt / chaos_extras.txt for this run, then put back
#   ... -Binary X.3dsx          # run another .3dsx (an older build) instead of the one in build\
#   ... -Perf                  # frame-time measurement (chaos_time.txt=1, log off) - use with perf start/report steps
#   ... -Grep 'duel|grid|start' # also print the engine's own log lines that match (chaos log is switched on)
# Exit code: 0 = the script reached its quit step, 1 = timeout / emulator died / no log, 2 = "[scn] FAIL"
# (an "until" timed out inside the game - the log line says which screen it was waiting for).
param(
    [Parameter(Mandatory)][string]$Scenario,
    [ValidateSet('release','chaos')][string]$Build = 'release',
    [int]$TimeoutSec = 240,
    [string]$Label = '',
    [ValidateSet('','standard','duel','dm','random','arena')][string]$Mode = '',
    [int]$Extras = -1,
    [int]$Boss = -1,              # chaos_boss.txt: -1 = leave/default, 0 = off, 1..100 = % adversary join chance
    [string]$Grep = '',
    [switch]$Perf,
    [string]$Binary = ''          # run this .3dsx instead of the build (e.g. an older build for a before/after comparison)
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot)
$sd   = "$env:APPDATA\Azahar\sdmc"
$azahar = 'C:\Program Files\Azahar\azahar.exe'
$src  = if ($Binary) { $Binary } elseif ($Build -eq 'chaos') { "$repo\build\3ds_chaos\dRally_3ds_chaos.3dsx" } else { "$repo\build\3ds\dRally_3ds.3dsx" }
$test3dsx = "$sd\dRally-3ds-test.3dsx"     # same folder as the player's 3dsx: the game finds its data relative to it
$scn  = "$sd\dr3_scenario.txt"
$log  = "$sd\drally_3ds.log"
if (-not $Label) { $Label = (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + [IO.Path]::GetFileNameWithoutExtension($Scenario) }
$out = "$repo\logs\az_runs\$Label"

if (-not (Test-Path $src)) { throw "no build at $src - build first (doc/scenario_harness.md)" }
if (-not (Test-Path $azahar)) { throw "Azahar not found at $azahar" }
Get-Process azahar -ErrorAction SilentlyContinue | Stop-Process -Force    # one emulator at a time

# the switch files the chaos modes read (dr3_log.c): remember what was there, put it back afterwards
# -Perf: the timing switch on and the main log OFF - measuring while the watchdog/diagnostics run is no measurement
$switches = if ($Perf) { @{ 'chaos_log.txt' = '0'; 'chaos_time.txt' = '1' } } else { @{ 'chaos_log.txt' = '1' } }
if ($Perf -and -not $Grep) { $Grep = '^\[perf\]' }
if ($Mode)        { $switches['chaos_mode.txt']   = @{ standard = 0; duel = 1; dm = 2; random = 3; arena = 4 }[$Mode] }
if ($Extras -ge 0) { $switches['chaos_extras.txt'] = $Extras }
if ($Boss -ge 0)   { $switches['chaos_boss.txt']   = $Boss }
$saved = @{}
foreach ($k in $switches.Keys) {
    $f = "$sd\$k"
    $saved[$k] = if (Test-Path $f) { Get-Content $f -Raw } else { $null }
    Set-Content -Path $f -Value ([string]$switches[$k]) -Encoding ascii
}

Copy-Item $src $test3dsx -Force
Copy-Item $Scenario $scn -Force
Remove-Item "$sd\dr3_shots" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $log -Force -ErrorAction SilentlyContinue

$ok = $false
$p = $null
$wall = [Diagnostics.Stopwatch]::StartNew()
try {
    $t0 = Get-Date
    $p = Start-Process $azahar -ArgumentList "`"$test3dsx`"" -PassThru
    while (((Get-Date) - $t0).TotalSeconds -lt $TimeoutSec) {
        Start-Sleep 2
        if ((Test-Path $log) -and (Select-String -Path $log -Pattern '\[scn\] done' -Quiet)) { $ok = $true; break }
        if ($p.HasExited) { break }
    }
    if ($ok) { Start-Sleep 2 }     # let the last screenshot reach the disk
}
finally {
    if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item $scn -Force -ErrorAction SilentlyContinue
    foreach ($k in $saved.Keys) {
        if ($null -eq $saved[$k]) { Remove-Item "$sd\$k" -Force -ErrorAction SilentlyContinue }
        else { Set-Content -Path "$sd\$k" -Value $saved[$k] -NoNewline -Encoding ascii }
    }
}

New-Item -ItemType Directory -Force $out | Out-Null
if (Test-Path $log) { Copy-Item $log "$out\drally_3ds.log" -Force }
Copy-Item $Scenario "$out\scenario.txt" -Force
if (Test-Path "$sd\dr3_shots") {
    Add-Type -AssemblyName System.Drawing
    foreach ($b in Get-ChildItem "$sd\dr3_shots\*.bmp") {
        $img = [System.Drawing.Image]::FromFile($b.FullName)
        $img.Save("$out\" + $b.BaseName + '.png', [System.Drawing.Imaging.ImageFormat]::Png)
        $img.Dispose()
    }
}

$failed = (Test-Path $log) -and (Select-String -Path $log -Pattern '\[scn\] FAIL' -Quiet)
"== az_run: $Label  build=$Build  mode=$(if ($Mode) { $Mode } else { 'as set' })  result=" + $(if ($failed) { 'FAILED (a step timed out in the game)' } elseif ($ok) { 'DONE' } else { 'FAILED (timeout / emulator exited)' })
"== output: $out   (wall time incl. emulator start: {0:N1} s)" -f $wall.Elapsed.TotalSeconds
if (Test-Path "$out\drally_3ds.log") {
    Select-String -Path "$out\drally_3ds.log" -Pattern '^\[scn\]' | ForEach-Object { $_.Line }
    $lines = (Get-Content "$out\drally_3ds.log" | Measure-Object -Line).Lines
    if ($Grep) { "== engine lines matching '$Grep':"; Select-String -Path "$out\drally_3ds.log" -Pattern $Grep | Where-Object { $_.Line -notmatch '^\[scn\]' } | ForEach-Object { $_.Line } | Select-Object -First 60 }
    "== log: $lines line(s); pngs: " + (Get-ChildItem "$out\*.png" -ErrorAction SilentlyContinue | Measure-Object).Count
} else { "== no log was written - the game never started" }
if ($failed) { exit 2 }
if (-not $ok) { exit 1 }
