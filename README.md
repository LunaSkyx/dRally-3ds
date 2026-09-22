# dRally

The main goal of this project is to create a port of Death Rally (1996) running natively on Linux and BSD based operating systems.

#### Nintendo 3DS port (branch `3ds`)

Native port with devkitARM, libctru and SDL2's own n3ds backend.  Renders straight into the 400x240
framebuffer (no SDL renderer), DSP audio at the DAC's native rate, pad remappable through
`dr3_controls.txt`.  The bottom screen shows the standings and, during a race, the track map with every
car and the lap times.

Also adds a fourth difficulty ("30th Anniversary") with the black SPECIAL as a championship rival, a
final two car challenge, and that car for sale in the shop.

* installation, controls: [INSTALL_3DS.md](INSTALL_3DS.md)
* technical notes and the profiler build: [doc/3ds.md](doc/3ds.md)
* no game data, not affiliated with Remedy Entertainment

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
