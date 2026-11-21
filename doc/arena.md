# The ARENA chaos mode (20. November 2026)

An epic free-for-all on a map of its own. **Not a race**: all cars start in the same moment, each from its own
place in one of the four corners of a walled arena, the floor is littered with items (ammunition, turbo, mines,
repairs), and the AI of every car *hunts* - it picks an opponent, drives at him and shoots, instead of following
a road. The last cars standing win; the **top three** are paid (`$6000 / $3500 / $1500`) and every place scores
the points of the chaos ladder (1st +6 ... 5th +2, everybody else +1).

`DR3_CHAOS_MODE_ARENA = 4` (`drally.h`). It is one of the four outcomes of the **Chaos wheel** of the sign-up
screen (RACE / CHALLENGE 1V1 / DESTROY 'EM ALL / ARENA, equal chances) and can be forced like every mode:
`sdmc:/chaos_mode.txt` = `4`, the `L+R+Y` combo, or `az_run.ps1 -Mode arena`.

| Piece | Where |
|---|---|
| the map generator | `scripts/arena/make_arena.py` (+ `bpa.py`, the BPA/BPK codec; `study.py` renders layers of a real track) |
| the map | `TRA.BPA` next to the other `TRn.BPA` (the game directory). **Generated from the player's own `TR1.BPA`** (its 86 shared palette entries are copied), so it is *not* in the repository: run the generator once |
| the rules + the AI | `platform_3ds/dr3_arena.h/.c` (pure functions are host tested: `tests/test_dr3.c`, `test_arena`) |
| the hooks | `race_main.c` (AI dispatch `dr3_ai_call`, places, music, field), `race___42824h.c` (corner placement), `race___5209ch.c` (item mix, 16 places), `race___55ae0h.c`/`race___40f48h.c` (no finish line, no lap rules), `___33010h.c` (track `TRA`, field, payout), `platform_3ds/dr3_dm.c` (the tick), `___3266ch.c` (the wheel) |

## Making `TRA.BPA`

```
python -I scripts/arena/make_arena.py "<game dir>" [--preview out.png]     # reads TR1.BPA there, writes TRA.BPA there
```

`<game dir>` is the directory with `TR1.BPA` (on the emulator `%APPDATA%\Azahar\sdmc\3ds\drally`, on a console
`sdmc:/3ds/drally`). The generator asserts that every pickup place and spawn slot is drivable and that the packed
`IMA` fits the loader's 307200 byte buffer. `--preview` writes the picture and the surface mask side by side
(`logs/arena_study/preview.png`).

**The map**: 960 x 600, a 44 px wall all round, four L-shaped bunkers and four pillars for cover, a round hub in the
middle, four spawn bays (one per corner, with a shield), oil patches and boost strips (**no slowdown surfaces** - mud and gravel were taken out on request, `MUD`/`GRAVEL` in the generator are empty), 16 item places. A track is a
BPA of `TRA-IMA/MAS/VAI/LR1/SHA/SCE.BPK`, `SKI/BLO/LIT.TAB`, `DRV.DAT`, `OHI.DAT`, `INF.BIN`, `FLIP.PAL`, plus `TRA-SHP.BPK`
(the 360 x 274 chrome-slab loading screen preview matching `TSHAPE01..19.BPK`, drawn by `___33010h.c` alongside `LAST CAR STANDING` and `PRIZE MONEY: $6000`); the formats
(variable width LZW with ROR3 on the output, the surface nibble of the mask: 0-3 solid, 4 rough, 6 mud, 7 gravel,
0xb boost, 0xd oil, 0xf hard; `INF.BIN` = size, zones, 4 starts, 16 pickup places) are in the docstring of
`make_arena.py`. `bpa.py` was checked against the real game data (its encoder reproduces the original sizes).

## The rules (as built)

* **Start**: every car at once, in `dr3_chaos_car_pos` order (the grid order the standings give): place n -> corner
  `n % 4`, slot `n / 4`, facing the centre (`dr3_arena_spawn`; 12 cars = 3 to a corner, the bays hold 16). Everybody gets the machine gun, full
  ammunition and 3 mines. The field is the event's cars + `chaos_extras.txt` extras, **at most 12** (`DR3_ARENA_CARS`).
* **No laps**: the track has no checkpoints, the finish line is off (as in Destroy 'em all), `NUM_OF_LAPS` = 1.
* **Items** (`race___5209ch.c`): all 16 places respawn (the original uses 12; places 12-15 are its one-shot places).
  The mix (`dr3_arena_item_for_roll`): ammunition 30 %, turbo 12 %, **single mine 16 %**, small repair 10 %,
  **pack of ten mines 12 %**, big repair 20 % - mines lie around to be collected.
* **Closing ring** (against hiding in a corner): a safe circle centred on the arena starts at radius `DR3_ARENA_RING_START_R`
  (620 px, outside the corners), holds for `DR3_ARENA_RING_GRACE_S` (12 s), then shrinks linearly over `DR3_ARENA_RING_SHRINK_S`
  (80 s) down to `DR3_ARENA_RING_END_R` (120 px, around the centre hub and inner pickups). Any car outside loses
  `DR3_ARENA_RING_DRAIN_PCT` (3 %) of full HP per second (`dr3_arena_ring_tick`). The ring is drawn on the track itself
  (4 px red circle painted into `TRX_IMA` and restored as it shrinks, leaving pickup pads untouched) and on the bottom-screen
  minimap (`dr3_minimap_set_ring`, updated incrementally with no flicker). A banner on the top screen announces `THE RING / CLOSES`
  for the first 5 s in yellow and blinks `GET INSIDE / THE RING` in red whenever the player's car is outside (`dr3_blit.c`).
  The AI ignores items past the ring and drives inward when approaching its edge (`dr3_arena_ring_safe_r`).
* **End**: when one car is left (`dr3_dm_tick`, shared with Destroy 'em all). The player being wrecked does **not**
  end it: the others get `DR3_ARENA_SPECTATE_S` (25 s) to finish, then the HP of the survivors decides. The sudden death of the deathmatch
  (`dmsudden`, 180 s) is a backstop against stalemates here too.
* **Places** (`dr3_arena_rank`, written into `Position` by `dr3_arena_final_places`, and kept live twice a second so the in-race
  board shows the order of the fight): survivors first - more HP ahead, ties by index - then the wrecks, the last one
  to die first.
* **Money**: the top three places pay `DR3_ARENA_PRIZE_1/2/3` into money, income and bonus of the player; what his wrecks
  paid (the kill bonus) comes on top. The points are the normal chaos ladder.
* **Difficulty & AI combat** (`dr3_arena_ai`): the human is *not* singled out — every car runs the same hunting logic.
  - **Wall survival**: `dr3_arena_room3` sweeps three parallel rays across the car's body (`±10 px`) out to `108 px` (`DR3_ARENA_PLAN_EVERY = 2`), and brakes down to `speed <= 1.7` whenever a wall sits within `62 px` ahead instead of slamming into bunkers for 10% HP damage.
  - **Gun discipline & aiming**: compensates for the engine's `5/6` Y-axis scaling (`race___4c434h.c`, `race___4ff50h.c`), checks clear line of sight (`dr3_arena_los`) and a tight `13 px` hit corridor before firing (so ammo isn't wasted on walls or air), and eases speed while tracking a close-range burst (`< 145 px`).
  - **Fair target selection**: prefers enemies in clear line of sight and gives a small priority bump to both low-HP finishers (`< 30%`) and healthy leaders (`> 75%`) so high-HP survivors aren't ignored while AI fights dying cars.
  - **Mines & items**: drops mines on pursuers within `135 px` (`DR3_ARENA_MINE_COOLDOWN = 140`). A car that chases the same item for `DR3_ARENA_ITEM_GIVE_UP` (260) frames leaves it alone for `DR3_ARENA_ITEM_BAN` (700) frames, and brakes to turn in near items instead of orbiting. **Stuck** (< 6 px in 36 frames): backs out, and from the second try on is set down a car's length into free room.
* The music is `TR6-MUS.CMF` (the arena has none), the bottom screen / minimap show `ARENA`.
* **Chaos Adversary invader** (`___33010h.c`, `dr3_grid.h/.c`, `dr3_log.c`, `race___4f300h.c`, `dr3_bottom.c`): in any non-duel Chaos race (`STANDARD`, `DM`, `ARENA` — never `DUEL`), the **Adversary** has a random chance (`DR3_CHAOS_BOSS_CHANCE_DEFAULT = 25%`, configurable via `sdmc:/chaos_boss.txt` or `az_run.ps1 -Boss <0..100>`) to join right from the start as a guest boss (`seat = -1`, so the 20-racer standings table is never corrupted). He drives the fully upgraded jet-black `SPECIAL` (car model 6, dual Deliverator guns, `+8%` speed / `+5%` steer, `$1000` kill bonus), appears in row 1 of `PREPARE TO RACE` with his silhouette portrait, starts at **pole position P1** at the front of the pack (or corner 0, slot 0 with 5 mines in Arena), and is highlighted with a vivid purple marker on the bottom-screen minimap.

## The wheel

Round 1 of the sign-up wheel now has **four cards** (`___3266ch.c`): the posters make room (the band they live in is
restored from the background, the player's panel on the right stays), the copper frame hops over the cards, lands on
the drawn mode, holds 3 s; then the posters come back and round 2 draws the race as before. The mode that was drawn
is committed with `dr3_chaos_mode_commit`, the setting stays RANDOM. The wheel only spins on a career that has raced once.

## Testing it

```
powershell -File scripts\3ds\az_run.ps1 -Scenario scripts\3ds\scenarios\arena.txt       -Mode arena -Extras 11   # 4 screenshots, ~45 s
powershell -File scripts\3ds\az_run.ps1 -Scenario scripts\3ds\scenarios\arena_full.txt  -Mode arena -Extras 11   # to race over + payout
powershell -File scripts\3ds\az_run.ps1 -Scenario scripts\3ds\scenarios\arena_prize.txt -Mode arena -Extras 0    # player healed -> a prize place
powershell -File scripts\3ds\az_run.ps1 -Scenario scripts\3ds\scenarios\wheel.txt       -Mode random -Extras 11  # the four cards, screenshots
powershell -File scripts\3ds\az_run.ps1 -Scenario scripts\3ds\scenarios\wheel_race.txt  -Mode random -Extras 11  # wheel -> race (repeat: 1 in 4 is ARENA)
powershell -File scripts\3ds\az_run.ps1 -Scenario scripts\3ds\scenarios\boss_race.txt   -Mode standard -Extras 18 -Boss 100  # Adversary at P1 (front of pack)
powershell -File scripts\3ds\az_run.ps1 -Scenario scripts\3ds\scenarios\boss_arena.txt  -Mode arena -Extras 11 -Boss 100     # Adversary in Arena -> payout
powershell -File tests\build_tests.ps1                                                                            # host tests (3364 checks)
```

Log vocabulary (`-Grep 'arena'`): `arena: car N (NAME) starts in corner C, slot S, at X,Y facing ...`, `arena: 35s, 8 left - N:hp%/aAMMO/sSPEED@x,y ...`
(every 5 s), `arena: car N is out at Ts - K car(s) left (at x,y, speed, heading, room ahead)`, `arena: car N is stuck at ...` / `is set down at ...`,
`arena: place P - car N (NAME), survived|out (HP ...)`, `arena: the player finished place P of M -> prize $X, kills $Y`,
`report of the race: ...`, `spin: round 1 - the mode: ARENA over the four cards ...`.

Scenario steps added for this: `raced` (the career counts as raced, so the wheel spins - `dr3_chaos_raced_force`), `playerhp N`
(sets the player's HP in the running race; heals him to fake a long-living player).

## Verified (20. November 2026, Azahar, release build)

* 12 cars start in 4 corners at once (`arena: car N starts in corner ...`), screenshots + minimap of `TRA`, pickups taken (all kinds), kills by
  gun / border / ram, full runs to the report (`logs/az_runs/final*`, `small*`, `prize3`): `place 2 of 4 -> prize $3500, kills $500 ... money 4695`.
* The wheel into a race: `spin: landed on race 2 ... mode 4 (ARENA)` -> `race mode 4 (ARENA)` -> 12 cars in the corners (`logs/az_runs/wr3`, `wr4`).
* The closing ring (`logs/az_runs/ringlong`, `scenarios/arena_ring_long.txt`): a player idling in his corner is wrecked by it at 59 s, so campers cannot win. Host tests 3331 / 0 failures; the PC harness (`gen_host_project.ps1 -Build`) still builds.

## Known limits / ideas

* Azahar is not a console: timings (frame rate with 12 cars on a 960x600 map) were not measured on hardware.
* A *human* player has not been played; the harness drives him with the autopilot, which uses the same hunting AI. The balance of
  the AI (ammunition runs out after ~20 s of fighting; fights end mostly by ramming / walls afterwards) is a first version.
* The item places are the track's 16 `INF.BIN` powerup places; more items at once would need a bigger table in the engine (`___1f2488h` has 20 slots, 12-15 are reserved by the original's special pickups).
* `TRA.BPA` holds palette entries copied from the player's `TR1.BPA` - do not redistribute it; the generator is the thing to share.
