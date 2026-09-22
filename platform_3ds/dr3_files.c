/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_files.h"
#include "dr3_bottom.h"
#include "dr3_log.h"
#include "dr3_paths.h"
#include "drally.h"

/* this file prints on the bottom screen console, so it needs the real printf */
#if defined(printf)
#undef printf
#endif

#include <stdarg.h>
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

/*
 * The console on the bottom screen is 40x30 characters.  A row that does not fit wraps and scrolls the
 * whole screen - the heading would go with it, on the one screen that has to tell a new user why the
 * game did not start.  dr3_bottom.c keeps its block below the console width for the same reason, so
 * every row here goes through dr3_files_row() and the list of names is cut off when it would not fit.
 */
#define DR3_FILES_TEXT_W 38
#define DR3_FILES_ROWS   30
#define DR3_FILES_HEAD   8                          /* header rows + a blank + "Not found (...)" */
#define DR3_FILES_FOOT   2                          /* a blank + "Press A to quit." */
#define DR3_FILES_LIST_MAX (DR3_FILES_ROWS - DR3_FILES_HEAD - DR3_FILES_FOOT)

/* one row of the screen, cut to the console width instead of wrapped */
static void dr3_files_row(const char *fmt, ...)
{
    char    row[DR3_FILES_TEXT_W + 1];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(row, sizeof(row), fmt, ap);
    va_end(ap);

    printf("%s\n", row);
}

/* the folder the game looked in - when the files ended up elsewhere its tail is the useful part */
static void dr3_files_row_cwd(void)
{
    const char *label = "looked in: ";
    const char *path  = dr3_cwd();
    const int   max   = DR3_FILES_TEXT_W - (int)strlen(label);

    if ((int)strlen(path) > max) path += strlen(path) - max;

    dr3_files_row("%s%s", label, path);
}

static void dr3_files_show(void)
{
    const int missing = dr3_files_missing();
    int       shown   = 0;
    int       i;

    if (!dr3_bottom_console_ensure()) consoleInit(GFX_BOTTOM, NULL);

    printf("\x1b[2J\x1b[H");

    dr3_files_row("Death Rally - files missing");
    dr3_files_row("Needs the files of an original copy,");
    dr3_files_row("next to dRally_3ds.3dsx in:");
    dr3_files_row("  sdmc:/3ds/drally/");
    dr3_files_row("  (all .BPA, CDROM.INI, CINEM/*.HAF)");
    dr3_files_row_cwd();
    printf("\n");

    dr3_files_row("Not found (%d of %d):", missing, DR3_REQUIRED_COUNT);

    for (i = 0; i < DR3_REQUIRED_COUNT; ++i) {
        if (dr3_file_ok(dr3_required[i])) continue;

        /* more names than rows: the log has the rest (dr3_files_log already wrote them) */
        if ((missing > DR3_FILES_LIST_MAX) && (shown == DR3_FILES_LIST_MAX - 1)) {
            dr3_files_row("  ... and %d more (log)", missing - shown);
            break;
        }

        dr3_files_row("  %s", dr3_required[i]);
        ++shown;
    }

    printf("\n");
    dr3_files_row("Press A to quit.");

    dr3_bottom_flush();
}

static void dr3_files_wait(void)
{
    /* libctru reads the pad directly here: SDL's input layer is not running yet */
    do {
        svcSleepThread(16000000);           /* ~1 frame, also drains the launcher's button press */
        hidScanInput();
    } while (!(hidKeysDown() & (KEY_A | KEY_B | KEY_START | KEY_SELECT)) && !(hidKeysHeld() & KEY_TOUCH));
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
