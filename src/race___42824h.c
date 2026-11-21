#include "drally.h"
#include "platform_3ds/dr3_chaos.h"
#if defined(DR3_CHAOS)
#include "platform_3ds/dr3_grid.h"   /* where the extra cars stand (chaos build) */
#include "platform_3ds/dr3_arena.h"  /* the arena: every car in its own corner */
#include "platform_3ds/dr3_dm.h"     /* street deathmatch: arm every car for battle */
#endif
#include "drally_structs_free.h"

#pragma pack(1)
typedef struct rgb24_s {
	__BYTE__ 	r;
	__BYTE__ 	g;
	__BYTE__ 	b;
} rgb24_t;

typedef rgb24_t pal256_t[0x100];

typedef struct xc50_s {
	int 		PointsN;			// +000
	int 		TrianglesN;			// +004
	__DWORD__ 	CONST_X___8[75];		// +008
	__DWORD__ 	CONST_Y_134[75];		// +134
	__DWORD__	CONST_Z_260[75];		// +260
	__DWORD__ 	_38C;			// +38c
	__DWORD__	_390;			// +390
	__DWORD__	_394;			// +394
	__DWORD__ 	_398;			// +398
	__DWORD__	IndicesP1[100];		// +39c
	__DWORD__	IndicesP2[100];		// +52c
	__DWORD__	IndicesP3[100];		// +6bc
	__DWORD__	_84C[100];		// +84c
	__DWORD__	TRX_X;			// +9dc
	__DWORD__ 	TRX_Y;			// +9e0
	__DWORD__	X_REL_VIEWPORT_CENTER;			// +9e4
	__DWORD__	Y_REL_VIEWPORT_CENTER;			// +9e8
	__DWORD__	PointsX[75];		// +9ec
	__DWORD__	PointsY[75];		// +b18
	__DWORD__	_C44;			// +c44
	__DWORD__	_C48;			// +c48
	__DWORD__	_C4C;			// +c4c
} xc50_t;

typedef struct x2c_s {
    __DWORD__   width;     	// +00
    __DWORD__   height;   	// +04
    __DWORD__   offset;    	// +08
    __DWORD__   XPos;      	// +0c
    __DWORD__   YPos;     	// +10
    __DWORD__   _14;        // +14
    __DWORD__   _18;        // +18
    __DWORD__   _1C;        // +1c
    __DWORD__   _20;        // +20
    __DWORD__   _24;        // +24
    __DWORD__   _28;        // +28
} x2c_t;

	extern char ___19bd64h[16];
	extern char TRX_BPA[14];
	extern __POINTER__ TRX_MAS;
	extern pal256_t ___1a51d0h;
	extern int TRX_WIDTH;
	extern int TRX_HEIGHT;
	extern __POINTER__ TRX_IMA;
	extern __BYTE__ ___243d08h[];
	extern __BYTE__ ___196dach[];
	extern __BYTE__ ___1a54d0h[];
	extern int TRX_WIDTH_QTR;
	extern int TRX_HEIGHT_QTR;
	extern __BYTE__ ___243cech[];
	extern __POINTER__ TRX_VAI;
	extern __POINTER__ TRX_LR1;
	extern car_spins_t * ___243d74h;
	extern __BYTE__ NUM_OF_CARS[];
	extern __BYTE__ ___1de580h[];
	extern __POINTER__ RACE_MINES1A_BPK;
	extern __POINTER__ RACE_BLOWI_BPK;
	extern __POINTER__ RACE_FLAMEX_BPK[6];
	extern __POINTER__ RACE_SHOTS_BPK;
	extern __POINTER__ RACE_OBSTACLE_BPK;
	extern __POINTER__ RACE_PEDESTR_BPK;
	extern __POINTER__ RACE_SPLATX_BPK[2];
	extern __POINTER__ RACE_ROCKETX_BPK[2];
	extern __POINTER__ RACE_BURN1A_BPK;
	extern __POINTER__ RACE_SMOKE_BPK;
	extern __BYTE__ ___243c5ch[];
	extern __BYTE__ ___2438c4h[];
	extern __BYTE__ X___1de920h[];
	extern __BYTE__ Y___1ded20h[];
	extern __BYTE__ ___1df120h[];
	extern __BYTE__ ___1e8448h[];
	extern __BYTE__ ___1e8048h[];
	extern __BYTE__ ___1e7c48h[];
	extern float TRX_DRV_DAT[0x200];
	extern int TRX_OHI_DAT[0x100];
	extern __POINTER__ GEN_FLA_BPK;
	extern __POINTER__ GEN_LAM_BPK;
	extern __POINTER__ GEN_MES_BPK;
	extern __POINTER__ DAMSLID_BPK;
	extern __BYTE__ MY_CAR_IDX[];
	extern __BYTE__ ___1c9f10h[];
	extern __POINTER__ RASTI1_BPK;
	extern __BYTE__ ___1d5890h[0x1f80];
	extern __BYTE__ ___1de210h[];
	extern __BYTE__ ___1d7810h[];
	extern __BYTE__ ___1d8910h[];
	extern __BYTE__ ___1d8a10h[];
	extern __BYTE__ ___1d2710h[];
	extern __POINTER__ GOALNUM2_BPK;
	extern __POINTER__ SIDEBOM1_BPK;
	extern __BYTE__ ___1d8110h[];
	extern __BYTE__ ___1de7d0h[];
	extern __DWORD__ NUM_OF_OBJECTS;
    extern __DWORD__ NUM_OF_TEXTURES;
    extern xc50_t ___1f3b08h[];
    extern x2c_t ___240b48h[];		// <0x240b48 - 0x240b74)
    extern __POINTER__ ___243d5ch;
	
	
__BYTE__ VARJO_TAB[0x100];
__BYTE__ TRX_LIT_TAB[0x100];
__BYTE__ TRX_BLO_TAB[0x100];
__BYTE__ TRX_SKI_TAB[0x100];

void old_bpa_read(const char *, __POINTER__, const char *);
void bpk_decode2(__POINTER__, __POINTER__);
void bpk_decode4(__DWORD__, __DWORD__, __POINTER__, __POINTER__);


typedef char char14[14];

static const char14 ___196dfch[7] = {
	[0] = "KUPLA",
	[1] = "PICKUP",
	[2] = "SEDAN",
	[3] = "CAMARO",
	[4] = "PORCHE",
	[5] = "LOTUS",
	[6] = "SPECIAL"
};

void race___42824h(void){

	int 			n;
	FILE * 			fd;
	char 			fname[0x10];
	pal256_t 		pal;
	char * 			suffix;
	__BYTE__ * 		CarSpinsStripe;
	struct_54_t *	s_54;


	strcat(strcpy(TRX_BPA, ___19bd64h), ".BPA");
	strcat(strcpy(fname, ___19bd64h), "-IMA.BPK");
	old_bpa_read(TRX_BPA, TRX_MAS, fname);
	bpk_decode4(0x300, 0xa, (__POINTER__)&pal, TRX_MAS);

	n = -1;
	while(++n < 0x100) ___1a51d0h[n] = pal[n];

	bpk_decode4(TRX_WIDTH*TRX_HEIGHT, 0x30a, TRX_IMA, TRX_MAS);

	if(D(___196dach) != 0){

		strcat(strcpy(fname, ___19bd64h), "-FLIP.PAL");
		old_bpa_read(TRX_BPA, (__POINTER__)&pal, fname);
		strcat(strcpy(fname, ___19bd64h), "-FLIP.SCX");
		
		if((fd = fopen(fname, "rb")) != 0){

			fseek(fd, 0xa, SEEK_SET);
			fread(&pal, sizeof(pal256_t), 1, fd);
			fclose(fd);
		}

		n = -1;
		while(++n < 0x100) ___1a51d0h[n] = pal[n];
	}

	D(___243d08h) = 0x100;

	strcat(strcpy(fname, ___19bd64h), "-MAS.BPK");
	old_bpa_read(TRX_BPA, ___1a54d0h, fname);
	bpk_decode4(TRX_WIDTH*TRX_HEIGHT, 0x30a, TRX_MAS, ___1a54d0h);

#if defined(DR3_CHAOS)
	if(dr3_log_enabled()){
		int cnt[16];
		int mx, my, i;

		for(i = 0; i < 16; ++i) cnt[i] = 0;

		for(my = 0; my < TRX_HEIGHT; my += 8){

			for(mx = 0; mx < TRX_WIDTH; mx += 8) ++cnt[B(TRX_MAS + mx + TRX_WIDTH*my) & 0xf];
		}

		dr3_log("[dr3] chaos: mask values: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d 6:%d 7:%d 8:%d 9:%d a:%d b:%d c:%d d:%d e:%d f:%d",
		        cnt[0], cnt[1], cnt[2], cnt[3], cnt[4], cnt[5], cnt[6], cnt[7],
		        cnt[8], cnt[9], cnt[10], cnt[11], cnt[12], cnt[13], cnt[14], cnt[15]);
	}
#endif

	strcat(strcpy(fname, ___19bd64h), "-VAI.BPK");
	old_bpa_read(TRX_BPA, ___1a54d0h, fname);
	bpk_decode4(TRX_WIDTH_QTR*TRX_HEIGHT_QTR, 0x30a, TRX_VAI, ___1a54d0h);

	strcat(strcpy(fname, ___19bd64h), "-LR1.BPK");
	old_bpa_read(TRX_BPA, ___1a54d0h, fname);
	bpk_decode4(TRX_WIDTH_QTR*TRX_HEIGHT_QTR, 0x30a, TRX_LR1, ___1a54d0h);  


	s_54 = (struct_54_t *)___1de7d0h;

	n = -1;
	while(++n < D(NUM_OF_CARS)){

		suffix = ((s_54[n].__24 == 1)&&((int)D(___1de580h+0x94*n) < 6)) ? "-S.BPK" : ".BPK";
		strcat(strcpy(fname, ___196dfch[D(___1de580h+0x94*n)]), suffix);
#if defined(DR3_CHAOS)
		dr3_log("[dr3] chaos: car %d picture %s (model %d, small %d)", n, fname,
		        (int)D(___1de580h+0x94*n), (int)s_54[n].__24);
#endif
		old_bpa_read("ENGINE.BPA", ___1a54d0h, fname);
		CarSpinsStripe = (__BYTE__ *)___243d74h[n];
#if defined(DR3_CHAOS)
		/*
		 * Wipe the stripe first: a car that brings fewer frames than the buffer holds (the "-S.BPK"
		 * variants) otherwise keeps whatever the previous car left in the tail - which shows up as grey
		 * lines across the car at the angles that fall into those frames.
		 */
		memset(CarSpinsStripe, 0, sizeof(car_spins_t));
#endif
		bpk_decode2(CarSpinsStripe, ___1a54d0h);	// 40*40*96

#if !defined(DR3_CHAOS)
		i = -1;
		while(++i < sizeof(car_spins_t)){

			/*
			 * "30th Anniversary": only the boss keeps the picture's own (black) colours.  Every other
			 * car - the player's own Anniversary included - gets its colour from the palette slot
			 * (see doc/3ds.md)
			 */
			if((((int)D(___1de7d0h+0x54*n+0x30) != DR3_ADVERSARY_CAR) || (n == D(MY_CAR_IDX))) &&
			   DR3_PALETTE_CAR_SLOT(n) &&
			   (CarSpinsStripe[i] >= 0xf)&&(CarSpinsStripe[i] <= 0x18)) CarSpinsStripe[i] += 0xa*n;
		}
#else
		/*
		 * The chaos build leaves the sprite on the palette's own ramp, so all ten of its shades stay:
		 * the colour of the car goes on top of those pixels by the 3DS port while it presents the
		 * frame (dr3_chaos.h, platform_3ds/dr3_fb.c) - that is how twenty cars can have twenty colours
		 * even though the palette has room for only four ten-entry ramps.
		 */
#endif

#if defined(DR3_CHAOS)
		/*
		 * A stripe that decoded to (almost) nothing is a car that collides but cannot be seen - the
		 * "invisible objects" of the reports.  Count what frame 0 got, hand such a car the first car's
		 * stripe and say so.
		 */
		{
			int nz = 0, k;

			for(k = 0; k < 0x640; ++k) if(CarSpinsStripe[k]) ++nz;

			if(nz < 32){

				dr3_log("[dr3] chaos: car %d stripe came out empty (%d pixels) - using car 0's picture", n, nz);
				memcpy(CarSpinsStripe, (const __BYTE__ *)___243d74h[0], sizeof(car_spins_t));
			}
			else dr3_log("[dr3] chaos: car %d stripe: %d pixels in frame 0", n, nz);
		}
#endif
	}

	old_bpa_read("ENGINE.BPA", ___1a54d0h, "MINES1A.BPK");
	bpk_decode2(RACE_MINES1A_BPK, ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "BLOWI.BPK");
	bpk_decode2(RACE_BLOWI_BPK, ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "FLAME1.BPK");
	bpk_decode2(RACE_FLAMEX_BPK[0], ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "FLAME2.BPK");
	bpk_decode2(RACE_FLAMEX_BPK[1], ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "FLAME3.BPK");
	bpk_decode2(RACE_FLAMEX_BPK[2], ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "FLAME4.BPK");
	bpk_decode2(RACE_FLAMEX_BPK[3], ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "FLAME5.BPK");
	bpk_decode2(RACE_FLAMEX_BPK[4], ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "FLAME6.BPK");
	bpk_decode2(RACE_FLAMEX_BPK[5], ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "SHOTS.BPK");
	bpk_decode2(RACE_SHOTS_BPK, ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, D(___196dach) ? "OBST_REV.BPK" : "OBSTACLE.BPK");
	bpk_decode2(RACE_OBSTACLE_BPK, ___1a54d0h);
#if defined(DR3_CHAOS)
	/*
	 * Synthesize our custom 16x16 MINE CRATE pickup sprite at slot 9 (kind == 10, offset 0x900).
	 * Styled as a bright hazard-yellow & dark-charcoal supply crate with a vivid red armed-mine
	 * icon + white highlight in the center so it looks unmistakably different from dropped 8x8 mines.
	 */
	{
		unsigned char *dst = (unsigned char *)RACE_OBSTACLE_BPK + 0x900;
		unsigned char c_yel = 55, c_red = 55, c_dk = 55, c_wht = 55;
		int best_yel = -10000, best_red = -10000, best_dk = 10000, best_wht = -10000;
		int k, px, py;

		for(k = 55; k < 256; ++k){
			const int r = ___1a51d0h[k].r, g = ___1a51d0h[k].g, b = ___1a51d0h[k].b;
			const int s_yel = r + g - 2*b;
			const int s_red = 2*r - g - b;
			const int s_lum = r + g + b;
			const int s_wht = s_lum - ((r > g ? r - g : g - r) + (g > b ? g - b : b - g));

			if(s_yel > best_yel){ best_yel = s_yel; c_yel = (unsigned char)k; }
			if(s_red > best_red){ best_red = s_red; c_red = (unsigned char)k; }
			if((s_lum < best_dk) && (s_lum > 4)){ best_dk = s_lum; c_dk = (unsigned char)k; }
			if(s_wht > best_wht){ best_wht = s_wht; c_wht = (unsigned char)k; }
		}

		/* 16x16 bitmap: '.'=transparent, 'Y'=hazard yellow, 'D'=dark frame, 'R'=red mine, 'W'=white glint */
		static const char *const mine_crate[16] = {
			"..YYYYYYYYYYYY..",
			".YYDDYYDDYYDDYY.",
			"YYDDDDDDDDDDDDYY",
			"YDDRDDDDDDDDDDDY",
			"YDDDDDDWWDDDDDDY",
			"YDDDDWRRRRDDDDDY",
			"YDDDRRRRRRRDDDDY",
			"YDDWRRRRRRRRDDDY",
			"YDDWRRRRRRRRDDDY",
			"YDDDRRRRRRRDDDDY",
			"YDDDDDRRRRDDDDDY",
			"YDDDDDDDDDDDDDDY",
			"YDDDDDDDDDDDRDDY",
			"YYDDDDDDDDDDDDYY",
			".YYDDYYDDYYDDYY.",
			"..YYYYYYYYYYYY.."
		};

		for(py = 0; py < 16; ++py){
			for(px = 0; px < 16; ++px){
				const char ch = mine_crate[py][px];
				dst[py * 16 + px] = (ch == 'Y') ? c_yel
				                  : (ch == 'D') ? c_dk
				                  : (ch == 'R') ? c_red
				                  : (ch == 'W') ? c_wht : 0;
			}
		}
	}
#endif
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "PEDESTR.BPK");
	bpk_decode2(RACE_PEDESTR_BPK, ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "SPLAT3.BPK");
	bpk_decode2(RACE_SPLATX_BPK[0], ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "SPLAT4.BPK");
	bpk_decode2(RACE_SPLATX_BPK[1], ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "ROCKET1.BPK");
	bpk_decode2(RACE_ROCKETX_BPK[0], ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "ROCKET2.BPK");
	bpk_decode2(RACE_ROCKETX_BPK[1], ___1a54d0h);
#if defined(DR3_CHAOS)
	if(dr3_log_enabled()){
		int n;

		for(n = 0; n < (int)D(NUM_OF_CARS); ++n){

			const __BYTE__ * s = (const __BYTE__ *)___243d74h + sizeof(car_spins_t)*n;
			unsigned         sum = 0u, xr = 0u;
			int              k;

			for(k = 0; k < (int)sizeof(car_spins_t); ++k){ sum += s[k]; xr = (xr << 1) ^ (unsigned)s[k]; }

			dr3_log("[dr3] chaos: stripe %d checksum sum %08x xor %08x (base %p)",
			        n, sum, xr, (const void *)s);
		}
	}
#endif

	old_bpa_read("ENGINE.BPA", ___1a54d0h, "BURN1A.BPK");
	bpk_decode2(RACE_BURN1A_BPK, ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "SMOKE.BPK");
	bpk_decode2(RACE_SMOKE_BPK, ___1a54d0h);
	
	old_bpa_read(TRX_BPA, ___1a54d0h, strcat(strcpy(fname, ___19bd64h), "-SHA.BPK"));
	bpk_decode4(4, 0, ___243c5ch, ___1a54d0h);
	D(___243c5ch) = SDL_SwapLE32(D(___243c5ch));
	bpk_decode4(4, 4, ___2438c4h, ___1a54d0h);
	D(___2438c4h) = SDL_SwapLE32(D(___2438c4h));
	bpk_decode4(4*D(___243c5ch), 8, X___1de920h, ___1a54d0h);
	bpk_decode4(4*D(___243c5ch), 4*D(___243c5ch)+8, Y___1ded20h, ___1a54d0h);
	bpk_decode4(4*D(___243c5ch), 8*D(___243c5ch)+8, ___1df120h, ___1a54d0h);
	bpk_decode4(4*D(___2438c4h), 0xc*D(___243c5ch)+8, ___1e8448h, ___1a54d0h);
	bpk_decode4(4*D(___2438c4h), 4*D(___2438c4h)+0xc*D(___243c5ch)+8, ___1e8048h, ___1a54d0h);
	bpk_decode4(4*D(___2438c4h), 8*D(___2438c4h)+0xc*D(___243c5ch)+8, ___1e7c48h, ___1a54d0h);
	for (int i = 0; i < 256; i++) {
		((int*)X___1de920h)[i] = SDL_SwapLE32(((int*)X___1de920h)[i]);
		((int*)Y___1ded20h)[i] = SDL_SwapLE32(((int*)Y___1ded20h)[i]);
		((int*)___1df120h)[i] = SDL_SwapLE32(((int*)___1df120h)[i]); // TODO unused?
		((int*)___1e8448h)[i] = SDL_SwapLE32(((int*)___1e8448h)[i]);
		((int*)___1e8048h)[i] = SDL_SwapLE32(((int*)___1e8048h)[i]);
		((int*)___1e7c48h)[i] = SDL_SwapLE32(((int*)___1e7c48h)[i]);
	}

#if defined(DR3_CHAOS)
	/*
	 * Chaos test build (-DDR3_CHAOS, see doc/3ds.md): put the extra cars on the road.
	 *
	 * A track file brings four start slots (race___49bd4h.c) and no more, so a race with more cars
	 * than that has the extras share slots 0..3 - they would stand on top of the four starters.  The
	 * placement that used to be here moved them with the *wrong* half of the surface mask: it looked
	 * for (mask & 0xf) < 4, and that is the soft ground beside the road, not the road.  That is why the
	 * extra cars stood in the grass, and where the ground runs out, outside the track.
	 *
	 * The road is (mask & 0xf) == 0xf - the classification of dr3_minimap.h, and the one the collision
	 * code and the skid marks use as well (race___4ff50h.c "< 4" is off road, race___54668h.c "== 0xf"
	 * is the hard surface).  platform_3ds/dr3_grid.c walks along it: from the rear-most of the four
	 * slots it steps down the middle of the road and hands back a spot every few car lengths, always
	 * DR3_GRID_CLEAR away from every car placed before it, and it carries on in front of the grid when
	 * the road behind runs out.  Every step is verified against this track's own mask, so it works on
	 * any track - and a car that comes back from it stands on the hard surface and touches no other car.
	 *
	 * This runs once per race setup and OUTSIDE the swap loop above: it used to sit inside that loop,
	 * which ran the whole placement 256 times per race (the result is the same every time - pure work).
	 */
	{
		const int dr3_have = ((int)D(NUM_OF_CARS) < DR3_HUD_CARS) ? (int)D(NUM_OF_CARS) : DR3_HUD_CARS;
		const int dr3_want = (int)D(NUM_OF_CARS) - dr3_have;    /* the cars that share a start slot */
		extern __BYTE__ ___1e6ed0h[];
		struct_35e_t *  dr3_s35e = (struct_35e_t *)___1e6ed0h;

		/*
		 * Which code is running: the log names the build and the facing the four starters carry, so a
		 * placement that comes out wrong can be read against the very values the field is compared with
		 * (a stale assignment in the placement once overwrote the facing and looked fine in the source).
		 */
		dr3_log("[dr3] chaos: build %s %s, track %s (rot %d, the starters face %.0f deg)",
		        __DATE__, __TIME__, ___19bd64h, (int)dr3_s35e[0].ImgIndex,
		        (double)dr3_s35e[0].Direction);

		/* the arena (platform_3ds/dr3_arena.h): no road to walk along - every car goes to a corner of its own */
		if (dr3_chaos_arena()) dr3_arena_setup();
		else
		if (dr3_want > 0) {

			dr3_grid_t      dr3_track;
			dr3_grid_car_t  dr3_grid[DR3_HUD_CARS];
			dr3_grid_spot_t dr3_spot[DR3_MAX_CARS];
			int             dr3_n, dr3_got;

			dr3_track.mask   = (const uint8_t *)TRX_MAS;
			dr3_track.mask_w = TRX_WIDTH;
			dr3_track.mask_h = TRX_HEIGHT;
			dr3_track.rot    = (int)dr3_s35e[0].ImgIndex;   /* the four slots all face this way */

			/*
			 * Read the ACTUAL four front-row cars (grid places 0..dr3_have-1, occupying track slots 0..3)
			 * rather than arbitrary car indices 0..3: in a career race cars 0..3 can all have rear ranks
			 * and miss slot 3 completely, which made dr3_grid_place() start from the wrong slot on Oasis!
			 */
			for (dr3_n = 0; dr3_n < dr3_have; ++dr3_n) {
				int dr3_c, dr3_found = dr3_n;
				for (dr3_c = 0; dr3_c < (int)D(NUM_OF_CARS); ++dr3_c) {
					if (dr3_chaos_car_pos[dr3_c] == dr3_n) { dr3_found = dr3_c; break; }
				}
				if (dr3_n == 0) dr3_track.rot = (int)dr3_s35e[dr3_found].ImgIndex;
				dr3_grid[dr3_n].x = (float)dr3_s35e[dr3_found].XLocation;
				dr3_grid[dr3_n].y = (float)dr3_s35e[dr3_found].YLocation;
			}

			dr3_got = dr3_grid_place(&dr3_track, dr3_grid, dr3_have, dr3_spot, dr3_want);

			/* Final safety check against TRX_VAI: guarantee every placed car faces forward along the track */
			for (dr3_n = 0; dr3_n < dr3_got; ++dr3_n) {
				if (dr3_grid_align_vai((const uint8_t *)TRX_VAI, TRX_WIDTH_QTR, TRX_HEIGHT_QTR,
				                       (int)D(___243cech), dr3_spot[dr3_n].x, dr3_spot[dr3_n].y,
				                       &dr3_spot[dr3_n].dir)) {
					dr3_log("[dr3] chaos: grid place %2d flipped +180 deg to match VAI race direction -> %.0f deg",
					        dr3_have + dr3_n + 1, (double)dr3_spot[dr3_n].dir);
				}
			}

			dr3_log("[dr3] chaos: grid of %d cars (start rot %d, the four starters face %.0f deg): "
			        "%d of %d extras placed on the road",
			        dr3_have, dr3_track.rot, DR3_GRID_ROT_DEG * (double)dr3_track.rot, dr3_got, dr3_want);

			if (dr3_got < dr3_want)
				dr3_log("[dr3] chaos: the road next to this start line has room for %d of them - "
				        "the other %d keep the slot they shared (the walk stopped because: %s)",
				        dr3_got, dr3_want - dr3_got,
				        (dr3_grid_last_reason() == DR3_GRID_NO_ROAD) ? "no road ahead"
				      : (dr3_grid_last_reason() == DR3_GRID_NO_ROOM) ? "never enough room for a car body"
				                                                     : "the road bent back over the start line");

			for (dr3_n = 0; dr3_n < dr3_got; ++dr3_n) {

				int dr3_car;

				/*
				 * The walk hands its spots out in order - the first one is the closest to the grid - so
				 * they go to the cars in the order of the standings: the spot after the four start slots
				 * is grid place 5, and the car that stands there is the one whose place that is
				 * (dr3_chaos.h).  That, and not the index the front end happened to give a car, is what
				 * lines the field up the way the standings read.
				 */
				for (dr3_car = 0; dr3_car < DR3_MAX_CARS; ++dr3_car)
					if (dr3_chaos_car_pos[dr3_car] == (dr3_have + dr3_n)) break;

				if (dr3_car >= DR3_MAX_CARS) dr3_car = dr3_have + dr3_n;    /* no order known: as before */

				dr3_s35e[dr3_car].XLocation = dr3_spot[dr3_n].x;
				dr3_s35e[dr3_car].YLocation = dr3_spot[dr3_n].y;

				/*
				 * ... and the car has to face the way the road goes there (dr3_grid.h).  On a track
				 * whose start line sits on a bend (utopia) the extras used to keep the facing of the
				 * slot they shared and stood across the road.  The three fields are the ones
				 * race___4a25ch.c sets for the starters, so an extra on a straight piece of road comes
				 * out with exactly the starter's angle.
				 */
				{
					const int dr3_img = dr3_grid_img_index(dr3_spot[dr3_n].dir);

					dr3_s35e[dr3_car].ImgIndex  = dr3_img;
					dr3_s35e[dr3_car].Direction = (float)(DR3_GRID_ROT_DEG * (double)dr3_img);
					dr3_s35e[dr3_car].__156     = dr3_s35e[dr3_car].Direction;
					dr3_s35e[dr3_car].ImgOffset = 0x640 * dr3_img + 0x25800 * dr3_car;
				}

				dr3_log("[dr3] chaos: grid place %2d: %-11s (standings %d) stands %s the grid, "
				        "%d px along the road, at %d,%d (surface %d), facing %.0f deg (img %d, lane %d)",
				        dr3_have + dr3_n + 1,
				        (const char *)(___1de7d0h + 0x54 * dr3_car), dr3_chaos_car_rank[dr3_car],
				        (dr3_spot[dr3_n].side == DR3_GRID_BEHIND) ? "behind" : "ahead of",
				        dr3_spot[dr3_n].dist,
				        (int)dr3_spot[dr3_n].x, (int)dr3_spot[dr3_n].y,
				        (int)(B(TRX_MAS + (int)dr3_spot[dr3_n].x + TRX_WIDTH*(int)dr3_spot[dr3_n].y) & 0xf),
				        (double)dr3_s35e[dr3_car].Direction, dr3_s35e[dr3_car].ImgIndex,
				        dr3_spot[dr3_n].lane);
			}

			/*
			 * What the engine's own contact test finds in the field as it now stands.  race___4dcach.c only
			 * ever looks at a pair whose distance is under 40 px in *both* axes - that is the kind of pair a
			 * player sees as two cars standing on top of each other.  The host probe (logs/grid_probe.c)
			 * checks the same thing against the track data before a build is flashed; this line says it for
			 * the race that is actually being set up, so a stacked field cannot go unnoticed.
			 */
			{
				int       dr3_i, dr3_j, dr3_close = 0, dr3_piled = 0;
				const int dr3_cars = (int)D(NUM_OF_CARS);

				for (dr3_i = 1; dr3_i < dr3_cars; ++dr3_i)
					for (dr3_j = 0; dr3_j < dr3_i; ++dr3_j) {

						const int dr3_dx = (int)(dr3_s35e[dr3_i].XLocation - dr3_s35e[dr3_j].XLocation);
						const int dr3_dy = (int)(dr3_s35e[dr3_i].YLocation - dr3_s35e[dr3_j].YLocation);

						if ((dr3_dx < 40) && (dr3_dx > -40) && (dr3_dy < 40) && (dr3_dy > -40)) ++dr3_close;
					}

				/* a car whose place is beyond the spots that were handed out kept its start slot */
				for (dr3_i = 0; dr3_i < dr3_cars; ++dr3_i)
					if (dr3_chaos_car_pos[dr3_i] >= (dr3_have + dr3_got)) ++dr3_piled;

				dr3_log("[dr3] chaos: field of %d cars: %d without a place of their own, %d pair(s) inside "
				        "the 40 px contact box (a pair 40 px apart in one axis is never looked at)",
				        dr3_cars, dr3_piled, dr3_close);
			}
		}

		if (dr3_chaos_active() && (dr3_chaos_mode() == DR3_CHAOS_MODE_DM)) dr3_dm_setup();
	}
#endif

	old_bpa_read(TRX_BPA, ___1a54d0h, strcat(strcpy(fname, ___19bd64h), "-SCE.BPK"));

	NUM_OF_OBJECTS = 0;
    bpk_decode4(1, 0, (__POINTER__)&NUM_OF_OBJECTS, ___1a54d0h);
	NUM_OF_OBJECTS = SDL_SwapLE32(NUM_OF_OBJECTS);
    bpk_decode4(sizeof(xc50_t)*NUM_OF_OBJECTS, 1, (__POINTER__)&___1f3b08h, ___1a54d0h);
	for (int i = 0; i < NUM_OF_OBJECTS; i++)
	{
		xc50_t* obj = &___1f3b08h[i];
		obj->PointsN = SDL_SwapLE32(obj->PointsN);
		obj->TrianglesN = SDL_SwapLE32(obj->TrianglesN);
		for (int j = 0; j < 75; j++) {
			obj->CONST_X___8[j] = SDL_SwapLE32(obj->CONST_X___8[j]);
			obj->CONST_Y_134[j] = SDL_SwapLE32(obj->CONST_Y_134[j]);
			obj->CONST_Z_260[j] = SDL_SwapLE32(obj->CONST_Z_260[j]);
			obj->PointsX[j] = SDL_SwapLE32(obj->PointsX[j]);
			obj->PointsY[j] = SDL_SwapLE32(obj->PointsY[j]);
		}
		for (int j = 0; j < 100; j++) {
			obj->IndicesP1[j] = SDL_SwapLE32(obj->IndicesP1[j]);
			obj->IndicesP2[j] = SDL_SwapLE32(obj->IndicesP2[j]);
			obj->IndicesP3[j] = SDL_SwapLE32(obj->IndicesP3[j]);
			obj->_84C[j] = SDL_SwapLE32(obj->_84C[j]);
		}
		obj->_38C = SDL_SwapLE32(obj->_38C);
		obj->_390 = SDL_SwapLE32(obj->_390);
		obj->_394 = SDL_SwapLE32(obj->_394);
		obj->_398 = SDL_SwapLE32(obj->_398);
		obj->TRX_X = SDL_SwapLE32(obj->TRX_X);
		obj->TRX_Y = SDL_SwapLE32(obj->TRX_Y);
		obj->X_REL_VIEWPORT_CENTER = SDL_SwapLE32(obj->X_REL_VIEWPORT_CENTER);
		obj->Y_REL_VIEWPORT_CENTER = SDL_SwapLE32(obj->Y_REL_VIEWPORT_CENTER);
		obj->_C44 = SDL_SwapLE32(obj->_C44);
		obj->_C48 = SDL_SwapLE32(obj->_C48);
		obj->_C4C = SDL_SwapLE32(obj->_C4C);
	}
	NUM_OF_TEXTURES = 0;
    bpk_decode4(1, sizeof(xc50_t)*NUM_OF_OBJECTS+1, (__POINTER__)&NUM_OF_TEXTURES, ___1a54d0h);
	NUM_OF_TEXTURES = SDL_SwapLE32(NUM_OF_TEXTURES);

    if(NUM_OF_TEXTURES > 0) bpk_decode4(sizeof(x2c_t)*NUM_OF_TEXTURES, sizeof(xc50_t)*NUM_OF_OBJECTS+2, (__POINTER__)&___240b48h, ___1a54d0h);
	for (int i = 0; i < NUM_OF_TEXTURES; i++)
	{
		x2c_t* texture = &___240b48h[i];
		texture->width = SDL_SwapLE32(texture->width);
		texture->height = SDL_SwapLE32(texture->height);
		texture->offset = SDL_SwapLE32(texture->offset);
		texture->XPos = SDL_SwapLE32(texture->XPos);
		texture->YPos = SDL_SwapLE32(texture->YPos);
		texture->_14 = SDL_SwapLE32(texture->_14);
		texture->_18 = SDL_SwapLE32(texture->_18);
		texture->_1C = SDL_SwapLE32(texture->_1C);
		texture->_20 = SDL_SwapLE32(texture->_20);
		texture->_24 = SDL_SwapLE32(texture->_24);
		texture->_28 = SDL_SwapLE32(texture->_28);
	}
/*
	n = -1;
	while(++n < NUM_OF_TEXTURES){

		printf("[%2d] %dx%d, +%d, X=%d, Y=%d, M=%d\n", n, 
			___240b48h[n].width,
			___240b48h[n].height,
			___240b48h[n].offset,
			___240b48h[n].XPos,
			___240b48h[n].YPos,
			___240b48h[n]._14
		);
	}
*/
    switch(___19bd64h[2]){
    case '0':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        bpk_decode4(0x493e0, sizeof(x2c_t)*NUM_OF_TEXTURES+sizeof(xc50_t)*NUM_OF_OBJECTS+2, ___243d5ch, ___1a54d0h);
        break;
    case '1':
    case '2':
        bpk_decode4(0x5f370, sizeof(x2c_t)*NUM_OF_TEXTURES+sizeof(xc50_t)*NUM_OF_OBJECTS+2, ___243d5ch, ___1a54d0h);
        break;
    default: 
        break;
    }

	old_bpa_read(TRX_BPA, TRX_SKI_TAB, strcat(strcpy(fname, ___19bd64h), "-SKI.TAB"));
	old_bpa_read(TRX_BPA, TRX_BLO_TAB, strcat(strcpy(fname, ___19bd64h), "-BLO.TAB"));
	old_bpa_read(TRX_BPA, TRX_LIT_TAB, strcat(strcpy(fname, ___19bd64h), "-LIT.TAB"));
	old_bpa_read("ENGINE.BPA", VARJO_TAB, "VARJO.TAB");
#if defined(DR3_CHAOS)
	/*
	 * What the two things the shadow drawing reads hold - the polygons (race___4f170h.c) and the colour
	 * table they are remapped through (VARJO.TAB, "shadow" in Finnish).  Both come from the track file, so
	 * a four car race and a twelve car race have to show the same numbers here.  If they do not, the extra
	 * cars are writing into track data somewhere instead of the other way round.
	 */
	dr3_log("[dr3] chaos: track shadows: %d polygons, %d vertices, first vertex %d,%d",
	        (int)D(___2438c4h), (int)D(___243c5ch), X___1de920h[0], Y___1ded20h[0]);
	dr3_log("[dr3] chaos: VARJO_TAB: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x",
	        B(VARJO_TAB + 0), B(VARJO_TAB + 1), B(VARJO_TAB + 2), B(VARJO_TAB + 3),
	        B(VARJO_TAB + 4), B(VARJO_TAB + 5), B(VARJO_TAB + 6), B(VARJO_TAB + 7),
	        B(VARJO_TAB + 8), B(VARJO_TAB + 9), B(VARJO_TAB + 10), B(VARJO_TAB + 11),
	        B(VARJO_TAB + 12), B(VARJO_TAB + 13), B(VARJO_TAB + 14), B(VARJO_TAB + 15));
#endif
	old_bpa_read(TRX_BPA, (__POINTER__)TRX_DRV_DAT, strcat(strcpy(fname, ___19bd64h), "-DRV.DAT"));
	for (int i = 0; i < 512; i++) {
		TRX_DRV_DAT[i] = SDL_SwapFloatLE(TRX_DRV_DAT[i]);
	}
	old_bpa_read(TRX_BPA, (__POINTER__)TRX_OHI_DAT, strcat(strcpy(fname, ___19bd64h), "-OHI.DAT"));
	for (int i = 0; i < 256; i++) {
		TRX_OHI_DAT[i] = SDL_SwapLE32(TRX_OHI_DAT[i]);
	}
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "GEN-FLA.BPK");
	bpk_decode2(GEN_FLA_BPK, ___1a54d0h);  
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "GEN-LAM.BPK");
	bpk_decode2(GEN_LAM_BPK, ___1a54d0h);
	old_bpa_read("ENGINE.BPA", ___1a54d0h, "GEN-MES.BPK");
	bpk_decode2(GEN_MES_BPK, ___1a54d0h);
	old_bpa_read("IBFILES.BPA", ___1a54d0h, "DAMSLID.BPK");
	bpk_decode2(DAMSLID_BPK, ___1a54d0h);
	old_bpa_read("IBFILES.BPA", ___1a54d0h, "BOARDS.BPK");

	if(D(0x94*D(MY_CAR_IDX)+___1de580h+0x2c) == 0){

		bpk_decode4(0x1a00, 0x1a00*D(MY_CAR_IDX)+0x8800, ___1c9f10h, ___1a54d0h);
	}
	else {

		bpk_decode4(0x1a00, 0x2200*D(MY_CAR_IDX), ___1c9f10h, ___1a54d0h);
	}

	n = -1;
	if(++n == D(MY_CAR_IDX)) n++;
	bpk_decode4(0x800, 0x2200*n+0x1a00, ___1c9f10h+0x1a00, ___1a54d0h);
	if(++n == D(MY_CAR_IDX)) n++;
	bpk_decode4(0x800, 0x2200*n+0x1a00, ___1c9f10h+0x2200, ___1a54d0h);
	if(++n == D(MY_CAR_IDX)) n++;
	bpk_decode4(0x800, 0x2200*n+0x1a00, ___1c9f10h+0x2a00, ___1a54d0h);


	old_bpa_read("IBFILES.BPA", ___1a54d0h, "RASTI1.BPK");
	bpk_decode2(RASTI1_BPK, ___1a54d0h);

	switch(D(0x94*D(MY_CAR_IDX)+___1de580h)){
	case 0:
		old_bpa_read("IBFILES.BPA", ___1a54d0h, "DAM-KUP.BPK");
		break;
	case 1:
		old_bpa_read("IBFILES.BPA", ___1a54d0h, "DAM-PIC.BPK");
		break;
	case 2:
		old_bpa_read("IBFILES.BPA", ___1a54d0h, "DAM-SED.BPK");
		break;
	case 3:
		old_bpa_read("IBFILES.BPA", ___1a54d0h, "DAM-CAM.BPK");
		break;
	case 4:
		old_bpa_read("IBFILES.BPA", ___1a54d0h, "DAM-POR.BPK");
		break;
	case 5:
	case 6:		/* Anniversary (SPECIAL) shares the Deliverator's damage silhouette */
		old_bpa_read("IBFILES.BPA", ___1a54d0h, "DAM-LOT.BPK");
		break;
	default:
		break;
	}

	bpk_decode4(0x1f80, 0x1f80*D(MY_CAR_IDX), ___1d5890h, ___1a54d0h);
	old_bpa_read("IBFILES.BPA", ___1a54d0h, "BIGNUM6.BPK");
	bpk_decode2(___1de210h, ___1a54d0h);
	old_bpa_read("IBFILES.BPA", ___1a54d0h, "SMALFO4A.BPK");
	bpk_decode2(___1d7810h, ___1a54d0h);
	old_bpa_read("IBFILES.BPA", ___1a54d0h, "POSNUM2.BPK");
	bpk_decode2(___1d8910h, ___1a54d0h);
	old_bpa_read("IBFILES.BPA", ___1a54d0h, "OWN-NUM1.BPK");
	bpk_decode2(___1d8a10h, ___1a54d0h);
	old_bpa_read("IBFILES.BPA", ___1a54d0h, "OTH-NUM1.BPK");
	bpk_decode2(___1d2710h, ___1a54d0h);
	old_bpa_read("IBFILES.BPA", ___1a54d0h, "GOALNUM2.BPK");
	bpk_decode2(GOALNUM2_BPK, ___1a54d0h);
	old_bpa_read("IBFILES.BPA", ___1a54d0h, "SIDEBOM1.BPK");
	bpk_decode2(SIDEBOM1_BPK, ___1a54d0h);
	old_bpa_read("IBFILES.BPA", ___1a54d0h, "SMALLBAR.BPK");

	if(D(0x94*D(MY_CAR_IDX)+___1de580h+0x2c) == 0){

		bpk_decode4(0x800, 0x800*D(MY_CAR_IDX)+0x2000, ___1d8110h, ___1a54d0h);
	}
	else {

		bpk_decode4(0x800, 0x800*D(MY_CAR_IDX), ___1d8110h, ___1a54d0h);
	}
}
