# Builds a Death Rally - 30th Anniversary Edition Nintendo 3DS .cia package.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\3ds\build_cia.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\3ds\build_cia.ps1 -DownloadBannertool
#
# When bannertool.exe is available (either on PATH, in C:\devkitPro\tools\bin, or in
# ..\third_party\bannertool), this script also compiles assets\3ds\banner.png +
# assets\3ds\banner.wav into a full 3DS Home Menu 3D banner (build\3ds\dRally_3ds.bnr)
# and embeds it inside dRally_3ds.cia.
param(
    [switch]$DownloadBannertool,
    [string]$OutCia = ""
)

$ErrorActionPreference = 'Stop'

$Repo      = (Get-Item (Join-Path $PSScriptRoot '..\..')).FullName
$Workspace = (Get-Item (Join-Path $Repo '..')).FullName
$AssetsDir = Join-Path $Repo 'assets\3ds'
$BuildDir  = Join-Path $Repo 'build\3ds'

$ElfPath    = Join-Path $BuildDir 'dRally_3ds.elf'
$SmdhPath   = Join-Path $BuildDir 'dRally_3ds.smdh'
$BnrPath    = Join-Path $BuildDir 'dRally_3ds.bnr'
$RomfsDir   = Join-Path $AssetsDir 'romfs'
$RsfPath    = Join-Path $AssetsDir 'dRally_3ds.rsf'
$IconPath   = Join-Path $AssetsDir 'icon.png'
$BannerPng  = Join-Path $AssetsDir 'banner.png'
$BannerWav  = Join-Path $AssetsDir 'banner.wav'
$DefaultCia = Join-Path $BuildDir 'dRally_3ds.cia'
if ([string]::IsNullOrWhiteSpace($OutCia)) { $OutCia = $DefaultCia }

function Find-Tool([string]$Name, [string[]]$ExtraPaths) {
    foreach ($p in $ExtraPaths) {
        if ($p -and (Test-Path $p)) { return $p }
    }
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

$Makerom = Find-Tool 'makerom.exe' @(
    'C:\devkitPro\tools\bin\makerom.exe',
    (Join-Path $Workspace 'third_party\makerom\makerom.exe')
)
if (-not $Makerom) { throw "makerom.exe not found. Install devkitPro 3ds-dev or put makerom.exe on PATH." }

$Smdhtool = Find-Tool 'smdhtool.exe' @(
    'C:\devkitPro\tools\bin\smdhtool.exe'
)
if (-not $Smdhtool) { throw "smdhtool.exe not found. Install devkitPro 3ds-dev or put smdhtool.exe on PATH." }

if (-not (Test-Path $ElfPath)) {
    throw "Missing $ElfPath - build the 3DS ELF first with Makefile.3ds."
}
if (-not (Test-Path $IconPath)) {
    throw "Missing $IconPath - run scripts\3ds\make_cia_assets.py first."
}
if (-not (Test-Path $RsfPath)) {
    throw "Missing $RsfPath."
}

$BannertoolDir = Join-Path $Workspace 'third_party\bannertool'
$Bannertool = Find-Tool 'bannertool.exe' @(
    (Join-Path $BannertoolDir 'bannertool.exe'),
    'C:\devkitPro\tools\bin\bannertool.exe'
)

if ($DownloadBannertool -and -not $Bannertool) {
    Write-Host "[bannertool] Downloading Windows bannertool release from GitHub..."
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    New-Item -ItemType Directory -Force -Path $BannertoolDir | Out-Null
    $zipPath = Join-Path $BannertoolDir 'bannertool.zip'
    $downloaded = $false
    $repos = @('carstene1ns/3ds-bannertool', 'Epicpkmn11/bannertool')
    foreach ($r in $repos) {
        try {
            $rel = Invoke-RestMethod -Uri "https://api.github.com/repos/$r/releases/latest" -Headers @{ 'User-Agent' = 'dRally-3ds-build' }
            $asset = $rel.assets | Where-Object { $_.name -match 'win|windows|x86_64|64' -and $_.name -match '\.(zip|exe)$' } | Select-Object -First 1
            if (-not $asset) {
                $asset = $rel.assets | Where-Object { $_.name -match '\.zip$' } | Select-Object -First 1
            }
            if ($asset) {
                Write-Host "  Fetching $($asset.browser_download_url)"
                if ($asset.name -match '\.exe$') {
                    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile (Join-Path $BannertoolDir 'bannertool.exe')
                } else {
                    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zipPath
                    Expand-Archive -Path $zipPath -DestinationPath $BannertoolDir -Force
                    $found = Get-ChildItem -Path $BannertoolDir -Recurse -Filter 'bannertool.exe' | Select-Object -First 1
                    if ($found -and $found.FullName -ne (Join-Path $BannertoolDir 'bannertool.exe')) {
                        Copy-Item $found.FullName (Join-Path $BannertoolDir 'bannertool.exe') -Force
                    }
                    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
                }
                if (Test-Path (Join-Path $BannertoolDir 'bannertool.exe')) {
                    $Bannertool = Join-Path $BannertoolDir 'bannertool.exe'
                    $downloaded = $true
                    break
                }
            }
        } catch {
            Write-Warning "  Failed fetching from ${r}: $($_.Exception.Message)"
        }
    }
    if (-not $downloaded) {
        throw "Could not download bannertool.exe automatically. Place bannertool.exe in $BannertoolDir and re-run."
    }
}

Write-Host "[1/4] Building Death Rally SMDH icon + metadata..."
& $Smdhtool --create "Death Rally" "Death Rally - 30th Anniversary Edition" "LunaSkyx / Remedy" $IconPath $SmdhPath
if ($LASTEXITCODE -ne 0) { throw "smdhtool failed with exit code $LASTEXITCODE" }

$HasBanner = $false
if ($Bannertool -and (Test-Path $BannerPng) -and (Test-Path $BannerWav)) {
    Write-Host "[2/4] Compiling 3DS Home Menu 3D banner + stereo audio via bannertool..."
    & $Bannertool makebanner -i $BannerPng -a $BannerWav -o $BnrPath
    if ($LASTEXITCODE -ne 0) { throw "bannertool failed with exit code $LASTEXITCODE" }
    $HasBanner = $true
} else {
    Write-Host "[2/4] Skipping 3D banner.bnr (bannertool.exe not found locally)."
    Write-Host "      Tip: run with -DownloadBannertool on an internet-connected shell to embed banner.png + banner.wav."
}

$HasRomfs = $false
if ((Test-Path $RomfsDir) -and ((Get-ChildItem -Path $RomfsDir -File -ErrorAction SilentlyContinue).Count -gt 0)) {
    Write-Host "[3/4] Embedding built-in 3DS RomFS (Battle Arena TRA.BPA) via makerom..."
    $HasRomfs = $true
} else {
    Write-Host "[3/4] Skipping RomFS (no files in $RomfsDir)."
}

Write-Host "[4/4] Packaging CIA via makerom..."
$makeromArgs = @(
    '-f', 'cia',
    '-o', $OutCia,
    '-rsf', $RsfPath,
    '-target', 't',
    '-exefslogo',
    '-elf', $ElfPath,
    '-icon', $SmdhPath
)
if ($HasBanner) {
    $makeromArgs += @('-banner', $BnrPath)
}
if ($HasRomfs) {
    $makeromArgs += @("-DROMFS=$RomfsDir")
}

& $Makerom @makeromArgs
if ($LASTEXITCODE -ne 0) { throw "makerom failed with exit code $LASTEXITCODE" }

$ReleaseDir = Join-Path $Workspace '3ds-release\dRally_3ds'
if (Test-Path $ReleaseDir) {
    $ReleaseCia = Join-Path $ReleaseDir 'dRally_3ds.cia'
    if ([IO.Path]::GetFullPath($OutCia) -ne [IO.Path]::GetFullPath($ReleaseCia)) {
        Copy-Item $OutCia $ReleaseCia -Force
        Write-Host "Deployed CIA -> $ReleaseCia"
    }
}

$sizeKB = [Math]::Round((Get-Item $OutCia).Length / 1024.0, 1)
Write-Host ""
Write-Host "SUCCESS: $OutCia ($sizeKB KB)"
Write-Host "  TitleID : 0004000000D4A100 (ProductCode: CTR-P-DRLY)"
Write-Host "  Icon    : Death Rally custom 48x48 SMDH"
Write-Host "  Banner  : $(if ($HasBanner) { 'Embedded 3D banner.bnr + 44.1kHz stereo jingle' } else { 'SMDH icon only (run with -DownloadBannertool to embed 3D banner + audio)' })"
Write-Host "  RomFS   : $(if ($HasRomfs) { 'Embedded Battle Arena (romfs:/TRA.BPA)' } else { 'None (external TRA.BPA required on SD)' })"
