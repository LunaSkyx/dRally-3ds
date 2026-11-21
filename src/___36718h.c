#include "drally.h"
#include "drally_fonts.h"
#include "drally_structs_fixed.h"

/* the chaos points of a race: dr3_chaos_points (drally.h), the racers of the cars (dr3_chaos.h) */
#include "platform_3ds/dr3_chaos.h"
#include "platform_3ds/dr3_log.h"      /* dr3_log - the rows this panel draws are logged below */
#include "platform_3ds/dr3_results.h"  /* dr3_results_rows - the four rows of this list        */

extern int NUM_OF_CARS;

#pragma pack(1)
typedef struct x655_s {
	__DWORD__ 	r;
	__DWORD__ 	g;
	__DWORD__ 	b;
} x655_t;

	extern x655_t ___19eb50h[0x100];

	extern __BYTE__ ___1a1028h[];
	extern __BYTE__ ___1de7d0h[];
	extern __BYTE__ ___1a0ef8h[];
	extern __POINTER__ ___1a112ch__VESA101_ACTIVESCREEN_PTR;
	extern __POINTER__ ___1a0fe4h;
	extern __BYTE__ ___185c0bh[];
	extern __POINTER__ ___1a1108h;
	extern __BYTE__ ___1a01e0h[];
	extern __BYTE__ ___1a1ef8h[];
	extern __BYTE__ ___196ab0h[];
	extern __POINTER__ ___1a0fa8h;
	extern __BYTE__ ___185c7ah[];
	extern __POINTER__ ___1a10b8h;

void __DISPLAY_SET_PALETTE_COLOR(__DWORD__ b, __DWORD__ g, __DWORD__ r, __DWORD__ n);
void ___12cb8h__VESA101_PRESENTSCREEN(void);
void ___12e78h_cdecl(__BYTE__ * A1, font_props_t * A2, const char * A3, __DWORD__ dst_off);
void ___38184h_cdecl(__DWORD__, __POINTER__);
void ___35f34h_cdecl(__DWORD__, __DWORD__, __DWORD__);
char * itoa_watcom106(int value, char * buffer, int radix);
void ___35dd0h(void);

static __BYTE__ helper_color(__DWORD__ eax, __DWORD__ edx){

	eax = edx / 0x10000 * eax;
	eax += 0x8000;
	eax = (int)eax>>0x10;

	return eax&0xff;
}

// HARD RACE RESULTS
void ___36718h_cdecl(__DWORD__ A1){

	__DWORD__ 	eax, ebx, ecx, edx, edi, esi, ebp, rr, gg, bb, nn;
	__BYTE__ 	esp[8];
	__POINTER__ 	ebxp;
	__POINTER__ 	esip;
	racer_t * 		s_6c;


	s_6c = (racer_t *)___1a01e0h;
	eax = A1;
	ebp = eax;
	
#if defined(DR3_CHAOS)
	if(dr3_chaos_active()){
		int dr3_place[DR3_MAX_CARS];
		int dr3_seat[DR3_MAX_CARS];
		int dr3_rows[DR3_RESULTS_ROWS];
		int dr3_n;

		(void)ebx; (void)esi;

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
		if(D(___1a1028h) != 2){

			ebx = 0x2;
			edx = 0x3;
			eax ^= eax;
			___35f34h_cdecl(eax, edx, ebx);
			L(eax) = B(___1a0ef8h+0xb);
			B(esp) = L(eax);
			L(eax) = B(___1a0ef8h+0xa);
			B(esp+0x1) = L(eax);
			L(eax) = B(___1a0ef8h+9);
			B(esp+0x2) = L(eax);
			L(eax) = B(___1a0ef8h+8);
			B(esp+0x3) = L(eax);
		}
		else {
			B(esp+0) = B(___1a0ef8h+8);
			B(esp+1) = B(___1a0ef8h+9);
			B(esp+2) = B(___1a0ef8h+10);
			B(esp+3) = B(___1a0ef8h+11);
			ebx ^= ebx;
			edx ^= edx;
			D(esp+0x4) = ebx;
			ebx = 0x4;

			while(1){

				if(D(edx+___1de7d0h+0x40) == 1){
				
					eax = D(esp+0x4);
					L(eax) = B(eax+___1a0ef8h+8);
					B(esp) = L(eax);
				}

				if(D(edx+___1de7d0h+0x40) == 2){

					eax = D(esp+0x4);
					L(eax) = B(eax+___1a0ef8h+8);
					B(esp+0x1) = L(eax);
				}

				if(D(edx+___1de7d0h+0x40) == 3){
				
					eax = D(esp+0x4);
					L(eax) = B(eax+___1a0ef8h+8);
					B(esp+0x2) = L(eax);
				}

				if(ebx == D(edx+___1de7d0h+0x40)){
				
					eax = D(esp+0x4);
					L(eax) = B(eax+___1a0ef8h+8);
					B(esp+0x3) = L(eax);
				}

				ecx = D(esp+0x4);
				ecx++;
				edx += 0x54;
				D(esp+0x4) = ecx;
				if((int)ebx <= (int)ecx) break;
			}
		}
	}

	if(ebp == 0){

		___35dd0h();
		ecx = 0x14;
		edx = 0x110;
		esip = ___1a0fe4h;
		ebxp = ___1a112ch__VESA101_ACTIVESCREEN_PTR+0xd362;

		while(1){

			edi = edx;

			while(1){

				L(eax) = B(esip);
				if(L(eax) != 0) B(ebxp) = L(eax);
				ebxp++;
				esip++;
				edi--;
				if(edi == 0) break;
			}

			ebxp += 0x280;
			ebxp -= edx;
			ecx--;
			if(ecx == 0) break;
		}

		{
			const char *dr3_title = "Hard Race Results";
			if(dr3_chaos_active()){
				if(dr3_chaos_arena()) dr3_title = "Arena Results";
				else if(dr3_chaos_mode() == DR3_CHAOS_MODE_DUEL) dr3_title = "Duel Results";
				else if(dr3_chaos_mode() == DR3_CHAOS_MODE_DM) dr3_title = "Destroy 'em All";
			}
			___12e78h_cdecl(___1a1108h, (font_props_t *)___185c0bh, dr3_title, 0xd890);
		}
		eax = 0x4;
		___38184h_cdecl(eax, esp);
	}

	edi ^= edi;
	D(esp+0x4) = edi;

	while(1){

		if(dr3_chaos_active() && ((int)D(esp+0x4) >= NUM_OF_CARS)) break;

		if(ebp == 0){
			
			eax = D(esp+0x4);
			edx ^= edx;
			L(edx) = B(esp+eax);
			eax = 0x6c*edx;

			if(s_6c[eax/0x6c].damage != 0x64){

				if(!dr3_chaos_active() && (edx == D(___1a1ef8h))&&(D(___196ab0h) == 1)) break;

				edx = D(esp+0x4);
				eax ^= eax;
				L(eax) = B(esp+edx);
				eax = 0x13*eax;
				eax += 0x57;
				eax = 0x280*eax;
				ecx = 0x12;
				esip = ___1a0fa8h;
				ebxp = ___1a112ch__VESA101_ACTIVESCREEN_PTR+eax+0xd9;
				edx = 0x2a;

				while(1){

					edi = edx;

					while(1){

						L(eax) = B(esip);
						if(L(eax) != 0) B(ebxp) = L(eax);
						ebxp++;
						esip++;
						edi--;
						if(edi == 0) break;
					}

					ebxp += 0x280;
					ebxp -= edx;
					ecx--;
					if(ecx == 0) break;
				}
			}
		}

		edi = D(esp+0x4);

		if(!dr3_chaos_active()){
			switch(edi){
		case 0:
			edx ^= edx;
			L(edx) = B(esp+edi);
			eax = 0x6c*edx;

			if(s_6c[eax/0x6c].damage != 0x64){
				
				if((edx != D(___1a1ef8h))||(D(___196ab0h) != 1)){
					
					edx = D(esp+0x4);
					eax ^= eax;
					L(eax) = B(esp+edx);
					edx = 0x6c*eax;
					eax = 0x13*eax;
					eax += 0x59;
					eax = 0x280*eax;
					edi = s_6c[edx/0x6c].points;
					edi += 0xa;
					s_6c[edx/0x6c].points = edi;
					ecx = eax+0xe6;
					___12e78h_cdecl(___1a10b8h, (font_props_t *)___185c7ah, "+10", ecx);
				}
			}
			break;
		case 1:
			eax ^= eax;
			L(eax) = B(esp+edi);
			edx = 0x6c*eax;

			if(s_6c[edx/0x6c].damage != 0x64){

				if((eax != D(___1a1ef8h))||(D(___196ab0h) != 1)){

					eax = D(esp+0x4);
					edx ^= edx;
					L(edx) = B(esp+eax);
					eax = 0x6c*edx;
					edx = 0x13*edx;
					edx += 0x59;
					edx = 0x280*edx;
					edi = s_6c[eax/0x6c].points;
					edi += 0x7;
					s_6c[eax/0x6c].points = edi;
					ecx = edx+0xe6;
					___12e78h_cdecl(___1a10b8h, (font_props_t *)___185c7ah, "+7", ecx);
				}
			}
			break;
		case 2:
			eax ^= eax;
			L(eax) = B(esp+edi);
			edx = 0x6c*eax;

			if(s_6c[edx/0x6c].damage != 0x64){
				
				if((eax != D(___1a1ef8h))||(D(___196ab0h) != 1)){
			
					eax = D(esp+0x4);
					edx ^= edx;
					L(edx) = B(esp+eax);
					eax = 0x6c*edx;
					edx = 0x13*edx;
					edx += 0x59;
					edx = 0x280*edx;
					edi = s_6c[eax/0x6c].points;
					edi += 0x4;
					s_6c[eax/0x6c].points = edi;
					ecx = edx+0xe6;
					___12e78h_cdecl(___1a10b8h, (font_props_t *)___185c7ah, "+4", ecx);
				}
			}
			break;
		default:
			break;
		}
		}

		eax = D(esp+0x4);
		eax++;
		D(esp+0x4) = eax;
		if((int)eax >= 3) break;
	}

	if(ebp == 0){

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

		D(esp+0x4) = 0;

		while(1){

			nn = D(esp+4);
			rr = helper_color(___19eb50h[D(esp+0x4)].r, 0x640000);
			gg = helper_color(___19eb50h[D(esp+0x4)].g, 0x640000);
			bb = helper_color(___19eb50h[D(esp+0x4)].b, 0x640000);
			__DISPLAY_SET_PALETTE_COLOR(bb, gg, rr, nn);
			D(esp+0x4)++;
			if((int)D(esp+0x4) >= 0x100) break;
		}

		___12cb8h__VESA101_PRESENTSCREEN();
	}
}
