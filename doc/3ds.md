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

A change is built and tested on the PC first: `doc/host_harness.md` builds the same sources for Windows
(`scripts/3ds/gen_host_project.ps1 -Build`), drives the front end and a race from a script, and lists what
it cannot cover - only the 3DS screen output and timing stay an emulator test.  Coding agents start at
`AGENTS.md`.

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
* the ARENA chaos mode (own map `TRA.BPA`, hunting AI, top-3 prize money, a card of the sign-up wheel) is described in `doc/arena.md`.
* the chaos test build (`-DDR3_CHAOS`) can put the whole standings on one grid - for testing, not for playing.
  How many cars it adds is a *runtime switch* (`dr3_chaos_extra_cars()` in `platform_3ds/dr3_chaos.h`, implemented
  in `dr3_log.c`), so a fault can be told apart from the bigger field without rebuilding: it starts at
  `DR3_CHAOS_EXTRA_DEFAULT` (18, i.e. every driver of the roster - the race's own four plus the rest,
  twenty on the grid).  How many
  are comes from `sdmc:/chaos_extras.txt` (one number, read at every race setup), and how much of an extra
  car is drawn comes from the *ladder* (0 = nothing of the extra cars is drawn at all, 1 = + sprite, 2 = +
  lights, 3 = + weapon and tyre effects), which takes things apart piece by piece when a fault has to be
  pinned to one of them; the harness script switches it (`stage <0..3>`, see `doc/host_harness.md`).  In game
  `L + R + X` is not the ladder but the *pass switch*: it walks the drawing passes of a race frame and leaves
  one of them out (`dr3_input.c` -> `dr3_chaos_skip_cycle()`, logged as `[dr3] chaos: PASS SWITCH`).  The ladder
  takes effect with the next race, because that
  is when a race gets its size (`race_main.c`), the extra `s_54` entries and its grid.
  The build's log is off unless `sdmc:/chaos_log.txt` says `1`: with `0` (the default) nothing is written at
  all and the expensive diagnostics - the frame dumps, the stripe checksums, the band measurement and the
  step trace - are not even computed.  The file is read at every race setup, so the full diagnostics can be
  switched back on from the PC while the game runs (`platform_3ds/dr3_log.c`).
  Four cars per race are baked into the BSS layout, so `bss.c` sizes `s_94`, `s_54`, `s_35e` and the car
  index tables with `DR3_MAX_CARS`; the extra grid entries in `___33010h.c` come from racers that are not
  in the race yet, they share the four start slots the track file provides (`race___49bd4h.c`), and
  `platform_3ds/dr3_grid.c` moves them onto the road: from the rear-most of the four slots a walk steps
  down the *middle* of the road (the hard surface 0x0f of the track mask - the classification of
  `dr3_minimap.h`) and hands back one spot every 52 px, always 48 px clear of every car placed before it,
  and it carries on in front of the grid when the road behind runs out.  Every step is verified against
  the track's own mask, so the cars stand on the road on every track; the placement runs once per race
  setup in `race___42824h.c`, *outside* the 256 step swap loop it used to sit in.  Which car gets which
  place follows the *standings*: `___33010h.c` keeps the rank of every racer of the field
  (`dr3_chaos_car_rank`) and derives the grid order from it (`dr3_chaos_car_pos`, rank 1 = place 0, via
  `dr3_grid_order_by_rank` in `dr3_grid.c`), the four start slots go to the first four places, the walk
  spots are handed out in the same order and `race___4a25ch.c` starts every car with that position - so
  the field lines up the way the standings read, and the log names the grid place by place.  The
  colours of the cars need the same kind of care, but the other way round: the palette has room for four
  ramps of ten entries (`0x0f..0x36`, `___4a064h.c`) and the tracks plus their textures already use
  180..215 of the 256 entries (measured, `logs/palette_probe.c`), so twenty cars cannot have ten shades
  each *in the palette*.  They do not have to: every car sprite keeps the palette's own ramp - all ten
  shades, exactly as the four car race has them - and the *port* re-colours those pixels with the colour
  of each car's driver while it presents the frame.  The engine only reports where each car was drawn
  (`dr3_chaos_car_draw`, filled in `race___4f300h.c`, read in `platform_3ds/dr3_fb.c`), the shades come
  from `dr3_carcolour_build()` (the same arithmetic as `___4a064h_cdecl()`) and `dr3_carcolour_tint()`
  writes them into the rotated framebuffer - so every driver has his own colour with the sprite's
  shading untouched, and the palette (and with it the track, its textures and every other sprite) is not
  changed at all.  The *four* colour ramps the left panel needs are built all the same: the bar behind a
  driver's name is part of that row's own panel picture and is drawn with the ramp of the car in the row
  (`race___4a064h_cdecl`, `10*n+15`), and the rows are the cars 0..3 with the player's left out
  (`race___405bch.c`; the panel pictures are decoded for exactly those cars in `race___42824h.c`), so
  `race___4a25ch.c` builds one ramp per row from the race entry's colour (`s_54 +0x34/+0x38/+0x3c`) - the
  same thing the four car build does.  Building only the first ramp (the "one ramp is enough, the port
  re-colours the sprites anyway" shortcut) left the bars of the other rows at whatever the *track* palette
  holds at `0x19..0x36`; on TR6 that is `0x3f003f`, so those rows showed **magenta name bars** - the fault
  reported on 27. September 2026.  Checked on TR6 itself (`logs/run_pinkbars2.ps1`, the log's `chaos: ramp
  1..3` lines; `logs/fixed_panel_f0022.png` shows the bars in the drivers' colours, `logs/presented_panel_w0014.png`
  the *presented* frame - palette plus the port's car-colour pass, i.e. what the 3DS draws - and
  `logs/pink_before_zoom.png` the same frame with the track's magenta put back).  The in-race
  car list
  stops at `DR3_HUD_CARS` rows because its fourth row already
  ends at row 177 of the 200, and the bottom screen map draws every car of the race (`dr3_bottom.c`).
  Screens that hard-wire four cars need a look too: the results list (`___36068h.c`, drawn by
  `___38184h_cdecl`) fills its four rows from the first four cars only, so a twelve car race left rows
  unset and sent an arbitrary index into `s_6c[]` and the screen buffer - it now falls back to the grid's
  own seats and clamps whatever it is handed.
  The difficulty pays, too: a wreck the player caused pays a kill bonus that pops up over the wreck
  (`race___4dcach.c`, the amount follows the victim's car class, drawn through the engine's own floating
  text as `DR3_EFFECT_KILL_BONUS`), and the race pays **every** finishing place instead of its first three
  only (`platform_3ds/dr3_payout.h`, worked out at the end of the race in `___33010h.c`): $100 for every
  place above the last one, a jump at the podium ($5000 / $4000 / $3000) and nothing at all for the last
  place - twenty cars pay $5000, $4000, $3000, $1600 for 4th, ... $100 for 19th and $0 for the last one, so
  a car that was wrecked and pushed to the back takes nothing home.  That ladder
  *replaces* the race's own prize money in chaos (the switch that pays 750/3000/12000 for the podium is
  skipped there), so a win is $5000 and not that prize on top.  Both are
  money the player sees after the race - the kills are credited to `racer.money` and `racer.income` when
  the wreck happens (`race___4dcach.c`) and the report after the race ("Your Statistics", `___36adch.c`)
  shows the kills and the rank money as its *Race income* and *Bonus income* lines, so the box under it
  adds up to what the race earned (`doc/host_harness.md` has the run that checks it).
  The modes go one step further (`DR3_CHAOS_MODE_...` in `drally.h`): a mode is what the *player* picks - the
  file `sdmc:/chaos_mode.txt`, the `L+R+Y` combo in the front end (`platform_3ds/dr3_input.c`) or the wheel of
  the sign up screen itself - and it applies to every race, on any savegame.  Only the extras the chaos
  *difficulty* adds (the bigger field and its AI tables) stay what that level is.  The selector comes up as
  the random mode in this build, so the wheel runs out of the box; `chaos_mode.txt` (or `L+R+Y`) sets a fixed
  mode or `STANDARD` for the plain race selection.  The sign up screen reads the file when it opens, and both
  places log what they read (`[dr3] chaos: chaos_mode.txt says mode N (NAME)` and `[dr3] chaos: sign up
  screen: the selector is armed/off`).
  `DUEL` turns the race into a two car duel against one rival of the standings - any driver within
  `DR3_CHAOS_DUEL_RANK_WINDOW` (three) ranks of the player's own, drawn at random (`dr3_log.c`:
  `dr3_chaos_duel_pick`), so it is a fight in his own class and not always the same neighbour.  A standings
  that has *nobody* that close still gets the nearest driver of the table instead of the seat next to the
  player.  The extra cars of the standard mode stay out of
  it (`___33010h.c` and `race_main.c`), the race's own payout is replaced by the duel's pot - a win against a
  top five rival pays $5000, up to rank 13 $2500, below that $1000, and a loss nothing at all - and the points
  are Elo (`DR3_CHAOS_ELO_BASE + DR3_CHAOS_ELO_STEP * (own rank - rival rank)`, capped at `_MAX_DIFF`, winner
  only, because the original career reads the player's points).  The results screens print the `+N` of a row
  from `dr3_chaos_car_gain[]` (the value the race itself handed out), so a duel shows its Elo there.
  Over the cars there is a health line now: full and green while a car is nearly whole, shrinking and turning
  over yellow to red as it loses HP, scaled to the HP it started the race with.  It only appears where it
  says something - while a car is *damaged*: below `DR3_CHAOS_HP_LINE_BELOW` (90) percent of that HP and
  above 0 HP.  A whole car (the line would be the full length bar of a car nobody has touched), a wreck
  (0 HP) and the **player's own car** (the bar is there to read the *other* cars; his own damage is the
  DAMAGE panel the game's own HUD draws, `s_54[n].__10` in `race___400ech.c`) get no line at all
  (`dr3_carcolour_hp_line_shown`, `platform_3ds/dr3_blit.h` - a portable function, so the host tests check
  the boundaries and the own car; `MY_CAR_IDX` is read the way `dr3_bottom.c` reads it).  It is drawn in the
  port's own image
  (`platform_3ds/dr3_blit.c`, the same tint pass that colours the cars), so the 3DS and the PC show the very
  same line and no entry of the game's palette has to be kept free for it.
  The duel is *offered* where the race would be picked: the sign up screen (`___3266ch.c`) names a driver of
  that window and asks for the two car race (`[FARMER TED WANTS A DUEL - ENTER: FIGHT, ESC: RACE
  ON`, capitals only: this font has no lower case and draws blocks).  The rival is picked once per visit and the line is
  cleared before it is drawn, at row 0x170 (it used to be drawn every frame with a new random rival, on top of the
  footer, at 0x1a4 - an unreadable smear, found in the 3DS run `scenarios/duel_offer.txt`; `duel_decline.txt` is the ESC path).  ENTER signs up as usual and the race runs as the duel against him, ESC turns him down - the mode falls
  back to `STANDARD`, the line is cleared and the choice stays an ordinary one.
  The two contenders are put in place **before** the race screens are built: the setup (`___33010h.c`) puts
  the player and the rival into the seat bytes `esp+0xcc` and cuts `NumCars` down to two at the top of the
  function, so the roster screen draws two portraits and the car table is filled from those same two seats.
  Read behind both of them - where that block sat until 27. September 2026 - only the number of cars came from
  the duel while the screen and the field kept the event's four entries: the "PREPARE TO RACE" screen showed
  drivers that never drove and the rival that was picked never started.  For the same reason the rival the
  screen named is taken back only while he still is a driver of the window (`dr3_chaos_duel_rival_ok`): a name
  from an earlier visit can be far away by the time the race is set up.
  Verified 27. September 2026 (`logs/run_prepare1.ps1`, `logs/prepare1_shots/`): the log reads `duel: rank 20
  has 3 driver(s) within 3 rank(s) - the rival is drawn from them`, `duel: seat 19 (, rank 20, 0 points)
  against seat 18 (FARMER TED, rank 19, 2 points)` and a grid of exactly those two cars, and the screen dump
  shows the two portraits with the boxes of entries three and four empty.  The counter-check
  (`logs/run_prepare2.ps1`, `logs/prepare2_shots/`) is the standard mode: four portraits again and the whole
  roster of twenty cars on the grid.
  Since 27. September 2026 the roster names the **real top 4**: the screen's background has four framed columns
  with the numbers 1..4 in them and the race lines up in the order of the standings, so the four *leaders* of
  the field - the cars of the first four grid places - are what it shows (`DR3_ROSTER_ROWS` in `drally.h`, drawn
  in `___33010h.c` with `dr3_grid_order_by_rank`).  Before this it drew the four seats the event had handed it,
  i.e. four drivers out of twenty in a chaos race: in `logs/prepare2_shots/w_0074.png` the ranks read 1, 2, 3
  and **20** (`logs/roster_before_w0074.png`).  A duel keeps its two contenders, the better placed one in
  column 1.
  The **lists of the sign up screen** stay **unnamed** in a chaos race instead (`___3079ch.c`:
  `dr3_signup_names_shown()`): the four entries of a tier are not who the player meets there - the four track
  pictures and the prices stay, only the `rank. NAME` lines do not.  The release build keeps its lists exactly
  as they are.
  **Die vier Zeilen der Ergebnis-Tafeln nennen die Autos, die gefahren sind** (`platform_3ds/dr3_results.h`,
  `___36068h.c`, `___36358h.c`, `___36718h.c`): eine Tafel zeichnet vier Zeilen und jede Zeile *ist* ein
  Fahrer - Name, Portrait, Auto und Rang, der Rang sogar als Zeilenabstand des Bildes - eine Zeile muss also
  ein Auto des Feldes sein.  Die Originalschleife suchte in den **ersten vier Autos** nach den Plätzen 1..4
  und nahm die Position des Autos als Index in die Entry-Liste des Rennens: in einem Vier-Auto-Rennen ist
  das dasselbe, ein Chaos-Feld stellt aber zwanzig Autos nach der Tabelle auf (`___33010h.c`), die vier
  besten Plätze liegen irgendwo darin, und eine Zeile, die keines der ersten vier Autos füllte, behielt, was
  gerade im Speicher stand.  Auf dem 3DS stand derselbe Fahrer dadurch mehrfach da (drei der vier Zeilen
  "JANE HONDA" im Panel "MEDIUM RACE RESULTS" eines Rennens, das er nur einmal gewonnen hatte); der
  Host-Lauf `logs/run_results1.ps1` hat es mit allen vier Zeilen reproduziert
  (`logs/res1_look/1_vorher_*.png`) - `dr3_results_rows()` (`platform_3ds/dr3_results.c`) bildet die Plätze
  über das ganze Feld ab (`dr3_chaos_car_seat[]`, `NUM_OF_CARS`) und füllt eine Zeile ohne Zielankunft mit
  einem Fahrer, der noch nicht in der Liste steht, nie mit einer zweiten Kopie.  Was eine Tafel gezeichnet
  hat, steht im Log (`chaos: results rows: 1. NAME (racer N), 2. ...`) direkt neben den
  `points: place N of M -> ...`-Zeilen desselben Rennens, ein Lauf ist damit Zeile für Zeile vergleichbar;
  seit dem Fix liest der Lauf dort `1. DUKE NUKEM, 2. NASTY NICK, 3. JANE HONDA, 4. MATT MILER` - genau die
  vier Plätze des Rennens (`logs/res1_look/3_nachher_*.png`).

  Both hang off one definition of the field: `dr3_chaos_field()` (`___33010h.c`) says which racers drive this
  race (the seats of the event and then the extras, in roster order, none in a duel) - the roster reads the
  same list the field setup does, so the screen cannot name a driver the race does not have.
  Verified 27. September 2026 (`logs/run_roster1.ps1`, `logs/run_roster2.ps1`, `logs/run_roster3.ps1`):
  `roster row 1 of 4: JANE HONDA (standings 1, 100 points, car 5)` ... `row 4: MOTOR MARY (standings 4,
  69 points, car 4)` against `grid place 1..4` of the same run - the same four drivers, and the race itself
  still runs its twenty cars (`grid place 20: ...`).  `logs/roster_after_w0180.png` is the screen out of that
  run.  The unnamed lists of the sign up screen are only visible in the game (the harness cannot reach that
  screen, doc/host_harness.md).
  "Destroy 'em all" (`DR3_CHAOS_MODE_DM`) takes the finish line away instead: the laps no longer finish anybody
  and the lap counter keeps running (`race___55ae0h.c`), the flag for "the race distance is done"
  (`___196dd4h`) stays 0 - that flag was what ended a deathmatch after the race distance in the first version -
  so the race ends when only one car is left standing, the engine's own rule for any race (it counts the
  destroyed cars, `race_main.c`).  The track is armed to the teeth for it: up to **twelve** pickups at once (every slot the original's obstacles
  and pickups live in, `DR3_CHAOS_DM_PICKUPS_MAX`) and every one of them is the **ammunition** pickup - kind 1,
  the one that fills the resource the HUD draws as its AMMO BAR (`race___526ach.c`, `+0x30`).  The original
  only ever rolls that kind for a car that is carrying a weapon already; the deathmatch takes that branch and
  rolls it every time (`race___5209ch.c`).
  The lap indicator next to the standings is a table of ten sprites (`___1de210h`), so a deathmatch drops the
  "of N" part entirely - there is no race distance to show - and its own counter stops at
  `DR3_CHAOS_HUD_LAP_MAX` (9) instead of reading past that table.
  The survivor takes the pot ($5000) and the winner's points, every destroyed car scores nothing for the race,
  but each wreck paid its kill money the moment it happened, so the money for destroyed opponents is exactly
  what the other modes pay.
  A deathmatch that never ends was the price for that: the engine only counts a car as out when it is
  destroyed *and* standing still (`race_main.c` wants `s_35e[n].__b0 <= 0.5f`), and the only other way out is
  the wreck of the player - one stuck or unreachable car therefore kept the race running forever.  So the mode
  has a deadline now (`platform_3ds/dr3_dm.h`): **three minutes** into the race the **sudden death** starts and
  every car - the player included - loses **one percent of its HP per second**, in the game's own unit
  (`race___400ech.c`: damage = 100 - HP/1024, so it is 1024 HP on every car, whatever its own scale is).  It is
  measured with the race's own clock (`getCounter(1)`, 70 ticks a second, started in `race___49a34h.c`), so it
  is game time.  As soon as only one car is left, the port starts the same wind down the player's own wreck
  uses - the engine's rule without its "and it stands still" - and the race is over; the pot then goes to the
  survivor as always.  A fat **SUDDEN DEATH** banner (the game's own 6x6 font at three times the size, outlined
  and in yellow, two lines so it stays between the two HUD columns, drawn by the port's own pass in
  `dr3_blit.c`) says what is going on: it stands for six seconds and then blinks.  `sdmc:/chaos_dm.txt` holds
  the seconds until it starts (0 switches it off - then the mode runs exactly as it did before), the harness
  reaches the same switch with `dmsudden <n>`.
  Und damit Camping nicht die Siegstrategie ist (gleicher Abzug für alle heißt: wer nicht kämpft, hat am Ende
  die meisten HP und bekommt den Pott), gibt es die **Lap-Pflicht**: pro Minute Rennzeit muss ein Auto eine
  Runde gefahren sein, sonst verliert es beim Check **25 % seiner HP** (25600, dieselben 1024 HP je Prozent) -
  vier verpasste Checks und es ist ein Wrack.  Die Runden zählt der Port selbst, und zwar am eigenen
  Rundenwechsel der Engine: `s_35e[n].__0` springt genau dann auf 0 zurück (`race___55ae0h.c`) - das `Lap`-Feld
  geht nicht, weil es im DM für die HUD auf 9 begrenzt wird.  `sdmc:/chaos_dm_lap.txt` hält die Sekunden
  zwischen zwei Checks (0 schaltet die Pflicht ab, im Harness `dmlap <n>`).
  Sobald das Rennen vorbei ist (`___243d14h != 0`, `dr3_chaos_race_over`), hört beides auf - und der Port
  zeichnet auch **keine Health-Bars und keinen Banner** mehr über den RACE-OVER-Screen und seine
  Schwarzblende: die Zeilen der Autos stehen dann als `over` im Log, plus eine Sammelzeile
  `hp line: the race is over ... N health line(s) ... are hidden`.
  There is a **random selector** too (`DR3_CHAOS_MODE_RANDOM`, `___3266ch.c`): instead of choosing, the sign up
  screen draws **twice**, in the two rounds the player watches - both over the three boxes of the screen (the
  poster columns; `D(___185a50h)` is the highlighted race, `___27f80h_cdecl` draws the copper frame around a box
  and `___281d0h_cdecl` takes it away).
  * **Round 1, the mode**: the three boxes carry the three rule sets (`RACE`, `CHALLENGE 1V1`, `DESTROY 'EM ALL`)
    as cards - the frame border of the screen's own message boxes (`___13248h_cdecl`, the long name on two
    lines) - and the copper frame hops over them with a delay that grows.  The mode it drew then stays on the
    screen as a banner at the top (`dr3_spin_header`, the same border with the screen's big font), so the
    player sees *which* rule set the second round is for.  Before that, round 1 keeps its own result - the
    cards and the frame on the box that was drawn - for `DR3_CHAOS_SPIN_MODE_HOLD_S` seconds (3, `dr3_spin_hold`)
    so the mode can really be read; without it round 2 starts in the same breath.  That pause is a length of
    *time* and not a number of frames: the front end runs at the 60 frame/s of `___605deh_cdecl(60, 0)`
    (`___60458h`, `___60420h.c`), the race at 70 - and the harness runs the same code much slower, so a frame
    count would be three seconds on the console and ten in a run.
  * **Round 2, the race**: what the wheel always did, over the same boxes.  The cards go with `___30df8h()` -
    the routine that paints the posters, which live in the *screen* and not in the background, so a restore
    from the background would leave empty boxes behind - and the frame lands on the race ENTER signs up for.
  All it sets in the end is `D(___185a50h)` and `dr3_chaos_mode_commit()` (the mode the race setup reads), so
  ENTER registers exactly as it always did.  Only races the player may sign up for are drawn (a free slot plus
  the class checks of the ENTER branch) and the three modes have equal chances; the harness can check the
  choice without that screen (`spin <n>`).  The wheel is armed whenever the mode says `RANDOM` - it does not
  ask for the chaos difficulty, because it is the way into these races.
  The harness never ran that screen until then: its quick race builds the field itself (`dr3_host_quick_race`),
  so a run could only ever check the *draw*, never the wheel.  `signup <ms>` / `--signup <ms>`
  (`platform_3ds/dr3_host.c`) calls the real `___3266ch()` from the menu instead, and the frames it presents are
  dumped like any other; `raced 1` sets the career counter the wheel needs, so a run does not have to drive a
  whole race first.  Verified 27. September 2026 (`logs/run_spin3.ps1`, frames in `logs/spin_look3/` and
  `run_host/frames/spin3/`): the log reads `[dr3] chaos: sign up screen: the selector is armed (mode 3,
  RANDOM)`, then `[dr3] chaos: spin: round 1 - the mode: DESTROY 'EM ALL over the three boxes of the screen
  (round 2 is the race)` and `[dr3] chaos: spin: landed on race 1 (0..2, the fights are there) and mode 2 (DM) -
  ENTER signs up`; the frames show the three cards with the frame on the third box, then the posters back with
  the banner up and the frame on the middle box.  `logs/run_signup1.ps1` (`logs/signup_demo1/`) is the older
  single-round run, `logs/run_spin1.ps1` the distribution of the draw.
  The look and the rules around it (the first version got three of them wrong):
  * The wheel starts fast and eases out (the delay per step grows quadratically), sits on the race it landed
    on for a moment and only then rolls the mode line, so both are readable.
  * The very **first race of a career** - the one right after the licence - is an ordinary race.  The wheel
    only spins while a race of that career has been set up (`dr3_spin_armed`, `dr3_chaos_raced` reads the
    career's own counter `racer_t.races`), and the first race is exactly the one that sets that counter.
  * It only draws races the player could really sign up for: a free slot plus the class gates of the ENTER
    branch (`dr3_spin_race_ok`: race 2 wants a car of class 2, race 3 one of class 4).  Landing on a race the
    ENTER branch would refuse is a dead end, and the first check compared the gate with the *highlight*
    instead of `1`, which is why the wheel could land on a race that could not be driven.
  * The choice is **locked** once the wheel stops: the arrow keys only bump (with the sound the screen plays
    for a full race) and ENTER signs up for the race that was drawn, with the mode the wheel rolled.  The line
    with the rolled rule set stays on the screen until the race starts.
  `dr3_log.c`/`dr3_log.h` carry the build's step trace (`[dr3] chaos step ...`), which is what makes a
  hang outside the race loop (results, front end) readable from the log.  Two things that trace taught:
  the signup loops of the next event (`___33010h.c`, `___3079ch.c`) are unbounded in the original and a
  twelve car race can leave the racer pool without a candidate they accept - that is the black screen
  freeze (the palette is already down there), so the chaos build bounds them and logs what it had; and
  the trace itself has to flush every 32nd line instead of every line, because one SD flush per line
  costs more than the frame it traces (72 -> 18 presents/s)

Profiler build (`debug`): `-DDR3_PROFILE` measures the frame budget on the console and writes `STAT`
and `SUMMARY` lines to the log; `scripts/analyze_3ds_profile.ps1` turns them into a table.
