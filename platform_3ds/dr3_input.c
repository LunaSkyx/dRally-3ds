/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include <stdio.h>
#include <string.h>

#include "dr3_input.h"
#include "dr3_input_map.h"
#include "dr3_log.h"

/*
 * Raw joystick button indices as exposed by SDL's n3ds backend: it passes the hid bits of libctru
 * straight through (SDL_sysjoystick.c: "if (current_state & BIT(i)) SDL_PrivateJoystickButton(...)")
 * and fills joystick->nbuttons with NB_BUTTONS = 23.  So the index is the bit position in
 * hidKeysDown(), which is what libctru's KEY_* defines are:
 *
 *   KEY_A = BIT(0)  KEY_B = BIT(1)  KEY_SELECT = BIT(2)  KEY_START = BIT(3)
 *   KEY_DPAD_RIGHT = BIT(4)  KEY_DPAD_LEFT = BIT(5)  KEY_DPAD_UP = BIT(6)  KEY_DPAD_DOWN = BIT(7)
 *   KEY_R = BIT(8)  KEY_L = BIT(9)  KEY_X = BIT(10)  KEY_Y = BIT(11)
 *   KEY_ZL = BIT(14)  KEY_ZR = BIT(15)                        <- New 3DS only
 *
 * ZL and ZR used to be read as 12 and 13, which libctru never sets - they simply could not be seen.
 */
#define DR3_JOY_A 0
#define DR3_JOY_B 1
#define DR3_JOY_SELECT 2
#define DR3_JOY_START 3
#define DR3_JOY_DRIGHT 4
#define DR3_JOY_DLEFT 5
#define DR3_JOY_DUP 6
#define DR3_JOY_DDOWN 7
#define DR3_JOY_R 8
#define DR3_JOY_L 9
#define DR3_JOY_X 10
#define DR3_JOY_Y 11
#define DR3_JOY_ZL 14
#define DR3_JOY_ZR 15

#define DR3_AXIS_DEADZONE 12000
#define DR3_QUEUE_LEN 32

static SDL_GameController *dr3_pad;
static SDL_Joystick       *dr3_joy;
static int                 dr3_ready;

static uint8_t dr3_prev[SDL_NUM_SCANCODES];
static uint32_t dr3_prev_held;
static int      dr3_zl_zr_seen;     /* "ZL/ZR reach the game" is logged once, for diagnostics */

static struct { int scancode; int pressed; } dr3_queue[DR3_QUEUE_LEN];
static int dr3_q_head, dr3_q_tail;

int dr3_input_init(void)
{
    if (dr3_ready) return 0;

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0)
        dr3_log("[dRally.3DS] input: SDL_InitSubSystem failed: %s\n", SDL_GetError());

    if (SDL_NumJoysticks() > 0) {
        if (SDL_IsGameController(0)) dr3_pad = SDL_GameControllerOpen(0);

        /*
         * The raw joystick is opened as well - always.  ZL and ZR only exist there (SDL has no
         * controller button for them, its n3ds mapping exposes them as the two triggers), so while
         * only the controller was open those two could not be read at all.  SDL allows both handles
         * on the same device.
         */
        if (!dr3_joy) dr3_joy = SDL_JoystickOpen(0);
    }

    memset(dr3_prev, 0, sizeof(dr3_prev));
    dr3_q_head = dr3_q_tail = 0;
    dr3_ready = 1;

    dr3_log("[dRally.3DS] input ready (pad=%s joystick=%s, %d buttons, ZL/ZR %s)\n",
           dr3_pad ? SDL_GameControllerName(dr3_pad) : "-",
           dr3_joy ? SDL_JoystickName(dr3_joy) : "-",
           dr3_joy ? SDL_JoystickNumButtons(dr3_joy) : 0,
           (dr3_joy && (SDL_JoystickNumButtons(dr3_joy) > DR3_JOY_ZR)) ? "readable" : "not available");
    return 0;
}

void dr3_input_describe(void)
{
    dr3_log("[dRally.3DS] R/L = accelerate/brake, d-pad or circle pad = steer, "
           "A = nitro + confirm, X = machine gun, Y = mine, B = horn, "
           "START = pause/back, L+R+START = quit\n");
}

static int dr3_btn(SDL_GameControllerButton gc, int joy_index)
{
    if (dr3_pad) return SDL_GameControllerGetButton(dr3_pad, gc) ? 1 : 0;
    if (dr3_joy) return SDL_JoystickGetButton(dr3_joy, joy_index) ? 1 : 0;
    return 0;
}

static int dr3_axis(SDL_GameControllerAxis gc, int threshold)
{
    Sint16 v;
    if (!dr3_pad) return 0;
    v = SDL_GameControllerGetAxis(dr3_pad, gc);
    if (v > threshold) return 1;
    if (v < -threshold) return -1;
    return 0;
}

static void dr3_build_state(dr3_pad_state_t *st)
{
    memset(st, 0, sizeof(*st));

    if (dr3_btn(SDL_CONTROLLER_BUTTON_A, DR3_JOY_A))               st->held |= DR3_PAD_A;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_B, DR3_JOY_B))               st->held |= DR3_PAD_B;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_X, DR3_JOY_X))               st->held |= DR3_PAD_X;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_Y, DR3_JOY_Y))               st->held |= DR3_PAD_Y;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_DPAD_UP, DR3_JOY_DUP))       st->held |= DR3_PAD_UP;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_DPAD_DOWN, DR3_JOY_DDOWN))   st->held |= DR3_PAD_DOWN;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_DPAD_LEFT, DR3_JOY_DLEFT))   st->held |= DR3_PAD_LEFT;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, DR3_JOY_DRIGHT)) st->held |= DR3_PAD_RIGHT;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, DR3_JOY_L))    st->held |= DR3_PAD_L;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, DR3_JOY_R))   st->held |= DR3_PAD_R;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_BACK, DR3_JOY_SELECT))       st->held |= DR3_PAD_SELECT;
    if (dr3_btn(SDL_CONTROLLER_BUTTON_START, DR3_JOY_START))       st->held |= DR3_PAD_START;

    /*
     * ZL/ZR are raw buttons 14 and 15 (New 3DS only - on an old 3DS those hid bits are never set).  A
     * backend that reports fewer buttons is skipped instead of reading past its end.  The first press
     * is logged once: that line separates "the buttons work" from "the emulator has no key mapped to
     * them at all".
     */
    if (dr3_joy && (SDL_JoystickNumButtons(dr3_joy) > DR3_JOY_ZR)) {
        if (SDL_JoystickGetButton(dr3_joy, DR3_JOY_ZL)) st->held |= DR3_PAD_ZL;
        if (SDL_JoystickGetButton(dr3_joy, DR3_JOY_ZR)) st->held |= DR3_PAD_ZR;
    }

    if (!dr3_zl_zr_seen && (st->held & (DR3_PAD_ZL | DR3_PAD_ZR))) {
        dr3_zl_zr_seen = 1;
        dr3_log("[dRally.3DS] ZL/ZR pressed - the New 3DS buttons reach the game\n");
    }

    /* SDL's Y axes point down, our map uses +1 = up */
    st->cpad_x   = dr3_axis(SDL_CONTROLLER_AXIS_LEFTX,  DR3_AXIS_DEADZONE);
    st->cpad_y   = -dr3_axis(SDL_CONTROLLER_AXIS_LEFTY,  DR3_AXIS_DEADZONE);
    st->cstick_x = dr3_axis(SDL_CONTROLLER_AXIS_RIGHTX, DR3_AXIS_DEADZONE);
    st->cstick_y = -dr3_axis(SDL_CONTROLLER_AXIS_RIGHTY, DR3_AXIS_DEADZONE);
}

static int dr3_queue_full(void) { return ((dr3_q_tail + 1) % DR3_QUEUE_LEN) == dr3_q_head; }

static void dr3_queue_push(int scancode, int pressed)
{
    if (dr3_queue_full()) return;
    dr3_queue[dr3_q_tail].scancode = scancode;
    dr3_queue[dr3_q_tail].pressed  = pressed;
    dr3_q_tail = (dr3_q_tail + 1) % DR3_QUEUE_LEN;
}

/* Diff the desired scancode set against what the engine already knows about. */
static void dr3_sync_keys(const dr3_pad_state_t *st)
{
    uint8_t now[SDL_NUM_SCANCODES];
    int     scan;

    dr3_input_scancodes(st, now);

    for (scan = 0; scan < SDL_NUM_SCANCODES; ++scan) {
        if (now[scan] && !dr3_prev[scan])      dr3_queue_push(scan, 1);
        else if (!now[scan] && dr3_prev[scan]) dr3_queue_push(scan, 0);
    }
    memcpy(dr3_prev, now, sizeof(dr3_prev));
}

/*
 * Characters typed on the 3DS software keyboard arrive as SDL_TEXTINPUT - turn them into the key
 * events the engine understands.  Three things are needed to make that reliable; all three were the
 * reason a first or last letter used to vanish:
 *
 *  1. The engine's text handler (___59720h) pops popLastKey() *and* popLastChar() and appends that
 *     pair to its text buffer.  dRally_Keyboard_make() sets both latches and dRally_Keyboard_break()
 *     (our key up) clears LAST_KEY again - so a key that is released too early is lost.  Instead of
 *     guessing a delay, a character is held down until both latches are empty again (keyboard.c), with
 *     a timeout as the safety net.
 *  2. Every key press calls dRally_Keyboard_make() - including the *pad* keys, which the engine also
 *     receives as scancodes.  A pad event arriving between our character and the game's read
 *     overwrites LAST_CHAR with a character of its own (0 for a direction key): that is what ate the
 *     first and the last letter of a typed name.  The pad therefore stays silent while a character is
 *     on its way, and for a short grace period after the last one, so the name is read with nothing
 *     interfering.
 *  3. Characters living on a shifted key ('!', '?', '_', ...) are sent as shift + key - the engine
 *     takes the character from its upper[] table while shift is down.  Umlauts are transliterated in
 *     dr3_input_map.c, and a character neither table can produce is dropped *and logged*, so a log
 *     says exactly why something is missing.
 */
#define DR3_INPUT_POLL_MS    3     /* how often the pad/SDL events are actually polled */
#define DR3_KEY_HOLD_MS      120   /* how long a bottom screen button stays down */
#define DR3_TEXT_HOLD_MIN_MS 20    /* keep a typed key down for at least one engine frame */
#define DR3_TEXT_HOLD_MAX_MS 200   /* ... but not longer than this, whatever the engine does */
#define DR3_TEXT_GRACE_MS    150   /* the pad stays quiet this long after the last character */

static struct { int scancode; unsigned int release_at; } dr3_pending[DR3_QUEUE_LEN];
static int      dr3_pending_count;

static char         dr3_text[64];
static int          dr3_text_len;
static int          dr3_text_pos;
static int          dr3_text_sentinel_done;
static int          dr3_text_key = -1;      /* scancode of the character being delivered, -1 = none */
static int          dr3_text_shift;         /* 1 while LSHIFT is held down for that character */
static unsigned int dr3_text_key_ms;        /* when it was pressed */
static unsigned int dr3_text_quiet_ms;      /* the pad keeps quiet until this tick */

/* The engine's keyboard latches (keyboard.c): make() sets them, the game empties them when it reads
   them.  Both empty therefore means "the character has arrived". */
extern unsigned char LAST_KEY;
extern unsigned char LAST_CHAR;

static void dr3_push_release(int scan, unsigned int now)
{
    if (dr3_pending_count >= DR3_QUEUE_LEN) return;
    dr3_pending[dr3_pending_count].scancode   = scan;
    dr3_pending[dr3_pending_count].release_at = now + DR3_KEY_HOLD_MS;
    ++dr3_pending_count;
}

static int dr3_text_consumed(void)
{
    return (LAST_KEY == 0) && (LAST_CHAR == 0);
}

/* 1 while a typed character is on its way or still waiting to be delivered - the pad is silent then */
static int dr3_text_busy(void)
{
    if ((dr3_text_pos < dr3_text_len) || (dr3_text_key >= 0) || dr3_text_shift) return 1;

    return (int)(dr3_text_quiet_ms - SDL_GetTicks()) > 0;
}

static void dr3_queue_text(const char *text)
{
    size_t n = strlen(text);

    if (n > sizeof(dr3_text) - 1) n = sizeof(dr3_text) - 1;
    memcpy(dr3_text, text, n);
    dr3_text[n]    = 0;
    dr3_text_len   = (int)n;
    dr3_text_pos   = 0;
    dr3_text_sentinel_done = 0;
    dr3_text_key   = -1;
    dr3_text_shift = 0;
    dr3_text_quiet_ms = SDL_GetTicks();     /* the pad is quiet from now on */
}

/*
 * Delivers one character per call: (shift and) key down, wait until the engine has read it, key up.
 * The first thing sent is a dead key (SDL scancode 0 = DOS 0, "no key"): the dialogue always consumes
 * one key before it looks at a character, which used to eat the first typed letter - and with the pad
 * silenced it really is that dead key that gets consumed, not the first letter.
 */
static void dr3_service_text(void)
{
    unsigned int now = SDL_GetTicks();

    if (dr3_text_key >= 0) {
        const int held = (int)(now - dr3_text_key_ms);

        if ((held < DR3_TEXT_HOLD_MIN_MS) || ((!dr3_text_consumed()) && (held < DR3_TEXT_HOLD_MAX_MS)))
            return;                             /* not read yet - one character at a time */

        dr3_queue_push(dr3_text_key, 0);        /* key up ... */
        dr3_text_key = -1;

        if (dr3_text_shift) {                   /* ... and let go of shift again */
            dr3_queue_push(SDL_SCANCODE_LSHIFT, 0);
            dr3_text_shift = 0;
        }

        dr3_text_quiet_ms = now + DR3_TEXT_GRACE_MS;
        return;
    }

    if (dr3_text_pos >= dr3_text_len) return;

    if (!dr3_text_sentinel_done) {
        dr3_text_sentinel_done = 1;
        dr3_text_key    = SDL_SCANCODE_UNKNOWN;
        dr3_text_key_ms = now;
        dr3_queue_push(dr3_text_key, 1);
        return;
    }

    {
        const char c    = dr3_text[dr3_text_pos++];
        const int  scan = dr3_char_to_scancode(c);

        if (scan < 0) {
            dr3_log("[dr3] software keyboard: 0x%02X cannot be typed with the game's character table - "
                    "skipped", (unsigned char)c);
            return;
        }

        if (dr3_char_needs_shift(c)) {
            dr3_text_shift = 1;
            dr3_queue_push(SDL_SCANCODE_LSHIFT, 1);     /* shift down, then the key below it */
        }

        dr3_text_key    = scan;
        dr3_text_key_ms = now;
        dr3_queue_push(scan, 1);
    }
}

static void dr3_service_pending(void)
{
    const unsigned int now = SDL_GetTicks();
    int                i   = 0;

    while (i < dr3_pending_count) {
        if ((int)(now - dr3_pending[i].release_at) >= 0) {
            dr3_queue_push(dr3_pending[i].scancode, 0);
            dr3_pending[i] = dr3_pending[--dr3_pending_count];
        } else {
            ++i;
        }
    }
}

/* returns 1 when the event should be forwarded, 0 when it was consumed here */
static int dr3_translate_event(SDL_Event *e)
{
    if (e->type == SDL_TEXTINPUT) {
        dr3_log("[dr3] software keyboard typed: '%s'", e->text.text);
        dr3_queue_text(e->text.text);
        return 0;
    }
    return 1;
}

/* Keys requested from outside the pad (bottom screen buttons).  Both the down and the up event are
   queued, so the engine sees one clean key press. */
void dr3_input_inject_key(int scancode, int pressed)
{
    if (!dr3_ready) dr3_input_init();
    if ((scancode < 0) || (scancode >= SDL_NUM_SCANCODES)) return;

    dr3_queue_push(scancode, pressed ? 1 : 0);

    if (pressed) dr3_push_release(scancode, SDL_GetTicks());
}

int dr3_poll_event(SDL_Event *e)
{
    static unsigned int dr3_last_poll_ms;

    if (!dr3_ready) dr3_input_init();

    dr3_service_pending();      /* release typed keys that have been held long enough */
    dr3_service_text();         /* deliver the next typed character */

    /* The engine calls IO_Loop() thousands of times per second (measured 2848/s, 60us each = 17% of
       the CPU time) because it spins in its own wait loops.  Pumping SDL and reading the pad that
       often is pure waste for a 70 Hz game, so the actual polling is limited to ~330 Hz.  Queued
       events are still delivered immediately (the check below). */
    if ((dr3_q_head == dr3_q_tail) && (dr3_pending_count == 0) &&
        ((SDL_GetTicks() - dr3_last_poll_ms) < DR3_INPUT_POLL_MS))
        return 0;

    dr3_last_poll_ms = SDL_GetTicks();

    /* real events (HOME button / SDL_QUIT, touch, software keyboard, ...) win over synthetic ones */
    if (SDL_PollEvent(e)) {
        if (dr3_translate_event(e)) return 1;
        /* the event was consumed (typed text) - fall through and drain the synthetic queue */
    }

    if (dr3_q_head == dr3_q_tail) {
        dr3_pad_state_t st;
        dr3_build_state(&st);

        /* SELECT opens the 3DS software keyboard so player names / save slots can be typed */
        if ((st.held & DR3_PAD_SELECT) && !(dr3_prev_held & DR3_PAD_SELECT)) {
            dr3_log("[dr3] SELECT: opening the 3DS software keyboard");
            SDL_StartTextInput();          /* blocks until the player is done typing */
            dr3_build_state(&st);          /* refresh: the pad state is stale after the modal */
        }
        dr3_prev_held = st.held;

        if (dr3_input_quit_combo(&st)) {
            memset(e, 0, sizeof(*e));
            e->type = SDL_QUIT;
            return 1;
        }

        /*
         * While a typed character is on its way the pad stays silent: every pad key press also calls
         * dRally_Keyboard_make() and would overwrite the character latch the dialogue is waiting for
         * (see dr3_service_text).  dr3_prev stays stale on purpose - the next call reports whatever is
         * really held, so no press is lost.
         */
        if (!dr3_text_busy()) dr3_sync_keys(&st);
    }

    if (dr3_q_head != dr3_q_tail) {
        const int scan    = dr3_queue[dr3_q_head].scancode;
        const int pressed = dr3_queue[dr3_q_head].pressed;
        dr3_q_head = (dr3_q_head + 1) % DR3_QUEUE_LEN;

#if defined(DR3_LOG_KEYS)
        if (pressed) dr3_log("[dr3] key down 0x%02X '%s'", scan, dr3_scancode_name(scan));
#endif

        memset(e, 0, sizeof(*e));
        e->type                = pressed ? SDL_KEYDOWN : SDL_KEYUP;
        e->key.type            = e->type;
        e->key.state           = pressed ? SDL_PRESSED : SDL_RELEASED;
        e->key.repeat          = 0;
        e->key.keysym.scancode = (SDL_Scancode)scan;
        e->key.keysym.sym      = SDL_SCANCODE_TO_KEYCODE(scan);
        e->key.keysym.mod      = KMOD_NONE;
        return 1;
    }

    return 0;
}
