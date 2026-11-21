#include "drally.h"
#include "drmath.h"
#include "drally_structs_free.h"
#include "platform_3ds/dr3_chaos.h"     /* the extra car ladder: DR3_HUD_CARS, dr3_chaos_extra_draw/lights */
#include "platform_3ds/dr3_boss.h"

	extern __BYTE__ MY_CAR_IDX[];
	extern int X___243c8ch;
	extern __BYTE__ ___1e6ed0h[];
	extern int CURRENT_VIEWPORT_CENTER_X;
	extern int TRX_WIDTH;
	extern int CURRENT_VIEWPORT_W;
	extern int Y___243c90h;
	extern int CURRENT_VIEWPORT_X;
	extern int CURRENT_VIEWPORT_CENTER_Y;
	extern int TRX_HEIGHT;
	extern int CURRENT_VIEWPORT_H;
	extern __BYTE__ ___1de580h[];
	extern __BYTE__ TRX_LIT_TAB[0x100];
	extern __BYTE__ ___243c60h[];
	extern __BYTE__ NUM_OF_CARS[];
	extern int TRX_VIEWPORT_TL_Y;
	extern int TRX_VIEWPORT_TL_X;
	extern __POINTER__ BACKBUFFER;
	extern __POINTER__ ___243d74h;
	extern __POINTER__ RACE_BURN1A_BPK;

void ___5e769h(__BYTE__ * A0, int Y3, int X3, int Y2, int X2, int Y1, int X1);
__DWORD__ __GET_FRAME_COUNTER_NOW(void);

#define LOC_PI 		3.14159265359
#define LOC_5OVER6	0.833333


#if defined(DR3_CHAOS)
/*
 * The headlight beam (helper00) was the main suspect for the grey box around the car, but the test
 * build with the player's beam switched off showed the box stayed - so the lights are innocent and
 * stay on.  The light table is still logged once (dr3_log_once_light), it is cheap and says whether
 * the track brought a sane table.
 */
#define DR3_CHAOS_NO_PLAYER_LIGHTS 0

/* logs what the track's light table holds - once, so the log stays short */
static void dr3_log_once_light(void){

	static int done;
	int i;

	if(done) return;
	done = 1;

	if(!dr3_log_enabled()) return;              /* the switch (dr3_log.h): no table dump while silent */

	dr3_log("[dr3] chaos: TRX_LIT_TAB (lights):");
	for(i = 0; i < 0x100; i += 0x20){

		dr3_log("[dr3] chaos:   %02x..: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x",
		        i,
		        TRX_LIT_TAB[i+0], TRX_LIT_TAB[i+1], TRX_LIT_TAB[i+2], TRX_LIT_TAB[i+3],
		        TRX_LIT_TAB[i+4], TRX_LIT_TAB[i+5], TRX_LIT_TAB[i+6], TRX_LIT_TAB[i+7],
		        TRX_LIT_TAB[i+8], TRX_LIT_TAB[i+9], TRX_LIT_TAB[i+10], TRX_LIT_TAB[i+11],
		        TRX_LIT_TAB[i+12], TRX_LIT_TAB[i+13], TRX_LIT_TAB[i+14], TRX_LIT_TAB[i+15]);
	}

	dr3_log_flush();
}
#endif

// LIGHTS
static void helper00(int n){

	struct_35e_t * 	s_35e;

	s_35e = (struct_35e_t *)___1e6ed0h;

#if defined(DR3_CHAOS)
	/* names this pass for the watchdog (dr3_log.c) - the remap filler has three callers */
	dr3_chaos_remap_who("car lights");
#endif

	___5e769h(
		TRX_LIT_TAB,
		s_35e[n].__8+(int)(40.0*dRMath_cos(((double)s_35e[n].Direction+170.0)*LOC_PI/180.0)*LOC_5OVER6),
		s_35e[n].__4+(int)(40.0*dRMath_sin(((double)s_35e[n].Direction+170.0)*LOC_PI/180.0)),
		s_35e[n].__8+(int)(40.0*dRMath_cos(((double)s_35e[n].Direction+190.0)*LOC_PI/180.0)*LOC_5OVER6),
		s_35e[n].__4+(int)(40.0*dRMath_sin(((double)s_35e[n].Direction+190.0)*LOC_PI/180.0)),
		s_35e[n].__8,
		s_35e[n].__4);

	___5e769h(
		TRX_LIT_TAB,
		s_35e[n].__8+(int)(40.0*dRMath_cos(((double)s_35e[n].Direction+190.0)*LOC_PI/180.0)*LOC_5OVER6),
		s_35e[n].__4+(int)(40.0*dRMath_sin(((double)s_35e[n].Direction+190.0)*LOC_PI/180.0)),
		s_35e[n].__8+(int)(36.0*dRMath_cos(((double)s_35e[n].Direction+198.0)*LOC_PI/180.0)*LOC_5OVER6),
		s_35e[n].__4+(int)(36.0*dRMath_sin(((double)s_35e[n].Direction+198.0)*LOC_PI/180.0)),
		s_35e[n].__8,
		s_35e[n].__4);

	___5e769h(
		TRX_LIT_TAB,
		s_35e[n].__8+(int)(36.0*dRMath_cos(((double)s_35e[n].Direction+162.0)*LOC_PI/180.0)*LOC_5OVER6),
		s_35e[n].__4+(int)(36.0*dRMath_sin(((double)s_35e[n].Direction+162.0)*LOC_PI/180.0)),
		s_35e[n].__8+(int)(40.0*dRMath_cos(((double)s_35e[n].Direction+170.0)*LOC_PI/180.0)*LOC_5OVER6),
		s_35e[n].__4+(int)(40.0*dRMath_sin(((double)s_35e[n].Direction+170.0)*LOC_PI/180.0)),
		s_35e[n].__8,
		s_35e[n].__4);
}

// CAR
static void helper01(int x, int y, __POINTER__ vp1, int A0){

	int 		i, n, offset;
	__BYTE__ 	px;

	int width = A0;
	int stride = A0;
	int height = A0;
	__BYTE__* src = vp1;
	__BYTE__* dest = BACKBUFFER + 0x200 * y + x;

	// y clip
	if (y < 0) {
		dest += (-y) * 0x200;
		src += (-y) * width;
		height += y;
		y = 0;
	}
	int ydiff = y + height - DR3_RACE_H;
	if (ydiff > 0) {
		height -= ydiff;
	}

	// x clip
	int xdiff = CURRENT_VIEWPORT_X + 0x60 + (-x);
	if (xdiff > 0) {
		dest += xdiff;
		src += xdiff;
		width -= xdiff;
		x += xdiff;
	}
	xdiff = x + width - (0x60 + DR3_RACE_W);
	if (xdiff > 0) {
		width -= xdiff;
	}

	if (width <= 0 || height <= 0) {
		return;
	}

	int yy = height;
	while (yy--) {
		int xx = width;
		__BYTE__* s = src;
		__BYTE__* d = dest;
		while (xx--) {
			if ((px = *s++)) {
				*d = px;
#if defined(DR3_CHAOS)
				/* mark the pixel as a pixel of the car being drawn (dr3_chaos.h): this is exactly the
				   place where it lands, so the port never has to work a position out */
				if (dr3_chaos_car_now >= 0) dr3_chaos_car_mask[d - BACKBUFFER] = (__BYTE__)(dr3_chaos_car_now + 1);
#endif
			}
			d++;
		}
		src += stride;
		dest += 0x200;
	}
}

#if defined(DR3_CHAOS)
/* 1.5x nearest-neighbor scaled car blit (40x40 -> 60x60) for the Phase 3 Giant Boss Deliverator. */
static void helper01_scaled_150(int x, int y, __BYTE__ * src, int src_dim){

	const int dst_dim = (src_dim * 3) / 2;
	int dy, dx;

	for(dy = 0; dy < dst_dim; ++dy){
		const int sy = (dy * 2) / 3;
		const int by = y + dy;
		const __BYTE__ * srow;
		__BYTE__ * drow;

		if((by < 0) || (by >= DR3_RACE_H)) continue;
		srow = src + sy * src_dim;
		drow = BACKBUFFER + 0x200 * by;

		for(dx = 0; dx < dst_dim; ++dx){
			const int bx = x + dx;
			const int sx = (dx * 2) / 3;
			__BYTE__ px;

			if((bx < CURRENT_VIEWPORT_X + 0x60) || (bx >= (0x60 + DR3_RACE_W))) continue;
			if((px = srow[sx]) != 0){
				drow[bx] = px;
				if(dr3_chaos_car_now >= 0) dr3_chaos_car_mask[0x200 * by + bx] = (__BYTE__)(dr3_chaos_car_now + 1);
			}
		}
	}
}
#endif

// DEAD CAR
static void helper02(int n){

	struct_35e_t * 	s_35e;
	const __DWORD__ fc = __GET_FRAME_COUNTER_NOW();

	s_35e = (struct_35e_t *)___1e6ed0h;

	helper01(s_35e[n].__4+88, s_35e[n].__8-8, RACE_BURN1A_BPK+0x100*s_35e[n].__1da, 0x10);

	if((s_35e[n].__1de+0x3) <= fc){

		s_35e[n].__1da++;
		if((int)s_35e[n].__1da > 7) s_35e[n].__1da = 0;
		s_35e[n].__1de = fc;
	}
}

#if defined(DR3_CHAOS)
/*
 * The colours of the cars of this frame (dr3_chaos.h).  The 3DS port re-colours the pixels a car
 * sprite wrote with the colour of that car's driver, and it can do that exactly because the sprite
 * blit marks every pixel it writes (dr3_chaos_car_mask) - no position has to be worked out anywhere.
 *
 * The colour comes from the race entry (s_54 +0x34/+0x38/+0x3c, one byte each): ___33010h.c fills
 * them from the racer's colour in carcol.pal and the bottom screen draws its markers from the same
 * fields (platform_3ds/dr3_bottom.c).  s_94 is *not* the place to look - its r/g/b are floats there.
 */
/* is this car driving the Adversary's SPECIAL (model 6)? */
static int dr3_chaos_car_is_boss(int car){

	extern __BYTE__ ___1de7d0h[];

	return (int)D(___1de7d0h+0x54*car+0x30) == DR3_ADVERSARY_CAR;
}

static void dr3_chaos_car_colours(void){

	extern __BYTE__ ___1de7d0h[];
	const int cars = (int)D(NUM_OF_CARS);
	int n;

	for(n = 0; (n < cars) && (n < DR3_MAX_CARS); ++n){

		if (dr3_boss_is_giant_car(n)) {
			/* Giant Boss Deliverator: menacing violet-black normally, blazing crimson when enraged */
			if (dr3_boss_enraged()) {
				dr3_chaos_car_col[n][0] = 52;
				dr3_chaos_car_col[n][1] = 4;
				dr3_chaos_car_col[n][2] = 4;
			}
			else {
				dr3_chaos_car_col[n][0] = 16;
				dr3_chaos_car_col[n][1] = 0;
				dr3_chaos_car_col[n][2] = 28;
			}
		}
		else if (dr3_chaos_car_is_boss(n) && (n != (int)D(MY_CAR_IDX))) {
			/*
			 * SPECIAL.BPK uses car ramp 0 (0x0f..0x18).  In Chaos mode car 0 is normally another
			 * racer (e.g. Jane Honda = blue), so leaving the boss untinted would paint him in
			 * car 0's colour.  Tint him to jet-black with silver highlights (6,6,8) in 6-bit VGA
			 * space and keep his sprite marked so his Chaos health bar works too.
			 */
			dr3_chaos_car_col[n][0] = 6;
			dr3_chaos_car_col[n][1] = 6;
			dr3_chaos_car_col[n][2] = 8;
		} else {
			dr3_chaos_car_col[n][0] = B(___1de7d0h+0x54*n+0x34);
			dr3_chaos_car_col[n][1] = B(___1de7d0h+0x54*n+0x38);
			dr3_chaos_car_col[n][2] = B(___1de7d0h+0x54*n+0x3c);
		}
	}
}
#endif

static int ltandlt(int v0, int v1, int v2){ return (v0<v1)&&(v1<v2); }
static int lqandlq_d(double v0, double v1, double v2){ return (v0<=v1)&&(v1<=v2); }

// RENDER CARS
void race___4f300h(void){

	int 			n;
	double 			d00, d01, d02;
	struct_35e_t * 	s_35e;


	s_35e = (struct_35e_t *)___1e6ed0h;

#if defined(DR3_CHAOS)
	/*
	 * The frame's car pixels start over (dr3_chaos.h): every sprite pixel this pass writes is marked
	 * with its car, and the colours of the cars are collected for the port's colouring.  The mask has
	 * the shape of BACKBUFFER and race_main.c copies it next to the image with the same offset.
	 */
	memset(dr3_chaos_car_mask, 0, sizeof(dr3_chaos_car_mask));
	dr3_chaos_car_colours();
	dr3_chaos_car_now = -1;
#endif

	d00 = (double)CURRENT_VIEWPORT_CENTER_X;
	d01 = (double)X___243c8ch+(double)s_35e[D(MY_CAR_IDX)].XLocation;
	d02 = (double)(int)(TRX_WIDTH-CURRENT_VIEWPORT_CENTER_X);

	if(d00 > d01) s_35e[D(MY_CAR_IDX)].__4 = (int)(double)s_35e[D(MY_CAR_IDX)].XLocation;
	if(d02 < d01) s_35e[D(MY_CAR_IDX)].__4 = (int)((double)s_35e[D(MY_CAR_IDX)].XLocation-(double)(int)(TRX_WIDTH-CURRENT_VIEWPORT_W));

	if(lqandlq_d(d00, d01, d02)) s_35e[D(MY_CAR_IDX)].__4 = CURRENT_VIEWPORT_CENTER_X-X___243c8ch;

	s_35e[D(MY_CAR_IDX)].__4 += CURRENT_VIEWPORT_X;

	d00 = (double)CURRENT_VIEWPORT_CENTER_Y;
	d01 = (double)Y___243c90h+(double)s_35e[D(MY_CAR_IDX)].YLocation;
	d02 = (double)(int)(TRX_HEIGHT-CURRENT_VIEWPORT_CENTER_Y);

	if(d00 > d01) s_35e[D(MY_CAR_IDX)].__8 = (int)(double)s_35e[D(MY_CAR_IDX)].YLocation;
	if(d02 < d01) s_35e[D(MY_CAR_IDX)].__8 = (int)((double)s_35e[D(MY_CAR_IDX)].YLocation-(double)(int)(TRX_HEIGHT-CURRENT_VIEWPORT_H));

	if(lqandlq_d(d00, d01, d02)) s_35e[D(MY_CAR_IDX)].__8 = CURRENT_VIEWPORT_CENTER_Y-Y___243c90h;


#if defined(DR3_CHAOS)
#if DR3_CHAOS_NO_PLAYER_LIGHTS
	/* (the test switch is off now - the lights are drawn again, only the table gets logged) */
	dr3_log_once_light();
#else
	dr3_log_once_light();
	if(!dr3_chaos_skip_pass(DR3_PASS_PLAYER_BEAM) &&
	   ((int)D(___1de580h+0x94*D(MY_CAR_IDX)+0x18) > 0)&&(s_35e[D(MY_CAR_IDX)].Finished == 0)) helper00(D(MY_CAR_IDX));
#endif
#else
	if(((int)D(___1de580h+0x94*D(MY_CAR_IDX)+0x18) > 0)&&(s_35e[D(MY_CAR_IDX)].Finished == 0)) helper00(D(MY_CAR_IDX));
#endif

	n = -1;
	while(++n < (int)D(NUM_OF_CARS)){

		if(n != D(MY_CAR_IDX)){

			s_35e[n].__4 = (int)((double)s_35e[n].XLocation-(double)TRX_VIEWPORT_TL_X+(double)CURRENT_VIEWPORT_X);
			s_35e[n].__8 = (int)((double)(int)(double)s_35e[n].YLocation-(double)TRX_VIEWPORT_TL_Y);

			if(ltandlt((int)(CURRENT_VIEWPORT_X-0x28), (int)s_35e[n].__4, DR3_RACE_W+0x28)){

				if(ltandlt(-40, (int)s_35e[n].__8, (int)(CURRENT_VIEWPORT_H+0x28))){
	
										if(
#if defined(DR3_CHAOS)
					   /* the extra cars' headlight beams are one step of the ladder (dr3_chaos.h) */
					   ((n < DR3_HUD_CARS) || dr3_chaos_extra_lights()) &&
					   !dr3_chaos_skip_pass(DR3_PASS_OTHER_BEAMS) &&
#endif
					   ((int)D(___1de580h+0x94*n+0x18) > 0)&&(s_35e[n].Finished == 0)) helper00(n);
				}
			}
		}
	}

#if defined(DR3_CHAOS)
	/* your own car: the sprite (and its wreck) is one of the three parts of this pass (dr3_chaos.h) */
	if(!dr3_chaos_skip_pass(DR3_PASS_PLAYER_SPRITE))
#endif
	{
#if defined(DR3_CHAOS)
		/* the sprite blit marks every pixel of this car (dr3_chaos.h) */
		dr3_chaos_car_now = dr3_chaos_car_is_boss((int)D(MY_CAR_IDX)) ? -1 : (int)D(MY_CAR_IDX);
#endif
		helper01(s_35e[D(MY_CAR_IDX)].__4+76, s_35e[D(MY_CAR_IDX)].__8-20, ___243d74h+s_35e[D(MY_CAR_IDX)].ImgOffset, 0x28);

#if defined(DR3_CHAOS)
		dr3_chaos_car_now = -1;      /* the burning wreck below is not in the car's colour */
#endif

		if((int)D(___1de580h+0x94*D(MY_CAR_IDX)+0x18) <= 0) helper02(D(MY_CAR_IDX));
	}

	n = -1;
	while(++n < (int)D(NUM_OF_CARS)){

		if(n != D(MY_CAR_IDX)){
				
#if defined(DR3_CHAOS)
			if(dr3_boss_is_giant_car(n)){
				if(ltandlt((int)(CURRENT_VIEWPORT_X-0x1e), (int)s_35e[n].__4, DR3_RACE_W+0x1e) &&
				   ltandlt(-30, (int)s_35e[n].__8, (int)(CURRENT_VIEWPORT_H+0x1e))){
					dr3_chaos_car_now = n;
					helper01_scaled_150(s_35e[n].__4+0x42, s_35e[n].__8-0x1e, ___243d74h+s_35e[n].ImgOffset, 0x28);
					dr3_chaos_car_now = -1;
				}
			}
			else
#endif
			if(
#if defined(DR3_CHAOS)
			   /* the extra cars' sprites are one step of the ladder (dr3_chaos.h) */
			   ((n < DR3_HUD_CARS) || dr3_chaos_extra_draw()) &&
#endif
			   ltandlt((int)(CURRENT_VIEWPORT_X-0x14), (int)s_35e[n].__4, DR3_RACE_W+0x14)){

				if(ltandlt(-20, (int)s_35e[n].__8, (int)(CURRENT_VIEWPORT_H+0x14))){

#if defined(DR3_CHAOS)
					/* the sprite blit marks every pixel of this car (dr3_chaos.h) */
					dr3_chaos_car_now = n;
#endif
					helper01(s_35e[n].__4+0x4c, s_35e[n].__8-0x14, ___243d74h+s_35e[n].ImgOffset, 0x28);
#if defined(DR3_CHAOS)
					dr3_chaos_car_now = -1;      /* the burning wreck below is not in the car's colour */
#endif
				}
			}

			if(
#if defined(DR3_CHAOS)
			   /* the wreck sprite of an extra car is part of the same step (dr3_chaos.h) */
			   ((n < DR3_HUD_CARS) || dr3_chaos_extra_draw()) &&
#endif
			   ltandlt((int)(CURRENT_VIEWPORT_X-0x8), (int)s_35e[n].__4, DR3_RACE_W+0x8)){
				
				if(ltandlt(-8, (int)s_35e[n].__8, (int)(CURRENT_VIEWPORT_H+0x8))){
							
					if((int)D(___1de580h+0x94*n+0x18) <= 0) helper02(n);
				}
			}
		}
	}

#if defined(DR3_CHAOS)
	/*
	 * One line for each of the first car passes, and only while the log is on: what the sprite blits
	 * actually marked.  This is the engine side of the port's colouring - if the cars are not coloured
	 * on the screen, this says whether the marks are there at all (dr3_chaos.h).  A marked count of
	 * zero is a result as well: then nothing reached the port to begin with.
	 */
	{
		static int dr3_mask_logged;

		if(dr3_log_enabled() && (dr3_mask_logged < 5)){

			int i, marked = 0, lo = 256, hi = -1;

			for(i = 0; i < (int)sizeof(dr3_chaos_car_mask); ++i){

				const int m = dr3_chaos_car_mask[i];

				if(!m) continue;
				++marked;
				if(m < lo) lo = m;
				if(m > hi) hi = m;
			}

			dr3_log("[dr3] chaos: car pass %d: %d pixel(s) marked (car index %d..%d), colours: %d,%d,%d / %d,%d,%d / %d,%d,%d",
			        dr3_mask_logged, marked, lo-1, hi-1,
			        dr3_chaos_car_col[0][0], dr3_chaos_car_col[0][1], dr3_chaos_car_col[0][2],
			        dr3_chaos_car_col[1][0], dr3_chaos_car_col[1][1], dr3_chaos_car_col[1][2],
			        dr3_chaos_car_col[2][0], dr3_chaos_car_col[2][1], dr3_chaos_car_col[2][2]);
			++dr3_mask_logged;

			/*
			 * The two things that decide whether the port can colour the cars: where the marks of this
			 * frame are, and what the player's own car looks like in the render buffer right here.  If
			 * the marks sit where the car is and the car's pixels are on the palette's ramp (0x0f..0x18,
			 * race___4a25ch.c), the port's colouring has everything it needs; anything else says which
			 * half is wrong.
			 */
			if(dr3_mask_logged == 1){

				struct_35e_t *   s_35e = (struct_35e_t *)___1e6ed0h;
				const __BYTE__ * stripe = (const __BYTE__ *)___243d74h;
				int              box[4] = {0x1000, 0x1000, -1, -1};
				int              x, y, n;

				for(y = 0; y < DR3_RACE_H; ++y){

					for(x = 0; x < 0x200; ++x){

						if(!dr3_chaos_car_mask[0x200*y+x]) continue;
						if(x < box[0]) box[0] = x;
						if(y < box[1]) box[1] = y;
						if(x > box[2]) box[2] = x;
						if(y > box[3]) box[3] = y;
					}
				}
				dr3_log("[dr3] chaos: the car marks of this frame: x %d..%d, y %d..%d (the visible window is x 0x60..0x1a0)",
				        box[0], box[2], box[1], box[3]);

				/*
				 * What the sprite of every car actually holds where the blit reads it: the palette
				 * indices the port has to recognise to colour a car.  This is the sprite data itself,
				 * not the screen - a car that is not on the palette's ramp (0x0f..0x18) here can never
				 * be coloured however well the marks line up.
				 */
				for(n = 0; (n < (int)D(NUM_OF_CARS)) && (n < DR3_MAX_CARS); ++n){

					const __BYTE__ * fr = stripe + s_35e[n].ImgOffset;
					int              hist[256], k, ramp = 0, top = 0, nonz = 0, most = 0;

					memset(hist, 0, sizeof(hist));
					for(k = 0; k < 0x640; ++k) ++hist[fr[k]];
					for(k = 0; k < 256; ++k){

						nonz += hist[k];
						if((k >= 0xf) && (k <= 0x18)) ramp += hist[k];
						if(hist[k] > most) { most = hist[k]; top = k; }
					}
					nonz -= hist[0];

					dr3_log("[dr3] chaos:   car %d sprite (img %d): %d pixel(s), %d on the ramp 0x0f..0x18, most common 0x%02x (%d)",
					        n, (int)s_35e[n].ImgIndex, nonz, ramp, top, most);
				}
			}
		}
	}
#endif

	D(___243c60h) = D(NUM_OF_CARS);
}
