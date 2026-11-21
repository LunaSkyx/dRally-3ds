# Notes for coding agents working on this repository

dRally-3ds is dRally (Death Rally, 1996) with a **Nintendo 3DS** port on branch `3ds`. The decompiled engine
is upstream code; the port lives in `platform_3ds/` and in the `#if defined(__3DS__)` blocks of the shared
files. Windows and Linux builds must keep working.

## Read first

| Document | What it is |
|---|---|
| `doc/scenario_harness.md` | **start here for any test of the real 3DS build**: `scripts\3ds\az_run.ps1` runs the actual `.3dsx` in Azahar unattended - event-driven scripts (`until ready`, `quickrace`, `until event race_start`), PNG screenshots of the top screen, `-Mode duel|dm|standard`, auto-quit; into a race in ~8 s. One command, no human. The PC harness below is the second choice. |
| `doc/arena.md` | the **ARENA chaos mode** (`DR3_CHAOS_MODE_ARENA`): map generator `scripts/arena/make_arena.py` -> `TRA.BPA`, rules, hunting AI (`platform_3ds/dr3_arena.c`), the 4-card wheel, how to test it |
| `doc/host_harness.md` | **how to test a change without the emulator** - the PC harness: build, run, switches, log vocabulary, what it cannot cover, the rules for guarding changes, and a recipe section for feature work (German) |
| `doc/3ds.md` | the port itself: platform code, the chaos test build, what the 3DS side does differently |
| `README.md` | install, controls, build |

## The commands that matter

```powershell
# the real 3DS build in Azahar, unattended (doc/scenario_harness.md) - build first, then:
powershell -File scripts/3ds/az_run.ps1 -Scenario scripts/3ds/scenarios/race_fast.txt -Mode duel -Extras 18 [-Boss 0..100] [-Grep 'regex'] [-Build chaos]   # -Mode standard|duel|dm|arena|random
# frame time / stutters / causes: ... -Scenario scripts/3ds/scenarios/perf_race.txt -Perf -Mode standard -Extras 18   (doc/scenario_harness.md, 'Frame time')
# race_fast.txt = straight into a Chaos race in ~8 s (event driven, quickrace), 3 screenshots, quits itself
# -> logs\az_runs\<stamp>_<scenario>\  drally_3ds.log + one PNG per "screenshot" step (open them with Read)
```

```powershell
powershell -File scripts\3ds\gen_host_project.ps1 -Build   # PC harness of the main build (MSVC + SDL2)
powershell -File scripts\3ds\run_host.ps1 -Extras 18 -QuickRaceMs 15000 -Frames 2500 -DumpEvery 8000
powershell -File scripts\3ds\run_host.ps1 -Extras 18 -QuickRaceMs 15000 -Frames 8000 -ExtraArgs '--auto 1 --track 16'
powershell -File scripts\3ds\host_shots.ps1                # BMP -> PNG
powershell -File scripts\3ds\report_host.ps1               # what the run did
powershell -File tests\build_tests.ps1                     # host unit tests (dr3_blit, dr3_input_map)
```

A run needs `run_host\dr3_keys.txt` to skip the ~80 s intro (`9000 return down`; the template in
`scripts\3ds\host\` does that), and the direct start waits for the menu by itself - `-QuickRaceMs` can no
longer fire into the intro (`doc/host_harness.md` §2).
```

```bash
bash scripts/3ds/build_3ds.sh    # the 3DS release the branch ships -> build/3ds/dRally_3ds.3dsx
bash logs/build_release_bg.sh    # main build -> logs/build_3ds.log (its md5sum is at the end of the log)
bash logs/build_repro.sh <name>  # the same with a fixed SOURCE_DATE_EPOCH -> logs/build_repro_<name>.log
```

`build_release_bg.sh` does not set `SOURCE_DATE_EPOCH`, so its hash changes on every run - only
`build_repro.sh` gives hashes two builds can be compared with.

## Rules

* **The 3DS main build is the reference.** Everything only the harness or a test needs goes behind
  `#if defined(DR3_HOST)` (or the existing `DR3_CHAOS`). After a change, prove the console build did not
  move: `bash logs/build_repro.sh <name>` (fixed `SOURCE_DATE_EPOCH`) and compare its `md5sum`.
* **The scenario runner (`platform_3ds/dr3_scenario.c`) is in every 3DS build, inert without
  `sdmc:/dr3_scenario.txt`.** Test the build that ships - there is no separate test build. Never leave that file
  on the SD card (`az_run.ps1` deletes it in a `finally`); never start the emulator by hand with one present.
  Builds made before 20. November 2026 do not have it, so their hashes are not comparable with newer ones.
* `__DATE__`/`__TIME__` end up in the binary (`race___42824h.c`), so a plain md5 comparison between two
  builds means nothing without `SOURCE_DATE_EPOCH`.
* Host-build warnings are findings: MSVC's `C4700` in `platform_3ds/` already caught a real bug that GCC
  had silently optimised away.
* Diagnostics for the chaos log: prefix `[dr3] chaos: `, and gate the expensive ones (per pixel, per car,
  per frame) on `dr3_log_enabled()` / `dr3_log_timing()` - otherwise the run costs the factor it measures.
* **Do not call `___3079ch_cdecl()` in a loop to fill a race**: its race choice is an unbounded `while(1)`
  that redraws a 640x480 screen per car (`doc/host_harness.md` §8.5). The harness fills the field itself.
* **The bottom screen is single buffered and the console draws on the real screen** (`platform_3ds/dr3_bottom.c`): never
  `ESC[2J` + redraw (a blank flash), overwrite in place with rows padded to the full width; stage pixel painting (the
  minimap) in `dr3_bottom_offscreen` and copy it in one go. `screenshot` saves the bottom screen too (`x_bottom.png`),
  and `scenarios/bottom_screen.txt` proves a page is drawn once (`ranking page drawn #1`, no `#2` while idle) -
  `doc/scenario_harness.md`, "The bottom screen".
* **The direct start waits for the menu** (`dr3_host_menu_ready()`, `dr3_host.c`). Started earlier - the
  intro of this game runs ~80 s - `___33010h_cdecl()` copies NULL graphics and dies in a `strcpy`/`memcpy`
  without a line in the log (`doc/host_harness.md` §2/§5).
* **A key script cannot steer - use the autopilot for anything that needs the player to race** (`auto 1`, or
  `--auto 1` / `run_host.ps1 -ExtraArgs '--auto 1'`): the engine's own AI (`race___4b62ch`) drives the
  player's car as well and the input trace stops writing his controls (both `#if defined(DR3_HOST)` in
  `race_main.c`).  Off by default, so a run that does not ask for it is unchanged.  With gas alone the
  player leaves the road in the first corner of the chaos grid he starts on (20 cars behind the start line
  are ~750 px of road on TR8) and grinds the `y = 20` clamp for the rest of the race - see the note in
  `platform_3ds/dr3_host.h`.
* **The chaos kill bonus is checked with `auto`, `field`, `power`, `hp`, `nomine`, `ammo` and `weapon`** (all
  host-only, `platform_3ds/dr3_host.c`): `field <ms>` logs every car's HP/position/lap (a wreck is `hp 0`),
  and the payout logs
  `kill bonus: NAME (car N, model M) wrecked by CAUSE pays $X into seat S: money A -> B, income C -> D, race kills $E, text slot T` -
  right next to the field lines it says whether the car that went to 0 HP paid, and what took its last HP
  (`the player's bumper`, `the player's gun`, `its own boost`, `a spectator`).  A wreck the player had no
  part in logs `NAME (car N) wrecked by CAUSE - no kill bonus` instead - the blame window (120 frames of
  the player's last hit or bump on that car, `race___4dcach.c`) decides.
  The harness' player drives the slowest car from the last grid place, so `power <x>` (the player's car
  power, an engine upgrade in game), `hp <car> <value>` (a weakened victim), `nomine <0|1>` (the AI mines
  the pack it drives in, and a mine pays nothing), `ammo <0|1>` and `weapon <0|1>` (the game's own full
  ammo cheat plus the machine gun itself, so the gun pays *reliably* instead of when a weapon pickup
  happens to be loaded) and `nohurt <0|1>` (full HP, so the player survives several kills) set that race
  up.  `doc/host_harness.md` §2 has the run; `scripts/3ds/report_host.ps1` ends with the list of wrecks,
  whether each one paid and, if not, why not.
* **The race report is where the chaos money shows up** (`___36adch.c`, "Your Statistics"; the numbers are
  worked out right after `race_main` in `___33010h.c`).  Two things pay in chaos and both are in that
  screen: the **kills of the race** (credited to `racer.money` *and* `racer.income` when the wreck happens,
  summed by `dr3_chaos_kill_money()`) and the **rank money of every finishing place**
  (`platform_3ds/dr3_payout.h`: `DR3_CHAOS_RANK_STEP` per place above the last one, a jump at the podium -
  `$5000`/`$4000`/`$3000` - and nothing at all for the last place, which is where a wrecked car ends up; the
  race's own tiers reach three places only, which is nothing on a field of twenty).  The ladder *replaces*
  those tiers in chaos (the `switch` that pays 750/3000/12000 is skipped, `___33010h.c`), so winning a chaos
  race is $5000 exactly.  It is a portable function (`platform_3ds/dr3_payout.c`) with a table test in
  `tests/test_dr3.c`, so the numbers can be retuned without touching the race.  The report logs
  `report of the race: place N of M -> rank money $X, kills $Y: race income $A + bonus income $B = $C, money D, income E`
  (flushed at once, because the screen after it waits for a key), and `report_host.ps1` prints it.  Reaching
  it in a host run needs the race to *end*: `--laps 1` (the harness' `laps` switch, `dr3_host.h`) shortens
  the race for that - five laps of twenty cars are minutes - and `-AutoKeyMs 5000 -AutoKey return` presses the
  key the "RACE OVER! - PRESS ENTER TO CONTINUE" screen waits for (`race_main` returns only after it).
  Verified 26. September 2026 (`logs/run_report3.ps1`, `logs/report_demo1/`): `place 6 of 20 -> rank money
  $3750, kills $375` in the log, and the screen shows `Placing: 6.`, `RACE INCOME: $3750`, `BONUS INCOME:
  $425`, `TOTAL RACE INCOME: $4175`, `TOTAL INCOME: $4175`, `TOTAL MONEY: $4670` (495 from the savegame plus
  the 4175).  The ladder *replaces* those tiers, and the same setup proves it on the podium
  (`logs/report_demo1/rank_money_lines.log`): `place 3 of 20 -> rank money $4500` (a tier would make it
  4687/4875) and `place 1 of 20 -> rank money $5000` (a tier would make it 5750).  A race the player *won*
  has no report (it plays the ending, `___22808h`), the numbers are logged either way.
  The numbers *behind* the ladder were reworked on 27. September 2026 (that flat
  `5000*(cars-place+1)/cars` paid $1000 for the last place of a five car field): the ladder is
  `platform_3ds/dr3_payout.h` now - $100 per place above the last one, $5000/$4000/$3000 on the podium and
  $0 for the last place.  Checked place by place in `tests/test_dr3.c` (`test_payout`) and in a race the
  player was wrecked in (`logs/run_payout11.ps1`, 1 HP on the player's car): `report of the race: place 20
  of 20 -> rank money $0, kills $0: race income $0 + bonus income $0 = $0, money 495, income 0`.
* **The chaos standings pay every place, the result screens only draw** (`drally.h`: `DR3_CHAOS_POINTS_TOP`,
  `dr3_chaos_points`): 1st +6, 2nd +5, 3rd +4, 4th +3, 5th +2 and *every* other place +1 - the spread of one
  race stays narrow, so the front of a twenty car field cannot pull away.  The points are awarded for all
  cars right after `race_main` (`___33010h.c`, *before* the standings are sorted) with one log line per place:
  `chaos: points: place N of M -> +X for NAME (points A -> B)`.  The tables of the results screens (easy
  3/2/1, medium 5/3/1, hard 10/7/4) are skipped in chaos - they reach three places only - and those screens
  draw nothing but the `+N` marks, for the whole field, not just the podium (`___36068h.c`, `___36358h.c`,
  `___36718h.c`).  Which racer sits in car n comes from `dr3_chaos_car_seat[]` (`dr3_chaos.h`), filled by the
  grid setup in `___33010h.c` (the four start slots) and for every extra car as it is added - the same place
  that fills `dr3_chaos_car_rank`/`dr3_chaos_car_pos`.
* **A chaos race has one result screen, not three.**  `___3892ch.c` normally walks the panels of all three
  classes one key press at a time (easy = cars 0-3 of `___1a0ef8h`, medium = 4-7, hard = 8-11).  Chaos runs
  *one* race of class `D(___1a1028h)` with the whole field in it, so only the panel of that class is drawn
  and only one key press follows - the two other classes never raced, and their panels would list racers that
* **The four rows of a result panel name the cars that raced** (`platform_3ds/dr3_results.h`,
  `___36068h.c`, `___36358h.c`, `___36718h.c`).  A panel draws four rows and every row is one racer - name,
  portrait, car picture and rank - so the row has to be a car of the field.  The original loop searched the
  *first four cars* for the places 1..4 and used the position of the car as the index into the entry list of
  the race: in a four car race that is the same thing, but a chaos field lines twenty cars up from the
  standings (`___33010h.c`), the four best places are anywhere in it, and a row no car of them filled kept
  whatever was in it.  On the 3DS that showed as the same racer in several rows (three of four rows of "JANE
  HONDA" in the MEDIUM RACE RESULTS panel of a race he won once) - the host run `logs/run_results1.ps1`
  reproduced it with all four rows of him (`logs/res1_look/1_vorher_*.png`).  `dr3_results_rows()`
  (`platform_3ds/dr3_results.c`) maps the places over the whole field (`dr3_chaos_car_seat[]`, `NUM_OF_CARS`)
  and fills a row without a finisher with a racer that is not in the list yet, never a second copy; the unit
  tests check the four car race as well as a duel, gaps in the result, garbage and the twenty car case.  The
  panels log what they drew (`chaos: results rows: 1. NAME (racer N), 2. ...`), right next to the
  `points: place N of M -> ...` lines of the same race, so a run can be compared line by line - and the panel
  of `logs/run_results1.ps1` now reads `1. DUKE NUKEM, 2. NASTY NICK, 3. JANE HONDA, 4. MATT MILER` where the
  race had exactly those four places (`logs/res1_look/3_nachher_*.png`).

  were not even in the field.  Checked with `logs/run_points3.ps1` (`logs/points_demo1/`).
* **The boss' "You were lapped, you loser!" screen stays out of chaos** (`___14cach.c`, drawn from the flag
  `___196ab0h`).  The flag is set in `race___55ae0h.c` when the best other car is more than a lap ahead of the
  player (logged as `[dr3] chaos: lapped: best other car at N, the player at M`), which a chaos field of twenty
  mixed classes does as a matter of course - and the screen ends with "No money or points awarded", which the
  chaos payout contradicts (`___33010h.c` pays the rank money and the points of every place).  The flag itself
  keeps its value: it is the game's own state (it keeps the player out of the podium rows of the result screens
  and out of the class bonus at `___33010h.c:1032`), only the two places that *show* the screen are gated - the
  dispatch after the race (`shop_main.c`, `if((D(___196ab0h) == 1)&&(!dr3_chaos_active()))`) and the fallback
  of the boss' message chain (`if((D(___196ab0h) != 1)||dr3_chaos_active())`).
* **The chaos modes** (`drally.h`: `DR3_CHAOS_MODE_...`, `platform_3ds/dr3_log.c`): a mode is a rule set the
  *player* picks - the file `chaos_mode.txt` / `sdmc:/chaos_mode.txt`, the `L+R+Y` combo in the front end
  (`platform_3ds/dr3_input.c`) or the wheel of the sign up screen itself.  It does *not* hang off the chaos
  difficulty: the wheel is the way into these races, so `dr3_chaos_mode()` returns what was set on *any*
  savegame, and only what the level adds on top of it (the bigger field, the AI tables, the lapped screen)
  stays behind `dr3_chaos_active()`.  A mode that was picked applies to every race - that is what the setup
  reads.  The selector comes up as `DR3_CHAOS_MODE_RANDOM` in the chaos build (`DR3_CHAOS_MODE_DEFAULT`), so
  the wheel runs out of the box, and `L+R+Y` (or the file) turns it to a fixed mode or back to `STANDARD` for
  the plain race selection.  The sign up screen reads the file *when it opens* (`___3266ch.c`), so a mode set
  on the SD card or with the combo is live for the very next registration instead of for the race after it.
  The setup logs what it read (`[dr3] chaos: chaos_mode.txt says mode N (NAME)`, `[dr3] chaos: sign up screen:
  the selector is armed/off`) - those two lines are the first place to look when the wheel seems dead.
  `DR3_CHAOS_MODE_DUEL`
  is the first one: two cars, the player's own and one rival of the standings - a driver within
  `DR3_CHAOS_DUEL_RANK_WINDOW` ranks of him, drawn at random (`dr3_log.c`: `dr3_chaos_duel_pick`) - while
  `race_main.c` keeps the extra cars out, and the results screens print the
  `+N` of a row from `dr3_chaos_car_gain[]` - the value the race itself handed out - instead of working the
  place curve out again, because a duel pays Elo.
  **The duel is offered where the race would be picked** (`___3266ch.c`, the sign up screen): a driver of that
  window is named right there (the same rule the race setup uses) and asks for the two car
  race - a line like `[FARMER TED WANTS A DUEL - ENTER: FIGHT, ESC: RACE ON` (capitals: the font has no lower case).  ENTER signs up as usual and
  the race then runs as the duel against him (the setup in `___33010h.c` keeps the rival the screen named),
  ESC turns him down: the mode falls back to `STANDARD`, the line is cleared and the screen stays, so an
  ordinary race can still be signed up for.  The PC harness cannot reach that screen (`--quickrace` starts
  the race directly and the career flow needs manual menu navigation), so this last step is checked in the
  game itself - the draw and the keys build into every 3DS build.
  **The two contenders are put in place before the race screens are built** (`___33010h.c`): the setup puts the
  player and the rival into the seat bytes `esp+0xcc` and cuts `NumCars` down to two at the *top* of the
  function, so the roster screen draws two portraits and the car table is filled from those same seats.  Read
  behind both of them - where that block sat until 27. September 2026 - only the number of cars came from the
  duel while the screen and the field kept the event's four entries: the "PREPARE TO RACE" screen showed
  drivers that never drove and the rival that was picked never started.  The rival the screen named is taken
  back only while he still is a driver of the window (`dr3_chaos_duel_rival_ok`), because a name from an
  earlier visit can be far away by the time the race is set up.
  Verified 27. September 2026 (`logs/run_prepare1.ps1`): `duel: rank 20 has 3 driver(s) within 3 rank(s)`,
  `duel: seat 19 (, rank 20) against seat 18 (FARMER TED, rank 19)`, a grid of those two cars - and the screen
  dump (`logs/prepare1_shots/`) shows two portraits.  `logs/run_prepare2.ps1` is the counter-check: the
  standard mode shows four portraits and twenty cars on the grid again.
* **The ARENA** (`DR3_CHAOS_MODE_ARENA`, `doc/arena.md`) is the fourth chaos mode and a fourth card of the sign-up wheel: its own
  map (`TRA.BPA`, made by `scripts/arena/make_arena.py` from the player's `TR1.BPA` - never committed/distributed), all cars start at once in the
  four corners, 16 respawning item places (ammunition, turbo, collectible mines, repairs), a *hunting* AI (`dr3_arena_ai`) instead of the
  road AI (`dr3_ai_call` in `race_main.c`), the top three survivors are paid (`DR3_ARENA_PRIZE_*`). It shares the tick and the "last car
  standing" end with the deathmatch (`dr3_chaos_battle()` = DM or ARENA).  Test: `-Mode arena` with `scenarios/arena*.txt`, the wheel with
  `scenarios/wheel*.txt` (the `raced` step arms it).
* **"Destroy 'em all"** (`DR3_CHAOS_MODE_DM`) has no finish line: `race___55ae0h.c` stops setting the `Finished`
  flag when the laps are up, *keeps the lap counter running* (no clamp any more, the HUD keeps counting) and
  leaves `___196dd4h` at 0 - that is the flag the frame pass uses for "the race distance is done", and it was
  what ended a deathmatch after the race distance before (`race_main.c` starts its race-over sequence when the
  leader with that flag set crosses the line).  So the race can only end the way the engine ends any race - when
  all but one car are destroyed (`race_main.c`, `___243d0ch`, the "N-1 done" count), and a line
  `deathmatch: race over with N of M car(s) still running` says what it ended with.  The track is armed for it as well: `race___5209ch.c` keeps `DR3_CHAOS_DM_PICKUPS_MAX` (12) pickups on the
  track at once instead of the original's four (every slot) and every one of them is the **ammunition** pickup
  (kind 1, the resource at `+0x30` the HUD draws as its AMMO BAR, `race___526ach.c`) - the original only rolls
  that kind for a car that already carries a weapon, the deathmatch rolls it always.  Verified 27. September
  2026 (`logs/run_ammo1.ps1`): `pickup: car 10 took the AMMO pickup (kind 1) - ammo is now 89700`.
  The lap indicator of the HUD is a table of ten sprites (`___1de210h`), so a deathmatch does not draw the
  "of N" part at all and its counter stops at `DR3_CHAOS_HUD_LAP_MAX` (9) instead of reading past it.
  The last car standing takes `DR3_CHAOS_DM_POT` ($5000) and the winner's points
  (`dr3_chaos_points(1)`, +6), every destroyed car scores nothing - but each wreck paid its kill money the
  moment it happened (`race___4dcach.c`), so the money for destroyed opponents stays exactly what the other
  modes pay.  Verified 27. September 2026 (`logs/run_dm2.ps1`, `logs/dm_demo1/`): 19 wrecks in one race,
  `deathmatch: the player is the last car standing (car 3) -> pot $5000, kills $475`, the survivor's
  `points: place 1 of 20 -> +6` against `+0` for every destroyed car.
  Verified again 27. September 2026 after the endless fix (`logs/run_dm4.ps1`, `logs/run_dm2_engine.log`): the
  race was **still running after 98 s** (~20 laps of a four lap track, the frames of the run simply ran out),
  16 of 19 opponents wrecked, seven payouts (`race kills $625`), no race-over line - the laps no longer end it,
  which is the whole point.
* **The deathmatch has a deadline: the sudden death** (`platform_3ds/dr3_dm.h` / `dr3_dm.c`, the entry in
  `drally.h`).  Reason: the engine's "all but one are out" count (`race_main.c:811`, `___243d0ch`) only counts
  a destroyed car while it *stands still* (`s_35e[n].__b0 <= 0.5f`), and the only other exit used to be the
  player's own wreck - so a stuck, unreachable or endlessly circling car kept a DM running forever (and a race
  the player *survives* could hang, `logs/run_dm4.ps1` is the run that shows how long it runs).  So:
  - `dr3_dm_tick()` runs per frame of a DM (`race_main.c`, right after the engine's own count).  It reads the
    race's own clock (`getCounter(1)`, 70 ticks a second, started by `race___49a34h.c` - and it is reset there
    by `dr3_dm_reset()`, so a slow frame does not stretch the deadline); the harness is therefore frame exact.
  - `DR3_DM_SUDDEN_AT_S` (180 s = 3 minutes, `sdmc:/chaos_dm.txt`, `dmsudden <n>` in the harness, 0 = off) into
    the race the sudden death starts: `DR3_DM_SUDDEN_PERCENT_S` (1) percent of HP per second for **every** car
    that still has one, the player included.  The unit is the game's own (`race___400ech.c`: damage = 100 -
    HP/1024), so 1 percent = `DR3_DM_HP_PER_PERCENT` (1024) HP on **any** car, whatever scale it started with.
    Nobody survives it: after 100 s of sudden death every car is a wreck.
  - The port starts the engine's wind down itself (`D(___2438c8h)++`, the counter `race_main.c:1074` reads)
    as soon as only one car has HP left - the engine's own rule without its "and it stands still", so a wreck
    that keeps rolling cannot keep the race alive either.  The payout needs no change: "the player is alive at
    the end" *is* "the player is the survivor" (the pot rule, `___33010h.c`).
  - The **banner** `SUDDEN DEATH` is drawn by the port's own pass (`dr3_blit.c`, `dr3_dm_banner()`, the same
    place the health line goes): the game's own font (`___1d7810h`, 6x6 glyphs from 0x20) at
    `DR3_DM_BANNER_SCALE` (3 = 18 px letters), outlined in black and drawn in yellow, centred at row 22 of the
    320x200 game screen - so the 3DS and the PC window show the very same banner and no palette entry is
    needed for it.  It holds `DR3_DM_BANNER_HOLD_MS` (6 s) and then blinks every `DR3_DM_BANNER_BLINK_MS`.
  - Log trail: `deathmatch: the sudden death starts after N second(s) ...` (race setup),
    `sudden death starts Ns into the race - ...`, then `sudden death after Ns: X percent taken, Y of Z car(s)
    left, the player's car at H HP` every ten percent, the banner line
    (`the 'SUDDEN DEATH' banner is drawn ...`) and `sudden death decided the race - N car(s) left at Ns`.
  Verified 27. September 2026 with `logs/run_dm7.ps1` (a DM with `dmsudden 8` and the field at 3 %): the drain
  takes exactly `DR3_DM_HP_PER_PERCENT` HP per car and second, the banner is drawn (`logs/dm7_shots/`), and the
  race ends with the usual winner/payout lines; `logs/run_dm8.ps1` is the counter-check with `dmsudden 0` - no
  sudden death, the mode runs exactly as before.
* **The lap duty against camping** (`platform_3ds/dr3_dm.c`, the same entry in `drally.h`).  Why it is needed:
  the drain takes the same HP per second from every car, so "park in a corner and take no damage" simply
  outlives the field - and the pot goes to the survivor (`___33010h.c`), so camping was the *winning* move of
  a pure sudden death.  So every `DR3_DM_LAP_INTERVAL_S` (60 s, `sdmc:/chaos_dm_lap.txt`, `dmlap <n>`, 0 = off)
  seconds of race time a car has to have finished `DR3_DM_LAP_PER_INTERVAL` (1) lap, or it loses
  `DR3_DM_LAP_PENALTY_PERCENT` (25) percent = 25600 HP at that check; four missed checks wreck it.  A lap takes
  ~25..40 s on the tracks, so nobody who drives notices it.
  - The laps are counted by the port, *not* read from `s_35e[n].Lap`: a deathmatch clamps that field to
    `DR3_CHAOS_HUD_LAP_MAX` (9) for the HUD (`race___55ae0h.c`).  Counted is the engine's own lap crossing: the
    checkpoint counter `s_35e[n].__0` (offset 0 of the car) goes back to 0 exactly when a lap is counted.
  - Log trail: `deathmatch: a car has to finish 1 lap(s) every 60 second(s) or it loses 25 percent ...`
    (race setup), `lap check N: car C finished no lap since the last one - 25 percent of its HP (25600 HP), it
    is now at H HP` per car and check, plus `chaos_dm_lap.txt: ...` when the switch file says something else.
  Verified 27. September 2026 with `logs/run_dm9.ps1`: the player's car stands still from second 0 (`auto 0`)
  while the field drives laps, `dmsudden 0` keeps the drain out of the picture and `dmlap 60` is the real rule -
  at the first check the standing car loses exactly 25600 HP, the lapping cars do not.
* **Nothing of the port is drawn over the RACE OVER screen** (`dr3_chaos.h`: `dr3_chaos_race_over()`,
  `dr3_blit.c`).  The engine's own race state is `___243d14h`: 0 = the race runs, 1 = the race over sequence
  started (`race_main.c`), -1 = the race is being aborted, and the next race sets it back to 0
  (`race___49a34h.c`).  The moment it is not 0, every health line of the screen goes to its own state 4
  (logged as `over`, one line per car) and the sudden death banner is not drawn either - the RACE OVER screen
  and the fade to black that follows have nothing of the port over them, *whatever* ended the race.  Before
  this, `logs/dm7_shots_before/w_0434.png` shows the banner still burning on top of the blackening screen (the
  frame is from the run *before* the change); afterwards `logs/dm7_shots/w_*.png` has it gone.
  One summary line says how many lines were hidden:
  `hp line: the race is over (___243d14h != 0) - N health line(s) of the screen and the sudden death banner
  are hidden from here on`.
* **The random selector** (`DR3_CHAOS_MODE_RANDOM`, `___3266ch.c`) replaces the choice of races, and it draws
  **twice** - both rounds over the three boxes of the sign up screen (`D(___185a50h)` is the highlighted race,
  `___27f80h_cdecl` draws the copper frame around it, `___281d0h_cdecl` takes it away again):
  * round 1, the mode: the boxes carry the three rule sets as cards (the screen's own message box border,
    `___13248h_cdecl`, with the name on it; the long one on two lines), the frame hops over them with a delay
    that grows (`DR3_CHAOS_SPIN_*`: fast at first, then settling) and the mode then stays on the screen as a
    banner at the top (`dr3_spin_header`) - from there on the player sees *which* rule set round 2 is for.
    The result is also given time: round 1 sits on its drawn box for `DR3_CHAOS_SPIN_MODE_HOLD_S` seconds (3,
    `dr3_spin_hold`, in the frames of `___60458h` - the front end's 60, so it reads the same on the console
    and in a run) before round 2 takes the boxes over.  The race round keeps its own short
    `DR3_CHAOS_SPIN_LAND_HOLD`.
  * round 2, the race: the cards go (`___30df8h()`, the routine that paints the posters - they live in the
    screen, not in the background, so a restore from the background would leave empty boxes) and the frame
    lands on the race ENTER signs up for.
  Then it only sets `D(___185a50h)` and `dr3_chaos_mode_commit()`, so ENTER registers exactly as it always did
  (car, sounds, seat, grid).  Drawn are only races the player may sign up for (`dr3_spin_race_ok`: a free slot
  in `___1a1f64h` and the class checks of the ENTER branch); the modes have equal chances and the draw is
  logged (`[dr3] chaos: spin: round 1 - the mode: NAME over the three boxes ...` and `[dr3] chaos: spin: landed
  on race N and mode M (NAME)`).  The harness can check the *choice* without that screen (`spin <n>` /
  `--spin <n>`, `dr3_chaos_spin_report`) and the *drawing* with `signup <ms>` plus `raced 1` (which sets the
  career counter `dr3_chaos_raced` reads - a run would otherwise have to drive a whole race first).
  Verified 27. September 2026 (`logs/run_spin3.ps1`, frames in `logs/spin_look3/`, `run_host/frames/spin3/`):
  the log has the round 1 line and the landing, the frames show the three cards with the frame on one of them,
  then the posters back with the mode banner up and the frame on the race that was drawn.  The distribution is
  `logs/run_spin1.ps1` (`logs/spin_demo1/`): `spin: 200 draws - race 0/1/2 = 96/0/104, mode RACE/CHALLENGE/DM =
  70/69/61` - the blocked race 1 (a fresh career has not won race 0 yet) never comes out, the modes are equal
  thirds.
* **A duel pays a pot by the rank of the rival, and only for a win** (`DR3_CHAOS_DUEL_POT_TOP/_MID/_LOW` with
  `_TOP_RANK`/`_MID_RANK`): $5000 for a rival of the top five, $2500 up to rank 13, $1000 below, and nothing at
  all for a loss - the rank money ladder of a field race (`DR3_CHAOS_RANK_MONEY`) does not apply to a duel
  (the Arena keeps paying no money, as it always did).  The points are Elo:
  `DR3_CHAOS_ELO_BASE + DR3_CHAOS_ELO_STEP * (own rank - rival rank)`, capped at `_MAX_DIFF`, for the winner
  only - a duel can only move the player up, because the original career reads his points (`___3892ch.c`).
  Verified 27. September 2026 (`logs/run_duel6.ps1`, `logs/duel_demo1/`): `duel: won against rank 19
  (FARMER TED) -> pot $1000`, `duel: place 1 of 2 -> +6 for … (points 0 -> 6)`, `rank money $1000`.
* **The health line over the cars** (`platform_3ds/dr3_blit.c`, `dr3_carcolour_tint_cars`): a small line above
  every car that is *damaged* - full length and green while it is nearly whole, shrinking and turning over
  yellow to red as it loses HP, scaled to the HP it started the race with (`dr3_chaos_hp_max[]`, which the
  tint learns from the first present of a race - that is what makes the harness' `hp <car> <value>` a car
  that is "whole at 12%" - and a pickup can raise it again).  A whole car and a wreck get **no line**: it is
  drawn for 0 < HP < `DR3_CHAOS_HP_LINE_BELOW` (90) percent of that HP only, so the strip over an untouched
  car and the single dark pixel over a wreck are gone.  The **player's own car** never gets one either,
  whatever its HP is: the bar is there to read the *other* cars (which of them is nearly done), and his own
  damage is what the game's own HUD shows him (the DAMAGE panel - `s_54[n].__10`, `race___400ech.c`; seen in
  `logs/owncar_w0070.png` at 70% while his car has no bar).  `dr3_own_car()` reads `MY_CAR_IDX`, guarded, the
  way
  `dr3_bottom.c` does.  The decision is
  `dr3_carcolour_hp_line_shown(hp_now, hp_max, own_car)`, a portable function next to the threshold in
  `platform_3ds/dr3_blit.h`; `tests/test_dr3.c` checks both boundaries and the own car, the test project
  does not define `DR3_CHAOS` and would not see the tint's own part of the file).  It is drawn in
  the port's own image with the target format's channel shifts, so the 3DS and the PC get the very same line
  and no entry of the game's palette has to be kept free for it.  A race logs the decision per car - the
  whole table for the first presents and every *change* after that (`[dr3] chaos: hp line: car N hp X/Y
  (Z%) shown|own|off|wrecked`), which is what `logs/run_hpline1.ps1` (a short run, one wreck set up by
  `--hp 5 0`), `logs/run_hpline2.ps1` (a long one, cars take damage in the race) and `logs/run_hpline3.ps1`
  (ten cars start weakened, so their bars appear when they are hit and go again when they are wrecked;
  `logs/run_hpline4.ps1` is that run with `--screenevery`, and `logs/hpline_whole_cars_w0020.png` (four
  untouched cars, not one bar) and `logs/hpline_damaged_cars_w0042.png` (two hit cars, one green bar each)
  are the frames out of it).  The own car is checked by `logs/run_hpline6.ps1` (a "Destroy 'em all"): the player
  drives car 3 in the direct start, the run pulls him down to 30000 HP (59%, the HP a bar *would* appear at)
  and pulls car 0 down the same way - the log then reads `car  3 ... own` (8 own lines, not one `shown`) while another
  car does report `shown` and the `field` lines show his HP really falling (down to 0).  `logs/owncar_w0070.png`
  has him damaged and without a bar while the game's own HUD says `DAMAGE 70%` for him, and
  `logs/owncar_w0085.png` has him burning and wrecked, still without a bar.  Seen **before** this change in
  `logs/duel_demo1/hp_0403.png` (a green strip over the whole car).
* **The bars behind the in-race driver names are the *palette's* car ramps** (`race___4a25ch.c` builds them,
  `race___405bch.c` draws the rows, `race___42824h.c` decodes the panel picture for exactly those cars): the
  bar of a row belongs to the picture of that row's car slot and uses that slot's ramp (`10*n+15`), so *all
  four* ramps have to be built in every build.  The chaos build built only ramp 0 ("one ramp is enough - the
  port re-colours the sprites anyway") and left the bars of the other rows at whatever the *track* palette
  holds at `0x19..0x36`: `0x3f003f` (magenta) on TR6, which is what showed up as **pink name bars** on the
  3DS.  Fixed 27. September 2026 (`race___4a25ch.c`, one ramp per panel row from `s_54 +0x34..+0x3c`);
  checked on TR6 (`logs/run_pinkbars2.ps1`): the log's `chaos: ramp 1..3` lines name the cars' colours
  instead of `3f003f .. 3f003f`, and `logs/fixed_panel_f0022.png` / `logs/presented_panel_w0014.png` (the
  *presented* frame - palette and the port's car-colour pass, `logs/run_pinkbars3.ps1`) show the bars in the
  drivers' colours (`logs/pink_before_zoom.png` is the same frame with the track's magenta put back, i.e.
  the reported fault).
* **`D(x)` is address semantics** (`drally.h`): `D(MY_CAR_IDX)` is only right where the file declares that
  name as an *array* (`extern __BYTE__ MY_CAR_IDX[];`). Where it is a plain value, `D()` reads the value as
  an address - that crashed the host race at address 3 (`race_main.c`, §5/§6 of the harness doc). Look at
  the declaration in the *same* file before adding `D()`.
* **A crash reports itself** (Windows build): `dr3_host_crash_filter` in `dr3_host.c` writes the exception,
  the touched address and the function/file/line of the crash plus its callers into the log - `run_host.ps1`
  copies the PDB next to the exe for that reason, keep it there.
* The engine opens its assets relative to the **CWD** (`platform_3ds/dr3_paths.c`), so a harness run needs
  `run_host/` as the working directory - `run_host.ps1` takes care of staging it.
* **The sign up lists stay unnamed in a chaos race, and the loading screen names the real top 4** (27. September
  2026).  Two screens of the front end said who races, and in a chaos race neither said it right:
  * the three tier boxes of the **sign up screen** (`___3079ch.c`) list a `rank. NAME` line per entry the picker
    adds (and one for the boss, `dr3_adversary_draw`).  Those four entries of a tier are *not* the field - the
    race is the whole standings - so `dr3_signup_names_shown()` (the `dr3_chaos_active()` switch, so the release
    build keeps its lists) stops the lines: the tier pictures and the prices stay, the names do not.
  * the **roster** ("PREPARE TO RACE", `___33010h.c`; "NOW LOADING THE GAME" comes first, then this screen) draws
    `NumCars` portraits with rank and name.  It drew the seats the event handed it, which in a chaos race are the
    four entries of the sign-up tier: `logs/prepare2_shots/w_0074.png` shows ranks 1, 2, 3 and **20**.  It now
    names the four *leaders* of the field (`DR3_ROSTER_ROWS` rows, `dr3_grid_order_by_rank`) - the cars of the
    first four grid places, which is the order the grid itself lines up in - in Destroy 'em all and the normal
    mass race alike.  A duel draws its two contenders, the better placed one first.
  Both read **one definition of the field**, `dr3_chaos_field()` (`___33010h.c`): the seats of the event plus the
  extras the setup adds (`dr3_chaos_extra_cars()`, in roster order, none in a duel).  The screen cannot name a
  driver the race does not have - before this the roster and the field setup each had their own copy of that
  rule.
  Verified 27. September 2026 (`logs/run_roster1.ps1`, `logs/run_roster2.ps1`, `logs/run_roster3.ps1`,
  `logs/roster_after_w0180.png` against `logs/roster_before_w0074.png`): `roster row 1 of 4: JANE HONDA
  (standings 1, 100 points, car 5)` ... `row 4: MOTOR MARY (standings 4, 69 points, car 4)` against
  `grid place 1..4` of the same run - the same four drivers, and the race still ran its twenty cars; the duel
  run names `LIZ ARDEN (standings 17)` and the player `(standings 20)` as its two rows.  The **unnamed sign up
  lists** are only visible in the game: the harness fills the field with its own copy of the picker's rule and
  never opens that screen (`doc/host_harness.md`).

## Where things are

| | |
|---|---|
| port code | `platform_3ds/` (`dr3_*.c`) |
| PC harness | `platform_3ds/dr3_host.c`, `tests/dRally3DSHost.vcxproj`, `scripts/3ds/*host*`, `scripts/3ds/gen_host_project.ps1` |
| chaos build | `-DDR3_CHAOS`; switches `chaos_log.txt`, `chaos_time.txt`, `chaos_extras.txt` (read at every race setup) |
| experiments, logs | `logs/` (build scripts, plus `analyse_band.ps1` / `analyse_races.ps1` / `analyse_stage.ps1`, which read the **emulator's** log `%APPDATA%\Azahar\sdmc\drally_3ds.log`), `doc/` |
| harness leftovers | `run_host/` (game data copy + `drally_host.log`), `logs/host_run.out` - both git-ignored |

Whatever a run taught goes into `doc/host_harness.md` (§6 "Was er schon gefunden hat", §8 for the workflow),
so the next agent does not repeat it.
