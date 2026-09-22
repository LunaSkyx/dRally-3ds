# Creates ready-to-load dRally save games for testing (3DS port).
#
#   powershell -ExecutionPolicy Bypass -File scripts/gen_3ds_saves.ps1
#
# Writes DR.SG1..DR.SG4 into the game folder on the emulator's SD card and into the release package.
# The format is a raw saved_game_t (0x883 bytes, see drally_structs_fixed.h) with the racer dwords stored
# little-endian and then encoded exactly like dREncryption_encodeSavedGame() does it:
#
#       key     = byte 0 (left as it is)
#       byte[n] = ror8(plain[n] + 0x11*n - key, n % 6)          for n = 1 .. 0x882
#
$ErrorActionPreference = 'Stop'

$outDirs = @(
    'C:\Users\M-PC\AppData\Roaming\Azahar\sdmc\3ds\drally',
    'C:\Users\M-PC\rally\3ds-release\dRally_3ds',
    (Join-Path (Split-Path -Parent $PSScriptRoot) 'build\3ds')
)

$names = @('SAM SPEED','JANE HONDA','DUKE NUKEM','NASTY NICK','MOTOR MARY','MAD MAC','MATT MILER',
           'CLINT WEST','LEE VICE','DARK RYDER','GREG PECK','SUZY STOCK','IRON JOHN','MORI SATO',
           'CHER STONE','DIESEL JOE','MIC DAIR','LIZ ARDEN','BOGUS BILL','FARMER TED')

# the cars the original gives the AI in a new game (___2415ch.c)
$cars = @(5,5,5,4,4,4,4,3,3,3,2,2,2,2,1,1,1,0,0,0)

# the starting points the original hands out (___2415ch.c: ((201-77*log10(n+1))*2-11n)/4)
$points = @(100,86,76,69,62,56,51,46,42,38,34,30,27,23,20,17,13,10,6,1)

function Set-Dword([byte[]]$b, [int]$off, [int]$v){
    $b[$off+0] = [byte]($v -band 0xFF)
    $b[$off+1] = [byte](($v -shr 8) -band 0xFF)
    $b[$off+2] = [byte](($v -shr 16) -band 0xFF)
    $b[$off+3] = [byte](($v -shr 24) -band 0xFF)
}

function New-Racer([string]$name, [int]$car, [int]$pts, [int]$rank, [int]$face, [int]$color, [int]$money){
    $b = New-Object byte[] 0x6c
    $n = [Text.Encoding]::ASCII.GetBytes($name)
    [Array]::Copy($n, 0, $b, 0, [Math]::Min($n.Length, 0xb))

    Set-Dword $b 0x0c 0                    # damage
    Set-Dword $b 0x10 0                    # engine
    Set-Dword $b 0x14 0                    # tires
    Set-Dword $b 0x18 0                    # armor
    Set-Dword $b 0x1c $car
    Set-Dword $b 0x2c $color
    Set-Dword $b 0x30 $money
    Set-Dword $b 0x34 -1                   # loanshark_type
    Set-Dword $b 0x38 -1                   # loanshark_counter
    Set-Dword $b 0x3c 500                  # refund: what a Vagabond is worth
    Set-Dword $b 0x40 $face
    Set-Dword $b 0x44 $pts
    Set-Dword $b 0x48 $rank
    Set-Dword $b 0x4c 0                    # wins
    Set-Dword $b 0x50 0                    # races
    Set-Dword $b 0x54 0                    # bonus
    Set-Dword $b 0x58 0                    # income
    Set-Dword $b 0x5c 0                    # mines
    Set-Dword $b 0x60 0                    # spikes
    Set-Dword $b 0x64 0                    # rocket_fuel
    Set-Dword $b 0x68 0                    # sabotage

    return ,$b
}

# $difficulty is the value the game keeps in byte 3 (the enum in race___3f970h.c):
#       0 = speed makes me dizzy         1 = i live to ride
#       2 = petrol in my veins           3 = 30th Anniversary (the level this port adds)
# Only level 3 has the black anniversary adversary in the field, so an easy save is a plain championship.
function New-Save([string]$slotName, [int]$money, [int]$playerPoints, [int]$playerRank,
                  [int]$leaderPoints, [int]$difficulty = 3){
    $b = New-Object byte[] 0x883

    $b[0] = 0x2a                           # key
    $b[1] = 19                             # me: the player sits in the last seat
    $b[2] = 1                              # weapons on
    $b[3] = $difficulty                    # difficulty
    $n = [Text.Encoding]::ASCII.GetBytes($slotName)
    [Array]::Copy($n, 0, $b, 4, [Math]::Min($n.Length, 0xe))

    for ($i = 0; $i -lt 19; $i++){

        $pts  = $points[$i]
        if (($i -eq 0) -and ($leaderPoints -gt 0)) { $pts = $leaderPoints }   # the racer ahead of the player

        # the player sits at $playerRank, so everybody from there on moves one place down
        $rank = $(if (($i + 1) -lt $playerRank) { $i + 1 } else { $i + 2 })

        $r = New-Racer $names[$i] $cars[$i] $pts $rank $i $i (50000 + 1000*$i)
        [Array]::Copy($r, 0, $b, 0x13 + 0x6c*$i, 0x6c)
    }

    $p = New-Racer $slotName 0 $playerPoints $playerRank 19 19 $money
    [Array]::Copy($p, 0, $b, 0x13 + 0x6c*19, 0x6c)

    # encode exactly like the game does when it saves
    $key = [int]$b[0]
    for ($i = 1; $i -lt 0x883; $i++){
        $v = ([int]$b[$i] + 0x11*$i - $key) -band 0xFF
        $s = $i % 6
        $b[$i] = [byte](((($v -shr $s) -bor (($v -shl (8 - $s)) -band 0xFF))) -band 0xFF)
    }

    return ,$b
}

function Save-File([string]$file, [byte[]]$data){
    foreach ($d in $outDirs){
        if (-not (Test-Path $d)) { continue }
        [IO.File]::WriteAllBytes((Join-Path $d $file), $data)
        Write-Host ("  {0}  ({1} bytes)" -f (Join-Path $d $file), $data.Length)
    }
}

$million = 5000000

Write-Host '=== DR.SG1: millions of dollars, mid table (Shopping / Anniversary test) ==='
Save-File 'DR.SG1' (New-Save 'MILLIONS' $million 60 5 0)

Write-Host '=== DR.SG2: player first on 30th Anniversary, millions ==='
Save-File 'DR.SG2' (New-Save 'CHAMPION' $million 200 1 0)

Write-Host '=== DR.SG3: player second, one racer ahead - the final challenge should trigger ==='
Save-File 'DR.SG3' (New-Save 'FINAL RACE' $million 110 2 150)

# the same table as DR.SG1, only on the easiest level: no anniversary in the field, no final challenge
Write-Host '=== DR.SG4: millions of dollars on "speed makes me dizzy" (easy) ==='
Save-File 'DR.SG4' (New-Save 'EASY MONEY' $million 60 5 0 0)

# ---- verify: decode what was written and print what the game will see ------------------------------
Write-Host '=== verification (decoded again) ==='

foreach ($f in 'DR.SG1','DR.SG2','DR.SG3','DR.SG4'){

    $p = Join-Path $outDirs[0] $f
    if (-not (Test-Path $p)) { continue }

    $b = [IO.File]::ReadAllBytes($p)
    $key = [int]$b[0]

    for ($i = 1; $i -lt 0x883; $i++){
        $s = $i % 6
        $v = ((($b[$i] -shl $s) -bor (($b[$i] -shr (8 - $s)) -band 0xFF)) -band 0xFF)
        $b[$i] = [byte](($v - 0x11*$i + $key) -band 0xFF)
    }

    $me   = [int]$b[1]
    $diff = [int]$b[3]
    $nm   = [Text.Encoding]::ASCII.GetString($b, 4, 0xe).TrimEnd([char]0)

    $off   = 0x13 + 0x6c*$me
    $pname = [Text.Encoding]::ASCII.GetString($b, $off, 0xb).TrimEnd([char]0)
    $money = [BitConverter]::ToInt32($b, $off + 0x30)
    $pts   = [BitConverter]::ToInt32($b, $off + 0x44)
    $rank  = [BitConverter]::ToInt32($b, $off + 0x48)
    $car   = [BitConverter]::ToInt32($b, $off + 0x1c)

    $ahead = 0
    $best  = 0
    for ($i = 0; $i -lt 20; $i++){
        if ($i -eq $me) { continue }
        $o = 0x13 + 0x6c*$i
        $q = [BitConverter]::ToInt32($b, $o + 0x44)
        if ($q -gt $pts) { $ahead++ }
        if ($q -gt $best) { $best = $q }
    }

    # the black anniversary only exists on difficulty 3, the other three levels race without him
    $adv = $(if ($diff -eq 3) { 'on' } else { 'off' })

    Write-Host ("{0}: slot '{1}', difficulty {2} (anniversary {3}), me = seat {4} '{5}' car {6}, money {7}, {8} points, rank {9}, racers ahead {10} (best {11})" -f
        $f, $nm, $diff, $adv, $me, $pname, $car, $money, $pts, $rank, $ahead, $best)
}
