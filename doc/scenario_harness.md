# Scenario harness - unattended runs of the real 3DS build in Azahar

**Read this first if you are about to test anything on the 3DS build.** It lets an agent run the *actual*
`.3dsx` (not the PC harness) in the Azahar emulator with no human: keys are pressed from a script, the
screen is saved as PNG, the log is read, the game quits itself. One run = one command.

```powershell
# 1. build (see "Building" below), 2. run a scenario, 3. look at logs\az_runs\<label>\
powershell -ExecutionPolicy Bypass -File scripts\3ds\az_run.ps1 -Scenario scripts\3ds\scenarios\explore.txt
powershell -ExecutionPolicy Bypass -File scripts\3ds\az_run.ps1 -Scenario <file> -Build chaos -TimeoutSec 300
```

Output: `logs\az_runs\<timestamp>_<scenario>\` with `drally_3ds.log`, `scenario.txt` (a copy) and one `.png` per
screenshot (400x240 = exactly what the top screen showed, **including the chaos car colours**, because it is
read back from the finished framebuffer, not from the engine's 8-bit screen). Open the PNGs with the Read
tool - you can see the game. The script prints the `[scn]` lines of the log and exits 0 only if the scenario
reached its `quit` step (1 = timeout, emulator died, no log).


## The FAST route: into a race in ~8 s, event driven (use this by default)

Fixed waits are slow and fragile, so scripts are **sequential and event driven**: every step goes on the moment
its condition holds, and every wait has a timeout (default 30 s) - a stuck screen fails the run
(`[scn] FAIL: timeout ... waiting for ...`, exit code 2) instead of hanging it.

```powershell
# a race in the chosen mode, 18 extra cars, 3 screenshots, done in ~15 s wall
powershell -File scripts/3ds/az_run.ps1 -Scenario scripts/3ds/scenarios/race_fast.txt -Mode duel -Extras 18 -Grep 'duel|grid of'
ace_fast.txt -Mode duel -Extras 18 -Grep 'duel|grid of'
#   -Mode standard | duel | dm | random | arena   (written to sdmc:/chaos_mode.txt for the run, then restored)
#   -Extras N                              (sdmc:/chaos_extras.txt)      -Boss 0..100 (sdmc:/chaos_boss.txt)
#   -Build chaos  -TimeoutSec N  -Label NAME
```

`race_fast.txt` is the template - copy it and change the part after `race_start`:

```
until ready spam Return every 150 timeout 40000      # Return skips the intro the moment it can be skipped
quickrace                                            # NO load menu / shop / market / sign up: field + PREPARE TO RACE + race_main
until event race_start spam Return every 150 timeout 30000   # Return also skips the PREPARE TO RACE hold (12.7 s -> 8 s)
until race_frames 100 timeout 20000                  # let the fade-in finish (frames 0..~60 are black)
screenshot s0_start.bmp
quit
```

Measured (Azahar, release build, script clock from the first poll): front end ready at **~6.3 s**, `race_start`
at **~8.0 s**, race visible ~1.5 s later. The emulator needs ~2-3 s more to boot before the script clock starts.

Sequential commands (full list in `platform_3ds/dr3_scenario.h`; they run in file order, no `at` prefix):

| Command | Meaning |
|---|---|
| `until ready [spam KEY every MS] [timeout MS]` | front end loaded **and** a racer (the player) exists |
| `until event NAME [spam KEY every MS] [timeout MS]` | engine event: `race_start` (race_main begins), `race_over` (it ends) - new events: call `dr3_scenario_event("x")` where it happens |
| `until race_frames N` / `until frames N` | N presented frames since `race_start` / since the run began |
| `quickrace [slot 0..2] [track 0..17]` | direct start, `platform_3ds/dr3_quick.c`. **The steps after it run inside the race.** |
| `key NAME tap\|down\|up`, `sleep MS`, `screenshot F.bmp`, `log TEXT`, `quit` | as the timed versions; `screenshot` waits for the frame |

`quickrace` is the 3DS twin of the PC harness' `dr3_host_quick_race` (`dr3_host.c`) - keep them in step. It
needs the racer the savegame provides at start-up (`ready` waits for it) and it sets the Chaos difficulty like
the licence screen does, so **every quickrace is a Chaos race**; the mode comes from `-Mode`.

**Menu route** (slow, ~75 s, only when the menus themselves are under test): `nav4.txt` walks Start Racing -> Load
Game -> slot 2 -> shop -> market -> sign up -> race with timed `atms` keys. The cursor skips greyed entries (one
`Down` from "Start A New Game" is "Load Game"). A loaded save is the *saved* difficulty - not necessarily Chaos.

### Gotchas found while building this
* `chaos_mode.txt` is read by the **sign up screen when it opens** and again by the race setup - but the setup
  first remembers "the mode the wheel rolled" (the setting, RANDOM, if nothing rolled) and puts it back over the
  file (`___33010h.c`, `dr3_rolled`). Without the screen a file asking for DUEL therefore ran as a plain 20 car race.
  `dr3_quick_race` reads the file first, which fixes the harness; the same stale value is why an in-game RANDOM
  default plus an external file change only takes effect through the sign up screen.
* Screenshots right after `race_start` are black: the race fades in (~60 frames). Wait `race_frames 100`.


## Frame time, stutters and their causes (`-Perf`)

```powershell
powershell -File scripts/3ds/az_run.ps1 -Scenario scripts/3ds/scenarios/perf_race.txt -Perf -Mode standard -Extras 18 -Label before
# change something, rebuild, run again with -Label after, compare the [perf] lines
```

`-Perf` switches the timing log on and the main log OFF (`chaos_time.txt=1`, `chaos_log.txt=0`: measuring with the
watchdog and the diagnostics running is no measurement) and prints the `[perf]` lines. `perf start` / `perf report NAME`
(scenario steps, `perf_race.txt` is the template; it pins `track 4` so runs are comparable) record **every frame**:

```
[perf] race: 750 frames, avg 15.24 ms (65.6 fps), p50 15.44, p95 16.48, p99 16.83, max 17.09 ms
[perf] race: 0 stutter frame(s) over 23.2 ms (0.0% of the frames)            <- slower than 1.5x median and 4 ms
[perf] race: stutter frames at: 190(26ms) 192(26ms) ...                       <- a constant gap = something periodic
[perf] race: ms/frame avg (p95) - FRAME 11.54(12.87) present 3.28(4.03) ...    <- what a frame is made of
[perf] race: worst #1 = frame 223, period 29.2 ms (median 16.2) <- FRAME +12.4, present +6.9, tint +6.9 ms   <- the cause
```

The phases are the `DR3_CT_*` slots of `dr3_log.h` (`dr3_chaos_tick()` before a section, `dr3_chaos_time(slot, t0)` after
it; they overlap - FRAME contains present, present contains blit + tint - so read each on its own). `other` is the
period minus pre + FRAME + wait: interrupts, audio, SD card I/O. **A new suspect = a new slot**: add a `DR3_CT_*`
define, a name in `dr3_ct_name[]` (dr3_log.c) and wrap the code, as the four `tint-*` slots in `dr3_blit.c` do.

Two shapes of problem the report separates: *isolated* spikes (a few frames, `stutter frames at` is sparse -> look at
`worst #n`, often `other`) and a *sustained* slow stretch (a contiguous run of indices, e.g. the start of a 20 car race
where the whole pack is on screen - that is cost, not a hiccup).

**Azahar caveat:** `svcGetSystemTick` there is guest time from the emulator's CPU model. It counts instructions well but
has no cache or memory-bus model, so an instruction-count win shows up here and a cache win does not (and a
cache-miss problem on the real console would not show at all). Treat ms as relative, check real hardware for the final word.

### Long runs and sweeps

* `autopilot on` (scenario step, before `quickrace`) hands the PLAYER's car to the engine's own AI - the same switch as
  the PC harness' `auto 1` (`race_main.c`, `dr3_scenario_autopilot()`). A key script cannot steer, and a parked player
  sees none of the race: the camera stays at the back of the grid. Off unless a script asks, so play is unchanged.
* `stutter_long.txt`: a whole race with the AI driving, measured in six 1500-frame windows (`perf report w1..w6`).
  A window that is worse than its neighbours points at what happened there.
* `scripts/3ds/sweep_perf.ps1 -Tracks '0,5,8,11' -Modes 'standard,dm' -Windows 2` runs one Azahar race per track x
  mode and writes `logs/az_runs/sweep_<stamp>/summary.md` (avg, p99/max, stutter frames, biggest phases, worst-frame
  cause). Track bytes: 0..8 = TR1..TR9, 9..17 = the same backwards (18 = TR0 is not raceable: missing TR0-FLIP.PAL, the run times out). Parameters are comma separated *strings*
  (an array does not survive `powershell -File`). About 1.5 min per combination.

## The bottom screen: screenshots, flicker rules (20. November 2026)

Every `screenshot x.bmp` step also saves the **bottom screen** as `x_bottom.png` (`dr3_bottom_save_bmp`, read from the
real framebuffer), so a run shows both screens. `scenarios/bottom_screen.txt` is the check: front end, idle for 5 s,
then a race. Its log lines are the proof that nothing is redrawn needlessly:

```
[dr3] bottom screen: ranking page drawn #1 (17 rows, cleared first)     <- ONCE for the whole front end
[dr3] bottom screen: map page drawn #1 (off-screen band copied in one go) <- once per race
```

A second `ranking page drawn` while nothing changed is the flicker bug back. What it was (found with exactly this run):

1. `dr3_bottom_update()` skipped an unchanged page only for **exactly 18 rows**. With no game loaded the block has 17
   (16 when the player leads), so the page was cleared and printed again **every 500 ms** - a blank flash twice a second
   for as long as the front end ran (that is the "flicker when the game starts").
2. A redraw began with `ESC[2J`: blank screen until the text was printed. Rows are padded to the full width now and
   written in place; the only clear left is the way back from the map page.
3. The map page cleared, printed, filled the band with the background and painted the track pixel by pixel - and the
   first incremental update painted the whole band **a second time** 125 ms later. Now the band is painted into an
   off-screen buffer (`dr3_bottom_offscreen`) and copied to the screen in one go, and `dr3_minimap_draw_incremental`
   only repaints after a full draw (`dr3_map_drawn`, reset by `build`/`reset`).

Rules that follow from it: the bottom screen is **single buffered** (libctru's console turns double buffering off), so
anything drawn is visible while it is drawn - never clear + redraw, overwrite in place; and **the console cannot draw
into an off-screen buffer** (this libctru ignores `PrintConsole.frameBuffer` and draws on the screen - tried, the text
never reached the buffer; its `ESC[2J` blanks the screen as well), only pixel painting (the map) can be staged.

### Measuring the flicker: the probe (`bprobe`), before and after on the same binary

The bottom screen is single buffered, so a viewer sees whatever the framebuffer holds when the LCD scans it. The
probe is a thread that samples the **real** bottom framebuffer every millisecond and counts the samples in which the
page is blank or half drawn (fewer than 60 % of the lit pixels of the settled page): the "virtual viewer".
`bottom legacy on` restores the OLD redraw behaviour (the 18 row early-out, clear before every page, map entry as
clear + print + direct painting), so the same binary gives the before and the after:

```powershell
az_run.ps1 -Scenario scripts/3ds/scenarios/flicker_frontend.txt -Grep 'probe|ranking page'   # front end idle, fix vs legacy
az_run.ps1 -Scenario scripts/3ds/scenarios/flicker_race.txt        -Mode standard -Extras 18 -Grep 'probe|map page'
az_run.ps1 -Scenario scripts/3ds/scenarios/flicker_race_legacy.txt -Mode standard -Extras 18 -Grep 'probe|map page'
```

Measured 20. November 2026 (Azahar; real hardware redraws are slower, so the blank stretches are longer there):

| | blank or half drawn samples | longest blank run | page redraws |
|---|---|---|---|
| front end, idle 8 s - **fix** | **0** (0.00 %) | 0 ms | 1 (the first one) |
| front end, idle 8 s - old behaviour | 63 (0.93 %) | 4 ms | 19 (every ~420 ms) |
| race start (load + map page) - **fix** | **0** (0.00 %) | 0 ms | 1 text + 1 map |
| race start - old behaviour | 21 (0.22 %) | 4 ms | many text + 1 map, painted twice |

Note: the sampler is a normal libctru thread (priority 0x18, 1 kHz) and preempts the main thread in the
middle of a redraw - which is why the old behaviour shows up at all; an all-zero legacy result would mean the probe
cannot see (check that first if the numbers ever look too good).

The minimap shows **every car of the field** now (it held four markers, so a 20 car race showed four): other cars in
their own car colour scaled to full brightness (`dr3_minimap_marker_colour` - the engine's colours are dark palette
values, near-black on the dark red road), the player white. Host tests: `test_minimap_marker_colour`.

`scenarios/newgame_flow.txt` walks Start Racing -> Start A New Game -> licence (name `BOB`) -> "Select difficulty:" with
**Chaos** (the fifth entry) -> the first race sign-up; the screenshots show the licence and the difficulty list.

### What the harness found and what was changed (20. November 2026, release build, track 4, chaos field)

| Phase | before | after | what |
|---|---|---|---|
| blit (index -> RGBA into the rotated framebuffer) | 4.37 ms | **1.53 ms** | `dr3_blit_rot_5_4`: 320 -> 400 is 4 source columns -> 5 target columns, so one group is read once and written five times |
| tint-scan (where are the car marks) | 1.44 ms | **0.36 ms** | `dr3_carcolour_mask_bounds`: word wise, empty rows cost 80 word reads instead of 320 byte reads |
| tint-mask (colour the car pixels), 20 cars | 2.7 ms (p95 6.5) | **0.44 ms** | walk the *mask* (marked pixels only, empty words skipped) instead of every target pixel of the cars' bounding box |
| 4 car race | 16.0 ms, 7.8% stutter frames | **14.0 ms, locked 71 fps, 0 stutter** | the frame work (14.75 ms) was over the 14.3 ms tick, so frames alternated long/short |
| 20 car race | 17.5 ms, 58 stutter frames (7.5%, up to 29 ms) | **15.2 ms, p99 16.8, 0 stutter** | a sustained slow stretch at the start, while the whole pack is on screen |

All three are portable functions in `dr3_blit.c` with host tests that compare **every pixel** against the original
algorithm on random data (`tests/test_dr3.c`: `test_blit_rot_5_4`, `test_mask_bounds`, `test_tint_mask_box`; 2765
checks, 0 failures) and a 20 car screenshot looks as before. The tint changes are inside the chaos pass, the blit one
applies to every 320 -> 400 present.

**Still on the table** (20 car race, ms/frame): `objdraw` 2.9 (constant, track objects), `pre` 2.6 (physics side),
`percar` 1.9, `shadow` 1.3, `tcopy` 0.8, blit 1.5. `objdraw` is the largest fixed cost left.

## How it works (three small pieces)

| Piece | File | Job |
|---|---|---|
| runner | `platform_3ds/dr3_scenario.c/.h` | reads `sdmc:/dr3_scenario.txt`, injects keys, takes screenshots, logs `[scn]` lines, asks the engine to quit |
| hooks | `dr3_input.c` (`dr3_poll_event` calls `dr3_scenario_tick`, new `dr3_input_inject_raw`), `dr3_fb.c` (`dr3_scenario_present` right before the flush) | the only two places in the port that know about it |
| launcher | `scripts/3ds/az_run.ps1` | copies the `.3dsx` + scenario to the emulated SD, starts `azahar.exe`, waits for `[scn] done`, kills it, **always deletes the scenario file again**, collects log + PNGs |

**The runner is compiled into every 3DS build but inert without the file** - one failed `fopen` at start-up. So
the build you test is the build that ships; there is no separate test build to keep in sync. Consequence: if
`sdmc:/dr3_scenario.txt` is ever left over, the game scripts itself (the launcher deletes it in a `finally`).

The emulated SD card is `%APPDATA%\Azahar\sdmc` (`sdmc:/` in the code). The launcher runs the build as
`dRally-3ds-test.3dsx` in that folder - the same folder as the player's `dRally-3ds.3dsx`, because the game finds
its data relative to its own path (`dr3_paths.c`). It does **not** touch the player's `.3dsx`.

## Scenario file format

One command per line, `#` starts a comment. A trigger is **`at <frame>`** (presented frames) or **`atms <ms>`**
(milliseconds since the first poll):

```
atms 3000 key Return tap          press + release (auto-release after DR3_KEY_HOLD_MS)
atms 5000 key Up down             hold ...
atms 9000 key Up up               ... and release
at 700 screenshot race1.bmp       saves sdmc:/dr3_shots/race1.bmp, PNG in the run folder
atms 100 log whatever text        writes "[scn] ... whatever text" into the log
quit_after_ms 52000               = atms 52000 quit
quit_after_frames 3000            = at 3000 quit
```

Key names are SDL key names (`SDL_GetScancodeFromName`, case-insensitive): `Return`, `Escape`, `Up`, `Down`,
`Left`, `Right`, `Space`, `Left Ctrl`, `F1`, letters... The engine reads keyboard scancodes, so these are the same
keys the pad is translated into (`dr3_input_map.c` - look there for which pad button is which key).

**Timed (`at`/`atms`) steps are the old style - prefer the sequential commands above.** If you do use them, pick the trigger by screen type:
* **Menus / intro / anything waiting for a key present almost no frames**, so a frame trigger stalls. Use `atms`.
* **Races present ~8 frames/s in Azahar with a 20 car chaos field** (the emulator is slow), so `atms 10000` is
  frame ~80 there. Use `at` when you need "N frames into the race" independent of speed, `atms` for wall time.
* A screenshot is taken by the **next present** after its trigger; the log line shows the real frame
  (`frame 307: screenshot e2.bmp -> ok`).

Every executed step is logged: `[scn] <ms> ms / frame <n>: key Return tap`. `[scn] done: ...` is written by the
`quit` step; the launcher waits for exactly that string. `dr3_log_write` is used, so `[scn]` lines appear even
when the chaos log switch (`sdmc:/chaos_log.txt`) is 0.

## Reading a run

* `[scn]` lines = what the script did and when. Compare the ms/frame columns: ~8 presents/s means a chaos race.
* The engine's own lines (`[dr3] chaos: ...`) are in the same `drally_3ds.log` **if** `sdmc:/chaos_log.txt`
  holds `1` (the log is silent when it holds `0`). Check it before expecting them. The `analyse_*.ps1` scripts in
  `logs/` read that same log.
* Look at the PNGs. A screenshot is the strongest evidence a change did (or did not) work - say what you saw.

## Building (what actually works on this machine)

`scripts/3ds/build_3ds.sh` points at `<repo>/third_party/SDL2-3ds-install`, which does not exist in the
`dRally-3ds` worktree - the SDL2 install lives one level up. Use:

```bash
MSYSTEM=MSYS /c/devkitPro/msys2/usr/bin/bash.exe -lc '
  export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM
  export PATH=$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH
  cd <repo> &&
  make -f Makefile.3ds -j8 SDL2_3DS_PREFIX=../third_party/SDL2-3ds-install'
# add  DR3_CHAOS=1  for build/3ds_chaos/dRally_3ds_chaos.3dsx  (az_run.ps1 -Build chaos)
```

Incremental builds take well under a minute. `Makefile.3ds` is generated by `scripts/3ds/gen_makefile_3ds.ps1` -
a new `.c` file goes into **both** (the generator's `EXTRA_OBJS` block and the Makefile).

## The loop for a change

1. Edit. 2. Build (above). 3. `az_run.ps1` with a scenario that reaches the code you changed. 4. Read the PNGs
and the `[scn]`/`[dr3]` lines. 5. Only then say it works. Host unit tests (`tests\build_tests.ps1`) still run
first for portable logic.

## Known limits

* Azahar timing is not the console's: ~8 fps in a 20 car race. Frame *counts* are comparable, ms are not.
* Keys only - no touch, no analog stick, no HOME menu. (The engine reads keyboard scancodes anyway.)
* Quitting goes through an `SDL_QUIT` event into the engine's own exit path; the launcher kills the emulator
  right after `[scn] done`, so a crash *inside* the exit path is not observable.
* A scenario that skips a screen it should have waited for fails silently in a *later* screenshot - always put
  a screenshot after each navigation step while writing a new scenario, and compare with what you expected.

## Ready-made scenarios (`scripts/3ds/scenarios/`)

| File | What it does |
|---|---|
| `perf_race.txt` | frame time of a race: `-Perf` -> percentiles, stutter frames, causes (see the section above) |
| `race_fast.txt` | **the template**: direct race start, event driven, 3 screenshots, ~15 s. Use with `-Mode standard|duel|dm|arena` |
| `arena.txt`, `arena_full.txt`, `arena_prize.txt` | the ARENA mode (`-Mode arena`, `doc/arena.md`): screenshots, a race to the report, a player healed onto the podium for the prize |
| `wheel.txt`, `wheel_race.txt` | the career flow into the sign-up wheel (`-Mode random`): the four cards in screenshots / the drawn mode into a race. Use the `raced` step to arm the wheel |
| `explore.txt` | taps Return through the intro, screenshots every 8 s (timed `atms` style) |
| `nav1.txt` ... `nav4.txt` | the menu route in steps: Start Racing -> Load Game -> shop -> market -> sign up -> race (slow, for menu testing) |
