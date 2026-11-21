#include "drally.h"
#include "drmath.h"
#include "drally_structs_free.h"

	extern __BYTE__ ___243c60h[];
	extern __BYTE__ ___1e6ed0h[];
	extern __BYTE__ ___243cfch[];
	extern __BYTE__ ___1de580h[];
	extern __BYTE__ MY_CAR_IDX[];
	extern __BYTE__ ___196db8h[];
	extern __BYTE__ ___243d24h[];
	extern __BYTE__ ___243d20h[];
	extern int TRX_WIDTH;
	extern int TRX_HEIGHT;
	extern __POINTER__ TRX_MAS;
	extern __POINTER__ ___243d74h;
	extern __BYTE__ ___243cb0h[];
	extern __BYTE__ ___243cb4h[];
#if defined(DR3_CHAOS)
	extern __BYTE__ NUM_OF_CARS[];		/* the field of the running race (the parked car probe below) */
	extern __BYTE__ ___1de7d0h[];		/* s_54: the race entries, +0x10 is a car's damage (0..100) */
#endif

#if defined(DR3_CHAOS)
/* the chaos test build bounds the "wiggle free" loop below: a car that the pile at the start pushed
   off the track must not spin there forever (see dr3_chaos.h) */
static int dr3_chaos_spins;
/* the parked player car: how many physics steps the player's car has had since the race started.  The
   probe at the end of race___4c434h prints every step of the opening seconds and every 16th after that. */
static unsigned dr3_pl_calls;
#endif

#define A_PI 	3.14159265359
#define L0_83 	0.833333

#define CTRL_NULL			0x00
#define CTRL_ACCELERATE		0x01
#define CTRL_BRAKE			0x02
#define CTRL_STEER_LEFT		0x04
#define CTRL_STEER_RIGHT	0x08
#define CTRL_TURBO_BOOST	0x10
#define CTRL_MACHINE_GUN	0x20
#define CTRL_DROP_MINE		0x40
#define CTRL_HORN			0x42

int rand_watcom106(void);

static __DWORD__ helper00(int i){

	__DWORD__	edx, rslt;

	if(i < 0){

		rslt = 0xf;
	}
	else {

		edx = TRX_WIDTH*TRX_HEIGHT;

		if(i >= (int)edx){

			rslt = 0xf;
		}
		else {

			rslt = B(TRX_MAS+i)&0xf;
		}
	}

	return rslt;
}

static void helper01(double d_sign){

	struct_35e_t * s_35e;

	s_35e = (struct_35e_t *)___1e6ed0h;

	if((s_35e[D(___243c60h)].__18 == 0)&&(s_35e[D(___243c60h)].__1c == 0)){

		s_35e[D(___243c60h)].Direction += (float)(d_sign*(double)s_35e[D(___243c60h)].__a8);
		F32(___243cfch) += (float)((double)F32(___1de580h+0x94*D(___243c60h)+0x10)*d_sign*(double)s_35e[D(___243c60h)].__a8/90.0);
		F32(___243cfch) *= (float)(1.0+d_sign*(double)s_35e[D(___243c60h)].__bc/18.0);

		if(s_35e[D(___243c60h)].__c0*d_sign < 36.0f) s_35e[D(___243c60h)].__c0 += (float)(2.0*d_sign);
	}
}

// RACE ENGINE
void race___4c434h(void){

	double 	d_tmp, d_hypotenuse, d_a, d_b, d_sign;
	__DWORD__ 	eax, ebx, ecx, edx, edi, esi;
	__BYTE__ 	esp[0x64];
	int 	breaker, i, j;
	__POINTER__ 	ebxp;
	__POINTER__ 	edip;
	struct_35e_t * 	s_35e;	


	s_35e = (struct_35e_t *)___1e6ed0h;
	D(___243cfch) = 0;
	s_35e[D(___243c60h)].__15e = 0.0f;
	s_35e[D(___243c60h)].__15a = 0.0f;
	s_35e[D(___243c60h)].__b0 *= 0.98;
	
	if((int)D(___1de580h+0x94*D(___243c60h)+0x18) > 0){

		if(s_35e[D(___243c60h)].Finished == 0){

			L(ecx) = s_35e[D(___243c60h)].Ctrls[getCounter(4)];

			if((L(ecx)&CTRL_ACCELERATE) != 0){
				
				if(((L(ecx)&CTRL_TURBO_BOOST) == 0)||((int)D(___1de580h+0x94*D(___243c60h)+0x34) <= 0)){

					s_35e[D(___243c60h)].__b0 += (float)(0.8*(double)F32(___1de580h+0x94*D(___243c60h)+4)/30.0);
				}
				else {

					if(D(___1de580h+0x94*D(___243c60h)+0x20) == 0){

						s_35e[D(___243c60h)].__b0 += (float)(1.3*(double)F32(___1de580h+0x94*D(___243c60h)+4)/30.0);
					}
					else {

						s_35e[D(___243c60h)].__b0 += (float)(1.5*(double)F32(___1de580h+0x94*D(___243c60h)+4)/30.0);

						{
							const int dr3_hp_before = (int)D(___1de580h+0x94*D(___243c60h)+0x18);

							D(___1de580h+0x94*D(___243c60h)+0x18) -= 0x16;
							if((int)D(___1de580h+0x94*D(___243c60h)+0x18) < 0) D(___1de580h+0x94*D(___243c60h)+0x18) = 0;

							/* the boost burns the car's own HP: the chaos bonus asks whether the player had a part in it */
							dr3_chaos_wreck(D(___243c60h), dr3_hp_before, "its own boost");
						}

						D(___1de580h+0x94*D(___243c60h)+0x58) = 1;
					}
				}

				s_35e[D(___243c60h)].__b0 *= 0.986f;
			}

			if(D(MY_CAR_IDX) == D(___243c60h)){

				if(D(___1de580h+0x94*D(___243c60h)+0x58) == 1) D(___196db8h)++;
				if(D(___1de580h+0x94*D(___243c60h)+0x58) == 0) D(___196db8h) = 0;
			}

			L(edx) = s_35e[D(___243c60h)].Ctrls[getCounter(4)];

			if(((L(edx)&CTRL_BRAKE) != 0)&&((L(edx)&CTRL_DROP_MINE) == 0)){

				s_35e[D(___243c60h)].__b0 -= F32(___1de580h+0x94*D(___243c60h)+4)/90.0;
				s_35e[D(___243c60h)].__b0 *= 0.986f;
			}

			if((s_35e[D(___243c60h)].Ctrls[getCounter(4)]&CTRL_TURBO_BOOST) != 0){

				if((int)D(___1de580h+0x94*D(___243c60h)+0x34) > 0){

					if((int)D(___1de580h+0x94*D(___243c60h)+0x18) > 0){

						D(___1de580h+0x94*D(___243c60h)+0x34) -= 0x190;
						
						if((int)D(___1de580h+0x94*D(___243c60h)+0x34) < 0) D(___1de580h+0x94*D(___243c60h)+0x34) = 0;
					}
				}
			}

			if((s_35e[D(___243c60h)].Ctrls[getCounter(4)]&CTRL_TURBO_BOOST) == 0){

				if((int)D(___1de580h+0x94*D(___243c60h)+0x18) > 0){

					D(___1de580h+0x94*D(___243c60h)+0x34) += 0x14;
					if((int)D(___1de580h+0x94*D(___243c60h)+0x34) > 0x19000) D(___1de580h+0x94*D(___243c60h)+0x34) = 0x19000;
				}
			}
		}

		if((int)D(___1de580h+0x94*D(___243c60h)+0x18) <= 0) D(___1de580h+0x94*D(___243c60h)+4) = 0;

		if(s_35e[D(___243c60h)].Finished != 0){

			s_35e[D(___243c60h)].__bc *= (float)(9.0/10.0);
		}
		else {

			if(0.0 >= (double)F32(___1de580h+0x94*D(___243c60h)+4)){

				s_35e[D(___243c60h)].__bc *= (float)(9.0/10.0);
			}
			else {

				s_35e[D(___243c60h)].__bc = (float)((((double)s_35e[D(___243c60h)].__c0*(double)s_35e[D(___243c60h)].__b0)*(double)F32(___1de580h+0x94*D(___243c60h)+0xc))/(double)F32(___1de580h+0x94*D(___243c60h)+4));

				if(dRMath_abs((double)s_35e[D(___243c60h)].__bc) > 20.0){

					s_35e[D(___243c60h)].__b0 *= (float)(49.0/50.0);
				}
			}
		}

		if((s_35e[D(___243c60h)].Ctrls[getCounter(4)]&CTRL_STEER_RIGHT) != 0) helper01(-1.0);
		if(s_35e[D(___243c60h)].Direction < 0.0f) s_35e[D(___243c60h)].Direction += 360.0f;
		if((s_35e[D(___243c60h)].Ctrls[getCounter(4)]&CTRL_STEER_LEFT) != 0) helper01(1.0);
		if(s_35e[D(___243c60h)].Direction >= 360.0f) s_35e[D(___243c60h)].Direction -= 360.0f;

		if(s_35e[D(___243c60h)].__c0 > 0.0f) s_35e[D(___243c60h)].__c0 -= 1.0f;
		if(s_35e[D(___243c60h)].__c0 < 0.0f) s_35e[D(___243c60h)].__c0 += 1.0f;

		if((int)D(___1de580h+0x94*D(___243c60h)+0x18) > 0){
		
			s_35e[D(___243c60h)].Direction += s_35e[D(___243c60h)].__104;
		}

		if(s_35e[D(___243c60h)].Direction < 0.0f) s_35e[D(___243c60h)].Direction += 360.0f;
		if(s_35e[D(___243c60h)].Direction >= 360.0f) s_35e[D(___243c60h)].Direction -= 360.0f;
#if defined(DR3_CHAOS)
		/*
		 * One +-360 step is not enough.  Direction drives the sprite frame (ImgIndex = Direction/3.75) and
		 * the sprite doubles as the collision shape, so a car that lands far outside [0,360) - a hard spin,
		 * or a hit that hands it a big slip - gets a frame index past frame 95: the blit then reads the next
		 * stripe in the block and draws a sliver of it over the car (the light grey lines), and its collision
		 * outline is that of a different frame.  Wrap until it really is in range, and say so when the old
		 * single step would have left it outside.
		 */
		if((s_35e[D(___243c60h)].Direction < -360.0f) || (s_35e[D(___243c60h)].Direction >= 720.0f))
			dr3_log("[dr3] chaos: car %d Direction %.1f - one +-360 step would not bring it back (the sprite frame would be out of range)",
			        (int)D(___243c60h), (double)s_35e[D(___243c60h)].Direction);

		while(s_35e[D(___243c60h)].Direction < 0.0f)    s_35e[D(___243c60h)].Direction += 360.0f;
		while(s_35e[D(___243c60h)].Direction >= 360.0f) s_35e[D(___243c60h)].Direction -= 360.0f;
#endif
	} // i

	F32(esp+0x38) = (float)((double)s_35e[D(___243c60h)].__bc*dRMath_sin(((double)s_35e[D(___243c60h)].Direction+ 90.0)*A_PI/180.0)/11.0);
	F32(esp+0x3c) = (float)((double)s_35e[D(___243c60h)].__bc*dRMath_cos(((double)s_35e[D(___243c60h)].Direction+ 90.0)*A_PI/180.0)*L0_83/11.0);
	F32(esp+0x34) = (float)((double)s_35e[D(___243c60h)].__b0*dRMath_sin(((double)s_35e[D(___243c60h)].Direction+180.0)*A_PI/180.0));
	F32(esp+0x30) = (float)((double)s_35e[D(___243c60h)].__b0*dRMath_cos(((double)s_35e[D(___243c60h)].Direction+180.0)*A_PI/180.0)*L0_83);
	F32(___243d24h) = (float)((double)F32(___243cfch)*dRMath_sin((((double)s_35e[D(___243c60h)].Direction+270.0)*A_PI)/180.0));
	F32(___243d20h) = (float)((double)F32(___243cfch)*dRMath_cos((((double)s_35e[D(___243c60h)].Direction+270.0)*A_PI)/180.0)*L0_83);

	d_a = (double)F32(esp+0x34)+(double)F32(esp+0x38);
	d_b = (double)F32(esp+0x30)+(double)F32(esp+0x3c);

	d_hypotenuse = dRMath_magnitude2(d_a, d_b);
	F32(esp+0x58) = (d_hypotenuse == 0.0)?1.0f:(float)(dRMath_abs((double)s_35e[D(___243c60h)].__b0)/d_hypotenuse);

	s_35e[D(___243c60h)].__15a = (float)((double)s_35e[D(___243c60h)].__fc+(double)F32(___243d24h)+(double)F32(esp+0x58)*d_a);
	s_35e[D(___243c60h)].__15e = (float)((double)s_35e[D(___243c60h)].__100+(double)F32(___243d20h)+(double)F32(esp+0x58)*d_b);

	s_35e[D(___243c60h)].__c4 = (float)(18.0*dRMath_sin(((double)s_35e[D(___243c60h)].Direction+206.0)*A_PI/180.0)+(double)s_35e[D(___243c60h)].__15a);
	s_35e[D(___243c60h)].__c8 = (float)(18.0*dRMath_cos(((double)s_35e[D(___243c60h)].Direction+206.0)*A_PI/180.0)*L0_83+(double)s_35e[D(___243c60h)].__15e);
	s_35e[D(___243c60h)].__cc = (float)(18.0*dRMath_sin(((double)s_35e[D(___243c60h)].Direction+154.0)*A_PI/180.0)+(double)s_35e[D(___243c60h)].__15a);
	s_35e[D(___243c60h)].__d0 = (float)(18.0*dRMath_cos(((double)s_35e[D(___243c60h)].Direction+154.0)*A_PI/180.0)*L0_83+(double)s_35e[D(___243c60h)].__15e);
	s_35e[D(___243c60h)].__d4 = (float)(18.0*dRMath_sin(((double)s_35e[D(___243c60h)].Direction+334.0)*A_PI/180.0)+(double)s_35e[D(___243c60h)].__15a);
	s_35e[D(___243c60h)].__d8 = (float)(18.0*dRMath_cos(((double)s_35e[D(___243c60h)].Direction+334.0)*A_PI/180.0)*L0_83+(double)s_35e[D(___243c60h)].__15e);
	s_35e[D(___243c60h)].__dc = (float)(18.0*dRMath_sin(((double)s_35e[D(___243c60h)].Direction+ 26.0)*A_PI/180.0)+(double)s_35e[D(___243c60h)].__15a);
	s_35e[D(___243c60h)].__e0 = (float)(18.0*dRMath_cos(((double)s_35e[D(___243c60h)].Direction+ 26.0)*A_PI/180.0)*L0_83+(double)s_35e[D(___243c60h)].__15e);
	s_35e[D(___243c60h)].__e4 = (float)( 8.0*dRMath_sin(((double)s_35e[D(___243c60h)].Direction+270.0)*A_PI/180.0)+(double)s_35e[D(___243c60h)].__15a);
	s_35e[D(___243c60h)].__e8 = (float)( 8.0*dRMath_cos(((double)s_35e[D(___243c60h)].Direction+270.0)*A_PI/180.0)*L0_83+(double)s_35e[D(___243c60h)].__15e);
	s_35e[D(___243c60h)].__ec = (float)( 8.0*dRMath_sin(((double)s_35e[D(___243c60h)].Direction+ 90.0)*A_PI/180.0)+(double)s_35e[D(___243c60h)].__15a);
	s_35e[D(___243c60h)].__f0 = (float)( 8.0*dRMath_cos(((double)s_35e[D(___243c60h)].Direction+ 90.0)*A_PI/180.0)*L0_83+(double)s_35e[D(___243c60h)].__15e);
	s_35e[D(___243c60h)].__f4 = (float)(22.0*dRMath_sin(((double)s_35e[D(___243c60h)].Direction+180.0)*A_PI/180.0)+(double)s_35e[D(___243c60h)].__15a);
	s_35e[D(___243c60h)].__f8 = (float)(22.0*dRMath_cos(((double)s_35e[D(___243c60h)].Direction+180.0)*A_PI/180.0)*L0_83+(double)s_35e[D(___243c60h)].__15e);

	D(___243d24h) = 0;
	D(___243d20h) = 0;

	edx = (int)((double)s_35e[D(___243c60h)].YLocation+(double)s_35e[D(___243c60h)].__f8);
	D(esp+0x5c) = (int)((double)s_35e[D(___243c60h)].XLocation+(double)s_35e[D(___243c60h)].__f4);
	D(esp+0x2c) = helper00(edx*TRX_WIDTH+D(esp+0x5c));

	edx = (int)((double)s_35e[D(___243c60h)].YLocation+(double)s_35e[D(___243c60h)].__c8);
	D(esp+0x5c) = (int)((double)s_35e[D(___243c60h)].XLocation+(double)s_35e[D(___243c60h)].__c4);
	D(esp+0x50) = helper00(edx*TRX_WIDTH+D(esp+0x5c));

	edx = (int)((double)s_35e[D(___243c60h)].YLocation+(double)s_35e[D(___243c60h)].__d0);
	D(esp+0x5c) = (int)((double)s_35e[D(___243c60h)].XLocation+(double)s_35e[D(___243c60h)].__cc);
	D(esp+0x48) = helper00(edx*TRX_WIDTH+D(esp+0x5c));

	edx = (int)((double)s_35e[D(___243c60h)].YLocation+(double)s_35e[D(___243c60h)].__d8);
	D(esp+0x5c) = (int)((double)s_35e[D(___243c60h)].XLocation+(double)s_35e[D(___243c60h)].__d4);
	D(esp+0x4c) = helper00(edx*TRX_WIDTH+D(esp+0x5c));

	edx = (int)((double)s_35e[D(___243c60h)].YLocation+(double)s_35e[D(___243c60h)].__e0);
	D(esp+0x5c) = (int)((double)s_35e[D(___243c60h)].XLocation+(double)s_35e[D(___243c60h)].__dc);
	D(esp+0x54) = helper00(edx*TRX_WIDTH+D(esp+0x5c));

	edx = (int)((double)s_35e[D(___243c60h)].YLocation+(double)s_35e[D(___243c60h)].__e8);
	D(esp+0x5c) = (int)((double)s_35e[D(___243c60h)].XLocation+(double)s_35e[D(___243c60h)].__e4);
	D(esp+0x40) = helper00(edx*TRX_WIDTH+D(esp+0x5c));

	edx = (int)((double)s_35e[D(___243c60h)].YLocation+(double)s_35e[D(___243c60h)].__f0);
	D(esp+0x5c) = (int)((double)s_35e[D(___243c60h)].XLocation+(double)s_35e[D(___243c60h)].__ec);
	D(esp+0x44) = helper00(edx*TRX_WIDTH+D(esp+0x5c));

#if defined(DR3_CHAOS)
	/*
	 * The sprite frame has to stay inside the car's own stripe: it holds 96 frames and ImgIndex is
	 * Direction/3.75, which is only 0..95 while Direction is inside [0,360).  A blit that reads past frame
	 * 95 draws the neighbouring stripe's pixels - a sliver of another car's frame over this one, which is
	 * what the light grey lines over the car are - and the very same frame is this car's collision shape
	 * (the loop above scans it against the track mask), so the collisions stop matching the picture too.
	 * Clamp it, and log every time the clamp has to do something.
	 */
	{
		const int dr3_img = (int)((double)s_35e[D(___243c60h)].Direction/3.75);

		s_35e[D(___243c60h)].ImgIndex = (dr3_img < 0) ? 0 : ((dr3_img > 95) ? 95 : dr3_img);

		if(dr3_img != (int)s_35e[D(___243c60h)].ImgIndex)
			dr3_log("[dr3] chaos: car %d sprite frame %d is outside 0..95 (Direction %.1f) - clamped",
			        (int)D(___243c60h), dr3_img, (double)s_35e[D(___243c60h)].Direction);
	}
#else
	s_35e[D(___243c60h)].ImgIndex = (int)((double)s_35e[D(___243c60h)].Direction/3.75);
#endif
	s_35e[D(___243c60h)].ImgOffset = 0x25800*D(___243c60h)+0x640*s_35e[D(___243c60h)].ImgIndex;
	ecx = (int)((double)s_35e[D(___243c60h)].__15e+(double)s_35e[D(___243c60h)].YLocation-20.0);
	ebxp = ___243d74h+(int)s_35e[D(___243c60h)].ImgOffset;
	edip = TRX_MAS+(int)(double)s_35e[D(___243c60h)].XLocation-0x14+TRX_WIDTH*(int)ecx+(int)(double)s_35e[D(___243c60h)].__15a;
	esi = 0xf;

	j = -1;
	while(++j < 0x28){

		breaker = 0;

		i = -1;
		while(++i < 0x28){

			if(((signed char)B(ebxp+0x28*j+i) > 3)&&((signed char)(B(edip+TRX_WIDTH*j+i)&0xf) < 4)){

				esi = B(edip+TRX_WIDTH*j+i)&0xf;
				D(___243cb0h) = i;
				D(___243cb4h) = j;
				breaker = 1;
				break;
			}
		}

		if(breaker == 1) break;
	}

	s_35e[D(___243c60h)].__14 = esi;

	if((int)esi >= 4){

		s_35e[D(___243c60h)].__14 = 0;
	}
	else {

		s_35e[D(___243c60h)].__172 = D(___243cb0h)-0x14;
		edx = D(___243cb4h);
		s_35e[D(___243c60h)].__18e = esi;
		edx = edx-0x14;
		ecx = s_35e[D(___243c60h)].__192;
		s_35e[D(___243c60h)].__176 = edx;

		if((int)ecx > 3){

			s_35e[D(___243c60h)].__b0 = 0.0f;
			s_35e[D(___243c60h)].__16a = 1;
			s_35e[D(___243c60h)].__16e = 0;

#if defined(DR3_CHAOS)
			dr3_chaos_spins = 0;
#endif
			while(1){

				if((int)s_35e[D(___243c60h)].__14 >= 4){

					s_35e[D(___243c60h)].__14 = 0;
					break;
				}

#if defined(DR3_CHAOS)
				/*
				 * Search attempts per frame (see dr3_chaos.h): every attempt scans the whole 40x40 sprite
				 * against the track mask, so this loop is by far the most expensive thing a car does -
				 * with twelve cars on a track full of soft ground (Rock Zone: 60% soft) the frame was
				 * spent here (measured: 630 ms of every second in the per car loops).  A car that gives up
				 * here simply tries again on the next frame, so a low bound costs nothing but a moment.
				 */
				if(++dr3_chaos_spins > 16){

					s_35e[D(___243c60h)].__14 = 0;
					break;
				}
#endif

				eax = rand_watcom106();
				ebx = 2*s_35e[D(___243c60h)].__16a+1;
				edx = (int)eax>>0x1f;
				edx = (int)eax%(int)ebx;
				edx = edx-s_35e[D(___243c60h)].__16a;
				D(esp+0x5c) = edx;
				s_35e[D(___243c60h)].__162 = (float)(double)(int)D(esp+0x5c);
				eax = rand_watcom106();
				ebx = 2*s_35e[D(___243c60h)].__16a+1;
				edx = (int)eax>>0x1f;
				edx = (int)eax%(int)ebx;
				edi = s_35e[D(___243c60h)].__16e+1;
				D(esp+0x5c) = edx-s_35e[D(___243c60h)].__16a;
				eax = edi;
				edx = (int)eax>>0x1f;
				edx = (int)eax%5;
				s_35e[D(___243c60h)].__16e = edi;
				s_35e[D(___243c60h)].__166 = (float)(int)D(esp+0x5c);

				if(edx == 0){
										
					if((int)s_35e[D(___243c60h)].__16a < 0x1e) s_35e[D(___243c60h)].__16a++; 
				}

				s_35e[D(___243c60h)].YLocation += s_35e[D(___243c60h)].__166;
				D(esp+0x5c) = (int)((double)s_35e[D(___243c60h)].YLocation+(double)s_35e[D(___243c60h)].__15e-20.0);
				edi = TRX_WIDTH*D(esp+0x5c);
				s_35e[D(___243c60h)].XLocation += s_35e[D(___243c60h)].__162;
				edip = TRX_MAS+(int)s_35e[D(___243c60h)].XLocation+(int)edi-0x14+(int)(double)s_35e[D(___243c60h)].__15a;
				ebxp = ___243d74h+s_35e[D(___243c60h)].ImgOffset;
				esi = 0xf;
				edx = 0;
				eax = 0;

				ecx = 0;
				while(1){

					if((signed char)B(ebxp) <= 3){

						edip++;
						ebxp++;
						ecx++;

						if(ecx == 0x28){

							edip = edip+TRX_WIDTH-0x28;
							edx++;
							if(edx == 0x28) break;
							ecx = 0;
						}
					}
					else {

						L(eax) = B(edip)&0xf;

						if((signed char)L(eax) >= 4){

							edip++;
							ebxp++;
							ecx++;

							if(ecx == 0x28){

								edip = edip+TRX_WIDTH-0x28;
								edx++;
								if(edx == 0x28) break;
								ecx = 0;
							}
						}
						else {

							esi = eax;
							D(___243cb0h) = ecx;
							D(___243cb4h) = edx;
							break;
						}
					}
				} // w

				s_35e[D(___243c60h)].__14 = esi;

				if((int)esi < 4){
					
					s_35e[D(___243c60h)].YLocation -= s_35e[D(___243c60h)].__166;
					s_35e[D(___243c60h)].XLocation -= s_35e[D(___243c60h)].__162;
				}
			} // w
		} // i

		if((s_35e[D(___243c60h)].__192 != 0)&&((int)s_35e[D(___243c60h)].__192 <= 3)){

			s_35e[D(___243c60h)].__fc = (float)(-1.0*(double)s_35e[D(___243c60h)].__15a);
			s_35e[D(___243c60h)].__100 = (float)(-1.0*(double)s_35e[D(___243c60h)].__15e);
			s_35e[D(___243c60h)].__b0 = 0.0f;
			s_35e[D(___243c60h)].__14 = 1;
		}

		if(s_35e[D(___243c60h)].__192 == 0){
	
			if(((int)D(esp+0x48) < 4)||((int)D(esp+0x4c) < 4)){

				if((int)D(esp+0x2c) >= 4){
					
					s_35e[D(___243c60h)].Direction += 20.0f;
				}
			}

			if(((int)D(esp+0x50) < 4)||((int)D(esp+0x54) < 4)){

				if((int)D(esp+0x2c) >= 4){
				
					s_35e[D(___243c60h)].Direction -= 20.0f;
				}
			}

			if(s_35e[D(___243c60h)].Direction < 0.0f) s_35e[D(___243c60h)].Direction += 360.0f;
			if(s_35e[D(___243c60h)].Direction >= 360.0f) s_35e[D(___243c60h)].Direction -= 360.0f;

			s_35e[D(___243c60h)].__b0 *= (float)(7.0/8.0);

			s_35e[D(___243c60h)].__14 = 1;
		} // i
	} // i

	if((D(esp+0x50) == 4)||(D(esp+0x48) == 4)||(D(esp+0x4c) == 4)||(D(esp+0x54) == 4)||(D(esp+0x40) == 4)||(D(esp+0x44) == 4)){

		s_35e[D(___243c60h)].__b0 *= (float)(34.0/35.0);
	}

	if((D(esp+0x48) == 5)||(D(esp+0x4c) == 5)){

		s_35e[D(___243c60h)].__104 += 0.2f;
		s_35e[D(___243c60h)].__b0 *= (float)(29.0/30.0);
	}

	if((D(esp+0x50) == 5)||(D(esp+0x54) == 5)){

		s_35e[D(___243c60h)].__104 -= 0.2f;
		s_35e[D(___243c60h)].__b0 *= (float)(29.0/30.0);
	}

	if((D(esp+0x50) == 6)||(D(esp+0x48) == 6)||(D(esp+0x4c) == 6)||(D(esp+0x54) == 6)||(D(esp+0x40) == 6)||(D(esp+0x44) == 6)){

		s_35e[D(___243c60h)].__b0 *= (float)(8.0/9.0);
	}

	if((D(esp+0x50) == 7)||(D(esp+0x48) == 7)||(D(esp+0x4c) == 7)||(D(esp+0x54) == 7)||(D(esp+0x40) == 7)||(D(esp+0x44) == 7)){

		s_35e[D(___243c60h)].__b0 *= (float)(19.0/20.0);
	}

	if((D(esp+0x50) == 0xa)||(D(esp+0x4c) == 0xa)){

		s_35e[D(___243c60h)].__104 -= 0.15f;
	}

	if((D(esp+0x48) == 0xa)||(D(esp+0x54) == 0xa)){

		s_35e[D(___243c60h)].__104 += 0.15f;
	}

	if((D(esp+0x50) == 0xb)||(D(esp+0x48) == 0xb)||(D(esp+0x4c) == 0xb)||(D(esp+0x54) == 0xb)||(D(esp+0x40) == 0xb)||(D(esp+0x44) == 0xb)){

		s_35e[D(___243c60h)].__b0 *= (float)(76.0/75.0);
	}

	if((D(esp+0x50) == 0xc)||(D(esp+0x4c) == 0xc)){

		s_35e[D(___243c60h)].__104 -= 0.05f;
	}

	if((D(esp+0x48) == 0xc)||(D(esp+0x54) == 0xc)){

		s_35e[D(___243c60h)].__104 += 0.05f;
	}

	if((D(esp+0x50) == 0xd)||(D(esp+0x48) == 0xd)||(D(esp+0x4c) == 0xd)||(D(esp+0x54) == 0xd)||(D(esp+0x40) == 0xd)||(D(esp+0x44) == 0xd)){

		F64(esp+0x10) = 7.0*(double)s_35e[D(___243c60h)].__b0;
		s_35e[D(___243c60h)].Direction += (float)((double)rand_watcom106()*F64(esp+0x10)*(1.0/65536.0));
		F64(esp+0x10) = 7.0*(double)s_35e[D(___243c60h)].__b0;
		s_35e[D(___243c60h)].Direction -= (float)((double)rand_watcom106()*F64(esp+0x10)*(1.0/65536.0));
		if(s_35e[D(___243c60h)].Direction < 0.0f) s_35e[D(___243c60h)].Direction += 360.0f;
		if(s_35e[D(___243c60h)].Direction >= 360.0f) s_35e[D(___243c60h)].Direction -= 360.0f;
		s_35e[D(___243c60h)].__b0 *= (float)(29.0/30.0);
	}

	if((D(esp+0x50) == 0xe)||(D(esp+0x48) == 0xe)||(D(esp+0x4c) == 0xe)||(D(esp+0x54) == 0xe)||(D(esp+0x40) == 0xe)||(D(esp+0x44) == 0xe)){

		s_35e[D(___243c60h)].__b0 *= (float)(39.0/40.0);
	}

	s_35e[D(___243c60h)].XLocation += s_35e[D(___243c60h)].__15a;
	s_35e[D(___243c60h)].YLocation += s_35e[D(___243c60h)].__15e;

	F32(esp+0x28) = (float)(int)(TRX_WIDTH-0x14);
	if(s_35e[D(___243c60h)].XLocation > F32(esp+0x28)) s_35e[D(___243c60h)].XLocation = F32(esp+0x28);
	if(s_35e[D(___243c60h)].XLocation < 20.0) s_35e[D(___243c60h)].XLocation = 20.0f;

	F32(esp+0x24) = (float)(int)(TRX_HEIGHT-0x14);		
	if(s_35e[D(___243c60h)].YLocation > F32(esp+0x24)) s_35e[D(___243c60h)].YLocation = F32(esp+0x24);
	if(s_35e[D(___243c60h)].YLocation < 20.0) s_35e[D(___243c60h)].YLocation = 20.0f;

#if defined(DR3_CHAOS)
	/*
	 * THE PARKED PLAYER CAR (see doc/host_harness.md).
	 *
	 * The harness holds gas/turbo/gun for the whole race and the input trace says the flags arrive
	 * (race___40164h.c), yet the player's car never leaves the grid while the AI cars lap it.  This prints
	 * the state of the player's car at the end of its own physics step: the controls the step actually read
	 * (Ctrls[getCounter(4)], the value of line 104), what came out of them (__b0, __15a/__15e are the per
	 * step movement) and the three things that leave a car standing still -
	 *
	 *   * health: the whole block at the top of this function runs only while s_94.__18 > 0, and HP is
	 *     "0x400*(100 - damage)" of the race entry (race___3f970h.c), so a car entered with damage 100 -
	 *     a destroyed car - never accelerates at all,
	 *   * a finished car (the race is over for it),
	 *   * the ground putting the car back: __192 counts the frames the car spent inside a wall
	 *     (race_main.c adds 2 per frame with __14 == 1, and __192 > 3 zeroes the speed in this function),
	 *     __196 the frames a collision pushed it back.
	 */
	if((int)D(___243c60h) == (int)D(MY_CAR_IDX)){

		const int		dr3_p = (int)D(___243c60h);
		struct_35e_t *	dr3_car = s_35e+dr3_p;

		++dr3_pl_calls;

		/* every car once, at the player's first step: who of the grid is hurt/dead and who has pedals */
		if((dr3_pl_calls == 1)&&dr3_chaos_dump_on()){

			int m = -1;

			while(++m < (int)D(NUM_OF_CARS)){

				dr3_log("[dr3] chaos: grid car %d/%d: hp %d damage %d finished %d ctrls[4] 0x%02x lap %d pos %d at %.1f,%.1f",
					m, (int)D(NUM_OF_CARS), (int)D(___1de580h+0x94*m+0x18), (int)D(___1de7d0h+0x54*m+0x10),
					(int)s_35e[m].Finished, (unsigned)s_35e[m].Ctrls[getCounter(4)], (int)s_35e[m].Lap, (int)s_35e[m].Position,
					(double)s_35e[m].XLocation, (double)s_35e[m].YLocation);
			}
		}

		if(((dr3_pl_calls <= 0x180)||((dr3_pl_calls & 0xf) == 0))&&dr3_chaos_dump_on()){

			dr3_log("[dr3] chaos: PLAYER step %u: ctrls 0x%02x (index %d of %d) hp %d power %.1f turbo %d ammo %d finished %d lap %d pos %d "
				"at %.1f,%.1f delta %.3f,%.3f speed %.3f dir %.1f stuck %d veer %d ground %d,%d,%d,%d,%d,%d mid %d bump %d push %d "
				"mine %d spins %d",
				dr3_pl_calls, (unsigned)dr3_car->Ctrls[getCounter(4)], (int)getCounter(4), (int)getCounter(5),
				(int)D(___1de580h+0x94*dr3_p+0x18), (double)F32(___1de580h+0x94*dr3_p+4), (int)D(___1de580h+0x94*dr3_p+0x34),
				(int)D(___1de580h+0x94*dr3_p+0x30),
				(int)dr3_car->Finished, (int)dr3_car->Lap, (int)dr3_car->Position,
				(double)dr3_car->XLocation, (double)dr3_car->YLocation,
				(double)dr3_car->__15a, (double)dr3_car->__15e, (double)dr3_car->__b0, (double)dr3_car->Direction,
				(int)dr3_car->__192, (int)dr3_car->__14,
				(int)D(esp+0x2c), (int)D(esp+0x48), (int)D(esp+0x4c), (int)D(esp+0x50), (int)D(esp+0x54), (int)D(esp+0x40),
				(int)helper00((int)dr3_car->YLocation*TRX_WIDTH+(int)dr3_car->XLocation),
				(int)dr3_car->__186, (int)dr3_car->__196, (int)dr3_car->MineTimer, dr3_chaos_spins);
		}
	}
#endif
}
