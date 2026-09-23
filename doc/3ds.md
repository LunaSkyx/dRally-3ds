# Nintendo 3DS port

Branch `3ds`, based on upstream `920d85a`.  Windows and Linux are untouched.

    bash scripts/3ds/build_sdl2_3ds.sh     # once: builds SDL2 into third_party/SDL2-3ds-install
    bash scripts/3ds/build_3ds.sh          # release  -> build/3ds/dRally_3ds.3dsx
    bash scripts/3ds/build_3ds.sh debug    # profiler -> build/3ds_debug/dRally_3ds_debug.3dsx
    make -f Makefile.3ds DR3_CHAOS=1 -j4   # test     -> build/3ds_chaos/dRally_3ds_chaos.3dsx

`scripts/3ds/gen_makefile_3ds.ps1` generates `Makefile.3ds` from the upstream object lists.
`scripts/3ds/deploy_emulator.ps1` copies a build plus test saves to the emulator's SD card.
`tests/build_tests.ps1` runs the host tests (no 3DS toolchain needed); `tests/` also builds the
`-D__3DS__` check project for MSVC.  Runtime log: `sdmc:/drally_3ds.log`.

Platform code is in `platform_3ds/` (input, display, minimap, bottom screen, lap time text).  Where it
meets the engine, the things worth knowing:

* the working directory is not the game folder; the engine opens `ENGINE.BPA` relative to the CWD, so
  `dr3_paths.c` changes it
* the pad arrives as a joystick while the engine reads keyboard scancodes (`dr3_input.c`).  ZL/ZR are
  raw buttons 14/15 (libctru `KEY_ZL = BIT(14)`) - SDL passes the hid bits through as button indices
* an SDL renderer would block `SDL_GetWindowSurface`, so there is none: `__PRESENTSCREEN__` converts
  the 8-bit screen with the palette LUT and writes into the window surface (`dr3_fb.c` scales it)
* the two screens have different pixel formats - SDL sets both to RGBA8, libctru's console switches the
  bottom one to RGB565
* libctru's console caches its frame buffer, so the bottom screen stays single buffered
* the DSP always runs at 32728 Hz; the DOS default (22050) made the music play 1.48x too fast
* the 3DS has no F-keys, so quick save/load (F2/F3) and typed text needed port-side plumbing; the
  engine keeps only the last key of a frame, so typed characters go in one per frame
* the n3ds backend provides a gamepad mapping, so `dr3_input_init` opens the raw joystick as well -
  ZL/ZR do not exist in the controller API
* `main()` checks the game files before the engine boots (`dr3_files.c`): missing ones are listed on the
  bottom screen and a button press is waited for, because the engine would dereference a failed `fopen`
* the chaos test build (`-DDR3_CHAOS`) can put eight extra cars on the grid - for testing, not for playing.
  How many it adds is a *runtime switch* (`dr3_chaos_extra_cars()` in `platform_3ds/dr3_chaos.h`, implemented
  in `dr3_log.c`), so a fault can be told apart from the twelve car grid without rebuilding: it starts at
  `DR3_CHAOS_EXTRA_DEFAULT` (0, i.e. the race the game asked for), `SELECT + RIGHT` toggles it while the game
  runs, and `sdmc:/chaos_extras.txt` (one number, read at every race setup) flips it from the PC.  It takes
  effect with the next race, because that is when a race gets its size (`race_main.c`), the extra `s_54`
  entries and its grid.
  Four cars per race are baked into the BSS layout, so `bss.c` sizes `s_94`, `s_54`, `s_35e` and the car
  index tables with `DR3_MAX_CARS`; the extra grid entries in `___33010h.c` come from racers that are not
  in the race yet and stand in grid rows of their own behind the four start slots the track file provides
  (`race___49bd4h.c`), the in-race car list stops at `DR3_HUD_CARS` rows because its fourth row already
  ends at row 177 of the 200, and the bottom screen map draws every car of the race (`dr3_bottom.c`).
  Screens that hard-wire four cars need a look too: the results list (`___36068h.c`, drawn by
  `___38184h_cdecl`) fills its four rows from the first four cars only, so a twelve car race left rows
  unset and sent an arbitrary index into `s_6c[]` and the screen buffer - it now falls back to the grid's
  own seats and clamps whatever it is handed.
  `dr3_log.c`/`dr3_log.h` carry the build's step trace (`[dr3] chaos step ...`), which is what makes a
  hang outside the race loop (results, front end) readable from the log.  Two things that trace taught:
  the signup loops of the next event (`___33010h.c`, `___3079ch.c`) are unbounded in the original and a
  twelve car race can leave the racer pool without a candidate they accept - that is the black screen
  freeze (the palette is already down there), so the chaos build bounds them and logs what it had; and
  the trace itself has to flush every 32nd line instead of every line, because one SD flush per line
  costs more than the frame it traces (72 -> 18 presents/s)

Profiler build (`debug`): `-DDR3_PROFILE` measures the frame budget on the console and writes `STAT`
and `SUMMARY` lines to the log; `scripts/analyze_3ds_profile.ps1` turns them into a table.
