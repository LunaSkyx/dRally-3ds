/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_files.h"
#include "dr3_bottom.h"
#include "dr3_log.h"
#include "drally.h"

/* this file prints on the bottom screen console, so it needs the real printf */
#if defined(printf)
#undef printf
#endif

#include <stdio.h>
#include <string.h>
#include <3ds.h>

/* the files of an original copy, as the release package lays them out */
static const char *const dr3_required[] = {
    "ENGINE.BPA", "IBFILES.BPA", "MENU.BPA", "MUSICS.BPA",
    "TR0.BPA", "TR1.BPA", "TR2.BPA", "TR3.BPA", "TR4.BPA",
    "TR5.BPA", "TR6.BPA", "TR7.BPA", "TR8.BPA", "TR9.BPA",
    "CDROM.INI",
    "CINEM/SANIM.HAF", "CINEM/ENDANI.HAF", "CINEM/ENDANI0.HAF"
};

#define DR3_REQUIRED_COUNT ((int)(sizeof(dr3_required) / sizeof(dr3_required[0])))

/* an empty placeholder counts as missing - the engine would read garbage from it */
static int dr3_file_ok(const char *name)
{
    FILE *fd = fopen(name, "rb");
    long  size;

    if (!fd) return 0;

    fseek(fd, 0, SEEK_END);
    size = ftell(fd);
    fclose(fd);

    return size > 0;
}

static int dr3_files_missing(void)
{
    int i, missing = 0;

    for (i = 0; i < DR3_REQUIRED_COUNT; ++i) {
        if (!dr3_file_ok(dr3_required[i])) ++missing;
    }

    return missing;
}

static void dr3_files_log(void)
{
    int i;

    dr3_log("[dr3] game files: %d of %d missing", dr3_files_missing(), DR3_REQUIRED_COUNT);

    for (i = 0; i < DR3_REQUIRED_COUNT; ++i) {
        if (!dr3_file_ok(dr3_required[i])) dr3_log("[dr3]   missing: %s", dr3_required[i]);
    }
}

static void dr3_files_show(void)
{
    const int missing = dr3_files_missing();
    int       i;

    if (!dr3_bottom_console_ensure()) consoleInit(GFX_BOTTOM, NULL);

    printf("\x1b[2J\x1b[H");
    printf("Death Rally - game files missing\n\n");
    printf("This port needs the files of an original Death Rally\n");
    printf("copy.  They belong next to dRally_3ds.3dsx:\n\n");
    printf("    sdmc:/3ds/drally/ENGINE.BPA, MENU.BPA, TR0.BPA ...\n");
    printf("    sdmc:/3ds/drally/CDROM.INI, CINEM/*.HAF\n\n");
    printf("Not found (%d of %d):\n", missing, DR3_REQUIRED_COUNT);

    for (i = 0; i < DR3_REQUIRED_COUNT; ++i) {
        if (!dr3_file_ok(dr3_required[i])) printf("    %s\n", dr3_required[i]);
    }

    printf("\n\nPress A to quit.\n");

    dr3_bottom_flush();
}

static void dr3_files_wait(void)
{
    /* libctru reads the pad directly here: SDL's input layer is not running yet */
    touchPosition touch;

    do {
        svcSleepThread(16000000);           /* ~1 frame, also drains the launcher's button press */
        hidScanInput();
        hidTouchRead(&touch);
    } while (!(hidKeysDown() & (KEY_A | KEY_B | KEY_START | KEY_SELECT)) && !touch.px && !touch.py);
}

int dr3_files_ok_or_wait(void)
{
    if (dr3_files_missing() == 0) {
        dr3_log("[dr3] game files complete (%d checked)", DR3_REQUIRED_COUNT);
        return 1;
    }

    dr3_files_log();
    dr3_files_show();
    dr3_files_wait();

    return 0;
}
