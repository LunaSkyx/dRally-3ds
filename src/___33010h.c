#include "drally.h"
#include "platform_3ds/dr3_chaos.h"
#include "platform_3ds/dr3_payout.h"   /* the rank money ladder of a chaos race (dr3_chaos_rank_money) */
#include "platform_3ds/dr3_cash.h"     /* street cash ($) unit value & payout (kept even when lapped)  */
#include "platform_3ds/dr3_arena.h"    /* the arena: its track, its field, the prizes of the top three */
#include "platform_3ds/dr3_host.h"   /* the PC harness: the race length of a run ("laps <n>") */
#if defined(DR3_CHAOS)
#include "platform_3ds/dr3_grid.h"      /* dr3_grid_order_by_rank - the grid order the standings give */
#include "platform_3ds/dr3_dm.h"        /* the sudden death of a deathmatch: its switch, read per race */
#include "platform_3ds/dr3_boss.h"      /* 30th Anniversary 3-phase final boss gauntlet */
#include "platform_3ds/dr3_anniv_prog.h" /* 30th Anniversary Rising Chaos progression & in-race radio */
#endif
#include "drmemory.h"
#include "drally_structs_fixed.h"
#include "bpa.h"

#define COOXY(x,y) (0x280*(y)+(x))

	const char ___106cbh[19][4] = { 
		"TR1","TR2","TR3","TR4","TR5","TR6","TR7","TR8","TR9",
		"TR1","TR2","TR3","TR4","TR5","TR6","TR7","TR8","TR9",
		"TR0" };

	__DWORD__ ___10718h[4] = { COOXY( 51, 177), COOXY(163, 216), COOXY( 51, 274), COOXY(163, 313) };
	__DWORD__ ___10728h[4] = { COOXY( 24, 201), COOXY(136, 240), COOXY( 24, 298), COOXY(136, 337) };
	__DWORD__ ___10738h[4] = { COOXY( 53, 129), COOXY(165, 168), COOXY( 53, 226), COOXY(165, 265) };

	extern __BYTE__ ___196a74h[];
	extern __POINTER__ ___1a112ch__VESA101_ACTIVESCREEN_PTR;
	extern __POINTER__ ___1a10e4h__VESA101h_DefaultScreenBufferA;
	extern __POINTER__ ___1a1138h__VESA101_BACKGROUND;
	extern __POINTER__ ___1a1100h__VESA101h_DefaultScreenBuffer;
	extern __POINTER__ ___1a0f8ch;
	extern __POINTER__ ___1a0f94h;
	extern __BYTE__ ___1a1ef8h[];
	extern __POINTER__ ___1a0f9ch;
	extern __BYTE__ ___1a2010h[];
	extern __BYTE__ ___1a54d0h[];
	extern __POINTER__ ___1a1114h;
	extern int NUM_OF_LAPS;
extern int NUM_OF_CARS;      /* the cars of the race (race_main.c: the grid plus the chaos extras) */
	extern __BYTE__ ___1de7d0h[];
	extern __BYTE__ ___1a1028h[];
	extern __BYTE__ ___1a1f64h[];
	extern char ___19bd64h[16];
	extern __BYTE__ ___1a1028h[];
	extern __BYTE__ ___1a0ef8h[];
	extern __BYTE__ ___185c7ah[];
	extern __POINTER__ ___1a10b8h;
#if defined(DR_MULTIPLAYER)
	extern __DWORD__ ___19bd60h;
	extern __DWORD__ MP_MONEY;
#endif // DR_MULTIPLAYER
	extern __POINTER__ ___1a0fb4h;
	extern __BYTE__ ___1a01e0h[];
	extern __POINTER__ ___19de70h[20];
	extern __BYTE__ ___196a84h[];
	extern __BYTE__ ___185a34h[];
	extern __BYTE__ ___1a1168h[];
	extern __BYTE__ ___196adch[];
	extern int LAP_BEST_SEC;
	extern int LAP_BEST_MIN;
	extern int LAP_PREVIOUS_100;
	extern int LAP_PREVIOUS_SEC;
	extern int LAP_PREVIOUS_MIN;
	extern __BYTE__ ___1a1090h[];
	extern __BYTE__ ___1a10a8h[];
	extern __BYTE__ ___1a10a0h[];
	extern __BYTE__ ___196ab0h[];
/* the cars of the running race (s_94, 0x94 bytes each) - the HP of a car sits at +0x18 (drally.h: the modes) */
extern __BYTE__ ___1de580h[];
	extern int LAP_BEST_100;
	extern __BYTE__ ___196aa4h[];
	extern __BYTE__ ___1a1ef8h[];
	extern __DWORD__ ___196a94h_difficulty;
	extern __BYTE__ ___185a14h_UseWeapons[];
	extern __POINTER__ ___1a0fb8h;
	extern __POINTER__ ___1a0fe0h;
	extern record_t ___19f750h[6][18];
	extern int LAP_RECORD_MIN;
	extern int LAP_RECORD_SEC;
	extern int LAP_RECORD_100;
	extern __BYTE__ ___1a103ch[];
	extern __DWORD__ CONNECTION_TYPE;
	extern __BYTE__ ___1a1ef8h[];
	extern __BYTE__ ___185a18h[];
	extern __BYTE__ ___196ae8h[];
	extern __BYTE__ ___196aa0h[];
	extern __BYTE__ ___196a9ch[];
	extern __BYTE__ ___196a98h[];
	extern __BYTE__ ___196aa8h[];
	extern __BYTE__ ___1a1028h[];
	extern __BYTE__ ___196aach[];
	extern __DWORD__ ___243d44h;
	extern __BYTE__ ___196af0h[];
	extern __BYTE__ ___196aech[];
	extern __BYTE__ ___196ab8h[];
	extern __BYTE__ ___196ad4h[];
	extern __BYTE__ ___196a80h[];
	extern __BYTE__ ___1a1ef8h[];
	extern __BYTE__ ___1a2010h[];
	extern __BYTE__ ___18d492h[][0xf];
	extern __DWORD__ ___24cc58h_msx_volume;
	extern __BYTE__ ___185a20h[];

void dRChatbox_push(const char * line, int col);
__POINTER__ ___3f71ch__allocateMemory(__DWORD__ size);
void old_bpa_read(const char *, __POINTER__, const char *);
void bpk_decode2(__POINTER__, __POINTER__);
void ___12e78h_cdecl(__BYTE__ *, __BYTE__ *, const char *, __DWORD__);
char * itoa_watcom106(int value, char * buffer, int radix);
char * strupr_watcom106(char * s);
int rand_watcom106(void);
__DWORD__ ___251e8h_cdecl(const char *);
void ___3a968h_cdecl(__DWORD__);
__BYTE__ dRally_Sound_setPosition(__DWORD__ pos_n);
void dRally_Sound_setMasterVolume(__DWORD__ vol);
void ___12cb8h__VESA101_PRESENTSCREEN(void);
void __WAIT_5(void);
void ___60a84h(void);
__DWORD__ ___60b60h(__DWORD__, __DWORD__, __POINTER__, __DWORD__);
void ___2ab50h(void);
__DWORD__ ___611c0h(__DWORD__ A1, __POINTER__ * A2);
void ___3881ch(void);
void ___24ec0h(void);
void ___2fc50h(void);
void ___12200h(void);
void ___12a54h(void);
void ___60719h(void);
void race_main(int MyIndex, int NumCars);
void ___60705h(void (*A)(void));
#if defined(DR_MULTIPLAYER)
void ___38878h_cdecl(void);
void ___10754h(void);
void ___3986ch(void);
void ___1e4f8h(void);
void ___23488h_cdecl(__POINTER__, __BYTE__, __BYTE__);
#endif // DR_MULTIPLAYER
void ___2ec68h_cdecl(void);
void ___11378h_cdecl(__DWORD__ A1, __DWORD__ A2, __DWORD__ A3);
void dRally_Sound_load(__DWORD__ msx_t, const char * msx_f, __DWORD__ sfx_t, const char * sfx_f, __DWORD__ num_ch);
void dRally_Sound_setMusicVolume(__DWORD__ vol);
void dRally_Sound_setSampleRate(__DWORD__ freq);
void dRally_Sound_play(void);
void ___606dfh(void);
void __VESA101_SETMODE(void);
void ___605deh_cdecl(__DWORD__, __DWORD__);
void ___12940h(void);
void __DISPLAY_SET_PALETTE_COLOR(__DWORD__ b, __DWORD__ g, __DWORD__ r, __DWORD__ n);
void ___31588h(void);
void ___3892ch_cdecl(__DWORD__);
void ___1240ch(void);
void ___24548h(void);
void ___2faf0h(void);
void ___22808h(void);
void ___3deb8h(void);
void ___117d4h(void);
void ___117f4h(void);
void ___3079ch_cdecl(__DWORD__);

typedef char char9[9];

static const char9 ___196b2ch[19] = {
	[0] = "TSHAPE01",
	[1] = "TSHAPE02",
	[2] = "TSHAPE03",
	[3] = "TSHAPE04",
	[4] = "TSHAPE05",
	[5] = "TSHAPE06",
	[6] = "TSHAPE07",
	[7] = "TSHAPE08",
	[8] = "TSHAPE09",
	[9] = "TSHAPE10",
	[10] = "TSHAPE11",
	[11] = "TSHAPE12", 
	[12] = "TSHAPE13",
	[13] = "TSHAPE14",
	[14] = "TSHAPE15",
	[15] = "TSHAPE16",
	[16] = "TSHAPE17",
	[17] = "TSHAPE18",
	[18] = "TSHAPE19"
};

#if defined(DR3_CHAOS)
/*
 * The racers of this race, in the order the cars are numbered: the NumCars seats of the event first (esp+0xcc
 * holds their startername bytes, and the duel has already cut them down to its two) and then the extras the
 * field setup adds - the free racers in roster order, up to dr3_chaos_extra_cars() of them and none in a duel.
 *
 * One place for that rule, read by both of the things that need it: the field setup itself and the roster
 * screen, which names the four leaders of the field ("PREPARE TO RACE").  The screen has four framed columns
 * with the numbers 1..4 in them and the race lines up in the order of the standings (race___49bd4h.c hands
 * the start slots out in it), so the leaders are the cars of the first four places - a screen that read the
 * four seats of the event instead showed four drivers out of twenty.
 *
 * seats is esp+0xcc, out takes DR3_MAX_CARS racers.  Returns how many drive (NumCars .. DR3_MAX_CARS).
 */
static int dr3_chaos_field(__BYTE__ * seats, int num_cars, int * out)
{
	const racer_t * s_6c = (const racer_t *)___1a01e0h;
	int taken[0x14];
	int n, r, e = 0;

	if(num_cars > DR3_MAX_CARS) num_cars = DR3_MAX_CARS;

	dr3_anniv_set_active_field(0);

	/*
	 * 30th Anniversary "Rising Chaos" progression: match opponents by car speed/class and grow
	 * the grid from 8 -> 12 -> 16 -> 20 cars (capped at 16 in Arena) across Standard, Destroy 'Em
	 * All, and Arena so a Vagabond isn't immediately lapped and shredded by Deliverators.
	 */
	if(dr3_chaos_active() && (D(___1a1028h) != 3) && (dr3_chaos_mode() != DR3_CHAOS_MODE_DUEL)
#if defined(DR_MULTIPLAYER)
	   && (___19bd60h == 0)
#endif
	  ){
		const int me    = (int)D(___1a1ef8h);
		int       pslot = (int)D(___1a103ch);
		int       tier  = (me >= 0 && me < 20) ? dr3_anniv_tier((int)s_6c[me].car, (int)s_6c[me].rank) : 0;
		int       dr3_extras = dr3_chaos_arena() ? dr3_arena_extras(num_cars, dr3_chaos_extra_cars())
		                                         : dr3_chaos_extra_cars();
		int       target = dr3_chaos_extra_cars_override()
		                 ? (num_cars + dr3_extras)
		                 : dr3_anniv_field_size_ex(tier, dr3_chaos_arena(), dr3_extreme_active());
		int       cars[20], ranks[20], matched[20];
		int       m_cnt, m_idx = 1;

		if(dr3_chaos_arena() && target > DR3_ARENA_CARS_MAX) target = DR3_ARENA_CARS_MAX;
		if(target > DR3_CHAOS_RACERS) target = DR3_CHAOS_RACERS;
		if(target < num_cars) target = num_cars;
		if(pslot < 0 || pslot >= num_cars) pslot = 0;

		for(r = 0; r < 20; ++r){
			cars[r]  = (int)s_6c[r].car;
			ranks[r] = (int)s_6c[r].rank;
		}

		m_cnt = dr3_anniv_select_field(me, target, cars, ranks, 20, matched);

		for(n = 0; n < m_cnt; ++n){
			int seat_pick;
			if(n == pslot){
				seat_pick = matched[0];
			} else {
				seat_pick = (m_idx < m_cnt) ? matched[m_idx++] : matched[0];
			}
			out[n] = seat_pick;
			if(n < num_cars) seats[n] = (__BYTE__)seat_pick;
		}

		dr3_anniv_set_active_field(m_cnt);
		dr3_log("[dr3] anniv%s: tier %d (car %d, rank %d) -> %d car(s), AI pace row %d (override=%d)",
		        dr3_extreme_active() ? " EXTREME" : "",
		        tier, (me >= 0 && me < 20) ? (int)s_6c[me].car : 0,
		        (me >= 0 && me < 20) ? (int)s_6c[me].rank : 20,
		        m_cnt, dr3_anniv_ai_difficulty_row_ex(tier, dr3_extreme_active()), dr3_chaos_extra_cars_override());
		return m_cnt;
	}

	for(n = 0; n < 0x14; ++n) taken[n] = 0;

	for(n = 0; n < num_cars; ++n){

		out[n] = (int)seats[n];
		if((out[n] >= 0) && (out[n] < 0x14)) taken[out[n]] = 1;
		e = n + 1;
	}

	const int dr3_extras = (D(___1a1028h) == 3) ? 0
	                     : dr3_chaos_arena() ? dr3_arena_extras(num_cars, dr3_chaos_extra_cars())
	                     : dr3_chaos_extra_cars();

	if((dr3_extras > 0) && (dr3_chaos_mode() != DR3_CHAOS_MODE_DUEL)){

		const int want = dr3_extras;

		taken[(int)D(___1a1ef8h)] = 1;                 /* the player's own seat is one of the seats above */

		for(r = 0; (r < 0x14) && (e < DR3_CHAOS_RACERS) && (e - num_cars < want); ++r){

			if(taken[r]) continue;

			out[e++] = r;
			taken[r] = 1;
		}
	}

	return e;
}
#endif

// PREPARE TO RACE
void ___33010h_cdecl(int NumCars){

	__DWORD__ 	eax, ebx, ecx, edx, edi, esi, ebp;
	__BYTE__ 	esp[0xfc];
	int 	i, j, m, n, bool_tmp;
	__BYTE__ 	px;
	__POINTER__ 	ebxp;
	__POINTER__	esip;
	__POINTER__ 	ecxp;
	__POINTER__ 	eaxp;
	char 	buffer[0x96];
	__POINTER__ esp0x98[4];
	racer_t * 	s_6c;

#if defined(DR3_CHAOS)
	/*
	 * The chaos build's race field and the rows of its roster screen (drally.h: DR3_ROSTER_ROWS).  Both are
	 * filled below, right after the duel has cut the seats down to its two cars: the screen draws its
	 * portraits out of dr3_roster and the race drives dr3_field.
	 */
	int dr3_field[DR3_MAX_CARS];        /* the racers of this race, in the order the cars are numbered */
	int dr3_field_n;
	int dr3_roster[DR3_ROSTER_ROWS];    /* the rows of the roster screen: the leaders of the field     */
	int dr3_boss_idx = -1;              /* car index of the Chaos Adversary invader, or -1 if absent   */
#endif


	s_6c = (racer_t *)___1a01e0h;
	memcpy(esp, ___106cbh, 4*19);
	memcpy(esp+0x88, ___10718h, 0x10);
	memcpy(esp+0xa8, ___10728h, 0x10);
	D(___196a74h) = 0;
	memcpy(esp+0xb8, ___10738h, 0x10);
	memcpy(___1a10e4h__VESA101h_DefaultScreenBufferA, ___1a112ch__VESA101_ACTIVESCREEN_PTR, 0x4b000);
	___1a112ch__VESA101_ACTIVESCREEN_PTR = ___1a10e4h__VESA101h_DefaultScreenBufferA;
	memcpy(___1a10e4h__VESA101h_DefaultScreenBufferA, ___1a1138h__VESA101_BACKGROUND, 0x4b000);

/* PREPARE TO RACE BANNER ********************************************************************** */
	j = -1;
	while(++j < 0x36){

		i = -1;
		while(++i < 0x280){

			if((px = read_b(___1a0f8ch+0x280*j+i)) != 0) write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x42900+0x280*j+i, px);
		}
	}
/* ********************************************************************** PREPARE TO RACE BANNER */
/* ROSTER BACKGROUND *************************************************************************** */
	j = -1;
	while(++j < 0x112){

		i = -1;
		while(++i < 0xe1){

			if((px = read_b(___1a0f94h+0xe1*j+i)) != 0) write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0xfa0d+0x280*j+i, px);
		}
	}
/* *************************************************************************** ROSTER BACKGROUND */

	if(NumCars < 4){

		j = -1;
		while(++j < 0x5b){

			i = j%2;
			while(i < 0x64){

				write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x280*j+i+0x28d03, 0xc4);
				i += 2;
			}
		}
	}

	if(NumCars < 3){

		j = -1;
		while(++j < 0x5b){

			i = j%2;
			while(i < 0x64){

				write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x280*j+i+0x22b13, 0xc4);
				i += 2;
			}
		}
	}

#if defined(DR_MULTIPLAYER)
	if(___19bd60h == 0){
#endif // DR_MULTIPLAYER

		D(___1de7d0h+0x48) = !!(B(___1a1f64h+D(___1a1028h)) > 8);

		switch(D(___1a1028h)){
		case 0:
			old_bpa_read("MENU.BPA", ___1a54d0h, strcat(strcpy(esp+0x4c, ___196b2ch[B(___1a1f64h)]), ".bpk"));
			bpk_decode2(___1a1114h, ___1a54d0h);

			j = -1;
			while(++j < 0x112){

				i = -1;
				while(++i < 0x168){

					if((px = read_b(___1a1114h+0x168*j+i)) != 0) write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0xfb08+0x280*j+i, px);
				}
			}

			strcpy(___19bd64h, esp+4*B(___1a1f64h));
			NUM_OF_LAPS = 4;
			break;
		case 1:
			old_bpa_read("MENU.BPA", ___1a54d0h, strcat(strcpy(esp+0x4c, ___196b2ch[B(___1a1f64h+1)]), ".bpk"));
			bpk_decode2(___1a1114h, ___1a54d0h);

			j = -1;
			while(++j < 0x112){

				i = -1;
				while(++i < 0x168){

					if((px = read_b(___1a1114h+0x168*j+i)) != 0) write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0xfb08+0x280*j+i, px);
				}
			}

			strcpy(___19bd64h, esp+4*B(___1a1f64h+1));
			NUM_OF_LAPS = 5;
			break;
		case 2:
			old_bpa_read("MENU.BPA", ___1a54d0h, strcat(strcpy(esp+0x4c, ___196b2ch[B(___1a1f64h+2)]), ".bpk"));
			bpk_decode2(___1a1114h, ___1a54d0h);

			j = -1;
			while(++j < 0x112){

				i = -1;
				while(++i < 0x168){

					if((px = read_b(___1a1114h+0x168*j+i)) != 0) write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0xfb08+0x280*j+i, px);
				}
			}

			strcpy(___19bd64h, esp+4*B(___1a1f64h+2));
			NUM_OF_LAPS = 6;
			break;
		case 3:
			old_bpa_read("MENU.BPA", ___1a54d0h, "TSHAPE19.BPK");
			bpk_decode2(___1a1114h, ___1a54d0h);

			j = -1;
			while(++j < 0x112){

				i = -1;
				while(++i < 0x168){

					if((px = read_b(___1a1114h+0x168*j+i)) != 0) write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0xfb08+0x280*j+i, px);
				}
			}

			strcpy(___19bd64h, "TR0");
			NUM_OF_LAPS = 9;
			D(___1de7d0h+0x48) = 0;
			break;
		default:
			break;
		}

		edx = D(___1a1028h);
		B(esp+0xcc) = B(edx*4+___1a0ef8h);
		B(esp+0xcd) = B(edx*4+___1a0ef8h+1);
		B(esp+0xce) = B(edx*4+___1a0ef8h+2);
		B(esp+0xcf) = B(edx*4+___1a0ef8h+3);

		if(edx == 3){
			
			B(esp+0xcc) = D(___1a1ef8h);
			B(esp+0xcd) = D(___1a1ef8h);
		}
#if defined(DR_MULTIPLAYER)
	}
	else {

		B(esp+0xcc) = 0;
		B(esp+0xce) = 2;
		B(esp+0xcd) = 1;
		B(esp+0xcf) = 3;
		___1a0f9ch = ___3f71ch__allocateMemory(4);
		B(___1a0f9ch) = D(___1a1ef8h);
		___23488h_cdecl(___1a0f9ch, 1, 0x13);
		dRMemory_free(___1a0f9ch);
		edx = B(___1a2010h+1+D(___196ad4h));
		strcpy(esp+0x4c, ___196b2ch[edx]);
		strcat(esp+0x4c, ".bpk");
		old_bpa_read("MENU.BPA", ___1a54d0h, esp+0x4c);
		bpk_decode2(___1a1114h, ___1a54d0h);

		j = -1;
			while(++j < 0x112){

				i = -1;
				while(++i < 0x168){

					if((px = B(___1a1114h+0x168*j+i)) != 0) B(___1a112ch__VESA101_ACTIVESCREEN_PTR+0xfb08+0x280*j+i) = px;
				}
			}

		eax = B(D(___196ad4h)+___1a2010h+1);
		strcpy(___19bd64h, esp+4*eax);
		L(edx) = B(D(___196ad4h)+___1a2010h+1);
		NUM_OF_LAPS = 5;

		if(L(edx) <= 8){

			D(___1de7d0h+0x48) = 0;
		}
		else {

			D(___1de7d0h+0x48) = 1;
		}
	}
#endif // DR_MULTIPLAYER

#if defined(DR3_CHAOS)
	/*
	 * The switches of the chaos build are read here, at the top of a race setup - the race *mode* first of
	 * all, because the duel below cuts the field down to two before anything reads it: the roster screen
	 * draws NumCars portraits out of the four seat bytes at esp+0xcc and the car table (both above) is
	 * filled from those same seats, so the screen and the race show the same two drivers.  Read behind the
	 * roster screen and the car table instead - where these calls sat before - the duel changed only what
	 * the *race* was, while the screen and the field kept driving the four entries of the event: drivers
	 * that never raced (see doc/3ds.md).
	 */
	const int dr3_rolled = dr3_chaos_mode();      /* what the wheel of the sign up screen rolled */

	dr3_chaos_extra_cars_reload();      /* sdmc:/chaos_extras.txt, if there is one */
	dr3_chaos_mode_reload();            /* sdmc:/chaos_mode.txt - the race mode (drally.h) */
	dr3_dm_sudden_reload();             /* sdmc:/chaos_dm.txt - the sudden death of a deathmatch */
	dr3_dm_lap_reload();                /* sdmc:/chaos_dm_lap.txt - its lap duty (against camping) */
	dr3_chaos_boss_reload();            /* sdmc:/chaos_boss.txt - % chance Adversary invades Chaos race */
	dr3_chaos_raced_set();              /* from now on the wheel of the sign up screen is armed (___3266ch.c) */
	dr3_log_quiet_reload();             /* sdmc:/chaos_log.txt - 0 silences the whole log */

	/*
	 * The reload above sets the *setting* and drops a roll with it.  What the wheel rolled is this race - the
	 * sign up screen draws it - so it is put back when the two differ; the wheel rolls again on the next
	 * visit to that screen and replaces it (dr3_log.c).
	 *
	 * Exception: D(___1a1028h) == 3 is the championship Final Challenge against the Adversary on TR0 (2 cars).
	 * It must never be hijacked by Arena / Duel / DM or extra cars from an earlier wheel roll.
	 */
	if (D(___1a1028h) == 3) {
		if (dr3_boss_in_race()) {
			dr3_boss_race_reset();
			dr3_chaos_mode_commit(dr3_boss_is_arena_phase() ? DR3_CHAOS_MODE_ARENA : DR3_CHAOS_MODE_STANDARD);
		}
		else {
			dr3_chaos_mode_commit(DR3_CHAOS_MODE_STANDARD);
		}
	}
	else if (dr3_rolled != dr3_chaos_mode_setting()) dr3_chaos_mode_commit(dr3_rolled);

	/*
	 * A new race: the cars of the last one scored their points and the health line over them has not seen
	 * any car of this race (dr3_chaos.h).
	 */
	dr3_chaos_race_reset();

	dr3_log("[dr3] chaos: race setup with %d extra car(s), ladder stage %d (chaos_extras.txt / the script; "
	        "in game L+R+X is the pass switch, see dr3_chaos.h)",
	        dr3_chaos_extra_cars(), dr3_chaos_extra_stage());
	dr3_log("[dr3] chaos: race mode %d (%s) - %s",
	        dr3_chaos_mode(), dr3_chaos_mode_name(dr3_chaos_mode()),
	        (dr3_chaos_mode() == DR3_CHAOS_MODE_DUEL) ? "a duel against one rival of the standings"
	        : (dr3_chaos_mode() == DR3_CHAOS_MODE_DM) ? "destroy 'em all - only the last car standing wins"
        : (dr3_chaos_mode() == DR3_CHAOS_MODE_ARENA) ? "the arena - everybody against everybody, the top three survivors are paid"
	                                                  : "the whole standings on one grid");

	/*
	 * The arena (platform_3ds/dr3_arena.h) is not one of the three races of the event: whichever one was signed
	 * up for, the cars drive onto TRA - and the original's own Arena (TR0) is the model: its track name, no
	 * reversed track (the flag is read from the +0x48 of the first car table row when the race starts).
	 */
	if(dr3_chaos_arena()){

		strcpy(___19bd64h, DR3_ARENA_TRACK);
		NUM_OF_LAPS = 1;
		D(___1de7d0h+0x48) = 0;

		/* replace the signed-up track's loading screen map with the arena's own 360x274 preview (TRA-SHP.BPK in TRA.BPA) */
		{
			BPA * dr3_bpa = bpa_open("TRA.BPA");
			int   dr3_got_shape = 0;

			if (dr3_bpa) {
				bpa_search(dr3_bpa, "TRA-SHP.BPK");
				if (bpa_entry_size(dr3_bpa) != 0) {
					bpa_read(dr3_bpa, ___1a54d0h);
					dr3_got_shape = 1;
				}
				bpa_close(dr3_bpa);
			}

			if (!dr3_got_shape) old_bpa_read("MENU.BPA", ___1a54d0h, "TSHAPE19.BPK");
			bpk_decode2(___1a1114h, ___1a54d0h);

			j = -1;
			while(++j < 0x112){

				i = -1;
				while(++i < 0x168){

					if((px = read_b(___1a1114h+0x168*j+i)) != 0) write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0xfb08+0x280*j+i, px);
				}
			}

			dr3_log("[dr3] chaos: arena: loading screen map drawn from %s", dr3_got_shape ? "TRA.BPA (TRA-SHP.BPK)" : "MENU.BPA (TSHAPE19.BPK fallback)");
		}

		dr3_log("[dr3] chaos: arena: the race runs on %s.BPA, a field of %d car(s) at most (%d to a corner), "
		        "prizes $%d / $%d / $%d for the top three", DR3_ARENA_TRACK, DR3_ARENA_CARS, DR3_ARENA_CARS / DR3_ARENA_CORNERS,
		        DR3_ARENA_PRIZE_1, DR3_ARENA_PRIZE_2, DR3_ARENA_PRIZE_3);
	}

	if (dr3_boss_is_escort_race()) {
		strcpy(___19bd64h, "TR0");
		NUM_OF_LAPS = DR3_BOSS_P2_LAPS;
		D(___1de7d0h+0x48) = 0;
		dr3_log("[dr3] boss: phase 2 escort race on TR0, %d laps, %d cars",
		        DR3_BOSS_P2_LAPS, DR3_BOSS_P2_CARS);
	}

	/*
	 * The endgame of a deathmatch (platform_3ds/dr3_dm.h): the deadline of its sudden death.  Logged with
	 * the mode, because a run is read for exactly this line - a race without it is the mode's old
	 * "wait for the last car standing" behaviour.
	 */
	if(dr3_chaos_mode() == DR3_CHAOS_MODE_DM)
		dr3_log("[dr3] chaos: deathmatch: the sudden death starts after %d second(s), %d percent of the HP of "
		        "every car a second (%d HP each) - sdmc:/chaos_dm.txt, 0 switches it off",
		        dr3_dm_sudden_s(), DR3_DM_SUDDEN_PERCENT_S, DR3_DM_HP_PER_PERCENT);

	/*
	 * ... and its lap duty: standing still is the winning move of a pure sudden death (everybody loses the
	 * same HP a second), so a car has to keep lapping or it loses HP at every check (platform_3ds/dr3_dm.h).
	 */
	if(dr3_chaos_mode() == DR3_CHAOS_MODE_DM)
		dr3_log("[dr3] chaos: deathmatch: a car has to finish %d lap(s) every %d second(s) or it loses %d "
		        "percent of its HP (%d HP) at that check - sdmc:/chaos_dm_lap.txt, 0 switches it off",
		        DR3_DM_LAP_PER_INTERVAL, dr3_dm_lap_s(), DR3_DM_LAP_PENALTY_PERCENT,
		        dr3_dm_hp_for_percent(DR3_DM_LAP_PENALTY_PERCENT));

	/*
	 * The duel (drally.h: DR3_CHAOS_MODE_DUEL): two cars - the player's own and one rival of the standings,
	 * within +-DR3_CHAOS_DUEL_RANK_WINDOW ranks of him.  It is done here, before the portraits and before
	 * the car table, so the screen, the grid and the race all see the same two car field; the extra cars of
	 * the standard mode stay out of it (see the guard on the block below).  The rival is left in
	 * dr3_chaos_duel_rival() - the payout at the end of the race reads him back for the Elo.
	 */
	/* a rival named outside - the sign up screen of a duel (___3266ch.c) - stays; a plain race clears it */
	if (dr3_chaos_mode() != DR3_CHAOS_MODE_DUEL) dr3_chaos_duel_rival_set(-1);

	if (dr3_chaos_mode() == DR3_CHAOS_MODE_DUEL) {

		const int dr3_player_seat = (int)B(esp + D(___1a103ch) + 0xcc);
		const int dr3_player_rank = (int)s_6c[dr3_player_seat].rank;
		int       dr3_rival       = dr3_chaos_duel_rival();   /* named by the sign up screen ... */

		/*
		 * ... and kept only while he is still one of the candidates: the standings move between the screen
		 * and the race, so a rival named on an earlier visit can be far away by now.  Anything else -
		 * nobody named, a rival outside the window, the player's own seat - is drawn from the standings
		 * (dr3_log.c: dr3_chaos_duel_pick, the same rule the screen offers with).
		 */
		if (!dr3_chaos_duel_rival_ok(dr3_player_seat, dr3_rival) &&
		    !((dr3_rival >= 0) && (dr3_rival < 0x14) && (dr3_rival != dr3_player_seat) && (s_6c[dr3_rival].damage > 0)))
			dr3_rival = dr3_chaos_duel_pick(dr3_player_seat);

		/* nothing left to draw from (a one-entry standings): the seat next to the player */
		if (dr3_rival < 0) dr3_rival = (dr3_player_seat + 1) % 0x14;

		B(esp + 0xcc) = (__BYTE__)dr3_player_seat;  /* the player into the first slot ... */
		B(esp + 0xcd) = (__BYTE__)dr3_rival;        /* ... and the rival next to him       */

		if ((int)D(___1a103ch) != 0) D(___1a103ch) = 0;

		NumCars = 2;
		NUM_OF_LAPS = dr3_anniv_duel_laps_ex(dr3_anniv_tier((int)s_6c[dr3_player_seat].car, dr3_player_rank), dr3_extreme_active());

		dr3_chaos_duel_rival_set(dr3_rival);

		/*
		 * ... and the start list of the race: the entry list of the sign up screen (___3079ch.c, four slots
		 * per race) is cut down to the two contenders as well, so the list the player comes back to reads
		 * two entries.  The portraits of the roster screen come from the seats above, not from here.
		 */
		{
			extern __BYTE__ ___1a0ef8h[];
			extern __BYTE__ ___1a1f64h[];

			const int dr3_race = (int)D(___1a1028h);

			if ((dr3_race >= 0) && (dr3_race <= 2)) {

				B(___1a1f64h + dr3_race + 3) = 2;
				B(___1a0ef8h + 4*dr3_race + 0) = (__BYTE__)dr3_player_seat;
				B(___1a0ef8h + 4*dr3_race + 1) = (__BYTE__)dr3_rival;

				dr3_log("[dr3] chaos: duel: race %d has %d entry/entries now (seat %d against seat %d)",
				        dr3_race, (int)B(___1a1f64h + dr3_race + 3),
				        (int)B(___1a0ef8h + 4*dr3_race + 0), (int)B(___1a0ef8h + 4*dr3_race + 1));
			}
		}

		dr3_log("[dr3] chaos: duel: seat %d (%s, rank %d, %d points) against seat %d (%s, rank %d, %d points)",
		        dr3_player_seat, s_6c[dr3_player_seat].name, dr3_player_rank,
		        (int)s_6c[dr3_player_seat].points,
		        dr3_rival, s_6c[dr3_rival].name, (int)s_6c[dr3_rival].rank,
		        (int)s_6c[dr3_rival].points);
	}
#endif

	/*
	 * The field of the race and the rows of the roster screen (drally.h: DR3_ROSTER_ROWS).
	 *
	 * The roster screen ("PREPARE TO RACE", below) has four framed columns with the numbers 1..4 in them and
	 * the race lines up in the order of the standings, so what it names is the four *leaders* of the field -
	 * the cars that start from the first four places.  Before this it drew the four seats the event handed
	 * it, which in a chaos race are four drivers out of twenty (and in a duel the two it means to show).
	 *
	 * The screen reads the very list the field setup below does (dr3_chaos_field), so it cannot name a driver
	 * the race does not have, and one log line per row says what it drew.
	 */
	dr3_field_n = dr3_chaos_field(esp + 0xcc, NumCars, dr3_field);

#if defined(DR3_CHAOS)
	/*
	 * Chaos Adversary invader: in any non-duel Chaos race (Standard, Destroy 'em All, Arena), the
	 * Adversary can randomly join the race at the very front of the pack (pole position P1 / Arena
	 * corner 0 slot 0).  Excluded in 1v1 Duels, the original boss duel (D(___1a1028h) == 3), and
	 * multiplayer.  He takes the last slot of the field (never the player's car) as a guest (seat = -1)
	 * so the 20-racer standings table is never corrupted.
	 */
	if (dr3_chaos_active() && !dr3_adversary_active() && (D(___1a1028h) != 3)
#if defined(DR_MULTIPLAYER)
	    && (___19bd60h == 0)
#endif
	   ) {
		const int dr3_m      = dr3_chaos_mode();
		const int dr3_me_b   = (int)D(___1a1ef8h);
		const int dr3_tier_b = (dr3_me_b >= 0 && dr3_me_b < 20)
		                     ? dr3_anniv_tier((int)s_6c[dr3_me_b].car, (int)s_6c[dr3_me_b].rank)
		                     : 0;
		const int dr3_chance = dr3_chaos_boss_override()
		                     ? dr3_chaos_boss_chance()
		                     : dr3_anniv_boss_chance_ex(dr3_tier_b, dr3_extreme_active());

		if (dr3_m == DR3_CHAOS_MODE_DUEL) {
			dr3_log("[dr3] chaos: adversary: skipped (mode DUEL)");
		} else {
			int rand_watcom106(void);
			const int dr3_roll = rand_watcom106() % 100;

			if (dr3_chaos_boss_should_join(dr3_roll, dr3_m, dr3_chance)) {
				dr3_boss_idx = dr3_chaos_boss_pick_car(dr3_field_n, NumCars, (int)D(___1a103ch));
				dr3_chaos_boss_car_set(dr3_boss_idx);
				if (dr3_boss_idx >= 0) {
					dr3_log("[dr3] chaos: adversary: joined race (mode %s, roll %d < %d%%) in car %d -> pole position P1",
					        dr3_chaos_mode_name(dr3_m), dr3_roll, dr3_chance, dr3_boss_idx);
				}
			} else {
				dr3_log("[dr3] chaos: adversary: did not join (mode %s, roll %d >= %d%%)",
				        dr3_chaos_mode_name(dr3_m), dr3_roll, dr3_chance);
			}
		}
	}
#endif

	{
		int dr3_ranks[DR3_MAX_CARS], dr3_order[DR3_MAX_CARS], dr3_r;
		const int dr3_rows = (NumCars < dr3_field_n) ? NumCars : dr3_field_n;

		for (dr3_r = 0; dr3_r < dr3_field_n; ++dr3_r)
			dr3_ranks[dr3_r] = (int)s_6c[dr3_field[dr3_r]].rank;

		/* the same rule the grid itself lines up with (___33010h.c, dr3_grid_order_with_leader) */
		dr3_grid_order_with_leader(dr3_ranks, dr3_field_n, dr3_boss_idx, dr3_order);

		for (dr3_r = 0; dr3_r < dr3_rows; ++dr3_r) {

			const int dr3_car_i = dr3_order[dr3_r];

			if ((dr3_boss_idx >= 0) && (dr3_car_i == dr3_boss_idx)) {

				dr3_roster[dr3_r] = -1;

				dr3_log("[dr3] chaos: roster row %d of %d: %-11s (boss invader, car %d)",
				        dr3_r + 1, dr3_rows, "ADVERSARY", DR3_ADVERSARY_CAR);
			}
			else {

				dr3_roster[dr3_r] = dr3_field[dr3_car_i];

				dr3_log("[dr3] chaos: roster row %d of %d: %-11s (standings %2d, %5d points, car %d)",
				        dr3_r + 1, dr3_rows, s_6c[dr3_roster[dr3_r]].name,
				        (int)s_6c[dr3_roster[dr3_r]].rank, (int)s_6c[dr3_roster[dr3_r]].points,
				        (int)s_6c[dr3_roster[dr3_r]].car);
			}
		}
	}

#if defined(DR3_CHAOS)
	if(dr3_boss_in_race()){
		strcpy(esp+0x4c, dr3_boss_phase_title(dr3_boss_phase()));
	}
	else if(dr3_chaos_battle()){
		strcpy(esp+0x4c, "LAST CAR STANDING");
	}
	else {
		strcpy(esp+0x4c, "NUMBER OF LAPS: ");
		itoa_watcom106(NUM_OF_LAPS, esp+0x74, 0xa);
		strcat(esp+0x4c, esp+0x74);
	}
#else
	strcpy(esp+0x4c, "NUMBER OF LAPS: ");
	itoa_watcom106(NUM_OF_LAPS, esp+0x74, 0xa);
	strcat(esp+0x4c, esp+0x74);
#endif
	___12e78h_cdecl(___1a10b8h, ___185c7ah, esp+0x4c, 0x3800d+10*0x280);	/* 10 rows lower: clear of the map picture / logo */
	strcpy(esp+0x4c, "PRIZE MONEY: ");

#if defined(DR_MULTIPLAYER)
	if(___19bd60h == 0){
#endif // DR_MULTIPLAYER

#if defined(DR3_CHAOS)
		if(dr3_boss_in_race()){
			if(dr3_boss_is_swarm())            strcpy(esp+0x4c, "1 VS 20 VAGABONDS");
			else if(dr3_boss_is_escort_race()) strcpy(esp+0x4c, "9 LAPS - 12 ELITES");
			else                               strcpy(esp+0x4c, "PRIZE: GLORY");
		}
		else if(dr3_chaos_active() && (D(___1a1028h) != 3)){
			int dr3_pot = DR3_CHAOS_RANK_MONEY;
			if(dr3_chaos_arena()){
				dr3_pot = DR3_ARENA_PRIZE_1;
			}
			else if(dr3_chaos_mode() == DR3_CHAOS_MODE_DM){
				dr3_pot = DR3_CHAOS_DM_POT;
			}
			else if(dr3_chaos_mode() == DR3_CHAOS_MODE_DUEL){
				const int dr3_rv = dr3_chaos_duel_rival();
				const int dr3_rk = (dr3_rv >= 0) ? (int)s_6c[dr3_rv].rank : 20;
				if(dr3_rk <= DR3_CHAOS_DUEL_TOP_RANK)      dr3_pot = DR3_CHAOS_DUEL_POT_TOP;
				else if(dr3_rk <= DR3_CHAOS_DUEL_MID_RANK) dr3_pot = DR3_CHAOS_DUEL_POT_MID;
				else                                       dr3_pot = DR3_CHAOS_DUEL_POT_LOW;
			}
			strcat(esp+0x4c, "$");
			itoa_watcom106(dr3_pot, esp+0x74, 0xa);
			strcat(esp+0x4c, esp+0x74);
		}
		else
#endif
		{
			if(D(___1a1028h) == 0) strcat(esp+0x4c, "$750");
			if(D(___1a1028h) == 1) strcat(esp+0x4c, "$3000");
			if(D(___1a1028h) == 2) strcat(esp+0x4c, "$12000");
			if(D(___1a1028h) == 3) strcat(esp+0x4c, "GLORY");
		}

#if defined(DR_MULTIPLAYER)
	}
	else {

		strcat(esp+0x4c, "$");
		D(esp+0xd4) = 3*MP_MONEY/10;
		itoa_watcom106(D(esp+0xd4), esp+0x74, 0xa);
		strcat(esp+0x4c, esp+0x74);
	}
#endif // DR_MULTIPLAYER

	eax = ___251e8h_cdecl(esp+0x4c);
	___12e78h_cdecl(___1a10b8h, ___185c7ah, esp+0x4c, 0x3816b-eax+10*0x280);

/* ROSTER FACES ******************************************************************************** */
	if(D(___1a1028h) != 3){

		D(esp+0xd0) = 0;

		if(NumCars > 0){

			D(esp+0xf4) = 0;

			while(1){

#if defined(DR3_CHAOS)
				/*
				 * Which racer the row is about: a chaos race names the leaders of its field there (the
				 * dr3_roster filled above - the screen has four columns for a field of twenty) and any
				 * other race the seat the event handed the screen, which is what the original game
				 * shows.  The rank, the name and the portrait below all read this one value.
				 */
				const int dr3_seat = dr3_chaos_active() ? dr3_roster[(int)D(esp+0xd0)]
				                                        : (int)B(esp + D(esp+0xd0) + 0xcc);
#else
				const int dr3_seat = (int)B(esp + D(esp+0xd0) + 0xcc);
#endif

				if(dr3_seat < 0){

					strupr_watcom106(strcpy(esp+0x4c, "adversary"));
					___12e78h_cdecl(___1a10b8h, ___185c7ah, esp+0x4c, D(esp+D(esp+0xf4)+0xa8));

					j = -1;
					while(++j < 0x40){

						i = -1;
						while(++i < 0x40){

							px = read_b(___1a0fb4h+0x40*j+i);
							write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+D(esp+D(esp+0xf4)+0xb8)+0x280*j+i, px);
						}
					}
				}
				else {

					itoa_watcom106(s_6c[dr3_seat].rank, esp+0x4c, 0xa);
					strcat(esp+0x4c, ".");
					eax = ___251e8h_cdecl(esp+0x4c);
					___12e78h_cdecl(___1a10b8h, ___185c7ah, esp+0x4c, D(esp+D(esp+0xf4)+0x88)-eax);
					strcpy(esp+0x4c, s_6c[dr3_seat].name);
					strupr_watcom106(esp+0x4c);
					___12e78h_cdecl(___1a10b8h, ___185c7ah, esp+0x4c, D(esp+D(esp+0xf4)+0xa8));

					j = -1;
					while(++j < 0x40){

						i = -1;
						while(++i < 0x40){

							px = read_b(___19de70h[s_6c[dr3_seat].face]+0x40*j+i);
							write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+D(esp+D(esp+0xf4)+0xb8)+0x280*j+i, px);
						}
					}
				}

				D(esp+0xf4) += 4;
				D(esp+0xd0) += 1;

				if((int)D(esp+0xd0) >= NumCars) break;
			}
		}
	}
	else {

		strupr_watcom106(strcpy(esp+0x4c, "adversary"));
		___12e78h_cdecl(___1a10b8h, ___185c7ah, esp+0x4c, D(esp+0xa8));

		j = -1;
		while(++j < 0x40){

			i = -1;
			while(++i < 0x40){

				px = read_b(___1a0fb4h+0x40*j+i);
				write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+D(esp+0xb8)+0x280*j+i, px);
			}
		}

		itoa_watcom106(s_6c[B(esp+0xcc)].rank, esp+0x4c, 0xa);
		strcat(esp+0x4c, ".");
		eax = ___251e8h_cdecl(esp+0x4c);
		___12e78h_cdecl(___1a10b8h, ___185c7ah, esp+0x4c, D(esp+0x8c)-eax);
		strcpy(esp+0x4c, s_6c[B(esp+0xcc)].name);
		strupr_watcom106(esp+0x4c);
		___12e78h_cdecl(___1a10b8h, ___185c7ah, esp+0x4c, D(esp+0xac));

		j = -1;
		while(++j < 0x40){

			i = -1;
			while(++i < 0x40){

				px = read_b(___19de70h[s_6c[B(esp+0xcc)].face]+0x40*j+i);
				write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+D(esp+0xbc)+0x280*j+i, px);
			}
		}
	}
/* ******************************************************************************** ROSTER FACES */

#if defined(DR3_CHAOS)
	if(dr3_chaos_active() && (D(___1a1028h) != 3)){
		void dr3_loading_header(int mode);
		dr3_loading_header(dr3_chaos_mode());
	}
#endif

	/* Ensure rows 0..73 on DefaultScreenBuffer have the clean background / Death Rally logo restored,
	 * clearing the old Y=16 dr3_spin_header from ___3266ch.c (which helper000 does not wipe, as it wipes rows 73..478). */
	memcpy(___1a1100h__VESA101h_DefaultScreenBuffer, ___1a1138h__VESA101_BACKGROUND, 0x280 * 73);

	___1a112ch__VESA101_ACTIVESCREEN_PTR = ___1a1100h__VESA101h_DefaultScreenBuffer;
	___3a968h_cdecl(-1);

#if defined(DR3_CHAOS)
	if(dr3_chaos_active() && (D(___1a1028h) != 3)){
		void dr3_loading_header(int mode);
		dr3_loading_header(dr3_chaos_mode());
	}
#endif
	
#if defined(DR_MULTIPLAYER)
	if(___19bd60h != 0){
		
		dRally_Sound_setPosition(0x1e00);
	}
	else {
#endif // DR_MULTIPLAYER
		dRally_Sound_setPosition(0x2800);
#if defined(DR_MULTIPLAYER)
	}
#endif // DR_MULTIPLAYER
	
	dRally_Sound_setMasterVolume(0x10000);
	___12cb8h__VESA101_PRESENTSCREEN();
	__WAIT_5();

#if defined(DR_MULTIPLAYER)
	if(___19bd60h != 0){
	
		if(D(___196a84h) != 0) D(___185a34h) = 1;

		if(D(___196a84h) == 0){
			
			esp0x98[0] = (__POINTER__)&s_6c[B(___1a1168h)];
			esp0x98[1] = (__POINTER__)&s_6c[B(___1a1168h+1)];
			esp0x98[2] = (__POINTER__)&s_6c[B(___1a1168h+2)];
			esp0x98[3] = (__POINTER__)&s_6c[B(___1a1168h+3)];
			___1a0f9ch = ___3f71ch__allocateMemory(4);
			B(___1a0f9ch) = D(___1a1ef8h);
			___60a84h();
			
			n = 0;
			esi = 0;
			while(1){

				if(D(___196a84h) != 0) break;
				___38878h_cdecl();
				___2ab50h();
				___2ab50h();

				if((esi%10) == 1) n = ___60b60h(1, D(___196adch), (__POINTER__)&s_6c[D(___1a1ef8h)], sizeof(racer_t));
				if((esi%900) == 899) ___23488h_cdecl(___1a0f9ch, 1, 0x13);

				esi++;
			
				if(n == 0xff) break;
			}

			___611c0h(0x6c, (__POINTER__ *)esp0x98);
			if(n == 0xff) ___3881ch();
			___12cb8h__VESA101_PRESENTSCREEN();
			dRMemory_free(___1a0f9ch);
		}
	}
#endif // DR_MULTIPLAYER

	if(D(___196a84h) != 0){
		
		D(___185a34h) = 1;
		return;
	}

	LAP_BEST_SEC = 0;
	LAP_BEST_MIN = 0;
	LAP_PREVIOUS_100 = 0;
	LAP_PREVIOUS_SEC = 0;
	LAP_PREVIOUS_MIN = 0;
	D(___1a1090h) = 0;
	D(___1a10a8h) = 0;
	D(___1a10a0h) = 0;
	D(___196ab0h) = 0;
	LAP_BEST_100 = 0;

	edx = 0;
	n = -1;
	while(++n < 0x14){

		if(((int)edx < (int)s_6c[n].points)&&(n != D(___1a1ef8h))) edx = s_6c[n].points;
	}

	D(___1de7d0h+0x4c) = 0;

#if defined(DR_MULTIPLAYER)
		if(___19bd60h == 0){
#endif
			if((int)D(___196aa4h) > 0) D(___1de7d0h+0x4c) = 1;
#if defined(DR_MULTIPLAYER)
		}
#endif // DR_MULTIPLAYER

	n = -1;
	while(++n < NumCars){

		strcpy(___1de7d0h+0x54*n, s_6c[B(esp+n+0xcc)].name);
		strupr_watcom106(___1de7d0h+0x54*n);
		D(___1de7d0h+0x54*n+0x10) = s_6c[B(esp+n+0xcc)].damage;
		D(___1de7d0h+0x54*n+0x14) = s_6c[B(esp+n+0xcc)].engine;
		D(___1de7d0h+0x54*n+0x18) = s_6c[B(esp+n+0xcc)].tires;
		D(___1de7d0h+0x54*n+0x30) = s_6c[B(esp+n+0xcc)].car;
		D(___1de7d0h+0x54*n+0x1c) = s_6c[B(esp+n+0xcc)].armor;

		edx = B(esp+n+0xcc);
		if(edx != D(___1a1ef8h)){
#if defined(DR3_CHAOS)
			if(dr3_chaos_active() && (D(___1a1028h) != 3)){
				const int dr3_me_d = (int)D(___1a1ef8h);
				const int dr3_td   = (dr3_me_d >= 0 && dr3_me_d < 20)
				                   ? dr3_anniv_tier((int)s_6c[dr3_me_d].car, (int)s_6c[dr3_me_d].rank)
				                   : 0;
				D(___1de7d0h+0x54*n+0x44) = dr3_anniv_ai_difficulty_row_ex(dr3_td, dr3_extreme_active());
			}
			else
#endif
			D(___1de7d0h+0x54*n+0x44) = DR3_DIFFICULTY_ROW(___196a94h_difficulty);
		}
		else {
		
			/* 4 = MY_DIFFICULTY: the row of the parameter tables in race___3f970h.c that belongs to the
			   player's own car - the four selectable difficulties are rows 0..3 there */
			D(___1de7d0h+0x54*n+0x44) = 4;
		}

		D(___1de7d0h+0x54*n+0x2c) = D(___185a14h_UseWeapons);
		D(___1de7d0h+0x54*n+0x20) = s_6c[B(esp+n+0xcc)].rocket_fuel;
		D(___1de7d0h+0x54*n+0x24) = s_6c[B(esp+n+0xcc)].spikes;
		D(___1de7d0h+0x54*n+0x28) = s_6c[B(esp+n+0xcc)].mines;		// LOAD BOUGHT MINES 

		edx = B(esp+n+0xcc);
		if(edx != D(___1a1ef8h)){
#if defined(DR_MULTIPLAYER)			
			if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
				D(___1de7d0h+0x54*n+0x28) = 0;
				D(___1de7d0h+0x54*n+0x24) = 0;
				D(___1de7d0h+0x54*n+0x20) = 0;
				if(D(___185a14h_UseWeapons) != 0){
					if(dr3_extreme_active()){
						if((rand_watcom106()%2) == 0) D(___1de7d0h+0x54*n+0x20) = 1;
						if((rand_watcom106()%2) == 0) D(___1de7d0h+0x54*n+0x24) = 1;
						if((rand_watcom106()%5) <  3) D(___1de7d0h+0x54*n+0x28) = 8;
					} else {
						if((rand_watcom106()%5) == 0) D(___1de7d0h+0x54*n+0x20) = 1;
						if((rand_watcom106()%5) == 0) D(___1de7d0h+0x54*n+0x24) = 1;
						if((rand_watcom106()%5) == 0) D(___1de7d0h+0x54*n+0x28) = 8;
					}
				}
#if defined(DR_MULTIPLAYER)	
			}
#endif // DR_MULTIPLAYER
		}

		eaxp = ___1a0fb8h;

#if defined(DR_MULTIPLAYER)	
		if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
			eax = B(esp+n+0xcc);
			if(eax != D(___1a1ef8h)) eaxp = ___1a0fe0h;
#if defined(DR_MULTIPLAYER)	
		}
#endif // DR_MULTIPLAYER

		D(___1de7d0h+0x54*n+0x34) = B(eaxp+3*s_6c[B(esp+n+0xcc)].color);
		D(___1de7d0h+0x54*n+0x38) = B(eaxp+3*s_6c[B(esp+n+0xcc)].color+1);
		D(___1de7d0h+0x54*n+0x3c) = B(eaxp+3*s_6c[B(esp+n+0xcc)].color+2);

#if defined(DR_MULTIPLAYER)
		if(___19bd60h != 0){

			if((int)(D(___196adch)-1) < n){

				D(___1de7d0h+0x54*n+0x34) = B(___1a0fe0h+0x1e);
				D(___1de7d0h+0x54*n+0x38) = B(___1a0fe0h+0x1f);
				D(___1de7d0h+0x54*n+0x3c) = B(___1a0fe0h+0x20);
			}
		}
#endif // DR_MULTIPLAYER
	}//w

#if defined(DR3_CHAOS)
	/*
	 * Chaos test build (-DDR3_CHAOS, see doc/3ds.md): the whole standings on the grid.
	 *
	 * The front end sets up NumCars entries for the race and its screens draw four of them, so every
	 * driver of the roster that is not in the race yet becomes one more car - with everything else
	 * copied from an existing entry, because the parameter tables in race___3f970h.c are indexed with
	 * the difficulty row and the car id and both stay valid this way.
	 *
	 * The four startername bytes at esp+0xcc are deliberately left alone: the screen code reads them
	 * with NumCars entries, and the counters it keeps at esp+0xd0/esp+0xd4 sit right behind them.
	 *
	 * How many cars join is the switch (dr3_chaos_extra_cars, dr3_chaos.h): it is read here, so the
	 * value a race was set up with is in the log, and the entries behind the race stay untouched while
	 * it is 0.
	 *
	 * Every car of the field also gets the rank of its racer (dr3_chaos_car_rank) and, from the whole
	 * set of ranks, the place in the grid it starts from (dr3_chaos_car_pos): place 0 is the leader, so
	 * race___49bd4h.c hands the four start slots out in that order, race___42824h.c walks the rest of
	 * the field onto the road in it and race___4a25ch.c starts them with that position - the field
	 * lines up the way the standings read.
	 */
	/*
	 * The switches of the chaos build and the duel are read at the top of this function, not here (see the
	 * block behind the seat table): the mode is what decides that this race *is* a duel, and the duel has to
	 * cut the field down to two *before* the roster screen draws its portraits and before the car table (both
	 * above) is filled from those seats.  Read here, the duel only ever changed what the race was, while the
	 * screen and the field kept driving the four entries of the event.
	 */

	/* which racer sits in car n - every mode, and every car the race itself brings (dr3_chaos.h) */
	{
		int dr3_m;

		for (dr3_m = 0; (dr3_m < NumCars) && (dr3_m < DR3_MAX_CARS); ++dr3_m)
			dr3_chaos_car_seat[dr3_m] = dr3_field[dr3_m];
	}

	/* the rank of every car of the race - the entries behind the race keep 0 and are not placed */
	{
		int dr3_m;

		for (dr3_m = 0; dr3_m < DR3_MAX_CARS; ++dr3_m) dr3_chaos_car_rank[dr3_m] = 0;
		for (dr3_m = 0; dr3_m < NumCars; ++dr3_m)
			dr3_chaos_car_rank[dr3_m] = (int)s_6c[dr3_field[dr3_m]].rank;
	}

	{
		int dr3_n;

		/*
		 * The extras of the field (dr3_chaos_field, above): every car behind the NumCars seats of the
		 * event.  A whole entry is copied first, so fields this block does not know about are set as
		 * well, and then everything the race reads - the parameter tables of race___3f970h.c stay valid,
		 * because the difficulty row and the car id come with the copy.
		 */
		for (dr3_n = NumCars; (dr3_n < DR3_MAX_CARS) && (dr3_n < dr3_field_n); ++dr3_n) {

			const int dr3_racer = dr3_field[dr3_n];

			/* a whole entry first, so fields this block does not know about are set as well */
			memcpy(___1de7d0h + 0x54 * dr3_n, ___1de7d0h + 0x54 * (dr3_n % NumCars), 0x54);

			strcpy(___1de7d0h + 0x54 * dr3_n, s_6c[dr3_racer].name);
			strupr_watcom106(___1de7d0h + 0x54 * dr3_n);

			D(___1de7d0h + 0x54 * dr3_n + 0x10) = s_6c[dr3_racer].damage;
			D(___1de7d0h + 0x54 * dr3_n + 0x14) = s_6c[dr3_racer].engine;
			D(___1de7d0h + 0x54 * dr3_n + 0x18) = s_6c[dr3_racer].tires;
			D(___1de7d0h + 0x54 * dr3_n + 0x1c) = s_6c[dr3_racer].armor;
			D(___1de7d0h + 0x54 * dr3_n + 0x20) = s_6c[dr3_racer].rocket_fuel;
			D(___1de7d0h + 0x54 * dr3_n + 0x24) = s_6c[dr3_racer].spikes;
			D(___1de7d0h + 0x54 * dr3_n + 0x28) = s_6c[dr3_racer].mines;
			if(dr3_extreme_active() && (D(___185a14h_UseWeapons) != 0)){
				if((rand_watcom106()%2) == 0) D(___1de7d0h + 0x54 * dr3_n + 0x20) = 1;
				if((rand_watcom106()%2) == 0) D(___1de7d0h + 0x54 * dr3_n + 0x24) = 1;
				if((rand_watcom106()%5) <  3) D(___1de7d0h + 0x54 * dr3_n + 0x28) = 8;
			}
			D(___1de7d0h + 0x54 * dr3_n + 0x30) = s_6c[dr3_racer].car;
			D(___1de7d0h + 0x54 * dr3_n + 0x34) = B(___1a0fe0h + 3 * s_6c[dr3_racer].color);
			D(___1de7d0h + 0x54 * dr3_n + 0x38) = B(___1a0fe0h + 3 * s_6c[dr3_racer].color + 1);
			D(___1de7d0h + 0x54 * dr3_n + 0x3c) = B(___1a0fe0h + 3 * s_6c[dr3_racer].color + 2);
			{
				const int dr3_me_e = (int)D(___1a1ef8h);
				const int dr3_te   = (dr3_me_e >= 0 && dr3_me_e < 20)
				                   ? dr3_anniv_tier((int)s_6c[dr3_me_e].car, (int)s_6c[dr3_me_e].rank)
				                   : 0;
				D(___1de7d0h + 0x54 * dr3_n + 0x44) = (dr3_chaos_active() && (D(___1a1028h) != 3))
				                                    ? dr3_anniv_ai_difficulty_row_ex(dr3_te, dr3_extreme_active())
				                                    : DR3_DIFFICULTY_ROW(___196a94h_difficulty);
			}

			dr3_chaos_car_rank[dr3_n] = (int)s_6c[dr3_racer].rank;
			dr3_chaos_car_seat[dr3_n] = dr3_racer;
		}

		/*
		 * Chaos Adversary invader: populate his car entry as a fully upgraded SPECIAL (model 6,
		 * jet-black sprite, dual Deliverator guns, +8% speed / +5% steer in race___3f970h.c).
		 * seat = -1 keeps him out of the 20-racer standings table while still racing and paying a
		 * $1000 kill bonus if the player wrecks him (race___4dcach.c).
		 */
		if ((dr3_boss_idx >= 0) && (dr3_boss_idx < DR3_MAX_CARS)) {

			if (dr3_boss_idx != 0)
				memcpy(___1de7d0h + 0x54 * dr3_boss_idx, ___1de7d0h, 0x54);

			strcpy(___1de7d0h + 0x54 * dr3_boss_idx, "ADVERSARY");
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x10) = 0;
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x14) = 3;
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x18) = 3;
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x1c) = 3;
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x20) = !!D(___185a14h_UseWeapons);
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x24) = 1;
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x28) = 8 * !!D(___185a14h_UseWeapons);
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x2c) = D(___185a14h_UseWeapons);
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x30) = DR3_ADVERSARY_CAR;
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x34) = 48;
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x38) = 0;
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x3c) = 63;
			D(___1de7d0h + 0x54 * dr3_boss_idx + 0x44) = DR3_DIFFICULTY_ROW(___196a94h_difficulty);

			dr3_chaos_car_seat[dr3_boss_idx] = -1;
			dr3_chaos_car_rank[dr3_boss_idx] = 1;
		}

		/*
		 * 30th Anniversary 3-Phase Final Boss Gauntlet (platform_3ds/dr3_boss.h):
		 *   Phase 1: 1 vs 20 Vagabonds on TRA (player = car 1, enemies = car 0 & cars 2..20)
		 *   Phase 2: 5-lap high-speed race on TR0 vs 11 maxed elite rivals (12 cars)
		 *   Phase 3: 1 vs 1 Giant Boss Deliverator (car 0 = ADVERSARY, car 1 = player) on TRA
		 */
		if (dr3_boss_is_swarm()) {
			const int me = (int)D(___1a1ef8h);
			int c, k = 0;

			dr3_field_n = DR3_BOSS_P1_CARS;
			dr3_chaos_car_seat[1] = me;
			dr3_chaos_car_rank[1] = 1;

			for (c = 0; c < DR3_BOSS_P1_CARS; ++c) {
				if (c == 1) continue;
				++k;
				memcpy(___1de7d0h + 0x54 * c, ___1de7d0h + 0x54 * 1, 0x54);
				snprintf((char *)(___1de7d0h + 0x54 * c), 16, "SWARM %02d", k);
				D(___1de7d0h + 0x54 * c + 0x10) = 0;
				D(___1de7d0h + 0x54 * c + 0x14) = 2;
				D(___1de7d0h + 0x54 * c + 0x18) = 2;
				D(___1de7d0h + 0x54 * c + 0x1c) = 1;
				D(___1de7d0h + 0x54 * c + 0x20) = 0;
				D(___1de7d0h + 0x54 * c + 0x24) = 0;
				D(___1de7d0h + 0x54 * c + 0x28) = !!D(___185a14h_UseWeapons);
				D(___1de7d0h + 0x54 * c + 0x2c) = D(___185a14h_UseWeapons);
				D(___1de7d0h + 0x54 * c + 0x30) = 0;   /* Vagabond (KUPLA) */
				D(___1de7d0h + 0x54 * c + 0x34) = B(___1a0fe0h + 3 * (k % 20));
				D(___1de7d0h + 0x54 * c + 0x38) = B(___1a0fe0h + 3 * (k % 20) + 1);
				D(___1de7d0h + 0x54 * c + 0x3c) = B(___1a0fe0h + 3 * (k % 20) + 2);
				D(___1de7d0h + 0x54 * c + 0x44) = DR3_DIFFICULTY_ROW(___196a94h_difficulty);

				dr3_chaos_car_seat[c] = -1;
				dr3_chaos_car_rank[c] = k + 1;
			}
			dr3_log("[dr3] boss: phase 1 populated %d Vagabonds + player (car 1)", k);
		}
		else if (dr3_boss_is_escort_race()) {
			const int me = (int)D(___1a1ef8h);
			int rivals[20], nr = 0, i2, j2, c, idx = 0;

			for (i2 = 0; i2 < 20; ++i2) {
				if (i2 == me) continue;
				rivals[nr++] = i2;
			}
			/* sort rivals by standings rank ascending so the top 11 championship contenders race */
			for (i2 = 1; i2 < nr; ++i2) {
				const int key = rivals[i2];
				for (j2 = i2 - 1; (j2 >= 0) && (s_6c[rivals[j2]].rank > s_6c[key].rank); --j2)
					rivals[j2 + 1] = rivals[j2];
				rivals[j2 + 1] = key;
			}

			dr3_field_n = DR3_BOSS_P2_CARS;
			dr3_chaos_car_seat[1] = me;
			dr3_chaos_car_rank[1] = DR3_BOSS_P2_CARS;   /* player starts P12 (slot 3) */

			for (c = 0; c < DR3_BOSS_P2_CARS; ++c) {
				int r;
				if (c == 1) continue;
				r = rivals[idx % nr];
				memcpy(___1de7d0h + 0x54 * c, ___1de7d0h + 0x54 * 1, 0x54);
				strcpy((char *)(___1de7d0h + 0x54 * c), s_6c[r].name);
				strupr_watcom106((char *)(___1de7d0h + 0x54 * c));
				D(___1de7d0h + 0x54 * c + 0x10) = 0;
				D(___1de7d0h + 0x54 * c + 0x14) = dr3_extreme_active() ? 3 : ((idx < 3) ? 3 : 2);
				D(___1de7d0h + 0x54 * c + 0x18) = dr3_extreme_active() ? 3 : ((idx < 3) ? 3 : 2);
				D(___1de7d0h + 0x54 * c + 0x1c) = dr3_extreme_active() ? 3 : ((idx < 7) ? 2 : 1);
				D(___1de7d0h + 0x54 * c + 0x20) = dr3_extreme_active() ? !!D(___185a14h_UseWeapons) : 0;
				D(___1de7d0h + 0x54 * c + 0x24) = (dr3_extreme_active() || (idx < 5)) ? 1 : 0;
				D(___1de7d0h + 0x54 * c + 0x28) = (dr3_extreme_active() ? 4 : ((idx < 5) ? 2 : 1)) * !!D(___185a14h_UseWeapons);
				D(___1de7d0h + 0x54 * c + 0x2c) = D(___185a14h_UseWeapons);
				D(___1de7d0h + 0x54 * c + 0x30) = dr3_extreme_active()
				                                ? ((idx < 6) ? 5 : 4)
				                                : ((idx < 3) ? 5 : ((idx < 7) ? 4 : 3));  /* Deliverators, Wraiths & Shriekers */
				D(___1de7d0h + 0x54 * c + 0x34) = B(___1a0fe0h + 3 * s_6c[r].color);
				D(___1de7d0h + 0x54 * c + 0x38) = B(___1a0fe0h + 3 * s_6c[r].color + 1);
				D(___1de7d0h + 0x54 * c + 0x3c) = B(___1a0fe0h + 3 * s_6c[r].color + 2);
				D(___1de7d0h + 0x54 * c + 0x44) = DR3_DIFFICULTY_ROW(___196a94h_difficulty);

				dr3_chaos_car_seat[c] = -1;
				dr3_chaos_car_rank[c] = idx + 1;
				++idx;
			}
			dr3_log("[dr3] boss: phase 2 populated %d elite rivals + player (car 1) on TR0", idx);
		}
		else if (dr3_boss_is_final_duel()) {
			const int me = (int)D(___1a1ef8h);

			dr3_field_n = DR3_BOSS_P3_CARS;
			strcpy((char *)___1de7d0h, "ADVERSARY");
			D(___1de7d0h + 0x10) = 0;
			D(___1de7d0h + 0x14) = 3;
			D(___1de7d0h + 0x18) = 3;
			D(___1de7d0h + 0x1c) = 3;
			D(___1de7d0h + 0x20) = !!D(___185a14h_UseWeapons);
			D(___1de7d0h + 0x24) = 1;
			D(___1de7d0h + 0x28) = DR3_BOSS_P3_START_MINES * !!D(___185a14h_UseWeapons);
			D(___1de7d0h + 0x2c) = D(___185a14h_UseWeapons);
			D(___1de7d0h + 0x30) = DR3_ADVERSARY_CAR;
			D(___1de7d0h + 0x34) = 48;
			D(___1de7d0h + 0x38) = 0;
			D(___1de7d0h + 0x3c) = 63;
			D(___1de7d0h + 0x44) = DR3_DIFFICULTY_ROW(___196a94h_difficulty);

			dr3_chaos_car_seat[0] = -1;
			dr3_chaos_car_rank[0] = 1;
			dr3_chaos_car_seat[1] = me;
			dr3_chaos_car_rank[1] = 2;
			dr3_log("[dr3] boss: phase 3 populated Giant Adversary (car 0) vs player (car 1) on TRA");
		}

		/*
		 * The grid order: place 0 is the leader (the Adversary when he invades, else rank 1), place 1
		 * the second and so on.  The four start slots of the track file go to the first four places
		 * (race___49bd4h.c), the road walk hands its spots out in the same order (race___42824h.c) and
		 * the position a car starts with is its place (race___4a25ch.c) - so the field lines up the way
		 * the standings read, with the Adversary right at the front of the pack when he joins.
		 */
		{
			int dr3_order[DR3_MAX_CARS];
			int dr3_i;

			dr3_grid_order_with_leader(dr3_chaos_car_rank, DR3_MAX_CARS, dr3_boss_idx, dr3_order);

			for (dr3_i = 0; dr3_i < DR3_MAX_CARS; ++dr3_i)
				dr3_chaos_car_pos[dr3_order[dr3_i]] = dr3_i;

			for (dr3_i = 0; dr3_i < dr3_field_n; ++dr3_i) {

				int dr3_car;

				for (dr3_car = 0; dr3_car < DR3_MAX_CARS; ++dr3_car)
					if (dr3_chaos_car_pos[dr3_car] == dr3_i) break;

				if (dr3_car >= DR3_MAX_CARS) continue;

				dr3_log("[dr3] chaos: grid place %2d: %-11s (standings %d, colour %d,%d,%d)",
				        dr3_i + 1, (const char *)(___1de7d0h + 0x54 * dr3_car),
				        dr3_chaos_car_rank[dr3_car],
				        B(___1de7d0h + 0x54 * dr3_car + 0x34), B(___1de7d0h + 0x54 * dr3_car + 0x38),
				        B(___1de7d0h + 0x54 * dr3_car + 0x3c));
			}

			/* Initialize 30th Anniversary in-race radio & rival dialogue for this race */
			if (dr3_chaos_active()) {
				const int me_r   = (int)D(___1a1ef8h);
				const int tier_r = (me_r >= 0 && me_r < 20)
				                 ? dr3_anniv_tier((int)s_6c[me_r].car, (int)s_6c[me_r].rank)
				                 : 0;
				const char *rival_nm = "RIVAL";
				int k_r;

				for (k_r = 0; k_r < dr3_field_n; ++k_r) {
					int c_r = dr3_order[k_r];
					if (c_r >= 0 && c_r < DR3_MAX_CARS && dr3_chaos_car_seat[c_r] != me_r &&
					    dr3_chaos_car_seat[c_r] >= 0) {
						rival_nm = (const char *)(___1de7d0h + 0x54 * c_r);
						break;
					}
				}
				dr3_anniv_radio_race_init(tier_r, dr3_field_n, dr3_chaos_mode(),
				                          (dr3_boss_idx >= 0) ? 1 : 0, rival_nm);
			}
		}
	}
#endif

#if defined(DR_MULTIPLAYER)
	if(___19bd60h != 0){

		n = -1;
		while(++n < 4){

			D(___1de7d0h+0x54*n+0x34) = B(___1a0fe0h+0x1e);
			D(___1de7d0h+0x54*n+0x38) = B(___1a0fe0h+0x1f);
			D(___1de7d0h+0x54*n+0x3c) = B(___1a0fe0h+0x20);
		}
	}
#endif // DR_MULTIPLAYER

	edx = 0;
	n = -1;
	while(++n < NumCars){

		if(((int)edx < (int)s_6c[n].points)&&(n != D(___1a1ef8h))) edx = s_6c[n].points;
	}

	/*
	 * On the fourth difficulty the adversary is in the race anyway (see doc/3ds.md), so nobody else's
	 * race entry has to turn into him here.  Same when the Chaos invader already joined.  On the other
	 * difficulties this stays as it was.
	 */
	if(!dr3_adversary_active()
#if defined(DR3_CHAOS)
	   && (dr3_boss_idx < 0)
	   && !dr3_boss_in_race()
#endif
	   && ((int)edx < (int)s_6c[D(___1a1ef8h)].points)){
#if defined(DR_MULTIPLAYR)
		if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
			strcpy(___1de7d0h, "ADVERSARY"); 
			D(___1de7d0h+0x44) = DR3_DIFFICULTY_ROW(___196a94h_difficulty);
			D(___1de7d0h+0x10) = 0;
			D(___1de7d0h+0x20) = 0;
			D(___1de7d0h+0x24) = 1;
			D(___1de7d0h+0x30) = 6;
			D(___1de7d0h+0x28) = 8*!!D(___185a14h_UseWeapons);
			D(___1de7d0h+0x2c) = D(___185a14h_UseWeapons);
			D(___1de7d0h+2*0x54+0x34) = B(___1a0fe0h+0x1e);
			D(___1de7d0h+2*0x54+0x38) = B(___1a0fe0h+0x1f);
			D(___1de7d0h+2*0x54+0x3c) = B(___1a0fe0h+0x20);
			D(___1de7d0h+3*0x54+0x34) = D(___1de7d0h+2*0x54+0x34);
			D(___1de7d0h+3*0x54+0x38) = D(___1de7d0h+2*0x54+0x38);
			D(___1de7d0h+3*0x54+0x3c) = D(___1de7d0h+2*0x54+0x3c);
#if defined(DR_MULTIPLAYR)
		}
#endif // DR_MULTIPLAYER
	}

	LAP_RECORD_MIN = ___19f750h[DR3_RECORD_CAR(s_6c[D(___1a1ef8h)].car)][B(___1a1f64h+D(___1a1028h))].min;
	LAP_RECORD_SEC = ___19f750h[DR3_RECORD_CAR(s_6c[D(___1a1ef8h)].car)][B(___1a1f64h+D(___1a1028h))].sec;
	LAP_RECORD_100 = ___19f750h[DR3_RECORD_CAR(s_6c[D(___1a1ef8h)].car)][B(___1a1f64h+D(___1a1028h))].sec100;
	D(___1de7d0h+0x50) = 0;

#if defined(DR_MULTIPLAYER)	
	if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
		{
			/* Compute max points across ALL 20 racers (not just 0..NumCars-1) so in-race $ popups match post-race payout */
			int dr3_max_other = 0, dr3_r;
			for(dr3_r = 0; dr3_r < 0x14; ++dr3_r){
				if((dr3_r != (int)D(___1a1ef8h)) && ((int)s_6c[dr3_r].points > dr3_max_other))
					dr3_max_other = (int)s_6c[dr3_r].points;
			}
			D(___1de7d0h+0x50) = dr3_cash_pickup_value((int)D(___1a1028h),
			                                           (int)s_6c[D(___1a1ef8h)].rank,
			                                           (int)s_6c[D(___1a1ef8h)].points > dr3_max_other);
		}
#if defined(DR_MULTIPLAYER)	
	}

	if(___19bd60h != 0) D(___1de7d0h+0x50) = MP_MONEY/0x32;
#endif // DR_MULTIPLAYER

	___12200h();
	___12a54h();
	___24ec0h();
	___2fc50h();

#if defined(DR_MULTIPLAYER)	
	if(___19bd60h != 0){
	
		D(___1a103ch) = D(___1a1ef8h);

		if(CONNECTION_TYPE == 2) ___60719h();
	}
#endif // DR_MULTIPLAYER

	/*
	 * The chaos payouts are counted from here on (platform_3ds/dr3_payout.h and the kill money):
	 * the race's kills start at zero, so a race that ends without a report (the player leaves to the
	 * menu) adds nothing to the next race's number.
	 */
	dr3_chaos_kill_money_reset();

#if defined(DR3_HOST)
	/*
	 * The race length of a harness run (dr3_host.h: "laps <n>" / --laps n).  The chaos report - "Your
	 * Statistics" and the box under it, the rank money and the kills of the race - comes after the last
	 * lap, and twenty cars on the race's own five laps are minutes of run time; without the switch this
	 * hands back what the race set up, so a run without it is the run the menu would have started.
	 */
	NUM_OF_LAPS = dr3_host_laps(NUM_OF_LAPS);
#endif

	dr3_chaos_trace("race_main", NumCars);
	race_main(D(___1a103ch), NumCars);
	dr3_chaos_trace("race done", NumCars);

#if defined(DR_MULTIPLAYER)	
	if((___19bd60h != 0)&&(CONNECTION_TYPE == 2)) ___60705h(&___10754h);
#endif // DR_MULTIPLAYER

	D(___185a18h) = D(___196ae8h) = D(___1de7d0h+0x40+0x54*D(___1a103ch));

	edx = 0;
	n = -1;
	while(++n < 0x14){

		if((int)edx < (int)s_6c[n].points){
		
			if(n != D(___1a1ef8h)) edx = s_6c[n].points;
		}
	}

	bool_tmp = 0;
#if defined(DR_MULTIPLAYER)
	if(___19bd60h != 0) bool_tmp = 1;
#endif // DR_MULTIPLAYER

	if(((int)edx >= (int)s_6c[D(___1a1ef8h)].points)||bool_tmp){

		eax = 0;

		n = -1;
		while(++n < NumCars){

			if((n != D(___1a103ch))&&(D(___1de7d0h+0x54*n+0x10) == 100)) eax++;
		}

		if((eax == (NumCars-1))&&((int)D(___1de7d0h+0x54*D(___1a103ch)+0x10) < 100)) D(___196aa0h) = 1;

#if defined(DR_MULTIPLAYER)
		if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
			if((D(___196ae8h) != (dr3_chaos_active() ? (__DWORD__)NUM_OF_CARS : 4u))&&((int)D(___1de7d0h+0x54*D(___1a103ch)+0x10) < 3)){
			
				if(D(___185a14h_UseWeapons) == 1) D(___196a9ch) = 1;
			}
#if defined(DR_MULTIPLAYER)
		}
		else {

			if(((int)D(___1de7d0h+0x54*D(___1a103ch)+0x10) < 3)||(D(___185a14h_UseWeapons) == 1)) D(___196a9ch) = 1;
		}
#endif // DR_MULTIPLAYER

		if(D(___196ae8h) != 1){

			D(___196a98h) = 0;
		}
		else {

			D(___196a98h)++;
		}

#if defined(DR_MULTIPLAYER)
		if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
			if((int)D(___196aa4h) > 0){

				if((D(___1de7d0h+0x4c) != 1)||(D(___1de7d0h+0x4c) != D(___196ae8h))){

					D(___196aa4h) = 0-D(___196aa4h);
				}
				else {

					if((int)D(___1de7d0h+0x54*D(___1a103ch)+0x10) >= 100) D(___196aa4h) = 0-D(___196aa4h);
				}
			}
#if defined(DR_MULTIPLAYER)
		}
			
		if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
			if((int)D(___196aa8h) > 0){

#if defined(DR3_CHAOS)
				if(dr3_chaos_active()){
					int dr3_hit = -1, dr3_k;
					for(dr3_k = 0; dr3_k < (int)NUM_OF_CARS; ++dr3_k){
						if(dr3_chaos_car_seat[dr3_k] == (int)D(___196aach)){ dr3_hit = dr3_k; break; }
					}
					if(dr3_hit < 0) D(___196aa8h) = 0;
					else if(D(___1de7d0h+0x54*dr3_hit+0x10) != 100) D(___196aa8h) = 0-D(___196aa8h);
				}
				else
#endif
				{
					edx = -1;
					while(++edx < 4){

						ebx = B(___1a0ef8h+4*D(___1a1028h)+edx);
						if(ebx == D(___196aach)) eax = edx;
					}

					if(D(___1de7d0h+0x54*eax+0x10) != 100) D(___196aa8h) = 0-D(___196aa8h);
				}
			}
#if defined(DR_MULTIPLAYER)
		}
#endif // DR_MULTIPLAYER
	}

	if(s_6c[D(___1a1ef8h)].loanshark_type != -1) s_6c[D(___1a1ef8h)].loanshark_counter++;
	___243d44h = 0;

	n = -1;
	while(++n < 0x14){

		s_6c[n].spikes = 0;
		s_6c[n].rocket_fuel = 0;
		s_6c[n].sabotage = 0;
		s_6c[n].mines = 0;
	}

	___2ec68h_cdecl();
	D(___196af0h) = 0;
	D(___196aech) = 0;

	n = -1;
	while(++n < 4) s_6c[B(esp+n+0xcc)].bonus = 0;
	s_6c[D(___1a1ef8h)].bonus = 0;

	ebx = 0;
	n = -1;
	while(++n < 0x14){

		if(((int)ebx < (int)s_6c[n].points)&&(n != D(___1a1ef8h))) ebx = s_6c[n].points;
	}

#if defined(DR_MULTIPLAYER)
	if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
		/*
		 * Street cash ($) payout for the player:
		 *   - 30th Anniversary (dr3_chaos_active): ALWAYS kept even if lapped, wrecked, last place,
		 *     or leading in points (across normal races, Last Car Standing DM/Arena, and Duels).
		 *   - Classic difficulties: original Death Rally rule (forfeited if lapped or finishing 4th/last).
		 * Paid once here using s_54[0].__50 and skipped for the player inside the 4-car loop below.
		 */
		{
			const __DWORD__ dr3_last_pos = dr3_chaos_active() ? (__DWORD__)NUM_OF_CARS : 4u;
			const int dr3_me_car   = (int)D(___1a103ch);
			const int dr3_me_seat  = (int)D(___1a1ef8h);
			const int dr3_lapped   = (D(___196ab0h) != 0);
			const int dr3_is_last  = (D(___196ae8h) == dr3_last_pos);
			const int dr3_pickups  = ((dr3_me_car >= 0) && (dr3_me_car < DR3_MAX_CARS))
			                         ? (int)D(___1de7d0h + 0x54 * dr3_me_car + 0xc) : 0;
			const int dr3_unit_val = (int)D(___1de7d0h + 0x50);
			const int dr3_keep     = dr3_cash_player_keeps(dr3_chaos_active(), dr3_lapped, dr3_is_last);
			const int dr3_cash_won = dr3_keep ? dr3_cash_payout(dr3_pickups, dr3_unit_val) : 0;

			if(dr3_cash_won > 0){

				D(___196af0h) = dr3_cash_won;
				s_6c[dr3_me_seat].bonus  += dr3_cash_won;
				s_6c[dr3_me_seat].money  += dr3_cash_won;
				s_6c[dr3_me_seat].income += dr3_cash_won;
			}

			dr3_log("[dr3] cash: post-race payout: chaos=%d keep=%d pickups=%d x $%d = $%d (lapped=%d, place=%d, seat=%d, money=%d)",
			        dr3_chaos_active() ? 1 : 0, dr3_keep, dr3_pickups, dr3_unit_val, dr3_cash_won,
			        dr3_lapped, (int)D(___196ae8h), dr3_me_seat, (int)s_6c[dr3_me_seat].money);
		}

		if(((int)ebx >= (int)s_6c[D(___1a1ef8h)].points)&&(D(___196ab0h) == 0)){

			const __DWORD__ dr3_last_pos = dr3_chaos_active() ? (__DWORD__)NUM_OF_CARS : 4u;

			n = -1;
			while(++n < 4){

				int dr3_add = 0;

#if defined(DR3_CHAOS)
				if (n == dr3_boss_idx) continue;    /* guest boss has no standings seat */
#endif
				/* AI street $ pickups (player's pickups were already paid unconditionally above) */
				if((n != (int)D(___1a103ch)) && (D(___196ae8h) != dr3_last_pos)){

					dr3_add += dr3_cash_payout((int)D(___1de7d0h+0x54*n+0xc), (int)D(___1de7d0h+0x50));
				}

				/*
				 * The prize money of the race by place: the first three places only, with the amounts the
				 * signup screen shows as "PRIZE MONEY" (750/3000/12000 for the easy, medium and hard race,
				 * GLORY in the Arena).
				 *
				 * Chaos does not take this road.  A podium almost never happens in a field of twenty, and
				 * the kills already pay on their own - so chaos pays *every* place instead, and that ladder
				 * (the ladder of platform_3ds/dr3_payout.h, added below the place loop) comes *instead* of these
				 * winner of a chaos race takes $5000, not the race's own prize on top of it.  `-1` is this
				 * switch's own default, which pays nothing - one condition for the whole block.
				 */
				switch(dr3_chaos_active() ? -1 : (int)D(___1a1028h)){
				case 0:
					if((D(___1de7d0h+0x54*n+0x40) == 1)&&(D(___1de7d0h+0x54*n+0x10) != 100)){

						dr3_add += 750;
						if(n == D(___1a103ch)) D(___196aech) = 750;
					}

					if((D(___1de7d0h+0x54*n+0x40) == 2)&&(D(___1de7d0h+0x54*n+0x10) != 100)){

						dr3_add += 375;
						if(n == D(___1a103ch)) D(___196aech) = 375;
					}

					if((D(___1de7d0h+0x54*n+0x40) == 3)&&(D(___1de7d0h+0x54*n+0x10) != 100)){
						
						dr3_add += 187;
						if(n == D(___1a103ch)) D(___196aech) = 187;
					}
					break;
				case 1:
					if((D(___1de7d0h+0x54*n+0x40) == 1)&&(D(___1de7d0h+0x54*n+0x10) != 100)){

						dr3_add += 3000;
						if(n == D(___1a103ch)) D(___196aech) = 3000;
					}

					if((D(___1de7d0h+0x54*n+0x40) == 2)&&(D(___1de7d0h+0x54*n+0x10) != 100)){

						dr3_add += 1500;
						if(n == D(___1a103ch)) D(___196aech) = 1500;
					}

					if((D(___1de7d0h+0x54*n+0x40) == 3)&&(D(___1de7d0h+0x54*n+0x10) != 100)){

						dr3_add += 375;
						if(n == D(___1a103ch)) D(___196aech) = 375;
					}
					break;
				case 2:
					if((D(___1de7d0h+0x54*n+0x40) == 1)&&(D(___1de7d0h+0x54*n+0x10) != 100)){

						dr3_add += 12000;
						if(n == D(___1a103ch)) D(___196aech) = 12000;
					}

					if((D(___1de7d0h+0x54*n+0x40) == 2)&&(D(___1de7d0h+0x54*n+0x10) != 100)){

						dr3_add += 6000;
						if(n == D(___1a103ch)) D(___196aech) = 6000;
					}

					if((D(___1de7d0h+0x54*n+0x40) == 3)&&(D(___1de7d0h+0x54*n+0x10) != 100)){

						dr3_add += 1500;
						if(n == D(___1a103ch)) D(___196aech) = 1500;
					}
					break;
				default:
					break;
				}

				if(dr3_add > 0){

					s_6c[B(esp+n+0xcc)].bonus  += dr3_add;
					s_6c[B(esp+n+0xcc)].money  += dr3_add;
					s_6c[B(esp+n+0xcc)].income += dr3_add;
				}
				if(!dr3_chaos_active() && (D(___1de7d0h+0x54*n+0x40) == 1)) s_6c[B(esp+n+0xcc)].wins++;
			}//w
		}//i
#if defined(DR_MULTIPLAYER)
	}

	if(___19bd60h == 0){
#endif // DR_MULTIPLAYER

		/*
		 * The payouts of a chaos race (platform_3ds/dr3_payout.h and drally.h: dr3_chaos_kill_money).  The
		 * tiers above are the prize money of a four car race and reach three places; over a field of
		 * twenty the player's own place is usually deeper than that and would pay nothing at all, so
		 * chaos pays every place - and the report below "Your Statistics" shows that as its "Race
		 * income" line (___36adch.c).
		 *
		 * The kills of the race are that report's bonus income: their money is in racer.money and
		 * racer.income already (race___4dcach.c credited it when the wreck happened, and "Total money"
		 * and "Total income" read exactly those two fields), so the report's own number is what gets
		 * them added here - the money must not be counted a second time.
		 */
		if(dr3_chaos_active() && !dr3_boss_in_race()){

			const int cars  = (int)NUM_OF_CARS;     /* race_main.c: the grid plus the chaos extras */
			const int kills = dr3_chaos_kill_money();
			const int seat  = (int)D(___1a1ef8h);
			int       place = 0;
			int       dr3_c = 0;
			int       prize = 0;

			D(___196af0h) = (int)D(___196af0h) + kills;

			place = (int)D(___196ae8h);         /* the place the report prints as "Placing" */

			if(dr3_chaos_mode() == DR3_CHAOS_MODE_DUEL){

				/*
				 * The duel's own pot: only a win pays, and what it pays follows the rank of the rival
				 * (drally.h: DR3_CHAOS_DUEL_POT_...) - a top driver 5000, a middle one 2500, a lower one
				 * 1000.  Losing pays nothing: a duel is worth a race, not a place in the field.  The Arena
				 * is the exception it always was - "GLORY" pays no money, a duel there is for the points.
				 */
				if((D(___1a1028h) != 3)&&(place == 1)){

					const int dr3_rival      = dr3_chaos_duel_rival();
					const int dr3_rival_rank = (dr3_rival >= 0) ? (int)s_6c[dr3_rival].rank : (int)NUM_OF_CARS;

					if(dr3_rival_rank <= DR3_CHAOS_DUEL_TOP_RANK)      prize = DR3_CHAOS_DUEL_POT_TOP;
					else if(dr3_rival_rank <= DR3_CHAOS_DUEL_MID_RANK) prize = DR3_CHAOS_DUEL_POT_MID;
					else                                               prize = DR3_CHAOS_DUEL_POT_LOW;

					dr3_log("[dr3] chaos: duel: won against rank %d (%s) -> pot $%d",
					        dr3_rival_rank, (dr3_rival >= 0) ? s_6c[dr3_rival].name : "?", prize);
				}
				else dr3_log("[dr3] chaos: duel: lost (place %d of %d) -> no money", place, cars);
			}
			else if(dr3_chaos_mode() == DR3_CHAOS_MODE_DM){

				/*
				 * "Destroy 'em all": the last car standing takes the pot, everybody else gets nothing for the
				 * race itself - what his wrecks paid is in racer.money and racer.income already and the report
				 * shows it as the bonus income (drally.h).  Whether the player is that survivor is whether his
				 * own car still has HP, the very test the race itself uses (race_main.c).
				 */
				const int dr3_car   = (int)D(___1a103ch);
				const int dr3_alive = ((int)D(___1de580h + 0x94*dr3_car + 0x18) > 0);

				if(dr3_alive&&(D(___1a1028h) != 3)) prize = DR3_CHAOS_DM_POT;

				dr3_log("[dr3] chaos: deathmatch: the player %s (car %d) -> pot $%d, kills $%d",
				        dr3_alive ? "is the last car standing" : "is out", dr3_car, prize, kills);
			}
			else if(dr3_chaos_arena()){

				/*
				 * The arena (platform_3ds/dr3_arena.h): the top three survivors are paid - the place is who outlived
				 * whom (dr3_arena_final_places) - and what the wrecks the player caused paid is in racer.money and
				 * racer.income already (the report shows it as the bonus income).
				 */
				prize = dr3_arena_prize(place);

				dr3_log("[dr3] chaos: arena: the player finished place %d of %d -> prize $%d, kills $%d",
				        place, cars, prize, kills);
			}
			else if((D(___1a1028h) != 3)&&(cars > 1)){

				/*
				 * The ladder of the places (platform_3ds/dr3_payout.h): $100 per place above the last one, a
				 * jump at the podium ($4000/$3000) and nothing at all for the last place - that is the place a
				 * wrecked car ends up in, so scraping home last does not pay the way a midfield place does.
				 */
				prize = dr3_chaos_rank_money(place, cars);
			}

			if(prize > 0){

				s_6c[seat].money  += prize;
				s_6c[seat].income += prize;
				s_6c[seat].bonus  += prize;

				D(___196aech) = (int)D(___196aech) + prize;
			}

			/*
			 * The points of the race (drally.h: dr3_chaos_points).  All twenty cars are in the race, so
			 * every place scores - the tables of the results screens reach the first three places only.
			 * This runs before the standings are sorted (___3892ch.c), so the ranking list shows the new
			 * totals; the screens only draw the "+N" marks (___36068h.c and its two siblings).
			 */
			/*
			 * The winner of a deathmatch: the car that still has HP when the race is over - the survivor - or,
			 * if the race ended because the player went out first, the car that carried the most HP at that
			 * moment (that is the rule the player asked for; the race itself ends ~300 frames after his wreck,
			 * race_main.c).  Every other car keeps the place the race gave it, so the points are distributed
			 * the way every other race distributes them.
			 */
			int dr3_dm_winner = -1;

			if(dr3_chaos_mode() == DR3_CHAOS_MODE_DM){

				int dr3_m, dr3_best = -1;

				for(dr3_m = 0; dr3_m < cars; ++dr3_m){

					const int dr3_hp = (int)D(___1de580h + 0x94*dr3_m + 0x18);

					if(dr3_hp > dr3_best){ dr3_best = dr3_hp; dr3_dm_winner = dr3_m; }
				}

				dr3_log("[dr3] chaos: deathmatch: the winner is car %d (hp %d of %d car(s))",
				        dr3_dm_winner, dr3_best, cars);
			}

			for(dr3_c = 0; dr3_c < cars; ++dr3_c){

				const int dr3_place = (int)D(___1de7d0h + 0x54*dr3_c + 0x40);
				const int dr3_seat  = (int)dr3_chaos_car_seat[dr3_c];

				if((dr3_place < 1)||(dr3_place > cars)) continue;
				if((dr3_seat < 0)||(dr3_seat >= 0x14)) continue;

				/*
				 * A duel pays Elo instead of the place curve: only a win scores, and what it is worth grows
				 * with the distance of the two ranks (drally.h: DR3_CHAOS_ELO...).  The ranks are the ones
				 * the race started with - the standings are sorted after this.
				 */
				{
					int dr3_pts;

					if(dr3_chaos_mode() == DR3_CHAOS_MODE_DUEL){

						dr3_pts = 0;

						if((dr3_place == 1)&&(dr3_chaos_duel_rival() >= 0)){

							/*
							 * Symmetric 1v1 Duel Elo: whoever wins (player OR AI rival) earns points
							 * based on how far up the standings they beat their opponent.
							 */
							const int dr3_me_seat = (int)D(___1a1ef8h);
							const int dr3_loser   = (dr3_seat == dr3_me_seat) ? dr3_chaos_duel_rival() : dr3_me_seat;

							if((dr3_loser >= 0)&&(dr3_loser < 0x14)&&(dr3_loser != dr3_seat)){
								dr3_pts = dr3_chaos_duel_points((int)s_6c[dr3_seat].rank,
								                                (int)s_6c[dr3_loser].rank,
								                                DR3_CHAOS_ELO_BASE,
								                                DR3_CHAOS_ELO_STEP,
								                                DR3_CHAOS_ELO_MAX_DIFF);
							}
						}
					}
					else if(dr3_chaos_mode() == DR3_CHAOS_MODE_DM){

						/*
						 * The winner of a deathmatch takes the points of the first place - the survivor, or, when
						 * the player went out first, the car with the most HP (dr3_dm_winner, computed above).
						 * Everybody else keeps the points of the place the race gave them: the distribution is
						 * the one every other race pays, the pot ($5000) is the deathmatch's own money on top.
						 */
						dr3_pts = dr3_chaos_points(dr3_place);

						if(dr3_c == dr3_dm_winner) dr3_pts = dr3_chaos_points(1);
					}
					else dr3_pts = dr3_chaos_points(dr3_place);

					/* what the results screens print as the row's "+N" - the curve does not know Elo */
					dr3_chaos_car_gain[dr3_c] = dr3_pts;

					{
						const int dr3_was = (int)s_6c[dr3_seat].points;

						s_6c[dr3_seat].points += dr3_pts;
						if((dr3_chaos_mode() == DR3_CHAOS_MODE_DM) ? (dr3_c == dr3_dm_winner) : (dr3_place == 1))
							s_6c[dr3_seat].wins++;

						dr3_log("[dr3] chaos: %s: place %2d of %d -> +%d for %s (points %d -> %d)",
						        (dr3_chaos_mode() == DR3_CHAOS_MODE_DUEL) ? "duel" :
					        (dr3_chaos_mode() == DR3_CHAOS_MODE_DM) ? "deathmatch" : "points",
						        dr3_place, cars, dr3_pts, s_6c[dr3_seat].name, dr3_was, (int)s_6c[dr3_seat].points);
					}
				}
			}

			dr3_log("[dr3] chaos: report of the race: place %d of %d -> rank money $%d, kills $%d: "
			        "race income $%d + bonus income $%d = $%d, money %d, income %d",
			        place, cars, prize, kills, (int)D(___196aech), (int)D(___196af0h),
			        (int)D(___196aech)+(int)D(___196af0h),
			        (int)s_6c[seat].money, (int)s_6c[seat].income);
			dr3_log_flush();
		}
#if defined(DR_MULTIPLAYER)
	}

	if(___19bd60h != 0){

		D(___196ad4h)++;

		if((int)D(___196ad4h) < (int)D(___196ab8h)){

			if(D(___1de7d0h+0x44) != -1){

				strcpy(buffer, "-- ");
				strcat(buffer, ___18d492h[B(___1a2010h+D(___196ad4h))]);
				strcat(buffer, " race completed. Next race ");
				strcat(buffer, ___18d492h[B(___1a2010h+D(___196ad4h)+1)]);
				strcat(buffer, ".");
			}
			else {

				strcpy(buffer, "-- Multiplayer connection lost!");
			}
		}
		else {
		
			D(___196a80h) = 1;
			strcat(strcat(strcpy(buffer, "-- "), s_6c[D(___1a1ef8h)].name), " completed the netgame.");
		}

		dRChatbox_push(buffer, 0);
		dRChatbox_push("", 0);

		D(___196aech) = 0;
		D(___196af0h) = 0;

		n = -1;
		while(++n < (int)D(___196adch)){

			D(esp+0xd8) = MP_MONEY/50;
			s_6c[B(esp+n+0xcc)].bonus = D(___1de7d0h+0x54*n+0xc)*D(esp+0xd8);

			if(n == D(___1a103ch)) D(___196af0h) = s_6c[B(esp+n+0xcc)].bonus;

			if(D(___1de7d0h+0x54*n+0x40) == 1){

				D(esp+0xdc) = 6*MP_MONEY/20;
				s_6c[B(esp+n+0xcc)].bonus += D(esp+0xdc);
				if(n == D(___1a103ch)) D(___196aech) = D(esp+0xdc);
				s_6c[B(esp+n+0xcc)].points += 6;
			}

			if(D(___1de7d0h+0x54*n+0x40) == 2){

				D(esp+0xe0) = 5*MP_MONEY/20;
				s_6c[B(esp+n+0xcc)].bonus = D(esp+0xe0);
				if(n == D(___1a103ch)) D(___196aech) = D(esp+0xe0);
				s_6c[B(esp+n+0xcc)].points += 4;
			}

			if(D(___1de7d0h+0x54*n+0x40) == 3){

				D(esp+0xe4) = 4*MP_MONEY/20;
				s_6c[B(esp+n+0xcc)].bonus += D(esp+0xe4);
				if(n == D(___1a103ch)) D(___196aech) = D(esp+0xe4);
				s_6c[B(esp+n+0xcc)].points += 2;
			}

			if(D(___1de7d0h+0x54*n+0x40) == 4){

				D(esp+0xe8) = 3*MP_MONEY/20;
				s_6c[B(esp+n+0xcc)].bonus += D(esp+0xe8);
				if(n == D(___1a103ch)) D(___196aech) = D(esp+0xe8);
			}

			s_6c[B(esp+n+0xcc)].money += s_6c[B(esp+n+0xcc)].bonus;
			s_6c[B(esp+n+0xcc)].income += s_6c[B(esp+n+0xcc)].bonus;

			if(D(___1de7d0h+0x54*n+0x40) == 1) s_6c[B(esp+n+0xcc)].wins++;
		}//w
	}//i
#endif // DR_MULTIPLAYER

	edx = 0;
	n = -1;
	while(++n < 0x14){

		if(((int)edx < (int)s_6c[n].points)&&(n != D(___1a1ef8h))) edx = s_6c[n].points;
	}

#if defined(DR_MULTIPLAYER)
	if(___19bd60h == 0){
#endif // DR_MULTIPLAYER

		if(dr3_chaos_active() ? (D(___1a1028h) == 3) : ((int)edx < (int)s_6c[D(___1a1ef8h)].points)){
		
			s_6c[D(___1a1ef8h)].damage = D(___1de7d0h+0x54+0x10);
			s_6c[D(___1a1ef8h)].races++;
		}
		else {

#if defined(DR3_CHAOS)
			if(dr3_chaos_active()){
				int dr3_c;
				for(dr3_c = 0; dr3_c < (int)NUM_OF_CARS; ++dr3_c){
					const int dr3_seat = dr3_chaos_car_seat[dr3_c];
					if((dr3_seat < 0) || (dr3_seat >= 0x14)) continue;
					s_6c[dr3_seat].damage = D(___1de7d0h+0x54*dr3_c+0x10);
					s_6c[dr3_seat].races++;
				}
			}
			else
#endif
			{
				n = -1;
				while(++n < NumCars){

					s_6c[B(esp+n+0xcc)].damage = D(___1de7d0h+0x54*n+0x10);
					s_6c[B(esp+n+0xcc)].races++;
				}
			}
		}
#if defined(DR_MULTIPLAYER)
	}
	else {

		n = -1;
		while(++n < NumCars){

			s_6c[B(esp+n+0xcc)].damage = D(___1de7d0h+0x54*n+0x10);
			s_6c[B(esp+n+0xcc)].races++;
		}
	}
	
	if(___19bd60h == 0){
#endif // DR_MULTIPLAYER
		___3deb8h();
#if defined(DR_MULTIPLAYER)
	}
#endif // DR_MULTIPLAYER

	___117d4h();	// Allocate VESA101 Screen
	___117f4h();

	edx = 0;
	n = -1;
	while(++n < 0x14){

		if(((int)edx < (int)s_6c[n].points)&&(n != D(___1a1ef8h))) edx = s_6c[n].points;
	}

	bool_tmp = 0;
#if defined(DR_MULTIPLAYER)
	if(___19bd60h != 0) bool_tmp = 1;
#endif // DR_MULTIPLAYER

	/*
	 * In 30th Anniversary (dr3_chaos_active) points were already awarded above, so winning a regular race
	 * that puts the player into #1 MUST NOT look like beating the Final Boss Challenge - only D(___1a1028h) == 3
	 * is the Final Challenge against the Adversary.
	 */
	if((dr3_chaos_active() ? ((D(___1a1028h) != 3) || (dr3_boss_in_race() && (dr3_boss_phase() < DR3_BOSS_PHASE_3)))
	                       : ((int)edx >= (int)s_6c[D(___1a1ef8h)].points))||((D(___196ae8h) != 1)||bool_tmp)){

		___11378h_cdecl(
			B(___1a0fb8h+3*s_6c[D(___1a1ef8h)].color),
			B(___1a0fb8h+3*s_6c[D(___1a1ef8h)].color+1),
			B(___1a0fb8h+3*s_6c[D(___1a1ef8h)].color+2));

		dRally_Sound_load(1, "MEN-MUS.CMF", 2, "MEN-SAM.CMF", 5);
		dRally_Sound_setMusicVolume(___24cc58h_msx_volume);
		dRally_Sound_setSampleRate(0x5622);
		dRally_Sound_play();
		___606dfh();
		__VESA101_SETMODE();
		___605deh_cdecl(0x46, 0);
	}

	___12940h();

	n = -1;
	while(++n < 0x100) __DISPLAY_SET_PALETTE_COLOR(0, 0, 0, n);

	dr3_chaos_trace("palette down", 0);

#if defined(DR_MULTIPLAYER)
	if(___19bd60h == 0){
#endif // DR_MULTIPLAYER

		edx = 0;
	
		n = -1;
		while(++n < 0x14){

			if(((int)edx < (int)s_6c[n].points)&&(n != D(___1a1ef8h))) edx = s_6c[n].points;
		}

		/*
		 * Final Challenge branch: in 30th Anniversary (dr3_chaos_active), only D(___1a1028h) == 3 is the
		 * Final Boss Challenge (since regular races already awarded points above and can push the player to #1).
		 */
		if(dr3_chaos_active() ? (D(___1a1028h) == 3)
		                      : (((int)edx < (int)s_6c[D(___1a1ef8h)].points) ||
		                         (dr3_adversary_active() && (NumCars == 2) && (D(___196ae8h) == 1)))){

			___31588h();

			dr3_chaos_trace("next event", 0);

#if defined(DR3_CHAOS)
			/*
			 * The signup below fills the field of the next event.  Its loop is unbounded in the original,
			 * and a chaos race can leave the racer pool without a candidate the loop accepts: that is the
			 * black screen freeze (the palette was set to black a few lines above).  The chaos build
			 * gives up after a while and logs what it had, so the game stays playable.
			 */
			{
				int dr3_passes = 0;

				while(1){

					___3079ch_cdecl(1);

					if((4 <= B(___1a1f64h+3))&&(4 <= B(___1a1f64h+4))&&(4 <= B(___1a1f64h+5))) break;

					if(++dr3_passes >= 200){

						dr3_log("[dr3] chaos: signup stuck after %d passes, field %d,%d,%d",
						        dr3_passes, B(___1a1f64h+3), B(___1a1f64h+4), B(___1a1f64h+5));
						break;
					}
				}
			}
#else
			while(1){
				
				___3079ch_cdecl(1);
				
				if((4 <= B(___1a1f64h+3))&&(4 <= B(___1a1f64h+4))&&(4 <= B(___1a1f64h+5))) break;
			}
#endif

			memset(esp+0x4c, 0, 4);

			n = 0;
			while(n < 4){

				D(___1de7d0h+0x54*n+0x40) = (rand_watcom106()%4)+1;

				if(B(esp+D(___1de7d0h+0x54*n+0x40)+0x4b) == 0){
					
					B(esp+D(___1de7d0h+0x54*n+0x40)+0x4b) = 1;

					if(++n == 4){
						
						/*
						 * ___196ae8h is this race's finishing position.  "30th Anniversary" (see doc/3ds.md):
						 * on that level the championship is not won by merely winning a race while leading -
						 * the anniversary has to be beaten in the final challenge first, and that duel is the
						 * only two-car race there is.
						 */
						if((D(___196ae8h) == 1) && (!dr3_adversary_active() || (NumCars == 2))
#if defined(DR3_CHAOS)
						   && (!dr3_boss_in_race() || (dr3_boss_phase() == DR3_BOSS_PHASE_3))
#endif
						){

							// after beating adversary
							___22808h();
						}
#if defined(DR3_CHAOS)
						else if(dr3_boss_in_race()){
							/*
							 * Boss Phase 1/2 clear or any phase defeat skips ___3892ch_cdecl(0), so reload
							 * the three menu/shop asset pools freed before race_main (___12200h/___24ec0h/___2fc50h)
							 * before returning to ___31008h.c for the notice box and Shop Pit Stop!
							 */
							dr3_chaos_trace("boss phase over", dr3_boss_phase());
							___1240ch();
							___24548h();
							___2faf0h();
							D(___196a74h) = 1;
						}
#endif
						else {

              dr3_chaos_trace("race over", 0);

							___3892ch_cdecl(0);
							D(___185a20h) = 1;
						}
					}
				}
			}
		}
		else {

			dr3_chaos_trace("standings", 0);
			___3892ch_cdecl(0);
		}
#if defined(DR_MULTIPLAYER)
	}
	else {

		___3986ch();

		if(D(___196a80h) != 0) ___1e4f8h();
	}
#endif // DR_MULTIPLAYER
}
