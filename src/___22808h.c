#include "drally.h"
#include "drally_fonts.h"
#include "drally_structs_fixed.h"
#include "platform_3ds/dr3_chaos.h"
#include "platform_3ds/dr3_boss.h"
#include "platform_3ds/dr3_log.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#pragma pack(1)
typedef struct x655_s {
	__DWORD__ 	r;
	__DWORD__ 	g;
	__DWORD__ 	b;
} x655_t;

	extern x655_t ___19eb50h[0x100];

	extern __BYTE__ ___1a0d60h[];
	extern __BYTE__ ___1a1ef8h[];
	extern __BYTE__ ___1a01e0h[];
	extern __POINTER__ ___1a0fb8h;
	extern __POINTER__ ___1a1138h__VESA101_BACKGROUND;
	extern __POINTER__ ___1a112ch__VESA101_ACTIVESCREEN_PTR;
	extern __POINTER__ ___1a1100h__VESA101h_DefaultScreenBuffer;
	extern __DWORD__ ___24cc58h_msx_volume;

void ___10b80h_cdecl(const char *, __DWORD__, const char *, __DWORD__, const char *, __DWORD__, __DWORD__);
void ___606dfh(void);
void ___605deh_cdecl(__DWORD__, __DWORD__);
void ___3d890h(void);
void ___1240ch(void);
void ___24548h(void);
void ___2faf0h(void);
void ___3d2bch(void);
void ___12940h(void);
void ___11564h_cdecl(__DWORD__, __DWORD__, __DWORD__);
void ___13248h_cdecl(__DWORD__, __DWORD__, __DWORD__, __DWORD__, __DWORD__);
void ___12cb8h__VESA101_PRESENTSCREEN(void);
void ___2ab50h(void);
__BYTE__ ___5994ch(void);
__BYTE__ ___59b3ch(void);
int rand_watcom106(void);
void ___223c4h(void);
void ___2b318h(void);
void ___58c60h(void);
void dRally_Sound_setMasterVolume(__DWORD__ vol);
void __DISPLAY_SET_PALETTE_COLOR(__DWORD__ b, __DWORD__ g, __DWORD__ r, __DWORD__ n);
void dRally_Sound_release(void);
void dRally_Sound_load(__DWORD__ msx_t, const char * msx_f, __DWORD__ sfx_t, const char * sfx_f, __DWORD__ num_ch);
void dRally_Sound_setMusicVolume(__DWORD__ vol);
void dRally_Sound_setSampleRate(__DWORD__ freq);
void dRally_Sound_play(void);

void __VESA101_SETMODE(void);
__DWORD__ GET_FILE_SIZE(const char *);

#if defined(DR3_CHAOS)
typedef struct {
	float x, y, vx, vy;
	int   life, max_life;
	__BYTE__ col;
} dr3_fw_spark_t;

static __BYTE__ dr3_pick_pal(int tr, int tg, int tb)
{
	int best_i = 1, best_d = 0x7fffffff, k;

	for (k = 1; k < 0x100; ++k) {
		const int r = (int)((100u * ___19eb50h[k].r + 0x8000u) >> 16);
		const int g = (int)((100u * ___19eb50h[k].g + 0x8000u) >> 16);
		const int b = (int)((100u * ___19eb50h[k].b + 0x8000u) >> 16);
		const int dr = r - tr, dg = g - tg, db = b - tb;
		const int d  = dr * dr + dg * dg + db * db;
		if (d < best_d) { best_d = d; best_i = k; }
	}
	return (__BYTE__)best_i;
}

static void dr3_draw_centered(int font_code, int props, const char *txt, int y)
{
	int x = 320 - getTextWidth(props, txt) / 2;
	if (x < 48) x = 48;
	___12e78h_v3(font_code, txt, x, y);
}

static void dr3_boss_victory_celebration(void)
{
	racer_t       *s_6c = (racer_t *)___1a01e0h;
	const int      me   = (int)D(___1a1ef8h);
	const char    *name = ((me >= 0) && (me < 20) && s_6c[me].name[0]) ? s_6c[me].name : "CHAMPION";
	const int      races = ((me >= 0) && (me < 20)) ? (int)s_6c[me].races : 0;
	const int      wins  = ((me >= 0) && (me < 20)) ? (int)s_6c[me].wins  : 0;
	const int      money = ((me >= 0) && (me < 20)) ? (int)s_6c[me].money : 0;
	char           sub_line[80], stats_line[80];
	__BYTE__       fw_cols[5];
	dr3_fw_spark_t sparks[128];
	int            frame = 0, next_burst = 4, i, k, n;

	memset(sparks, 0, sizeof(sparks));
	fw_cols[0] = dr3_pick_pal(63, 56, 14); /* bright gold */
	fw_cols[1] = dr3_pick_pal(63, 30,  6); /* blazing orange */
	fw_cols[2] = dr3_pick_pal(63, 63, 63); /* pure white */
	fw_cols[3] = dr3_pick_pal(63, 12, 12); /* crimson */
	fw_cols[4] = dr3_pick_pal(22, 58, 63); /* electric cyan */

	snprintf(sub_line,   sizeof(sub_line),   "%s - UNDISPUTED KING OF THE ARENA", name);
	snprintf(stats_line, sizeof(stats_line), "RACES: %d    WINS: %d    FINAL BANK: $%d", races, wins, money);

	___1a112ch__VESA101_ACTIVESCREEN_PTR = ___1a1100h__VESA101h_DefaultScreenBuffer;
	___5994ch();
	___59b3ch();

	dr3_log("[dr3] boss: entering 30th Anniversary Champion Celebration screen (%s)", name);

	while (1) {
		__BYTE__ *fb = (__BYTE__ *)___1a1100h__VESA101h_DefaultScreenBuffer;

		/* Launch vibrant twin firework bursts around the victory plaque */
		if (frame >= next_burst) {
			const int zones[8][2] = {
				{ 78, 128 }, { 562, 128 }, { 64, 260 }, { 576, 260 },
				{ 78, 392 }, { 562, 392 }, { 200, 448 }, { 440, 448 }
			};
			int b;
			for (b = 0; b < 2; ++b) {
				const int z  = (frame / 13 + b * 3 + rand_watcom106()) % 8;
				const int cx = zones[z][0] + (rand_watcom106() % 33) - 16;
				const int cy = zones[z][1] + (rand_watcom106() % 25) - 12;
				const __BYTE__ base_col = fw_cols[(rand_watcom106() + b) % 5];
				int spawned = 0;

				for (i = 0; (i < 128) && (spawned < 24); ++i) {
					if (sparks[i].life > 0) continue;
					{
						const float ang = (6.283185f * (float)spawned) / 24.0f + ((float)(rand_watcom106() % 100) * 0.002f);
						const float spd = 1.6f + ((float)(rand_watcom106() % 100) * 0.036f);
						sparks[i].x        = (float)cx;
						sparks[i].y        = (float)cy;
						sparks[i].vx       = cosf(ang) * spd;
						sparks[i].vy       = sinf(ang) * spd - 0.55f;
						sparks[i].max_life = 30 + (rand_watcom106() % 22);
						sparks[i].life     = sparks[i].max_life;
						sparks[i].col      = (spawned % 3 == 0) ? fw_cols[2] : base_col;
						++spawned;
					}
				}
			}
			next_burst = frame + 14 + (rand_watcom106() % 10);
		}

		/* Base menu background + crisp dark celebration plaque */
		memcpy(fb, ___1a1138h__VESA101_BACKGROUND, 0x4b000);
		___13248h_cdecl(36, 98, 568, 336, 1);

		/* Update & draw vibrant 4x4 glowing firework starbursts with bright cores + trails */
		for (i = 0; i < 128; ++i) {
			if (sparks[i].life <= 0) continue;
			{
				const int ox = (int)sparks[i].x;
				const int oy = (int)sparks[i].y;
				sparks[i].x  += sparks[i].vx;
				sparks[i].y  += sparks[i].vy;
				sparks[i].vx *= 0.965f;
				sparks[i].vy  = sparks[i].vy * 0.965f + 0.068f;
				sparks[i].life--;

				{
					const int px = (int)sparks[i].x;
					const int py = (int)sparks[i].y;
					const int sz = (sparks[i].life > sparks[i].max_life / 3) ? 4 : 2;
					if ((px >= 4) && (px < 635) && (py >= 86) && (py < 474)) {
						int dy, dx;
						for (dy = 0; dy < sz; ++dy)
							for (dx = 0; dx < sz; ++dx)
								fb[640 * (py + dy) + (px + dx)] = sparks[i].col;
						if (sz == 4) {
							fb[640 * (py + 1) + (px + 1)] = fw_cols[2];
							fb[640 * (py + 1) + (px + 2)] = fw_cols[2];
						}
					}
					if ((ox >= 2) && (ox < 637) && (oy >= 86) && (oy < 476)) {
						fb[640 * oy + ox] = sparks[i].col;
						fb[640 * oy + ox + 1] = sparks[i].col;
					}
				}
			}
		}

		/* Staggered dramatic triumph text reveal */
		if (frame >= 6)   dr3_draw_centered(___1a10cch___185ba9h, FONTPROPS01, "30TH ANNIVERSARY CHAMPION!", 118);
		if (frame >= 24)  dr3_draw_centered(___1a1108h___185c0bh, FONTPROPS02, sub_line, 156);
		if (frame >= 48)  dr3_draw_centered(___1a1108h___185c0bh, FONTPROPS02, "PHASE 1: THE SWARM     - 20 VAGABONDS CRUSHED", 196);
		if (frame >= 74)  dr3_draw_centered(___1a1108h___185c0bh, FONTPROPS02, "PHASE 2: THE ESCORT    - 11 ELITES OUTRACED", 220);
		if (frame >= 100) dr3_draw_centered(___1a1108h___185c0bh, FONTPROPS02, "FINAL PHASE: ADVERSARY - GIANT TITAN DESTROYED", 244);
		if (frame >= 130) dr3_draw_centered(___1a1108h___185c0bh, FONTPROPS02, stats_line, 286);
		if (frame >= 152) dr3_draw_centered(___1a10cch___185ba9h, FONTPROPS01, "HAIL TO THE KING!", 324);
		if (frame >= 168) dr3_draw_centered(___1a1108h___185c0bh, FONTPROPS02, "YOU CONQUERED 30 YEARS OF DEATH RALLY!", 360);
		if ((frame >= 180) && (((frame / 22) & 1) == 0))
			dr3_draw_centered(___1a1108h___185c0bh, FONTPROPS02, "PRESS ANY KEY FOR THE HALL OF FAME", 402);

		___12cb8h__VESA101_PRESENTSCREEN();

		/* Smooth fade-in over the first 25 frames */
		if (frame <= 25) {
			const int pct = (frame * 100) / 25;
			for (n = 0; n < 0x100; ++n) {
				const __DWORD__ rr = ((pct * ___19eb50h[n].r + 0x8000u) >> 16) & 0xff;
				const __DWORD__ gg = ((pct * ___19eb50h[n].g + 0x8000u) >> 16) & 0xff;
				const __DWORD__ bb = ((pct * ___19eb50h[n].b + 0x8000u) >> 16) & 0xff;
				__DISPLAY_SET_PALETTE_COLOR(bb, gg, rr, n);
			}
		}

		___2ab50h();
		++frame;

		if ((frame > 75) && (___5994ch() != 0)) break;
	}

	/* Fade cleanly back to black so ___223c4h() (Hall of Fame) fades in seamlessly */
	for (k = 0; k < 26; ++k) {
		const int pct = 100 - 4 * k;
		___58c60h();
		for (n = 0; n < 0x100; ++n) {
			const __DWORD__ rr = ((pct * ___19eb50h[n].r + 0x8000u) >> 16) & 0xff;
			const __DWORD__ gg = ((pct * ___19eb50h[n].g + 0x8000u) >> 16) & 0xff;
			const __DWORD__ bb = ((pct * ___19eb50h[n].b + 0x8000u) >> 16) & 0xff;
			__DISPLAY_SET_PALETTE_COLOR(bb, gg, rr, n);
		}
	}
	___5994ch();
	___59b3ch();
}
#endif

void ___22808h(void){

	__DWORD__ 	rr, gg, bb;
	__DWORD__ 	eax, ebx, ecx, edx, edi, ebp;
	__POINTER__ 	esi;
	__BYTE__ 	esp[0x70];
	int 		i, k, n;
	racer_t * 	s_6c;

	s_6c = (racer_t *)___1a01e0h;
	strcat(strcpy(esp, ___1a0d60h), "endani.haf");
	if(GET_FILE_SIZE(esp) != 0) ___10b80h_cdecl("endani.haf", 1, "tr0-mus.cmf", 2, "endani-e.cmf", 1, 0x78);

	___606dfh();
	__VESA101_SETMODE();
	___605deh_cdecl(70, 0);
	___3d890h();		// Title Screen
	___1240ch();
	___24548h();
	___2faf0h();
	___3d2bch();
	___12940h();
	esi = ___1a0fb8h+3*s_6c[D(___1a1ef8h)].color;
	___11564h_cdecl(B(esi), B(esi+1), B(esi+2));
#if defined(DR3_CHAOS)
	if(dr3_chaos_active()) dr3_boss_victory_celebration();
#endif
	___223c4h();
	___2b318h();

	k = -1;
	while(++k < 51){

		___58c60h();	// wait 1 frame
		i = 100-2*k;
		dRally_Sound_setMasterVolume(655*i);

		n = -1;
		while(++n < 0x100){

			rr = ((i*___19eb50h[n].r+0x8000)>>0x10)&0xff;
			gg = ((i*___19eb50h[n].g+0x8000)>>0x10)&0xff;
			bb = ((i*___19eb50h[n].b+0x8000)>>0x10)&0xff;
			__DISPLAY_SET_PALETTE_COLOR(bb, gg, rr, n);
		}
	}

	dRally_Sound_release();
	dRally_Sound_load(1, "MEN-MUS.CMF", 2, "MEN-SAM.CMF", 5);
	dRally_Sound_setMusicVolume(___24cc58h_msx_volume);
	dRally_Sound_setSampleRate(0x5622);
	dRally_Sound_play();
}
