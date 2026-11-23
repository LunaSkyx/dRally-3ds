#include "drally.h"
#include "drally_fonts.h"
#include "drally_keyboard.h"
#include "platform_3ds/dr3_chaos.h"     /* dr3_chaos_duel_rival_set - the rival of a duel (drally.h) */
#include "platform_3ds/dr3_anniv_prog.h"
#include "drally_structs_fixed.h"
#include "sfx.h"

#pragma pack(1)
typedef struct x655_s {
	__DWORD__ 	r;
	__DWORD__ 	g;
	__DWORD__ 	b;
} x655_t;

	extern x655_t ___19eb50h[0x100];

	extern __BYTE__ ___1a1028h[];
	extern __POINTER__ ___1a1138h__VESA101_BACKGROUND;
	extern __POINTER__ ___1a10e4h__VESA101h_DefaultScreenBufferA;
	extern __POINTER__ ___1a112ch__VESA101_ACTIVESCREEN_PTR;
	extern __BYTE__ ___185a3ch[];
	extern __BYTE__ ___185a50h[];
	extern __DWORD__ ___24cc54h_sfx_volume;
	extern __BYTE__ ___185a44h[];
	extern __BYTE__ ___1a1ef8h[];
	extern __BYTE__ ___185a48h[];
	extern __BYTE__ ___1a1f64h[];
	extern __POINTER__ ___1a0f98h;
	extern __BYTE__ ___1a01e0h[];
	extern __BYTE__ ___185c7ah[];
	extern __POINTER__ ___1a10b8h;
	extern __BYTE__ ___1a0ef8h[];
	extern __BYTE__ ___1a103ch[];
	extern __BYTE__ ___185a20h[];
	extern __BYTE__ ___185a14h_UseWeapons[];
	extern __BYTE__ ___185a4ch[];
	extern __BYTE__ ___1a1ef4h[];
	extern __BYTE__ ___185a28h[];

void dRally_Sound_freeEffectChannel(__BYTE__ ch_num);
void ___12940h(void);
void ___3892ch_cdecl(__DWORD__);
__BYTE__ dRally_Sound_setPosition(__DWORD__ pos_n);
void dRally_Sound_setMasterVolume(__DWORD__ vol);
void ___58c60h(void);
void ___2b318h(void);
void ___13248h_cdecl(__DWORD__, __DWORD__ ,__DWORD__, __DWORD__, __DWORD__);
__DWORD__ ___32230h(void);
__DWORD__ ___31868h(void);
void ___33010h_cdecl(int NumCars);
char * strupr_watcom106(char * s);
char * itoa_watcom106(int value, char * buffer, int radix);
int rand_watcom106(void);
void ___3174ch_cdecl(__DWORD__);
void ___17324h(void);
void ___12cb8h__VESA101_PRESENTSCREEN(void);
void ___2ab50h(void);
void ___3079ch_cdecl(__DWORD__);
void dr3_adversary_ensure(void);
__BYTE__ ___5994ch(void);
void dRally_Sound_pushEffect(__BYTE__ channel, __BYTE__ n, __DWORD__ unk, __DWORD__ a0, __DWORD__ a1, __DWORD__ a2);
void ___281d0h_cdecl(__DWORD__, __DWORD__, __DWORD__, __DWORD__);
void ___30df8h(void);
void ___27f80h_cdecl(__DWORD__, __DWORD__, __DWORD__, __DWORD__);
void ___17248h(void);
void ___3a6a4h_v2(int);
void ___31588h(void);
void __DISPLAY_SET_PALETTE_COLOR(__DWORD__, __DWORD__, __DWORD__, __DWORD__);


static __DWORD__ helper_color(__DWORD__ eax, __DWORD__ edx){

	eax = edx / 0x10000 * eax;
	eax += 0x8000;
	eax = (int)eax>>0x10;
	eax &= 0xff;

	return eax;
}

// SIGNUP SCREEN
/*
 * The random selector (drally.h: DR3_CHAOS_MODE_RANDOM): the player lets the game choose.  The screen draws
 * for a rule set *and* for a race, lands on both and leaves the registration to ENTER - so everything that
 * makes a sign up a sign up (the car driving in, the sounds, the seat in the grid) stays untouched.
 *
 * Only the *choice* is new, and it is the choice the screen already has: D(___185a50h) is the highlighted
 * race, the highlight is drawn with ___27f80h_cdecl and cleared with ___281d0h_cdecl.  The draw happens in
 * the two rounds the player sees, both over those same three boxes:
 *
 *   round 1 - the mode.  The boxes carry the three rule sets (the frame border of the screen's own message
 *             boxes, ___13248h_cdecl) and the copper frame hops over them; the mode it drew then stays on
 *             the screen as a banner at the top (dr3_spin_header) - from there on the player sees *which*
 *             rule set the second round is for.
 *   round 2 - the race.  What the wheel always did: the same frame, the same boxes, and the race it lands
 *             on is the one ENTER signs up for.
 *
 * Both rounds only draw - what they draw was decided before either of them started (dr3_chaos_spin_pick).
 */
static int dr3_spin_done;               /* one spin per visit of the screen */
static int dr3_spin_armed;              /* the wheel may spin at all on this visit (see ___3266ch) */
static int dr3_spin_race = -1;          /* the race the wheel landed on, -1 = it did not spin */
static int dr3_spin_result;             /* the rule set it rolled: a DR3_CHAOS_MODE_ (RACE / CHALLENGE / DM / ARENA) */
static int dr3_arena_skip_map;          /* 1 = ARENA mode picked: skip the 3-map selection & enter directly */
static int dr3_arena_fast_enter;        /* 1 = skip post-signup 280-tick pause on the map screen */

/*
 * Round 1 has four cards: the three rule sets of the original wheel and the ARENA (platform_3ds/dr3_arena.h).
 * A card is a "slot" 0..3; the mode that goes with it is what the setup reads.
 */
#define DR3_SPIN_SLOTS 4
static const char * dr3_spin_mode_name[DR3_SPIN_SLOTS] = { "RACE", "CHALLENGE 1V1", "DESTROY 'EM ALL", "ARENA" };

static int dr3_spin_mode_of_slot(int slot) { return (slot >= 3) ? DR3_CHAOS_MODE_ARENA : slot; }
static int dr3_spin_slot_of_mode(int mode) { return (mode == DR3_CHAOS_MODE_ARENA) ? 3 : mode; }
static const char * dr3_spin_name_of_mode(int mode) { return dr3_spin_mode_name[dr3_spin_slot_of_mode(mode)]; }

/* a race the player may sign up for - the checks the ENTER branch makes, in the same order */
static int dr3_spin_race_ok(int race)
{
	if((race < 0) || (race > 2)) return 0;
	if(B(___1a1f64h+race+3) >= 4) return 0;                 /* full: the sign up would refuse it */

	/* the two races the player's car is not good enough for (the same fields s_6c of the screen) */
	/*
	 * No class gate here.  The wheel is allowed to pick *any* of the three races, even for a driver whose car
	 * would only be good enough for the first one - the player asked for exactly that, and the ENTER branch
	 * below lets a race the wheel picked through without its class check, so the choice is never a dead end.
	 */
	return 1;
}

int dr3_chaos_spin_pick(int *mode)
{
	int race = -1, hits = 0, pick, n;

	for(n = 0; n < 3; ++n) if(dr3_spin_race_ok(n)) ++hits;

	if(hits > 0){

		pick = rand_watcom106() % hits;

		for(n = 0; n < 3; ++n){

			if(!dr3_spin_race_ok(n)) continue;
			if(pick-- == 0){ race = n; break; }
		}
	}

	/* nothing open at all: stay on the race the player had highlighted */
	if(race < 0) race = (int)D(___185a50h);

	/* 4-mode shuffle-bag in 30th Anniversary: all 4 modes appear every 4 races, never back-to-back */
	if(mode){
		if(dr3_chaos_active()){
			*mode = dr3_anniv_mode_bag_next(rand_watcom106);
		}
		else {
			*mode = dr3_spin_mode_of_slot(rand_watcom106() % DR3_SPIN_SLOTS);
		}
	}

	return race;
}

/*
 * The line under the three poster boxes: what the wheel landed on, and the duel offer.  It sits at row 0x170 -
 * the band between the boxes (they end at ~0x166) and the footer ("Welcome to Death Rally ... Powered by dRally",
 * from row 0x190 on).  It used to be written at 0x1a4, on top of the footer's second line, without clearing
 * anything first: the two texts piled up into an unreadable smear (seen in the 3DS run, logs/az_runs/duel_offer).
 */
#define DR3_OFFER_LINE_Y   0x170

static void dr3_offer_line_clear(void)
{
	memcpy(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x280*DR3_OFFER_LINE_Y,
	       ___1a1138h__VESA101_BACKGROUND+0x280*DR3_OFFER_LINE_Y, 0x280*16);
}

static void dr3_spin_line_draw(const char * text)
{
	char buf[0x64];

	dr3_offer_line_clear();

	snprintf(buf, sizeof(buf), "%s", text);

	___12e78h_v3(___1a10b8h___185c7ah, buf, 0x1e, DR3_OFFER_LINE_Y);
}

/* the ramp of both rounds: fast at first, then settling (the ramp is quadratic) */
static int dr3_spin_delay(int n, int steps)
{
	if(steps < 2) return DR3_CHAOS_SPIN_DELAY_FIRST;

	return DR3_CHAOS_SPIN_DELAY_FIRST
	     + (DR3_CHAOS_SPIN_DELAY_LAST - DR3_CHAOS_SPIN_DELAY_FIRST)*n*n/((steps - 1)*(steps - 1));
}

extern int ___60458h;      /* the frames per second the game itself runs at (set by ___60420h.c) */

/*
 * A pause of whole seconds, in the frames this game runs at (60 in the front end, 70 in the race).  The
 * delay ramp above counts frames, which is what a slot machine feels like; "look at what was drawn" is a
 * length of time instead, and the harness runs the same code slower than the console, so a fixed number of
 * frames would read differently in a run than on the 3DS.
 */
static void dr3_spin_hold(int ms)
{
	const int fps = (___60458h > 0) ? ___60458h : 60;
	int       n   = (ms * fps) / 1000 + 1, i;

	for(i = 0; i < n; ++i) ___2ab50h();
}

/* the two lines a box gets - the long mode is split so it never runs into the border of the panel */
static const char * dr3_spin_label[DR3_SPIN_SLOTS][2] = {
	{ "RACE",      ""        },
	{ "CHALLENGE", "1V1"     },
	{ "DESTROY",   "'EM ALL" },
	{ "ARENA",     ""        }
};

/*
 * The boxes of round 1: four cards across the screen, one every 0x84 pixels (the three poster columns of the
 * screen are 0xa0 apart and end at 0x27e - there is no room for a fourth one there, so round 1 paints its own
 * band, see dr3_spin_band_clear).
 */
#define DR3_SPIN_BOX_W   0x7c      /* narrower than a poster: four of them end before the player's panel (x 0x21a) */
#define DR3_SPIN_BOX_PITCH 0x84
#define DR3_SPIN_BAND_W  0x218

static int dr3_spin_box_x(int slot) { return 0x04 + DR3_SPIN_BOX_PITCH*slot; }

/* the rows the boxes of round 1 live in: the cards, their frames and what the frames restore */
#define DR3_SPIN_BAND_Y  0x70
#define DR3_SPIN_BAND_H  0x90

static void dr3_spin_band_clear(void)
{
	int row;

	/* the rows left of the player's panel only: the panel stays where it is */
	for(row = DR3_SPIN_BAND_Y; row < DR3_SPIN_BAND_Y + DR3_SPIN_BAND_H; ++row)
		memcpy(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x280*row, ___1a1138h__VESA101_BACKGROUND+0x280*row, DR3_SPIN_BAND_W);
}

/*
 * One box as the card of one mode: the frame border of this screen's own message boxes (the same
 * ___13248h_cdecl the "you did not sign up" box calls) with the mode written on it.  The inset keeps the
 * card clear of the copper frame the selection draws around the box.
 */
static void dr3_spin_box_draw(int slot)
{
	int n, len, lines;
	int x = dr3_spin_box_x(slot) + 0x14, y = 0x76 + 0x14;
	int w = DR3_SPIN_BOX_W - 0x28, h = 0x84 - 0x28;

	___13248h_cdecl(x, y, w, h, 0);

	lines = (dr3_spin_label[slot][1][0] != '\0') ? 2 : 1;

	n = -1;
	while(++n < lines){

		len = getTextWidth(FONTPROPS03, dr3_spin_label[slot][n]);
		___12e78h_v3(___1a10b8h___185c7ah, dr3_spin_label[slot][n],
		             x + (w - len)/2, y + h/2 - (lines*0x0c + (lines - 1)*0x04)/2 + n*0x10);
	}
}

/*
 * The copper frame the selection draws around a box, and it again: ___281d0h_cdecl builds back exactly what
 * the frame touched (the edges of the rect); the card in the middle of the box stays where it is - the
 * posters of this screen are painted into the *screen* (___30df8h), so they must not be restored from the
 * background.
 */
static void dr3_spin_box_clear(int slot)
{
	___281d0h_cdecl(dr3_spin_box_x(slot), 0x76, DR3_SPIN_BOX_W, 0x84);
}

/*
 * The mode the wheel drew in round 1 stays on the screen from then on: a banner at the top, in the two
 * pieces of this screen - the border of the message boxes (active) and their font (the big one).
 */
static void dr3_spin_header(int mode)
{
	const char * text = dr3_spin_name_of_mode(mode);
	int text_w = getTextWidth(FONTPROPS02, text);
	int w = text_w + 0x40;                          /* the corner blocks of the border are 0x20 wide */
	int x = (0x280 - w)/2;

	___13248h_cdecl(x, 0x10, w, 0x3c, 1);
	___12e78h_v3(___1a1108h___185c0bh, text, x + 0x20, 0x10 + 0x16);
}

void dr3_loading_header(int mode)
{
	const char * text = (dr3_boss_in_race()) ? dr3_boss_phase_title(dr3_boss_phase()) : dr3_spin_name_of_mode(mode);
	int text_w = getTextWidth(FONTPROPS02, text);
	int w = text_w + 0x40;
	int x = (0x280 - w)/2;
	int y = 74;
	int h = 42;

	___13248h_cdecl(x, y, w, h, 1);
	___12e78h_v3(___1a1108h___185c0bh, text, x + 0x20, y + 12);
}

/*
 * Round 1, the rule set: the three boxes carry the three modes and the copper frame hops over them, so the
 * player sees a *mode* being drawn - the draw itself happened before, in dr3_chaos_spin_pick.
 */
static void dr3_spin_mode_run(int mode)
{
	int n, i, cur, delay;

	dr3_log("[dr3] chaos: spin: round 1 - the mode: %s over the four cards of the screen (round 2 is the race)",
	        dr3_spin_name_of_mode(mode));

	dr3_spin_band_clear();                          /* the posters make room for the four cards */
	for(n = 0; n < DR3_SPIN_SLOTS; ++n) dr3_spin_box_draw(n);
	___12cb8h__VESA101_PRESENTSCREEN();

	cur = (int)D(___185a50h);

	for(n = 0; n < DR3_CHAOS_SPIN_MODE_STEPS; ++n){

		dr3_spin_box_clear(cur);                    /* the frame goes, the card of the box stays */

		cur = (cur >= DR3_SPIN_SLOTS - 1) ? 0 : cur + 1;

		___27f80h_cdecl(dr3_spin_box_x(cur), 0x76, DR3_SPIN_BOX_W, 0x84);
		___12cb8h__VESA101_PRESENTSCREEN();
		dRally_Sound_pushEffect(1, SFX_CLICK_2, 0, ___24cc54h_sfx_volume, 0x28000, 0x8000);

		delay = dr3_spin_delay(n, DR3_CHAOS_SPIN_MODE_STEPS);
		for(i = 0; i < delay; ++i) ___2ab50h();
	}

	/* ... and it lands on the mode it drew */
	dr3_spin_box_clear(cur);
	cur = dr3_spin_slot_of_mode(mode);
	___27f80h_cdecl(dr3_spin_box_x(cur), 0x76, DR3_SPIN_BOX_W, 0x84);
	___12cb8h__VESA101_PRESENTSCREEN();
	dRally_Sound_pushEffect(1, SFX_SIGNUP_BRAKES, 0, ___24cc54h_sfx_volume, 0x28000, 0x8000);

	/*
	 * The pause the wheel needs: round 1 keeps its result - the cards and the frame on the box that was
	 * drawn - for a few seconds before round 2 takes the boxes over.  Without it the second draw starts in
	 * the same breath and the mode is never really read.
	 */
	dr3_log("[dr3] chaos: spin: round 1 done (%s) - the boxes stay on it for %d second(s) = %d frame(s) at %d frame/s",
	        dr3_spin_name_of_mode(mode), DR3_CHAOS_SPIN_MODE_HOLD_S,
	        DR3_CHAOS_SPIN_MODE_HOLD_S*((___60458h > 0) ? ___60458h : 60),
	        (___60458h > 0) ? ___60458h : 60);

	dr3_spin_hold(DR3_CHAOS_SPIN_MODE_HOLD_S*1000);

	/* ARENA always runs on TRA.BPA: no need to restore the 3 street posters for a map spin */
	if(mode == DR3_CHAOS_MODE_ARENA){
		dr3_spin_header(mode);
		___12cb8h__VESA101_PRESENTSCREEN();
		return;
	}

	/* the frames go and the posters come back - ___30df8h paints them and takes the cards with them ... */
	dr3_spin_band_clear();
	___30df8h();

	/* ... and the mode stays on the screen, at the top, while round 2 draws the race */
	dr3_spin_header(mode);
	___12cb8h__VESA101_PRESENTSCREEN();
}

/* round 2, the race: the copper frame over the same boxes - what the wheel did before there were two rounds */
static void dr3_spin_race_run(int race)
{
	int n, i, delay;

	for(n = 0; n < DR3_CHAOS_SPIN_STEPS; ++n){

		___281d0h_cdecl(0xa0*D(___185a50h)+0x16, 0x76, 0x94, 0x84);
		D(___185a50h) = ((int)D(___185a50h) >= 2) ? 0 : (D(___185a50h) + 1);
		___27f80h_cdecl(0xa0*D(___185a50h)+0x16, 0x76, 0x94, 0x84);
		___12cb8h__VESA101_PRESENTSCREEN();
		dRally_Sound_pushEffect(1, SFX_CLICK_1, 0, ___24cc54h_sfx_volume, 0x28000, 0x8000);

		delay = dr3_spin_delay(n, DR3_CHAOS_SPIN_STEPS);
		for(i = 0; i < delay; ++i) ___2ab50h();
	}

	/* ... and it lands: the drawn race keeps the highlight and the frame here */
	___281d0h_cdecl(0xa0*D(___185a50h)+0x16, 0x76, 0x94, 0x84);
	D(___185a50h) = race;
	___27f80h_cdecl(0xa0*D(___185a50h)+0x16, 0x76, 0x94, 0x84);
	___12cb8h__VESA101_PRESENTSCREEN();
	dRally_Sound_pushEffect(1, SFX_SIGNUP_BRAKES, 0, ___24cc54h_sfx_volume, 0x28000, 0x8000);

	/*
	 * A short hold on what it landed on: without it the mode banner and the bottom line are read in the
	 * same frame and the race the wheel picked is never really readable.
	 */
	for(i = 0; i < DR3_CHAOS_SPIN_LAND_HOLD; ++i) ___2ab50h();
}

static void dr3_chaos_spin_run(void)
{
	int race, mode;

	race = dr3_chaos_spin_pick(&mode);

	/* round 1: the rule set - the four boxes of the screen carry the four modes */
	dr3_spin_mode_run(mode);

	/* round 2: skip map spin for ARENA (it always runs on TRA.BPA) and go straight into the fight */
	if(mode == DR3_CHAOS_MODE_ARENA){
		D(___185a50h)        = race;
		dr3_arena_skip_map   = 1;
		dr3_arena_fast_enter = 1;
	}
	else {
		dr3_spin_race_run(race);
	}

	/*
	 * The draw *is* the race: the mode is what the setup reads (___33010h.c) and the race is what ENTER
	 * signs up for.  The rival of a challenge is drawn by the setup out of the standings (dr3_log.c: the
	 * drivers within +-DR3_CHAOS_DUEL_RANK_WINDOW ranks of the player).
	 *
	 * `dr3_chaos_mode_commit`, not `_set`: what was rolled applies to the race that follows, the *setting*
	 * stays what the player chose (RANDOM, dr3_log.c) - otherwise the wheel would turn itself off here and
	 * never spin on the next trip to this screen.
	 */
	dr3_chaos_mode_commit(mode);

	dr3_log("[dr3] chaos: spin: landed on race %d (0..2, the fights are there) and mode %d (%s) - ENTER signs up",
	        race, mode, dr3_chaos_mode_name(mode));

	/* what the wheel decided is kept: the loop below puts the highlight back and keeps the line on the screen */
	dr3_spin_race   = race;
	dr3_spin_result = mode;

	dr3_spin_done = 1;
}

/* the rival the duel offer of this visit named: -2 = not drawn yet, -1 = nobody within the window */
static int dr3_offer_rival = -2;

void ___3266ch(void){

	__DWORD__ 	rr, gg, bb, nn;
	__DWORD__ 	eax, ebx, edx, edi, esi, ebp, cf;
	__BYTE__ 	esp[0x64];
	int 	x, y, i, j, n, n2, n3, n4;
	__BYTE__ 		px;
	racer_t * 		s_6c;


	s_6c = (racer_t *)___1a01e0h;
	D(___1a1028h) = -1;
	___31588h();

	/* a visit of the sign up screen: the wheel of the random selector may spin once (drally.h) */
	dr3_spin_done        = 0;
	dr3_spin_race        = -1;
	dr3_arena_skip_map   = 0;
	dr3_arena_fast_enter = 0;
	dr3_offer_rival      = -2;      /* the duel offer has not picked its rival yet (once per visit, below) */

	/*
	 * The wheel of the selector asks the same question the player would: which race is worth signing up for.
	 * It only runs in 30th Anniversary (dr3_chaos_active) and needs the career to have started: the *first*
	 * race of a savegame - the one right after the licence - is an ordinary race with the whole field.
	 */
	dr3_spin_armed = dr3_chaos_active() && dr3_chaos_raced();

	/*
	 * ... and it has to know what the player set *now*: the mode file is normally read when a race is set
	 * up (___33010h.c), which happens after this screen - so a mode set from the SD card (or with the key
	 * combo of platform_3ds/dr3_input.c) would only reach the second race.  Reading it here makes the file
	 * live for the wheel.
	 */
	dr3_chaos_mode_reload();

	/* On Race 1 (wheel not armed yet) in RANDOM mode, clear any leftover roll from an earlier session/career. */
	if(!dr3_spin_armed && (dr3_chaos_mode_setting() == DR3_CHAOS_MODE_RANDOM)) dr3_chaos_mode_clear_roll();

	/*
	 * One line per visit: the wheel is a thing the player has to be able to check, in the log as well as on
	 * the screen ("mode" is what the selector reads, see the switch of the key combo in dr3_input.c).
	 */
	dr3_log("[dr3] chaos: sign up screen: the selector is %s (mode %d, %s, the player's rank is %d, "
	        "the wheel may land on race %d%d%d)",
	        (dr3_spin_armed&&(dr3_chaos_mode_setting() == DR3_CHAOS_MODE_RANDOM)) ? "armed" : "off",
	        dr3_chaos_mode_setting(), dr3_chaos_mode_name(dr3_chaos_mode_setting()), (int)s_6c[(int)D(___1a1ef8h)].rank,
	        dr3_spin_race_ok(0), dr3_spin_race_ok(1), dr3_spin_race_ok(2));

	n = -1;
	while(++n < 7){

		rr = helper_color(___19eb50h[n+0xb0].r, 0x640000);
		gg = helper_color(___19eb50h[n+0xb0].g, 0x640000);
		bb = helper_color(___19eb50h[n+0xb0].b, 0x640000);

		__DISPLAY_SET_PALETTE_COLOR(bb, gg, rr, n+0xb0);
	}

	memcpy(___1a10e4h__VESA101h_DefaultScreenBufferA, ___1a1138h__VESA101_BACKGROUND, 0x4b000);
	___1a112ch__VESA101_ACTIVESCREEN_PTR = ___1a10e4h__VESA101h_DefaultScreenBufferA;
	memcpy(___1a10e4h__VESA101h_DefaultScreenBufferA+0x280*103, ___1a1138h__VESA101_BACKGROUND+0x280*103, 0x28a00);
	___30df8h();

	if(D(___185a3ch) == 0) ___27f80h_cdecl(0xa0*D(___185a50h)+0x16, 0x76, 0x94, 0x84);
	if(D(___185a3ch) != 0) ___17248h();

	___3a6a4h_v2(1);

	if(D(___185a3ch) != 0){

		___17324h();
		memcpy(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x10180, ___1a1138h__VESA101_BACKGROUND+0x10180, 0x28a00);
		___30df8h();
		___27f80h_cdecl(0xa0*D(___185a50h)+0x16, 0x76, 0x94, 0x84);
		___12cb8h__VESA101_PRESENTSCREEN();
	}

	esi = 0;

	while(1){

		___2ab50h();
		___2ab50h();
		___3079ch_cdecl(0x4b);

		/*
		 * The random selector: once per visit the wheel decides a race and a rule set, then the player only
		 * confirms with ENTER (drally.h: DR3_CHAOS_MODE_RANDOM).  The mode the wheel set is the one the race
		 * setup reads, so nothing else has to happen here.  This does *not* ask for the chaos difficulty:
		 * the selector is the way into these races, so it works on any savegame - only the chaotic extras
		 * of the difficulty stay what they are (___33010h.c).
		 */
		if(!dr3_spin_done&&dr3_spin_armed&&(dr3_chaos_mode_setting() == DR3_CHAOS_MODE_RANDOM)) dr3_chaos_spin_run();

		/* Fixed ARENA mode: TRA.BPA is always the map, so skip the 3-map selection screen immediately */
		if(!dr3_arena_skip_map && !dr3_arena_fast_enter && dr3_chaos_active() &&
		   (dr3_chaos_mode_setting() == DR3_CHAOS_MODE_ARENA)){
			int slot = 0, k;
			for(k = 0; k < 3; ++k){ if(dr3_spin_race_ok(k)){ slot = k; break; } }
			if(B(___1a1f64h + slot + 3) < 4){
				D(___185a50h)        = slot;
				dr3_spin_race        = slot;
				dr3_arena_skip_map   = 1;
				dr3_arena_fast_enter = 1;
				dr3_log("[dr3] chaos: arena: skipping map selection screen (slot %d -> TRA.BPA)", slot);
			}
		}

		/*
		 * What the wheel decided stays decided: the highlighted race is put back where it landed (the arrow
		 * keys of the screen would move it away - the race the wheel drew is the one ENTER has to sign up
		 * for), and the line with the rolled rule set and the banner with the mode are redrawn, because the
		 * screen pass above wipes both every frame.
		 */
		if(dr3_spin_done&&(dr3_spin_race >= 0)&&!dr3_arena_skip_map){

			if((int)D(___185a50h) != dr3_spin_race){

				___281d0h_cdecl(0xa0*(int)D(___185a50h)+0x16, 0x76, 0x94, 0x84);
				D(___185a50h) = dr3_spin_race;
				___27f80h_cdecl(0xa0*dr3_spin_race+0x16, 0x76, 0x94, 0x84);
			}

			dr3_spin_line_draw(dr3_spin_name_of_mode(dr3_spin_result));
			dr3_spin_header(dr3_spin_result);
			___12cb8h__VESA101_PRESENTSCREEN();
		}

		/*
		 * A duel is offered right here, where the race would be picked: one driver of the standings - any
		 * driver within +-DR3_CHAOS_DUEL_RANK_WINDOW ranks of the player's own, drawn at random - wants a
		 * two car race (drally.h: DR3_CHAOS_MODE_DUEL).  ENTER below signs up as usual and the race then
		 * runs as the duel against him - the setup in ___33010h.c takes the rival named here back, as long
		 * as he still is one of those drivers - while ESC turns him down and leaves the screen an ordinary
		 * one.
		 */
		if(!dr3_spin_done&&(dr3_chaos_mode_setting() == DR3_CHAOS_MODE_DUEL)){

			const int dr3_player_seat = (int)D(___1a1ef8h);

			/*
			 * Any driver of the standings whose rank is within +-3 of the player's own, drawn at random
			 * (dr3_log.c: dr3_chaos_duel_pick).  The very same rule draws the rival in the race setup, and
			 * the rival named here is what the setup takes back (while he still is one of them).
			 */
			/*
			 * Drawn ONCE per visit: this loop runs every frame, and a pick per frame named a different rival
			 * each time (and wrote the log line 120 times in a few seconds).  The *line* is drawn every frame,
			 * because the screen pass wipes it, but it is cleared first and always names the same driver.
			 */
			if(dr3_offer_rival == -2){

				dr3_offer_rival = dr3_chaos_duel_pick(dr3_player_seat);

				if(dr3_offer_rival >= 0) dr3_chaos_duel_rival_set(dr3_offer_rival);
			}

			if(dr3_offer_rival >= 0){

				dr3_offer_line_clear();

				strcpy(esp, s_6c[dr3_offer_rival].name);
				strcat(esp, " WANTS A DUEL - ENTER: FIGHT, ESC: RACE ON");     /* capitals only: the font has no lower case (blocks) */
				___12e78h_v3(___1a10b8h___185c7ah, esp, 0x1e, DR3_OFFER_LINE_Y);
				___12cb8h__VESA101_PRESENTSCREEN();      /* like the selector's line: drawn into the screen, shown by this */
			}
		}

		{
			__BYTE__ dr3_key = ___5994ch();
			if(dr3_arena_skip_map){
				dr3_arena_skip_map = 0;
				if(B(___1a1f64h+D(___185a50h)+3) < 4) dr3_key = DR_SCAN_ENTER;
			}
		switch(dr3_key){
		case DR_SCAN_ESCAPE:

			/*
			 * In a duel ESC means "not now": the rival is turned down and the screen stays, so an ordinary
			 * race can still be signed up for (the mode is what the race setup reads, ___33010h.c).  The
			 * line the challenge was drawn in is cleared - this loop only redraws the rival's own entry.
			 */
			if((dr3_chaos_mode_setting() == DR3_CHAOS_MODE_DUEL)){

				dr3_chaos_mode_set(DR3_CHAOS_MODE_STANDARD);
				dr3_log("[dr3] chaos: duel: turned down - the next race is an ordinary one");

				dr3_offer_line_clear();
				___12cb8h__VESA101_PRESENTSCREEN();
				break;
			}

			while(1){

				___3079ch_cdecl(1);
				if((B(___1a1f64h+3) >= 4)&&(B(___1a1f64h+4) >= 4)&&(B(___1a1f64h+5) >= 4)) break;
			}

			esi = 0;
			break;
		case DR_SCAN_ENTER:
		case DR_SCAN_KP_ENTER:
			if((D(___185a50h) != 1)||(D(___185a50h) != D(___185a44h))||((int)s_6c[D(___1a1ef8h)].car >= 2)||(dr3_spin_race >= 0)){

				if((D(___185a50h) != 2)||(D(___185a48h) != 1)||((int)s_6c[D(___1a1ef8h)].car >= 4)||(dr3_spin_race >= 0)){

					if(B(___1a1f64h+D(___185a50h)+3) >= 4){

						dRally_Sound_pushEffect(1, SFX_BUMMER, 0, ___24cc54h_sfx_volume, 0x28000, 0x8000);

						while(1){

							___3079ch_cdecl(1);
							if((B(___1a1f64h+3) >= 4)&&(B(___1a1f64h+4) >= 4)&&(B(___1a1f64h+5) >= 4)) break;
						}

						esi = 0;
					}
					else {

						dRally_Sound_pushEffect(1, SFX_SIGNUP_BRAKES, 0, ___24cc54h_sfx_volume, 0x28000, 0x8000);
						edx = B(___1a1f64h+D(___185a50h)+3);

						j = -1;
						while(++j < 2){

							i = -1;
							while(++i < 0x88){

								if((px = B(___1a0f98h+0x88*j+i)) != 0) B(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x280*(0x12*(edx+1)+0x106)+0xa0*D(___185a50h)+0x1b+0x280*j+i) = px;
							}
						}

						itoa_watcom106(s_6c[D(___1a1ef8h)].rank, esp+0x3c, 0xa);
						strcpy(esp, "");
						if(strlen(esp+0x3c) < 2) strcat(esp, " ");
						strcat(strcat(strcat(esp, esp+0x3c), "."), strupr_watcom106(strcpy(esp+0x50, s_6c[D(___1a1ef8h)].name)));
						edx = B(___1a1f64h+D(___185a50h)+3);

						x = 0x280*(0x12*(edx+1)+0x100)+0xa0*D(___185a50h)+0x22;
						y = x/0x280;
						x = x%0x280;
						___12e78h_v3(___1a10b8h___185c7ah, esp, x, y);
						ebx = B(___1a1f64h+D(___185a50h)+3);
						D(___1a1028h) = D(___185a50h);
						B(___1a0ef8h+4*D(___185a50h)+ebx) = D(___1a1ef8h);
						B(___1a1f64h+D(___185a50h)+3)++;
						D(___1a103ch) = ebx;
						___12cb8h__VESA101_PRESENTSCREEN();

						while(1){

							___2ab50h();
							___2ab50h();
							___3079ch_cdecl(3);
							if((B(___1a1f64h+3) >= 4)&&(B(___1a1f64h+4) >= 4)&&(B(___1a1f64h+5) >= 4)) break;
						}

						/*
						 * "30th Anniversary": he must be registered in one of the three races, otherwise
						 * he cannot collect any points.  The picker usually did that already; this only
						 * catches the case that it did not - and it keeps him out of the race the player
						 * signed up for, unless there is no other choice (see doc/3ds.md)
						 */
						dr3_adversary_ensure();

						n2 = -1;
						while(++n2 < 3){

							memset(esp+0x3c, 0, 0x14);
							B(esp+0x50) = B(___1a0ef8h+4*n2);
							B(esp+0x51) = B(___1a0ef8h+4*n2+1);
							B(esp+0x52) = B(___1a0ef8h+4*n2+2);
							B(esp+0x53) = B(___1a0ef8h+4*n2+3);

							edi = 4*n2;
							n3 = -1;
							while(++n3 < 4){

								eax = 0;
								while(1){

									n = B(esp+eax+0x50);
									eax++;
									if(B(esp+n+0x3c) == 0) break;
								}

								eax = B(esp+0x51);
								if(((int)eax > n)&&(B(esp+eax+0x3c) == 0)) n = eax;

								eax = B(esp+0x52);
								if(((int)eax > n)&&(B(esp+eax+0x3c) == 0)) n = eax;

								eax = B(esp+0x53);
								if(((int)eax > n)&&(B(esp+eax+0x3c) == 0)) n = eax;

								B(esp+n+0x3c) = 1;
								B(___1a0ef8h+edi) = n;
								if(D(___1a1ef8h) == n) D(___1a103ch) = n3;
								edi++;
							}
						}

						if(!___32230h()&&!___31868h()&&!dr3_arena_fast_enter){	// NO_SABOTAGE && NO_EVENTS

							n = 0;
							while(1){

								___2ab50h();
								n++;
								if((___5994ch() != 0)||(n >= 280)) break;
							}
						}

						esi = 1;
						___33010h_cdecl(4);
						D(___185a20h) = 1;
					}
				}
				else {

					___3174ch_cdecl(D(___185a48h));
					D(___185a48h) = 0;
				}
			}
			else {

				___3174ch_cdecl(0);
				D(___185a44h) = 0;
			}
			break;
		case DR_SCAN_KP_4:
		case DR_SCAN_LEFT:
			/*
			 * The wheel decided: the choice is done.  The arrows only bump into it (the sound the screen
			 * plays for a race that is full), so the race the wheel drew is the one ENTER signs up for.
			 */
			if(dr3_spin_done&&(dr3_spin_race >= 0)){

				dRally_Sound_pushEffect(1, SFX_BUMMER, 0, ___24cc54h_sfx_volume, 0x28000, 0x8000);
				dr3_log("[dr3] chaos: spin: the choice is locked to race %d - the arrows only bump", dr3_spin_race);
				break;
			}

			if((int)D(___185a50h) > 0){

				dRally_Sound_pushEffect(1, SFX_CLICK_2, 0, ___24cc54h_sfx_volume, 0x28000, 0x8000);
				___281d0h_cdecl(0xa0*D(___185a50h)+0x16, 0x76, 0x94, 0x84);
				D(___185a50h)--;
				___27f80h_cdecl(0xa0*D(___185a50h)+0x16, 0x76, 0x94, 0x84);
				___12cb8h__VESA101_PRESENTSCREEN();
			}
			break;
		case DR_SCAN_KP_6:
		case DR_SCAN_RIGHT:
			/* the wheel decided - see the left arrow above */
			if(dr3_spin_done&&(dr3_spin_race >= 0)){

				dRally_Sound_pushEffect(1, SFX_BUMMER, 0, ___24cc54h_sfx_volume, 0x28000, 0x8000);
				dr3_log("[dr3] chaos: spin: the choice is locked to race %d - the arrows only bump", dr3_spin_race);
				break;
			}

			if((int)D(___185a50h) < 2){

				dRally_Sound_pushEffect(1, SFX_CLICK_2, 0, ___24cc54h_sfx_volume, 0x28000, 0x8000);
				___281d0h_cdecl(0xa0*D(___185a50h)+0x16, 0x76, 0x94, 0x84);
				D(___185a50h)++; 
				___27f80h_cdecl(0xa0*D(___185a50h)+0x16, 0x76, 0x94, 0x84);
				___12cb8h__VESA101_PRESENTSCREEN();
			}
			break;
		default:
			break;
		}
		}

		if(esi == 0){

			if((B(___1a1f64h+3) == 4)&&(B(___1a1f64h+3) == B(___1a1f64h+4))&&(B(___1a1f64h+3) == B(___1a1f64h+5))){
					
				___13248h_cdecl(33, 200, 580, 112, 1);
				___12e78h_v3(___1a10cch___185ba9h, "You did not sign up in any race.", 48, 225);
				___12e78h_v3(___1a1108h___185c0bh, "Press any key to continue.", 208, 261);
				___12cb8h__VESA101_PRESENTSCREEN();
				dRally_Sound_pushEffect(1, SFX_LAUGHTER, 0, ___24cc54h_sfx_volume, 0x25500-0x1000, 0x8000);

				while(1){

					___2ab50h();
					if(___5994ch() != 0) break;
				}

				___2b318h();

				n4 = -1;
				while(++n4 < 51){

					if((D(___185a14h_UseWeapons) != 0)&&(D(___185a4ch) != 0)) dRally_Sound_setMasterVolume(1310*(50-n4));

					___58c60h();
					
					n = -1;
					while(++n < 0x100){

						rr = helper_color(___19eb50h[n].r, 0x20000*(50-n4));
						gg = helper_color(___19eb50h[n].g, 0x20000*(50-n4));
						bb = helper_color(___19eb50h[n].b, 0x20000*(50-n4));

						__DISPLAY_SET_PALETTE_COLOR(bb, gg, rr, n);
					}
				}

				if(D(___185a14h_UseWeapons) != 0){

					if(D(___185a4ch) != 0){

						dRally_Sound_setPosition(D(___1a1ef4h));
						D(___185a28h) = 1;
						dRally_Sound_freeEffectChannel(1);
					}
				}

				___12940h();

				n = -1;
				while(++n < 0x100) __DISPLAY_SET_PALETTE_COLOR(0, 0, 0, n);

				esi = 1;
				___3892ch_cdecl(1);
				D(___185a20h) = 1;
			}
		}

		if(esi != 0) break;
	}

}
