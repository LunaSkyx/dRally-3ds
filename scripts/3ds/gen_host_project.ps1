# Erzeugt tests/dRally3DSHost.vcxproj - die PC-Fassung des Haupt-Builds (der Harness, platform_3ds/dr3_host.c).
#
# Das Projekt ist das vorhandene Check-Projekt (tests/dRally3DSCheck.vcxproj) mit vier Unterschieden:
#   * Preprocessor: DR_LETTERBOX (wie der Haupt-Build), DR3_LOG_KEYS, DR3_CHAOS (der Haupt-Build hat die
#     Chaos-Difficulty) und DR3_HOST (der Harness: relative Logpfade, SDL-Takt, Skript, Direktstart -
#     platform_3ds/dr3_host.h)
#   * die Harness-Quelle platform_3ds/dr3_host.c
#   * eigene Ausgabe: tests/host/<Config>/dRally3DSHost.exe - das Check-Projekt bleibt unangetastet
#   * DR3_USE_GFX und DR3_PROFILE bleiben aus (kein libctru auf dem PC - dieselbe Grenze wie im Check-Build)
#
# Aufruf:  powershell -ExecutionPolicy Bypass -File scripts/3ds/gen_host_project.ps1 [-Build]
param(
    [switch]$Build
)
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$src  = Join-Path $root 'tests\dRally3DSCheck.vcxproj'
$dst  = Join-Path $root 'tests\dRally3DSHost.vcxproj'

if (-not (Test-Path -LiteralPath $src)) { throw "nicht gefunden: $src" }

$text = Get-Content -Raw -LiteralPath $src
$nl   = "`r`n"

function Replace-Once([string]$t, [string]$old, [string]$new, [string]$what) {
    if (-not $t.Contains($old)) { throw "Muster nicht gefunden ($what) - das Check-Projekt wurde geaendert" }
    return $t.Replace($old, $new)
}

# 1) eigene GUID: beide Projekte liegen im selben Ordner
$text = Replace-Once $text '{7A1E6D6E-1D0B-4E4F-9E1E-3A1C2B7F5D22}' '{7A1E6D6E-1D0B-4E4F-9E1E-3A1C2B7F5D33}' 'ProjectGuid'

# 2) die Defines des Harness-Builds
$oldDef = '<PreprocessorDefinitions>__3DS__;DR_MULTIPLAYER;IPXNET;%(PreprocessorDefinitions)</PreprocessorDefinitions>'
$newDef = '<PreprocessorDefinitions>__3DS__;DR_LETTERBOX;DR_MULTIPLAYER;IPXNET;DR3_LOG_KEYS;DR3_CHAOS;DR3_HOST;%(PreprocessorDefinitions)</PreprocessorDefinitions>'
$text = Replace-Once $text $oldDef $newDef 'PreprocessorDefinitions'

# 3) eigene Ausgabe (das Check-Projekt schreibt nach tests\x64\)
$oldOut = '<IncludePath>$(ProjectDir)..;$(ProjectDir)..\platform_3ds;$(ProjectDir)..\platform_3ds\sdl2_net_stub\include;C:\include;C:\include\SDL2;$(IncludePath)</IncludePath>'
$newOut = $oldOut + $nl +
          '    <OutDir>$(ProjectDir)host\$(Configuration)\</OutDir>' + $nl +
          '    <IntDir>$(ProjectDir)host\obj\$(Configuration)\</IntDir>' + $nl +
          '    <TargetName>dRally3DSHost</TargetName>'
$text = Replace-Once $text $oldOut $newOut 'IncludePath/OutDir'

# 4) die Harness-Quelle
$oldSrc = '    <ClCompile Include="..\platform_3ds\dr3_paths.c" />'
$newSrc = $oldSrc + $nl + '    <ClCompile Include="..\platform_3ds\dr3_host.c" />'
$text = Replace-Once $text $oldSrc $newSrc 'dr3_paths.c'

# 5) die Harness-Header (nur fuer die IDE, der Build braucht sie nicht)
$oldInc = '    <ClInclude Include="..\drally.h" />'
$newInc = $oldInc + $nl + '    <ClInclude Include="..\platform_3ds\dr3_host.h" />'
$text = Replace-Once $text $oldInc $newInc 'drally.h (ClInclude)'

Set-Content -LiteralPath $dst -Value $text -Encoding UTF8 -NoNewline
Write-Host "geschrieben: $dst" -ForegroundColor Green

if ($Build) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw 'vswhere.exe nicht gefunden' }
    $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
               Select-Object -First 1
    if (-not $msbuild) { throw 'MSBuild.exe nicht gefunden' }

    & $msbuild $dst /p:Configuration=Release /p:Platform=x64 /nologo /v:m
    if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen (exit $LASTEXITCODE)" }

    Write-Host "exe: $(Join-Path $root 'tests\host\Release\dRally3DSHost.exe')" -ForegroundColor Green
}
