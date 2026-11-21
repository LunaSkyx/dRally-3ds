# Macht aus einem (oder mehreren) Harness-Logs einen kurzen Bericht: was das Rennen war, was das Grid
# gesagt hat, was der Watchdog gesehen hat, womit der Lauf geendet hat - und ob der Chaos-Killbonus gezahlt
# hat (siehe doc/host_harness.md).
#
#   powershell -ExecutionPolicy Bypass -File scripts/3ds/report_host.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/3ds/report_host.ps1 -Log run_host\drally_host.log -Tail 30
param(
    [string]$Log = 'run_host\drally_host.log',
    [int]$Tail = 25,
    [switch]$All
)
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$path = Join-Path $root $Log
if (-not (Test-Path -LiteralPath $path)) { throw "kein Log: $path" }

$lines = [System.IO.File]::ReadAllLines($path)

# Die Abschnitte werden in *einem* Durchgang durch das Log gefuellt.  Frueher lief pro Abschnitt ein eigener
# Where-Object-Durchlauf ueber alle Zeilen (und in jedem davon noch einer ueber die Muster): bei einem
# Fahrlauf mit ~200.000 Zeilen (die Frame-Dumps des Autopiloten, doc/host_harness.md 2) dauerte der Bericht
# dadurch Minuten.  Ein flaches foreach ueber dieselben Muster waehlt dasselbe aus, nur einmal ueber die Zeilen.
#
# '[host]'/'[dr3]' sind fuer -like Zeichenklassen, die Muster brauchen die Klammern also literal - das macht
# das Replace() unten (ein regex-freies Escapen, '.' bleibt dabei Platzhalter).
$sections = @(
    @{ T = 'Harness'          ; M = 20 ; P = @('[host] harness:*', '[host] quick race:*', '[host] *switch*', '[host] extra cars*') },
    @{ T = 'Autopilot'        ; M = 20 ; P = @('[host] autopilot*', '[host] scenario:*') },
    @{ T = 'Boot/Modus'       ; M = 12 ; P = @('[dr3] SDL*', '[dr3] cwd*', '[dr3] argv0*', '[dr3] mode =*', '[dr3] display init*') },
    @{ T = 'Renn-Setup'       ; M = 20 ; P = @('[dr3] chaos: race setup*', '[dr3] chaos: this race*', '[dr3] chaos: the extra car ladder*', '[dr3] chaos: *car(s)*', '[dr3] chaos: build *', '[dr3] CHAOS build*') },
    @{ T = 'Grid/Aufstellung' ; M = 30 ; P = @('[dr3] chaos: grid*', '*placed on the road*', '[dr3] chaos: field of*', '[dr3] chaos: the road next to this start line*') },
    @{ T = 'Watchdog/Band'    ; M = 30 ; P = @('*WATCH*', '*BAND*', '*remap*') },
    @{ T = 'Pass-Suche'       ; M = 25 ; P = @('[dr3] chaos: PASS SWITCH*', '*switched off drawing pass*') },
    @{ T = 'Frame-Zeit'       ; M = 4  ; P = @('[dr3] chaos frame time:*', '[dr3] chaos frame detail:*') },
    @{ T = 'Extras/Stufen'    ; M = 15 ; P = @('*EXTRA CAR STAGE*', '*EXTRA CARS*', '*ladder*') },
    @{ T = 'Heap/Fehler'      ; M = 20 ; P = @('*HEAP*', '*heap*', '*guard*', '*Fatal*', '*FAILED*', '*FAIL*', '*crash:*') }
)

$hits = @{}

foreach ($s in $sections) {

    $s.Escaped = @($s.P | ForEach-Object { $_.Replace('[', '[[]').Replace(']', '[]]') })
    $hits[$s.T] = New-Object System.Collections.ArrayList
}

foreach ($l in $lines) {

    foreach ($s in $sections) {

        foreach ($p in $s.Escaped) {

            if ($l -like $p) { [void]$hits[$s.T].Add($l); break }
        }
    }
}

Write-Host ("=== Bericht: {0} ===" -f $path) -ForegroundColor Green
Write-Host ("Zeilen: {0}   Groesse: {1} kB" -f $lines.Count, [math]::Round((Get-Item $path).Length/1kB,1))

foreach ($s in $sections) {

    $hit = @($hits[$s.T])

    Write-Host ""
    Write-Host ("=== {0} ({1}) ===" -f $s.T, $hit.Count) -ForegroundColor Cyan

    if ($All) { $hit | ForEach-Object { Write-Host $_ } }
    else      { $hit | Select-Object -Last $s.M | ForEach-Object { Write-Host $_ } }
}

# --- der Killbonus -----------------------------------------------------------------------------
# Die "field car N hp ..."-Zeilen (Harness-Schalter "field <ms>") sagen, wer auf 0 HP gegangen ist, die
# "kill bonus"-Zeilen (race___4dcach.c) sagen, wer dafuer bezahlt hat.  Beides nebeneinander ist die
# Pruefung: ein Wrack mit Zahlung ist der Kill des Spielers, ein Wrack ohne Zahlung ist fremder Schaden
# (Wand, Explosion, dritter Gegner) - der darf nichts zahlen.  Siehe doc/host_harness.md.
#
# Die Feldzeile wird an den Leerzeichen zerlegt statt mit einem Muster gelesen (die Werte stehen damit an
# festen Stellen: "[host] field car 3 hp 0 pos 5 lap 1 finished 0 speed 0.009 at 406.0,211.6 ...").
$payoutLines = @($lines | Where-Object { $_ -like '*kill bonus:*' })
$wrecks      = @{}

foreach ($l in $lines) {

    if ($l -notlike '*field car*') { continue }

    $f = @($l.Trim() -split ' +')

    if (($f.Count -gt 15) -and ($f[1] -eq 'field') -and ($f[2] -eq 'car') -and ($f[4] -eq 'hp') -and ($f[14] -eq 'at')) {

        $car = [int]$f[3]
        [int]$hp = [int]$f[5]

        if (($hp -le 0) -and (-not $wrecks.ContainsKey($car))) { $wrecks[$car] = $f[15] }
    }
}

Write-Host ""
Write-Host ("=== Killbonus: {0} Wrack(s) (hp 0), {1} Zahlung(en) ===" -f $wrecks.Count, $payoutLines.Count) -ForegroundColor Cyan

foreach ($car in ($wrecks.Keys | Sort-Object)) {

    $paid = @($payoutLines | Where-Object { $_ -like "*kill bonus: * (car $car, model *" })

    if ($paid.Count -gt 0) {
        Write-Host ("  car {0,2}: Wrack bei {1} - bezahlt" -f $car, $wrecks[$car]) -ForegroundColor Green
        foreach ($l in $paid) { Write-Host "      $l" -ForegroundColor Green }
        $popup = @($lines | Where-Object { $_ -like "*kill bonus popup:*" -and $_ -like "*$car,*" })
        Write-Host ("      ({0} Popup-Zeile(n) im Log: Position und Groesse des `$)" -f $popup.Count) -ForegroundColor DarkGray
    }
    else {
        Write-Host ("  car {0,2}: Wrack bei {1} - keine Zahlung" -f $car, $wrecks[$car]) -ForegroundColor Yellow
        # den Grund nennt seit dem 26. September 2026 dr3_chaos_wreck selbst (wer das Wrack verursacht hat)
        $why = @($lines | Where-Object { ($_ -like '*no kill bonus*') -and ($_ -match "\(car $car\) wrecked") })
        foreach ($l in ($why | Select-Object -First 3)) { Write-Host "      $l" -ForegroundColor DarkYellow }
    }
}

# --- der Renn-Report ---------------------------------------------------------------------------
# Das Rennen endet mit dem Report ("Your Statistics", ___36adch.c): das Rang-Geld jeder Position
# (DR3_CHAOS_RANK_MONEY, das ___33010h.c am Ende des Rennens ausrechnet) und die Kills des Rennens
# (dr3_chaos_kill_money) stehen dort in einer Zeile.  Sie kommt nur, wenn das Rennen auch zu Ende
# gefahren wurde - im Host-Lauf kuerzt der Schalter "laps <n>" es dafuer (doc/host_harness.md).
$report = @($lines | Where-Object { $_ -like '*report of the race:*' })

Write-Host ""
Write-Host ("=== Renn-Report: {0} Zeile(n) ===" -f $report.Count) -ForegroundColor Cyan

if ($report.Count -eq 0) {
    Write-Host "  (kein Rennende im Log: das Rennen lief noch, als der Lauf aufhoerte)" -ForegroundColor DarkGray
}
else {
    foreach ($l in $report) { Write-Host "  $l" -ForegroundColor Green }
}

# was am Ende des Logs steht, ist die Stelle, an der der Lauf aufgehoert hat
Write-Host ""
Write-Host "=== letzte Zeile ===" -ForegroundColor Cyan
Write-Host $lines[-1]
