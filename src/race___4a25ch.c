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
#if defined(DR3_CHAOS)
		/*
		 * The position a car starts with is its place in the standings order (dr3_chaos.h), so the
		 * grid the field was lined up on is also the order the position list starts with - the cars
		 * with the best rank lead it.  Without -DDR3_CHAOS the original expression runs.
		 */
		s_35e[n].Position = dr3_chaos_car_pos[n] + 1;
#else
		s_35e[n].Position = n+1;
#endif
		s_35e[n].__34a = 0;

		if((n > 0)||(s_94[0].__0 != 6)){

#if defined(DR3_CHAOS)
			/*
			 * The left panel draws the driver's name on a bar that is part of the panel's own
			 * picture, and that bar is drawn with *that car's* colour ramp: the block for car n
			 * uses the palette entries 10*n+15..10*n+24 (measured - see the note in
			 * doc/3ds.md).  The rows of the panel are the cars 0..3 with the player's left out
			 * (race___405bch.c) and the blocks are decoded the same way (race___42824h.c), so
			 * all four ramps have to be built here, exactly as the four car build below does.
			 *
			 * They were not: only ramp 0 was written, and the bars of the other rows then
			 * showed whatever the *track* palette happens to hold at 0x19..0x36 - on TR6 that
			 * is 0x3f003f (magenta), which is where the pink name bars came from.  The car
			 * sprites are unaffected: they all keep ramp 0 and the port re-colours them with
			 * their driver's colour while it presents the frame (dr3_chaos.h).
			 */
			if(n < DR3_HUD_CARS)
				___4a064h_cdecl(10*n+15,
				                (float)(int)B(___1de7d0h+0x54*n+0x34),
				                (float)(int)B(___1de7d0h+0x54*n+0x38),
				                (float)(int)B(___1de7d0h+0x54*n+0x3c));
#else
			___4a064h_cdecl(10*n+15, (float)(int)s_94[n].__38, (float)(int)s_94[n].__3c, (float)(int)s_94[n].__40);
#endif
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
	if(dr3_chaos_dump_on()) dr3_chaos_dump_palette();
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

		dr3_log("[dr3] chaos: ramp %d: %02x%02x%02x .. %02x%02x%02x", i,
		        B(___1a51d0h + 3*(10*i+15)), B(___1a51d0h + 3*(10*i+15) + 1), B(___1a51d0h + 3*(10*i+15) + 2),
		        B(___1a51d0h + 3*(10*i+24)), B(___1a51d0h + 3*(10*i+24) + 1), B(___1a51d0h + 3*(10*i+24) + 2));
	}

	/* the car sprites use entries 0..14 (their grey/dark parts) and 15+24n (their colour) - dump both */
	for(i = 0; i < 3; ++i){

		int e;

		dr3_log("[dr3] chaos: palette %d..%d:%s%d%s", i*5, i*5+4,
		        " ", 0, "");

		for(e = i*5; e < i*5+5; ++e){

			dr3_log("[dr3] chaos:   entry %2d = %02x %02x %02x", e,
			        B(___1a51d0h + 3*e), B(___1a51d0h + 3*e + 1), B(___1a51d0h + 3*e + 2));
		}
	}

	for(i = 0; i < 0x640; ++i){

		const int v = B(stripe + i);

		if(v < lo) lo = v;
		if(v > hi) hi = v;
		if((v >= 0x0f) && (v <= 0x18)) ++in_ramp;
	}

	dr3_log("[dr3] chaos: player car %d stripe: pixel values %02x..%02x, %d of %d in the ramp range",
	        (int)MY_CAR_IDX, lo, hi, in_ramp, 0x640);

	/*
	 * A "box around the car" at some rotation angles is a sprite frame whose used pixels reach the
	 * whole 40x40, instead of the car's own outline.  Look at all 96 frames of the player's stripe and
	 * print the first frame that looks like that (the car body is at most ~36x30).
	 */
	{
		int f, x, y, odd = 0, empty = 0, normal = 0;

		for(f = 0; f < 96; ++f){

			const __BYTE__ *fr = stripe + 0x640*f;
			int nz = 0, x0 = 40, y0 = 40, x1 = -1, y1 = -1;

			for(y = 0; y < 40; ++y){

				for(x = 0; x < 40; ++x){

					if(fr[40*y+x]){

						++nz;
						if(x < x0) x0 = x;
						if(x > x1) x1 = x;
						if(y < y0) y0 = y;
						if(y > y1) y1 = y;
					}
				}
			}

			if(nz == 0) ++empty;
			else if(((x1-x0+1) > 34) || ((y1-y0+1) > 30)){

				++odd;
				dr3_log("[dr3] chaos: frame %d: %d pixels, box %d,%d..%d,%d", f, nz, x0, y0, x1, y1);

				if(odd == 1){

					for(y = 0; y < 40; ++y){

						char row[41];

						for(x = 0; x < 40; ++x){

							const int v = fr[40*y+x];

							row[x] = v ? (((v >= 0x0f) && (v <= 0x18)) ? '+' : ((v < 0x0f) ? '#' : '.')) : ' ';
						}

						row[40] = 0;
						dr3_log("[dr3] chaos: |%s|", row);
					}
				}
			}
			else ++normal;
		}

		dr3_log("[dr3] chaos: player stripe frames: %d normal, %d empty, %d with a full box",
		        normal, empty, odd);
	}
}
#endif
