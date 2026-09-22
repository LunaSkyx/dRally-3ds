# Nintendo 3DS port

Branch `3ds`, based on upstream `920d85a`.
Windows remains the reference build - this branch does not touch it.

## Status (2026-09-21): runs on the emulator, playable

| | |
|---|---|
| Runs in **Azahar** | yes - boots, menus, name entry, in-game |
| Runs on real hardware | not tested yet (next step; `3dslink` or copy the `.3dsx` to the SD card) |
| Input | d-pad/circle pad steer, R accelerate, L brake, **front end:** A confirm + B select, **race:** A horn, B boost, Y shoot, X mine, ZL/ZR boost/shoot, START pause, SELECT opens the 3DS software keyboard, L+R+START quits (the mapping follows the display mode - see the table below) |
| Video | direct top-screen framebuffer output, double buffered, box-filtered downscale for the 640x480 menus |
| Speed | engine logic keeps its ~70 fps; presentation is the bottleneck (12-27 fps in the emulator). The emulator itself is a big part of that - measure on hardware |
| Sound | **works** - needs the DSP firmware `sdmc:/3ds/dspfirm.cdc` (Luma3DS provides it on real hardware; for Azahar copy it from your own console to `%APPDATA%\azahar\sdmc\3ds\dspfirm.cdc`). The mixer runs at the DAC's native 32728 Hz: the DOS default of 22050 Hz made everything play 1.48x too fast because the 3DS DAC does not resample |
| Known gaps | `___59720h` was ported (keyboard path only, joystick branches omitted); some menus/dialogs may still be unimplemented upstream (they print `TODO` and `exit(1)`) |

### Release package

`make -f Makefile.3ds` produces `build/3ds/dRally_3ds.smdh` (icon/title metadata) and embeds it into
the `.3dsx` via `3dsxtool --smdh=`. The ready-to-copy SD package (executable + original game data +
`README.txt`) is assembled next to the build output (executable + original game data + guide).
On the console the folder must end up as `sdmc:/3ds/drally/`.

### First profile run (Azahar, 268 MHz) - what it found

| measurement | MENU 640x480 | RACE 320x240 |
|---|---|---|
| `present` with filter + blit | 13826 us (blit 14540) | - |
| `present` filter off (nearest) | 4721 us (blit 4618) | - |
| `present` skip-blit (floor) | 340 us (flush 62, swap 1) | 178 us |
| `IO_Loop` | 2848 calls/s, 60 us each = **17% CPU** | same |
| audio | **22.5k of 32.7k frames/s, 11 stalls/s** | same |

Conclusions and fixes applied:

1. **Audio starvation had a clear cause**: SDL's n3ds backend paces its wave-buffer queue with
   `SDL_Delay(samples * 1000 / spec.freq)`, but the DAC always runs at 32728 Hz.  The device was
   opened with the DOS default (22050), so SDL waited 1.48x too long per buffer - exactly the
   22528/32728 ratio seen in the log.  `a.freq = DR3_DSP_RATE` when opening fixes the pacing (and
   with it the "music is a bit slow" symptom).
2. **The blit is the only real cost** - flush + swap together are 63 us, so there is nothing to gain
   there.  The filter was the expensive part (10 ms per menu frame); the common 1.6:1 case now uses
   the packed average of two converted pixels instead of three palette lookups and three
   multiplications per pixel.
3. **`IO_Loop` was called 2848 times per second** (the engine spins in its own wait loops) and each
   call pumped SDL and read the pad.  Polling is now limited to ~330 Hz, which is plenty for a 70 Hz
   game.
4. The frame time is now measured as wall-clock time between engine frames (the timer handler itself
   only takes ~2 us, so measuring that was useless).
5. `consoleInit()` on the bottom screen must not run before SDL's video driver called `gfxInit()` -
   doing so jumped into a NULL pointer inside libctru.  It is initialised lazily now.

### Bottom screen (release build)

The game never draws to the bottom screen, so `platform_3ds/dr3_bottom.c` uses it: the controls in
English on the left and the current driver standings on the right, in the style of the Death Rally
front end:

```
CONTROLS           TOP DRIVERS
------------------ --------------------
D-pad     steer    1   Sam            420
R         gas      2 * PLAYERNAME     380
L         brake    3   Farmer Ted     350
A         horn     ...
```

The list comes straight from the running game (`racer_t ___1a01e0h[20]`, `points` at +0x44, plus the
player index `___1a1ef8h`; see `drally_structs_fixed.h`): sorted by points, the player marked with
`*`, top ten shown, and if the player is not in the top ten his row is appended. Before a game is
loaded the list shows `(no game loaded)` - names are sanity checked so uninitialised memory cannot
show up as drivers.  It refreshes at most once per second and only when the text changed.

* the pad mapping is described with *functions* (GAS, BRAKE, BOOST, SHOOT, MINE, HORN, LEFT, RIGHT,
  PAUSE, CONFIRM, MENU_UP/DOWN/NEXT) and can be reassigned freely in `dr3_controls.txt` in the game
  folder: `FUNCTION = BUTTON[, BUTTON]`, with the analog stick directions usable as buttons
  (`GAS = R, UP, STICK_UP, B`).  A function belongs to the front end, a race or both, so the same
  button can confirm a menu entry and honk the horn (`platform_3ds/dr3_input_map.c`)
* tapping the bottom screen switches the controls/standings (or the profiler statistics) off and on
  again; the state is kept in `dr3_bottom_hidden`.  While a race is running the map is the only page
  there, so a tap just switches between **minimap and dark** - the standings page is not reachable
  during a race (see below)

* the engine's own `printf()` output is sent to the log file (`#define printf(...) dr3_log(__VA_ARGS__)`
  in drally.h, __3DS__ only): libctru's console makes stdout draw onto the bottom screen, and the
  decompiled leftovers - e.g. `[TODO] IN instruction for Joystick/Gamepad` fires repeatedly while
  racing - made the bottom screen flicker even when it was switched off.  `dr3_bottom.c` /
  `dr3_prof.c` undefine the macro again because they draw there on purpose

* the bottom screen is drawn with a **single** `printf()` per update and without clearing it: libctru's
  console redraws the screen for every `printf()`, so writing the block line by line (after a clear)
  was visible as flicker.  The release build prints a fixed height of padded lines (so removed lines are
  overwritten) and only when the content changed; the profiler overlay collects its output in a buffer
  (`dr3_out()`) and writes it once, including the clear, so the update is atomic

#### Minimap (bottom screen, race only)

A race uses that screen for something better: the whole track with every car on it, plus the lap times.
It is the only page there, a tap switches between map and dark.  Three console rows belong to it - the
track name with the running lap clock, the last/best/record lap and the race position - and the map is
painted into the band between them.

* the map is built **once per race** from the engine's own track mask (`TRX_MAS`, one byte per track
  pixel, low nibble = surface) and cached as a small bitmap of at most 320x224:
  `0xf` = hard surface (the racing line), `0x0..0x3` = soft ground, everything else = scenery.
  A `step x step` block is classified by the share of its groups, so roads much thinner than the
  step still survive (`step` is chosen so the map fits - 3..5 for the shipped tracks)
* the colours come from **the track itself**: the average of the track image (`TRX_IMA`) through the
  track's own palette (`___1a51d0h`, taken from `TRn-IMA.BPK`) is used for every cell, darkened a
  little so the racing line stands out, and the asphalt average is blended in proportionally to its
  share of the block (brightened by a quarter) - so a desert track looks sandy and a night track
  dark.  The palette range is 0..63 or 0..100 depending on the file, so the brightest entry is
  scaled to 255 first.  Without an image/palette the fixed scheme (`DR3_MAP_COL_*`) is the fallback
* the map keeps the track's aspect ratio (letterboxed and centred inside its band - `y=16`, 320x216:
  below the two text rows and above the footer - so a 1016x716 or 960x600 track is not stretched).
  Row 1 shows **the map's name** on the left ("Suburbia", "Downtown", ...) - the very names the front
  end uses (`___18d492h`), resolved from the loaded track id plus the reverse flag, because the second
  season half drives the same tracks backwards under different names (`race_main.c` does that lookup,
  the platform layer only prints it).  On its right the **running lap clock** ticks; as long as no lap
  time exists yet, that spot carries the tap hint instead - the hint is needed once, the clock every
  lap.  Row 30 below the map shows the position and the lap of the race
* the cars come from `struct_35e_t ___1e6ed0h[4]` (`XLocation`/`YLocation` in track pixels, `Lap`,
  `Position`); this page refreshes **eight** times a second instead of twice (`DR3_BOTTOM_MAP_MS`),
  because the lap clock lives on it
* **row 2 carries the lap times**: `LAST` (the lap that was just finished), `BEST` (the best lap of this
  race) and `REC` (the record of this car on this track).  All three come from the engine's own globals:
  `LAP_PREVIOUS_*` is the clock of the lap that is running (`race___40db4h.c` keeps updating it every
  frame, so it *is* the live lap time), `LAP_BEST_*` is zeroed when a race starts (`___33010h.c`) and
  `LAP_RECORD_*` is loaded from the lap record table for **the player's car** (the same `DR3_RECORD_CAR`
  wrap the seventh car needed) and overwritten the moment it is beaten.  The finished lap is also kept
  as a raw tick counter (`D(___243cb8h)`, 70 ticks per second), which `platform_3ds/dr3_laptime.c`
  converts with the engine's own arithmetic - what the screen shows is what the game would show.  A time
  that was never driven prints as `-:--.--`, so the three columns stay aligned, and a record that just
  fell turns the row into `*** NEW RECORD 0:39.80 ***` for three seconds - the engine plays
  `SFX_LAP_RECORD` for the very same moment
* no border is drawn any more, and the player marker is a rounded blob in **his own car colour**
  (the per-driver colours that the front end uses live in menu_main.c's `___1a0fb8h`, indexed by
  `racer_t.color`; yellow stays as the fallback) with a dark outline, so it remains readable on bright
  tracks.  The other cars are plain red dots
* the screen is deliberately **not** double buffered: libctru's console remembers the frame buffer it
  was initialised with, so with two buffers its text ends up in the buffer that is not being shown
  (the text flickered).  The calm screen comes from the drawing instead: the map is painted **once per
  race** and every update after that goes through `dr3_minimap_draw_incremental()`, which restores the
  few pixels the previous markers occupied from the map and then draws the new markers - only the two
  text rows are ever rewritten, and only when their content changed (with the cursor escape, never a
  `\x1b[2J`, which would also wipe the band)
* every race writes one summary line plus an ASCII preview into `sdmc:/drally_3ds.log`, so the
  classification can be checked from a log without looking at the screen:

  ```
  [dr3] minimap: TR7 'Holocaust' 1016x716 track -> 254x179 map (step 4): road 17625, soft 10171, other 17670, none 0, colours from the track image
  minimap preview:
  ::::....#######..::::::::::::::....:::::
  :::...############.::::::::::.........:
  ...
  ```

`platform_3ds/dr3_minimap.c` has no platform dependency at all - the framebuffer is described by a
small `dr3_canvas_t` with per-axis strides **and the pixel format** (RGBA8888 / RGB565 / BGR888), which
turns both the rotated 3DS buffer and the fact that the bottom screen is 16-bit into details instead
of special cases - and is unit-tested on the host, negative y stride and 16-bit writes included.  The
engine hooks are one line in `race_main.c` (behind the reverse-track mirroring, so a mirrored race
gets a mirrored map) and one in `race_memory.c` (the map is dropped before the track memory is freed);
outside the 3DS release build both compile to nothing (see `platform_3ds/dr3_bottom.h`).

The profiler build keeps that screen for the profiler (`-DDR3_PROFILE` disables `dr3_bottom`).

### Profiler build (autonomous measurement)

```
make -f Makefile.3ds DR3_DEBUG=1        # -> build/3ds_debug/dRally_3ds_debug.3dsx
```

`platform_3ds/dr3_prof.{c,h}` measure the whole frame budget with the ARM11 system tick, aggregate per
second and per display mode, rotate measurement variants on their own and write everything to
`sdmc:/drally_3ds.log` - no user interaction needed:

* phases are detected at the engine entry points (`menu_main`, `race_main`, audio rate changes)
* variants rotate automatically: menu 10 s `normal` -> 10 s `filter-off` -> 10 s `skip-blit`,
  races 15 s `normal` -> 15 s `skip-blit`
* slots: `present` (total) split into `blit` (our conversion + write), `flush`
  (`GSPGPU_FlushDataCache`) and `swap` (`gfxScreenSwapBuffers`), plus `game` (`IRQ0_TimerISR`),
  `IO_Loop`, audio `mix` and the state of the frame-time histogram
* the bottom screen (unused by the game) shows the live numbers; it costs ~1 ms per update and is
  counted separately
* the log gets one `STAT` line per second and a `SUMMARY` block with per-variant averages when a
  phase has run for 20 s or the phase changes ("MENU DONE" / "RACE DONE")
* `scripts/analyze_3ds_profile.ps1 -Log <file>` turns that log into a table

The release build is untouched: without `-DDR3_PROFILE` every profiler call compiles away.

### Things learned the hard way (all handled in the code)

1. **Working directory** - the engine opens `ENGINE.BPA` etc. relative to the CWD; on the 3DS that is
   not the game folder, so the game crashed right after startup. `platform_3ds/dr3_paths.c` fixes it.
2. **Azahar pauses the app at start** (`Debugging_DelayStartForLLEModules`) - with an empty NAND this
   leaves *every* homebrew on a black screen. Set `delay_start_for_lle_modules=false` in
   `%APPDATA%\azahar\config\qt-config.ini` (Azahar must be closed while editing, it rewrites the file
   on exit).
3. **The engine keeps only the last key of a frame**, so typed text must be delivered one character per
   frame, and a dialogue needs exactly one key per press (that is why A emits a single RETURN).
4. **SDL's n3ds present path** copies the window surface pixel by pixel into the rotated hardware
   buffer - far too slow, hence the direct gfx output (with double buffering, otherwise it tears).
5. **Debugging without a console**: Azahar is a GUI application, so guest stdout is lost;
   `platform_3ds/dr3_log.c` writes `sdmc:/drally_3ds.log`, which is readable from the PC.
6. **The two screens do not share a pixel format.**  SDL's n3ds driver calls
   `gfxInit(GSP_RGBA8_OES, GSP_RGBA8_OES, false)`, but `consoleInit(GFX_BOTTOM, ...)` switches the
   *bottom* screen to `GSP_RGB565_OES` (2 bytes per pixel) because libctru's console has no 32-bit
   mode.  The direct top screen output (`dr3_fb.c`) is therefore 4 bytes per pixel while the bottom
   is 2 - and writing 32-bit pixels there covers **two** screen pixels per store, which shreds the
   image and, with a `+239` base offset in 4-byte units, wraps content all over the screen (the first
   minimap build looked exactly like that).  `dr3_minimap.c` now carries the pixel format in its
   `dr3_canvas_t` (`RGBA8888` / `RGB565` / `BGR888`) and the glue asks `gfxGetScreenFormat()`; the
   geometry line in the log (`[dr3] bottom fb 240x320 fmt 2 (2 bytes/px, canvas fmt 1) ...`) makes
   the layout verifiable without a screenshot, and a host test asserts that a 16-bit write does not
   touch the neighbouring pixel.
7. **libctru's console caches its frame buffer** (it fetches it once in `consoleInit`), so double
   buffering a screen the console draws on makes its text land in the buffer that is *not* being
   shown.  The bottom screen therefore stays single buffered (the top screen has no console and is
   double buffered in `dr3_fb.c`), and the calm updates come from not repainting: the minimap is
   drawn once per race, afterwards only the car markers are restored and redrawn.

## Why there is no SDL shim

SDL 2.30.11 already ships a **native Nintendo 3DS backend** - `src/video/n3ds` (GSP framebuffer),
`src/audio/n3ds` (ndsp), `src/joystick/n3ds` (hid), `src/thread/timer/file/filesystem/power/sensor/
locale/main/n3ds`, plus `docs/README-n3ds.md` and CMake support. devkitPro does *not* package SDL2
for the 3DS (only `3ds-sdl` = SDL 1.2), so SDL2 is built from source here.

Consequences that shape this port:

| SDL2-3DS fact | Effect on dRally |
|---|---|
| only the **software renderer** exists | the hot path is index8 â†’ 32-bit conversion; `platform_3ds/dr3_blit.c` does it via a palette LUT (unit-tested) |
| frame-buffer driver (`CreateWindowFramebuffer`) | presenting via a cached streaming texture (as the PS Vita port does) is the plan for the display patch |
| `SDL2main` needed for ROMFS | `LIBS := -lSDL2 -lctru -lm` plus the 3DS rules |
| **cooperative threading on one core** - a thread only yields on `SDL_Delay` / blocking waits | the Vita port's "remove all `SDL_Delay`" patch must NOT be copied blindly: the engine's sound thread would starve. Keep a small yield |
| New 3DS clock boost + extra L2 cache on by default | good for the frame budget; the old 3DS remains the risk case |
| joystick backend reports **buttons**, not keys | `platform_3ds/dr3_input.c` turns the pad into the SDL scancodes the engine expects |

## What was added on this branch

| File | Purpose |
|---|---|
| `Makefile.3ds` | devkitPro/3DS build. **Generated** by `scripts/gen_makefile_3ds.ps1`, which copies the object lists verbatim from the upstream `Makefile` |
| `platform_3ds/dr3_input.c` / `.h` | wraps `SDL_PollEvent`: pad â†’ synthetic `SDL_KEYDOWN/UP` events, plus the `L+R+START` quit combo |
| `platform_3ds/dr3_input_map.c` / `.h` | the button â†’ scancode table (data only, unit-tested) |
| `platform_3ds/dr3_blit.c` / `.h` | palette â†’ 32-bit LUT and an integer-only nearest-neighbour / centred scaler (unit-tested) |
| `platform_3ds/dr3_bottom.c` / `.h` | the bottom screen: controls + driver standings, the minimap page with its lap clocks and the tap handling (libctru console, plus direct framebuffer pixels for the map) |
| `platform_3ds/dr3_minimap.c` / `.h` | track mask -> classified minimap bitmap, canvas drawing, ASCII log preview; no platform header, fully unit-tested |
| `platform_3ds/dr3_laptime.c` / `.h` | lap times as text for the bottom screen (`m:ss.cc` from the engine's minute/seconds/hundredths triples and from a raw tick counter, using the engine's own arithmetic); no platform header, unit-tested |
| `platform_3ds/sdl2_net_stub/` | inert SDL_net so the multiplayer code compiles and links |
| `tests/test_dr3.c`, `tests/Dr3Tests.vcxproj`, `tests/build_tests.ps1` | host unit tests (459 checks), runnable **without** a 3DS toolchain |
| `events.c` | engine patch 1: `while(dr3_poll_event(&e))` under `#if defined(__3DS__)` |
| `drally_linux_c.c` | engine patch 2 (display): window created as the fixed 400x240 top screen, **no SDL renderer**, `__PRESENTSCREEN__` converts the 8-bit screen with the palette LUT straight into the window surface and calls `SDL_UpdateWindowSurface`; `SDL_SetWindowSize` calls are skipped |

### A fourth difficulty: "pedal to the metal"

The original ships three levels (the quirky names live in `___18768ah`, the hall of fame shows them);
this branch adds a fourth one, one step above "petrol in my veins", for when even that gets too easy.

| Where | What changed |
|---|---|
| `___3ab5ch.c` | the "Select difficulty:" dialog: a fourth row at `y+0x9e`, the down key stops at `NUM_OF_DIFFICULTIES-1`, the frame grew from `0xba` to `0xd6` (so the highlight cannot run into the bottom border) and the repaint block covers `0x70` rows instead of `0x54` |
| `race___3f970h.c` | the AI parameter tables grew from 4 to 5 rows: rows 0..3 are the four levels, row 4 stays `MY_DIFFICULTY` - the row of the **player's own car**.  The new row is the "petrol" row with more top speed (+0.15), a sharper steering rate and noticeably more armour (see the field table below) |
| `___33010h.c` | the player's car still gets `MY_DIFFICULTY` - now 4, it was a hardcoded 3.  **This has to follow the enum**, otherwise his car would silently use the new AI row and every difficulty would feel different |
| `race___4c21ch.c` | the rubber band tables are indexed by `2*difficulty`, so they grew from 6 to 8 values |
| `menu_data.c` | the name for the hall of fame (`___18768ah[3]`) - without it that screen would read past the table |
| `config_c.c` | the enum, so both ends of the config agree |

Everything else works unchanged: the level is picked and saved like the other three (`dr.cfg` gets
`difficulty = 3`), and the hall of fame, savegames and quicksaves all store it as a plain number that
nobody validates.  It has no jingle of its own in the game data, so it plays `SFX_LETS_ROCK`.

| Field | Set from | What it does in the race | petrol -> pedal |
|---|---|---|---|
| `+4` / `+8` | `___3f1f0h_floats[diff][car][engine]` | the car's **top speed**: it scales the per-frame acceleration (`__b0 += 0.8*F32(+4)/30`) and the terminal speed | **+0.15** per value |
| `+0x14` | `3.75 / (___3f5b0h_floats[diff][car] - 0.05*engine)` | becomes `s_35e.__a8`, the **steering rate**: `Direction += __a8` per frame | 1.60..1.20 -> **1.50..1.10** (sharper) |
| `+0x1c` | `___3f610h_ints[diff][car]` + `___3f670h_ints[diff][armour]` | **armour / hit points** - the damage taken is `(0x400 - armour)*...`, capped at 900 | base **+15..+30**, per armour level **+50..+60** |
| `+0xc` | `___3f3d0h_floats[diff][car][engine]` | the cornering/slide factor (`__bc`), unchanged on purpose | = petrol |
| `+4` (again) | `race___4c21ch.c`, indexed `2*difficulty` | the **rubber band**: a catch-up boost when the AI is behind, a slow-down when it leads | boost 0.18/0.32 -> **0.20/0.36**, slow-down 0.03/0.06 -> 0.03/0.05 |
| `+4` | `race___3f970h.c` adversary block | the SPECIAL's own top speed, per difficulty | 4.5/4.7 -> **4.6/4.8** |

The player's own car always uses the `MY_DIFFICULTY` row plus its `+0x64` (100) armour bonus, so nothing
above touches him.  Everything else that reads `___196a94h_difficulty` is bookkeeping (config, hall of
fame, savegames) - the difficulty has no other effect on the race.

**Tuning** is one table row in `race___3f970h.c`: `___3f1f0h_floats` (top speed), `___3f5b0h_floats`
(steering rate, smaller is sharper), `___3f610h_ints` (armour) and `___3f670h_ints` (armour per upgrade
level); the cornering table `___3f3d0h_floats` deliberately keeps the "petrol" values.  Renaming means
the string in `___3ab5ch.c` (twice - the dialog and its repaint) and `menu_data.c`.

Because it is engine code, the level is there in every build of this branch, not only on the 3DS.

### The adversary on the fourth difficulty

What the level does to the *opponents* is the difficulty table above.  On top of that the adversary -
the black SPECIAL car - stops being a guest: he becomes a full championship participant, and the final
challenge is offered to the runner-up instead of to the leader.

| Where | What |
|---|---|
| `drally.h`, `bss.c` | `DR3_DIFFICULTY_ADVERSARY` (3), `DR3_ADVERSARY_CAR` (6), `DR3_ADVERSARY_RACER` (18), `DR3_ADVERSARY_START_RANK` (3) and `dr3_adversary_active()`.  The active test is `dr3_adversary_wanted || difficulty == 3`: the flag is set when the player picks the level in the licence screen, so a config re-read or a defaulted config cannot switch the feature off behind our back (that made him stop joining races) |
| `___3ab5ch.c` | picks that flag up where the difficulty dialog sets `___196a94h_difficulty` |
| `car_data.c` | a seventh `cardata_t` (row 6, the adversary's car) and a seventh `CARENCS` record - without them every screen that looks up `___18e298h[s_6c[x].car]` would read past the table.  He is not for sale: the shop list stays at six |
| `___31588h.c` | right after the event's field, flags and counters are cleared, `dr3_adversary_enter()` makes sure he exists before the lists are filled (the seeding is a one-off, his car is the marker) |
| `___3079ch.c` | the picker offers him as a **candidate like any other racer**, and he is *drawn* there - which matters, because a signup list is only ever drawn when a racer is picked: an entry added after the filling would be in the grid but invisible, the player would never pick that race and he would collect nothing.  "One race per event" is a **status flag of its own** (`dr3_adversary_placed`, cleared whenever a signup starts), *not* a look at the field: a field that still held his entry from the race he had just driven made this code think he was entered already - and he stopped joining races for good.  His seat is not fixed (the roster is sorted by points after every race, `___30a84h`), so he is found through his car - with his **name as a fallback**, which also repairs the car if a rebuilt roster kept only the name |
| `___38184h.c` | the standings draw each racer's car picture out of `carres.bpk` (`0x5140` bytes per car, the file being `0x1e780` = six cars).  Car 6 has no picture of its own, so `DR3_ADVERSARY_CAR_PIC` (5 - the Deliverator, the SPECIAL being its black twin) is used instead of reading past the end of that table; the face index is clamped to `face01..face20` as well.  For the adversary the draw uses `dr3_adversary_car_pic()`, a private copy of that picture darkened to `DR3_ADVERSARY_DARK` percent - the original must stay untouched, the player may be driving the Deliverator |
| `___3079ch.c` (his pictures) | his **driver picture** is rewritten once: every pixel index is replaced by the index of the closest neutral grey of its brightness, scaled down by `DR3_ADVERSARY_DARK` percent - so he ends up black.  Because every screen reads the same picture (signup roster, licence screen, standings), no per-screen hook is needed.  The car picture gets the same treatment in `dr3_adversary_car_pic()` |
| `race___3f970h.c` (boss) | after the per-car setup (and after the slot 0 block, which gives the race entry its own top speed) his top speed is multiplied with `DR3_ADVERSARY_SPEED` and his steering rate with `DR3_ADVERSARY_STEER` - he is a little quicker than the tables say.  The player is skipped, in case he ever drives the SPECIAL himself |
| `race___42824h.c` | every car sprite gets its colour from its grid slot (`index += 0xa*slot` in a 10-entry palette ramp).  The adversary is exempt: his car is the black one and stays black |
| `___3266ch.c` | the signup: once the field is complete and the player confirms a tier, `dr3_adversary_ensure()` only checks that he is registered in **one** of the three races - the picker normally did that while the lists were filling up, and the race he was entered in is the one he drives, whether or not the player picks it.  Only if the picker missed him completely is he put into a grid, and then preferably one of the *other* two races: meeting him is meant to be the hard race, and that stays the player's own choice |
| `race___3f970h.c` | car 6 has no row of its own in the parameter tables, so he borrows the Deliverator's (`const int car = ...` in the setup loop) and got his own two-gun entry in the car-body chain.  Grid slot 0 still adds the SPECIAL's own top speed (4.5-4.7) |
| `___3079ch.c` | the randomised race field is a *candidate list* per slot: his car is outside every class range it tests, so he is offered as one candidate for the first slot.  The original "picked" flag then keeps him to one race of the event - the same rule that applies to every other racer |
| `___33010h.c` | the old rule ("the leader's race entry turns into the adversary") is switched off on this difficulty - he is in the race anyway |
| `___33010h.c` (the end game) | the video + hall of fame (`___22808h`) used to run when the player finished a race first *and* led the championship - the finished position is what `___196ae8h` holds.  On this level that is not enough any more: the anniversary has to be beaten in the final challenge first, and that duel is the only two-car race there is.  So on lower difficulties and in the duel the old rule still holds |
| `shop___28e40h.c`, `underground___2e350h.c` | the final challenge is offered **from the top of the table on** - first place (you are the champion, now face him) or second place, right behind him.  Below that it is a normal signup, as the anniversary is meant to lead the championship |

His points come from the normal race results (he is an ordinary roster entry, so nothing else had to
change), and because the roster gets sorted by points he is identified by his **car**, never by his seat
number.  The roster is built when a game starts, so an **existing** save keeps the roster it has - the
adversary appears in games created on this difficulty.

### The Anniversary you can buy

The boss's car - the SPECIAL - is in the shop's car list for **$100,000**, upgrades included.  What makes
the boss special is not the car but the level he is met on: he is black and a little stronger there,
while the one you buy behaves like the best car of the game.

| Where | What |
|---|---|
| `car_data.c` | the seventh `cardata_t` is the car on sale: name, price 100000, its own upgrade prices and a seventh `CARENCS` record |
| `___24548h.c` | `___1a01b8h[6]` gets the Deliverator's picture (MENU.BPA has no SPECIAL picture) - the very pointer of `[5]`, so `___12200h.c` must not free it twice.  The shop's default selection clamps to `DR3_LAST_CAR` |
| `shop___2836ch.c` | the car carousel walks seven cars instead of six |
| `shop___28e40h.c` | the "next car" offer stops at the Anniversary |
| `race___3f970h.c` | it uses the Deliverator's parameter row and its own two-gun entry, and it does **not** get the boss's speed bonus - that is exactly what makes it "just a little slower" |
| `race___42824h.c`, `___38184h.c` | only the *boss* keeps the black colours and the darkened picture; your own Anniversary gets its slot colour like every other car |
| `drally.h` (`DR3_RECORD_CAR`) | the lap-record table has six rows and lives in `dr.cfg`, so its size must not change: an Anniversary owner shares the Deliverator's row |

### Why the multiplayer code stays in

Removing the multiplayer objects was tried and **fails to link**: menus, race code and the chat box
reference multiplayer symbols unconditionally (`___61278h`, `___61518h`, `___618c4h`, `npg_zero`,
`npg_peekb`, `npg_override`, `dRChatbox_clear/getFont/getLine`, plus data from `__mp_data.c`) -
20 unresolved externals. The inert SDL_net stub keeps the object list identical to the working
Windows configuration; multiplayer simply cannot connect (it could not on PC either).

## Controls

| 3DS | Effect (DOS key it synthesises) |
|---|---|
| D-pad / circle pad / c-stick | steer (`LEFT`/`RIGHT`, `KP_4`/`KP_6`), up/down also accelerate/brake (`A`/`Z`) |
| R | accelerate (`A`) |
| L | brake / reverse (`Z`) |
| A | front end: confirm (`RETURN` - exactly one key so dialogues see it)   /   race: horn (`SPACE`) |
| B | front end: select (`SPACE`)   /   race: **boost** (`LSHIFT`) |
| Y | race: **shoot** (`LCTRL`)   /   front end: **quick load** (`F3`, works on every 3DS) |
| X | race: **drop mine** (`LALT`)   /   front end: **quick save** (`F2`, works on every 3DS) |
| ZL / ZR (New 3DS) | race: boost (`LSHIFT`) / shoot (`LCTRL`)   /   front end: quick save (`F2`) / quick load (`F3`) |
| START | pause / back (`ESCAPE`) |
| **SELECT** | opens the **3DS software keyboard** (type player names, save slots) |
| **L + R + START** | quit |

The mapping follows the display mode: VESA101 (640x480) is the front end - where A confirms and B
selects - and VGA13 (320x240) is a race, where the same buttons become horn, boost, shoot and mine.
The engine tells the input layer which of the two is active (`dr3_input_set_context`).

**Quick save / quick load** is the front end's own feature (`___2a6a8h.c`, F2/F3 on the PC; the shop and
the underground call it every frame).  The 3DS has no F2/F3, so the whole feature was unreachable until
`QUICKSAVE`/`QUICKLOAD` were added to the function table.  They write and read `DR.SG7`, which the load
menu shows as slot 7.  It stays a front end feature on purpose - a race in progress is not part of a
save game.

**Typed text** (SELECT) is fed into the engine one character at a time, and each key is held down until
the engine really has read it (both of its keyboard latches in `keyboard.c` are empty again) instead of
being released after a fixed delay.  While a character is on its way the pad stays silent: pad keys go
through the very same `dRally_Keyboard_make()` and used to overwrite the character latch the dialogue
was waiting for - that is what ate the first and the last letter of a typed name.  Characters that sit
on a shifted key (`!`, `?`, `_`, `:`, ...) are sent as shift + the key below them (the engine then takes
them from its `upper[]` table), umlauts are transliterated (`Müller` becomes `MULLER` instead of
`MLLER`) and a character the engine's character table cannot express at all is skipped *and logged*, so
the log says why something is missing.

The defaults are dRally's own (`config_c.c`): accelerate `A`, brake `Z`, arrows steer, turbo
`LSHIFT`, horn `SPACE`, mine `LALT`, machine gun `LCTRL`. Covered by
`tests/test_dr3.c::test_input_map` and `test_text_and_filter`.

## Building

```bash
# 1. devkitPro with devkitARM + libctru (3ds-dev, 3ds-cmake, 3ds-pkg-config)
# 2. SDL2 with the n3ds backend (devkitPro ships no SDL2 for the 3DS)
git clone --depth 1 https://github.com/libsdl-org/SDL third_party/SDL
bash scripts/3ds/build_sdl2_3ds.sh          # -> third_party/SDL2-3ds-install

# 3. the game
bash scripts/3ds/build_3ds.sh               # -> build/3ds/dRally_3ds.3dsx
bash scripts/3ds/build_3ds.sh all           # ... and the profiler build
```

`Makefile.3ds` is generated (`powershell -File scripts/3ds/gen_makefile_3ds.ps1`); it reuses the
upstream object lists verbatim, so the 3DS build compiles exactly the same sources as the other
platforms.  Both scripts honour `DEVKITPRO`, `SDL_SRC` and `SDL2_3DS_PREFIX` from the environment.

## Running

* **Emulator (no hardware needed):** install Azahar (Citra fork, `winget install AzaharEmu.Azahar`),
  load `build/3ds/dRally_3ds.3dsx` and point its SD-card folder at a directory containing the game
  data.  If nothing shows up, check that `delay_start_for_lle_modules` is `false` in Azahar's
  `qt-config.ini` - otherwise every homebrew stays on a black screen without system files.
* **Real hardware:** `3dslink build/3ds/dRally_3ds.3dsx` pushes and boots it over WiFi (needs a
  homebrew-enabled 3DS), or just copy the file to the SD card.
* The port writes `sdmc:/drally_3ds.log` - the first place to look, since the 3DS has no console.

Data layout (`sdmc:/3ds/drally/` or the emulator's SD root). The original game files are required
and are **not** shipped:

```
dRally_3ds.3dsx
ENGINE.BPA  IBFILES.BPA  MENU.BPA  MUSICS.BPA  TR0..TR9.BPA
CDROM.INI                 <- contains: ./CINEM
CINEM/ENDANI.HAF  ENDANI0.HAF  SANIM.HAF
```

### Ready-made test saves

`scripts/gen_3ds_saves.ps1` writes four save games (raw `saved_game_t`, 0x883 bytes, encoded exactly the
way the game encodes them when it saves) into the game folder on the emulator's SD card, into
`build/3ds` and into the release package.  `scripts/3ds/deploy_emulator.ps1` runs it after every build,
so the saves are always next to the `.3dsx`.

| File | Name | Difficulty | Money | Table |
|---|---|---|---|---|
| `DR.SG1` | MILLIONS | 3 - 30th Anniversary | 5 000 000 | 60 points, 5th place - the anniversary races along, so a normal signup |
| `DR.SG2` | CHAMPION | 3 - 30th Anniversary | 5 000 000 | 200 points, 1st place - he is the champion, the challenge is offered |
| `DR.SG3` | FINAL RACE | 3 - 30th Anniversary | 5 000 000 | 110 points, 2nd place, one racer ahead (150) - the challenge is offered |
| `DR.SG4` | EASY MONEY | 0 - speed makes me dizzy | 5 000 000 | 60 points, 5th place - a plain championship, no anniversary in the field |

The season itself is not part of a save game (only difficulty, weapons and the racers are), so a loaded
save simply continues wherever the running session stands.  What the save sets is the money, the points
and the places - which is enough to test the final challenge: `DR.SG2` and `DR.SG3` are near the top.

## Why there is no SDL renderer on the 3DS

`src/video/n3ds` only registers a *frame-buffer* driver (`CreateWindowFramebuffer` /
`UpdateWindowFramebuffer`) - there is no accelerated renderer. Two further details from that driver:

* The hardware frame buffer is **rotated** (240x400 internally, `src/video/n3ds/SDL_n3dsframebuffer.c`
  uses `GetDestOffset(x, y, dest_width) = dest_width - y - 1 + dest_width * x`) and SDL copies the
  window surface into it, so we only have to fill the window surface - orientation is free.
* `SDL_GetWindowSizeInPixels()` defines the size of that surface, and the copy clamps to
  `min(hardware, surface)`. That is why the window is created with 400x240 and why the
  `SDL_SetWindowSize(1024x768)` calls are skipped on the 3DS - resizing would make SDL copy a
  cropped region.

An active renderer would also block `SDL_GetWindowSurface`, which `__PRESENTSCREEN__` writes into,
so the renderer is not created at all on the 3DS.

## Verification (all runnable without a 3DS toolchain)

| Check | Command | Result |
|---|---|---|
| Portable logic | `tests\build_tests.ps1` (MSVC) | **459 checks, 0 failures** - LUT byte order + masks (`SDL_PIXELFORMAT_RGBA8888` as used by the 3DS), centred/scaled blit pixels, full pad â†’ scancode map, quit combo |
| Minimap against the real tracks | `logs\minimap_probe.c` (local throwaway host tool, not committed: `old_bpa_read` + `bpk_decode4` + `dr3_minimap_build`) | reads `TR*.BPA` from the original game data, prints the class shares and an ASCII preview - `TR7 1016x716 -> 254x179 (step 4), road 17625 / soft 10171 / other 17670` and `TR1 960x600 -> 320x200 (step 3)`, both previews show a recognisable circuit |
| Whole engine with `-D__3DS__` | `scripts\gen_3ds_check.ps1` â†’ `tests\dRally3DSCheck.vcxproj` | **334 translation units compile and link** (exit 0) - validates every `#if defined(__3DS__)` path with the real SDL2 headers, catching typos/prototype errors before devkitARM exists |
| Windows regression | `scripts\build_windows.ps1 -GameDir dRally-3ds -SkipDeps -SkipStage` | still builds (exit 0) |
| Emulator | `Azahar` in `C:\Program Files\Azahar` | installed, ready for the first `.3dsx` |

Build and analysis scripts (in `scripts\/3ds`):
`gen_makefile_3ds.ps1` (Makefile from upstream lists), `patch_3ds_display.ps1` (display patch,
whitespace tolerant + idempotent), `gen_3ds_check.ps1` (`__3DS__` syntax/link check), plus
`build_windows.ps1` / `run_windows.cmd` for the reference build.

## Roadmap

1. ~~branch, makefile, input layer, host tests~~ (done)
2. ~~display patch: renderer-free present path + `dr3_blit` LUT with SDL masks, fixed 400x240
   window~~ (done, verified by the `-D__3DS__` link check)
3. Software keyboard (`SDL_n3dsswkb.c`) for the driver-name / save prompts.
4. Audio sanity check (ndsp) and frame-time measurement; the old 3DS may need a reduced frame rate.
5. Make `cinem.c` (HAF cinematics) skippable - CPU heavy.
6. Packaging (`*.3dsx`, optional `.cia`) and a user-facing README.
7. First real 3DS run: `make -f Makefile.3ds` with devkitARM, then Azahar / `3dslink`.

## Open blocker

`devkitpro.org`, `registry-1.docker.io` and `ghcr.io` are unreachable from this machine, so
devkitARM/SDL2 cannot be fetched or built here yet. Options: VPN/proxy, another machine/network, or
a GitHub Actions workflow (the `dRally-vita` fork has CI examples). See `docs/NETWORK_NOTES.md`.
