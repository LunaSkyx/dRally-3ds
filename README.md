# Death Rally — Nintendo 3DS (30th Anniversary Edition)

A native Nintendo 3DS port of **Death Rally (1996)** built with devkitARM, libctru, and SDL2 — featuring a brand-new **30th Anniversary** championship mode alongside all classic difficulties.

Based on the [urxp/dRally](https://github.com/urxp/dRally) decompilation.

---

## 30th Anniversary Modes

Pick **30th Anniversary** (Normal) or **30th Anniv EXTREME** (Very Hard) on the driver licence screen:

- **30th Anniversary (Normal — Rising Chaos)**: Grids start at **8 speed-matched cars** in Vagabond/Dervish and grow through **12 → 16 → full 20 cars** as you climb the standings and upgrade, with progressive AI pace and unlocking Adversary invasions.
- **30th Anniv EXTREME (Very Hard)**: All **20 cars from Race 1** (16 in Arena), boosted AI speed/acceleration/armor on top of full Anniversary pace, heavily armed rivals (mines, spikes, rocket fuel), 45–60% Adversary invasions, longer 1v1 duels (4–6 laps), 2× Arena storm drain, a beefed-up 3-Phase Boss Gauntlet (4× HP Giant Boss), and higher Underdog Bounties (`+$200`/class gap).
- **4-Mode Shuffle Wheel**: Rotates all four event types with no back-to-back repeats:
  - **Race** — Large-grid street racing with extra off-line street cash drops (kept even if lapped).
  - **Challenge 1v1** — High-stakes head-to-head duel against a close rival for Elo rank points.
  - **Destroy 'Em All** — Street Last Car Standing with aggressive combat AI, mine crates, ammo, and sudden death.
  - **Battle Arena** — Custom free-for-all arena (`TRA.BPA`) with closing storm ring, grudge rivals, and top-3 survivor payouts.
- **Live In-Race Radio & Underdog Bounties**: Dynamic pit radio, rival trash-talk overlays, and extra cash bonuses for wrecking higher-class cars.
- **3-Phase Final Boss Gauntlet**: Reach the top of the standings to face **The Swarm** (1v20 Vagabonds) → **The Escort** (12-car elite oval race) → **The Adversary** (1v1 Giant Boss with enrage phase).
- **Purchasable Anniversary Car**: Buy the Adversary's supercar in the shop — complete with custom obsidian GT graphics, chrome nameplate, and live color slider.

### 3DS Port Highlights
- **Native 400×240 widescreen** race viewport on the top screen
- **Live bottom screen**: interactive track minimap, car positions, standings, and lap times
- **DSP hardware audio**, **hold-to-slide car color picker**, quick save/load, and remappable controls (`dr3_controls.txt`)

---

## Installation

> **Requires original Death Rally game files** (not included). The custom Battle Arena track (`TRA.BPA`) is included in the release zip.

1. Place your original Death Rally files (`ENGINE.BPA`, `IBFILES.BPA`, `MENU.BPA`, `MUSICS.BPA`, `TR0.BPA`–`TR9.BPA`, `CDROM.INI`, and `CINEM/`) plus `TRA.BPA` from the release into:
   - `sdmc:/3ds/dRally_3ds/`
2. Launch using either:
   - **CIA**: Install `dRally_3ds.cia` with FBI and launch from the Home Menu.
   - **3DSX**: Copy `dRally_3ds.3dsx` into `sdmc:/3ds/dRally_3ds/` and launch via Homebrew Launcher.
3. Audio requires `sdmc:/3ds/dspfirm.cdc` (dump once via Luma3DS Rosalina menu).

---

## Controls

| Button | Front End | In Race |
|---|---|---|
| **D-Pad / Circle Pad** | Navigate / hold to slide colors | Steer (Up/Down = gas/brake) |
| **R / L** | — | Accelerate / Brake |
| **A / B** | Confirm / Back | Turbo / Horn |
| **Y / X** | Quick Load / Quick Save | Machine Gun / Drop Mine |
| **START / SELECT** | Pause / 3DS Keyboard | Pause / — |
| **L + R + Y** | — | Cycle race mode manually |
| **L + R + START** | Quit to Home/HBL | Quit to Home/HBL |

Rebind any control via `dr3_controls.txt` next to the game data.

---

## Building from Source

Requires devkitPro (`devkitARM` + `3ds-dev`) and SDL2 built for 3DS (see [doc/3ds.md](doc/3ds.md)):

```sh
make -f Makefile.3ds -j6
powershell -ExecutionPolicy Bypass -File scripts/3ds/build_cia.ps1
```

Project layout: `src/` (engine C sources), `platform_3ds/` (3DS port & 30th Anniversary systems), `assets/` (CIA icon/banner/RSF), `scripts/` (build & packaging), `tests/` (host unit tests), `doc/` (technical notes).

---

## Credits & Legal

Original game © Remedy Entertainment. Decompilation by [urxp](https://github.com/urxp/dRally).  
This repository contains **no copyrighted game data** and is not affiliated with Remedy Entertainment.
