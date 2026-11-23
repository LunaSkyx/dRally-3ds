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

static int dr3_romfs_ready;

void dr3_romfs_init(void)
{
    Result rc;

    if (dr3_romfs_ready) return;
    rc = romfsInit();
    if (R_SUCCEEDED(rc)) {
        dr3_romfs_ready = 1;
        dr3_log("[dr3] romfs mounted (built-in TRA.BPA available)");
    } else {
        dr3_log("[dr3] romfsInit returned 0x%08lx (no embedded RomFS)", (unsigned long)rc);
    }
}

void dr3_romfs_exit(void)
{
    if (!dr3_romfs_ready) return;
    romfsExit();
    dr3_romfs_ready = 0;
}

/*
 * The console on the bottom screen is 40x30 characters.  Keep every printed line <= 38 visible
 * characters so nothing wraps or scrolls the header off-screen.
 */
#define DR3_FILES_TEXT_W 38

static void dr3_files_wait(void);

static void dr3_files_row(const char *ansi, const char *fmt, ...)
{
    char    row[DR3_FILES_TEXT_W + 1];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(row, sizeof(row), fmt, ap);
    va_end(ap);

    if (ansi && ansi[0]) printf("%s%s\x1b[0m\n", ansi, row);
    else printf("%s\n", row);
}

/* the folder the game looked in - when the files ended up elsewhere its tail is the useful part */
static void dr3_files_row_cwd(void)
{
    const char *label = "Checked: ";
    const char *path  = dr3_cwd();
    const int   max   = DR3_FILES_TEXT_W - (int)strlen(label);

    if ((int)strlen(path) > max) path += strlen(path) - max;

    dr3_files_row("\x1b[37m", "%s%s", label, path);
}

static void dr3_files_show(void)
{
    const int   missing = dr3_files_missing();
    const char *miss_list[DR3_REQUIRED_COUNT];
    int         miss_n = 0;
    int         i;

    for (i = 0; i < DR3_REQUIRED_COUNT; ++i) {
        if (!dr3_file_ok(dr3_required[i])) miss_list[miss_n++] = dr3_required[i];
    }

    if (!dr3_bottom_console_ensure()) consoleInit(GFX_BOTTOM, NULL);

    printf("\x1b[2J\x1b[H");

    dr3_files_row("\x1b[1;31m", "======================================");
    dr3_files_row("\x1b[1;33m", "  DEATH RALLY - 30TH ANNIVERSARY 3DS");
    dr3_files_row("\x1b[1;31m", "======================================");
    printf("\n");

    dr3_files_row("\x1b[1;31m", "Original PC game files are missing!");
    dr3_files_row("\x1b[37m",   "Copy your Death Rally (1996) files");
    dr3_files_row("\x1b[37m",   "(UPPERCASE) to your 3DS SD card:");
    dr3_files_row("\x1b[1;36m", "  sdmc:/3ds/dRally_3ds/");
    dr3_files_row_cwd();
    if (dr3_romfs_ready) {
        dr3_files_row("\x1b[1;32m", "(Battle Arena TRA.BPA is built-in!)");
    }
    printf("\n");

    dr3_files_row("\x1b[1;33m", "Not found (%d of %d):", missing, DR3_REQUIRED_COUNT);

    /* Pack missing filenames cleanly into 2 columns (<= 38 chars/row) so all 18 fit with zero scroll */
    for (i = 0; i < miss_n; i += 2) {
        if (i + 1 < miss_n) {
            dr3_files_row("\x1b[37m", "  %-17s %s", miss_list[i], miss_list[i + 1]);
        } else {
            dr3_files_row("\x1b[37m", "  %s", miss_list[i]);
        }
    }

    printf("\n");
    dr3_files_row("\x1b[1;36m", "Press A, B, START, or tap to exit.");

    dr3_bottom_flush();
}

void dr3_files_fatal_error(const char *err)
{
    const char *msg = (err && err[0]) ? err : "Unknown asset error.";

    dr3_log("[dr3] FATAL ERROR: %s", msg);
    dr3_log_flush();

    if (!dr3_bottom_console_ensure()) consoleInit(GFX_BOTTOM, NULL);

    printf("\x1b[2J\x1b[H");
    dr3_files_row("\x1b[1;31m", "======================================");
    dr3_files_row("\x1b[1;33m", "  DEATH RALLY - ASSET ERROR");
    dr3_files_row("\x1b[1;31m", "======================================");
    printf("\n");

    /* Wrap long error text cleanly across up to 3 lines */
    {
        const char *p = msg;
        int         line;
        for (line = 0; line < 3 && *p; ++line) {
            char chunk[DR3_FILES_TEXT_W + 1];
            int  len = (int)strlen(p);
            if (len > DR3_FILES_TEXT_W) len = DR3_FILES_TEXT_W;
            memcpy(chunk, p, (size_t)len);
            chunk[len] = 0;
            dr3_files_row("\x1b[1;31m", "%s", chunk);
            p += len;
        }
    }

    printf("\n");
    dr3_files_row("\x1b[37m",   "Please verify your game files in:");
    dr3_files_row("\x1b[1;36m", "  sdmc:/3ds/dRally_3ds/");
    dr3_files_row_cwd();
    printf("\n");
    dr3_files_row("\x1b[1;36m", "Press A, B, START, or tap to exit.");

    dr3_bottom_flush();
    dr3_files_wait();
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
