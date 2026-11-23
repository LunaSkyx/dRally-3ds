#include "drally.h"
#include "platform_3ds/dr3_chaos.h"
#include "platform_3ds/dr3_results.h"
#include "platform_3ds/dr3_dm.h"    /* the endgame of a deathmatch: sudden death + its banner (dr3_dm.h) */
#include "platform_3ds/dr3_arena.h" /* the arena: the hunting AI, the places of the survivors */
#include "platform_3ds/dr3_cash.h"  /* off-line street cash ($) pickups for normal street races */
#include "platform_3ds/dr3_boss.h"  /* 30th Anniversary 3-phase final boss gauntlet */
#include "platform_3ds/dr3_anniv_prog.h"
#include "platform_3ds/dr3_bottom.h"
#if defined(__3DS__)
#include "platform_3ds/dr3_scenario.h"
#endif
#include "platform_3ds/dr3_prof.h"
#include "drally_keyboard.h"
#include "drally_race.h"
#include "drenums.h"
#include "drmath.h"
#include "sfx.h"
#include "drally_structs_free.h"
#if defined(DR3_HOST)
#include "platform_3ds/dr3_host.h"      /* the PC harness: the autopilot hands the player's car to the AI */
#elif defined(__3DS__)
/* the scenario harness has the same autopilot ("autopilot on", platform_3ds/dr3_scenario.h); off unless a script asks */
#define dr3_host_autopilot() dr3_scenario_autopilot()
#endif

#if defined(DR_MULTIPLAYER)
	extern __DWORD__ ___19bd60h;
#endif // DR_MULTIPLAYER

	extern __DWORD__ MY_CAR_IDX;
	extern int NUM_OF_CARS;
	extern int NUM_OF_LAPS;
	extern int TRX_WIDTH;
	extern int TRX_HEIGHT;
	extern __POINTER__ TRX_IMA;
	extern __POINTER__ ___243d74h;
	extern __POINTER__ TRX_MAS;
	extern __BYTE__ ___1a51d0h[0x300];	/* the palette of the loaded track image */
	extern const char ___18d492h[][0xf];	/* the track names the front end shows */
	extern __POINTER__ BACKBUFFER;
	extern __BYTE__ ___243d08h[];
	extern __BYTE__ ___243d0ch[];
	extern __BYTE__ ___1de580h[];
	extern __BYTE__ ___24387ch[];
	extern __BYTE__ ___243880h[];
	extern __BYTE__ ___243318h[];
	extern __BYTE__ ___243cd8h[];
	extern __BYTE__ ___243878h[];
	extern void_cb ___2432c8h;
	extern char ___19bd64h[16];
	extern __DWORD__ ___24cc58h_msx_volume;
	extern __DWORD__ ___24cc54h_sfx_volume;
	extern __BYTE__ ___196dach[];
	extern __BYTE__ ___243ca0h[];
	extern __BYTE__ ___243cdch[];
	extern __BYTE__ ___243cb8h[];
	extern __BYTE__ ___243cd4h[];
	extern __BYTE__ SUPERGLOBAL___243cd0h[];
	extern __BYTE__ ___243ce0h[];
	extern __BYTE__ ___1e6ed0h[];
	extern __BYTE__ ___196df0h[];
	extern __BYTE__ ___243334h[];
	extern __BYTE__ ___243c60h[];
	extern __BYTE__ ___2438cch[];
	extern __BYTE__ ___2438d4h[];
	extern __BYTE__ ___196ddch[];
	extern __BYTE__ ___196de0h[];
	extern __BYTE__ ___196df8h[];
	extern __BYTE__ ___2438c8h[];
	extern __BYTE__ kmap[];
	extern __BYTE__ ___243d00h[];
	extern int TRX_VIEWPORT_TL_X;
	extern int TRX_VIEWPORT_TL_Y;
	extern int CURRENT_VIEWPORT_W;
	extern int CURRENT_VIEWPORT_X;
	extern __BYTE__ ___196dd4h[];
	extern __BYTE__ ___243d14h[];
	extern __BYTE__ * VGA13_ACTIVESCREEN;
	extern int CURRENT_VIEWPORT_CENTER_X;
	extern __BYTE__ ___1de7d0h[];
	extern __BYTE__ ___196dd8h[];
	extern __DWORD__ ___243d44h;

#if defined(DR_MULTIPLAYER)
void race___44304h(void);
void race___44a78h(void);
void race___47ed8h(__DWORD__);
#endif // DR_MULTIPLAYER

void race___400ech(void);
void race___46a10h(void);
void race___45d3ch(void);
void race___560d8h(void);
void race___454ach(void);
void __VGA13_PRESENTSCREEN__(void);
__DWORD__ __GET_FRAME_COUNTER(void);
void race___4207ch(void);
void race___40f48h(void);
void race___40db4h(void);
void race___42218h(void);
void race___56594h(void);
void race___51eb4h(void);
void race___50ba4h(void);
void race___53464h(void);
void race___53310h(void);
void race___51204h(void);
void race___50a48h(void);
void race___4f170h(void);
void race___51ce0h(void);
void race___4f300h(void);
void race___50ef4h(void);
void race___52d7ch(void);
void race___4f030h(void);
void race___4ee9ch(void);
void race___46738h__VGA13_PRESENTCLEARSCREEN(void);
void race___478c8h(__DWORD__);
void race___48458h(void);
void race___49390h(void);
void race___4c21ch(void);
void race___56170h(void);
void race___55ae0h(void);
void race___54668h(void);
int rand_watcom106(void);
void race___4ff50h(void);
void race___526ach(void);
void race___514d8h(void);
void race___4dcach(void);
void race___4c434h(void);
void race___4b62ch(int);
void race___5209ch(void);
void race___563ach(void);
void race___405bch(void);
void race___496b0h(void);
void race___4af3ch(void);
void race___45a24h(void);
void race___3f970h(void);
void race___49a34h(void);
void race___40164h(void);
void ___60705h(void_cb);
void race___49bd4h(void);
void race___492f4h(void);
void dRally_Sound_stop(void);
void dRally_Sound_release(void);
void ___606dfh(void);
void ___605deh_cdecl(__DWORD__, __DWORD__);
void dRally_Sound_load(__DWORD__ msx_t, const char * msx_f, __DWORD__ sfx_t, const char * sfx_f, __DWORD__ num_ch);
void dRally_Sound_setMasterVolume(__DWORD__ vol);
void dRally_Sound_setMusicVolume(__DWORD__ vol);
void dRally_Sound_setEffectsVolume(__DWORD__ vol);
void dRally_Sound_setSampleRate(__DWORD__ freq);
void dRally_Sound_play(void);
void dRally_Sound_pushEffect(__BYTE__ channel, __BYTE__ n, __DWORD__ unk, __DWORD__ a0, __DWORD__ a1, __DWORD__ a2);
__BYTE__ dRally_Sound_setPosition(__DWORD__ pos_n);
void race___52ac4h(void);
void dRally_Sound_freeEffectChannel(__BYTE__ ch_num);
void dRally_Sound_adjustEffect(__BYTE__ channel, __DWORD__ vol, __DWORD__ freq, __DWORD__ balance);
__DWORD__ dRally_Sound_getPosition(void);
void ___60719h(void);
void ___61418h(__DWORD__);
void srand_watcom106(__DWORD__);
void race___4539ch(void);
void race___42824h(void);
void race___4a25ch(void);
void race___4a6b4h(void);
void race___4083ch(void);
void race___4adach(void);
void race___4580ch(void);
void __VGA13_SETMODE(void);
void __RACE400_SETMODE(void);

static void helper01(void (*cb)(int), int slot){

	int 	n;
	unsigned long long t0 = dr3_chaos_tick();

	n = -1;
	while(++n < NUM_OF_CARS) cb(n);

	dr3_chaos_time(slot, t0);               /* this one loop ...                */
	dr3_chaos_time(DR3_CT_PER_CAR, t0);     /* ... and all six of them together */
}

static __WORD__ helper22(const char * cp){

	if(!strcmp(cp, "TR0")) return 0x1e00;
	if(!strcmp(cp, "TR1")) return 0x3700;
	if(!strcmp(cp, "TR2")) return 0x2d00;
	if(!strcmp(cp, "TR3")) return 0x3200;
	if(!strcmp(cp, "TR4")) return 0x2d00;
	if(!strcmp(cp, "TR5")) return 0x3700;
	if(!strcmp(cp, "TR6")) return 0x3200;
	if(!strcmp(cp, "TR7")) return 0x3200;
	if(!strcmp(cp, "TR8")) return 0x3200;
	if(!strcmp(cp, "TR9")) return 0x3200;

	return 0;
}

static void helper_freeEffectChannels(void){
	
	int 	n;

	n = -1;
	while(++n < 0xe) dRally_Sound_freeEffectChannel(n+1);
}

static __WORD__ helper33(void){

	__DWORD__ 	ecx, ecxx;

	helper_freeEffectChannels();
	ecx = dRally_Sound_getPosition()&0xff00;
	if((ecxx = helper22(___19bd64h)) != 0) dRally_Sound_setPosition(ecxx);
	dRally_Sound_setMasterVolume(0x8000);

	return ecx;
}

#if defined(DR3_CHAOS)
/*
 * Chaos test build (-DDR3_CHAOS, doc/3ds.md): the race steps go into the log once the traffic light is
 * about to turn green (counter 1 reaches 0xbe = the release), so if a race hangs the log shows the
 * step and the car it stopped on.  Capped, so the log cannot grow without bound.
 */
#define DR3_CHAOS_STEP_LOG 2500

static void dr3_chaos_step(const char *what, int car){

	static int shown;

	if(!dr3_log_enabled()) return;              /* the switch: no trace, no heap check */
	if((int)getCounter(1) < 0xa0) return;
	if(shown >= DR3_CHAOS_STEP_LOG) return;

	if(shown == 0) dr3_log("[dr3] chaos trace on (light counter %d)", (int)getCounter(1));

	dr3_log("[dr3] chaos step %-8s car %d", what, car);
	++shown;
}
#else
#define dr3_chaos_step(what, car) ((void)0)
#endif

/*
 * The band measurement lives in platform_3ds/dr3_log.c and is only there with the test build: drally.h
 * pulls dr3_log.h in on the 3DS (which itself turns these into no-ops without DR3_CHAOS), and this keeps
 * the other targets, which do not see that header, compiling as well.
 */
#if !defined(DR3_CHAOS)
#define dr3_chaos_band_start()    ((void)0)
#define dr3_chaos_band_mark(pass) ((void)0)
#define dr3_chaos_extra_pass_limit(cars) (cars)
#define dr3_chaos_skip_pass(which) (0)
#endif

#if defined(DR3_CHAOS)
/*
 * THE GREY LINE CHASE: the palette indices of the rendered frame around the player's car, dumped twice
 * per frame - once right after the track shadows (that is the road the car pass starts from) and once at
 * the end of the frame.  A pixel that differs between the two dumps was written after the shadows, and
 * its value says by what: the car's own sprite lives in 0x00..0x18, everything else in that rectangle
 * came from a layer that has no business being there.
 *
 * Every car of the race is listed with its screen position as well, so a car drawn over the player - the
 * other cars' sprites are drawn *after* his (race___4f300h.c) - is visible from the log alone.
 */
static int dr3_chaos_dump_due(void)
{
	static int dr3_dump_n;
	static int dr3_dump_img = -99;

	struct_35e_t * s_35e = (struct_35e_t *)___1e6ed0h;

	if(!dr3_chaos_dump_on()) return 0;           /* the switch: no frame dump at all (chaos_dump.txt) */

	/* once the car has turned by 30 degrees, so every angle gets captured */
	if(((int)s_35e[MY_CAR_IDX].ImgIndex - dr3_dump_img + 96) % 96 >= 8){

		dr3_dump_img = (int)s_35e[MY_CAR_IDX].ImgIndex;
		dr3_dump_n = 0;
	}

	if(dr3_dump_n >= 3) return 0;

	++dr3_dump_n;
	return 1;
}

/*
 * The player's sprite position for *this* frame.  race___4f300h.c sets __4/__8 while it draws the cars -
 * after the first dump of a pair - so that dump would otherwise be taken around last frame's position and
 * the two dumps of a pair would be a pixel or two apart, which makes every diff between them noise.  The
 * camera values are final here (race___4ee9ch.c ran before), so repeating its formula gives both dumps the
 * same window.  __4/__8 end up with exactly these numbers once the car pass has run.
 */
static void dr3_chaos_player_pos(int *px, int *py)
{
	extern int X___243c8ch;
	extern int Y___243c90h;
	extern int CURRENT_VIEWPORT_W;
	extern int CURRENT_VIEWPORT_H;
	extern int CURRENT_VIEWPORT_CENTER_Y;

	struct_35e_t * s = (struct_35e_t *)___1e6ed0h;
	const int      n = (int)MY_CAR_IDX;
	double         dx;
	int            x, y;

	dx = (double)CURRENT_VIEWPORT_CENTER_X;
	if(dx > (double)X___243c8ch + (double)s[n].XLocation) x = (int)s[n].XLocation;
	else if((double)(TRX_WIDTH - CURRENT_VIEWPORT_CENTER_X) < (double)X___243c8ch + (double)s[n].XLocation)
		x = (int)((double)s[n].XLocation - (double)(int)(TRX_WIDTH - CURRENT_VIEWPORT_W));
	else x = CURRENT_VIEWPORT_CENTER_X - X___243c8ch;

	dx = (double)CURRENT_VIEWPORT_CENTER_Y;
	if(dx > (double)Y___243c90h + (double)s[n].YLocation) y = (int)s[n].YLocation;
	else if((double)(TRX_HEIGHT - CURRENT_VIEWPORT_CENTER_Y) < (double)Y___243c90h + (double)s[n].YLocation)
		y = (int)((double)s[n].YLocation - (double)(int)(TRX_HEIGHT - CURRENT_VIEWPORT_H));
	else y = CURRENT_VIEWPORT_CENTER_Y - Y___243c90h;

	*px = x + CURRENT_VIEWPORT_X + 76;      /* the sprite's left edge */
	*py = y - 20;                           /*       and top edge     */
}

static void dr3_chaos_dump_frame(const char *when){
	struct_35e_t * s_35e = (struct_35e_t *)___1e6ed0h;
	extern __POINTER__ ___243d74h;
	int            px, py;
	int            x, y, n;

	if(!dr3_log_enabled()) return;              /* the switch: nothing is dumped, nothing is hashed */

	dr3_chaos_player_pos(&px, &py);

	dr3_log("[dr3] chaos: frame dump %s: car at %d,%d (direction %d, img %d) - 40 px above included",
	        when, px, py, (int)s_35e[MY_CAR_IDX].Direction, (int)s_35e[MY_CAR_IDX].ImgIndex);

	/*
	 * The sprite frame that is on screen right now, and its top rows: the lines sit in the rows above the
	 * car's body, which are the sprite's own first rows - so if they are in the data, they show up here.
	 */
	{
		const __BYTE__ * fr = (const __BYTE__ *)___243d74h + (int)s_35e[MY_CAR_IDX].ImgOffset;
		char line[96];
		int  r, c, at;

		dr3_log("[dr3] chaos:   sprite: img %d, offset 0x%x, dir %d, stripe base %p, first row bytes %02x %02x %02x",
		        (int)s_35e[MY_CAR_IDX].ImgIndex, (int)s_35e[MY_CAR_IDX].ImgOffset,
		        (int)s_35e[MY_CAR_IDX].Direction, (void *)___243d74h, fr[0], fr[1], fr[2]);

		/*
		 * The state of the player's stripe right now: if the sprite data is being damaged while racing, this
		 * number changes from dump to dump (and not just between the races).
		 */
		{
			const __BYTE__ * base = (const __BYTE__ *)___243d74h;
			unsigned         sum = 0u, xr = 0u;
			int              k;

			for(k = 0; k < (int)sizeof(car_spins_t); ++k){ sum += base[k]; xr = (xr << 1) ^ (unsigned)base[k]; }

			dr3_log("[dr3] chaos:   stripe 0 checksum sum %08x xor %08x", sum, xr);
		}

		for(r = 0; r < 40; ++r){

			if((r >= 8) && (r < 32)) continue;      /* the frame's top and bottom rows are what matters */

			at = 0;

			for(c = 0; c < 40; ++c){

				const int v = fr[40*r + c];

				line[at++] = "0123456789abcdef"[(v >> 4) & 0xf];
				line[at++] = "0123456789abcdef"[v & 0xf];
			}

			line[at] = 0;
			dr3_log("[dr3] chaos:   sprite row %2d |%s|", r, line);
		}
	}

	for(n = 0; n < NUM_OF_CARS; ++n)
		dr3_log("[dr3] chaos:   car %2d at %3d,%3d img %2d%s", n,
		        (int)s_35e[n].__4, (int)s_35e[n].__8, (int)s_35e[n].ImgIndex,
		        (n == (int)MY_CAR_IDX) ? "   <- the player" : "");

	for(y = py - 40; y < py + 46; y += 2){

		char row[128];
		int  at = 0;

		for(x = px - 10; x < px + 50; ++x){

			if((x < 0) || (y < 0) || (x >= 320) || (y >= 200)){

				row[at++] = '?';
				row[at++] = '?';
			}
			else {

				/* the exact palette index, two hex digits, so the box cannot hide */
				const int v = BACKBUFFER[0x200*y + x];

				row[at++] = "0123456789abcdef"[(v >> 4) & 0xf];
				row[at++] = "0123456789abcdef"[v & 0xf];
			}
		}

		row[at] = 0;
		dr3_log("[dr3] chaos: |%s|", row);
	}

	dr3_log_flush();
}
#endif

/* the AI of one car: the road race's, or - in the arena - the hunter's (platform_3ds/dr3_arena.h) */
static void dr3_ai_call(int n){

#if defined(DR3_CHAOS)
	if(dr3_chaos_arena()){ dr3_arena_ai(n); return; }
#endif
	race___4b62ch(n);
}

static void race___4b62ch_helper(int n){

	dr3_chaos_step("4b62ch", n);

	//D(___243c60h) = n;

	if(D(___243ce0h) != 0){

		dr3_ai_call(n);
	}
	else {
#if defined(DR_MULTIPLAYER)
		if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
#if defined(DR3_HOST) || defined(__3DS__)
			/*
			 * The harness autopilot (platform_3ds/dr3_host.h): with it on the engine's own AI drives the
			 * player's car too, so a scripted run races the track instead of holding gas into the first
			 * corner (the chaos grid puts the player around a bend of the road - see the note in
			 * dr3_host.h).  Off unless the run asked for it; the console build compiles the plain line.
			 */
			if((MY_CAR_IDX != n)||dr3_host_autopilot()) dr3_ai_call(n);
#else
			if(MY_CAR_IDX != n) dr3_ai_call(n);
#endif
#if defined(DR_MULTIPLAYER)
		}
#endif // DR_MULTIPLAYER
	}
}

static void race___4c434h_helper(int n){ dr3_chaos_step("4c434h", n); D(___243c60h) = n; race___4c434h(); }
static void race___514d8h_helper(int n){ dr3_chaos_step("514d8h", n); D(___243c60h) = n; race___514d8h(); }
static void race___52ac4h_helper(int n){ dr3_chaos_step("52ac4h", n); D(___243c60h) = n; race___52ac4h(); }
static void race___526ach_helper(int n){ dr3_chaos_step("526ach", n); D(___243c60h) = n; race___526ach(); }
static void race___4ff50h_helper(int n){ dr3_chaos_step("4ff50h", n); D(___243c60h) = n; race___4ff50h(); }

// ___56774h
void race_main(int MyIndex, int NumCars){		// my_position_index, number_of_racers

	dr3_prof_phase(DR3_PHASE_RACE);


	__DWORD__ 	eax, ebx, ecx, edx;
	__BYTE__ 	esp[0x1c];
	int 	i, j, n, bool_tmp, k, m, c, frames;
	struct_35e_t *	s_35e;
	struct_94_t * 	s_94;
	struct_54_t *	s_54;



	s_54 = (struct_54_t *)___1de7d0h;
	s_94 = (struct_94_t *)___1de580h;
	s_35e = (struct_35e_t *)___1e6ed0h;
	MY_CAR_IDX = MyIndex;
	NUM_OF_CARS = NumCars;
#if defined(__3DS__)
	dr3_scenario_event("race_start");    /* scripted runs wait for this (platform_3ds/dr3_scenario.h) */
#endif
#if defined(DR3_CHAOS)
	/*
	 * Chaos test build (-DDR3_CHAOS, see doc/3ds.md): more cars than the race asked for.  Every
	 * per car array is sized with DR3_MAX_CARS (bss.c), the extra grid entries are set up in
	 * ___33010h.c and the in-race car list stops at DR3_HUD_LOOP_LIMIT rows, so only the race grows.
	 *
	 * How many that are is the switch dr3_chaos_extra_cars() - a variable, so it can be flipped while
	 * the game runs (SELECT + RIGHT, or sdmc:/chaos_extras.txt, see dr3_chaos.h).  It is read here, in
	 * ___33010h.c and in race___42824h.c, all of which run when a race starts - so the switch always
	 * takes effect with the next race.
	 */
	{
		extern __BYTE__ ___1a1028h[];
		const int dr3_final_boss = (D(___1a1028h) == 3);
		const int dr3_extra = (dr3_final_boss || (dr3_chaos_mode() == DR3_CHAOS_MODE_DUEL)) ? 0
		                    : dr3_chaos_arena() ? dr3_arena_extras(NumCars, dr3_chaos_extra_cars())
		                    : dr3_chaos_extra_cars();

		if (dr3_boss_is_swarm())            NUM_OF_CARS = DR3_BOSS_P1_CARS;
		else if (dr3_boss_is_escort_race()) NUM_OF_CARS = DR3_BOSS_P2_CARS;
		else if (dr3_boss_is_final_duel())  NUM_OF_CARS = DR3_BOSS_P3_CARS;
		else if (dr3_chaos_active() && !dr3_final_boss && (dr3_chaos_mode() != DR3_CHAOS_MODE_DUEL) && (dr3_anniv_active_field() > 0))
			NUM_OF_CARS = dr3_anniv_active_field();
		else NUM_OF_CARS = (NumCars + dr3_extra <= DR3_CHAOS_RACERS) ? (NumCars + dr3_extra) : DR3_CHAOS_RACERS;
		dr3_log("[dr3] CHAOS build: %d cars on the grid (the race asked for %d, the extras are %s (%d))",
		        NUM_OF_CARS, NumCars, dr3_extra ? "on" : "off", dr3_extra);
		dr3_log("[dr3] chaos: track shadows are %s for this race",
		        dRally_Race_getSettings(RACE_SHADOWS) ? "ON" : "OFF");
		dr3_log("[dr3] chaos: the extra car ladder is at stage %d for this race (0 nothing drawn, %d everything)",
		        dr3_chaos_extra_stage(), DR3_EXTRA_STAGE_ALL);
		dr3_log("[dr3] chaos: this race runs on track %s", ___19bd64h);
	}
#endif
	race___3f970h();
	race___49a34h();

#if defined(DR_MULTIPLAYER)
	if(___19bd60h != 0){
			
		D(___243d08h) = 0;
		D(___243d0ch) = 0;

		n = -1;
		while(++n < NUM_OF_CARS){

			D(___243d0ch) += s_94[n].__38;
			D(___243d0ch) += s_94[n].__3c;
			D(___243d0ch) += s_94[n].__40;
			D(___243d08h) += 1;
		}

		srand_watcom106(D(___243d0ch));
		D(___24387ch) = 0;
		D(___243880h) = 1;
		D(___243318h) = 0;
		D(___243cd8h) = 0;
		D(___243878h) = 0;
	}
#endif // DR_MULTIPLAYER

	___60705h(&race___40164h);
	race___49bd4h();
	race_alloc();
	race___4539ch();
	___2432c8h = &race___492f4h;
	race___42824h();
	race___4a25ch();
	race___405bch();
	race___4a6b4h();
	race___4083ch();
	race___4adach();
	race___4580ch();
	dRally_Sound_stop();
	dRally_Sound_release();
	___606dfh();
#if DR3_NATIVE_WIDESCREEN
	__RACE400_SETMODE();
#else
	__VGA13_SETMODE();
#endif

	bool_tmp = 1;
#if defined(DR_MULTIPLAYER)
	if(___19bd60h != 0) bool_tmp = 0;
#endif // DR_MULTIPLAYER

	___605deh_cdecl(0x46, bool_tmp);
	dRally_Sound_load(1, strcat(strcpy(esp, dr3_chaos_arena() ? DR3_ARENA_MUSIC : ___19bd64h), "-MUS.CMF"), 2, "GEN-EFE.CMF", 0xe);
	dRally_Sound_setMasterVolume(0);
	dRally_Sound_setMusicVolume(___24cc58h_msx_volume);
	dRally_Sound_setEffectsVolume(___24cc54h_sfx_volume);
	dRally_Sound_setSampleRate(0x5622);
	dRally_Sound_play();
	dRally_Sound_setMasterVolume(0);
	if(!dRally_Race_getSettings(RACE_MSX)) dRally_Sound_setMusicVolume(0);
	if(!dRally_Race_getSettings(RACE_SFX)) dRally_Sound_setEffectsVolume(0);
	race___45a24h();
	if(D(___196dach) != 0) race___4af3ch();
	/* hand the finished track (mask + image + its palette + its name) to the bottom screen minimap -
	   a no-op on every other build (see platform_3ds/dr3_bottom.h).  It goes here, behind the
	   reverse-track mirroring, so the map shows the track as it is driven. */
	{
		/*
		 * The name for the header: ___106cbh[0..8] holds TR1..TR9 for the first season half and
		 * [9..17] the same tracks driven backwards, [18] is TR0 - and ___18d492h holds 18 names for
		 * exactly those slots (so "TR7" has a different name in the reversed half).
		 */
		const int    n    = ((___19bd64h[0] == 'T') && (___19bd64h[1] == 'R')) ? (___19bd64h[2] - '0') : -1;
		int          slot = -1;
		const char * name = NULL;

		if ((n >= 1) && (n <= 9)) slot = (n - 1) + ((D(___196dach) != 0) ? 9 : 0);
		if ((slot >= 0) && (slot <= 17)) name = ___18d492h[slot];

		if (dr3_chaos_arena()) name = "ARENA";

		dr3_bottom_track_loaded(TRX_MAS, TRX_IMA, ___1a51d0h, TRX_WIDTH, TRX_HEIGHT,
		                        ___19bd64h, name);
	}
	race___496b0h();
#if defined(DR3_CHAOS)
	dr3_cash_setup();
#endif
	race___405bch();
	race___49a34h();
	dRally_Sound_pushEffect(1, (s_94[MY_CAR_IDX].__0&0xff)+25, 0, 0x10000, 0x28000, 0x8000);
	resetCounter(1);
	D(___243ca0h) = 0;
	D(___243cdch) = 0;
	D(___243cb8h) = 0;
	D(___243cd4h) = 0;
	resetCounter(5);
	resetCounter(2);
	D(SUPERGLOBAL___243cd0h) = 0;

	while(1){

		const unsigned long long dr3_t_pre = dr3_chaos_tick();

		resetCounter(4);
		setCounter(5, getCounter(2));
		resetCounter(2);
		s_35e[MY_CAR_IDX].__a4 = s_35e[MY_CAR_IDX].ActionFlags_i;

		if((int)D(___196df0h) > 0){

			D(___196df0h) -= D(___243334h);

			if((int)D(___196df0h) <= 0){

				dRally_Sound_pushEffect(2, SFX_LAP_RECORD, 0, 0x10000, 0x50000, 0x8000);
				D(___196df0h) = 0;
			}
		}

#if defined(DR_MULTIPLAYER)
		if((___19bd60h != 0)&&(D(___24387ch) != 0)) race___44a78h();
#endif // DR_MULTIPLAYER

		D(___243d08h) = 0;

		if((int)getCounter(5) > 0){

			while(1){

				s_35e[MY_CAR_IDX].__a4--;
				if((int)s_35e[MY_CAR_IDX].__a4 < 0) s_35e[MY_CAR_IDX].__a4 = 0xf;
				D(___243d08h)++;
#if defined(DR3_HOST) || defined(__3DS__)
				/*
				 * The harness autopilot (platform_3ds/dr3_host.h): the AI of race___4b62ch owns the player's
				 * controls then, so the input trace - which holds gas/turbo/gun and never steers - must not
				 * write them as well (it would put the keys back into the slot of a sub-step the AI wrote,
				 * and the car would drive with the keys again).  The ring bookkeeping above is kept, so the
				 * rest of the frame sees exactly what it always saw.
				 */
				if(!dr3_host_autopilot())
#endif
				s_35e[MY_CAR_IDX].Ctrls[getCounter(5)-D(___243d08h)] = s_35e[MY_CAR_IDX].ActionFlags[s_35e[MY_CAR_IDX].__a4];
				if((int)D(___243d08h) >= (int)getCounter(5)) break;
			}
		}

		race___563ach();	// HORNS
		race___5209ch();
#if defined(DR3_CHAOS)
		dr3_cash_tick();
#endif
	
		if((int)getCounter(5) > 0){

			while(1){

				incCounter(1);

				if((int)getCounter(1) > 0xbe){

					helper01(&race___4b62ch_helper, DR3_CT_PC_SPRITE);
					k = -1; while(++k < NUM_OF_CARS) s_35e[k].__14 = 0;
					helper01(&race___4c434h_helper, DR3_CT_PC_PHYS);
					k = -1; while(++k < NUM_OF_CARS) s_35e[k].__18 = 0;
					k = -1; while(++k < NUM_OF_CARS) s_35e[k].__1c = 0;
					{
						unsigned long long t1 = dr3_chaos_tick();
						race___4dcach();
						dr3_chaos_time(DR3_CT_COLLISION, t1);
					}
					helper01(&race___514d8h_helper, DR3_CT_PC_SKID);

#if defined(DR_MULTIPLAYER)
					if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
						helper01(&race___52ac4h_helper, DR3_CT_PC_A);
#if defined(DR_MULTIPLAYER)
					}
#endif // DR_MULTIPLAYER

					helper01(&race___526ach_helper, DR3_CT_PC_B);
#if defined(DR3_CHAOS)
					dr3_cash_collect();
#endif
					helper01(&race___4ff50h_helper, DR3_CT_PC_OFFROAD);
				}

				k = -1;
				while(++k < NUM_OF_CARS){

					if((int)0 < (int)s_35e[k].__192) s_35e[k].__192--;
					if((int)0 < (int)s_35e[k].__196) s_35e[k].__196--;
					if((int)0 < (int)s_35e[k].__17a) s_35e[k].__17a--;

					if(s_35e[k].__14 == 1) s_35e[k].__192 += 2;
					if((int)s_35e[k].__192 > 8) s_35e[k].__192 = 8;

					if(s_35e[k].__18 == 1){

						s_35e[k].XLocation = s_35e[k].__14e;
						s_35e[k].__196 += 2;
					}

					if(s_35e[k].__1c == 1){

						s_35e[k].YLocation = s_35e[k].__152;
						s_35e[k].__196 += 2;
					}
					if((int)s_35e[k].__196 > 8) s_35e[k].__196 = 8;

					/*
					 * Final post-collision wall clamp before committing (__14e, __152):
					 * If car-to-car nudges in race___4dcach() moved car k into a solid wall ((TRX_MAS & 0xf) < 4),
					 * revert to the cleaner of (__14e, __152) or eject along the outward wall normal so a
					 * 10-20 car Mega Chaos pileup can NEVER commit a wall-embedded position.
					 */
					if((s_35e[k].__14 != 0) || (s_35e[k].__18 != 0) || (s_35e[k].__1c != 0)){
						const unsigned char *sp = (const unsigned char *)(___243d74h + (int)s_35e[k].ImgOffset);
						dr3_wall_hit_t cur_hit;

						dr3_wall_scan_sprite(
							sp,
							(const unsigned char *)TRX_MAS,
							TRX_WIDTH, TRX_HEIGHT,
							(int)((double)s_35e[k].XLocation - 20.0),
							(int)((double)s_35e[k].YLocation - 20.0),
							&cur_hit);

						if(cur_hit.count > 0){
							dr3_wall_hit_t prev_hit;
							float zero_vx = 0.0f, zero_vy = 0.0f;

							dr3_wall_scan_sprite(
								sp,
								(const unsigned char *)TRX_MAS,
								TRX_WIDTH, TRX_HEIGHT,
								(int)((double)s_35e[k].__14e - 20.0),
								(int)((double)s_35e[k].__152 - 20.0),
								&prev_hit);

							if(prev_hit.count < cur_hit.count){
								s_35e[k].XLocation = s_35e[k].__14e;
								s_35e[k].YLocation = s_35e[k].__152;
								cur_hit = prev_hit;
							}
							if(cur_hit.count > 0){
								dr3_wall_resolve_step(
									sp,
									(const unsigned char *)TRX_MAS,
									TRX_WIDTH, TRX_HEIGHT,
									&s_35e[k].XLocation,
									&s_35e[k].YLocation,
									&zero_vx,
									&zero_vy,
									&s_35e[k].__fc,
									&s_35e[k].__100,
									NULL,
									(int)s_35e[k].__192,
									NULL,
									&cur_hit);
							}
						}
					}

					s_35e[k].__152 = s_35e[k].YLocation;
					s_35e[k].__156 = s_35e[k].Direction;
					s_35e[k].__14e = s_35e[k].XLocation;

					if((s_35e[k].__192 == 2)||(s_35e[k].__196 == 2)){

						if(s_35e[k].Finished == 0){

							const int dr3_hp_before = (int)s_94[k].__18;

							D(esp+0x18) = 0x400-s_94[k].__1c;
							D(___2438cch) = (int)((double)D(esp+0x18)*(double)(s_35e[k].__fc*s_35e[k].__fc+s_35e[k].__100*s_35e[k].__100));
							if((int)D(___2438cch) > 0x2710) D(___2438cch) = 0x2710;
							s_94[k].__18 -= D(___2438cch);
							if((int)0 > (int)s_94[k].__18) s_94[k].__18 = 0;

							/*
							 * This is the car that ran into the border of the road (__192/__196 are the
							 * counters a clamped car keeps).  That is nobody's fault - but when the player
							 * had a hand in it a moment ago, the chaos bonus (race___4dcach.c) pays for it.
							 */
							dr3_chaos_wreck(k, dr3_hp_before, "the road border");
						}
					}
				}

				if(((int)getCounter(1) > 0xbe) && (((int)getCounter(1) % 70) == 0)){
					int wa_k, wa_embedded = 0, wa_max_px = 0, wa_touching = 0;
					for(wa_k = 0; wa_k < NUM_OF_CARS; ++wa_k){
						const unsigned char *wa_sp = (const unsigned char *)(___243d74h + (int)s_35e[wa_k].ImgOffset);
						dr3_wall_hit_t wa_hit;
						if(s_35e[wa_k].__14 != 0) ++wa_touching;
						dr3_wall_scan_sprite(
							wa_sp,
							(const unsigned char *)TRX_MAS,
							TRX_WIDTH, TRX_HEIGHT,
							(int)((double)s_35e[wa_k].XLocation - 20.0),
							(int)((double)s_35e[wa_k].YLocation - 20.0),
							&wa_hit);
						if(wa_hit.count > 0){
							++wa_embedded;
							if(wa_hit.count > wa_max_px) wa_max_px = wa_hit.count;
						}
					}
					dr3_log("[dr3] wall_audit: tick=%d cars=%d wall_rubs=%d embedded_in_wall=%d max_wall_px=%d",
					        (int)getCounter(1), (int)NUM_OF_CARS, wa_touching, wa_embedded, wa_max_px);
				}

				if(s_35e[MY_CAR_IDX].__14||s_35e[MY_CAR_IDX].__18||s_35e[MY_CAR_IDX].__1c){

					D(___2438cch) = (int)(25000.0*sqrt((double)(s_35e[MY_CAR_IDX].__fc*s_35e[MY_CAR_IDX].__fc+s_35e[MY_CAR_IDX].__100*s_35e[MY_CAR_IDX].__100)));

					if((int)D(___2438cch) > 0x10000) D(___2438cch) = 0x10000;

					if((s_35e[MY_CAR_IDX].__18 != 0)||(s_35e[MY_CAR_IDX].__1c != 0)){

						switch((int)getCounter(3)%3){
						case 0:
							dRally_Sound_pushEffect(5, SFX_CAR_HIT_1, 0, D(___2438cch), 0x21000, 0x8000);
							break;
						case 1:
							dRally_Sound_pushEffect(5, SFX_METAL, 0, D(___2438cch), 0x23000, 0x8000);
							break;
						case 2:
							dRally_Sound_pushEffect(5, SFX_CAR_HIT_3, 0, D(___2438cch), 0x23000, 0x8000);
							break;
						default:
							break;
						}
					}

					D(___243d08h) = rand_watcom106()%0x6000;

					switch(s_35e[MY_CAR_IDX].__18e){
					case 0:
						switch((int)getCounter(3)%3){
						case 0:
							dRally_Sound_pushEffect(5, SFX_METAL_HIT, 0, D(___2438cch), D(___243d08h)+0x22000, 0x8000);
							break;
						case 1:
							dRally_Sound_pushEffect(5, SFX_CAR_HIT_2, 0, D(___2438cch), D(___243d08h)+0x22000, 0x8000);
							break;
						case 2:
							dRally_Sound_pushEffect(5, SFX_CAR_HIT_3, 0, D(___2438cch), D(___243d08h)+0x22000, 0x8000);
							break;
						default:
							break;
						}
						break;
					case 1:
						dRally_Sound_pushEffect(5, SFX_CAR_HIT_1, 0, D(___2438cch), D(___243d08h)+0x22000, 0x8000);
						break;
					case 2:
						dRally_Sound_pushEffect(5, SFX_CAR_HIT_2, 0, D(___2438cch), D(___243d08h)+0x22000, 0x8000);
						break;
					case 3:
						dRally_Sound_pushEffect(5, SFX_CAR_HIT_3, 0, D(___2438cch), D(___243d08h)+0x22000, 0x8000);
						break;
					default:
						break;
					}
				}

				if(((int)D(___2438d4h) > 0)&&(D(___196ddch) == 0)){

					if(((int)s_94[MY_CAR_IDX].__18 > 0)&&(s_35e[MY_CAR_IDX].Finished == 0)){
						
						if(D(___196de0h) == 2){

							dRally_Sound_pushEffect(6, SFX_BRAKES, 0, D(___2438d4h), 0x28000, 0x8000);
							D(___196ddch) = 1;
						}
					}
				}

				if((D(___2438d4h) == 0)&&(D(___196ddch) == 1)){
					
					dRally_Sound_freeEffectChannel(6);
					D(___196ddch) = 0;
				}

				D(___2438d4h) = 0;

				if(((int)s_94[MY_CAR_IDX].__18 <= 0)||(s_35e[MY_CAR_IDX].Finished != 0)){

					dRally_Sound_freeEffectChannel(1);
					dRally_Sound_freeEffectChannel(6);
				}

				F32(esp+0x14) = (float)dRMath_abs((double)s_35e[MY_CAR_IDX].__b0/(double)s_94[MY_CAR_IDX].__4);
				D(___196df8h) = (int)((double)(5*s_35e[MY_CAR_IDX].__19e)*(double)F32(esp+0x14)+(double)(s_35e[MY_CAR_IDX].__19a+0x28000));
				dRally_Sound_adjustEffect(1, 0x10000, D(___196df8h), 0x8000);
				race___54668h();	// POSITION, LAP COUNTER
				race___55ae0h();	// POSITION, LAP COUNTER

#if defined(DR_MULTIPLAYER)
				if((___19bd60h == 0)||(D(___243318h) == 0)){
#endif // DR_MULTIPLAYER
					race___56170h();
#if defined(DR_MULTIPLAYER)
				}

				if(___19bd60h == 0){
#endif // DR_MULTIPLAYER			
#if defined(DR3_CHAOS)
					/* regular FFA arena goes on without the player; Boss Arena (1v20 Swarm / 1v1 Giant Boss) ends once the player is wrecked */
					if(dr3_chaos_arena() && !dr3_boss_in_race()){ if(s_35e[MY_CAR_IDX].Finished != 0) D(___2438c8h)++; }
					else if(dr3_boss_in_race() && ((int)s_94[MY_CAR_IDX].__18 <= 0)){ D(___2438c8h) += 2; }
					else
#endif
					if((s_35e[MY_CAR_IDX].Finished != 0)||((int)s_94[MY_CAR_IDX].__18 <= 0)) D(___2438c8h)++;
#if defined(DR_MULTIPLAYER)
				}
#endif // DR_MULTIPLAYER		
		
				D(___243c60h) = 0;
				D(___243d0ch) = 0;

				if(NUM_OF_CARS > 0){

					while(1){

						if(s_35e[D(___243c60h)].__b0 <= 0.5f){

							if((s_35e[D(___243c60h)].Finished != 0)||((int)s_94[D(___243c60h)].__18 <= 0)) D(___243d0ch)++;
						}

						D(___243c60h)++;
						if((int)D(___243c60h) >= NUM_OF_CARS) break;
					}
				}

				eax = NUM_OF_CARS-1;
				if((int)eax <= (int)D(___243d0ch)) D(___2438c8h)++;
#if defined(DR3_CHAOS)
				/*
				 * The endgame of a deathmatch (platform_3ds/dr3_dm.h, drally.h: DR3_CHAOS_MODE_DM).  The
				 * count above is the engine's own "who is out" rule and it is not enough to end the mode: it
				 * wants a wreck to *stand still* (`s_35e[n].__b0 <= 0.5f`), so one stuck or unreachable car
				 * keeps the race running forever.  The port's own tick takes HP from every car that still has
				 * one once the sudden death started and starts the counter above as soon as only one car is
				 * left - the very wind down the player's own wreck uses.  Any other mode does nothing here
				 * (the switch is sdmc:/chaos_dm.txt, 0 switches it off).
				 */
				dr3_dm_tick();
				dr3_anniv_radio_tick();
#endif
				incCounter(4);
				decCounter(5);
				if((int)getCounter(5) <= 0) break;
			} // w
		}
	
#if defined(DR_MULTIPLAYER)
		if(___19bd60h == 0){
#endif // DR_MULTIPLAYER		
			if(s_94[0].__0 != 6) race___4c21ch();
#if defined(DR_MULTIPLAYER)
		}

		if((___19bd60h != 0)&&(D(___24387ch) != 0)) race___44304h();

#endif // DR_MULTIPLAYER
		if(kmap[DR_SCAN_F12] != 0) race___49390h();
		if(kmap[DR_SCAN_F1] != 0){

#if defined(DR_MULTIPLAYER)
			if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
				ecx = helper33();
#if DR3_NATIVE_WIDESCREEN
				__VGA13_SETMODE();
#endif
				race___48458h();
#if DR3_NATIVE_WIDESCREEN
				__RACE400_SETMODE();
#endif
				dRally_Sound_setPosition(ecx);
				dRally_Sound_setMasterVolume(0x10000);
				dRally_Sound_pushEffect(1, (s_94[MY_CAR_IDX].__0&0xff)+25, 0, 0x10000, 0x28000, 0x8000);
#if defined(DR_MULTIPLAYER)
			}
#endif // DR_MULTIPLAYER
		}

#if defined(__3DS__) && defined(DR3_USE_GFX)
		/* On 3DS, race standings and telemetry live on the bottom screen so the top screen stays fullscreen. */
		dRally_Race_unsetSettings(RACE_NOTWIDE);
#else
		if(!D(___243d00h)&&kmap[DR_SCAN_TAB]){
			
			D(___243d00h) = 1;
			dRally_Race_toggleSettings(RACE_NOTWIDE);
		}

		if(kmap[DR_SCAN_TAB] == 0) D(___243d00h) = 0;

		if((int)s_94[MY_CAR_IDX].__18 <= 0) dRally_Race_setSettings(RACE_NOTWIDE);
#endif

		if(kmap[DR_SCAN_F2] != 0){	// MSX

			dRally_Sound_setMusicVolume(dRally_Race_getSettings(RACE_MSX) ? 0 : ___24cc58h_msx_volume);
			dRally_Race_toggleSettings(RACE_MSX);
			kmap[DR_SCAN_F2] = 0;
		}

		if(kmap[DR_SCAN_F3] != 0){	// SFX

			dRally_Sound_setEffectsVolume(dRally_Race_getSettings(RACE_SFX) ? 0 : ___24cc54h_sfx_volume);
			dRally_Race_toggleSettings(RACE_SFX);
			kmap[DR_SCAN_F3] = 0;
		}

		if(kmap[DR_SCAN_F4] != 0){	// TEXTURES

			dRally_Race_toggleSettings(RACE_TEXTURES);
			kmap[DR_SCAN_F4] = 0;
		}

		if(kmap[DR_SCAN_F5] != 0){	// SHADOWS

			dRally_Race_toggleSettings(RACE_SHADOWS);
			kmap[DR_SCAN_F5] = 0;
		}

		if(kmap[DR_SCAN_P] != 0){	// GAME PAUSED
#if defined(DR_MULTIPLAYER)
			if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
				ecx = helper33();
				race_msg(E_GAME_PAUSED);
				race___478c8h(0);
				dRally_Sound_setPosition(ecx);
				dRally_Sound_setMasterVolume(0x10000);
				dRally_Sound_pushEffect(1, (s_94[MY_CAR_IDX].__0&0xff)+25, 0, 0x10000, 0x28000, 0x8000);
#if defined(DR_MULTIPLAYER)
			}
#endif // DR_MULTIPLAYER
		}

		dr3_chaos_time(DR3_CT_PRE, dr3_t_pre);

		frames = __GET_FRAME_COUNTER();

		{
			const unsigned long long dr3_t_frame = dr3_chaos_tick();

		{
			const unsigned long long dr3_t = dr3_chaos_tick();
			race___4ee9ch();
			race___4f030h();
			dr3_chaos_time(DR3_CT_SKIDS, dr3_t);
		}

		{
			const unsigned long long dr3_t = dr3_chaos_tick();
			ecx = CURRENT_VIEWPORT_W + ((CURRENT_VIEWPORT_W < DR3_RACE_W) ? 4 : 0);
			ecx -= ecx&3;
			m = -1;
			while(++m < DR3_RACE_H){	// COPY TRACK BACKGROUND


				memcpy(BACKBUFFER+0x200*m+CURRENT_VIEWPORT_X+0x60, TRX_IMA+TRX_WIDTH*(TRX_VIEWPORT_TL_Y+m)+TRX_VIEWPORT_TL_X, ecx);
			}
			dr3_chaos_time(DR3_CT_TRACKCOPY, dr3_t);
		}

#if defined(DR3_CHAOS)
		/*
		 * The first of the two band snapshots (see dr3_log.c): this is the road the pre car passes start
		 * from, so a pass that paints the rows above the car before the cars are drawn - the spectators,
		 * the tyre dust, the track shadows - is named in the log as well.  The second one is taken after
		 * the cars (dr3_chaos_band_start further down).
		 */
		dr3_chaos_band_start();
#endif

#if defined(DR_MULTIPLAYER)
		if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
			{
				const unsigned long long dr3_t = dr3_chaos_tick();
				if(!dr3_chaos_skip_pass(DR3_PASS_SPECTATORS)) race___52d7ch();
				dr3_chaos_time(DR3_CT_SPECT, dr3_t);
			}
#if defined(DR_MULTIPLAYER)
		}
#endif // DR_MULTIPLAYER
		dr3_chaos_band_mark("52d7ch spectators");
		D(___243c60h) = -1;
		{
			unsigned long long t3 = dr3_chaos_tick();
			while(++D(___243c60h) < dr3_chaos_extra_pass_limit(NUM_OF_CARS)){ dr3_chaos_step("50ef4h", (int)D(___243c60h)); if(!dr3_chaos_skip_pass(DR3_PASS_TYRE_DUST)) race___50ef4h(); }
			dr3_chaos_time(DR3_CT_DUST, t3);
		}
		dr3_chaos_band_mark("50ef4h tyre dust");
		dr3_chaos_step("4f300h", -1);
#if defined(DR3_CHAOS)
		/* is this one of the three frames per 30 degrees?  both dumps of the pair use this answer */
		const int dr3_dump = dr3_chaos_dump_due();

		/*
		 * The track's shadow polygons (race___4f170h, table VARJO_TAB) are drawn *before* the cars here.
		 * They used to come after them, and since the shadow table remaps whatever pixel it finds - the
		 * car's own pixels included - the edge of a nearby shadow brightened the car: light grey lines
		 * over the car that flickered while turning.  The WATCH log named the layer ("remap" wrote
		 * 98/9d/a0 into the rows above the sprite), and the report's "over the car, under the palm"
		 * matches this layer's place between the cars and the objects.  On the 3DS the shadows are an
		 * option; the DOS order is ground, shadows, cars, objects.
		 */
		{
			const unsigned long long dr3_t = dr3_chaos_tick();
			if(!dr3_chaos_skip_pass(DR3_PASS_TRACK_SHADOW) && dRally_Race_getSettings(RACE_SHADOWS)) race___4f170h();
			dr3_chaos_time(DR3_CT_SHADOW, dr3_t);
		}
		dr3_chaos_band_mark("4f170h track shadow");
		if(dr3_dump) dr3_chaos_dump_frame("after the shadows, before the cars");
#endif
		{
			unsigned long long t2 = dr3_chaos_tick();
			if(!dr3_chaos_skip_pass(DR3_PASS_CARS)) race___4f300h();	// RENDER CARS
			dr3_chaos_time(DR3_CT_CARS, t2);
		}
#if defined(DR3_CHAOS)
		/*
		 * From here on every pass is measured against what the cars left behind: the grey lines over
		 * the car are put there by one of them (dr3_log.c, dr3_chaos_band_mark).
		 */
		dr3_chaos_band_start();
#endif
		dr3_chaos_step("51ce0h", -1);
		{
			const unsigned long long dr3_t = dr3_chaos_tick();
			if(!dr3_chaos_skip_pass(DR3_PASS_EXPLOSIONS)) race___51ce0h();
			dr3_chaos_time(DR3_CT_EXPLOS, dr3_t);
		}
		dr3_chaos_band_mark("51ce0h explosions");
		dr3_chaos_step("4f170h", -1);
#if !defined(DR3_CHAOS)
		if(dRally_Race_getSettings(RACE_SHADOWS)) race___4f170h();
#endif
		D(___243c60h) = -1;
		{
			const unsigned long long dr3_t = dr3_chaos_tick();
			while(++D(___243c60h) < dr3_chaos_extra_pass_limit(NUM_OF_CARS)){ dr3_chaos_step("50a48h", (int)D(___243c60h)); if(!dr3_chaos_skip_pass(DR3_PASS_GUN_FIRE)) race___50a48h(); }
			dr3_chaos_band_mark("50a48h gun fire");
			D(___243c60h) = -1;
			while(++D(___243c60h) < dr3_chaos_extra_pass_limit(NUM_OF_CARS)){ dr3_chaos_step("51204h", (int)D(___243c60h)); if(!dr3_chaos_skip_pass(DR3_PASS_ROCKET)) race___51204h(); }
			dr3_chaos_band_mark("51204h rocket fuel");
			dr3_chaos_time(DR3_CT_WEAPONS, dr3_t);
		}
		dr3_chaos_step("53310h", -1);
		{
			const unsigned long long dr3_t = dr3_chaos_tick();
			race___53310h();	// SORT OBJECTS
			dr3_chaos_time(DR3_CT_OBJSORT, dr3_t);
		}
		dr3_chaos_step("53464h", -1);
		{
			const unsigned long long dr3_t = dr3_chaos_tick();
			if(!dr3_chaos_skip_pass(DR3_PASS_OBJECTS)) race___53464h();	// OBJECTS
			dr3_chaos_time(DR3_CT_OBJDRAW, dr3_t);
		}
		dr3_chaos_band_mark("53464h objects");
		D(___243c60h) = -1;
		{
			const unsigned long long dr3_t = dr3_chaos_tick();
			while(++D(___243c60h) < dr3_chaos_extra_pass_limit(NUM_OF_CARS)){ dr3_chaos_step("50ba4h", (int)D(___243c60h)); if(!dr3_chaos_skip_pass(DR3_PASS_SHOTS)) race___50ba4h(); }
			dr3_chaos_time(DR3_CT_WEAPONS, dr3_t);
		}
		dr3_chaos_band_mark("50ba4h shots");
		dr3_chaos_step("51eb4h", -1);
		{
			const unsigned long long dr3_t = dr3_chaos_tick();
			if(!dr3_chaos_skip_pass(DR3_PASS_CASH_TEXT)) race___51eb4h();
			dr3_chaos_band_mark("51eb4h cash text");
#if defined(DR_MULTIPLAYER)
		if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
			race___56594h();	// INRACE CHEATS
#if defined(DR_MULTIPLAYER)
		}
#endif // DR_MULTIPLAYER
		if((int)getCounter(1) < 0x122){ if(!dr3_chaos_skip_pass(DR3_PASS_BANNER)) race___42218h(); }
		dr3_chaos_band_mark("42218h banner");
		dr3_chaos_step("hud", -1);
		if(!dr3_chaos_skip_pass(DR3_PASS_HUD)) (CURRENT_VIEWPORT_W != DR3_RACE_W) ? race___40f48h() : race___40db4h();
		dr3_chaos_band_mark("hud");
		dr3_chaos_time(DR3_CT_HUD, dr3_t);
		}

#if defined(DR3_CHAOS)
		/* the second half of the pair the shadow pass opened (see dr3_chaos_dump_frame) */
		if(dr3_dump) dr3_chaos_dump_frame("at the end of the frame");
#endif

		D(___243c60h) = -1;
		while(++D(___243c60h) < NUM_OF_CARS){

			if((s_35e[D(___243c60h)].Position == 1)&&(D(___196dd4h) != 0)) race___4207ch();
		}

		if((int)D(___2438c8h) > 0x12c){

			/*
			 * A deathmatch may only end when one car is left (drally.h: DR3_CHAOS_MODE_DM) - this line says
			 * how many were still running when the race-over sequence started, which is the first thing to
			 * look at when a deathmatch ends too early.
			 */
			if(dr3_chaos_battle()){

				int dr3_dm_alive = 0, dr3_dm_n;

				for(dr3_dm_n = 0; dr3_dm_n < NUM_OF_CARS; ++dr3_dm_n)
					if((int)s_94[dr3_dm_n].__18 > 0) ++dr3_dm_alive;

				dr3_log("[dr3] chaos: deathmatch: race over with %d of %d car(s) still running",
				        dr3_dm_alive, NUM_OF_CARS);
			}

#if defined(DR_MULTIPLAYER)
			if(___19bd60h != 0) D(___24387ch) = 0;
#endif // DR_MULTIPLAYER
			helper_freeEffectChannels();
			dRally_Sound_pushEffect(2, SFX_RACE_OVER, 0, 0x10000, 0x50000, 0x8000);
			race_msg(E_RACE_OVER);
			race___478c8h(0);
			D(___243d14h) = 1;
		}

		/*
		D(___243d08h) = __GET_FRAME_COUNTER();
		while(D(___243d08h) == __GET_FRAME_COUNTER());
		*/
		{
			const unsigned long long dr3_t = dr3_chaos_tick();
			while (frames == __GET_FRAME_COUNTER());
			dr3_chaos_time(DR3_CT_WAIT, dr3_t);
		}

		{
			const unsigned long long dr3_t = dr3_chaos_tick();

			if((int)s_35e[MY_CAR_IDX].Drug > 0){

				race___454ach();
				if(D(___196dd8h) != 0) __VGA13_PRESENTSCREEN__();
			}
			else {

				{
					const unsigned long long dr3_t = dr3_chaos_tick();
					c = -1;
					while(++c < DR3_RACE_H) memcpy(VGA13_ACTIVESCREEN+DR3_RACE_W*c, BACKBUFFER+0x200*c+0x60, DR3_RACE_W);
					dr3_chaos_time(DR3_CT_COPY, dr3_t);
				}

#if defined(DR3_CHAOS)
				{
					const unsigned long long dr3_t = dr3_chaos_tick();
					/*
					 * The car mask travels with the image (dr3_chaos.h): the same rows and the same offset, so
					 * the mask the port colours from cannot be off by anything - it is the write pointer's own
					 * offset that was marked (race___4f300h.c).
					 */
					for(c = 0; c < DR3_RACE_H; ++c)
						memcpy(dr3_chaos_car_screen+DR3_RACE_W*c, dr3_chaos_car_mask+0x200*c+0x60, DR3_RACE_W);
					dr3_chaos_time(DR3_CT_MASKCOPY, dr3_t);
				}
#endif

				/*
				 * On the very first frame (D(___196dd8h) == 0), the palette is still all-zero black from
				 * race___45a24h(), and race___45d3ch() immediately follows below to clear and tilt the screen in.
				 * Skipping the present on that frame prevents a 1-frame flash of full-brightness colored cars
				 * floating on a pitch-black screen before the race intro animation starts.
				 */
				if(D(___196dd8h) != 0) __VGA13_PRESENTSCREEN__();
				s_35e[MY_CAR_IDX].Drug = 0;
			}

			dr3_chaos_time(DR3_CT_PRESENT, dr3_t);
		}

		dr3_chaos_time(DR3_CT_FRAME, dr3_t_frame);
		}

		D(___243c60h) = 0;
		eax = -1;
		while(++eax < NUM_OF_CARS){

			if((int)s_35e[eax].Drug > 0) s_35e[eax].Drug -= D(___243334h);
		}

		D(___243c60h) = NUM_OF_CARS;

		if(dRally_Race_getSettings(RACE_NOTWIDE)){

			if(CURRENT_VIEWPORT_W > (DR3_RACE_W - 0x40)) CURRENT_VIEWPORT_W -= 4*D(___243334h);
			if(CURRENT_VIEWPORT_W < (DR3_RACE_W - 0x40)) CURRENT_VIEWPORT_W = (DR3_RACE_W - 0x40);
		}
		else {

			if(CURRENT_VIEWPORT_W < DR3_RACE_W) CURRENT_VIEWPORT_W += 2*D(___243334h);
			if(CURRENT_VIEWPORT_W > DR3_RACE_W) CURRENT_VIEWPORT_W = DR3_RACE_W;
		}

		CURRENT_VIEWPORT_CENTER_X = CURRENT_VIEWPORT_W/2;
		CURRENT_VIEWPORT_X = DR3_RACE_W-CURRENT_VIEWPORT_W;

#if defined(DR_MULTIPLAYER)
		if(	(!___19bd60h&&kmap[DR_SCAN_ESCAPE])
		||	(___19bd60h&&kmap[DR_SCAN_ESCAPE]&&((int)getCounter(1) > 0xc8))
		||	(___19bd60h&&D(___243cd8h)&&((int)getCounter(1) > 0xc8))
		){
#else
		if(kmap[DR_SCAN_ESCAPE]){
#endif // DR_MULTPLAYER

#if defined(DR_MULTIPLAYER)
			if(___19bd60h != 0){

				D(___24387ch) = 0;
				if(D(___243cd8h) == 0) ___61418h(0xfd);
			}
#endif // DR_MULTIPLAYER

			D(___243d14h) = -1;
			helper_freeEffectChannels();

#if defined(DR_MULTIPLAYER)
			if((___19bd60h != 0)&&(D(___243cd8h) != 0)){

				race_msg(E_ABOUT_TO_ABORT);
				race___47ed8h(1);
			}
			else {
#endif // DR_MULTIPLAYER
				race_msg(E_ABORT_RACE);
#if defined(DR_MULTIPLAYER)
			}

			if((___19bd60h != 0)&&(D(___243cd8h) == 0)) race___47ed8h(0);
			if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
				race___478c8h(0);
#if defined(DR_MULTIPLAYER)
			}
#endif // DR_MULTIPLAYER

			if(D(___243d14h) != 1){

				D(___243d14h) = 0;
				dRally_Sound_pushEffect(1, (s_94[MY_CAR_IDX].__0&0xff)+25, 0, 0x10000, 0x28000, 0x8000);
			}
			else {
#if defined(DR_MULTIPLAYER)
				if(___19bd60h == 0){
#endif // DR_MULTIPLAYER		
					race___560d8h();
#if defined(DR_MULTIPLAYER)
				}
#endif // DR_MULTIPLAYER
			}

#if defined(DR_MULTIPLAYER)
			if((___19bd60h != 0)&&(D(___243d14h) == 0)){
				
				D(___24387ch) = 1;
				D(___243cd8h) = 0;
			}
#endif // DR_MULTIPLAYER
		}









#if defined(DR_MULTIPLAYER)
		if((___19bd60h != 0)&&(D(___243318h) == 1)){

			D(___24387ch) = 0;
			___61418h(0xfc);
			D(___243d14h) = -1;
			race_msg(E_CONNECTION_LOST);
			race___47ed8h(0);
			D(___243d14h) = 1;
			s_54[0].__44 = -1;
		}
#endif // DR_MULTIPLAYER

		D(___196dd8h)++;
		if(D(___196dd8h) == 1){

			race___45d3ch();	// ENTERING RACE SCREEN ANIMATION

#if defined(DR_MULTIPLAYER)
			if(___19bd60h == 0){
#endif // DR_MULTIPLAYER

				if(___243d44h != 0){

					race_msg(E_WELCOME);
					race___478c8h(0);
				}
#if defined(DR_MULTIPLAYER)
			}
#endif // DR_MULTIPLAYER
		}

		if(D(___243d14h) != 0) break;
	} // w

	race___46a10h();	// LEAVING RACE SCREEN ANIMATION
	race___46738h__VGA13_PRESENTCLEARSCREEN();

#if defined(DR3_CHAOS)
	/* the arena: the place of a car is who outlived whom, not the lap counter (platform_3ds/dr3_arena.h) */
	if(dr3_chaos_arena()) dr3_arena_final_places();
#endif

	n = -1;
	while(++n < NUM_OF_CARS){
		s_94[n].__54 = s_35e[n].Position;
#if defined(DR3_CHAOS)
		if(dr3_log_enabled()){
			dr3_log("[dr3] chaos: finish car %2d %-11s: pos=%2d lap=%d/%d chk=%2d fin=%d hp=%d%s",
			        n, (const char *)(___1de7d0h + 0x54 * n), (int)s_35e[n].Position,
			        (int)s_35e[n].Lap, NUM_OF_LAPS, (int)s_35e[n].__0,
			        (int)s_35e[n].Finished, (int)s_94[n].__18,
			        (n == (int)MY_CAR_IDX) ? " [PLAYER]" : "");
		}
#endif
	}

	___60719h();
	dRally_Sound_stop();
	dRally_Sound_release();
	race___400ech();
	race_free();
	kmap[DR_SCAN_ESCAPE] = 0;
#if defined(__3DS__)
	dr3_scenario_event("race_over");
#endif
}
