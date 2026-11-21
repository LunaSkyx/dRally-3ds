# Wandelt die Frame-Dumps des Harness (8-bit BMP, so wie die Engine zeichnet) in PNG um, damit sie
# angesehen werden koennen.
#
#   powershell -ExecutionPolicy Bypass -File scripts/3ds/host_shots.ps1 -Dir run_host\frames
param(
    [string]$Dir = 'run_host\frames',
    [switch]$DeleteBmp
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$path = Join-Path $root $Dir
if (-not (Test-Path -LiteralPath $path)) { throw "kein Ordner: $path" }

$bmp = Get-ChildItem -LiteralPath $path -Filter '*.bmp'
Write-Host ("{0} BMP(s) in {1}" -f $bmp.Count, $path)

foreach ($f in $bmp) {
    $png = [System.IO.Path]::ChangeExtension($f.FullName, '.png')
    $img = [System.Drawing.Image]::FromFile($f.FullName)
    $img.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
    Write-Host ("  {0} -> {1}" -f $f.Name, (Split-Path $png -Leaf))
    if ($DeleteBmp) { Remove-Item -LiteralPath $f.FullName -Force }
}
