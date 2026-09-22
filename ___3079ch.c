#include "drally.h"
#include "drally_fonts.h"
#include "drally_structs_fixed.h"
#include "drally_structs_free.h"

	extern __BYTE__ ___1a1f64h[];
	extern __BYTE__ ___1a1ef8h[];
	extern __BYTE__ ___1a0f04h[];
	extern __BYTE__ ___1a0ef8h[];
	extern __BYTE__ ___1a01e0h[];
	extern __BYTE__ ___185a50h[];
	extern __POINTER__ ___1a10b8h;
	extern __BYTE__ ___185c7ah[];

void ___12e78h_cdecl(__BYTE__ * A1, font_props_t * A2, const char * A3, __DWORD__ dst_off);
void ___12cb8h__VESA101_PRESENTSCREEN(void);
char * itoa_watcom106(int value, char * buffer, int radix);
int rand_watcom106(void);

extern cardata_t ___18e298h[7];

extern __POINTER__ ___19de70h[20];                 /* MENU.BPA face01..face20, 0x40x0x40 each */

void DISPLAY_GET_PALETTE(unsigned char * dst);

static int dr3_adversary_seat(void);

/*
 * Make the adversary's driver picture black and white.  The faces are palette-indexed pictures, so this
 * rewrites the one that belongs to him once: every pixel index is replaced by the index of the closest
 * *neutral grey* of the same brightness in the palette that is active at that moment - and those greys
 * are the ones the front end itself draws its frames and dialogs with, so the picture stays black and
 * white on every screen that shows it (the signup roster, the licence screen, the standings).
 */
static void dr3_adversary_face_mono(void){

	static const void * done;                      /* the picture that was converted already */
	unsigned char       pal[0x100*3], grey[0x100];
	const int           seat = dr3_adversary_seat();
	const int           face = (seat < 0) ? -1 : (int)((racer_t *)___1a01e0h)[seat].face;
	__BYTE__ *          pic;
	int                 i, n;

	if((face < 0) || (face > 0x13)) return;

	pic = (__BYTE__ *)___19de70h[face];
	if((pic == NULL) || ((const void *)pic == done)) return;

	DISPLAY_GET_PALETTE(pal);

	for(i = 0; i < 0x100; ++i){

		const int lum = (2*(int)pal[3*i] + 5*(int)pal[3*i+1] + (int)pal[3*i+2]) / 8;
		int       best = 0, best_d = 0x7fffffff;

		for(n = 0; n < 0x100; ++n){

			const int dr = (int)pal[3*n]   - lum;
			const int dg = (int)pal[3*n+1] - lum;
			const int db = (int)pal[3*n+2] - lum;
			const int d  = 5*dr*dr + 12*dg*dg + 3*db*db;      /* weight like the eye does */

			if(d < best_d){ best_d = d; best = n; }
		}

		grey[i] = (unsigned char)best;
	}

	for(i = 0; i < 0x1000; ++i) pic[i] = grey[pic[i]];

	done = pic;

	dr3_log("[dr3] adversary: driver picture face %d converted to black and white", face);
}

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

	/*
	 * He starts third: the two best scores of the others stay ahead of him, so the first race is about
	 * beating him - and he works his way up from there (see doc/3ds.md).  He is that good.
	 */
	{
		int first = 0, second = 0;

		for(i = 0; i < 0x14; ++i){

			if((i == me) || (i == DR3_ADVERSARY_RACER)) continue;

			if((int)s_6c[i].points >= first){ second = first; first = (int)s_6c[i].points; }
			else if((int)s_6c[i].points > second) second = (int)s_6c[i].points;
		}

		best = (second > 0) ? (second - 1) : 0;
	}

	strcpy(s_6c[DR3_ADVERSARY_RACER].name, "ADVERSARY");
	s_6c[DR3_ADVERSARY_RACER].car    = DR3_ADVERSARY_CAR;
	s_6c[DR3_ADVERSARY_RACER].damage = 0;
	s_6c[DR3_ADVERSARY_RACER].engine = ___18e298h[DR3_ADVERSARY_CAR].n_engine_upgrades - 1;
	s_6c[DR3_ADVERSARY_RACER].tires  = ___18e298h[DR3_ADVERSARY_CAR].n_tire_upgrades - 1;
	s_6c[DR3_ADVERSARY_RACER].armor  = ___18e298h[DR3_ADVERSARY_CAR].n_armor_upgrades - 1;
	s_6c[DR3_ADVERSARY_RACER].points = best;
	s_6c[DR3_ADVERSARY_RACER].rank   = DR3_ADVERSARY_START_RANK;
	s_6c[DR3_ADVERSARY_RACER].refund = ___18e298h[DR3_ADVERSARY_CAR].price;

	dr3_log("[dr3] adversary: created in seat %d, %d points (should be rank %d)",
	        DR3_ADVERSARY_RACER, (int)s_6c[DR3_ADVERSARY_RACER].points, DR3_ADVERSARY_START_RANK);

	dr3_adversary_face_mono();
}

/*
 * The seat the adversary sits in - and it is *not* a constant: the roster is sorted by points after
 * every race (___30a84h), so he moves up the table as he wins.  His car is the marker, and no race
 * result ever changes it.  Returns -1 while he is not in the game.
 */
static int dr3_adversary_seat(void){

	racer_t * s_6c = (racer_t *)___1a01e0h;
	int       i;

	if(!dr3_adversary_active()) return -1;

	/*
	 * His car is the marker - and the name is the fallback, so a roster that was rebuilt (or a car that
	 * was reset) does not lose him: the car is simply put back.
	 */
	for(i = 0; i < 0x14; ++i){
		if((int)s_6c[i].car == DR3_ADVERSARY_CAR) return i;
	}

	for(i = 0; i < 0x14; ++i){

		if(i == (int)D(___1a1ef8h)) continue;          /* not the player, whatever he calls himself */

		if(strcmp(s_6c[i].name, "ADVERSARY") == 0){
			s_6c[i].car = DR3_ADVERSARY_CAR;
			return i;
		}
	}

	return -1;
}

/*
 * Draw one entry of a signup list - the very call the picker makes for every opponent it picks, so an
 * entry added from here looks like all the others.  The list rows are indexed by the slot counter *after*
 * the increment, exactly like in the picker.
 */
static void dr3_adversary_draw(int tier, int slot, int rank, const char * name){

	char esp[0x64];

	itoa_watcom106(rank, esp+0x50, 0xa);
	strcpy(esp, "");
	if(strlen(esp+0x50) < 2) strcat(esp, " ");
	strcat(strcat(strcat(esp, esp+0x50), "."), name);
	___12e78h_cdecl(___1a10b8h, (font_props_t *)___185c7ah, esp,
	                0x280*(0x12*(slot+1)+0x100)+0xa0*tier+0x22);
	___12cb8h__VESA101_PRESENTSCREEN();
}

/*
 * Set while the adversary has a place in the event that is currently being set up.  It is cleared in
 * dr3_adversary_enter() (i.e. when a signup starts and the event's field is cleared), so it is a
 * per-event state and not a guess: scanning the field instead looked safe but a stale field (from the
 * race he just took part in) made this code believe he was entered already - and he never joined again.
 */
static int dr3_adversary_placed;

/*
 * Make sure he exists before the signup lists are filled: the picker offers him as a candidate, and the
 * seeding is a one-off (his car is the marker).  This runs right after the event's field was cleared -
 * the lists themselves are drawn by the picker, so nothing is drawn here (the screen paints its
 * background right after this point anyway).
 */
void dr3_adversary_enter(void){

	dr3_adversary_placed = 0;              /* a new event: he has no race yet */

	dr3_adversary_seed();

	dr3_adversary_face_mono();             /* in case his picture changed in the meantime */
}

/*
 * Is the adversary still to be placed in the event that is being set up?
 */
static int dr3_adversary_pending(void){

	return dr3_adversary_placed ? 0 : (dr3_adversary_seat() >= 0);
}

/*
 * Make sure he is registered in exactly one of the three races of this event - and once he is, he stays
 * there: the race he was entered in is the one he drives, whether or not the player picks it.  Without a
 * home in the event he would collect no points at all.
 *
 * Called when the player confirms a signup, i.e. after the field is complete.  The picker normally puts
 * him somewhere while the lists are filling up; this only catches the case that it did not.  It then
 * picks one of the *other* two races: meeting him is meant to be the hard race, and that should stay the
 * player's own choice.  The player's slot in a grid is never touched.
 */
void dr3_adversary_ensure(void){

	racer_t * s_6c = (racer_t *)___1a01e0h;
	const int seat = dr3_adversary_seat();
	const int me   = (int)D(___1a1ef8h);
	int       i, pass;

	if((seat < 0) || dr3_adversary_placed) return;

	for(pass = 0; pass < 2; ++pass){

		for(i = 0; i < 0xc; ++i){

			if(B(___1a0ef8h+i) == me) continue;                 /* never the player's own slot */
			if((pass == 0) && ((i/4) == (int)D(___185a50h))) continue;   /* first the other races */

			B(___1a0ef8h+i) = seat;
			dr3_adversary_placed = 1;

			/* the lists were drawn while they filled up, so he has to be drawn here too */
			dr3_adversary_draw(i/4, i%4, (int)s_6c[seat].rank, s_6c[seat].name);

			dr3_log("[dr3] adversary: entered in tier %d as slot %d (pass %d)", i/4, i%4, pass);
			return;
		}
	}
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

		dr3_log("[dr3] adversary: diff %d, seat %d; field %d,%d,%d,%d / %d,%d,%d,%d / %d,%d,%d,%d; "
		        "counters %d,%d,%d; selected tier %d",
		        (int)___196a94h_difficulty, dr3_adversary_seat(),
		        B(___1a0ef8h+0), B(___1a0ef8h+1), B(___1a0ef8h+2), B(___1a0ef8h+3),
		        B(___1a0ef8h+4), B(___1a0ef8h+5), B(___1a0ef8h+6), B(___1a0ef8h+7),
		        B(___1a0ef8h+8), B(___1a0ef8h+9), B(___1a0ef8h+0xa), B(___1a0ef8h+0xb),
		        B(___1a1f64h+3), B(___1a1f64h+4), B(___1a1f64h+5), (int)D(___185a50h));

		n = -1;
		while(++n < 0x32){

			ebp = rand_watcom106()%3;
			if(B(___1a1f64h+ebp+3) <= 3) break;
		}

		if(n < 0x32){

			while(1){

				/*
				 * The adversary rides along like any other candidate: the first free slot of the event is
				 * his, and dr3_adversary_pending() keeps that to one race per event.  He goes through the
				 * same bookkeeping as everybody else, so counters and flags stay consistent - and whichever
				 * of the three races he lands in is the one he drives (dr3_adversary_ensure() only steps in
				 * if the picker missed him completely).
				 */
				if(dr3_adversary_pending()){

					r = dr3_adversary_seat();
					dr3_adversary_placed = 1;
					dr3_log("[dr3] adversary: placed in tier %d as slot %d", ebp, B(___1a1f64h+ebp+3));
					break;
				}

				if(ebp == 0){

					n = -1;
					while(++n < 0x64){

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
