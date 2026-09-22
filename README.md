# dRally-3ds

The main goal of this project is to create a port of Death Rally (1996) running natively on Linux and BSD based operating systems.

Credits for the full decomp go to [urxp](https://github.com/urxp)

#### Nintendo 3DS port (branch `3ds`)

Native port with devkitARM, libctru and SDL2's own n3ds backend.  Renders straight into the 400x240
framebuffer (no SDL renderer), DSP audio at the DAC's native rate, pad remappable through
`dr3_controls.txt`.  The bottom screen shows the standings and, during a race, the track map with every
car and the lap times.  Technical notes: [doc/3ds.md](doc/3ds.md).

The branch also adds a fourth very hard difficulty ("30th Anniversary").
championship entry, the title is settled in a two car challenge, and that car can be bought in the shop.

Install: copy the release folder to `sdmc:/3ds/drally/` so that `dRally_3ds.3dsx` sits next to the game
data (all `*.BPA`, `CDROM.INI`, `CINEM/*.HAF`), then start it from the Homebrew Launcher.  Sound needs
`sdmc:/3ds/dspfirm.cdc` (Luma3DS creates it on real hardware).  The first start asks for a driver name -
SELECT opens the software keyboard, A accepts it.

| | |
|---|---|
| D-pad, circle pad | steer, up/down also gas and brake |
| R, L | gas, brake |
| A, B | front end: confirm, select - race: horn, boost |
| Y, X | race: shoot, drop a mine |
| X / ZL, Y / ZR | front end: quick save, quick load (`DR.SG7`) |
| START | pause |
| SELECT | 3DS software keyboard |
| L + R + START | quit |

`dr3_controls.txt` rebinds any of that (`GAS = R, UP, STICK_UP, B`); typos go to `sdmc:/drally_3ds.log`.

This branch contains no game data and is not affiliated with Remedy Entertainment.

#### Linux requirements
* GCC/Clang C compiler
* GNU/Make
* SDL2


#### Building

```sh
FLAGS="YOUR CFLAGS" make
```

#### Installation - needs original game assets

* [Death Rally registered free windows version CHIP](https://www.chip.de/downloads/Death-Rally-Vollversion_38550689.html)

```sh
7z e -o drally DeathRallyWin_10.exe
cd drally && mkdir CINEM && mv ENDANI* CINEM && mv SANIM* CINEM
echo "./CINEM" &> CDROM.INI
```

Only versions including the DR.IDF file are able to use the `FLAGS += -DDR_CDCHECK`

    dRally
    |--CINEM
    |  |--DR.IDF
    |  |--ENDANI.HAF
    |  |--ENDANI0.HAF
    |  |--SANIM.HAF
	|--CDROM.INI        [1]
    |--ENGINE.BPA
    |--IBFILES.BPA
    |--MENU.BPA
    |--MUSICS.BPA
    |--TR[0-9].BPA

    Make sure these file/dir names in dRally directory are in uppercase.

    [1] CDROM.INI contains relative location of CINEM directory (./CINEM)

#### Work in progress
*   Multiplayer not available 
