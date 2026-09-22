# Nintendo 3DS port

Branch `3ds`, based on upstream `920d85a`.  Windows and Linux are untouched.

    bash scripts/3ds/build_sdl2_3ds.sh     # once: builds SDL2 into third_party/SDL2-3ds-install
    bash scripts/3ds/build_3ds.sh          # release  -> build/3ds/dRally_3ds.3dsx
    bash scripts/3ds/build_3ds.sh debug    # profiler -> build/3ds_debug/dRally_3ds_debug.3dsx

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

Profiler build (`debug`): `-DDR3_PROFILE` measures the frame budget on the console and writes `STAT`
and `SUMMARY` lines to the log; `scripts/analyze_3ds_profile.ps1` turns them into a table.
