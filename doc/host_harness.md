# Der PC-Harness (dRally auf dem Windows-PC fahren, ohne Azahar)

Der Chaos-Build läuft auf dem 3DS über Azahar nur **von Hand**: Bildschirm anschauen, Menü durchklicken,
warten. Für die Fehlersuche an den grauen Linien, am Grid und an den Extras ist das zu langsam und nicht
wiederholbar - jeder Test war eine Sitzung.

Der Harness ist derselbe Engine-Code (inklusive `-DDR3_CHAOS`, also die Chaos-Difficulty des
Haupt-Builds), für den PC gebaut (`-DDR3_HOST`) und **von einem Skript gefahren**:

* Tasten werden in das echte SDL-Event-Queue gelegt - die Menüs laufen wie von Hand bedient;
* `quickrace` springt **direkt ins Rennen** (Feld aufsetzen + `race_main`, wie der Signup-Screen);
* Frame-Dumps zeigen, was die Engine gezeichnet hat;
* `frames <n>` beendet den Lauf, ein Bericht fasst das Log zusammen.

Der Harness ist **kein Port** und **kein Ersatz** für den 3DS-Test: er ändert keinen Spielcode, nur die
drei Stellen, die ohne libctru nicht übersetzen (Logpfade, Takt, `_ReturnAddress`) und die Ein/Ausgabe.
Was er nicht abdeckt, steht unten.

## 1. Bauen

```powershell
# erzeugt tests\dRally3DSHost.vcxproj aus dem vorhandenen Check-Projekt und baut es
powershell -ExecutionPolicy Bypass -File scripts\3ds\gen_host_project.ps1 -Build
# -> tests\host\Release\dRally3DSHost.exe
```

Voraussetzungen: Visual Studio 2022 (MSBuild) und SDL2 unter `C:\lib\SDL2-2.0.18` / `C:\include\SDL2`
(dieselben Pfade wie beim Check-Projekt `tests\dRally3DSCheck.vcxproj`).

Das Projekt enthält genau die Quellen des 3DS-Hauptbuilds. Die einen Unterschied machen:

| Define | warum |
|---|---|
| `DR3_CHAOS` | die Chaos-Difficulty ist im Haupt-Build; hier ist sie auch drin |
| `DR3_HOST` | Harness: relative Logpfade, SDL-Takt, Skript, Direktstart (`platform_3ds/dr3_host.h/.c`) |
| `DR_LETTERBOX`, `DR3_LOG_KEYS` | wie der Haupt-Build |
| **nicht** `DR3_USE_GFX` | das ist libctru (3DS-Framebuffer) - auf dem PC nicht linkbar |

## 2. Fahren

```powershell
# ein Chaos-Rennen, 18 Extras, Start bei 12 s, nach 3000 Frames Schluss
powershell -ExecutionPolicy Bypass -File scripts\3ds\run_host.ps1 -Extras 18 -QuickRaceMs 12000 -Frames 3000

# dabei alle 1,5 s ein Bild
powershell -ExecutionPolicy Bypass -File scripts\3ds\run_host.ps1 -Extras 18 -QuickRaceMs 12000 -Frames 3000 `
           -DumpEvery 1500
```

Der Runner legt `run_host\` einmalig aus `3ds-release\dRally_3ds` an (ENGINE.BPA, DR.SG*, TR*.BPA, CINEM),
kopiert Exe + `SDL2.dll` hinein, schreibt die Schalter des Chaos-Builds und startet mit **CWD = run_host**
(die Engine öffnet ihre Assets relativ zum CWD, `platform_3ds/dr3_paths.c`).

Wichtige Schalter von `run_host.ps1`:

| Schalter | Wirkung |
|---|---|
| `-QuickRaceMs <ms>` | **direkt ins Rennen** - Feld + Difficulty wie im Menü, ohne Menü |
| `-Extras <0..20>` | wie viele Extras der nächste Rennstart hat (`chaos_extras.txt`) |
| `-Frames <n>` | nach n präsentierten Frames aufhören |
| `-DumpEvery <ms>` / `-DumpPath` | Bilder schreiben (8-bit BMP, 320x240/640x480) |
| `-AutoKeyMs <ms> -AutoKey <name>` | eine Taste immer wieder drücken (z. B. `return` fürs Intro) |
| `-TimeoutSec <s>` | Notbremse |
| `-Stage` | `run_host\` neu aus den Spieldaten aufsetzen |
| `-Audio` | echter Audiottreiber (Standard: `SDL_AUDIODRIVER=dummy`) |
| `-ExtraArgs '<argumente>'` | Argumente direkt an die Exe, z. B. `--auto 1` (Autopilot) oder `--track 16` |

Ohne `-QuickRaceMs` läuft das Spiel normal im Frontend - **F9** startet dann jederzeit das Direktrennen,
**F10** schreibt einen Frame-Dump (`f10.bmp`).

### Der Autopilot (`auto 1` / `--auto 1`)

Eine Tastenroute kann **nicht lenken**. Das Grid des Chaos-Builds ist eine Schlange von zwanzig Autos
hinter der Startlinie und auf TR8 rund **750 px** lang - der letzte Platz (der des Spielers, Standings 20
von 20) steht damit schon **hinter der ersten Kurve** der Strecke, etwa 50 px vor einem rechten Winkel. Mit
festgehaltenem Gas fährt das Auto dort geradeaus weiter, verlässt die Straße bei der ersten Kurve und
schleift danach am Kartenrand entlang: die Physik klemmt jedes Auto auf `y >= 20` (race___4c434h.c), und im
Log sieht das so aus:

```
[dr3] chaos: PLAYER step  48: ... ctrls 0x31 ... at 754.8,20.0 ... ground 12,12,15,15,15,15
[dr3] chaos: PLAYER step 1168: ... ctrls 0x31 ... at 715.3,20.0 ... ground 0,12,15,12,15,15
[dr3] chaos: PLAYER step 1216: ... ctrls 0x31 ... at 783.1,20.0 ... ground 0,12,15,12,15,15
```

`ctrls 0x31` ist Gas + Turbo + Maschinengewehr und **nie** 0x04/0x08 (links/rechts) - das Feld fährt davon,
ein Rammstoß (und damit der Killbonus) kommt so nie zustande. Das ist **kein Platzierungsfehler**: der
Host-Probe (`logs/grid_probe.c`, „20 of 20 cars placed, 0 off the road") und der Grid-Zähler im Log („16 of
16 extras placed on the road") zeigen alle zwanzig Plätze auf der Straße, entlang der Straße stehend, in
der Reihenfolge der Standings.

Mit `auto 1` fährt deshalb **die Engine-KI** (`race___4b62ch`) auch das Auto des Spielers, und die
Eingabespur schreibt seine Steuerung nicht mehr (`race_main.c`, beide Stellen hinter `#if defined(DR3_HOST)`;
Schalter: `platform_3ds/dr3_host.h`). Der Lauf fährt dann ein Rennen statt eines Autos gegen den Kartenrand:

```
[dr3] chaos: PLAYER step 960: ctrls 0x00 ... hp 100876 ... at 338.8,507.6 ... ground 15,15,15,15,15,15 mid 15 mine 338
```

`ctrls` wechselt mit den Entscheidungen der KI (0x00 bremsen, 0x01 Gas, 0x05 Gas + links), `ground` ist
überall 15 (Straße), und der Spieler legt Minen, schießt und rammt wie jedes andere Auto - nur so kann ein
Lauf einen Killbonus sehen. Die Tasten aus `dr3_keys.txt` erreichen das Auto dann **nicht mehr** (sie
werden gar nicht mehr in die Steuerung kopiert); das Intro-Skip (`return`) wirkt weiter, weil es den
Frontend-Pfad geht. Standard ist **aus**: ohne `auto` ist ein Lauf genau der Lauf wie vorher.

### Den Killbonus prüfen (`power`, `hp`, `field`)

Der Chaos-Killbonus (`race___4dcach.c`) zahlt Geld für ein Wrack, das **der Spieler** verursacht hat.  Was
"verursacht" heißt, entscheidet genau eine Stelle (`dr3_chaos_wreck`, §"Wer ein Wrack bezahlt"): der
Spieler muss diesem Auto höchstens 120 Frames (etwa zwei Sekunden) vorher HP genommen oder es berührt
haben - mit der Stoßstange, der Maschinenpistole oder einer Ramme.  Ein Wrack, an dem er nicht beteiligt
war (Wand, Explosion, zwei Gegner unter sich), zahlt nichts.  Genau das ist im Lauf nachweisbar:

* `field <ms>` schreibt jede Sekunde **jedes** Auto ins Log (`[host] field car N hp ... pos ... lap ...`) -
  ein Wrack ist ein Auto mit `hp 0`, und damit steht im Log, wer wann kaputtgegangen ist;
* eine `kill bonus`-Zeile **an derselben Stelle** heißt: dieser Abschuss hat gezahlt
  (`kill bonus: NAME (car N, model M) wrecked by CAUSE pays $X into seat S: money A -> B, text slot T`) -
  Betrag nach Modell des Opfers, `money` wächst um genau den Betrag, der Textplatz ist einer von 0x10..0x13,
  und `CAUSE` nennt, was die letzten HP genommen hat (`the player's bumper`, `the player's gun`,
  `its own boost`, `a spectator`, `the road border`, `a mine or an explosion`);
* ein Auto, das auf `hp 0` geht **ohne** so eine Zeile, ist der Fall "fremder Schaden zahlt nichts" - und
  seit dem 26. September 2026 steht der Grund als eigene Zeile daneben:
  `NAME (car N) wrecked by CAUSE - no kill bonus (the player had no part in it)`.

Nur passiert das in einem normalen Chaos-Rennen mit dem Harness **nie**: der Spieler fährt das schwächste
Auto des Feldes an letzter Startposition (Rang 20 von 20).  Seine Höchstgeschwindigkeit ist
`0.784 * s_94.__4` px/frame, mit dem Rookie-Auto des Savegames (2.5) also 1.96 - das Feld fährt 3.9, er
holt niemanden ein (ein komplettes Rennen, 8000 Frames, kein einziges Wrack).  Drei Schalter machen die
Situation erreichbar, ohne das Gemessene zu verfälschen:

| Schalter | Wirkung |
|---|---|
| `power <x>` | Motorleistung des Spielerautos (`s_94.__4`). `power 6` bringt es auf ~4.7 px/frame und damit in die Liga des Feldes - im Spiel kommt das aus dem Shop (Motor-Upgrades) |
| `hp <auto> <n>` | mit wie vielen HP ein Auto startet (102400 = unbeschädigt, `0x400*(100-Schaden)`). `hp 19 2500` heißt: zwei Rammstöße des Spielers sind ein Wrack |
| `nomine <0\|1>` | kein Auto legt eine Mine. Die KI des Chaos-Feldes vermint die Strecke, auf der sie fährt: mit abgeschwächten Autos waren das 10 von 16 Wracks in einer Runde (Host-Lauf) - und eine Mine zahlt nichts, sie verdeckt also genau das, was hier geprüft wird |
| `ammo <0\|1>` | volle Munition für den Spieler - der In-Race-Cheat des Spiels ("DREAD", `race___56594h.c`, setzt `s_94 +0x30`). Die Maschinenpistole ruft denselben Killbonus (`race___4ff50h.c`), braucht aber Munition |
| `weapon <0\|1>` | der Spieler **hat** die Maschinenpistole ab der ersten Frame (`s_94 +0x2c`, das Feld, das ein Waffen-Pickup füllt) - den Feuerknopf drückt der Autopilot von selbst. Ohne den Schalter hängt der Kill des Spielers davon ab, ob zufällig ein Waffen-Pickup geladen ist, wenn endlich ein Auto vor ihm fährt; mit ihm ist er reproduzierbar |
| `nohurt <0\|1>` | volle HP für den Spieler - der zweite In-Race-Cheat des Spiels ("DRUB", `race___56594h.c`). Der Spieler fährt das schwächste Auto des Feldes; ohne das Wracken ihn die Führenden direkt nach dem ersten Kill (Host-Lauf: eine Zahlung, dann `hp 0`) - für **mehrere** Zahlungen in einem Rennen |
| `laps <n>` | die **Renndauer** des Laufs (`NUM_OF_LAPS`, gefragt in `___33010h.c` direkt vor `race_main`). Ein Chaos-Feld fährt sonst die fünf Runden der Strecke; der **Report** nach dem Rennen („Your Statistics", das Rang-Geld, das Kill-Geld des Rennens) kommt aber erst nach der letzten. Mit `laps 1` ist er in einem kurzen Lauf erreichbar |
| `mode <n>` | die **Rennart** eines Chaos-Rennens (`drally.h`: `DR3_CHAOS_MODE_...`): `0` = ganzen Feld (Standard), `1` = **Duell** (1v1 gegen einen Fahrer innerhalb `DR3_CHAOS_DUEL_RANK_WINDOW` Rängen um deinen eigenen, zufällig ausgelost - Pott + Elo), `2` = **Destroy 'em all** (kein Ziel, letzter Wagen gewinnt den Pott - siehe `dmsudden`), `3` = **Zufall** (der Sign-up-Screen dreht Strecke + Modus). Der Rennaufbau liest sie bei jedem Rennen |
| `dmsudden <n>` | die **Sudden Death** eines „Destroy 'em all" (`platform_3ds/dr3_dm.h`, `chaos_dm.txt`): nach `n` Sekunden Rennzeit verliert **jedes** Auto - den Spieler eingeschlossen - ein Prozent seiner HP pro Sekunde (1024 HP, die Einheit der DAMAGE-Anzeige des Spiels), und sobald nur noch ein Auto HP hat, windet der Port das Rennen ab. `0` schaltet sie ab (dann läuft der Modus wie vorher, bis ein Auto übrig ist). Der Aufbau liest den Wert bei jedem Rennen und schreibt ihn ins Log. Der kurze Wert (`dmsudden 8`) ist der Prüfweg für `logs/run_dm7.ps1` |
| `dmlap <n>` | die **Lap-Pflicht** desselben Modus (`chaos_dm_lap.txt`): alle `n` Sekunden Rennzeit wird geprüft, ob jedes Auto seit dem letzten Check eine Runde gefahren ist - wenn nicht, verliert es **25 % seiner HP** (25600, dieselben 1024 HP je Prozent; vier verpasste Checks = Wrack). Damit ist „in der Ecke stehen und warten" keine Siegstrategie mehr (der Pott geht an den Überlebenden). `0` schaltet sie ab; der Standard ist `60` (eine Runde je Minute, gefahren wird sie in ~25-40 s). Der Steh-Prüflauf ist `logs/run_dm9.ps1` (`auto 0` = der Spieler fährt nicht los) |
| `spin <n>` | würfelt `n` mal genau die Wahl, die der Zufalls-Selector im Sign-up-Screen trifft (Strecke + Modus) und schreibt die Verteilung ins Log — damit ist der Zufall prüfbar, ohne den Menü-Screen zu erreichen |
| `raced <0\|1>` | setzt den **Karriere-Zähler** des geladenen Spielstands (`s_6c[player].races`, gelesen von `dr3_chaos_raced`) auf 1 (bzw. zurück auf 0). Nur damit dreht das Rad des Sign-up-Screens überhaupt (`signup <ms>`): das erste Rennen einer Karriere bleibt im Spiel ein gewöhnliches Rennen. Der Befehl wird zusätzlich direkt vor dem Aufruf des Screens angewandt, weil das Skript gelesen wird, bevor der Spielstand geladen ist. Prüflauf für das Rad in zwei Runden ist `logs/run_spin3.ps1` |

Ein kompletter Prüflauf:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\3ds\run_host.ps1 -Extras 18 -QuickRaceMs 15000 -Frames 5000 `
           -ExtraArgs '--auto 1 --track 16 --power 6 --nomine 1 --weapon 1 --field 500 --nohurt 1 --hp 4 2500 ... --hp 9 2500'
```

`--track 16` ist TR8 rückwärts (0..8 = TR1..TR9 vorwärts, 9..17 dieselben rückwärts) - dieselbe Strecke,
auf der das Grid des Spielers hinter der ersten Kurve liegt.  Was so ein Lauf zeigt (26. September 2026,
`logs/host_run_payout_fixiert.log` und `logs/host_run_multi_cheats.log`):

```
[host] field car  3 hp      0 pos  5 ... at 406.0,211.6   <- the player     <- 16 Wracks im Rennen
[host] field car 17 hp      0 pos 10 ... at 398.0,291.5                    <- nur dieses hat gezahlt
[dr3] chaos: kill bonus: LIZ ARDEN (car 17, model 1) pays $75 into seat 19: money 495 -> 570, text slot 16
[dr3] chaos: kill bonus popup: $75 at 241,104 (slot 16, life 140)          <- 85 Frames lang ueber dem Wrack
```

und in einem zweiten Lauf **zwei** Zahlungen (die Geldsumme wächst mit):

```
[dr3] chaos: kill bonus: MORI SATO (car 13, model 2) pays $150 into seat 19: money 495 -> 645, text slot 16
[dr3] chaos: kill bonus: GREG PECK (car 10, model 3) pays $200 into seat 19: money 645 -> 845, text slot 16
```

Das ist der Bonus, wie er sein soll: Betrag nach Modell des Opfers (Modell 1/2/3 -> $75/$150/$200 = die
Tabelle in `race___4dcach.c`), in den Platz des Spielers (19), das Geld wächst um genau den Betrag, der
Textplatz ist einer von 0x10..0x13, und die 15 übrigen Wracks (auch das eigene des Spielers) zahlen
**nichts** - fremder Schaden.  `scripts/3ds/report_host.ps1` zählt am Ende genau das auf: jedes Auto, das
auf 0 HP gegangen ist, und ob dazu eine Zahlung im Log steht.  (`power`/`hp`/`nomine`/`ammo`/`nohurt` sind
Harness-Schalter; das *erste* Mal, dass ein Gegner in einem Host-Lauf gewrackt wurde, hat übrigens einen
Absturz in `dr3_chaos_kill_bonus` gefunden - §6.)

### Wer ein Wrack bezahlt (der Zeitstempel)

Wer zahlt, entscheidet genau eine Stelle: `dr3_chaos_wreck(victim, hp_before, cause)` in `race___4dcach.c`.
Jede Stelle, die einem Auto HP nehmen kann, ruft sie direkt nach ihrem Schreibzugriff auf und meldet vorher,
was sie getan hat:

* **der Spieler selbst**: seine Stoßstange (die Kollisionsschleife in `race___4dcach.c`) und seine
  Maschinenpistole (`race___4ff50h.c`) melden sich als Schaden des Spielers (`dr3_chaos_player_damage`);
* **ein Rempler**: die Kollisionsschleife markiert **jede** Paarung, in der der Spieler steckt und deren
  Bilder sich überlappen, auch ohne HP-Verlust (`dr3_chaos_player_contact`).  Ein Auto, das er von der
  Straße schiebt oder in den Pulk drückt, ist damit seines - auch wenn es danach ein Dritter erledigt;
* **die anderen vier Stellen, die einem Auto HP nehmen können**, fragen dieselbe Stelle und verlassen sich
  ganz auf den Zeitstempel (sie wissen nicht, *wer* schuld war): das eigene Boost-Feuer (`race___4c434h.c`),
  ein gerammter Zuschauer (`race___52ac4h.c`), der Bordstein (`race_main.c`; dort steht die zweite Kopie
  der Aufprallrechnung für ein Auto, das am Kartenrand klemmt) und eine Mine oder Explosion
  (`race___514d8h.c`).  Ohne sie fehlten genau die Wracks, die ein Spieler als "ich habe ihn erledigt"
  erlebt - und im Log fehlte die Zeile, die sagt, wodurch es starb.

Sechs Stellen sind es, weil sonst nichts im Baum die HP eines Autos senkt - geprüft in allen Schreibweisen
(`___1de580h+0x94*..+0x18`, `s_94[..].__18` und die umgedrehte Adressform).  `race_main.c` gehört dazu,
obwohl der eigentliche Aufprall in `race___4dcach.c` steht: die Bordstein-Rechnung dort trifft ein Auto,
bei dem der Aufprall schon passiert ist.

Bezahlt wird, wenn ein Auto in dieser Frame von HP > 0 auf 0 gegangen ist **und** die letzte Handlung des
Spielers an ihm höchstens `DR3_KILLBONUS_BLAME` Frames her ist (120, etwa zwei Sekunden).  Die Uhr
(`dr3_chaos_frame`, einmal pro Rennframe aus `race___4dcach`) startet hoch über allen
"nie berührt"-Stempeln: die Stempel beginnen bei 0, ein Auto in der Startphase des Rennens wird also nicht
plötzlich dem Spieler zugeschrieben.  Das eigene Wrack des Spielers zahlt nie.

Zwei Gründe für die Spanne statt "nur der letzte Treffer":

* die Ramme ist die schwächste Waffe - ein angerempeltes Auto geht selten in derselben Frame drauf, sondern
  ein paar Frames später an dem, was *es* trifft (oder an seinem eigenen Boost);
* ein Auto, das der Spieler in den Pulk schiebt, wird oft von einem **Dritten** gewrackt.  Für den Spieler
  ist es trotzdem sein Wrack - und deshalb zahlt es.

Das `$` zeichnet `race___51eb4h.c`: dieselbe 6x6-Schrift wie bei einem aufgesammelten Item, aber doppelt so
groß (`DR3_KILL_BONUS_TEXT_SCALE`) und **mittig auf das Wrack gesetzt** - in dem Koordinatenraum, in dem
die Engine die *Welt* positioniert (das `x` eines Autos: `__4 = XLocation - TRX_VIEWPORT_TL_X +
CURRENT_VIEWPORT_X`, `race___4f300h.c`).  Drei Fallen stecken darin, alle aus Läufen (Bild + Log
zusammen):

* **Der Objektversatz `0x60`.** Der Text-Blitter (`___424c8h_cdecl`) schreibt an die *rohe* Bufferposition,
  die Welt aber wird durch einen Blitt gezeichnet, dessen linke Kante `CURRENT_VIEWPORT_X + 0x60` ist
  (`helper01`, `race___4f300h.c`).  Dieselbe `x` liegt für den Text also `0x60` weiter rechts
  (`DR3_KILL_BONUS_TEXT_OBJ`) - die Item-Popups der Engine haben den Versatz (`x + 0x60`); **fehlt er,
  landet das `$` 96 px links neben dem Wrack** (nachgemessen: Text bei x = 76..118, sein Auto bei
  174..214).
* **Die rechte Kante.** Der Item-Text sitzt bei `x + 0x60` und prüft nur `x` gegen den Bildschirm - ein
  Wrack in der rechten Bildhälfte schob sein `$` damit komplett aus dem Bild (Host-Lauf: `x = 341..413`).
  Die Zahl wird deshalb in das sichtbare Band geklemmt.
* **Das HUD.** Es wird *nach* `race___51eb4h` gezeichnet und deckt den linken Rand zu (seine Autoliste
  endet bei x ≈ 75 in demselben Raum - auf einem Host-Bild nachgemessen).  Rutscht das Wrack dort hinein,
  bleibt die Zahl am HUD-Rand stehen, statt zu verschwinden: das Wrack ist in dem Moment selbst verdeckt.
  Verlässt das Wrack das Bild **oben oder unten**, wird gar nichts gezeichnet (wie der Item-Text der
  Engine) - sonst klebte die Zahl am Bildrand, weit weg von ihrem Wrack.

Zahlung und Popup stimmen damit überein: `kill bonus popup: $50 at screen 234,72 (buffer 330,72,
wreck 252, ...)` - der Text (234..267) liegt auf dem Wrack (232..272), im Bild nachgemessen.

### Den Renn-Report prüfen („Your Statistics", `laps`)

Der Report nach dem Rennen ist der Bildschirm `___3892ch_cdecl()` -> `___36adch()` („Your Statistics:").
Er hat zwei Kästen, und beide sind im Chaos-Modus vollständig:

* oben die **Laufbahn** des Fahrers: `Position`, `Races won`, `Total races`, `Total income`, `Total money`;
* darunter das **Ergebnis dieses Rennens**: `Placing`, `Race income`, `Bonus income`, `Total race income`,
  `Number of laps`, `Race time` und der Streckenrekord.

Woher die Zahlen kommen (`___33010h.c` rechnet sie direkt nach `race_main` aus, `___36adch.c` liest sie):

| Zeile des Reports | Feld | wer es füllt |
|---|---|---|
| `Total money` | `racer.money` (+0x30) | Shop, Kill-Geld (`race___4dcach.c`), Rang-Geld (`___33010h.c`) |
| `Total income` | `racer.income` (+0x58) | wie `Total money` - jedes Rennen und jeder Kill zählt hier mit |
| `Race income` | `___196aech` | das **Preisgeld des Rangs** (die Stufen des Rennens, im Chaos die Rang-Leiter) |
| `Bonus income` | `___196af0h` | das schadensabhängige Preisgeld **plus** die Kills des Rennens |
| `Total race income` | Summe der beiden | |

Zwei Dinge daran sind neu (beide nur im Chaos-Modus):

* **Jede Position zahlt** (`DR3_CHAOS_RANK_MONEY`, `drally.h`): die Stufen des Spiels erreichen nur die drei
  ersten Plätze, auf einem Feld von zwanzig Autos liegt der eigene Platz aber meist tiefer. Die Leiter ist
  `DR3_CHAOS_RANK_MONEY * (Autos - Platz + 1) / Autos` - bei zwanzig Autos also **$5000 für Platz 1**,
  $4750 für Platz 2, ... $500 für Platz 19 und $250 für den letzten. Sie geht in `racer.money`,
  `racer.income` und in die Zeile `Race income`. **Sie ersetzt die Renn-Stufen** statt sich auf sie zu
  legen: der `switch`, der 750/3000/12000 für die ersten drei Plätze auszahlt, wird im Chaos übersprungen
  (`___33010h.c`, `dr3_chaos_active() ? -1 : ...`), Rang 1 bekommt also genau $5000. Die Arena (das vierte
  Rennen) bleibt „GLORY" und zahlt nichts.
* **Die Kills des Rennens sind Einkommen**: das Kill-Geld landet beim Wrack sofort in `racer.money` **und**
  `racer.income` (vorher nur in `money` - „Total income" ging an den Abschüssen vorbei), und die Summe des
  Rennens (`dr3_chaos_kill_money()`) steht im Report als `Bonus income` neben dem Preisgeld. Doppelt gezählt
  wird nichts: `money`/`income` schreibt der Kill, der Report addiert nur seine eigene Zeile.

Das Log nennt beide Zahlen in einer Zeile und schreibt sie sofort auf die Platte - der Bildschirm wartet
danach auf eine Taste:

```
[dr3] chaos: report of the race: place 6 of 20 -> rank money $3750, kills $375: race income $3750 + bonus income $425 = $4175, money 4670, income 4175
```

Ein Lauf, der bis dorthin kommt (eine Runde, damit der Report nicht Minuten entfernt ist):

```powershell
powershell -ExecutionPolicy Bypass -File scripts\3ds\run_host.ps1 -Extras 18 -QuickRaceMs 15000 -Frames 7000 `
           -DumpEvery 200 -TimeoutSec 900 -AutoKeyMs 5000 -AutoKey return `
           -ExtraArgs '--auto 1 --track 16 --power 6 --nomine 1 --ammo 1 --weapon 1 --field 1000 --nohurt 1 --laps 1 --hp 4 2500 ... --hp 9 2500'
```

**`-AutoKey return` gehört dazu.** `race_main` kehrt erst zurück, wenn der Bildschirm „RACE OVER! - PRESS
ENTER TO CONTINUE" bestätigt ist; erst danach rechnet `___33010h.c` aus und zeichnet den Report. Ohne den
Tastendruck stand der Lauf dort bis zum Frame-Limit (Bild `logs/view_f_1289.png`).

Das Ergebnis eines solchen Laufs (26. September 2026, `logs/run_report3.ps1`, `logs/report_demo1/`) - links
das Log, rechts der Bildschirm:

| Log / Report | Wert | woher |
|---|---|---|
| `place 6 of 20` | `Placing: 6.` | die Position des Spielerautos am Rennende (`___196ae8h`), im Rennen stand „6TH" im HUD |
| `rank money $3750` | `RACE INCOME: $3750` | die Chaos-Leiter, `5000*(20-6+1)/20` |
| `kills $375` | `BONUS INCOME: $425` | die vier Abschüsse des Rennens (`$50+$75+$75+$150`) plus das schadensabhängige Preisgeld des Rennens (`$50`) |
| `race income $3750 + bonus income $425` | `TOTAL RACE INCOME: $4175` | die Summe, die `___36adch.c` selbst addiert |
| `money 4670` | `TOTAL MONEY: $4670` | 495 aus dem Savegame + 4175 |
| `income 4175` | `TOTAL INCOME: $4175` | dasselbe Einkommen, ohne den Anfangsbestand |

`RACE INCOME` ist damit **jede** Position wert: die Leiter läuft von `$5000` für Platz 1 über `$3750` für
Platz 6 bis `$250` für den letzten - auf einem Feld von zwanzig Autos, wo die drei Stufen des Spiels nichts
mehr zahlen. Und `BONUS INCOME`/`TOTAL INCOME` enthalten die Kills, die `money` schon lange enthielt.

*Nachtrag 27. September 2026:* Die Leiter wurde neu geschnitten und liegt jetzt in
`platform_3ds/dr3_payout.h` (`dr3_chaos_rank_money`): `$100` pro Platz **oberhalb des letzten**, ein Sprung
auf dem Podium (`$5000`/`$4000`/`$3000`) und **`$0` für den letzten Platz** - dort landet ein gechrottetes
Auto, und vorher zahlte schon ein Fünf-Wagen-Feld dafür noch `$1000`. Die Zahlen oben sind der Stand der
Läufe vom 26. September; die neue Tabelle steht Platz für Platz in `tests/test_dr3.c` (`test_payout`) und ein
Rennlauf dazu ist `logs/run_payout11.ps1` (der Spieler startet mit 1 HP, das erste Antippen im Startgetümmel
ist ein Wrack und beendet das Rennen): `report of the race: place 20 of 20 -> rank money $0, kills $0: race
income $0 + bonus income $0 = $0, money 495, income 0` - auf demselben Feld von zwanzig wären das vorher noch
`$250` gewesen (und `$1000` in dem Fünf-Wagen-Feld, das zu der Meldung geführt hat).

Dass die Leiter die Stufen **ersetzt** (und nicht auf sie drauflegt), zeigt ein Lauf, in dem der Spieler
auf dem Podium landet - dort zahlt das Spiel sonst seine eigene Stufe:

```
powershell -ExecutionPolicy Bypass -File scripts\3ds\run_host.ps1 -Extras 18 -QuickRaceMs 15000 -Frames 4500 `
           -DumpEvery 250 -TimeoutSec 600 -AutoKeyMs 5000 -AutoKey return `
           -ExtraArgs '--auto 1 --track 16 --power 10 --nomine 1 --ammo 1 --weapon 1 --field 1000 --nohurt 1 --laps 1 --hp 0 800 --hp 1 800 --hp 2 800 --hp 4 2500 ... --hp 19 2500'

[dr3] chaos: report of the race: place 3 of 20 -> rank money $4500, kills $125: race income $4500 + bonus income $125 = $4625, money 5120, income 4625
```

`$4500` ist genau `5000*(20-3+1)/20`; mit der Stufe des Rennens für Platz 3 ($187 im leichten, $375 im
mittleren Rennen) stünden dort $4687 bzw. $4875. (`power 10` macht den Spieler doppelt so schnell, die
schwachen Autos 0..2 sind die Streckenführer, die sonst vor ihm ins Ziel fahren - Auto 3 ist sein eigener
Startplatz, siehe `doc/host_harness.md` §2.)

Derselbe Aufbau, nur mit *einem* schwachen Führer (der Spieler überholt ihn und gewinnt), liefert die
Kopfzahl: `place 1 of 20 -> rank money $5000, kills $1000: race income $5000 + bonus income $1000 =
$6000, money 6495, income 6000` - **$5000 und nicht $5750**. Alle drei Zeilen (`logs/report_demo1/
rank_money_lines.log`): Platz 6 → $3750, Platz 3 → $4500, Platz 1 → $5000.

### Die Ergebnis-Tafeln prüfen (`slot`, `laps`)

Der Screen zwischen dem Rennen und dem Report ist `___3892ch_cdecl()`: links die Rangliste, rechts die
Ergebnis-Tafel des Rennens („MEDIUM RACE RESULTS", `___36358h_cdecl` - die Tafel der Klasse, in der das
Rennen lief, `slot`/`--slot`, `D(___1a1028h)`; ein Chaos-Rennen fährt das ganze Feld in *einer* Klasse).  Sie
zeichnet **vier Zeilen**, und jede Zeile ist ein Fahrer: Portrait (`s_6c[row].face`), Auto-Bild
(`s_6c[row].car`), Name und der Rang als kleines „N." darunter.  Wer in den Zeilen steht, ist der Punkt, an
dem der Port einen Bug hatte: die Originalschleife suchte die Plätze 1..4 unter den *ersten vier Autos* und
nahm die Position des Autos als Index in die Entry-Liste - in einem Vier-Auto-Rennen richtig, in einem
Chaos-Feld von zwanzig Autos nicht mehr, und eine Zeile, die keines der ersten vier Autos füllte, zeigte den
Fahrer, der zufällig noch im Speicher stand (auf dem 3DS dreimal „JANE HONDA").  Seit dem 27. September 2026
bildet `dr3_results_rows()` (`platform_3ds/dr3_results.h`) die Plätze über das ganze Feld ab, und jede Tafel
schreibt ihre vier Zeilen ins Log:

```
[dr3] chaos: results rows: 1. DUKE NUKEM (racer 1), 2. NASTY NICK (2), 3. JANE HONDA (0), 4. MATT MILER (5) out of 20 car(s)
```

Die Zeile steht direkt hinter den `points: place N of M -> ...`-Zeilen desselben Rennens, ein Lauf ist damit
Zeile für Zeile vergleichbar (`1.` der Tafel = `place 1 of 20`).  Der Prüflauf:

```powershell
powershell -ExecutionPolicy Bypass -File logs\run_results1.ps1
```

Er fährt ein **Medium**-Rennen (`--slot 1`) mit dem Chaos-Feld (`-Extras 18`, `--laps 1`) bis zum Report.  Die
Tafeln sind nur ~5 s zu sehen (sie warten auf eine Taste, `-AutoKeyMs 5000 -AutoKey return`), deshalb ein
Bild je Sekunde.  Vorher und nachher (`logs/res1_look/`):

* `1_vorher_medium_panel_jane_honda_4x.png` - vor dem Fix: **alle vier** Zeilen „JANE HONDA" (Rang „1."),
  obwohl das Rennen 1. JANE HONDA, 2. NASTY NICK, 3. DUKE NUKEM, 4. MOTOR MARY gefahren war;
* `3_nachher_medium_panel_richtig.png` - nach dem Fix: 1. DUKE NUKEM, 2. NASTY NICK, 3. JANE HONDA,
  4. MATT MILER, jeder mit seinem eigenen Portrait, Auto und Rang;
* `4_nachher_report.png` - der Report („Your Statistics") folgt wie immer.

Ein Rennen mit *voller* Entry-Liste (ohne Chaos-Feld) prüft dasselbe ohne die `-Extras`: die Tafel muss die
vier Plätze des Rennens zeigen, die im Log als `points: place N of M -> +X for NAME` stehen.


### Fahrzeugfarben und der PC-Present-Pfad

Die Portierung hat einen eigenen Present-Pfad (`platform_3ds/dr3_blit.c`, `drally_linux_c.c`): das 8-Bit-Bild
der Engine wird in **einem** Durchgang über eine Palette-LUT in 32-Bit-Pixel umgesetzt - und **dabei werden
die Autos gefärbt**. Bis zum 26. September 2026 war beides in `dr3_fb.c` (libctru, `<3ds.h>`) eingesperrt
und nur der 3DS hatte es; der PC nahm den SDL-Renderer-Pfad, der **pro Frame eine Textur erzeugte und
wieder freigab** - und dessen Bild keine Fahrzeugfarben hatte.

Was jetzt beide Seiten teilen:

| Baustein | Datei | Wirkung |
|---|---|---|
| `dr3_carcolour_tint_cars(...)` | `platform_3ds/dr3_blit.c` | die ganze Tint-Stufe: Maske auswerten, 10 Schattierungen je Auto bauen, Pixel färben. Ziel und Stride kommen als Argumente, deshalb passt dieselbe Funktion auf den **rotieren** 3DS-Framebuffer `(stride x = Zeilenbreite, stride y = -1)` und auf ein **lineares** Fenster/Textur-Pixelarray `(1, Zeilenbreite)` |
| `dr3_fb_tint_cars(...)` | `platform_3ds/dr3_fb.c` | nur noch der 3DS-Aufruf der geteilten Stufe (Ziel = rotierender Top-Screen) |
| `dr3_present_is_race(...)`, `dr3_present_row_off(...)` | `drally_linux_c.c` | „ist das der Renn-Screen" (nur dessen Maske gilt) und der VGA13-Zeilenversatz (20 Zeilen) |
| PC-Present | `drally_linux_c.c` | LUT in eine **einmal** erzeugte Streaming-Textur (`SDL_LockTexture` → `dr3_blit8_lut32` → Tint → `SDL_UnlockTexture` → `SDL_RenderCopy`/`SDL_RenderPresent`), Skalierung macht der Renderer (`SDL_RenderSetLogicalSize`). Ein Fenster mit Renderer hat **keine** Window-Surface, deshalb Textur statt Surface |
| 3DS-Present | `drally_linux_c.c` + `dr3_fb.c` | unverändert: direkter (rotierter) Hardware-Framebuffer, Fenster-Surface nur als Rückfall |

Gemessen im Host-Lauf (`logs/run_colour2.ps1`, `--screenevery 400` schreibt die **präsentierten** Frames nach
`run_host\frames\w_XXXX.bmp`):

```
[dr3] present: src 320x240 pitch 320 -> dst 1024x768 ...      (der LUT-Pfad ist aktiv)
[dr3] chaos: cars: 1592 pixel(s) marked, 1048 coloured, 1048 of the marks on the ramp 0x0f..0x18,
      ramp pixels on screen 1048, most common marked value 0x04 ... (game row = src row - 20)
[dr3] 37 presents/s (texture 320x240, SDLms=50395)             (vorher: ~23 presents/s im Rennen)
[host] screen dump frames\w_0101.bmp (320x240) - what the PC shows
```

Im Menü/Intro waren es vorher ~7 presents/s, jetzt 46-96. `logs/view_colour_race.png` und
`logs/view_colour_race2.png` sind solche Dumps: das erste zeigt die zwanzig Fahrer in zwanzig Farben, das
zweite das vollständige Bild (Strecke, HUD, Autos) nach dem Paletten-Fix (`dr3_pal` wird jetzt auch auf dem
PC gefüllt, nicht mehr nur hinter `__3DS__`).

Zwei Dinge, die dieser Pfad **nicht** ändert: der Rand des Spiels (`DR3_LETTERBOX`) und die Tint-Zuordnung,
die die **Nearest-Nachbarschaft** von `dr3_blit8_lut32()` annimmt (der gefilterte 640x480-Pfad des Frontends
hat keine Autos und wird nicht getintet). Und: nur ein Present des Renn-Screens darf die Maske benutzen -
sie gehört zum Bild, das `race_main.c` aus dem BACKBUFFER kopiert.

### Die Lebensbalken ueber den Autos

Sie haengen am selben Durchgang (`dr3_carcolour_tint_cars`, `platform_3ds/dr3_blit.c`): ein Balken
erscheint nur, solange ein Auto **beschaedigt** ist - unter 90 % der HP, mit denen es ins Rennen ging
(`DR3_CHAOS_HP_LINE_BELOW` in `platform_3ds/dr3_blit.h`), und ueber 0 %; ein unberuehrtes Auto und ein
Wrack bekommen keinen.  Massstab ist die HP, die das Auto beim **ersten Bild** des Rennens hatte - ein per
`hp <auto> <wert>` abgeschwaechtes Auto ist damit "ganz" und bekommt erst dann einen Balken, wenn es im
Rennen weiteren Schaden nimmt.

Der **eigene Wagen** bekommt nie einen Balken, egal wie wenig HP er hat: der Balken ist dafuer da, die
*anderen* Autos zu lesen (wer gleich hin ist), und den eigenen Schaden zeigt die DAMAGE-Anzeige der eigenen
HUD (`s_54[n].__10`, `race___400ech.c` - in `logs/owncar_w0070.png` zu sehen), dazu der Rauch und das Feuer
am Auto und die Warnung, die der Motor bei wenig HP abspielt (`DamageWarning`, `race___40864h.c`).  Welcher
Wagen der eigene ist, sagt `MY_CAR_IDX`
(`race_main.c`; `dr3_own_car()` in `dr3_blit.c` liest ihn wie
`dr3_bottom.c` und begrenzt ihn auf 0..19).

Das Log sagt pro Auto, was entschieden wurde: die ganze Tabelle fuer die ersten Bilder eines Rennens und
danach jede Aenderung (ein Auto wird getroffen, gewrackt oder von einem Pickup repariert).  Ein ganzes Auto
steht als "off" da, ein beschaedigtes als "shown", ein Wrack als "wrecked" und der eigene Wagen als "own" -
nur das beschaedigte bekommt einen Balken:

```text
[dr3] chaos: hp line: car  3 hp  51200/ 51200 (100%) off      <- ganz (gemessen an dem, womit es startete)
[dr3] chaos: hp line: car  3 hp  39936/ 51200 ( 78%) shown    <- beschaedigt: der Balken ist da
[dr3] chaos: hp line: car  3 hp      0/ 51200 (  0%) wrecked  <- Wrack: kein Balken
[dr3] chaos: hp line: car  3 hp  20000/ 51200 ( 39%) own      <- der eigene Wagen: kein Balken
```

`logs/run_hpline1.ps1` (kurz, dazu ein Wrack per `--hp 5 0`), `logs/run_hpline2.ps1` (lang, die Autos
nehmen im Rennen Schaden) und `logs/run_hpline3.ps1` (zehn Autos starten abgeschwaecht: ihre Balken
erscheinen, sobald sie getroffen werden, und verschwinden wieder, wenn sie gewrackt sind) sind die Laeufe
dazu, `--screenevery` liefert zusaetzlich das gezeigte Bild.  `logs/run_hpline4.ps1` ist derselbe
abgeschwaechte Lauf mit `--screenevery`, und die Bilder daraus zeigen es: `logs/hpline_whole_cars_w0020.png`
(vier unberuehrte Autos, kein Balken - vorher lag ueber jedem ein gruener Streifen) und
`logs/hpline_damaged_cars_w0042.png` (zwei getroffene Autos, je ein gruener Balken).

Den eigenen Wagen prueft `logs/run_hpline6.ps1` (ein "Destroy 'em all"): er faehrt im Direktstart Wagen 3
(`field`-Zeilen: `<- the player`), der Lauf zieht ihn per `--hp 3 30000` auf 59 % - und Wagen 0 zum
Vergleich genauso.  Das Log liest dann fuer ihn **immer nur** `hp line: car  3 ... own` (im Lauf: 8
`own`-Zeilen, **keine** `shown`-Zeile), waehrend andere Wagen sehr wohl `shown` melden
(`car 14 hp 92029/102400 ( 89%) shown`) und die `field`-Zeilen zeigen, dass seine HP wirklich fallen (bis
auf 0: er wird gewrackt).  Die Bilder aus dem Lauf zeigen es: `logs/owncar_w0070.png` (der eigene Wagen
beschaedigt unterwegs, kein Balken ueber ihm - waehrend die DAMAGE-Anzeige der eigenen HUD 70 % zeigt) und
`logs/owncar_w0085.png` (der eigene Wagen brennend und gewrackt, kein Balken, HUD 100 %).

### Der Direktstart


`quickrace` macht genau das, was der Signup-Screen (`___3266ch.c`) vor `___33010h_cdecl(4)` tut:

1. die Chaos-Difficulty setzen (`___196a94h_difficulty`, `dr3_chaos_wanted` - das macht sonst der
   Lizenzschirm, `___3ab5ch.c`);
2. das Event aufsetzen (`___31588h()`: drei Strecken, leeres Feld);
3. das Feld der drei Rennen füllen - mit der Regel des Signup-Pickers (`___3079ch.c`): jeder Fahrer, der
   noch nicht im Event ist, kommt in das erste Rennen mit freiem Platz. Der Picker **selbst** wird dafür
   nicht gerufen: er wählt sein Rennen mit `rand()%3` in einer `while(1)` **ohne Zähler** und zeichnet für
   jedes Auto den ganzen 640x480-Bildschirm, im Host saß er damit minutenlang fest (siehe unten);
4. den Spieler in sein Rennen setzen (Slot 0..3 des gewählten Rennens, `___1a103ch`);
5. `___33010h_cdecl(4)` - Rennaufbau und `race_main`.

**Er wartet auf das Menü.** `___33010h_cdecl()` kopiert die Prepare-Grafik aus Zeigern, die `menu_main()`
erst *nach* dem Vorspann füllt (`___2faf0h()`: `___1a0f8ch` = `prep4.bpk`, `___1a1114h` = der Decode-Puffer,
dazu die Gesichter in `___19de70h`). Vorher sind sie NULL und der Lauf stirbt in einem `strcpy`/`memcpy`
(`0xC0000005`, beobachtet in `VCRUNTIME140`) - **ohne eine Zeile im Log**. Genau das passiert mit
`-QuickRaceMs 8000`: das Intro des Spiels (`sanim.haf`, gestartet in `___3e720h.c`) dauert gut 80 s, der
Direktstart feuerte also mitten hinein. `dr3_host_menu_ready()` in `dr3_host.c` hält ihn jetzt zurück, bis
die Grafik geladen ist, und schreibt ins Log, dass er wartet: `quick race: holding - ...`. `-QuickRaceMs
8000` heißt damit "8 s nach dem Start **und** erst, wenn das Menü steht". Das Intro überspringt ein
`key 9000 return down` in `dr3_keys.txt` (§8.2) - ohne Taste wartet der Lauf die gut 80 s ab.

Es werden also nur die **Bildschirme** übersprungen, nicht die Logik. Ein Beispiel aus dem Log:

```
[host] quick race: player 19, slot 0 (class ranges 0/7/5/3)
[host] quick race: difficulty 4 (chaos on), 18 extra car(s) asked for
[host] quick race: track TR4 (byte 12), field 4/4/4 after 11 fill step(s), player is car 0
[dr3]  chaos: race setup with 18 extra car(s), ladder stage 3
[dr3]  chaos: grid of 4 cars (start rot 88, the four starters face 330 deg): 16 of 16 extras placed on the road
```

### Das Skript (`run_host\dr3_host.txt`)

Alles, was der Runner als Schalter hat, geht auch als Textdatei - mit Zeitangaben:

```
log 1                 # chaos_log.txt
timing 1              # chaos_time.txt (Frame-Zeiten)
extras 18             # chaos_extras.txt
stage 3               # Extras-Ausbaustufe (0 unsichtbar .. 3 alles)
pass 6                # einen Zeichen-Durchgang abschalten (Suche nach den grauen Linien)
frames 3000
quickrace_ms 12000
track 4               # Strecke festnageln (0..17, -1 = wuerfeln)
slot 0                # welches der drei Rennen
laps 1                # Renndauer in Runden (0 = die der Strecke) - kurz bis zum Report
key 9000 return down  # eine Taste zu einer Zeit
key 9180 return up
keys dr3_keys.txt     # eine ganze Route aus einer Datei
dumpevery 1500 frames\f_%04d.bmp
```

`run_host\dr3_keys.txt` ist eine Route: `"<ms> <key> down|up"`, eine Zeile pro Tastendruck. Die Namen sind
die SDL-Namen (`return`, `escape`, `up`, `down`, `left`, `right`, `kp_enter`, `space`, `a`..`z`, `f1`..`f12`).
Ein Tastendruck sollte **100-200 ms** gehalten werden: die Menüs dieses Spiels lesen die gehaltene Taste in
eigenen Schleifen, ein zu kurzer Puls kann zwischen zwei Abfragen fallen.

## 3. Anschauen

```powershell
powershell -ExecutionPolicy Bypass -File scripts\3ds\host_shots.ps1 -Dir run_host\frames   # BMP -> PNG
```

Der Dump ist der Bildschirm **der Engine** (ihr eigener 8-Bit-Puffer), nicht das, was die 3DS-Portierung
daraus macht. 320x240 heißt VGA13 (Rennen, Intro), 640x480 heißt VESA101 (Menüs, Ladebildschirme).

```powershell
# ein Rechteck ausschneiden und mit Nearest Neighbour vergroessern (HUD, Panel, Autos ansehen)
powershell -ExecutionPolicy Bypass -File logs\zoom_panel.ps1 -In run_host\frames\f_0022.png `
           -Out logs\panel.png -X 0 -Y 96 -W 72 -H 110 -Zoom 6
```

### Welche Farbschiene ein Bild benutzt (`logs/rampmap.py`)

Der Dump traegt die Palette der Engine mit. `logs/rampmap.py` zeigt fuer ein Rechteck, zu welcher der vier
Auto-Farbschienen (`0x0f..0x18`, `0x19..0x22`, `0x23..0x2c`, `0x2d..0x36` - `___4a064h_cdecl(10*n+15, ...)`)
jeder Pixel gehoert, und druckt einzelne Paletteneintraege:

```powershell
python logs/rampmap.py run_host/frames/f_0022.bmp -x 0 -y 104 -w 64 -hh 96 --rampcount `
       --palette 0x0f 0x19 0x23 0x2d
```

Damit sind die Namensleisten des linken Panels zuzuordnen: die Reihe mit dem Namen eines Autos nutzt die
Farbschiene *dieses* Autos, und die Zeilen sind die Autos 0..3 ohne den Spieler (`race___405bch.c`).
`logs/bmp_probe.ps1` druckt dieselben Pixel als Indizes, und `logs/pink_before.py` legt einen
Palettenwert der Strecke wieder in einen Dump hinein, um "vorher/nachher" zu vergleichen (§6, die pinken
Namensleisten).

## 4. Auswerten

```powershell
powershell -ExecutionPolicy Bypass -File scripts\3ds\report_host.ps1
powershell -ExecutionPolicy Bypass -File scripts\3ds\report_host.ps1 -All       # alles ausgeben
```

Der Bericht holt aus dem Log: Harness-Zeilen, Rennaufbau, Grid (jeder Platz mit Name/Standings/Winkel),
Watchdog- und Band-Messung (die grauen Linien), Pass-Suche, Frame-Zeiten und Heap-Meldungen.

Live im Log steht außerdem jede Sekunde eine Zeile (`frame N at T ms ... frame/s`), damit ein hängender
Lauf sofort sichtbar ist.

## 5. Was der Harness nicht kann

* **Den Rennstart im Host zu Ende bringen** - *erledigt (26. September 2026)*. Es waren zwei Abstürze
  hintereinander, beide ohne eine Zeile im Log:
  1. Der Direktstart feuerte mitten im Intro, bevor `___2faf0h()` die Prepare-Grafik geladen hatte
     (`0xC0000005` in `VCRUNTIME140`, §2). Behoben mit dem Halten auf das Menü
     (`dr3_host_menu_ready()`).
  2. In der ersten Rennframe las `race_main.c` in den Chaos-Diagnosen `D(MY_CAR_IDX)`. Die Datei
     deklariert die Größe aber als **Wert** (`extern __DWORD__ MY_CAR_IDX;`), und `D()` ist
     Adress-Semantik (`drally.h`) - der Lauf las also die Speicherstelle 3 (`invalid read of 0x3`, in der
     ersten Rennframe, `race_main.c:939`). Behoben: die Chaos-Diagnosen benutzen `MY_CAR_IDX` direkt, so
     wie der Rest der Datei. (In `race___3f970h.c`/`race___405bch.c` ist dieselbe Größe als *Array*
     deklariert, dort ist `D()` richtig - die Deklaration in *derselben* Datei entscheidet, §8.5.)

  Ein Lauf mit 20 Autos fährt jetzt Rennen bis zum Frame-Limit durch (§6). Was davon bleibt: der
  Direktstart ist **empfindlich gegen den Startzeitpunkt**, und ein Absturz war unsichtbar - deshalb
  meldet der Harness einen Absturz jetzt selbst (§8.5).
* **Die 3DS-Bildausgabe**: `platform_3ds/dr3_fb.c` (Framebuffer, Tint, Zweitbildschirm) ist libctru und
  wird nicht gebaut - das bleibt ein Azahar-Test.
* **3DS-Zeitverhalten**: die Frame-Zeiten des Harness sind PC-Zeiten, nur die Struktur (was wie oft
  passiert) überträgt sich.
* **Die Listen des Sign-up-Screens**: der `quickrace`-Direktstart baut den Roster ("PREPARE TO RACE") auf, den
  der Screen danach zeigt - den Sign-up-Screen (`___3266ch.c`) selbst erreicht er nicht (er setzt das Feld mit
  seiner eigenen Kopie der Picker-Regel auf, §8.5/§2), und der Picker (`___3079ch.c`) zeichnet dabei nichts. Die
  Zeilen, die der Screen in einem Chaos-Rennen *nicht* mehr schreibt (`dr3_signup_names_shown`), sieht man also
  nur im Spiel; der Log sagt mit
  `[dr3] chaos: sign up screen: the three lists stay unnamed (the race names the drivers)` wenigstens, dass der
  Schirm sie beim naechsten Besuch weglaesst. Der **Roster** dagegen ist pruefbar (§8.6, letztes Rezept).
* Der Harness ersetzt **keinen** Test auf echter Hardware: der Haupt-Build auf dem 3DS bleibt die
  Referenz.

## 6. Was er schon gefunden hat

* `platform_3ds/dr3_grid.c`, `dr3_grid_place()`: ein Block las `bx, by, bh`, **bevor** `dr3_grid_snap()`
  und die Heading-Zeile sie gesetzt hatten (MSVC: C4700; GCC mit `-O2` sagt nichts). Er war wirkungslos -
  er schrieb nur `bh`, das eine Zeile später neu gesetzt wird - und ist entfernt.
* `platform_3ds/dr3_host.c`: `dr3_host_dump_add(0, atoi(argv[++i]), argv[++i])` - MSVC wertet Argumente von
  rechts nach links aus, dadurch kamen Zeit und Pfad vertauscht an (der Dump lief nur einmal).
* `___3079ch.c`: die Wahl des Rennens, das der Picker füllt, ist ein `while(1)` **ohne Zähler**
  (`ebp = rand_watcom106()%3; if (B(___1a1f64h+ebp+3) <= 3) break;`). Landet der Zufall immer wieder auf
  einem vollen Rennen, endet die Schleife nie. Im Host ist genau das passiert (der Direktstart stand
  minutenlang in Schritt 2). Auf der 3DS trat es bisher nicht auf, aber die Stelle hat keinen Schutz -
  die Kandidatenschleife darunter hat einen (`dr3_tries`, 4000).
* `scripts/3ds/report_host.ps1`: die Abschnitte mit `[host]`/`[dr3]` fanden **nichts** - PowerShells `-like`
  liest `[host]` als Zeichenklasse ("ein Zeichen aus h,o,s,t"). Der Bericht lief also durch, zeigte aber
  0 Treffer in *Harness*, *Boot/Modus*, *Renn-Setup* und *Grid/Aufstellung*, während die Muster ohne
  Klammern (`*pass *`, `*heap*`) trafen - genau das hat den Fehler verdeckt. Jetzt escapet die Suche in
  `Section` `[`/`]`. Wer Logmarken sonst per `-like` sucht, muss dasselbe tun (§8.5).
* `platform_3ds/dr3_host.c`: der Direktstart lief **mitten im Intro** los - das Spiel zeigt beim Boot
  80 s lang `sanim.haf` (`___3e720h.c`), also bevor `menu_main()` in `___2faf0h()` die Prepare-Grafik lädt
  (`___1a0f8ch`, `___1a1114h`, die Gesichter in `___19de70h`). `___33010h_cdecl()` kopiert genau daraus und
  endete in einem `strcpy`/`memcpy` (`0xC0000005` in `VCRUNTIME140`), **ohne eine Zeile im Log**. Jetzt hält
  `dr3_host_menu_ready()` den Start zurück, bis die Grafik da ist (§2).
* `race_main.c`: die Chaos-Diagnosen (`dr3_chaos_dump_due()`, `dr3_chaos_player_pos()`,
  `dr3_chaos_dump_frame()`) lasen `D(MY_CAR_IDX)`. Die Datei deklariert `MY_CAR_IDX` als **Wert**
  (`extern __DWORD__ MY_CAR_IDX;`), `D()` ist aber Adress-Semantik (`drally.h`) - die erste Rennframe las
  damit die Speicherstelle **3** und der Prozess endete mit `0xC0000005` (`invalid read of 0x3`,
  `race_main.c:939`). Gefunden hat das der Crash-Report des Harness (§8.5), behoben sind die zehn Stellen
  (`MY_CAR_IDX` direkt, wie im Rest der Datei). Achtung: `DR3_CHAOS` ist in **jedem** 3DS-Build an
  (`Makefile.3ds`), die Stelle wird also mitgebaut - auf dem 3DS läuft sie nur, wenn das Log
  eingeschaltet ist (`dr3_chaos_dump_due()` fragt zuerst `dr3_log_enabled()`), im Harness ist es das immer.
  Ein Repro-Hash (`logs/build_repro.sh`) **muss** sich durch diesen Fix ändern.
* `scripts/3ds/report_host.ps1`: das Escaping der Klammern lief **pro Zeile** (in `Like()`). Mit einem
  Rennlog von über 12.000 Zeilen dauerte der Bericht dadurch länger als eine Minute; jetzt wird jedes Muster
  einmal escaped, bevor die Zeilen durchlaufen werden.
* **`race___4dcach.c`: der Killbonus stürzte beim ersten Aufruf ab** (*erledigt, 26. September 2026*).
  Die Datei deklarierte `___1a1ef8h` als **Zeiger** (`extern __BYTE__ * ___1a1ef8h;`), in `bss.c` ist es
  aber `__BYTE__ ___1a1ef8h[4]` - jedes andere File liest es als Array. `D(___1a1ef8h)` las damit nicht den
  Platz des Spielers, sondern **dereferenzierte den Platzwert (19) als Adresse**: `invalid read of 0x13` in
  `dr3_chaos_kill_bonus`, aufgerufen aus der Maschinenpistole (`race___4ff50h.c:131`), mitten im Rennen -
  der Lauf endete dort. Gefunden hat das erst der Lauf, der extra für den Killbonus aufgesetzt war: vorher
  hatte in keinem Harness-Lauf je ein Gegner gewrackt, die Stelle war also nie ausgeführt worden. Warum der
  Autopilot trotzdem *nicht* reicht, den Bonus zu sehen: der Spieler fährt das schwächste Auto des Feldes
  und startet als Rang 20 auf dem letzten Platz - siehe §2. Dieselbe Falle wie beim `D(MY_CAR_IDX)` in
  `race_main.c` (§5): die Deklaration in *derselben* Datei entscheidet, was `D()` liest.
* **Die pinken Namensleisten des linken Panels** (*erledigt, 27. September 2026*).  Der Balken hinter einem
  Fahrernamen ist Teil des Panel-Bildes dieser Zeile und wird mit der *Farbschiene dieses Autos* gezeichnet
  (`___4a064h_cdecl`, `10*n+15`); die Zeilen sind die Autos 0..3 ohne den Spieler (`race___405bch.c`).
  Der Chaos-Build baute nur Schiene 0 ("eine Schiene reicht - die Portierung faerbt die Sprites ohnehin"),
  also zeigten die anderen Zeilen, was die *Strecken*-Palette bei `0x19..0x36` stehen hat - auf TR6 ist das
  `0x3f003f` (Magenta), und genau das waren die pinken Balken in der Meldung.  Der Harness hat das ohne
  Emulator sichtbar gemacht: `logs/rampmap.py` zeigt, dass Reihe 1 Schiene 0, Reihe 2 Schiene 1 und Reihe 3
  Schiene 2 benutzt, und der Palettendump eines alten TR6-Laufs (`logs/host_run_auto_tr6.log`) nannte fuer
  die Schienen 1..3 genau `3f003f .. 3f003f`.  Nach der Korrektur (`race___4a25ch.c` baut alle vier
  Schienen aus den Farben der Renneintraege, `logs/run_pinkbars2.ps1`) nennen dieselben Zeilen die
  Fahrerfarben - `chaos: ramp 1: 040000 .. 3c3333` ist die Schiene der Farbe `48,3,3` des Fahrers
  (`0.1*48 = 4` dunkel, `12.6*4 + 0.2*48 = 60` hell).  Bilder: `logs/fixed_panel_f0022.png` (Balken in
  Fahrerfarben) gegen `logs/pink_before_zoom.png` (derselbe Frame mit dem Magenta der Strecke,
  `logs/pink_before.py`).

## 7. Die Neutralität des 3DS-Hauptbuilds

Alles, was der Harness braucht, steht hinter `#if defined(DR3_HOST)` - der 3DS-Build kennt das Define
nicht. Zwei Stellen sind trotzdem zu prüfen, wenn sich am Harness etwas ändert:

* `platform_3ds/dr3_log.c`: die Pfade (`sdmc:/...` gegen den Arbeitsordner) und der Takt (`svcGetSystemTick`
  gegen `SDL_GetPerformanceCounter`) stehen in `#if/#else`-Zweigen; der `#else`-Zweig ist der 3DS-Text.
* `drally_linux_c.c`: die Present-Weichen sind `#if defined(__3DS__) && !defined(DR3_HOST)`, damit der Host
  den SDL-Renderer nimmt und der 3DS seinen Framebuffer-Pfad.

Kontrolle:

```bash
# devkitPro MSYS2
bash logs/build_release_bg.sh          # baut den Hauptbuild, logs/build_3ds.log
md5sum build/3ds/dRally_3ds.3dsx       # muss dem Stand vor der Änderung entsprechen
```

Geprüft (26.09.2026): der Hauptbuild wurde **reproduzierbar** zweimal gebaut - einmal mit dem alten,
fehlerhaften Block in `dr3_grid_place()` und einmal ohne ihn, sonst identisch:

```
bash logs/build_repro.sh withgrid      # b4ffe9e87fe7e1589e4e187f2ae06268
bash logs/build_repro.sh nogrid        # b4ffe9e87fe7e1589e4e187f2ae06268   <- gleich
```

Dasselbe Ergebnis heißt zweierlei:

* der Harness (alle `DR3_HOST`-Zweige) kostet den 3DS-Hauptbuild **kein Byte** - er ist nicht enthalten;
* GCC hat den fehlerhaften Block wegoptimiert, er war also wirklich toter Code. Dass ein einfacher
  `md5sum`-Vergleich vorher eine Änderung zeigte, lag an `__DATE__`/`__TIME__` (`race___42824h.c:394`
  schreibt den Build-Zeitpunkt ins Log) - deshalb der zweite Build mit fixem `SOURCE_DATE_EPOCH`.

## 8. Für Agenten: eine Änderung bauen und testen

Diese Reihenfolge hat sich bewährt, wenn an der Engine, am Chaos-Teil oder an der Portierung etwas geändert
wurde. Der Harness ersetzt den 3DS-Test nicht - er beantwortet die Fragen, die dort eine Emulator-Sitzung
**pro Versuch** kosten.

### 8.1 Wann was

| Frage | Werkzeug |
|---|---|
| Was zeichnet die Engine hier (Grid, Extras, Farben)? | Harness, `-DumpEvery` + `host_shots.ps1` |
| Was macht der neue Code bei 20 Autos? | Harness, `-Extras 18` |
| Hält ein Rennen N Frames durch (Heap, Grid, Standings)? | Harness, `-Frames`, Log greppen |
| Wie kommt es auf der 3DS an (Framebuffer, Tint, Zweitbildschirm, Takt)? | nur Azahar (`scripts/3ds/deploy_emulator.ps1`) |
| Ist der 3DS-Hauptbuild unverändert? | `logs/build_repro.sh` + `md5sum` (§7) |
| Rechnet der portable Teil noch richtig? | `powershell -File tests\build_tests.ps1` |

### 8.2 Der Ablauf

**1. Bauen** - nach jeder Änderung an `platform_3ds/` oder an einer Engine-Datei:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\3ds\gen_host_project.ps1 -Build
```

Warnungen sind hier Befunde, keine Nebengeräusche: `C4700` hat schon einen echten Fehler gefunden (§6).

**2. Fahren** - mit Frame-Limit, damit der Lauf von selbst endet:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\3ds\run_host.ps1 `
           -Extras 18 -QuickRaceMs 15000 -Frames 2500 -DumpEvery 8000 -TimeoutSec 150
```

Der Lauf muss das Intro überspringen, sonst wartet der Direktstart gut 80 s (§2): die Vorlage von
`run_host\dr3_keys.txt` (`scripts\3ds\host\dr3_keys.txt`) schickt dafür `9000 return down`. `-QuickRaceMs`
ist die Zeit ab dem Start - der Direktstart wartet zusätzlich, bis das Menü steht. Beispiel aus dem
26.09.2026 (20 Autos auf der Strecke): 2500 Frames in 50,8 s (49,3 Frame/s), 5 Dumps, Rennen bis zum
Frame-Limit, kein Absturz.

`-TimeoutSec` ist die Notbremse; ist sie zu klein, sieht ein langsamer Lauf wie ein Hänger aus (der Runner
schreibt dann `TIMEOUT nach ... s`). Ergebnisse: `run_host\drally_host.log`, `logs\host_run.out`.

**3. Anschauen** - was die Engine wirklich gezeichnet hat:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\3ds\host_shots.ps1 -Dir run_host\frames
```

**4. Auswerten**:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\3ds\report_host.ps1            # Übersicht
powershell -ExecutionPolicy Bypass -File scripts\3ds\report_host.ps1 -All      # alles
```

**5. Neutralität prüfen** - der 3DS-Hauptbuild darf sich **nicht** geändert haben (§7):

```bash
# devkitPro MSYS2
bash logs/build_release_bg.sh     # Hauptbuild -> logs/build_3ds.log (md5sum steht am Ende des Logs)
bash logs/build_repro.sh vorher   # derselbe Build mit fixem SOURCE_DATE_EPOCH -> logs/build_repro_vorher.log
#  ... Aenderung ...
bash logs/build_repro.sh nachher  # -> logs/build_repro_nachher.log, md5 vergleichen
```

`build_release_bg.sh` setzt `SOURCE_DATE_EPOCH` **nicht** - sein Hash ist bei jedem Lauf anders (siehe
`__DATE__` in §8.5). Reproduzierbar und damit vergleichbar ist nur `build_repro.sh` (es setzt die Variable
und `touch`t die Dateien, die am Port beteiligt sind).

Vorher/nachher vergleichen. Gleich = die Änderung ist 3DS-neutral (oder GCC optimiert sie weg). **Pflicht:**
alles, was nur der Harness braucht, steht hinter `#if defined(DR3_HOST)` - ohne Guard ist es im Hauptbuild,
und der Vergleich schlägt dann zu Recht fehl.

### 8.3 Was im Log steht (zum Greppen)

| Marke | sagt |
|---|---|
| `[host] harness: ...` | mit welchen Schaltern der Lauf startete |
| `[host] script ...: N line(s) - frames ...` | was im Skript stand |
| `[host] quick race: ...` | jeder Schritt des Direktstarts (Spieler, Feld, Strecke) |
| `[host] frame N at T ms (... frame/s)` | Sekundentakt - ein hängender Lauf ist sofort sichtbar |
| `[host] reached N presented frame(s)` | der Lauf hat sich selbst beendet |
| `[dr3] CHAOS build: N cars on the grid (...)` | diese Binary hat die Chaos-Difficulty (steht einmal pro Rennen) |
| `[dr3] chaos: race setup with N extra car(s), ladder stage S` | die Konfiguration des Rennens |
| `[dr3] chaos: grid of N cars ... X of Y extras placed on the road` | das Grid auf einen Blick |
| `[dr3] chaos: duel: rank N has M driver(s) within K rank(s)` | das Auswahlfenster des Duells: wen der Rivale sein konnte (M = 0 heisst: der naechste Fahrer der Tabelle) |
| `[dr3] chaos: grid place NN: NAME (standings SS) stands ...` | jeder Platz einzeln |
| `[dr3] chaos: field of N cars: ... pair(s) inside the 40 px contact box` | Zustand der Aufstellung |
| `[dr3] chaos: WATCH <wer> wrote .. at x,y (car sprite top left X,Y)` | wer in die Zeilen über dem Auto schreibt |
| `[dr3] chaos: BAND <pass> wrote N px over the car (...)` | welcher Durchgang die grauen Linien schreibt |
| `[dr3] chaos: PASS SWITCH: ...` | welcher Durchgang gerade abgeschaltet ist (3DS: L+R+X) |
| `[dr3] chaos: EXTRA CAR STAGE d/3 - ...` | die Ausbaustufe der Extras |
| `[dr3] chaos: chaos_extras.txt says N extra car(s), was M` | der Extras-Schalter hat sich geändert (steht nur bei Änderung im Log) |
| `[dr3] chaos: chaos_dm.txt: the sudden death ... starts after N second(s), 1 percent per second (1024 HP per car and second)` | die Sudden Death eines „Destroy 'em all" hat sich geändert (steht nur bei Änderung im Log, `dr3_dm.c`) |
| `[dr3] chaos: chaos_dm_lap.txt: the lap duty ... asks for 1 lap(s) every N second(s), ... loses 25 percent` | die Lap-Pflicht hat sich geändert (nur bei Änderung im Log) |
| `[dr3] chaos: deathmatch: a car has to finish 1 lap(s) every N second(s) or it loses 25 percent ...` | was der Rennaufbau für dieses Rennen eingestellt hat (`___33010h.c`) |
| `[dr3] chaos: deathmatch: lap check N: car C finished no lap since the last one - 25 percent of its HP (25600 HP), it is now at H HP` | die Strafe der Lap-Pflicht, je Auto und Check (der stehende Wagen verliert genau 25600) |
| `[dr3] chaos: deathmatch: the sudden death starts after N second(s), ...` | was der Rennaufbau für dieses Rennen eingestellt hat (`___33010h.c`) |
| `[dr3] chaos: deathmatch: sudden death starts Ns into the race - ... car(s) still hold HP` | die Sudden Death läuft ab jetzt (`platform_3ds/dr3_dm.h`) |
| `[dr3] chaos: deathmatch: sudden death after Ns: X percent taken, Y of Z car(s) left, the player's car at H HP` | der Abzug, alle zehn Prozent: `X` wächst um eins je Sekunde und `H` fällt je Auto um 1024 HP |
| `[dr3] chaos: deathmatch: the 'SUDDEN DEATH' banner is drawn (...)` | der Banner ist im gezeigten Bild (einmal je Lauf) |
| `[dr3] chaos: deathmatch: sudden death decided the race - N car(s) left at Ns, the wind down ... starts` | nur noch ein Auto hat HP: der Port startet das Rennende (§8.6, `logs/run_dm7.ps1`) |
| `[dr3] chaos: deathmatch: race over with N of M car(s) still running` / `the winner is car N` / `the player is the last car standing ... -> pot $N` | das Ende und die Wertung des Modus |
| `[dr3] chaos: hp line: car N hp X/Y (Z%) over` | der RACE-OVER-Screen läuft (`___243d14h != 0`): diese Health-Linie ist versteckt, dazu der Banner |
| `[dr3] chaos: hp line: the race is over (___243d14h != 0) - N health line(s) ... are hidden from here on` | einmal, sobald das Rennen vorbei ist (der Beweis, dass der Port die Bars wegnimmt) |
| `[dr3] chaos: results rows: 1. NAME (racer N), 2. ... out of M car(s)` | was die vier Zeilen der Ergebnis-Tafel des Rennens zeichnen (§2, `logs/run_results1.ps1`) |

| `[dr3] chaos: EXTRA CARS n (...)` | nur wenn der Schalter im Spiel betätigt wird (derzeit kein Aufrufer, §8.5) |
| `[dr3] chaos step <name> n` | "der Motor läuft noch" - die letzte Zeile vor einem Hänger |
| `[dr3] chaos frame time: ...` / `frame detail: ...` | das Frame-Budget (Sekundentakt) |
| `[host] crash: exception ... / invalid read of ... / fault <fn> (datei:zeile)` | ein Absturz **mit** Stelle statt eines abgebrochenen Logs (§8.5) |

Das alles steht hinter `chaos_log.txt = 1` (`run_host.ps1` setzt das; die teuren Diagnosen - Watchdog,
Band, Dumps, Step-Trace - laufen nur dann). Nur Frame-Zeiten, ohne Diagnose: `chaos_log.txt = 0`,
`chaos_time.txt = 1`.

### 8.4 Eigene Diagnose einbauen

* Ausgabe mit `dr3_log("[dr3] chaos: <was> %d", x)` (`platform_3ds/dr3_log.h`). Das Präfix `[dr3] chaos:`
  macht die Zeile auffindbar: im Host-Log für `report_host.ps1`, im Emulator-Log (`sdmc:/drally_3ds.log`)
  für `logs\analyse_*.ps1`.
* Teure Ausgabe (pro Pixel, pro Auto, pro Frame) an `dr3_log_enabled()` koppeln - sonst kostet der Lauf
  den Faktor, den man gerade messen will. Feine Netze gibt es schon: `dr3_chaos_watch()`,
  `dr3_chaos_band_mark()`, `dr3_chaos_trace()`.
* Im Hauptbuild kostet `dr3_log()` nichts (No-ops in `dr3_log.h`), die Aufrufer brauchen keinen Guard -
  eigene **Datenstrukturen** hinter dem Diagnose-Schalter brauchen ihn aber doch (siehe `race___42824h.c`).
* Neuer Harness-Schalter: eine `strcmp(cmd, "...")`-Zeile in der Skript-Schleife (`dr3_host.c`), ein Feld in
  der `H`-Struktur (`platform_3ds/dr3_host.c`: `static struct { ... } H;`) und - wenn er von außen kommen
  soll - ein Parameter in `run_host.ps1`. Kommandozeilen-Schalter werden in der `argv`-Schleife derselben
  Datei ergänzt.
* Neuen Skript-Befehl sofort in Abschnitt 2 (`### Das Skript`) nachtragen.

### 8.5 Fallen (alle hier schon passiert)

* **MSVC wertet Argumente von rechts nach links aus**: `f(atoi(argv[++i]), argv[++i])` kommt vertauscht an.
* **MSVC meldet nicht initialisierte Variablen (`C4700`), GCC `-O2` schweigt** - und optimiert den Block
  weg. Ein Fehler, den nur der Host-Build sieht, ist trotzdem ein Fehler (§6).
* **`__DATE__`/`__TIME__` stecken im Binary** (`race___42824h.c:394`): ein nackter `md5sum`-Vergleich
  sagt nichts, `SOURCE_DATE_EPOCH` setzen (`logs/build_repro.sh`).
* **`___3079ch_cdecl()` nicht in einer Schleife rufen**, um das Feld zu füllen - seine Rennwahl ist ein
  `while(1)` ohne Zähler und zeichnet pro Auto den ganzen 640x480-Bildschirm (§5/§6). Der Direktstart füllt
  das Feld selbst.
* **Tastendrücke 100-200 ms halten** - die Menüs lesen die gehaltene Taste in eigenen Schleifen.
* **Immer `-Frames` setzen**: ohne Limit endet der Lauf erst mit dem Timeout und der Log ist abgeschnitten.
  Das Chaos-Log flusht alle 32 Zeilen, ein Abbruch verliert also höchstens 31.
* **`run_host\frames\*` vorher leeren**, sonst liegen Dumps alter Läufe dazwischen.
* **Der Direktstart braucht das Menü** (§2): Ist `-QuickRaceMs` kleiner als das Intro (~80 s) und
  überspringt keine Taste das Intro, hält der Harness ihn zurück (`quick race: holding - ...`) - der Lauf
  dauert dann entsprechend. Seit dem Fix ist "keine Rennframes" ein echter Befund über die Änderung.
* **Ein Absturz steht jetzt im Log.** Der Harness setzt auf Windows einen `SetUnhandledExceptionFilter`
  (`dr3_host_crash_filter` in `dr3_host.c`) und schreibt Ausnahme, die berührte Adresse (`invalid read of
  0x0` = NULL-Zeiger) und - mit der PDB neben der exe, die `run_host.ps1` mitkopiert - Funktion, **Datei und
  Zeile** der Absturzstelle samt Aufrufern. Fehlt die PDB, steht nur die Adresse; dann hat die
  Windows-Ereignisanzeige den Fehler-Offset:
  `Get-WinEvent -LogName Application | Where-Object { $_.Id -eq 1000 }` (Modulname + `Fehleroffset`, also
  die RVA in der exe).
* **`D(x)` ist Adress-Semantik** (`*(__DWORD__*)(x)`, `drally.h`). `D(MY_CAR_IDX)` ist nur richtig, wenn die
  Datei die Größe als **Array** deklariert (`extern __BYTE__ MY_CAR_IDX[];`). Bei einem Wert
  (`extern __DWORD__ MY_CAR_IDX;`) liest es den Wert als Adresse - im Fehlerfall die Stelle 3. Vor jeder
  `D()`-Nutzung die Deklaration in *derselben* Datei ansehen (§5, §6).
* **`pass` und `stage` sind nur Skript-Befehle.** Es gibt kein `--pass`/`--stage` auf der Kommandozeile
  (`dr3_host.c` kennt `--script`, `--keys`, `--frames`, `--quit_ms`, `--quickrace`, `--extras`, `--track`,
  `--slot`, `--autokey`, `--dumpevery`, `--dumpat`). Wer einen Durchgang abschalten will, braucht also eine
  Skriptdatei (§8.6).
* **Im Spiel schaltet `L + R + X` den Pass-Schalter**, nicht die Anzahl oder die Stufe der Extras:
  `dr3_input.c` ruft `dr3_chaos_skip_cycle()`. `dr3_chaos_extra_cars_toggle()` und
  `dr3_chaos_extra_stage_cycle()` haben derzeit **keinen** Aufrufer im Spiel - die Kommentare in
  `dr3_chaos.h` (Zeile 35-44 und 100-104) sagen noch das Gegenteil. Wer das ändern will, entscheidet es
  bewusst; die Wege, die es sicher gibt, sind `chaos_extras.txt` und das Skript (`extras`, `stage`).
* **`extra cars = 0` im Log ist kein Fehler.** `dr3_chaos_extra_cars()` gibt 0 zurück, solange die
  Chaos-Difficulty nicht aktiv ist (`dr3_log.c`: `dr3_chaos_active()`); die Startzeile `[host] harness: ...
  N extra car(s)` und die Antwort auf den Skript-Befehl `extras` stehen deshalb auf 0. Verlässlich ist der
  Wert erst nach Schritt 1/3 des Rennaufbaus (`quick race: difficulty 4 (chaos on), N extra car(s) asked
  for`), gefolgt von `[dr3] chaos: race setup with N extra car(s)`. Wer den Schalter prüfen will, liest
  diese Zeile - oder setzt die Zahl in `chaos_extras.txt` (`-Extras`), die bei **jedem** Rennsetup neu
  gelesen wird.
* **Marken in Klammern brauchen in PowerShell ein Häkchen.** `-like '[host]*'` sucht *ein Zeichen aus
  {h,o,s,t}*, nicht die Marke. Wer Skripte zur Auswertung schreibt, escaped `[`/`]` (so wie
  `scripts\3ds\report_host.ps1`, siehe §6).

### 8.6 Rezepte für die offenen Fragen

**Graue Linien über dem Auto.** Erst die `WATCH`- und `BAND`-Zeilen des Laufs sichten - im Host-Log macht
das der Bericht:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\3ds\report_host.ps1   # Abschnitt Watchdog/Band
powershell -ExecutionPolicy Bypass -File scripts\3ds\run_host.ps1 -Extras 18 -QuickRaceMs 12000 -Frames 1500
```

Die Intro-Taste aus der Vorlage (`9000 return down`) muss dabei sein, sonst setzt der Direktstart erst nach
dem ~80 s langen Vorspann ein (§2).

Für die 3DS-Seite macht `logs\analyse_band.ps1` dasselbe, aber aus dem **Azahar**-Log
(`%APPDATA%\Azahar\sdmc\drally_3ds.log`, Ausgabe `logs\band_report.txt`) - nicht aus `drally_host.log`. Dort
stehen die Instrumente ohnehin nur, wenn der Emulator-Lauf mit eingeschaltetem Log lief.

Verdächtige laut Kommentaren: `race___53464h` (Objekt-Schatten) und `race___4f170h` (Strecken-Schatten).
Am Harness ist die Suche billiger als am Emulator: **ein** Durchgang wird abgeschaltet. Dafür in
`run_host\dr3_host.txt` (Vorlage `scripts\3ds\host\dr3_host.txt`, §2) die Zeile setzen - `pass` ist ein
Skript-Befehl, kein Schalter von `run_host.ps1`:

```
pass 6            # der Durchgang aus dr3_log.c (dr3_pass_name); 0 = alle wieder an
```

Steht der Durchgang im Log, als die Linien weg waren, war er es (die Namen nennt `PASS SWITCH:`).

**Standings doppelt ("standings 15" zweimal).** Ausgangspunkt sind die
`[dr3] chaos: grid place NN: ... (standings SS)`-Zeilen: aufsteigend lesen, die doppelte `SS` benennt die
Autos. Dann in `___33010h.c` (`dr3_chaos_car_rank()`) und `dr3_grid.c` (`dr3_grid_order_by_rank()`) weiter.
Dieselben Autos stehen mit `[dr3] chaos: car N stripe ...` und in `field of ...` im selben Log.

**Grid/Aufstellung.** Abschnitt *Grid/Aufstellung* des Berichts lesen - jede Zeile hat Platz, Name,
Standings, Pixel entlang der Straße, Koordinate, Surface und Winkel. `X of Y extras placed on the road`
muss gleich sein; sonst steht der Grund in der Zeile darunter (`the walk stopped because: ...`).

**Endet ein „Destroy 'em all" jetzt?** `logs/run_dm7.ps1` ist der Prüflauf (§8.2, ein DM mit 20 Wagen):
Der Spieler fährt Wagen 3 und bleibt per `nohurt` am Leben, die anderen 19 starten mit 3000 HP (drei
Prozent - die Einheit des Spiels ist 1024 HP je Prozent), und `dmsudden 8` setzt die Sudden Death auf
8 Sekunden statt auf drei Minuten. Erwartet wird die Kette

```text
[host] deathmatch sudden death after 8 second(s) (0 = off)
[dr3] chaos: chaos_dm.txt: the sudden death of a deathmatch starts after 8 second(s), 1 percent per second (1024 HP per car and second), 0 switches it off
[dr3] chaos: deathmatch: the sudden death starts after 8 second(s), 1 percent of the HP of every car a second (1024 HP each)
[dr3] chaos: deathmatch: sudden death after 9s: 1 percent taken, 5 of 20 car(s) left, the player's car at 101376 HP
[dr3] chaos: deathmatch: the 'SUDDEN DEATH' banner is drawn (123 and 102 game pixel(s) wide, scale 3, row 18 of the 320x240 screen, race age 8 s)
[dr3] chaos: deathmatch: sudden death decided the race - 1 car(s) left at 11s, the wind down of the race starts (race_main.c)
[dr3] chaos: deathmatch: race over with 1 of 20 car(s) still running
[dr3] chaos: deathmatch: the player is the last car standing (car 3) -> pot $5000, kills $775
```

Der HP-Abzug ist die Zahl, an der die Regel hängt: `101376` ist `102400 - 1024`, also genau ein Prozent.
Die Bilder liegen in `logs/dm7_shots/` (der Banner steht über der Strecke, zwischen den beiden HUD-Spalten);
`logs/run_dm8.ps1` ist die Gegenprobe mit `dmsudden 0`: dort darf **keine** Sudden-Death-Zeile stehen, und
die Setup-Zeile meldet `0 second(s)`. Wer die *echten* drei Minuten prüfen will, lässt den Lauf mit dem
Standardwert laufen (`dmsudden 180`, ~16300 Frames für vier Minuten Rennzeit) - mit `field 800` im Skript
sieht man dann in den `field car N hp ...`-Zeilen jedes Auto fallen.

**Camping in der Ecke.** `logs/run_dm9.ps1` ist der Prüflauf für die Lap-Pflicht: `auto 0` (der Autopilot
fährt den Spieler nicht, ohne Eingaben steht sein Wagen ab Sekunde 0), `dmsudden 0` (die Sudden Death aus,
sonst überlagern sich beide Regeln) und `dmlap 60` (der Standard: eine Runde je Minute). Erwartet wird beim
ersten Check nach 60 s Rennzeit für den stehenden Wagen genau eine Strafe - und zwar

```text
[host] deathmatch lap duty: one lap every 60 second(s) (0 = off)
[dr3] chaos: deathmatch: a car has to finish 1 lap(s) every 60 second(s) or it loses 25 percent of its HP (25600 HP) at that check
[dr3] chaos: deathmatch: lap check 1: car 3 finished no lap since the last one - 25 percent of its HP (25600 HP), it is now at 74240 HP
```

`25600` ist `102400 / 4`, und `74240` ist genau die HP, die der Wagen vorher hatte, minus diese 25 Prozent -
die Autos, die ihre Runden gefahren sind, tauchen in keiner `lap check`-Zeile auf. Wer die Pflicht feiner
prüfen will, setzt `dmlap` kleiner (z.B. `dmlap 5`): dann verliert *jeder* 25 %, weil keine Runde in fünf
Sekunden zu fahren ist - der Mechanismus wird dabei genauso sichtbar, nur die Aussage „nur der Camper" nicht
mehr.

**Health-Bars über dem RACE-OVER-Screen.** Jeder Lauf, der ein Rennen beendet, zeigt es: sobald
`___243d14h != 0` ist, steht in den Zeilen der Autos `over` statt `shown`, dazu einmal
`hp line: the race is over (___243d14h != 0) - N health line(s) of the screen and the sudden death banner are
hidden from here on`, und in den Bildern ist über der schwarz werdenden Sequenz nichts mehr zu sehen. Der
Vorher-Zustand liegt als `logs/dm7_shots_before/w_0434.png` (derselbe Lauf vor der Änderung: der Banner
brannte noch auf dem Bildschirm, der gerade schwarz wird), der Nachher-Zustand in `logs/dm7_shots/`.

### 8.7 Wenn der Harness etwas nicht kann

Nicht am Harness vorbeibauen, sondern die Frage aufteilen: das Verhalten (Grid, Extras, Reihenfolge,
Standings) hier prüfen, die Ausgabe (Framebuffer, Tint, Zweitbildschirm, Geschwindigkeit) auf dem Emulator.
Ein Befund, der nur auf dem PC gemessen wurde, gilt nicht als 3DS-Befund - im Bericht dazuschreiben, wie er
entstanden ist.


## Der Sign-up-Screen selbst (`signup <ms>` / `--signup <ms>`)

Der Harness hat den Screen, in dem die Chaos-Modi angeboten werden und das Rad des Zufalls-Selectors dreht,
lange nie ausgefuehrt: `quickrace <ms>` baut das Feld selbst auf - "the way the signup screen does" - und
springt direkt ins Rennen. Damit war die *Auswahl* pruefbar (`spin <n>`), das *Rad* aber nicht - und genau
deshalb ist ein Fehler im Screen durch alle Pruefungen geschluepft.

`signup <ms>` ruft statt des Quick Race den echten `___3266ch()` aus dem Menue auf (dieselbe Stelle wie der
Quick Race, `platform_3ds/dr3_host.c`), sobald der Menue-Screen bereit ist. Die Frames, die der Screen dabei
praesentiert, gehen durch `dr3_host_present` und werden wie ueberall gedumpt (`dump` / `dumpevery`, das
Arbeitsverzeichnis des Harness ist `run_host/`, die Dump-Pfade sind relativ dazu).

Wichtig fuer den Modus: der Screen liest `chaos_mode.txt`, wenn er sich oeffnet. Ein `--mode <n>` auf der
Kommandozeile allein hilft nur, wenn es auch ankommt (`run_host.ps1` teilt `-ExtraArgs` an Leerzeichen, das
Skript des Harness kennt die Zeile `mode <n>`). Am verlaesslichsten ist die Zeile im Skript: die Vorlage liegt
in `scripts/3ds/host/dr3_host.txt`, eine vorhandene `run_host/dr3_host.txt` wird nicht ueberschrieben (nur mit
`-Stage`), und `mode <n>` schreibt den Wert in genau die Datei, die der Screen liest.

Prueflauf 24 (`logs/run_signup1.ps1`, Frames in `run_host/frames/signup1/`): `--signup 6000`,
`-DumpEvery 100`. Erwartet und gesehen:

    [host] signup: entering the real sign up screen (___3266ch) - the wheel lives there
    [dr3] chaos: chaos_mode.txt says mode 3 (RANDOM), was 3 (RANDOM)
    [dr3] chaos: sign up screen: the selector is armed (mode 3, RANDOM)
    [dr3] chaos: spin: landed on race 2 (0..2, the fights are there) and mode 2 (DM) - ENTER signs up

Der Fund, den erst dieser Lauf sichtbar gemacht hat: `dr3_chaos_mode()` maskierte den Modus auf `STANDARD`,
solange der Spielstand nicht auf der Chaos-Schwierigkeit stand. Die Datei sagte 3, der Screen sah 0 und drehte
nichts - auf der 3DS genauso wie hier. Das Rad haengt jetzt allein am Modus (`drally.h`), die Chaos-
Schwierigkeit bleibt das, was sie ist (das groessere Feld und die AI-Tabellen).

**Das Rad in zwei Runden (`logs/run_spin3.ps1`).** Seit dem 27. September 2026 zieht der Selector zweimal, und
beide Runden laufen ueber dieselben drei Kaesten des Screens. Runde 1 schreibt die drei Regelwerke als Karten in
die Kaesten (der Rahmen der Message-Boxen des Screens, `___13248h_cdecl`, plus der Name, der lange auf zwei
Zeilen) und laesst den Kupferrahmen darueber springen; der gezogene Modus bleibt danach als Banner oben mittig
stehen (`dr3_spin_header`). Runde 2 dreht das Rennen wie vorher. Die Karten verschwinden ueber `___30df8h()` -
die Poster des Screens liegen im *Screen*, nicht im Hintergrund, ein Restore aus dem Hintergrund liesse leere
Kaesten zurueck.

Zwischen den beiden Runden liegt eine Pause, in der das Ergebnis von Runde 1 stehen bleibt:
`DR3_CHAOS_SPIN_MODE_HOLD_S` Sekunden (3, `dr3_spin_hold`) - die Karten und der Rahmen auf dem gezogenen Modus,
ohne dass schon Runde 2 laeuft. Die Pause ist eine *Zeit*, keine Framezahl: sie rechnet mit `___60458h`, der
Framerate des Spiels (der Frontend setzt 60, `___60420h.c`). Im Harness liest derselbe Code darum laenger - der
Host praesentiert deutlich weniger Bilder pro Sekunde als die 3DS - und das Log schreibt beide Zahlen hin
(`round 1 done (NAME) - the boxes stay on it for 3 second(s) = 180 frame(s) at 60 frame/s`).

Der Lauf braucht die Zeile `raced 1` (sonst ist das Rad aus, siehe die Tabelle oben) und liest:

    [host] signup: entering the real sign up screen (___3266ch) - the wheel lives there
    [dr3] chaos: sign up screen: the selector is armed (mode 3, RANDOM)
    [dr3] chaos: spin: round 1 - the mode: DESTROY 'EM ALL over the three boxes of the screen (round 2 is the race)
    [dr3] chaos: spin: round 1 done (DESTROY 'EM ALL) - the boxes stay on it for 3 second(s) = 180 frame(s) at 60 frame/s
    [dr3] chaos: spin: landed on race 1 (0..2, the fights are there) and mode 2 (DM) - ENTER signs up

Die Frames (`logs/spin_look3/`, alle Rohbilder in `run_host/frames/spin3/`) zeigen die drei Karten mit dem Rahmen
auf dem gezogenen Kasten, danach die Poster zurueck, das Banner oben und den Rahmen auf dem gezogenen Rennen.
Die Key-Datei des Harness hat das Format `"<ms> <key> down|up"` (§5.4) - ein fuehrendes `key ` macht aus jeder
Zeile eine Fehlermeldung, und der Lauf drueckt dann keine Taste. Wer die Ansicht nach dem Spin sehen will, darf
die Vorlage nicht mit `RETURN` fuellen: die Taste liegt waehrend des Spins in der Warteschlange und meldet den
Fahrer an, sobald das Rad steht.

Prueflauf 25 (`logs/run_prepare1.ps1`): derselbe Direktstart, aber `--mode 1` (Duell) und `--screenevery 150`
- damit ist auch der **Prepare-Screen** pruefbar, den der Quick Race sonst nur durchlaeuft. Erwartet und
gesehen:

    [dr3] chaos: race mode 1 (DUEL) - a duel against one rival of the standings
    [dr3] chaos: duel: rank 20 has 3 driver(s) within 3 rank(s) - the rival is drawn from them
    [dr3] chaos: duel: race 0 has 2 entry/entries now (seat 19 against seat 18)
    [dr3] chaos: duel: seat 19 (, rank 20, 0 points) against seat 18 (FARMER TED, rank 19, 2 points)
    [dr3] chaos: grid place  1: FARMER TED  (standings 19, ...)
    [dr3] chaos: grid place  2:             (standings 20, ...)   <- der Spieler (im Host ohne Namen)
    [dr3] CHAOS build: 2 cars on the grid (the race asked for 2, the extras are off (0))

Die beiden praesentierten Frames um den Rennstart (`run_host/frames/w_0075.bmp`, umgewandelt nach
`logs/prepare1_shots/`) zeigen genau zwei Gesichter (Rang 20 und 19, "FARMER TED") und leere Kaesten in den
Slots 3 und 4; das Feld im Log hat genau diese zwei Autos. Die Gegenprobe `logs/run_prepare2.ps1`
(`--mode 0`, `logs/prepare2_shots/`) zeigt vier Gesichter und zwanzig Autos auf dem Grid. Vor dem Fix standen
im Duell die vier Eintraege der Veranstaltung auf dem Screen und fuhren auch - der ausgeloste Rivale kam gar
nicht an den Start.

**Der Roster ("PREPARE TO RACE") und die Sign-up-Listen.** Der Roster-Schirm wird beim Rennaufbau
(`___33010h_cdecl`) gezeichnet und ist damit im Direktstart des Harness zu sehen - er liegt hinter dem
"NOW LOADING THE GAME"-Schirm und ist nur mit dichten Bildern zu erwischen:

```powershell
powershell -ExecutionPolicy Bypass -File logs\run_roster3.ps1       # Standard-Modus, --screenevery 60
Select-String -Path run_host\drally_host.log -Pattern 'roster row'  # was der Schirm nennt
# das Bild (640x480) daneben: die vier Spalten links ausschneiden und vergroessern
powershell -ExecutionPolicy Bypass -File logs\zoom_panel.ps1 -In run_host\frames\w_0180.bmp `
           -Out logs\roster.png -X 4 -Y 100 -W 138 -H 238 -Zoom 3
```

Die Zeilen des Rosters stehen eins zu eins im Log (`[dr3] chaos: roster row 1 of 4: JANE HONDA (standings 1,
100 points, car 5)`) und die ersten vier Plaetze des Grids (`grid place 1..4:`) muessen dieselben vier Fahrer
nennen - sonst zeigt der Schirm etwas anderes als das Rennen faehrt. Vorher standen dort die vier Sitze der
Veranstaltung (`logs/prepare2_shots/w_0074.png`: Raenge 1, 2, 3 und **20**), jetzt die vier Fuehrenden
(`logs/roster_after_w0180.png`). Fuer das Duell ist `logs/run_roster2.ps1` (`--mode 1`) die Gegenprobe: zwei
Zeilen, der besser platzierte Rivale in Spalte 1. Die **Listen des Sign-up-Screens** sind auf dem PC nicht zu
sehen (siehe §5).

