#include "drally.h"
#include "drally_fonts.h"
#include "drally_keyboard.h"
#include "drally_structs_fixed.h"
#include "sfx.h"
#include "platform_3ds/dr3_chaos.h"
#include "platform_3ds/dr3_boss.h"
#include "platform_3ds/dr3_log.h"

#pragma pack(1)
typedef struct x655_s {
	__DWORD__ 	r;
	__DWORD__ 	g;
	__DWORD__ 	b;
} x655_t;

	extern x655_t ___19eb50h[0x100];

	extern __POINTER__ ___1a112ch__VESA101_ACTIVESCREEN_PTR;
	extern __POINTER__ ___1a10e4h__VESA101h_DefaultScreenBufferA;
	extern __POINTER__ ___1a1100h__VESA101h_DefaultScreenBuffer;
	extern __POINTER__ ___1a1138h__VESA101_BACKGROUND;
	extern __POINTER__ ___1a0f90h;
	extern __POINTER__ ___1a0f88h;
	extern __POINTER__ ___1a0f84h;
	extern __BYTE__ ___1a1028h[];
	extern __BYTE__ ___1a103ch[];
	extern __BYTE__ ___196af0h[];
	extern __BYTE__ ___196aech[];
	extern __BYTE__ ___196ae8h[];
	extern __BYTE__ ___1866b8h[];
	extern __BYTE__ ___185a48h[];
	extern __BYTE__ ___185a44h[];
	extern __BYTE__ ___185a40h[];
	extern __BYTE__ ___185a3ch[];
	extern __BYTE__ ___185b58h[];
	extern __BYTE__ ___1866b8h[];
	extern __DWORD__ ___243d44h;
	extern __BYTE__ ___185a5ch[];
	extern __BYTE__ ___185a2ch[];
	extern __DWORD__ ___24cc54h_sfx_volume;
	extern __BYTE__ ___185a14h_UseWeapons[];
	extern __BYTE__ ___185a4ch[];
	extern __BYTE__ ___1a1ef4h[];
	extern __BYTE__ ___185a28h[];
	extern __BYTE__ ___1a1f64h[];
	extern __BYTE__ ___1de7d0h[];
	extern __BYTE__ ___1a1ef8h[];
	extern __BYTE__ ___1a01e0h[];
#if defined(DR_MULTIPLAYER)
	extern __DWORD__ ___19bd60h;
#endif // DR_MULTIPLAYER
	extern __BYTE__ ___1a0a50h[];
	extern __BYTE__ ___185a20h[];

void ___3892ch_cdecl(__DWORD__);
int rand_watcom106(void);
void ___3079ch_cdecl(__DWORD__);
__BYTE__ dRally_Sound_setPosition(__DWORD__ pos_n);
void dRally_Sound_freeEffectChannel(__BYTE__ ch_num);
void dRally_Sound_setMasterVolume(__DWORD__ vol);
void dRally_Sound_pushEffect(__BYTE__ channel, __BYTE__ n, __DWORD__ unk, __DWORD__ a0, __DWORD__ a1, __DWORD__ a2);
void ___12940h(void);
void ___58c60h(void);
void ___2b318h(void);
void ___2415ch(void);
void ___3a6a4h_v2(int);
void ___12cb8h__VESA101_PRESENTSCREEN(void);
void ___2ab50h(void);
__BYTE__ ___5994ch(void);
void ___33010h_cdecl(int NumCars);
void ___135fch(__DWORD__, __DWORD__, __DWORD__, __DWORD__);
void ___23230h(void);
void ___25330h(void);
void ___27f80h_cdecl(__DWORD__, __DWORD__, __DWORD__, __DWORD__);
void ___13248h_cdecl(__DWORD__, __DWORD__, __DWORD__, __DWORD__, __DWORD__);
void __DISPLAY_SET_PALETTE_COLOR(__DWORD__ b, __DWORD__ g, __DWORD__ r, __DWORD__ n);

static __BYTE__ helper_color(__DWORD__ eax, __DWORD__ edx){

	eax = edx / 0x10000 * eax;
	eax += 0x8000;
	eax= (int)eax>>0x10;

	return eax&0xff;
}

#if defined(DR3_CHAOS)
static void dr3_boss_notice(const char *title, const char *l1, const char *l2)
{
	int k, p, step;
	int xt, x1, x2, xp;
	const char *prompt = "Press any key to continue.";

	/*
	 * ___33010h.c blacks out all 256 palette entries before returning: draw the notice onto the
	 * active default VESA buffer, present it, and smoothly fade the full menu palette (___19eb50h)
	 * back in to 100% brightness (0x640000) so the text is razor-sharp and unmistakable.
	 */
	___1a112ch__VESA101_ACTIVESCREEN_PTR = ___1a1100h__VESA101h_DefaultScreenBuffer;
	memcpy(___1a112ch__VESA101_ACTIVESCREEN_PTR, ___1a1138h__VESA101_BACKGROUND, 0x4b000);

	___13248h_cdecl(50, 152, 540, 174, 1);

	xt = 320 - getTextWidth(FONTPROPS01, title) / 2;
	x1 = 320 - getTextWidth(FONTPROPS02, l1) / 2;
	x2 = 320 - getTextWidth(FONTPROPS02, l2) / 2;
	xp = 320 - getTextWidth(FONTPROPS02, prompt) / 2;
	if (xt < 64) xt = 64;
	if (x1 < 64) x1 = 64;
	if (x2 < 64) x2 = 64;
	if (xp < 64) xp = 64;

	___12e78h_v3(___1a10cch___185ba9h, title,  xt, 174);
	___12e78h_v3(___1a1108h___185c0bh, l1,     x1, 218);
	___12e78h_v3(___1a1108h___185c0bh, l2,     x2, 244);
	___12e78h_v3(___1a1108h___185c0bh, prompt, xp, 286);
	___12cb8h__VESA101_PRESENTSCREEN();

	/* Fade full 256-colour menu palette up to 100% brightness (0x640000) */
	for (step = 0x080000; step <= 0x640000; step += 0x080000) {
		const __DWORD__ sc = (step > 0x640000) ? 0x640000 : (__DWORD__)step;
		___58c60h();
		for (p = 0; p < 0x100; ++p) {
			__DISPLAY_SET_PALETTE_COLOR(
			    helper_color(___19eb50h[p].b, sc),
			    helper_color(___19eb50h[p].g, sc),
			    helper_color(___19eb50h[p].r, sc),
			    p);
		}
	}
	for (p = 0; p < 0x100; ++p) {
		__DISPLAY_SET_PALETTE_COLOR(
		    helper_color(___19eb50h[p].b, 0x640000),
		    helper_color(___19eb50h[p].g, 0x640000),
		    helper_color(___19eb50h[p].r, 0x640000),
		    p);
	}
	dRally_Sound_setMasterVolume(0x10000);
	___12cb8h__VESA101_PRESENTSCREEN();

	for (k = 0; k < 20; ++k) ___2ab50h();
	___5994ch();
	while (1) {
		___2ab50h();
		if (___5994ch() != 0) break;
	}
}
#endif

// FINAL CHALLENGE SIGNUP
void ___31008h(void){

	__DWORD__ 	eax, ebx, ecx, edx, edi, esi, ebp, rr, gg, bb, nn, p5;
	__BYTE__ 	esp[0x30];
	__BYTE__ 	px;
	int 		i, j, n;
	racer_t * 	s_6c;


	s_6c = (racer_t *)___1a01e0h;
	D(esp+0x28) = 0;

	while(1){

		nn = D(esp+0x28)+0xb0;
		rr = helper_color(___19eb50h[D(esp+0x28)+0xb0].r, 0x640000);
		gg = helper_color(___19eb50h[D(esp+0x28)+0xb0].g, 0x640000);
		bb = helper_color(___19eb50h[D(esp+0x28)+0xb0].b, 0x640000);
		__DISPLAY_SET_PALETTE_COLOR(bb, gg, rr, nn);
		D(esp+0x28)++;
		if((int)D(esp+0x28) >= 7) break;
	}

	memcpy(___1a10e4h__VESA101h_DefaultScreenBufferA, ___1a112ch__VESA101_ACTIVESCREEN_PTR, 0x4b000);
	___1a112ch__VESA101_ACTIVESCREEN_PTR = ___1a10e4h__VESA101h_DefaultScreenBufferA;
	memcpy(___1a112ch__VESA101_ACTIVESCREEN_PTR+0xe100, ___1a1138h__VESA101_BACKGROUND+0xe100, 0x2aa80);
	memcpy(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x39580, ___1a1138h__VESA101_BACKGROUND+0x39580, 0x2800);
	___135fch(0, 0x173, 0x27f, 0x6d);
	___23230h();
	___25330h();

	j = -1;
	while(++j < 0x10){

		i = -1;
		while(++i < 0x1b8){

			if((px = B(___1a0f90h+0x1b8*j+i)) != 0) B(___1a112ch__VESA101_ACTIVESCREEN_PTR+0xe628+0x280*j+i) = px;
		}
	}

	j = -1;
	while(++j < 0xe4){

		i = -1;
		while(++i < 0x100){

			if((px = B(___1a0f88h+0x100*j+i)) != 0) B(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x140b0+0x280*j+i) = px;
		}
	}

	j = -1;
	while(++j < 0x68){

		i = -1;
		while(++i < 0x73){

			if((px = B(___1a0f84h+0x73*j+i)) != 0) B(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x25d1c+0x280*j+i) = px;
		}
	}

	ecx = 0xf4;
	ebx = 0x10c;
	edx = 0x78;
	___27f80h_cdecl(0xaa, edx, ebx, ecx);

#if defined(DR3_CHAOS)
	if(dr3_chaos_active()){
		int p = dr3_boss_phase();
		if(p == DR3_BOSS_PHASE_NONE){
			p = DR3_BOSS_PHASE_1;
			dr3_boss_set_phase(p);
		}
		/* Top title bar inside the y=90..399 active menu band (above THE ARENA frame at y=128) */
		___13248h_cdecl(24, 91, 592, 32, 1);
		___12e78h_v3(___1a10cch___185ba9h, dr3_boss_phase_title(p), 42, 98);
		/* Bottom info bar inside y=358..397 (below THE ARENA frame ending at y=356) */
		___13248h_cdecl(24, 358, 592, 39, 1);
		___12e78h_v3(___1a1108h___185c0bh, dr3_boss_phase_subtitle(p), 42, 364);
		___12e78h_v3(___1a1108h___185c0bh,
		             (p >= DR3_BOSS_PHASE_2) ? "ENTER: START PHASE   ESC: BACK TO SHOP"
		                                     : "ENTER: BEGIN GAUNTLET   ESC: RETREAT",
		             42, 380);
		dr3_log("[dr3] boss: challenge screen showing %s (%s)", dr3_boss_phase_title(p), dr3_boss_phase_subtitle(p));
	}
#endif

	___3a6a4h_v2(1);

	L(eax) = 0;
	n = -1;
	while(++n < 0x1e0){

		if((L(eax) == DR_SCAN_ENTER)||(L(eax) == DR_SCAN_KP_ENTER)||(L(eax) == DR_SCAN_ESCAPE)) break;
		___2ab50h();
		L(eax) = ___5994ch();
	}

	if(L(eax) == DR_SCAN_ESCAPE){ // CHICKEN OUT (or Pit Stop back to shop)
#if defined(DR3_CHAOS)
		if(dr3_chaos_active() && (dr3_boss_phase() >= DR3_BOSS_PHASE_2)){
			/* During Pit Stop (Phase 2 or 3), ESC returns cleanly to Shop so player can upgrade/save */
			dr3_log("[dr3] boss: pit stop ESC - returning to shop with phase %d preserved", dr3_boss_phase());
			D(___185a20h) = 1;
			return;
		}
		if(dr3_chaos_active()){
			dr3_boss_reset();
			dr3_boss_save_sync();
		}
#endif

		dRally_Sound_pushEffect(0x1, SFX_LAUGHTER, 0, ___24cc54h_sfx_volume, 0x25500-0x1000, 0x8000);
		___2b318h();
		ebp = 0x640000;
		D(esp+0x2c) = 0xffdc;

		while(1){

			if((D(___185a14h_UseWeapons) != 0)&&(D(___185a4ch) != 0)) dRally_Sound_setMasterVolume(D(esp+0x2c));

			___58c60h();
			esi = ebp;
			D(esp+0x28) = 0;

			while(1){

				nn = D(esp+0x28);
				rr = helper_color(___19eb50h[D(esp+0x28)].r, esi);
				gg = helper_color(___19eb50h[D(esp+0x28)].g, esi);
				bb = helper_color(___19eb50h[D(esp+0x28)].b, esi);
				__DISPLAY_SET_PALETTE_COLOR(bb, gg, rr, nn);
				D(esp+0x28)++;
				if((int)D(esp+0x28) >= 0x100) break;
			}

			edx = D(esp+0x2c);
			edx -= 0x51e;
			ebp -= 0x20000;
			D(esp+0x2c) = edx;
			if(ebp == 0xfffe0000) break;
		}

		if((D(___185a14h_UseWeapons) != 0)&&(D(___185a4ch) != 0)){

			dRally_Sound_setPosition(D(___1a1ef4h));
			D(___185a28h) = 1;
			dRally_Sound_freeEffectChannel(1);
		}

		___12940h();

		D(esp+0x28) = 0;
		while(1){

			__DISPLAY_SET_PALETTE_COLOR(0, 0, 0, D(esp+0x28));
			D(esp+0x28)++;
			if((int)D(esp+0x28) >= 0x100) break;
		}

		while(1){

			___3079ch_cdecl(1);
			if((B(___1a1f64h+3) >= 4)&&(B(___1a1f64h+4) >= 4)&&(B(___1a1f64h+5) >= 4)) break;
		}

		memset(esp, 0, 4);

		n = -1;
		while(++n < 4){

			while(1){

				eax = 1+rand_watcom106()%4;
				D(___1de7d0h+0x54*n+0x40) = eax;
				if(B(esp+eax-1) == 0) break;
			}
			
			B(esp+eax-1) = 1;
		}

		___3892ch_cdecl(1);
		edx  = 0;

		n = -1;
		while(++n < 0x14){

			ecx = s_6c[n].points;
			if(((int)edx < (int)ecx)&&(n != D(___1a1ef8h))) edx = ecx;
		}

#if defined(DR_MULTIPLAYER)
		if(___19bd60h != 0){

			D(___1a0a50h+0xc) = 1;
		}
		else {
#endif // DR_MULTIPLAYER
			D(___1a0a50h+0xc) = !((int)edx < (int)s_6c[D(___1a1ef8h)].points);
#if defined(DR_MULTIPLAYER)
		}
#endif // DR_MULTIPLAYER

		D(___185a20h) = 1;
	}
	else {

		D(___1a1028h) = 3;
		D(___1a103ch) = 1;
		D(___196af0h) = 0;
		D(___196aech) = 0;
#if defined(DR3_CHAOS)
		if(dr3_chaos_active()){
			dr3_boss_set_in_race(1);
			dr3_boss_race_reset();
		}
#endif
		___33010h_cdecl(2);

#if defined(DR3_CHAOS)
		if(dr3_chaos_active() && dr3_boss_in_race()){
			const int cur_phase = dr3_boss_phase();
			const int won       = (D(___196ae8h) == 1);
			const int me        = (int)D(___1a1ef8h);

			dr3_boss_set_in_race(0);

			if(won && (cur_phase < DR3_BOSS_PHASE_3)){
				const int bonus = dr3_boss_phase_bonus(cur_phase);
				const int next  = dr3_boss_next_phase(cur_phase, 1);
				char      msg1[80], msg2[80];

				/* Mandatory free full repair + phase clear bonus cash -> Pit Stop at The Shop */
				if((me >= 0) && (me < 20)){
					s_6c[me].damage  = 0;
					s_6c[me].money  += bonus;
					s_6c[me].income += bonus;
				}
				dr3_boss_set_phase(next);
				dr3_boss_save_sync();

				snprintf(msg1, sizeof(msg1), "FREE FULL REPAIR + $%d BONUS!", bonus);
				snprintf(msg2, sizeof(msg2), "SHOP & SAVE FOR %s", dr3_boss_phase_title(next));
				dr3_log("[dr3] boss: %s CLEARED! Free repair + $%d -> advancing to phase %d",
				        dr3_boss_phase_title(cur_phase), bonus, next);
				dr3_boss_notice(
				    (cur_phase == DR3_BOSS_PHASE_1) ? "PHASE 1 CLEARED!" : "PHASE 2 CLEARED!",
				    msg1, msg2);

				D(___196ae8h) = 0;
				D(___185a20h) = 1;
				return;
			}
			else if(!won){
				/* Dying in ANY phase resets the whole gauntlet back to Phase 1 */
				dr3_log("[dr3] boss: %s FAILED (place %d) - resetting gauntlet to Phase 1",
				        dr3_boss_phase_title(cur_phase), (int)D(___196ae8h));
				dr3_boss_reset();
				dr3_boss_save_sync();
				dr3_boss_notice("CHALLENGE FAILED!",
				                "GAUNTLET RESET BACK TO PHASE 1",
				                "REPAIR AT THE SHOP AND TRY AGAIN");
				D(___196ae8h) = 0;
				D(___185a20h) = 1;
				return;
			}
			else {
				/* Phase 3 Victory! Clear gauntlet state and fall through to championship finish */
				dr3_log("[dr3] boss: FINAL PHASE CLEARED! Champion of 30th Anniversary!");
				dr3_boss_reset();
				dr3_boss_save_sync();
			}
		}
#endif

		if(D(___196ae8h) == 1){
			
			D(___185a48h) = 0;
			D(___185a44h) = 0;
			D(___185a40h) = 0;
			D(___185a3ch) = 0;
			B(___185b58h+0x1) = 1;
			strcpy(___1866b8h+0x1c2, "Start A New Game");
			B(___185b58h+0xa) = 0;
			B(___185b58h+0xd) = 0;
			B(___185b58h+0xb) = 0;
			strcpy(___1866b8h, "Start Racing");
			___2415ch();
			___243d44h = 1;
			D(___185a5ch+0x34) = 0;
			D(___185a2ch) = 1;
			D(___196ae8h) = 0;
		}
	}
}
