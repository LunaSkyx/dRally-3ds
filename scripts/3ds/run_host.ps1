# Startet den PC-Harness (tests/dRally3DSHost.exe) auf einer Kopie der Spieldaten - kein Handgriff am
# Emulator, kein Fenster, das bedient werden muss.
#
#   powershell -ExecutionPolicy Bypass -File scripts/3ds/run_host.ps1 -QuickRaceMs 5000 -Frames 1200
#   powershell -ExecutionPolicy Bypass -File scripts/3ds/run_host.ps1 -Script dr3_host.txt -TimeoutSec 120
#
# Ablauf:
#   * run_host/ wird einmalig aus 3ds-release/dRally_3ds aufgesetzt (ENGINE.BPA, DR.SG*, TR*.BPA, CINEM)
#   * exe + SDL2.dll hinein, die Schalterdateien des Chaos-Builds (chaos_log.txt, chaos_time.txt, ...)
#   * Start mit CWD = run_host (die Engine oeffnet die Assets relativ zum CWD, dr3_paths.c)
#   * Timeout, Abbruch, Log-Tail
#
# Der Ton ist standardmaessig auf SDL_AUDIODRIVER=dummy: eine Pruefung braucht ihn nicht, und eine
# Session ohne Audiogeraet soll nicht der Grund sein, warum ein Lauf scheitert (-Audio nimmt das zurueck).
param(
    [string]$Script     = 'dr3_host.txt',
    [int]$Frames        = 0,
    [int]$QuickRaceMs   = 0,
    [int]$Extras        = -1,
    [int]$TimeoutSec    = 240,
    [int]$DumpEvery     = 0,
    [string]$DumpPath   = 'frames\f_%04d.bmp',
    [int]$AutoKeyMs     = 0,
    [string]$AutoKey    = '',
    [switch]$Stage,
    [switch]$Audio,
    [switch]$NoStart,
    [string]$ExtraArgs  = ''
)
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$run  = Join-Path $root 'run_host'
$exe  = Join-Path $root 'tests\host\Release\dRally3DSHost.exe'
$dll  = 'C:\lib\SDL2-2.0.18\lib\x64\SDL2.dll'

$data = Join-Path $root '3ds-release\dRally_3ds'
if (-not (Test-Path (Join-Path $data 'ENGINE.BPA'))) {
    $data = Join-Path (Split-Path -Parent $root) '3ds-release\dRally_3ds'
}
if (-not (Test-Path (Join-Path $data 'ENGINE.BPA'))) { throw "Spieldaten nicht gefunden (3ds-release/dRally_3ds)" }
if (-not (Test-Path $exe)) { throw "Harness nicht gebaut: $exe  (scripts/3ds/gen_host_project.ps1 -Build)" }

New-Item -ItemType Directory -Force -Path $run | Out-Null

if ($Stage -or -not (Test-Path (Join-Path $run 'ENGINE.BPA'))) {
    Write-Host "staging $data  ->  $run" -ForegroundColor Cyan
    Copy-Item (Join-Path $data '*') -Destination $run -Recurse -Force -Exclude '*.3dsx','*.zip'
}

Copy-Item $exe -Destination $run -Force
Copy-Item $dll -Destination $run -Force

# die Symbole des Harness: der Crash-Report (dr3_host.c) holt sich daraus Funktion und Zeile, wenn ein Lauf
# abstuerzt - ohne sie steht im Log nur die Adresse
$pdb = [System.IO.Path]::ChangeExtension($exe, '.pdb')
if (Test-Path $pdb) { Copy-Item $pdb -Destination $run -Force }

# die Skript-Vorlagen des Harness (eine vorhandene Datei bleibt stehen, -Stage ueberschreibt sie)
$tpl = Join-Path $root 'scripts\3ds\host'
foreach ($f in @('dr3_host.txt', 'dr3_keys.txt')) {
    $dst = Join-Path $run $f
    if (Test-Path (Join-Path $tpl $f)) {
        if ($Stage -or -not (Test-Path $dst)) { Copy-Item (Join-Path $tpl $f) -Destination $dst -Force }
    }
}

# die Schalter des Chaos-Builds (dr3_log.c liest sie im CWD dieses Builds)
Set-Content -LiteralPath (Join-Path $run 'chaos_log.txt')  -Value '1' -Encoding ascii
Set-Content -LiteralPath (Join-Path $run 'chaos_time.txt') -Value '1' -Encoding ascii
if ($Extras -ge 0) { Set-Content -LiteralPath (Join-Path $run 'chaos_extras.txt') -Value "$Extras" -Encoding ascii }

$argList = @()
if (Test-Path (Join-Path $run $Script)) { $argList += @('--script', $Script) }
if ($Frames -gt 0)      { $argList += @('--frames', "$Frames") }
if ($QuickRaceMs -gt 0) { $argList += @('--quickrace', "$QuickRaceMs") }
if ($DumpEvery -gt 0)   { $argList += @('--dumpevery', "$DumpEvery", $DumpPath) }
if ($AutoKeyMs -gt 0 -and $AutoKey) { $argList += @('--autokey', "$AutoKeyMs", $AutoKey) }
if ($ExtraArgs)         { $argList += $ExtraArgs.Split(' ') }

Write-Host ("start: dRally3DSHost.exe " + ($argList -join ' ')) -ForegroundColor Cyan
Write-Host "cwd:   $run"

if ($NoStart) { Write-Host '-NoStart: nicht gestartet' -ForegroundColor Yellow; return }

$out = Join-Path $root 'logs\host_run.out'
$err = Join-Path $root 'logs\host_run.err'

if (-not $Audio) { $env:SDL_AUDIODRIVER = 'dummy' }
try {
    $p = Start-Process -FilePath (Join-Path $run 'dRally3DSHost.exe') -ArgumentList $argList `
                       -WorkingDirectory $run -RedirectStandardOutput $out -RedirectStandardError $err `
                       -NoNewWindow -PassThru

    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        Write-Host "TIMEOUT nach $TimeoutSec s - Prozess wird beendet" -ForegroundColor Yellow
        $p.Kill()
        Start-Sleep -Seconds 3
    }
    Write-Host "exit code: $($p.ExitCode)" -ForegroundColor Cyan
}
finally {
    Remove-Item Env:\SDL_AUDIODRIVER -ErrorAction SilentlyContinue
}

Write-Host "--- stdout (letzte 20 Zeilen) ---"
if (Test-Path $out) { Get-Content $out -Tail 20 }
if ((Test-Path $err) -and (Get-Item $err).Length -gt 0) {
    Write-Host "--- stderr ---" -ForegroundColor Yellow
    Get-Content $err -Tail 20
}
Write-Host "Log: $(Join-Path $run 'drally_host.log')  ($((Get-Item (Join-Path $run 'drally_host.log') -ErrorAction SilentlyContinue).Length) bytes)"
