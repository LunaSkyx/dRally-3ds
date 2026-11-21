#include "drally.h"
#include "drally_structs_free.h"
#include "drmath.h"
#include "sfx.h"
#include "platform_3ds/dr3_chaos.h"
#include "platform_3ds/dr3_boss.h"

	extern __BYTE__ ___243c60h[];
	extern __BYTE__ ___1e6ed0h[];
	extern __BYTE__ ___1de580h[];
	extern __BYTE__ ___1df520h[];
	extern int MY_CAR_IDX;
	extern __POINTER__ RACE_MINES1A_BPK;
	extern __POINTER__ TRX_IMA;
	extern int TRX_WIDTH;
	extern int TRX_HEIGHT;
	extern __POINTER__ ___243d74h;
	extern __POINTER__ RACE_BLOWI_BPK;

int rand_watcom106(void);
void dRally_Sound_pushEffect(__BYTE__ channel, __BYTE__ n, __DWORD__ unk, __DWORD__ a0, __DWORD__ a1, __DWORD__ a2);

#define LOC_83 0.833333
#define LOC_PI 3.14159265359

static int helper00(double dval){

	return ((dval-(double)(int)dval) < 0.5)?(int)dval:(int)(1.0+dval);
}

// MINES
void race___514d8h(void){

	__DWORD__ 	eax, ebx, ecx, edx, edi;
	__BYTE__ 	esp[0xc8];
	__POINTER__ 	edxp;
	int 			i, j, n;
	__BYTE__ 		px;
	struct_94_t * 	s_94;
	struct_35e_t * 	s_35e;


	s_94 = (struct_94_t *)___1de580h;
	s_35e = (struct_35e_t *)___1e6ed0h;
	n = D(___243c60h);

	if((int)s_35e[n].__1a6 > 0) s_35e[n].__1a6--;

	if((int)s_94[n].__18 > 0){

		if(s_35e[n].Finished == 0){

			if((s_35e[n].Ctrls[getCounter(4)]&0x40) != 0){

				if((int)s_94[n].__28 > 0){

					if((int)getCounter(1) > 0x1ae){

						if(s_35e[n].__1a6 == 0){

							if(((s_35e[n].Ctrls[getCounter(4)]&2) == 0)||(( s_35e[n].Ctrls[getCounter(4)]&0x40) == 0)){

								s_35e[n].__1a6 = 0x32;
								D(esp+0x60) = helper00(2.3*dRMath_sin((double)s_35e[n].Direction*LOC_PI/180.0)*(double)s_94[n].__10);
								D(esp+0x88) = (int)((double)(int)D(esp+0x60)+(double)s_35e[n].XLocation-4.0);
								D(esp+0x70) = helper00(2.3*dRMath_cos((double)s_35e[n].Direction*LOC_PI/180.0)*LOC_83*(double)s_94[n].__10);
								D(esp+0x74) = (int)((double)(int)D(esp+0x70)+(double)s_35e[n].YLocation-4.0);
								edi = getCounter(6);
								ebx = 0;

								if(((int)edi >= DR3_MAX_MINES)||
								   ((int)D(esp+0x88) < 8)||((int)D(esp+0x88) > TRX_WIDTH-16)||
								   ((int)D(esp+0x74) < 8)||((int)D(esp+0x74) > TRX_HEIGHT-16)) ebx = 1;

								if((ebx == 0)&&((int)edi > 0)){

									ecx = 0x10*edi;

									eax = 0;
									while(1){

										if(dRMath_abs_i(D(___1df520h+eax)-D(esp+0x88)-4) < 8){

											if(dRMath_abs_i(D(___1df520h+eax+4)-D(esp+0x74)-4) < 8) ebx = 1;
										}

										eax += 0x10;
										if((int)eax >= (int)ecx) break;
									}
								}

								if(ebx == 0){

									if(n != MY_CAR_IDX){

										eax = (int)(s_35e[n].XLocation-s_35e[MY_CAR_IDX].XLocation);
										edx = (int)(s_35e[n].YLocation-s_35e[MY_CAR_IDX].YLocation);
										ecx = 0x10000-75*dRMath_magnitude2_i(eax, edx);
			
										if((int)ecx > 0x1000){

											dRally_Sound_pushEffect(4, SFX_MINE, 0, ecx, 0x21000, 0x8000);	// DROPPING MINE
										}
									}
									else {

										dRally_Sound_pushEffect(4, SFX_MINE, 0, 0x9000, 0x21000, 0x8000);	// DROPPING MINE
									}

									edx = getCounter(6);
									D(___1df520h+0x10*edx) = D(esp+0x88)+0x4;
									D(___1df520h+0x10*edx+4) = D(esp+0x74)+0x4;

			/*
			 * Who laid it (the list has that field free): the car index of the layer, which is `n`
			 * here - this loop runs once per car.  The chaos kill bonus reads it when the mine blows
			 * up (dr3_chaos_wreck in race___4dcach.c), so the player's own mines pay for the wrecks
			 * they cause, exactly like his bumper or his gun does.
			 */
			D(___1df520h+0x10*edx+0xc) = n;
									eax = getCounter(6);
									eax++;
									setCounter(6, eax);

									__BYTE__* s = RACE_MINES1A_BPK;
									__BYTE__* d = TRX_IMA + TRX_WIDTH * (D(esp + 0x74)) + D(esp + 0x88);
									int stride = TRX_WIDTH - 8;
									j = 8;
									do {
										i = 8;
										do {
											if ((px = *s++) != 0) *d = px;
											d++;
										} while (--i);
										d += stride;
									} while (--j);

									s_94[n].__28--;
								}
							}
						}
					}
				}
			}
		}
	}

	D(esp+0x7c) = 0;

	if((int)getCounter(6) > 0){

		edi = 0;
		while(1){

			edx = (int)(double)s_35e[n].XLocation-D(___1df520h+edi);
			eax = (int)(double)s_35e[n].YLocation-D(___1df520h+edi+4);

			{
				const int giant = dr3_boss_is_giant_car(n);
				const int mlim  = giant ? 30 : 0x14;
				const int mdx   = giant ? (((int)edx * 2) / 3) : (int)edx;
				const int mdy   = giant ? (((int)eax * 2) / 3) : (int)eax;

				if((dRMath_abs_i(edx) < mlim)&&(dRMath_abs_i(eax) < mlim)){

					eax = 0x28*(mdy+0x14);
					edxp = ___243d74h+(int)s_35e[n].ImgOffset+mdx;

					if(B(edxp+(int)eax+0x14) > 3){

					if(D(___1df520h+edi+8) == -1){

						__BYTE__* s = RACE_BLOWI_BPK + 0x500;
						__BYTE__* d = TRX_IMA + D(___1df520h + edi) + TRX_WIDTH * (D(___1df520h + edi + 4) - 8) - 8;
						int stride = TRX_WIDTH - 0x10;
						j = 0x10;
						do {
							i = 0x10;
							do {
								if ((px = *s++) != 0) *d = px;
								d++;
							} while (--i);
							d += stride;
						} while (--j);

						D(___1df520h+edi+8) = 0;

						{
							const int dr3_hp_before = (int)s_94[n].__18;
							const int dr3_owner     = (int)D(___1df520h+edi+0xc);

							if(s_35e[n].Finished == 0) s_94[n].__18 -= 0x14*(0x400-s_94[n].__1c);

							if((int)s_94[n].__18 < 0) s_94[n].__18 = 0;

							/*
							 * A mine or an explosion took this car's HP (RACE_BLOWI_BPK is its picture).  The
							 * list keeps who laid the mine (+0xc, written where the mine is put down), so a car
							 * that drives into the *player's* mine is his wreck and pays like a ram - that is
							 * what dr3_chaos_player_damage() marks.  Any other mine (or an explosion nobody
							 * owns) still pays when the player hit or bumped that car a moment ago, which is
							 * what the blame window in dr3_chaos_wreck() is for.
							 */
							if(dr3_owner == MY_CAR_IDX) dr3_chaos_player_damage(n);

							dr3_chaos_wreck(n, dr3_hp_before,
							                (dr3_owner == MY_CAR_IDX) ? "the player's mine" : "a mine or an explosion");
						}

						s_35e[n].__fc += (float)(0.5*(double)(int)(rand_watcom106()%3-1));
						s_35e[n].__100 += (float)(0.5*(double)(int)(rand_watcom106()%3-1));
						s_35e[n].XLocation += (float)(int)(rand_watcom106()%11-5);
						s_35e[n].YLocation += (float)(int)(rand_watcom106()%11-5);
						s_35e[n].__104 = (float)(int)(rand_watcom106()%22-10);
						s_35e[n].__b0 *= -0.7f;

						if(n != MY_CAR_IDX){

							eax = (int)(s_35e[n].XLocation-s_35e[MY_CAR_IDX].XLocation);
							edx = (int)(s_35e[n].YLocation-s_35e[MY_CAR_IDX].YLocation);
							ecx = 0x10000-75*dRMath_magnitude2_i(eax, edx);
			
							if((int)ecx > 0x1000){

								dRally_Sound_pushEffect(4, SFX_MINE_EXPLOSION, 0, ecx, 0x21000, 0x8000);	// MINE EXPLOSION
							}
						}
						else {

							dRally_Sound_pushEffect(4, SFX_MINE_EXPLOSION, 0, 0x9000, 0x21000, 0x8000);	// MINE EXPLOSION
						}
					}
				}
				}
			}

			edx = getCounter(6);
			edi += 0x10;
			D(esp+0x7c)++;
			if((int)D(esp+0x7c) >= (int)edx) break;
		}
	}

	return;
}
