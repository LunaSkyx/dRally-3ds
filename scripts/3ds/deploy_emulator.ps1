# Puts a freshly built 3DS port on the emulator's SD card - the .3dsx files *and* the test save games.
#
#   powershell -ExecutionPolicy Bypass -File scripts/3ds/deploy_emulator.ps1
#
# Copies build/3ds/dRally_3ds.3dsx (and the debug build) to
#   %APPDATA%\Azahar\sdmc\3ds\drally\   (where the emulator reads its SD card from)
#   ..\3ds-release\dRally_3ds\          (the release package next to the repository)
# and (re)creates DR.SG1..DR.SG4 there and in build/3ds, so the saves are always next to the .3dsx.
#
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sdmc = Join-Path $env:APPDATA 'Azahar\sdmc\3ds\drally'
$rel  = Join-Path (Split-Path -Parent $repo) '3ds-release\dRally_3ds'

$files = @(
    @{ src = Join-Path $repo 'build\3ds\dRally_3ds.3dsx';       dstdir = $sdmc; name = 'dRally_3ds.3dsx' },
    @{ src = Join-Path $repo 'build\3ds_debug\dRally_3ds_debug.3dsx'; dstdir = $sdmc; name = 'dRally_3ds_debug.3dsx' }
)

foreach ($f in $files){

    if (-not (Test-Path $f.src)) { Write-Host ("missing build: {0}" -f $f.src) -ForegroundColor Yellow; continue }

    Copy-Item $f.src (Join-Path $f.dstdir $f.name) -Force
    Write-Host ("{0} -> {1}" -f (Split-Path $f.src -Leaf), (Join-Path $f.dstdir $f.name))

    if (Test-Path $rel){
        Copy-Item $f.src (Join-Path $rel $f.name) -Force
    }
}

Write-Host '=== test save games ==='
& (Join-Path $repo 'scripts\gen_3ds_saves.ps1')

Write-Host '=== in place now ==='
Get-ChildItem $sdmc -Filter 'DR.SG*' | ForEach-Object { Write-Host ("  {0}  {1} bytes  {2}" -f $_.Name, $_.Length, $_.LastWriteTime) }
Get-Item (Join-Path $sdmc 'dRally_3ds.3dsx') | ForEach-Object { Write-Host ("  {0}  {1} bytes  SHA256 {2}" -f $_.Name, $_.Length, (Get-FileHash $_.FullName -Algorithm SHA256).Hash.Substring(0,16)) }
