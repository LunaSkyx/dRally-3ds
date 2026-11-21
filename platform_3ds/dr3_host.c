/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_host.c - the PC harness (see dr3_host.h).
 *
 * The point of this build is that the engine can be driven without a hand on the emulator:
 *
 *   * the front end is navigated by a key script (dr3_host.txt, "key <ms> <name> down|up"), pushed into
 *     the same SDL queue the real keyboard uses, so the engine walks through its own menu code;
 *   * "quickrace <ms>" skips the menu at once - it sets the field up the way the signup screen does
 *     (___31588h -> ___3079ch -> ___33010h, see ___3266ch.c) and starts the race - and F9 does the same
 *     whenever the harness runs with a window;
 *   * frame dumps (SDL_SaveBMP of the engine's own 8 bit screen) make the result visible on the PC;
 *   * "frames <n>" ends the run, so a check is a command instead of a session.
 *
 * Everything else is the engine of the 3DS build: the chaos difficulty, the grid, the passes, the
 * standings.  Only the log switches are located relative to the current directory here (the SD card is
 * sdmc:/ on the 3DS) - see the DR3_HOST paths in dr3_log.c.
 */
#include "dr3_host.h"

#include "drally.h"         /* D()/B(), the difficulty macros and the globals of the race setup */
#include "drally_structs_free.h"    /* struct_35e_t - the field dump below is the harness' own view */
#include "dr3_chaos.h"      /* DR3_MAX_CARS - the field dump walks at most that many cars */
#include "dr3_dm.h"         /* the sudden death of a deathmatch: its switch ("dmsudden <s>") */
#include "dr3_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if defined(_WIN32) && defined(_M_X64)
/*
 * The crash report (dr3_host_crash_filter below) is the only platform specific part of the harness and it
 * is the only reason this file knows Windows at all: dbghelp ships with the system, its import library with
 * the Windows SDK the host project links against anyway.  The 3DS build never compiles this file.
 */
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

/* ------------------------------------------------------------- the engine side --- */

extern __BYTE__ ___1a1f64h[];   /* the track of each of the three races, then its signed up car count */
extern __BYTE__ ___1a0ef8h[];   /* the field: three races of four racers                          */
extern __BYTE__ ___1a1ef8h[];   /* the player's racer (an index into s_6c)                        */
extern __BYTE__ ___1a1028h[];   /* which of the three races is the next one                       */
extern __BYTE__ ___1a103ch[];   /* the player's car index inside that race                        */
extern __BYTE__ ___1a01e0h[];   /* s_6c: the racer roster the savegame was loaded into            */
extern __BYTE__ ___1a0f04h[];   /* the "this racer is in the event" flags (0x14 of them)         */
extern __BYTE__ ___196abch[];   /* the class ranges of the career (the field picker uses them)    */
extern const char ___106cbh[19][4];  /* "TR1".."TR9", "TR1".."TR9" (driven backwards), "TR0"      */
extern __POINTER__ ___1a0f8ch;   /* the PREPARE TO RACE banner, loaded by menu_main's ___2faf0h    */
extern __POINTER__ ___1a1114h;   /* the menu's decode buffer, allocated by ___2faf0h as well       */

void ___31588h(void);            /* a fresh event: three random tracks, empty field (___31588h.c)  */
void ___3079ch_cdecl(int);       /* fills the field of the three races (___3079ch.c)               */
void ___33010h_cdecl(int);       /* "prepare to race" + race_main (___33010h.c)                    */

/* --------------------------------------------------------------------- state --- */

#define DR3_HOST_MAX_KEY_LINES 4096
#define DR3_HOST_MAX_DUMPS     512

typedef struct dr3_host_key_s {
    int  ms;                    /* when it is pushed, counted from the start of the run */
    int  scancode;
    int  down;
    int  done;
} dr3_host_key_t;

typedef struct dr3_host_dump_s {
    int  ms;                    /* 0 with "dumpevery": the period, then it repeats */
    int  every;
    char path[512];
    int  n;
    int  last;                  /* the time of the last "dumpevery" dump */
    int  done;                  /* set once a "dump" fired */
} dr3_host_dump_t;

static struct {
    unsigned int  t0;
    int           frames;           /* stop after that many presented frames (0 = never) */
    int           frames_done;
    int           quit_ms;
    int           quickrace_ms;     /* start a race at once when this time is reached (0 = never) */
    int           quickrace_done;
    int           signup_ms;        /* call the *real* sign up screen at this time (0 = never) */
    int           signup_done;
    int           raced_want;       /* "raced 1": the career counts as raced (see dr3_host_raced_set) */
    int           quickrace_waiting;/* the "waiting for the menu" hint is in the log already */
    int           track;            /* the track the quick race is pinned to (-1 = the one the game picked) */
    int           slot;             /* which of the three races the quick race uses */
    int           dumps;            /* dumps written (the %d of the file name) */
    dr3_host_key_t key[DR3_HOST_MAX_KEY_LINES];
    int           key_n;
    int           autokey_ms;
    int           autokey_sc;
    unsigned int  autokey_last;
    int           autokey_down_until;
    dr3_host_dump_t dump[DR3_HOST_MAX_DUMPS];
    int           dump_n;
    int           dump_now;         /* F10: dump the next presented frame */
    int           screen_ms;        /* "screenevery <ms>": dump the *presented* frame every ms (0 = never) */
    int           screen_last;
    int           screen_dumps;
    int           f9, f10;          /* the edge of the two harness keys */
    int           report_last;      /* the last "still alive" line (once a second) */
    int           running;
    int           autopilot;        /* the engine's AI drives the player's car too (dr3_host.h) */
    int           field_ms;         /* "field <ms>": dump the whole field every ms (0 = never) */
    int           field_last;
    float         power;            /* "power <x>": the player's car power for the run (the payout check) */
    int           hp[DR3_MAX_CARS]; /* "hp <car> <value>": the starting HP of one car (-1 = as set up) */
    int           nomine;           /* "nomine <0|1>": no car lays a mine for the whole race */
    int           ammo;             /* "ammo <0|1>": the in race cheat of the game: full ammo for the player */
    int           weapon;           /* "weapon <0|1>": the player's car carries the machine gun from the start */
    int           nohurt;           /* "nohurt <0|1>": the in race cheat of the game: full HP for the player */
    int           laps;             /* "laps <n>": the race length of the run (0 = as the race set up) */
    int           scenario_done;    /* the scenario switches are applied once per race */
} H;

static void dr3_host_out(const char * fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);

    /* the engine log is the report, so the harness writes its own lines into it as well */
    {
        char line[1024];

        va_start(ap, fmt);
        vsnprintf(line, sizeof(line), fmt, ap);
        va_end(ap);

        dr3_log("[host] %s", line);
    }
}

#if defined(_WIN32) && defined(_M_X64)
/*
 * A run that dies in the engine used to leave nothing behind: the log stops where the engine stopped and
 * the only trace of it was the exit code (0xC0000005 = access violation) - no line, no address, no
 * function.  The harness therefore reports the death itself: the exception, the address that was touched,
 * and - with the PDB that the host build writes next to its exe - the function and the line of the crashing
 * frame plus its callers.  The report goes through dr3_host_out(), so it lands in the same log.
 */
#define DR3_HOST_CRASH_FRAMES 20

static void dr3_host_crash_frame(HANDLE proc, unsigned long long addr, const char * what)
{
    union { SYMBOL_INFO info; char pad[sizeof(SYMBOL_INFO) + 256]; }  sym;
    IMAGEHLP_LINE64   line;
    IMAGEHLP_MODULE64 mod;
    DWORD64           off = 0;
    char              name[256];
    char              where[512];

    memset(&sym, 0, sizeof(sym));
    sym.info.SizeOfStruct = sizeof(SYMBOL_INFO);
    sym.info.MaxNameLen   = 250;

    if (SymFromAddr(proc, addr, &off, &sym.info) != FALSE)
        snprintf(name, sizeof(name), "%s+0x%x", sym.info.Name, (unsigned)off);
    else
        snprintf(name, sizeof(name), "?");

    where[0] = 0;
    memset(&line, 0, sizeof(line));
    line.SizeOfStruct = sizeof(line);

    {
        /* the two symbol APIs disagree: SymFromAddr takes a DWORD64, SymGetLineFromAddr64 a DWORD */
        DWORD off_line = 0;

        if (SymGetLineFromAddr64(proc, addr, &off_line, &line) != FALSE)
            snprintf(where, sizeof(where), "  (%s:%u)", line.FileName, (unsigned)line.LineNumber);
    }

    memset(&mod, 0, sizeof(mod));
    mod.SizeOfStruct = sizeof(mod);

    if (SymGetModuleInfo64(proc, addr, &mod) == FALSE)
        dr3_host_out("crash: %-8s %016llx  %s%s", what, addr, name, where);
    else
        dr3_host_out("crash: %-8s %016llx  %s%s  [%s]", what, addr, name, where,
                     mod.ImageName[0] ? mod.ImageName : "?");
}

static LONG WINAPI dr3_host_crash_filter(EXCEPTION_POINTERS * info)
{
    static const char * const kind[3] = { "read", "write", "execute" };
    HANDLE       proc = GetCurrentProcess();
    CONTEXT      ctx  = *info->ContextRecord;
    STACKFRAME64 frame;
    int          n;

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, NULL, TRUE);            /* the PDB of the exe, as the linker recorded it */

    dr3_host_out("crash: exception %08lx at %016llx after %d frame(s) - this is where the run ends",
                 (unsigned long)info->ExceptionRecord->ExceptionCode,
                 (unsigned long long)(ULONG_PTR)info->ExceptionRecord->ExceptionAddress, H.frames_done);

    if (((info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) ||
         (info->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR)) &&
        (info->ExceptionRecord->NumberParameters >= 2)) {

        const unsigned long long what = info->ExceptionRecord->ExceptionInformation[0];

        dr3_host_out("crash: invalid %s of %016llx",
                     (what < 3) ? kind[what] : "access",
                     (unsigned long long)info->ExceptionRecord->ExceptionInformation[1]);
    }

    dr3_host_crash_frame(proc, (unsigned long long)(ULONG_PTR)info->ExceptionRecord->ExceptionAddress,
                         "fault");

    memset(&frame, 0, sizeof(frame));
    frame.AddrPC.Offset    = ctx.Rip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode   = AddrModeFlat;

    for (n = 0; n < DR3_HOST_CRASH_FRAMES; ++n) {

        char what[32];

        if (StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &frame, &ctx, NULL,
                        SymFunctionTableAccess64, SymGetModuleBase64, NULL) == FALSE) break;
        if (frame.AddrPC.Offset == 0) break;

        snprintf(what, sizeof(what), "frame %d", n);
        dr3_host_crash_frame(proc, (unsigned long long)frame.AddrPC.Offset, what);
    }

    dr3_log_flush();

    return EXCEPTION_EXECUTE_HANDLER;           /* the process still ends - but the log says where */
}
#endif

/* ------------------------------------------------------------------- keyboard --- */

typedef struct { const char * name; SDL_Scancode sc; } dr3_host_keyname_t;

/*
 * The names the engine cares about (DR_SCAN_* in drally_keyboard.h) plus the ones a script may want for
 * the menus.  The names are the SDL ones, so dr3_host.txt reads like the key it stands for.
 */
static const dr3_host_keyname_t dr3_host_keynames[] = {
    { "return",     SDL_SCANCODE_RETURN },    { "enter",     SDL_SCANCODE_RETURN },
    { "kp_enter",   SDL_SCANCODE_KP_ENTER },  { "escape",    SDL_SCANCODE_ESCAPE },
    { "space",      SDL_SCANCODE_SPACE },     { "tab",       SDL_SCANCODE_TAB },
    { "backspace",  SDL_SCANCODE_BACKSPACE },
    { "up",         SDL_SCANCODE_UP },        { "down",      SDL_SCANCODE_DOWN },
    { "left",       SDL_SCANCODE_LEFT },      { "right",     SDL_SCANCODE_RIGHT },
    { "kp_8",       SDL_SCANCODE_KP_8 },      { "kp_2",      SDL_SCANCODE_KP_2 },
    { "kp_4",       SDL_SCANCODE_KP_4 },      { "kp_6",      SDL_SCANCODE_KP_6 },
    { "kp_5",       SDL_SCANCODE_KP_5 },      { "kp_plus",   SDL_SCANCODE_KP_PLUS },
    { "kp_minus",   SDL_SCANCODE_KP_MINUS },  { "kp_0",      SDL_SCANCODE_KP_0 },
    { "home",       SDL_SCANCODE_HOME },      { "end",       SDL_SCANCODE_END },
    { "pageup",     SDL_SCANCODE_PAGEUP },    { "pagedown",  SDL_SCANCODE_PAGEDOWN },
    { "insert",     SDL_SCANCODE_INSERT },    { "delete",    SDL_SCANCODE_DELETE },
    { "lshift",     SDL_SCANCODE_LSHIFT },    { "rshift",    SDL_SCANCODE_RSHIFT },
    { "lctrl",      SDL_SCANCODE_LCTRL },     { "rctrl",     SDL_SCANCODE_RCTRL },
    { "lalt",       SDL_SCANCODE_LALT },      { "ralt",      SDL_SCANCODE_RALT },
    { "minus",      SDL_SCANCODE_MINUS },     { "equals",    SDL_SCANCODE_EQUALS },
    { "0", SDL_SCANCODE_0 }, { "1", SDL_SCANCODE_1 }, { "2", SDL_SCANCODE_2 },
    { "3", SDL_SCANCODE_3 }, { "4", SDL_SCANCODE_4 }, { "5", SDL_SCANCODE_5 },
    { "6", SDL_SCANCODE_6 }, { "7", SDL_SCANCODE_7 }, { "8", SDL_SCANCODE_8 },
    { "9", SDL_SCANCODE_9 },
    { "a", SDL_SCANCODE_A }, { "b", SDL_SCANCODE_B }, { "c", SDL_SCANCODE_C },
    { "d", SDL_SCANCODE_D }, { "e", SDL_SCANCODE_E }, { "f", SDL_SCANCODE_F },
    { "g", SDL_SCANCODE_G }, { "h", SDL_SCANCODE_H }, { "i", SDL_SCANCODE_I },
    { "j", SDL_SCANCODE_J }, { "k", SDL_SCANCODE_K }, { "l", SDL_SCANCODE_L },
    { "m", SDL_SCANCODE_M }, { "n", SDL_SCANCODE_N }, { "o", SDL_SCANCODE_O },
    { "p", SDL_SCANCODE_P }, { "q", SDL_SCANCODE_Q }, { "r", SDL_SCANCODE_R },
    { "s", SDL_SCANCODE_S }, { "t", SDL_SCANCODE_T }, { "u", SDL_SCANCODE_U },
    { "v", SDL_SCANCODE_V }, { "w", SDL_SCANCODE_W }, { "x", SDL_SCANCODE_X },
    { "y", SDL_SCANCODE_Y }, { "z", SDL_SCANCODE_Z },
    { "f1",  SDL_SCANCODE_F1 },  { "f2",  SDL_SCANCODE_F2 },  { "f3",  SDL_SCANCODE_F3 },
    { "f4",  SDL_SCANCODE_F4 },  { "f5",  SDL_SCANCODE_F5 },  { "f6",  SDL_SCANCODE_F6 },
    { "f7",  SDL_SCANCODE_F7 },  { "f8",  SDL_SCANCODE_F8 },  { "f9",  SDL_SCANCODE_F9 },
    { "f10", SDL_SCANCODE_F10 }, { "f11", SDL_SCANCODE_F11 }, { "f12", SDL_SCANCODE_F12 },
    { (const char *)0, SDL_SCANCODE_UNKNOWN }
};

static int dr3_host_scancode(const char * name)
{
    int i;

    for (i = 0; dr3_host_keynames[i].name; ++i)
        if (strcmp(dr3_host_keynames[i].name, name) == 0) return (int)dr3_host_keynames[i].sc;

    return -1;
}

/*
 * Pushes one key into SDL's event queue.  IO_Loop() polls with SDL_PollEvent() in this build (events.c),
 * so the engine gets it through dRally_Keyboard_make/break exactly like a real key - a menu that waits
 * for a key cannot tell the difference.
 */
static void dr3_host_key_push(int scancode, int down)
{
    SDL_Event e;

    if (scancode < 0) return;

    memset(&e, 0, sizeof(e));
    e.type                  = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.type              = e.type;
    e.key.state             = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.repeat            = 0;
    e.key.keysym.scancode   = (SDL_Scancode)scancode;
    e.key.keysym.sym        = SDL_GetKeyFromScancode((SDL_Scancode)scancode);
    e.key.keysym.mod        = KMOD_NONE;

    SDL_PushEvent(&e);
}

/* ------------------------------------------------------------------- the script --- */

static void dr3_host_trim(char * s)
{
    char * p = s;
    size_t n;

    while (*p == ' ' || *p == '\t') ++p;
    if (p != s) memmove(s, p, strlen(p) + 1);

    n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = 0;
}

/* the pointer to the n-th whitespace separated token of a line (n = 0 is the command itself) */
static char * dr3_host_token(char * line, int n)
{
    char * p = line;

    while (n-- > 0) {
        while (*p && *p != ' ' && *p != '\t') ++p;
        while (*p == ' ' || *p == '\t') ++p;
    }

    return p;
}

static void dr3_host_switch_file(const char * path, int value)
{
    FILE * f = fopen(path, "w");

    if (!f) {
        dr3_host_out("cannot write %s - is the run directory writable?", path);
        return;
    }

    fprintf(f, "%d\n", value);
    fclose(f);
}

#if defined(DR3_CHAOS)
#include "dr3_chaos.h"      /* dr3_chaos_extra_cars_set(), the ladder cycles */

/*
 * The selector of the sign up screen only spins once a race of *this* career has been run (dr3_log.c:
 * dr3_chaos_raced - the career counter of the loaded savegame).  Driving a whole race for that takes
 * minutes, so a run that only wants to see the wheel says "raced 1" instead: the counter of the player's
 * racer in s_6c is set here, nothing else.
 */
static void dr3_host_raced_set(int on)
{
    const int me = (int)D(___1a1ef8h);

    if ((me < 0) || (me >= 0x14)) return;

    if (on && ((int)D(___1a01e0h + 0x6c*me + 0x50) <= 0)) D(___1a01e0h + 0x6c*me + 0x50) = 1;
}

static void dr3_host_set_cycle(int want, int (*get)(void), int (*cycle)(void), int count, const char * what)
{
    int n = 0;

    while ((get() != want) && (n < (count + 1))) { cycle(); ++n; }

    dr3_host_out("%s is %d now (%d step(s))", what, get(), n);
}
#endif

/* ------------------------------------------------------------------ quick race --- */

/*
 * The autopilot switch (dr3_host.h).  The engine asks for it (race_main.c, DR3_HOST only): it is on
 * whenever the script or the command line said "auto 1", and off for every run that does not - H is
 * static, so this is 0 before the harness was initialised as well.
 */
int dr3_host_autopilot(void)
{
    return H.autopilot;
}

/*
 * The race length of the run (dr3_host.h).  The engine asks for it in ___33010h.c right before race_main,
 * so this only ever decides how long the race that follows is - and it is asked once per race, not per
 * frame, because the engine counts the laps off NUM_OF_LAPS as the race runs.
 */
int dr3_host_laps(int asked)
{
    return (H.laps > 0) ? H.laps : asked;
}

/*
 * The direct race start.
 *
 * ___3266ch.c is the screen the player signs up on and it does three things before it calls
 * ___33010h_cdecl(4): the event is set up (___31588h - three tracks and an empty field), the player is
 * entered into the race he picked and the other two races are filled (___3079ch_cdecl in a loop until all
 * three have their four cars).  This does the same without the screen - the race that follows is the race
 * the menu would have started, only the screens between are skipped.
 *
 * Without a loaded savegame there is no roster and no class ranges, so the field cannot be filled: that is
 * checked and reported instead of crashing the run.
 */
void dr3_host_quick_race(int track_slot)
{
    const int player = (int)D(___1a1ef8h);
    int       i, at, tries;

    if ((player < 0) || (player > 0x13)) {
        dr3_host_out("quick race: no racer loaded yet (player index %d) - drive the front end first", player);
        return;
    }

    if ((track_slot < 0) || (track_slot > 2)) track_slot = 0;

    dr3_host_out("quick race: player %d, slot %d (class ranges %d/%d/%d/%d)",
                 player, track_slot,
                 (int)B(___196abch), (int)B(___196abch+1), (int)B(___196abch+2), (int)B(___196abch+3));

    /*
     * The Chaos level is picked in the licence screen and picking it does two things: it sets the
     * difficulty and the flag the extra cars hang off (___3ab5ch.c: dr3_chaos_wanted = (edi ==
     * DR3_DIFFICULTY_CHAOS)).  A direct start has to do the same, otherwise the race that follows would
     * be the plain four car race.
     */
    ___196a94h_difficulty = DR3_DIFFICULTY_CHAOS;
    dr3_chaos_wanted      = 1;

    dr3_host_out("quick race: difficulty %d (chaos %s), %d extra car(s) asked for",
                 (int)___196a94h_difficulty, dr3_chaos_active() ? "on" : "OFF", dr3_chaos_extra_cars());

    dr3_host_out("quick race: step 1/3 - the event setup (___31588h)");
    ___31588h();                                    /* three tracks, empty field (the event setup) */
    dr3_host_out("quick race: step 1/3 done (___31588h returned)");

    if (H.track >= 0) B(___1a1f64h + track_slot) = H.track;     /* pinned track, if the script asked for one */

    /*
     * The field first, the player afterwards.
     *
     * ___3079ch_cdecl(1) fills the races one candidate at a time and picks the race for each one with
     * "rand()%3 while that race still has a free slot" - if it is asked for one car too many (because the
     * player was entered by hand before the field was full), that loop never ends.  So the field is
     * filled exactly the way the signup does it and the player takes his place afterwards.
     */
    dr3_host_out("quick race: step 2/3 - filling the three races (the picker's own rule, see below)");

    tries = 0;

    /*
     * The field is filled here instead of through the picker screen (___3079ch_cdecl).
     *
     * The picker picks the race it fills with rand()%3 in an unprotected "while (1)" and draws the whole
     * 640x480 screen for every single car; on the PC it can sit in that choice for minutes - the runs
     * looked like a hang - and when the random choice keeps landing on a race that is already full, that
     * loop cannot end at all (it has no counter, unlike the candidate loop below it).  What the race needs
     * is the field array and the counters, so they are filled the same way the picker does it: every racer
     * that is not in the event yet goes into the first race that still has a slot.  Nothing else of the
     * picker's work is used by the race (his screen is the front end's).
     */
    {
        int tier, placed_here = 0;

        for (tier = 0; tier < 3; ++tier) {

            while (B(___1a1f64h + tier + 3) < 4) {

                int free_racer = -1;

                for (i = 0; i < 0x14; ++i) if (B(___1a0f04h + i) != 1) { free_racer = i; break; }
                if (free_racer < 0) break;

                B(___1a0f04h + free_racer) = 1;
                B(___1a0ef8h + 4 * tier + B(___1a1f64h + tier + 3)) = free_racer;
                B(___1a1f64h + tier + 3)++;
                ++placed_here;
            }
        }

        if (placed_here > 0)
            dr3_host_out("quick race: %d car(s) placed by the picker's rule", placed_here);
    }

    dr3_host_out("quick race: step 2/3 done - field %d/%d/%d after %d picker step(s)",
                 (int)B(___1a1f64h+3), (int)B(___1a1f64h+4), (int)B(___1a1f64h+5), tries);

    /* the player into the race he picked (his own slot if he is already in that field) */
    {
        int found = -1;

        for (i = 0; i < 4; ++i) if (B(___1a0ef8h + 4 * track_slot + i) == player) found = i;

        if (found < 0) {
            found = 3;                              /* the last slot of that race */
            B(___1a0ef8h + 4 * track_slot + found) = player;
            dr3_host_out("quick race: the player was not in the field - he takes slot %d of race %d",
                         found, track_slot);
        }

        at = found;
        D(___1a1028h) = track_slot;
        D(___1a103ch) = at;
    }

    dr3_host_out("quick race: track %s (byte %d), field %d/%d/%d after %d fill step(s), player is car %d",
                 (B(___1a1f64h + track_slot) < 19) ? ___106cbh[B(___1a1f64h + track_slot)] : "??",
                 (int)B(___1a1f64h + track_slot),
                 (int)B(___1a1f64h+3), (int)B(___1a1f64h+4), (int)B(___1a1f64h+5), tries + 1,
                 (int)D(___1a103ch));

    dr3_chaos_trace("host quick race", 4);

    dr3_host_out("quick race: step 3/3 - PREPARE TO RACE + race_main (___33010h)");
    ___33010h_cdecl(4);                             /* PREPARE TO RACE + race_main */

    dr3_host_out("quick race: race_main returned");
}

/* --------------------------------------------------------------------- script --- */

static unsigned int dr3_host_now(void)
{
    if (!H.running) {
        H.t0 = SDL_GetTicks();
        H.running = 1;
    }

    return SDL_GetTicks() - H.t0;
}

static void dr3_host_key_add(int ms, int scancode, int down)
{
    if (H.key_n >= DR3_HOST_MAX_KEY_LINES) return;

    H.key[H.key_n].ms       = ms;
    H.key[H.key_n].scancode = scancode;
    H.key[H.key_n].down     = down;
    H.key[H.key_n].done     = 0;

    ++H.key_n;
}

/* the pass the ladder has switched off right now (0 = all of them drawn) */
static int dr3_host_pass_get(void)
{
    int i;

    for (i = 1; i < DR3_PASS_COUNT; ++i)
        if (dr3_chaos_skip_pass(i)) return i;

    return DR3_PASS_NONE;
}

/*
 * The key script (dr3_keys.txt, or whatever "keys <path>" names): one "<ms> <key> down|up" per line.
 * The times are counted from the start of the run and the keys are pushed in that order, so walking the
 * menus is a text file instead of a list of hand movements - and the same file runs the same route twice.
 */
static void dr3_host_keys_file(const char * path)
{
    FILE * f = fopen(path, "r");
    char   line[512];
    int    at = 0, added = 0;

    if (!f) {
        dr3_host_out("key script %s not found", path);
        return;
    }

    while (fgets(line, sizeof(line), f)) {

        char name[64], act[16];
        int  ms, sc;

        ++at;
        dr3_host_trim(line);
        if (!line[0] || line[0] == '#' || line[0] == ';') continue;

        if (sscanf(line, "%d %63s %15s", &ms, name, act) != 3) {
            dr3_host_out("%s(%d): expected '<ms> <key> down|up'", path, at);
            continue;
        }

        sc = dr3_host_scancode(name);

        if (sc < 0) {
            dr3_host_out("%s(%d): unknown key '%s'", path, at, name);
            continue;
        }

        dr3_host_key_add(ms, sc, (act[0] == 'd') || (act[0] == 'D'));
        ++added;
    }

    fclose(f);

    dr3_host_out("key script %s: %d line(s), %d queued", path, at, added);
}

static void dr3_host_dump_add(int ms, int every, const char * path)
{
    dr3_host_dump_t * d;

    if (H.dump_n >= DR3_HOST_MAX_DUMPS) {
        dr3_host_out("too many dumps in the script (max %d)", DR3_HOST_MAX_DUMPS);
        return;
    }

    d = &H.dump[H.dump_n++];

    d->ms    = ms;
    d->every = every;
    d->n     = 0;
    d->last  = 0;
    d->done  = 0;

    strncpy(d->path, path, sizeof(d->path) - 1);
    d->path[sizeof(d->path) - 1] = 0;
}

/*
 * The switch files of the chaos build as the harness reads and writes them.  dr3_log.c and dr3_dm.c hold
 * the same names for this build (their DR3_HOST branch) - they have to stay in step.
 */
#define DR3_HOST_LOG_SWITCH     "chaos_log.txt"
#define DR3_HOST_TIME_SWITCH    "chaos_time.txt"
#define DR3_HOST_EXTRAS_SWITCH  "chaos_extras.txt"
#define DR3_HOST_MODE_SWITCH    "chaos_mode.txt"
#define DR3_HOST_DM_SWITCH      "chaos_dm.txt"
#define DR3_HOST_DMLAP_SWITCH   "chaos_dm_lap.txt"

static void dr3_host_script(const char * path)
{
    FILE * f = fopen(path, "r");
    char   line[1024];
    int    at = 0;

    if (!f) {
        dr3_host_out("script %s not found - the harness uses its defaults", path);
        return;
    }

    while (fgets(line, sizeof(line), f)) {

        char  cmd[64];
        char * rest;

        ++at;
        dr3_host_trim(line);
        if (!line[0] || line[0] == '#' || line[0] == ';') continue;

        if (sscanf(line, "%63s", cmd) != 1) continue;
        rest = dr3_host_token(line, 1);
        dr3_host_trim(rest);

        if (strcmp(cmd, "log") == 0) {

            dr3_host_switch_file(DR3_HOST_LOG_SWITCH, atoi(rest));
            dr3_log_quiet_reload();
            dr3_host_out("log switch %s = %s", DR3_HOST_LOG_SWITCH, dr3_log_enabled() ? "on" : "off");
        }
        else if (strcmp(cmd, "timing") == 0) {

            dr3_host_switch_file(DR3_HOST_TIME_SWITCH, atoi(rest));
            dr3_log_quiet_reload();
            dr3_host_out("timing switch %s = %s", DR3_HOST_TIME_SWITCH, dr3_log_timing() ? "on" : "off");
        }
        else if (strcmp(cmd, "extras") == 0) {

            dr3_host_switch_file(DR3_HOST_EXTRAS_SWITCH, atoi(rest));
            dr3_chaos_extra_cars_set(atoi(rest));
            dr3_host_out("extra cars = %d", dr3_chaos_extra_cars());
        }
        else if (strcmp(cmd, "stage") == 0) {

            dr3_host_set_cycle(atoi(rest), dr3_chaos_extra_stage, dr3_chaos_extra_stage_cycle,
                               DR3_EXTRA_STAGE_ALL, "extra car stage");
        }
        else if (strcmp(cmd, "mode") == 0) {

            dr3_host_switch_file(DR3_HOST_MODE_SWITCH, atoi(rest));
            dr3_chaos_mode_set(atoi(rest));
            dr3_host_out("chaos race mode = %d (%s)", dr3_chaos_mode(), dr3_chaos_mode_name(dr3_chaos_mode()));
        }
        else if (strcmp(cmd, "spin") == 0) {

            dr3_chaos_spin_report(atoi(rest));
        }
        else if (strcmp(cmd, "raced") == 0) {

            /*
             * "raced 1" makes the career count as raced, so the wheel of the sign up screen spins without
             * driving a race first ("signup <ms>" then shows it).  The value is read back for the log.
             */
            const int me = (int)D(___1a1ef8h);

            H.raced_want = atoi(rest) ? 1 : 0;
            dr3_host_raced_set(atoi(rest));
            dr3_host_out("career races of seat %d = %d - the sign up wheel is %s", me,
                         ((me >= 0) && (me < 0x14)) ? (int)D(___1a01e0h + 0x6c*me + 0x50) : -1,
                         dr3_chaos_raced() ? "armed" : "off");
        }
        else if (strcmp(cmd, "dmsudden") == 0) {

            const int v = atoi(rest);

            dr3_host_switch_file(DR3_HOST_DM_SWITCH, v);
            dr3_dm_sudden_set(v);
            dr3_host_out("deathmatch sudden death after %d second(s) (0 = off)", dr3_dm_sudden_s());
        }
        else if (strcmp(cmd, "dmlap") == 0) {

            const int v = atoi(rest);

            dr3_host_switch_file(DR3_HOST_DMLAP_SWITCH, v);
            dr3_dm_lap_set(v);
            dr3_host_out("deathmatch lap duty: one lap every %d second(s) (0 = off)", dr3_dm_lap_s());
        }
        else if (strcmp(cmd, "pass") == 0) {

            dr3_host_set_cycle(atoi(rest), dr3_host_pass_get, dr3_chaos_skip_cycle,
                               DR3_PASS_COUNT, "switched off drawing pass");
        }
        else if (strcmp(cmd, "frames") == 0)       H.frames       = atoi(rest);
        else if (strcmp(cmd, "quit_ms") == 0)      H.quit_ms      = atoi(rest);
        else if (strcmp(cmd, "quickrace_ms") == 0) H.quickrace_ms = atoi(rest);
        else if (strcmp(cmd, "slot") == 0)         H.slot         = atoi(rest);
        else if (strcmp(cmd, "track") == 0)        H.track        = atoi(rest);
        else if (strcmp(cmd, "auto") == 0) {
            H.autopilot = (atoi(rest) != 0);
            dr3_host_out("autopilot: the engine's AI drives the player's car = %s", H.autopilot ? "on" : "off");
        }
        else if (strcmp(cmd, "field") == 0) {
            H.field_ms = atoi(rest);
            dr3_host_out("field dump every %d ms", H.field_ms);
        }
        else if (strcmp(cmd, "power") == 0) {
            H.power = (float)atof(rest);
            dr3_host_out("scenario: the player's car power = %.2f", (double)H.power);
        }
        else if (strcmp(cmd, "nomine") == 0) {
            H.nomine = (atoi(rest) != 0);
            dr3_host_out("scenario: no car lays a mine = %s", H.nomine ? "on" : "off");
        }
        else if (strcmp(cmd, "ammo") == 0) {
            H.ammo = (atoi(rest) != 0);
            dr3_host_out("scenario: full ammo for the player (the game's own cheat) = %s", H.ammo ? "on" : "off");
        }
        else if (strcmp(cmd, "weapon") == 0) {
            H.weapon = (atoi(rest) != 0);
            dr3_host_out("scenario: the player's car carries the machine gun = %s", H.weapon ? "on" : "off");
        }
        else if (strcmp(cmd, "nohurt") == 0) {
            H.nohurt = (atoi(rest) != 0);
            dr3_host_out("scenario: full HP for the player (the game's own cheat) = %s", H.nohurt ? "on" : "off");
        }
        else if (strcmp(cmd, "laps") == 0) {
            H.laps = atoi(rest);
            dr3_host_out("race length: %d laps (0 = as the race set up)", H.laps);
        }
        else if (strcmp(cmd, "hp") == 0) {

            const int car = atoi(rest);
            const int val = atoi(dr3_host_token(line, 2));

            if ((car >= 0) && (car < DR3_MAX_CARS)) {
                H.hp[car] = val;
                dr3_host_out("scenario: car %d starts with %d HP", car, val);
            }
            else dr3_host_out("%s(%d): 'hp' needs a car 0..%d", path, at, DR3_MAX_CARS - 1);
        }
        else if (strcmp(cmd, "keys") == 0)         dr3_host_keys_file(rest);
        else if (strcmp(cmd, "quickrace") == 0)    dr3_host_quick_race(atoi(rest));
        else if (strcmp(cmd, "signup") == 0)       H.signup_ms = atoi(rest);
        else if (strcmp(cmd, "dump") == 0)         dr3_host_dump_add(atoi(rest), 0, dr3_host_token(line, 2));
        else if (strcmp(cmd, "dumpevery") == 0)    dr3_host_dump_add(0, atoi(rest), dr3_host_token(line, 2));
        else if (strcmp(cmd, "screenevery") == 0) {
            H.screen_ms = atoi(rest);
            dr3_host_out("screen dump every %d ms (what the PC shows, with the car colours)", H.screen_ms);
        }
        else if (strcmp(cmd, "echo") == 0)         dr3_host_out("script: %s", rest);
        else if (strcmp(cmd, "key") == 0) {

            const int ms = atoi(rest);
            const int sc = dr3_host_scancode(dr3_host_token(line, 2));
            char      act[16], name[64];

            sscanf(dr3_host_token(line, 2), "%63s", name);
            sscanf(dr3_host_token(line, 3), "%15s", act);

            if (sc < 0) dr3_host_out("%s(%d): unknown key '%s'", path, at, name);
            else        dr3_host_key_add(ms, sc, (act[0] == 'd') || (act[0] == 'D'));
        }
        else if (strcmp(cmd, "autokey") == 0) {

            char name[64];

            H.autokey_ms = atoi(rest);
            sscanf(dr3_host_token(line, 2), "%63s", name);
            H.autokey_sc = dr3_host_scancode(name);

            if (H.autokey_sc < 0) dr3_host_out("%s(%d): unknown key '%s'", path, at, name);
            else                  dr3_host_out("autokey: '%s' every %d ms", name, H.autokey_ms);
        }
        else {
            dr3_host_out("%s(%d): unknown command '%s'", path, at, cmd);
        }
    }

    fclose(f);

    dr3_host_out("script %s: %d line(s) - frames %d, quick race at %d ms, %d key(s), %d dump(s)",
                 path, at, H.frames, H.quickrace_ms, H.key_n, H.dump_n);
}

/* ------------------------------------------------------------------ the harness --- */

static void dr3_host_dump_save(SDL_Surface * screen, const char * path, int number)
{
    char file[600];

    if (!screen) return;

    if (strchr(path, '%')) snprintf(file, sizeof(file), path, number);
    else                   snprintf(file, sizeof(file), "%s%04d.bmp", path, number);

    if (SDL_SaveBMP(screen, file) != 0) dr3_host_out("dump %s FAILED: %s", file, SDL_GetError());
    else                                dr3_host_out("dump %s (%dx%d)", file, screen->w, screen->h);
}

/*
 * The second look at a frame (dr3_host.h): dr3_host_present() gets the engine's own 8-bit screen, this one
 * gets the 32-bit image the port turned it into - converted, and with the cars of a chaos race coloured
 * (drally_linux_c.c, platform_3ds/dr3_blit.c).  That is a second BMP per step, so it only happens when
 * "screenevery <ms>" (or --screenevery) asks for it.
 */
void dr3_host_present_final(const void * px, int w, int h, int pitch)
{
    static int   screen_logged;
    SDL_Surface * shot;
    char         file[600];

    if ((H.screen_ms <= 0) || !px) return;

    {
        const unsigned int now = dr3_host_now();

        if ((int)now - H.screen_last < H.screen_ms) return;

        H.screen_last = (int)now;
    }

    /* ARGB8888 in memory, exactly what the port wrote into the texture */
    shot = SDL_CreateRGBSurfaceFrom((void *)px, w, h, 32, pitch, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    if (!shot) {

        if (!screen_logged) { dr3_host_out("screen dump: SDL_CreateRGBSurfaceFrom failed: %s", SDL_GetError()); screen_logged = 1; }
        return;
    }

    snprintf(file, sizeof(file), "frames\\w_%04d.bmp", ++H.screen_dumps);

    if (SDL_SaveBMP(shot, file) != 0) dr3_host_out("screen dump %s FAILED: %s", file, SDL_GetError());
    else                              dr3_host_out("screen dump %s (%dx%d) - what the PC shows", file, shot->w, shot->h);

    SDL_FreeSurface(shot);
}

void dr3_host_init(int argc, char ** argv)
{
    const char * script = "dr3_host.txt";
    int          i;

#if defined(_WIN32) && defined(_M_X64)
    /*
     * From here on a crash leaves a report in the log instead of stopping the log mid-line (see
     * dr3_host_crash_filter).  It is set before anything else can go wrong.
     */
    SetUnhandledExceptionFilter(dr3_host_crash_filter);
#endif

    /*
     * The switches the chaos build obeys (dr3_log.c).  A run starts with the log and the frame timing on:
     * the whole point of the harness is that the result can be read from the log afterwards.
     */
    dr3_host_switch_file(DR3_HOST_LOG_SWITCH, 1);
    dr3_host_switch_file(DR3_HOST_TIME_SWITCH, 1);
    dr3_log_quiet_reload();

    H.slot  = 0;
    H.track = -1;

    /* no car is weakened until a "hp <car> <value>" says so (dr3_host_scenario) */
    for (i = 0; i < DR3_MAX_CARS; ++i) H.hp[i] = -1;

    /* first the script, so a script can be named on the command line at all */
    for (i = 1; i < argc; ++i)
        if ((strcmp(argv[i], "--script") == 0) && (i + 1 < argc)) script = argv[i + 1];

    dr3_host_script(script);

    /*
     * Then the command line.  It overrides what the script says, so a one-off check does not need a script
     * of its own:
     *
     *   dRally3DSHost.exe --frames 3000 --quickrace 4000 --extras 18 --slot 0 --track 4 --keys x.txt
     *                     --dumpevery 500 frames\f_%04d.bmp
     */
    for (i = 1; i < argc; ++i) {

        if (strcmp(argv[i], "--script") == 0 && (i + 1 < argc))       ++i;   /* already read above */
        else if (strcmp(argv[i], "--keys") == 0 && (i + 1 < argc))    dr3_host_keys_file(argv[++i]);
        else if (strcmp(argv[i], "--frames") == 0 && (i + 1 < argc))  H.frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--quit_ms") == 0 && (i + 1 < argc)) H.quit_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--quickrace") == 0 && (i + 1 < argc)) H.quickrace_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--signup") == 0 && (i + 1 < argc))    H.signup_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--extras") == 0 && (i + 1 < argc)) {

            const int v = atoi(argv[++i]);
            dr3_host_switch_file(DR3_HOST_EXTRAS_SWITCH, v);
            dr3_chaos_extra_cars_set(v);
        }
        else if (strcmp(argv[i], "--track") == 0 && (i + 1 < argc))   H.track = atoi(argv[++i]);
        else if (strcmp(argv[i], "--mode") == 0 && (i + 1 < argc)) {

            const int v = atoi(argv[++i]);
            dr3_host_switch_file(DR3_HOST_MODE_SWITCH, v);
            dr3_chaos_mode_set(v);
            dr3_host_out("chaos race mode = %d (%s)", dr3_chaos_mode(), dr3_chaos_mode_name(dr3_chaos_mode()));
        }
        else if (strcmp(argv[i], "--spin") == 0 && (i + 1 < argc)) {

            dr3_chaos_spin_report(atoi(argv[++i]));
        }
        else if (strcmp(argv[i], "--dmsudden") == 0 && (i + 1 < argc)) {

            const int v = atoi(argv[++i]);

            dr3_host_switch_file(DR3_HOST_DM_SWITCH, v);
            dr3_dm_sudden_set(v);
            dr3_host_out("deathmatch sudden death after %d second(s) (0 = off)", dr3_dm_sudden_s());
        }
        else if (strcmp(argv[i], "--dmlap") == 0 && (i + 1 < argc)) {

            const int v = atoi(argv[++i]);

            dr3_host_switch_file(DR3_HOST_DMLAP_SWITCH, v);
            dr3_dm_lap_set(v);
            dr3_host_out("deathmatch lap duty: one lap every %d second(s) (0 = off)", dr3_dm_lap_s());
        }
        else if (strcmp(argv[i], "--slot") == 0 && (i + 1 < argc))    H.slot = atoi(argv[++i]);
        else if (strcmp(argv[i], "--auto") == 0 && (i + 1 < argc)) {

            H.autopilot = (atoi(argv[++i]) != 0);
            dr3_host_out("autopilot: the engine's AI drives the player's car = %s", H.autopilot ? "on" : "off");
        }
        else if (strcmp(argv[i], "--field") == 0 && (i + 1 < argc)) {

            H.field_ms = atoi(argv[++i]);
            dr3_host_out("field dump every %d ms", H.field_ms);
        }
        else if (strcmp(argv[i], "--power") == 0 && (i + 1 < argc)) {

            H.power = (float)atof(argv[++i]);
            dr3_host_out("scenario: the player's car power = %.2f", (double)H.power);
        }
        else if (strcmp(argv[i], "--nomine") == 0 && (i + 1 < argc)) {

            H.nomine = (atoi(argv[++i]) != 0);
            dr3_host_out("scenario: no car lays a mine = %s", H.nomine ? "on" : "off");
        }
        else if (strcmp(argv[i], "--ammo") == 0 && (i + 1 < argc)) {

            H.ammo = (atoi(argv[++i]) != 0);
            dr3_host_out("scenario: full ammo for the player (the game's own cheat) = %s", H.ammo ? "on" : "off");
        }
        else if (strcmp(argv[i], "--weapon") == 0 && (i + 1 < argc)) {

            H.weapon = (atoi(argv[++i]) != 0);
            dr3_host_out("scenario: the player's car carries the machine gun = %s", H.weapon ? "on" : "off");
        }
        else if (strcmp(argv[i], "--nohurt") == 0 && (i + 1 < argc)) {

            H.nohurt = (atoi(argv[++i]) != 0);
            dr3_host_out("scenario: full HP for the player (the game's own cheat) = %s", H.nohurt ? "on" : "off");
        }
        else if (strcmp(argv[i], "--laps") == 0 && (i + 1 < argc)) {

            H.laps = atoi(argv[++i]);
            dr3_host_out("race length: %d laps (0 = as the race set up)", H.laps);
        }
        else if (strcmp(argv[i], "--hp") == 0 && (i + 2 < argc)) {

            const int car = atoi(argv[i + 1]);
            const int val = atoi(argv[i + 2]);

            i += 2;

            if ((car >= 0) && (car < DR3_MAX_CARS)) {
                H.hp[car] = val;
                dr3_host_out("scenario: car %d starts with %d HP", car, val);
            }
            else dr3_host_out("--hp needs a car 0..%d", DR3_MAX_CARS - 1);
        }
        else if (strcmp(argv[i], "--autokey") == 0 && (i + 2 < argc)) {

            H.autokey_ms = atoi(argv[++i]);
            H.autokey_sc = dr3_host_scancode(argv[++i]);

            if (H.autokey_sc < 0) dr3_host_out("--autokey: unknown key '%s'", argv[i]);
        }
        else if (strcmp(argv[i], "--dumpevery") == 0 && (i + 2 < argc)) {

            const int ms = atoi(argv[i + 1]);
            dr3_host_dump_add(0, ms, argv[i + 2]);
            i += 2;
        }
        else if (strcmp(argv[i], "--screenevery") == 0 && (i + 1 < argc)) {

            H.screen_ms = atoi(argv[++i]);
            dr3_host_out("screen dump every %d ms (what the PC shows, with the car colours)", H.screen_ms);
        }
        else if (strcmp(argv[i], "--dumpat") == 0 && (i + 2 < argc)) {

            const int ms = atoi(argv[i + 1]);
            dr3_host_dump_add(ms, 0, argv[i + 2]);
            i += 2;
        }
    }

    /*
 * Autokey keeps the key down for a while: the menus of this engine read the held key (kmap) in their own
 * loops, so a 4 ms pulse can fall between two of their polls and be missed completely.
 */
#define DR3_HOST_AUTOKEY_HOLD_MS 120

    /* a key script that was not named is looked for anyway (dr3_keys.txt) */
    if (H.key_n == 0) dr3_host_keys_file("dr3_keys.txt");

    dr3_host_out("harness: script %s, CWD must hold ENGINE.BPA", script);
    dr3_host_out("harness: log %s, timing %s, %d extra car(s), stage %d, pass %d, track %d, slot %d, "
                 "frames %d, quick race at %d ms, autopilot %s, field %d ms",
                 dr3_log_enabled() ? "on" : "off", dr3_log_timing() ? "on" : "off",
                 dr3_chaos_extra_cars(), dr3_chaos_extra_stage(), dr3_host_pass_get(), H.track, H.slot,
                 H.frames, H.quickrace_ms, H.autopilot ? "on" : "off", H.field_ms);
}

void dr3_host_shutdown(void)
{
    const unsigned int ms = dr3_host_now();

    dr3_host_out("harness: %d frame(s) in %u ms (%.1f frame/s), %d dump(s), %d key(s) queued, log %d line(s)",
                 H.frames_done, ms, (ms != 0) ? (1000.0 * H.frames_done / (double)ms) : 0.0,
                 H.dumps, H.key_n, dr3_log_lines());

    dr3_log_flush();
}

/*
 * Is the menu up - can the direct start run at all?  ___33010h_cdecl() copies the PREPARE TO RACE banner
 * out of ___1a0f8ch (prep4.bpk) and the faces of the roster out of ___19de70h; both come from menu_main()'s
 * ___2faf0h(), which runs *after* the intro.  Started before that, those pointers are still NULL and the
 * engine dies in a strcpy/memcpy - an access violation inside VCRUNTIME140 with nothing in the log.  That
 * is what "quickrace 8000" looks like against the ~80 s intro of this game (the direct start fired in the
 * middle of it; a "key 9000 return down" in dr3_keys.txt skips the intro).  ___1a1114h is the buffer
 * ___2faf0h() allocates before it decodes anything, so the two together say "the menu has its assets".
 */
static int dr3_host_menu_ready(void)
{
    return (___1a0f8ch != 0) && (___1a1114h != 0);
}

/*
 * IO_Loop() runs from every wait loop of the engine (thousands of times per second), so the timed part of
 * the script lives here: keys are pushed when their time has come, the direct race start fires, and the
 * two harness keys (F9, F10) are watched.  All of it is driven by milliseconds, not by frames - a menu
 * that waits for a key does not present a frame while it waits, and the script has to keep running anyway.
 */
void dr3_host_poll(void)
{
    const unsigned int now = dr3_host_now();
    const Uint8 *      keys;
    int                i;

    for (i = 0; i < H.key_n; ++i) {

        if (H.key[i].done) continue;
        if ((int)now < H.key[i].ms) continue;

        H.key[i].done = 1;
        dr3_host_key_push(H.key[i].scancode, H.key[i].down);
    }

    if ((H.autokey_ms > 0) && (H.autokey_sc > 0)) {

        if ((int)now - H.autokey_last >= H.autokey_ms) {

            H.autokey_last       = (int)now;
            H.autokey_down_until = (int)now + DR3_HOST_AUTOKEY_HOLD_MS;
            dr3_host_key_push(H.autokey_sc, 1);
        }

        if (H.autokey_down_until && ((int)now >= H.autokey_down_until)) {

            H.autokey_down_until = 0;
            dr3_host_key_push(H.autokey_sc, 0);
        }
    }

    if ((H.quickrace_ms > 0) && !H.quickrace_done && ((int)now >= H.quickrace_ms)) {

        if (dr3_host_menu_ready()) {

            H.quickrace_done = 1;
            dr3_host_quick_race(H.slot);
        }
        else if (!H.quickrace_waiting) {

            H.quickrace_waiting = 1;
            dr3_host_out("quick race: holding - the engine is still in its intro/title screen and the menu "
                         "has not loaded its assets yet (a 'key %d return down' skips the intro)",
                         H.quickrace_ms);
        }
    }

    /*
     * The sign up screen itself ("signup <ms>" / --signup <ms>): the harness' quick race *builds the field
     * the way* that screen does (dr3_host_quick_race), so the screen itself never runs in a normal harness
     * run - and that screen is where the chaos modes are offered and where the random selector spins
     * (___3266ch.c).  This switch calls the real screen instead, so the wheel can be watched (and dumped)
     * in a run.  It needs the menu to be up, like the quick race.
     */
    if ((H.signup_ms > 0) && !H.signup_done && ((int)now >= H.signup_ms)) {

        if (dr3_host_menu_ready()) {

            extern void ___3266ch(void);

            H.signup_done = 1;

            /*
             * "raced 1" is read at startup, before the savegame is loaded - the counter the wheel reads
             * (dr3_chaos_raced, s_6c) is set again here, right before the screen that reads it.
             */
            if (H.raced_want) dr3_host_raced_set(1);

            dr3_host_out("signup: entering the real sign up screen (___3266ch) - the wheel lives there");

            ___3266ch();

            dr3_host_out("signup: the screen is back");
        }
    }

    keys = SDL_GetKeyboardState(NULL);

    if (keys) {

        if (keys[SDL_SCANCODE_F9]) {
            if (!H.f9) {
                H.f9 = 1;
                if (dr3_host_menu_ready()) { dr3_host_out("F9: direct race start"); dr3_host_quick_race(H.slot); }
                else                       dr3_host_out("F9: ignored - the menu is not up yet (see doc/host_harness.md)");
            }
        }
        else H.f9 = 0;

        if (keys[SDL_SCANCODE_F10]) {
            if (!H.f10) { H.f10 = 1; H.dump_now = 1; }
        }
        else H.f10 = 0;
    }
}

/*
 * The field, as the harness sees it ("field <ms>"): one line per car of the race with the numbers a race
 * is judged by - HP (s_94 at ___1de580h, +0x18), position, lap, the car's own speed and where it is.  It
 * needs nothing of the engine: both arrays are the ones the race is running in, and the harness reads
 * them the way the chaos diagnostics do (race___4c434h.c).
 *
 * It exists for the chaos kill bonus (race___4dcach.c, "kill bonus: ... pays $N into seat S"): a wreck is
 * a car whose HP reached 0, and the payout is only for a wreck the *player* caused.  These two lines
 * together say whether the payout did what it promises - a car that went to 0 without a "kill bonus" line
 * paid nothing (that is what a wall, an explosion or another opponent is worth), and a car that paid has
 * a line right there.  The run is stopped by "frames", so the dump is what makes the last frames of a
 * race visible at all.
 */
static void dr3_host_field_dump(void)
{
    extern __BYTE__ ___1de580h[];
    extern __BYTE__ ___1e6ed0h[];
    extern int      NUM_OF_CARS;
    extern int      MY_CAR_IDX;
    const struct_35e_t * s_35e = (const struct_35e_t *)___1e6ed0h;
    int                  n;

    if (!s_35e || (NUM_OF_CARS <= 0) || (NUM_OF_CARS > DR3_MAX_CARS)) return;

    dr3_log("[host] field: %d car(s)", NUM_OF_CARS);

    for (n = 0; n < NUM_OF_CARS; ++n)
        dr3_log("[host] field car %2d hp %6d pos %2d lap %2d finished %d speed %.3f at %.1f,%.1f%s",
                n, (int)D(___1de580h + 0x94*n + 0x18), (int)s_35e[n].Position, (int)s_35e[n].Lap,
                (int)s_35e[n].Finished, (double)s_35e[n].__b0,
                (double)s_35e[n].XLocation, (double)s_35e[n].YLocation,
                ((int)n == (int)MY_CAR_IDX) ? "   <- the player" : "");
}

/*
 * The scenario switches ("power <x>", "hp <car> <value>"), applied once per race: they set up the race
 * the payout check (the chaos kill bonus, race___4dcach.c) needs, and they are the only thing the harness
 * ever *writes* into a running race.
 *
 * Why they are needed: the payout is money for wrecking an opponent, and only the player's own damage pays
 * (the pair the collision code knows is "n's bumper pokes into k", and the payout fires for n == MY_CAR_IDX).
 * In the twenty car race of the harness the player *is* the slowest car of the field and he starts on the
 * last grid place (rank 20, ~750 px behind the line on TR8), so he never catches anybody: a full three lap
 * race showed him lapping with everybody in front of him and no wreck at all.  The switches below make that
 * race reachable without touching what is measured:
 *
 *   power <x>        the power of the player's car (s_94.__4, the factor both the acceleration and the AI's
 *                    target speed use: terminal speed = 0.784 * power).  The rookie car of the harness'
 *                    savegame has 2.5 (1.96 px/frame, while the field runs 3.9), which is why it cannot
 *                    catch anyone - a car of the field's power can, and that is the player the kill bonus
 *                    was made for.  An in game player has this from the shop (engine upgrades).
 *   hp <car> <val>   the HP a car starts the race with (0x400 * (100 - damage) is the setup value, so
 *                    102400 is undamaged).  A victim with 1 HP is wrecked by the player's first touch,
 *                    which is what makes the payout check deterministic: the log then has the "kill bonus"
 *                    line with the amount of that car's model, the money before and after and the text
 *                    slot - and a car that went to 0 HP without such a line is the case that must pay
 *                    nothing (a wall, an explosion, another opponent).  "field <ms>" shows both.
 *   nomine <0|1>     keep every car's mine timer at its maximum, so no car lays a mine.  The AI of the
 *                    chaos field lays mines into the pack it is driving in, and in a twenty car race that
 *                    is what wrecks most of the field: with the field weakened it wrecked ten of the
 *                    sixteen cars inside one lap (host run) - a wreck through a mine is an explosion and
 *                    pays nothing, so it hides exactly what is being looked at here.
 */
static void dr3_host_scenario(void)
{
    extern __BYTE__ ___1de580h[];
    extern __BYTE__ ___1e6ed0h[];
    extern int      NUM_OF_CARS;
    extern int      MY_CAR_IDX;
    int             n;

    if (!___1e6ed0h || (NUM_OF_CARS <= 0) || (NUM_OF_CARS > DR3_MAX_CARS)) {

        H.scenario_done = 0;            /* no race: the next one gets the switches again */
        return;
    }

    if (!H.scenario_done) {

        H.scenario_done = 1;

        if (H.power > 0.0f)
            dr3_host_out("scenario: the player's car power is %.2f now (top speed about %.2f px/frame)",
                         H.power, 0.784 * (double)H.power);

        for (n = 0; n < NUM_OF_CARS; ++n)
            if ((H.hp[n] >= 0) && (n < DR3_MAX_CARS))
                dr3_host_out("scenario: car %d holds at most %d HP from here on (is %d)", n, H.hp[n],
                             (int)D(___1de580h + 0x94 * n + 0x18));
    }

    /*
     * Applied on every frame of the race, not once: the race setup (___33010h -> race___4a25ch) writes
     * these fields *after* the first presented frame (the PREPARE TO RACE screen), so a value written once
     * is overwritten by the very thing it is meant to change.  The HP is applied as a cap - only a car that
     * still has more than the asked for HP is pulled down to it, so a wrecked car (that is what this is for)
     * is never brought back and a car the race itself weakened is left alone.
     */
    if ((H.power > 0.0f) && ((int)D(___1de580h + 0x94 * (int)MY_CAR_IDX + 0x18) > 0))
        F32(___1de580h + 0x94 * (int)MY_CAR_IDX + 4) = H.power;

    for (n = 0; n < NUM_OF_CARS; ++n) {

        if ((H.hp[n] < 0) || (n >= DR3_MAX_CARS)) continue;

        if ((int)D(___1de580h + 0x94 * n + 0x18) > H.hp[n]) D(___1de580h + 0x94 * n + 0x18) = H.hp[n];
    }

    /*
     * No mines: the AI lays one when its mine timer has run out and a car is close behind it, and its own
     * timer is the whole gate (race___4b62ch.c).  Keeping it at the value a fresh mine sets means no car
     * can lay one - the wrecks then come from bumpers, which is what the payout is for.
     */
    if (H.nomine)
        for (n = 0; n < NUM_OF_CARS; ++n) ((struct_35e_t *)___1e6ed0h)[n].MineTimer = 350;

    /*
     * Full ammo: the engine's own in race cheat (race___56594h.c, the code "DREAD") keeps the player's
     * ammo at its maximum for as long as its flag is set.  The gun is the second half of the kill bonus
     * (race___4ff50h.c calls the same payout), and a machine gun needs ammo - with this flag a run can
     * watch the gun wrecks too instead of hoping for a weapon pickup to still be loaded when a car is in
     * front (the weapon itself is what a pickup puts into s_94 +0x2c; the AI shoots by itself).
     */
    if (H.ammo) {

        extern __BYTE__ ___196dc0h[];

        D(___196dc0h) = 1;
    }

    /*
     * The machine gun, ready to fire: the gun function returns unless s_94 +0x2c says the car carries a
     * weapon (race___4ff50h.c) - that field is what a weapon pickup fills.  "ammo" only keeps the
     * ammunition full, so a run that wants a *deterministic* player kill (the chaos kill bonus,
     * doc/host_harness.md) cannot rely on a pickup still being loaded when a car is finally in front.
     * Applied every frame, like the HP above: the race setup writes s_94 after the first presented frame.
     */
    if (H.weapon && ((int)D(___1de580h + 0x94 * (int)MY_CAR_IDX + 0x18) > 0))
        D(___1de580h + 0x94 * (int)MY_CAR_IDX + 0x2c) = 1;

    /*
     * Full HP: the second in race cheat of the game (race___56594h.c, the code "DRUB") puts the player's HP
     * back to 0x19000 on every frame for as long as its flag is set.  A run that wants to see *several*
     * payouts in one race needs it: the player drives the slowest car of the field, and without it the
     * leaders of the chaos grid wreck him right after his first kill (host run: one payout, then hp 0).
     */
    if (H.nohurt) {

        extern __BYTE__ ___196dbch[];

        D(___196dbch) = 1;
    }
}

/*
 * One presented engine frame (called from __PRESENTSCREEN__, drally_linux_c.c): the frame dumps, the
 * "frames <n>" stop and the quit time.  The surface handed in is the engine's own 8 bit screen (320x200 or
 * 640x480), so a dump shows what the game drew - the port's conversion for the real screen is not involved.
 */
void dr3_host_present(SDL_Surface * screen)
{
    const unsigned int now = dr3_host_now();
    int                i;

    ++H.frames_done;

    /* the scenario switches and the state of the field (see dr3_host_scenario / dr3_host_field_dump) */
    dr3_host_scenario();

    if ((H.field_ms > 0) && ((int)now - H.field_last >= H.field_ms)) {

        H.field_last = (int)now;
        dr3_host_field_dump();
    }

    /* once a second a line: a run that hangs or crawls has to be visible in the log, not only at the end */
    if ((int)now - H.report_last >= 1000) {

        const int secs = (int)(now / 1000);

        H.report_last = (int)now;

        dr3_host_out("frame %d at %u ms (%d s, %.1f frame/s)%s", H.frames_done, now, secs,
                     (secs > 0) ? (H.frames_done / (double)secs) : 0.0,
                     H.quickrace_done ? "" : " (the script has not started a race yet)");
    }

    for (i = 0; i < H.dump_n; ++i) {

        dr3_host_dump_t * d = &H.dump[i];

        if (d->every > 0) {

            if ((int)now - d->last >= d->every) {

                d->last = (int)now;
                dr3_host_dump_save(screen, d->path, ++H.dumps);
            }
        }
        else if (!d->done && ((int)now >= d->ms)) {

            d->done = 1;
            dr3_host_dump_save(screen, d->path, ++H.dumps);
        }
    }

    if (H.dump_now) {

        H.dump_now = 0;
        ++H.dumps;

        if (screen && (SDL_SaveBMP(screen, "f10.bmp") == 0))
            dr3_host_out("dump f10.bmp (%dx%d)", screen->w, screen->h);
        else
            dr3_host_out("dump f10.bmp FAILED: %s", SDL_GetError());
    }

    if ((H.frames > 0) && (H.frames_done >= H.frames)) {

        dr3_host_out("reached %d presented frame(s) - stopping", H.frames_done);
        dr3_host_shutdown();
        exit(0);
    }

    if ((H.quit_ms > 0) && ((int)now >= H.quit_ms)) {

        dr3_host_out("quit time %d ms reached - stopping", H.quit_ms);
        dr3_host_shutdown();
        exit(0);
    }
}
