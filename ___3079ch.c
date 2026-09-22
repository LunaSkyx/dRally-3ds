#include "drally.h"
#include "drally_fonts.h"
#include "drally_structs_fixed.h"
#include "drally_structs_free.h"

	extern __BYTE__ ___1a1f64h[];
	extern __BYTE__ ___1a1ef8h[];
	extern __BYTE__ ___1a0f04h[];
	extern __BYTE__ ___1a0ef8h[];
	extern __BYTE__ ___1a01e0h[];
	extern __POINTER__ ___1a10b8h;
	extern __BYTE__ ___185c7ah[];

void ___12e78h_cdecl(__BYTE__ * A1, font_props_t * A2, const char * A3, __DWORD__ dst_off);
void ___12cb8h__VESA101_PRESENTSCREEN(void);
char * itoa_watcom106(int value, char * buffer, int radix);
int rand_watcom106(void);

extern cardata_t ___18e298h[7];

/*
 * The adversary ("pedal to the metal", see doc/3ds.md): one of the AI seats belongs to him - his own
 * name, his own car (the SPECIAL), full equipment and a head start in points, so he leads the
 * championship and is the one to beat.
 *
 * This is done here, when a race event is signed up for, and not when the roster is created: the
 * difficulty is picked in the licence screen, i.e. *after* the roster exists.  His car is the marker,
 * so the check survives the roster being sorted by points - and it makes the seeding a one-off, his
 * points then move like everybody else's.
 */
static void dr3_adversary_seed(void){

	racer_t * s_6c = (racer_t *)___1a01e0h;
	const int me  = (int)D(___1a1ef8h);
	int       i, best = 0;

	if(!dr3_adversary_active()) return;

	for(i = 0; i < 0x14; ++i){
		if((int)s_6c[i].car == DR3_ADVERSARY_CAR) return;           /* he is already in the game */
	}

	for(i = 0; i < 0x14; ++i){
		if((i != me) && (i != DR3_ADVERSARY_RACER) && ((int)s_6c[i].points > best)) best = (int)s_6c[i].points;
	}

	strcpy(s_6c[DR3_ADVERSARY_RACER].name, "ADVERSARY");
	s_6c[DR3_ADVERSARY_RACER].car    = DR3_ADVERSARY_CAR;
	s_6c[DR3_ADVERSARY_RACER].damage = 0;
	s_6c[DR3_ADVERSARY_RACER].engine = ___18e298h[DR3_ADVERSARY_CAR].n_engine_upgrades - 1;
	s_6c[DR3_ADVERSARY_RACER].tires  = ___18e298h[DR3_ADVERSARY_CAR].n_tire_upgrades - 1;
	s_6c[DR3_ADVERSARY_RACER].armor  = ___18e298h[DR3_ADVERSARY_CAR].n_armor_upgrades - 1;
	s_6c[DR3_ADVERSARY_RACER].points = best + DR3_ADVERSARY_LEAD;
	s_6c[DR3_ADVERSARY_RACER].rank   = 1;
	s_6c[DR3_ADVERSARY_RACER].refund = ___18e298h[DR3_ADVERSARY_CAR].price;
}

// RACE SIGNUP RANDOMIZATION
void ___3079ch_cdecl(__DWORD__ A1){

	__DWORD__ 	eax, ebx, ecx, edx, edi, esi, ebp;
	__BYTE__ 	esp[0x6c];
	int 		n, r;
	racer_t * 	s_6c;


	s_6c = (racer_t *)___1a01e0h;

	dr3_adversary_seed();

	if((rand_watcom106()%A1) == 0){
	
		n = -1;
		while(++n < 0x32){

			ebp = rand_watcom106()%3;
			if(B(___1a1f64h+ebp+3) <= 3) break;
		}

		if(n < 0x32){

			while(1){

				if(ebp == 0){

					n = -1;
					while(++n < 0x64){

						/*
						 * The adversary takes part like every other racer: he is one candidate for this
						 * slot, and the "picked" flag below makes sure that happens once per event (see
						 * doc/3ds.md).  His car is outside every class range tested here, so without
						 * this he would never be picked at all.
						 */
						if(dr3_adversary_active() && (B(___1a0f04h+DR3_ADVERSARY_RACER) != 1)){

							r = DR3_ADVERSARY_RACER;
							break;
						}

						r = rand_watcom106()%0x14;
						if((s_6c[r].car >= 0)&&(s_6c[r].car <= 2)) break;
					}
				}

				if(ebp == 1){

					n = -1;
					while(++n < 0x64){

						r = rand_watcom106()%0x14;

						if(((int)s_6c[D(___1a1ef8h)].car >= 0)&&((int)s_6c[D(___1a1ef8h)].car <= 2)){	

							if(((int)s_6c[r].car >= 1)&&((int)s_6c[r].car) <= 3) break;
						}

						if(((int)s_6c[D(___1a1ef8h)].car >= 3)&&((int)s_6c[D(___1a1ef8h)].car <= 5)){

							if(((int)s_6c[r].car >= 2)&&((int)s_6c[r].car <= 4)) break;
						}
					}
				}

				if(ebp == 2){

					n = -1;
					while(++n < 0x64){

						r = rand_watcom106()%0x14;
						if((s_6c[r].car >= 3)&&(s_6c[r].car <= 5)) break;
					}
				}

				if(B(___1a0f04h+r) != 1) break;
			}

			B(___1a0f04h+r) = 1;
			B(___1a0ef8h+B(___1a1f64h+ebp+3)+4*ebp) = r;
			B(___1a1f64h+ebp+3)++; 
			itoa_watcom106(s_6c[r].rank, esp+0x50, 0xa);
			strcpy(esp, "");
			if(strlen(esp+0x50) < 2)strcat(esp, " ");
			strcat(strcat(strcat(esp, esp+0x50), "."), s_6c[r].name);
			___12e78h_cdecl(___1a10b8h, (font_props_t *)___185c7ah, esp, 0x280*(0x12*B(___1a1f64h+ebp+3)+0x100)+0xa0*ebp+0x22);
			___12cb8h__VESA101_PRESENTSCREEN();
		}
	}
}
