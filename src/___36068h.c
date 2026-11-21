#include "drally.h"
#include "drally_fonts.h"
#include "drally_structs_fixed.h"

/* the chaos points of a race: dr3_chaos_points (drally.h), the racers of the cars (dr3_chaos.h) */
#include "platform_3ds/dr3_chaos.h"
#include "platform_3ds/dr3_log.h"      /* dr3_log - the rows this panel draws are logged below */
#include "platform_3ds/dr3_results.h"  /* dr3_results_rows - the four rows of this list        */

extern int NUM_OF_CARS;

	extern __BYTE__ ___1a1028h[];
	extern __BYTE__ ___1de7d0h[];
	extern __BYTE__ ___1a0ef8h[];
	extern __POINTER__ ___1a112ch__VESA101_ACTIVESCREEN_PTR;
	extern __POINTER__ ___1a0fech;
	extern __BYTE__ ___185c0bh[];
	extern __POINTER__ ___1a1108h;
	extern __BYTE__ ___1a01e0h[];
	extern __BYTE__ ___1a1ef8h[];
	extern __BYTE__ ___196ab0h[];
	extern __POINTER__ ___1a0fb0h;
	extern __BYTE__ ___185c7ah[];
	extern __POINTER__ ___1a10b8h;

void ___12cb8h__VESA101_PRESENTSCREEN(void);
void ___12e78h_cdecl(__BYTE__ * A1, font_props_t * A2, const char * A3, __DWORD__ dst_off);
void ___38184h_cdecl(__DWORD__, __POINTER__);
void ___35dd0h(void);
void ___35f34h_cdecl(__DWORD__, __DWORD__, __DWORD__);
char * itoa_watcom106(int value, char * buffer, int radix);

static const int EasyPoints[3] = {3, 2, 1};
static char Points[8];

static char * fmtPoints(int val){

	return itoa_watcom106(val, strcpy(Points, "+")+1, 10)-1;
}

// EASY RACE RESULTS
void ___36068h_cdecl(__DWORD__ A1){

    __DWORD__   eax, ebx, ecx, edx, edi, esi, ebp;
    __BYTE__    esp[8];
	int 		i, j, n;
	__BYTE__ 	px;
	racer_t * 	s_6c;

 
	s_6c = (racer_t *)___1a01e0h;

#if defined(DR3_CHAOS)
	if(dr3_chaos_active()){
		/*
		 * Chaos mode: all cars race in one field, so the top four finishers come from
		 * dr3_chaos_car_seat across NUM_OF_CARS (platform_3ds/dr3_results.h).
		 */
		int dr3_place[DR3_MAX_CARS];
		int dr3_seat[DR3_MAX_CARS];
		int dr3_rows[DR3_RESULTS_ROWS];
		int dr3_n;

		(void)eax; (void)ebx; (void)ecx; (void)edx; (void)edi; (void)esi; (void)ebp;

		for(dr3_n = 0; dr3_n < DR3_MAX_CARS; ++dr3_n){

			const int dr3_in = (dr3_n < NUM_OF_CARS);

			dr3_place[dr3_n] = dr3_in ? (int)D(___1de7d0h + 0x54*dr3_n + 0x40) : 0;
			dr3_seat[dr3_n]  = dr3_in ? (int)dr3_chaos_car_seat[dr3_n] : -1;
		}

		dr3_results_rows(dr3_rows, DR3_RESULTS_RACERS, dr3_seat, dr3_place, NUM_OF_CARS);

		for(dr3_n = 0; dr3_n < DR3_RESULTS_ROWS; ++dr3_n) B(esp+dr3_n) = (__BYTE__)dr3_rows[dr3_n];

		dr3_log("[dr3] chaos: results rows: 1. %s (racer %d), 2. %s (%d), 3. %s (%d), 4. %s (%d) out of %d car(s)",
		        s_6c[B(esp+0)].name, B(esp+0), s_6c[B(esp+1)].name, B(esp+1),
		        s_6c[B(esp+2)].name, B(esp+2), s_6c[B(esp+3)].name, B(esp+3), NUM_OF_CARS);
	}
	else
#endif
	{
		/* Classic modes (Speed makes me dizzy / I live to ride / Full throttle / 30th Anniv):
		   simulate Easy race if the player raced Medium/Hard, otherwise use the Easy race results. */
		if(D(___1a1028h)){

			___35f34h_cdecl(0, 3, 0);
			B(esp) = B(___1a0ef8h+3);
			B(esp+1) = B(___1a0ef8h+2);
			B(esp+2) = B(___1a0ef8h+1);
			B(esp+3) = B(___1a0ef8h);
		}
		else {

			for(n = 0; n < 4; ++n) B(esp+n) = B(n+___1a0ef8h);
			n = -1;
			while(++n < 4){

				if(D(0x54*n+D(___1a1028h)+___1de7d0h+0x40) == 1) B(esp) = B(n+___1a0ef8h);
				if(D(0x54*n+D(___1a1028h)+___1de7d0h+0x40) == 2) B(esp+1) = B(n+___1a0ef8h);
				if(D(0x54*n+D(___1a1028h)+___1de7d0h+0x40) == 3) B(esp+2) = B(n+___1a0ef8h);
				if(D(0x54*n+D(___1a1028h)+___1de7d0h+0x40) == 4) B(esp+3) = B(n+___1a0ef8h);
			}
		}
	}

	if(A1 == 0){

		___35dd0h();

		j = -1;
		while(++j < 0x14){

			i = -1;
			while(++i < 0x110){

				if((px = read_b(___1a0fech+0x110*j+i))) write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0xd362+0x280*j+i, px);
			}
		}

		{
			const char *dr3_title = "Easy Race Results";
			if(dr3_chaos_active()){
				if(dr3_chaos_arena()) dr3_title = "Arena Results";
				else if(dr3_chaos_mode() == DR3_CHAOS_MODE_DUEL) dr3_title = "Duel Results";
				else if(dr3_chaos_mode() == DR3_CHAOS_MODE_DM) dr3_title = "Destroy 'em All";
			}
			___12e78h_cdecl(___1a1108h, (font_props_t *)___185c0bh, dr3_title, 0xd890);
		}
		___38184h_cdecl(0x4, esp);
	}

	n = -1;
	while(++n < 3){

		if(dr3_chaos_active() && (n >= NUM_OF_CARS)) break;

		if((A1 == 0)&&(s_6c[B(esp+n)].damage != 0x64)){

			if(!dr3_chaos_active() && (B(esp+n) == D(___1a1ef8h))&&(D(___196ab0h) == 1)) break;

			j = -1;
			while(++j < 0x12){

				i = -1;
				while(++i < 0x2a){

					if((px = B(___1a0fb0h+0x2a*j+i))) write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x280*(0x13*B(esp+n)+0x57)+0xd9+0x280*j+i, px);
				}
			}
		}

		if((s_6c[B(esp+n)].damage != 0x64)&&((B(esp+n) != D(___1a1ef8h))||(D(___196ab0h) != 1))){

			/* chaos scores every place at the end of the race - see the loop below */
			if(!dr3_chaos_active()){

				s_6c[B(esp+n)].points += EasyPoints[n];
				___12e78h_cdecl(___1a10b8h, (font_props_t *)___185c7ah, fmtPoints(EasyPoints[n]), 0x280*(0x13*B(esp+n)+0x59)+0xe6);
			}
		}

	}

	if(A1 == 0){
	
	/*
	 * The "+N" marks of a chaos race.  Every racer of the field scored already - the points are
	 * credited at the end of the race (___33010h.c, drally.h: dr3_chaos_points) - and here every row of
	 * the standings gets its mark, not only the three podium places the table of this screen reaches.
	 * Which racer sits in car n is dr3_chaos_car_seat (dr3_chaos.h).
	 */
	if(dr3_chaos_active()){

		int dr3_c;

		for(dr3_c = 0; dr3_c < (int)NUM_OF_CARS; ++dr3_c){

			const int dr3_place = (int)D(___1de7d0h + 0x54*dr3_c + 0x40);
			const int dr3_seat  = (int)dr3_chaos_car_seat[dr3_c];
			char      dr3_buf[8];

			if((dr3_place < 1)||(dr3_place > (int)NUM_OF_CARS)) continue;
			if((dr3_seat < 0)||(dr3_seat >= 0x14)) continue;
			if(dr3_chaos_car_gain[dr3_c] <= 0) continue;

			strcpy(dr3_buf, "+");
			/*
			 * What the car scored, taken from the race itself (___33010h.c): a duel pays Elo, which the
			 * place curve of this screen does not know.
			 */
			itoa_watcom106(dr3_chaos_car_gain[dr3_c], dr3_buf + 1, 0xa);

			___12e78h_cdecl(___1a10b8h, (font_props_t *)___185c7ah, dr3_buf, 0x280*(0x13*dr3_seat+0x59)+0xe6);
		}
	}

		___12e78h_cdecl(___1a1108h, (font_props_t *)___185c0bh, "Press any key to continue...", 0x46b6e);
		___12cb8h__VESA101_PRESENTSCREEN();
	}
}
