/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/* dr3_scenario.c - see dr3_scenario.h (the file format is documented there and in doc/scenario_harness.md) */
#include "dr3_scenario.h"
#include "dr3_bottom.h"
#include "dr3_input.h"
#include "dr3_log.h"
#include "dr3_quick.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#define DR3_MKDIR(p) _mkdir(p)
#else
#define DR3_MKDIR(p) mkdir((p), 0777)
#endif

#define DR3_SCN_PATH     "sdmc:/dr3_scenario.txt"
#define DR3_SHOT_DIR     "sdmc:/dr3_shots"
#define DR3_SCN_MAX      256
#define DR3_SEQ_MAX      128
#define DR3_SCN_TEXT     96
#define DR3_EVENT_MAX    16
#define DR3_UNTIL_TIMEOUT_MS  30000UL      /* "until" without "timeout": never wait forever */
#define DR3_SHOT_TIMEOUT_MS    5000UL

/* ---- timed steps ("at"/"atms": fire when their time comes, independent of each other) ---- */
enum { SCN_KEY_TAP, SCN_KEY_DOWN, SCN_KEY_UP, SCN_SHOT, SCN_LOG, SCN_QUIT };

typedef struct {
    int           by_ms;
    unsigned long when;
    int           what;
    int           scancode;
    char          text[DR3_SCN_TEXT];
    int           done;
} dr3_scn_step_t;

/* ---- sequential steps (run in file order; "until" blocks the sequence until its condition holds) ---- */
enum { SEQ_KEY_TAP, SEQ_KEY_DOWN, SEQ_KEY_UP, SEQ_UNTIL, SEQ_SLEEP, SEQ_SHOT, SEQ_LOG, SEQ_QUIT, SEQ_QUICK,
       SEQ_PERF_START, SEQ_PERF_REPORT, SEQ_AUTOPILOT, SEQ_BPROBE_START, SEQ_BPROBE_REPORT, SEQ_BOTTOM_LEGACY, SEQ_RACED, SEQ_PLAYER_HP,
       SEQ_QUICK_BOSS, SEQ_BOSS_HP, SEQ_BOTTOM_TAP, SEQ_PLAYER_DRUG, SEQ_PLAYER_RANK, SEQ_RECORD };
enum { COND_READY, COND_EVENT, COND_RACE_FRAMES, COND_FRAMES };

typedef struct {
    int           kind;
    int           scancode;               /* key steps                        */
    int           cond;                   /* until                            */
    unsigned long arg;                    /* sleep ms / frames / slot         */
    int           slot, track;            /* quickrace                        */
    int           spam_sc;                /* until ... spam KEY every MS      */
    char          spam_name[24];
    unsigned long spam_every;
    unsigned long timeout;
    char          text[DR3_SCN_TEXT];     /* key name / file / log text / event name */
    int           started;
    unsigned long t0, f0, next_spam;
    int           shot_requested;
} dr3_seq_t;

typedef struct { char name[24]; int count; unsigned long frame, ms; } dr3_event_t;

static dr3_scn_step_t dr3_scn[DR3_SCN_MAX];
static int            dr3_scn_n;
static dr3_seq_t      dr3_seq[DR3_SEQ_MAX];
static int            dr3_seq_n, dr3_seq_pc;
static dr3_event_t    dr3_events[DR3_EVENT_MAX];
static int            dr3_event_n;

static int            dr3_scn_autopilot;
static int            dr3_scn_loaded;
static int            dr3_scn_on;
static int            dr3_scn_quit;
static unsigned long  dr3_scn_frames;
static unsigned long  dr3_scn_t0;
static int            dr3_scn_t0_set;
static int            dr3_scn_shots;
static char           dr3_scn_shot_pending[DR3_SCN_TEXT];
static int            dr3_scn_rec_every;
static int            dr3_scn_rec_frames;

static unsigned long dr3_scn_now(void) { return dr3_scn_t0_set ? (unsigned long)(SDL_GetTicks() - dr3_scn_t0) : 0UL; }

/* ------------------------------------------------------------------ events --- */

void dr3_scenario_event(const char *name)
{
    int i;

    if (!dr3_scn_on || !name) return;

    for (i = 0; i < dr3_event_n; ++i) if (strcmp(dr3_events[i].name, name) == 0) break;
    if (i == dr3_event_n) {
        if (dr3_event_n >= DR3_EVENT_MAX) return;
        ++dr3_event_n;
        snprintf(dr3_events[i].name, sizeof(dr3_events[i].name), "%s", name);
        dr3_events[i].count = 0;
    }
    ++dr3_events[i].count;
    dr3_events[i].frame = dr3_scn_frames;
    dr3_events[i].ms    = dr3_scn_now();
    dr3_log_write("[scn] event %s #%d at %lu ms / frame %lu", name, dr3_events[i].count, dr3_events[i].ms, dr3_scn_frames);
}

static const dr3_event_t *dr3_scn_find_event(const char *name)
{
    int i;

    for (i = 0; i < dr3_event_n; ++i) if (strcmp(dr3_events[i].name, name) == 0) return &dr3_events[i];
    return NULL;
}

/* ------------------------------------------------------------------ parsing --- */

static void dr3_scn_add(int by_ms, unsigned long when, int what, int scancode, const char *text)
{
    dr3_scn_step_t *s;

    if (dr3_scn_n >= DR3_SCN_MAX) return;

    s = &dr3_scn[dr3_scn_n++];
    memset(s, 0, sizeof(*s));
    s->by_ms = by_ms; s->when = when; s->what = what; s->scancode = scancode;
    if (text) snprintf(s->text, sizeof(s->text), "%s", text);
}

static dr3_seq_t *dr3_seq_new(int kind)
{
    dr3_seq_t *q;

    if (dr3_seq_n >= DR3_SEQ_MAX) return NULL;
    q = &dr3_seq[dr3_seq_n++];
    memset(q, 0, sizeof(*q));
    q->kind  = kind;
    q->track = -1;
    return q;
}

/* "key NAME MODE": the name may hold a space ("Left Ctrl"), so the mode is the LAST word.  Returns the
   scancode (or -1) and the mode word through *mode. */
static int dr3_scn_key_args(char *args, const char **mode, int lineno)
{
    char *last = strrchr(args, ' ');
    int   sc;

    if (!last) { dr3_log_write("[scn] line %d: key needs a name and tap/down/up", lineno); return -1; }
    *last++ = 0;
    *mode   = last;
    sc = (int)SDL_GetScancodeFromName(args);
    if (sc == SDL_SCANCODE_UNKNOWN) { dr3_log_write("[scn] line %d: unknown key '%s'", lineno, args); return -1; }
    return sc;
}

/* a timed command after "at N" / "atms N" */
static void dr3_scn_parse_timed(int by_ms, unsigned long when, char *rest, int lineno)
{
    while (*rest == ' ' || *rest == '\t') ++rest;

    if (strncmp(rest, "key ", 4) == 0) {
        const char *mode;
        char       *name = rest + 4;
        const int   sc   = dr3_scn_key_args(name, &mode, lineno);

        if (sc < 0) return;
        if      (strcmp(mode, "tap")  == 0) dr3_scn_add(by_ms, when, SCN_KEY_TAP,  sc, name);
        else if (strcmp(mode, "down") == 0) dr3_scn_add(by_ms, when, SCN_KEY_DOWN, sc, name);
        else if (strcmp(mode, "up")   == 0) dr3_scn_add(by_ms, when, SCN_KEY_UP,   sc, name);
        else dr3_log_write("[scn] line %d: key mode '%s' is not tap/down/up", lineno, mode);
    }
    else if (strncmp(rest, "screenshot ", 11) == 0) {
        char name[DR3_SCN_TEXT];

        if (sscanf(rest + 11, "%79s", name) == 1) dr3_scn_add(by_ms, when, SCN_SHOT, 0, name);
    }
    else if (strncmp(rest, "log ", 4) == 0)  dr3_scn_add(by_ms, when, SCN_LOG, 0, rest + 4);
    else if (strncmp(rest, "quit", 4) == 0)  dr3_scn_add(by_ms, when, SCN_QUIT, 0, NULL);
    else dr3_log_write("[scn] line %d: not understood: %s", lineno, rest);
}

/* "until COND [spam KEY every MS] [timeout MS]" */
static void dr3_seq_parse_until(char *rest, int lineno)
{
    dr3_seq_t *q = dr3_seq_new(SEQ_UNTIL);
    char      *sp, *to;
    unsigned long n;

    if (!q) return;
    q->timeout = DR3_UNTIL_TIMEOUT_MS;

    to = strstr(rest, " timeout ");
    if (to) { q->timeout = strtoul(to + 9, NULL, 10); *to = 0; }

    sp = strstr(rest, " spam ");
    if (sp) {
        char *ev = strstr(sp + 6, " every ");
        char *name = sp + 6;

        *sp = 0;
        if (!ev) { dr3_log_write("[scn] line %d: spam needs 'every MS'", lineno); --dr3_seq_n; return; }
        *ev = 0;
        q->spam_every = strtoul(ev + 7, NULL, 10);
        q->spam_sc    = (int)SDL_GetScancodeFromName(name);
        if (q->spam_sc == SDL_SCANCODE_UNKNOWN) { dr3_log_write("[scn] line %d: unknown key '%s'", lineno, name); --dr3_seq_n; return; }
        snprintf(q->spam_name, sizeof(q->spam_name), "%s", name);
    }

    if (strcmp(rest, "ready") == 0) q->cond = COND_READY;
    else if (sscanf(rest, "race_frames %lu", &n) == 1) { q->cond = COND_RACE_FRAMES; q->arg = n; }
    else if (sscanf(rest, "frames %lu", &n) == 1)      { q->cond = COND_FRAMES;      q->arg = n; }
    else if (strncmp(rest, "event ", 6) == 0) {
        q->cond = COND_EVENT;
        snprintf(q->text, sizeof(q->text), "%s", rest + 6);
    }
    else { dr3_log_write("[scn] line %d: unknown condition '%s'", lineno, rest); --dr3_seq_n; }
}

static void dr3_seq_parse(char *p, int lineno)
{
    unsigned long n;
    dr3_seq_t    *q;

    if (strncmp(p, "key ", 4) == 0) {
        const char *mode;
        char       *name = p + 4;
        const int   sc   = dr3_scn_key_args(name, &mode, lineno);

        if (sc < 0) return;
        if      (strcmp(mode, "tap")  == 0) q = dr3_seq_new(SEQ_KEY_TAP);
        else if (strcmp(mode, "down") == 0) q = dr3_seq_new(SEQ_KEY_DOWN);
        else if (strcmp(mode, "up")   == 0) q = dr3_seq_new(SEQ_KEY_UP);
        else { dr3_log_write("[scn] line %d: key mode '%s' is not tap/down/up", lineno, mode); return; }
        if (q) { q->scancode = sc; snprintf(q->text, sizeof(q->text), "%s", name); }
    }
    else if (strncmp(p, "until ", 6) == 0)    dr3_seq_parse_until(p + 6, lineno);
    else if (sscanf(p, "sleep %lu", &n) == 1) { q = dr3_seq_new(SEQ_SLEEP); if (q) q->arg = n; }
    else if (strncmp(p, "screenshot ", 11) == 0) { q = dr3_seq_new(SEQ_SHOT); if (q) sscanf(p + 11, "%79s", q->text); }
    else if (strncmp(p, "log ", 4) == 0)      { q = dr3_seq_new(SEQ_LOG);  if (q) snprintf(q->text, sizeof(q->text), "%s", p + 4); }
    else if (strcmp(p, "quit") == 0)          dr3_seq_new(SEQ_QUIT);
    else if (strcmp(p, "perf start") == 0)    dr3_seq_new(SEQ_PERF_START);
    else if (strncmp(p, "autopilot ", 10) == 0) { q = dr3_seq_new(SEQ_AUTOPILOT); if (q) q->arg = (strcmp(p + 10, "on") == 0); }
    else if (sscanf(p, "playerhp %lu", &n) == 1) { q = dr3_seq_new(SEQ_PLAYER_HP); if (q) q->arg = n; }
    else if (sscanf(p, "playerdrug %lu", &n) == 1) { q = dr3_seq_new(SEQ_PLAYER_DRUG); if (q) q->arg = n; }
    else if (sscanf(p, "playerrank %lu", &n) == 1) { q = dr3_seq_new(SEQ_PLAYER_RANK); if (q) q->arg = n; }
    else if (sscanf(p, "bosshp %lu", &n) == 1)   { q = dr3_seq_new(SEQ_BOSS_HP);   if (q) q->arg = n; }
    else if (sscanf(p, "record %lu", &n) == 1)   { q = dr3_seq_new(SEQ_RECORD);    if (q) q->arg = n; }
    else if (sscanf(p, "quickboss %lu", &n) == 1) { q = dr3_seq_new(SEQ_QUICK_BOSS); if (q) q->arg = n; }
    else if (strcmp(p, "raced") == 0)         dr3_seq_new(SEQ_RACED);      /* the career counts as raced: the wheel of the sign up screen is armed */
    else if (strcmp(p, "bprobe start") == 0)  dr3_seq_new(SEQ_BPROBE_START);
    else if (strncmp(p, "bprobe report", 13) == 0) {
        q = dr3_seq_new(SEQ_BPROBE_REPORT);
        if (q) snprintf(q->text, sizeof(q->text), "%s", (p[13] == ' ') ? p + 14 : "run");
    }
    else if (strncmp(p, "bottom legacy ", 14) == 0) { q = dr3_seq_new(SEQ_BOTTOM_LEGACY); if (q) q->arg = (strcmp(p + 14, "on") == 0); }
    else if (strcmp(p, "bottom tap") == 0)    dr3_seq_new(SEQ_BOTTOM_TAP);
    else if (strncmp(p, "perf report", 11) == 0) {
        q = dr3_seq_new(SEQ_PERF_REPORT);
        if (q) snprintf(q->text, sizeof(q->text), "%s", (p[11] == ' ') ? p + 12 : "run");
    }
    else if (strncmp(p, "quickrace", 9) == 0) {
        int v;

        q = dr3_seq_new(SEQ_QUICK);
        if (!q) return;
        if (strstr(p, "slot ")  && sscanf(strstr(p, "slot ") + 5, "%d", &v) == 1)  q->slot  = v;
        if (strstr(p, "track ") && sscanf(strstr(p, "track ") + 6, "%d", &v) == 1) q->track = v;
    }
    else dr3_log_write("[scn] line %d: not understood: %s", lineno, p);
}

static void dr3_scn_load(void)
{
    FILE *f;
    char  line[240];
    int   lineno = 0;

    dr3_scn_loaded = 1;

    f = fopen(DR3_SCN_PATH, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        unsigned long n;
        char         *p = line, *e;

        ++lineno;
        e = p + strlen(p);
        while (e > p && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == 0 || *p == '#') continue;

        if      (sscanf(p, "quit_after_frames %lu", &n) == 1) dr3_scn_add(0, n, SCN_QUIT, 0, NULL);
        else if (sscanf(p, "quit_after_ms %lu", &n) == 1)     dr3_scn_add(1, n, SCN_QUIT, 0, NULL);
        else if (sscanf(p, "atms %lu ", &n) == 1) {
            char *r = strchr(p, ' '); r = r ? strchr(r + 1, ' ') : NULL;
            if (r) dr3_scn_parse_timed(1, n, r, lineno);
        }
        else if (sscanf(p, "at %lu ", &n) == 1) {
            char *r = strchr(p, ' '); r = r ? strchr(r + 1, ' ') : NULL;
            if (r) dr3_scn_parse_timed(0, n, r, lineno);
        }
        else dr3_seq_parse(p, lineno);
    }
    fclose(f);

    dr3_scn_on = (dr3_scn_n > 0) || (dr3_seq_n > 0);
    DR3_MKDIR(DR3_SHOT_DIR);
    dr3_log_write("[scn] loaded %d timed + %d sequential step(s) from %s", dr3_scn_n, dr3_seq_n, DR3_SCN_PATH);
}

int dr3_scenario_active(void)         { return dr3_scn_on; }
int dr3_scenario_autopilot(void)      { return dr3_scn_autopilot; }
int dr3_scenario_quit_requested(void) { return dr3_scn_quit; }

/* ------------------------------------------------------------------ running --- */

static void dr3_scn_finish(const char *why)
{
    dr3_log_write("[scn] done: %s, %lu ms, %lu frame(s), %d screenshot(s)", why, dr3_scn_now(), dr3_scn_frames, dr3_scn_shots);
    dr3_scn_quit = 1;
}

static void dr3_scn_fail(const char *what)
{
    dr3_log_write("[scn] FAIL: %s", what);
    dr3_scn_finish("failed");
}

static void dr3_scn_run(dr3_scn_step_t *s, unsigned long now_ms)
{
    s->done = 1;

    switch (s->what) {
    case SCN_KEY_TAP:  dr3_log_write("[scn] %lu ms / frame %lu: key %s tap",  now_ms, dr3_scn_frames, s->text); dr3_input_inject_key(s->scancode, 1); break;
    case SCN_KEY_DOWN: dr3_log_write("[scn] %lu ms / frame %lu: key %s down", now_ms, dr3_scn_frames, s->text); dr3_input_inject_raw(s->scancode, 1); break;
    case SCN_KEY_UP:   dr3_log_write("[scn] %lu ms / frame %lu: key %s up",   now_ms, dr3_scn_frames, s->text); dr3_input_inject_raw(s->scancode, 0); break;
    case SCN_SHOT:     snprintf(dr3_scn_shot_pending, sizeof(dr3_scn_shot_pending), "%s", s->text); break;
    case SCN_LOG:      dr3_log_write("[scn] %lu ms / frame %lu: %s", now_ms, dr3_scn_frames, s->text); break;
    case SCN_QUIT:     dr3_scn_finish("quit step"); break;
    }
}

static int dr3_seq_cond_true(const dr3_seq_t *q)
{
    const dr3_event_t *ev;

    switch (q->cond) {
    case COND_READY:       return dr3_quick_ready();
    case COND_FRAMES:      return dr3_scn_frames >= q->arg;
    case COND_EVENT:       return dr3_scn_find_event(q->text) != NULL;
    case COND_RACE_FRAMES: ev = dr3_scn_find_event("race_start"); return ev && (dr3_scn_frames - ev->frame) >= q->arg;
    }
    return 0;
}

static const char *dr3_seq_cond_name(const dr3_seq_t *q, char *buf, size_t n)
{
    switch (q->cond) {
    case COND_READY:       return "ready (menu loaded + a racer exists)";
    case COND_FRAMES:      snprintf(buf, n, "frames %lu", q->arg); return buf;
    case COND_EVENT:       snprintf(buf, n, "event %s", q->text);  return buf;
    case COND_RACE_FRAMES: snprintf(buf, n, "race_frames %lu", q->arg); return buf;
    }
    return "?";
}

static void dr3_seq_tick(unsigned long now)
{
    int guard;

    for (guard = 0; guard < 64; ++guard) {
        dr3_seq_t *q;
        char       nb[64];

        if (dr3_scn_quit) return;
        if (dr3_seq_pc >= dr3_seq_n) { dr3_scn_finish("end of script"); return; }

        q = &dr3_seq[dr3_seq_pc];
        if (!q->started) { q->started = 1; q->t0 = now; q->f0 = dr3_scn_frames; q->next_spam = now; }

        switch (q->kind) {
        case SEQ_KEY_TAP:  dr3_log_write("[scn] %lu ms: key %s tap",  now, q->text); dr3_input_inject_key(q->scancode, 1); ++dr3_seq_pc; continue;
        case SEQ_KEY_DOWN: dr3_log_write("[scn] %lu ms: key %s down", now, q->text); dr3_input_inject_raw(q->scancode, 1); ++dr3_seq_pc; continue;
        case SEQ_KEY_UP:   dr3_log_write("[scn] %lu ms: key %s up",   now, q->text); dr3_input_inject_raw(q->scancode, 0); ++dr3_seq_pc; continue;
        case SEQ_LOG:      dr3_log_write("[scn] %lu ms / frame %lu: %s", now, dr3_scn_frames, q->text); ++dr3_seq_pc; continue;
        case SEQ_QUIT:     ++dr3_seq_pc; dr3_scn_finish("quit step"); return;
        case SEQ_PLAYER_HP:   dr3_quick_set_player_hp((int)q->arg); dr3_log_write("[scn] %lu ms: playerhp %lu", now, q->arg); ++dr3_seq_pc; continue;
        case SEQ_PLAYER_DRUG: dr3_quick_set_player_drug((int)q->arg); dr3_log_write("[scn] %lu ms: playerdrug %lu", now, q->arg); ++dr3_seq_pc; continue;
        case SEQ_PLAYER_RANK: dr3_quick_set_player_rank((int)q->arg); dr3_log_write("[scn] %lu ms: playerrank %lu", now, q->arg); ++dr3_seq_pc; continue;
        case SEQ_BOSS_HP:     dr3_quick_set_boss_hp((int)q->arg); dr3_log_write("[scn] %lu ms: bosshp %lu", now, q->arg); ++dr3_seq_pc; continue;
        case SEQ_RECORD:      dr3_scn_rec_every = (int)q->arg; dr3_log_write("[scn] %lu ms: record every %d frame(s) (%d recorded so far)", now, dr3_scn_rec_every, dr3_scn_rec_frames); ++dr3_seq_pc; continue;
        case SEQ_QUICK_BOSS:
            ++dr3_seq_pc;
            dr3_log_write("[scn] %lu ms: quickboss phase %lu", now, q->arg);
            dr3_quick_boss((int)q->arg);
            return;
        case SEQ_RACED:       dr3_quick_set_raced(); dr3_log_write("[scn] %lu ms: raced - the career has raced once", now); ++dr3_seq_pc; continue;
        case SEQ_AUTOPILOT:   dr3_scn_autopilot = (int)q->arg; dr3_log_write("[scn] %lu ms: autopilot %s", now, dr3_scn_autopilot ? "on" : "off"); ++dr3_seq_pc; continue;
        case SEQ_PERF_START:  dr3_log_write("[scn] %lu ms / frame %lu: perf start", now, dr3_scn_frames); dr3_chaos_perf_start(); ++dr3_seq_pc; continue;
        case SEQ_PERF_REPORT: dr3_chaos_perf_report(q->text); ++dr3_seq_pc; continue;
        case SEQ_BPROBE_START: dr3_bottom_probe_start(); dr3_log_write("[scn] %lu ms: bottom screen probe started", now); ++dr3_seq_pc; continue;
        case SEQ_BPROBE_REPORT: { char line[320]; dr3_bottom_probe_report(q->text, line, sizeof(line)); dr3_log_write("%s", line); } ++dr3_seq_pc; continue;
        case SEQ_BOTTOM_LEGACY: dr3_bottom_set_legacy((int)q->arg); dr3_log_write("[scn] %lu ms: bottom screen legacy redraw %s", now, q->arg ? "ON (the old behaviour)" : "off"); ++dr3_seq_pc; continue;
        case SEQ_BOTTOM_TAP: dr3_bottom_toggle_hidden(); dr3_bottom_update(); dr3_log_write("[scn] %lu ms: bottom screen tap -> %s", now, dr3_bottom_is_hidden() ? "darkened" : "shown"); ++dr3_seq_pc; continue;
        case SEQ_SLEEP:
            if ((now - q->t0) >= q->arg) { ++dr3_seq_pc; continue; }
            return;
        case SEQ_SHOT:
            if (!q->shot_requested) { q->shot_requested = 1; snprintf(dr3_scn_shot_pending, sizeof(dr3_scn_shot_pending), "%s", q->text); }
            if (!dr3_scn_shot_pending[0]) { ++dr3_seq_pc; continue; }
            if ((now - q->t0) > DR3_SHOT_TIMEOUT_MS) { dr3_scn_shot_pending[0] = 0; dr3_scn_fail("a screenshot was never taken - no frame was presented"); }
            return;
        case SEQ_QUICK:
            /* advance FIRST: race_main runs inside this call and polls events, which re-enters this function -
               the steps after "quickrace" are the ones that run while the race is on */
            ++dr3_seq_pc;
            dr3_log_write("[scn] %lu ms: quickrace slot %d track %d", now, q->slot, q->track);
            dr3_quick_race(q->slot, q->track);
            return;
        case SEQ_UNTIL:
            if (dr3_seq_cond_true(q)) {
                dr3_log_write("[scn] %lu ms: until %s: ok after %lu ms / %lu frame(s)", now,
                              dr3_seq_cond_name(q, nb, sizeof(nb)), now - q->t0, dr3_scn_frames - q->f0);
                ++dr3_seq_pc;
                continue;
            }
            if ((now - q->t0) > q->timeout) {
                char what[160];

                snprintf(what, sizeof(what), "timeout after %lu ms waiting for %s", q->timeout, dr3_seq_cond_name(q, nb, sizeof(nb)));
                dr3_scn_fail(what);
                return;
            }
            if (q->spam_sc && (now >= q->next_spam)) {
                dr3_input_inject_key(q->spam_sc, 1);
                q->next_spam = now + q->spam_every;
            }
            return;
        }
    }
}

void dr3_scenario_tick(void)
{
    unsigned long now;
    int           i;

    if (!dr3_scn_loaded) dr3_scn_load();
    if (!dr3_scn_on || dr3_scn_quit) return;

    if (!dr3_scn_t0_set) { dr3_scn_t0 = SDL_GetTicks(); dr3_scn_t0_set = 1; dr3_log_write("[scn] started"); }
    now = dr3_scn_now();

    for (i = 0; i < dr3_scn_n; ++i) {
        dr3_scn_step_t *s = &dr3_scn[i];

        if (s->done) continue;
        if (s->by_ms ? (now < s->when) : (dr3_scn_frames < s->when)) continue;

        dr3_scn_run(s, now);
        if (dr3_scn_quit) return;
    }

    if (dr3_seq_n > 0) dr3_seq_tick(now);
}

/* ------------------------------------------------------------------ screenshots --- */

static void dr3_scn_put32(unsigned char *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

static int dr3_scn_save_bmp(const char *name, const uint32_t *fb, int fb_w, int fb_h, const dr3_lut32_t *lut)
{
    char           path[160];
    unsigned char  hdr[54];
    unsigned char *row;
    FILE          *f;
    const int      w = fb_h, h = fb_w;                   /* the buffer is rotated: 400 x 240 on screen */
    const int      stride = (w * 3 + 3) & ~3;
    int            x, y;

    snprintf(path, sizeof(path), DR3_SHOT_DIR "/%s", name);
    f = fopen(path, "wb");
    if (!f) return 0;

    row = (unsigned char *)malloc((size_t)stride);
    if (!row) { fclose(f); return 0; }

    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    dr3_scn_put32(hdr + 2, 54 + (uint32_t)stride * (uint32_t)h);
    dr3_scn_put32(hdr + 10, 54);
    dr3_scn_put32(hdr + 14, 40);
    dr3_scn_put32(hdr + 18, (uint32_t)w);
    dr3_scn_put32(hdr + 22, (uint32_t)h);
    hdr[26] = 1; hdr[28] = 24;
    dr3_scn_put32(hdr + 34, (uint32_t)stride * (uint32_t)h);
    fwrite(hdr, 1, sizeof(hdr), f);

    for (y = h - 1; y >= 0; --y) {                       /* BMP rows run bottom-up */
        memset(row, 0, (size_t)stride);
        for (x = 0; x < w; ++x) {
            const uint32_t px = fb[(size_t)x * (size_t)fb_w + (size_t)(h - 1 - y)];

            row[x * 3 + 0] = (unsigned char)(px >> lut->bs);
            row[x * 3 + 1] = (unsigned char)(px >> lut->gs);
            row[x * 3 + 2] = (unsigned char)(px >> lut->rs);
        }
        fwrite(row, 1, (size_t)stride, f);
    }
    free(row);
    fclose(f);
    return 1;
}

void dr3_scenario_present(const uint32_t *fb, int fb_w, int fb_h, const dr3_lut32_t *lut)
{
    ++dr3_scn_frames;

    if ((dr3_scn_rec_every > 0) && ((dr3_scn_frames % (unsigned long)dr3_scn_rec_every) == 0)) {
        char vname[32];
        ++dr3_scn_rec_frames;
        snprintf(vname, sizeof(vname), "v_%04d.bmp", dr3_scn_rec_frames);
        dr3_scn_save_bmp(vname, fb, fb_w, fb_h, lut);
    }

    if (dr3_scn_shot_pending[0]) {
        const int ok = dr3_scn_save_bmp(dr3_scn_shot_pending, fb, fb_w, fb_h, lut);

        /* the bottom screen goes next to it: "x.bmp" -> "x_bottom.bmp" (best effort, not every run has a console) */
        {
            char bname[DR3_SCN_TEXT + 16], bpath[DR3_SCN_TEXT + 64];
            char *dot;

            snprintf(bname, sizeof(bname), "%s", dr3_scn_shot_pending);
            dot = strrchr(bname, '.');
            if (dot) *dot = 0;
            snprintf(bpath, sizeof(bpath), DR3_SHOT_DIR "/%s_bottom.bmp", bname);
            dr3_bottom_save_bmp(bpath);
        }

        dr3_log_write("[scn] frame %lu: screenshot %s -> %s", dr3_scn_frames, dr3_scn_shot_pending, ok ? "ok" : "FAILED");
        if (ok) ++dr3_scn_shots;
        dr3_scn_shot_pending[0] = 0;
    }
}
