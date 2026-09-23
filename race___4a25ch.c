#include "drally.h"
#include "platform_3ds/dr3_chaos.h"
#include "drally_structs_free.h"

	extern __BYTE__ ___243c60h[];
	extern __BYTE__ ___1e6ed0h[];
	extern __BYTE__ ___243d08h[];
	extern __BYTE__ ___1de580h[];
	extern __BYTE__ ___1de7d0h[];


void ___4a064h_cdecl(int, float, float, float);

#if defined(DR3_CHAOS)
/* the palette/stripe dump below (defined at the end of this file) */
static void dr3_chaos_dump_palette(void);
#endif

void race___4a25ch(void){

	int 	i, n;
	struct_35e_t * 	s_35e;
	struct_94_t * 	s_94;
	struct_54_t * 	s_54;

	s_35e = (struct_35e_t *)___1e6ed0h;
	s_94 = (struct_94_t *)___1de580h;
	s_54 = (struct_54_t *)___1de7d0h;

	n = -1;
	/*
	 * This is where a car of the running race gets its whole state: the position history (__14e/__152),
	 * the facing (Direction, from the start slot's rotation), the sprite (ImgOffset = one 0x25800 byte
	 * stripe per car), lap, position, controls, the tyre smoke rings, everything.  Four cars in the
	 * original - the chaos test build (-DDR3_CHAOS, doc/3ds.md) has more, and the ones behind the
	 * fourth have to be set up here as well: half initialised cars do not drive, do not draw and send
	 * the collision code into its "wiggle free" loop.
	 */
	while(++n < DR3_MAX_CARS){

		s_35e[n].__14e = s_35e[n].XLocation;
		s_35e[n].__152 = s_35e[n].YLocation;
		s_35e[n].Direction = (float)(3.75*(double)(int)s_35e[n].ImgIndex);
		s_35e[n].__156 = s_35e[n].Direction;
		s_35e[n].ImgOffset = 0x640*s_35e[n].ImgIndex+0x25800*n;
		s_35e[n].Lap = 1;
		s_35e[n].Position = n+1;
		s_35e[n].__34a = 0;

		if((n > 0)||(s_94[0].__0 != 6)){

#if defined(DR3_CHAOS)
			/* only the first four cars own a palette ramp - DR3_PALETTE_CAR_SLOT, dr3_chaos.h */
			if(DR3_PALETTE_CAR_SLOT(n))
#endif
			___4a064h_cdecl(10*n+15, (float)(int)s_94[n].__38, (float)(int)s_94[n].__3c, (float)(int)s_94[n].__40);
		}

		s_35e[n].__19a = 0;
		s_35e[n].__0 = 0;
		s_35e[n].__b0 = 0.0f;
		s_35e[n].Finished = 0;
		s_35e[n].__18a = 0;
		s_35e[n].__14 = 0;
		s_35e[n].__18 = 0;
		s_35e[n].__19e = 0x1388*s_54[n].__14+0x3a98;
		s_35e[n].__1c = 0;
		s_35e[n].__a8 = s_94[n].__14;

		i = -1;
		while(++i < 0x10){

			s_35e[n].Ctrls[i] = 0;
			s_35e[n].ActionFlags[i] = 0;
		}

		s_35e[n].__bc = 0.0f;
		s_35e[n].__c0 = 0.0f;
		s_35e[n].__fc = 0.0f;
		s_35e[n].__100 = 0.0f;
		s_35e[n].__104 = 0.0f;
		s_35e[n].__17e = 0;
		s_35e[n].__182 = 0;
		s_35e[n].__186 = 0;
		s_35e[n].__192 = 0;
		s_35e[n].__196 = 0;
		s_35e[n].__162 = 0.0f;
		s_35e[n].__166 = 0.0f;
		s_35e[n].__16a = 0;
		s_35e[n].__16e = 0;
		s_35e[n].__1a6 = 0;
		s_35e[n].MineTimer = 0;
		s_35e[n].__1aa = 0;
		s_35e[n].__1ae = 0;
		s_35e[n].__1b2 = 0;
		s_35e[n].ActionFlags_i = 0;
		s_35e[n].__a4 = 0;

		i = -1;
		while(++i < 0xf){

			s_35e[n].__1e2[i] = 0;
			s_35e[n].__21e[i] = 0;
		}

		s_35e[n].__1b6 = 0;
		s_35e[n].__1da = 0;
		s_35e[n].__1de = 0;
		s_35e[n].Drug = 0;
		s_35e[n].__352 = 0;
		s_35e[n].__356 = 0;
		s_35e[n].__35a = 0;
		D(___243d08h) = 0xf;
	}

	D(___243c60h) = DR3_MAX_CARS;

#if defined(DR3_CHAOS)
	dr3_chaos_dump_palette();
#endif
}

#if defined(DR3_CHAOS)
/*
 * What the car colour ramps and the car's sprite actually contain.  ___4a064h_cdecl() builds each ramp
 * from 2x5 entries and the upper half is lightened with 12.6*n towards grey, so a car whose colour
 * arrives as (r,0,0) gets grey rows in its shading.  Logging the ramps and the pixel values of the
 * player's stripe says which of the two is wrong.
 */
static void dr3_chaos_dump_palette(void){

	extern __BYTE__ ___1a51d0h[];
	extern __POINTER__ ___243d74h;
	extern __DWORD__ MY_CAR_IDX;
	extern __BYTE__ ___1e6ed0h[];
	const struct_35e_t * s_35e = (const struct_35e_t *)___1e6ed0h;
	const __BYTE__ * stripe = (const __BYTE__ *)___243d74h + sizeof(car_spins_t)*(int)MY_CAR_IDX;
	int i, n, lo = 255, hi = 0, in_ramp = 0;

	for(i = 0; i < 4; ++i){

		dr3_log("[dr3] chaos: ramp %d: %02x%02x%02x .. %02x%02x%02x (colour %d,%d,%d)", i,
		        B(___1a51d0h + 3*(10*i+15)), B(___1a51d0h + 3*(10*i+15) + 1), B(___1a51d0h + 3*(10*i+15) + 2),
		        B(___1a51d0h + 3*(10*i+24)), B(___1a51d0h + 3*(10*i+24) + 1), B(___1a51d0h + 3*(10*i+24) + 2));
	}

	for(n = 0; n < 0x640; ++n){

		const int v = B(stripe + n);

		if(v < lo) lo = v;
		if(v > hi) hi = v;
		if((v >= 0x0f) && (v <= 0x18)) ++in_ramp;
	}

	dr3_log("[dr3] chaos: player car %d stripe: pixel values %02x..%02x, %d of %d in the ramp range",
	        (int)MY_CAR_IDX, lo, hi, in_ramp, 0x640);
}
#endif
