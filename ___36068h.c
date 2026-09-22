#include "drally.h"
#include "drally_fonts.h"
#include "drally_structs_fixed.h"

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

	if(D(___1a1028h)){

		___35f34h_cdecl(0, 3, 0);
		B(esp) = B(___1a0ef8h+3);
		B(esp+1) = B(___1a0ef8h+2);
		B(esp+2) = B(___1a0ef8h+1);
		B(esp+3) = B(___1a0ef8h);
	}
	else {

#if defined(DR3_CHAOS)
		/*
		 * The four rows of this list are drawn from the racer index found here: it is used both as an
		 * index into s_6c[] and as the row pitch of the pictures (___38184h_cdecl, and 0x13*index
		 * below), so it can only hold one of the four racers the list has room for.  The original loop
		 * fills the rows from the first four cars of the race and only when one of them finished
		 * 1st..4th - in a four car race that is the same thing, in the chaos test build (twelve cars)
		 * the top four are usually somewhere else, which left rows unset and sent a garbage index into
		 * s_6c[...] and the screen buffer.
		 *
		 * So the rows start as the grid's own seats (the racers this screen is written for) and only a
		 * finisher the list can draw overwrites one of them.
		 */
		for(i = 0; i < 4; ++i){

			const int seat = B(i+___1a0ef8h);

			B(esp+i) = (((seat >= 0) && (seat < 4)) ? seat : i);
		}

		n = -1;
		while(++n < 4){

			const int pos = D(0x54*n+D(___1a1028h)+___1de7d0h+0x40);
			const int who = B(n+___1a0ef8h);

			if((pos >= 1) && (pos <= 4) && (who >= 0) && (who < 4)) B(esp+pos-1) = who;
		}
#else
		n = -1;
		while(++n < 4){

			if(D(0x54*n+D(___1a1028h)+___1de7d0h+0x40) == 1) B(esp) = B(n+___1a0ef8h);
			if(D(0x54*n+D(___1a1028h)+___1de7d0h+0x40) == 2) B(esp+1) = B(n+___1a0ef8h);
			if(D(0x54*n+D(___1a1028h)+___1de7d0h+0x40) == 3) B(esp+2) = B(n+___1a0ef8h);
			if(D(0x54*n+D(___1a1028h)+___1de7d0h+0x40) == 4) B(esp+3) = B(n+___1a0ef8h);
		}
#endif
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

		___12e78h_cdecl(___1a1108h, (font_props_t *)___185c0bh, "Easy Race Results", 0xd890);
		___38184h_cdecl(0x4, esp);
	}

	n = -1;
	while(++n < 3){

		if((A1 == 0)&&(s_6c[B(esp+n)].damage != 0x64)){

			if((B(esp+n) == D(___1a1ef8h))&&(D(___196ab0h) == 1)) break;

			j = -1;
			while(++j < 0x12){

				i = -1;
				while(++i < 0x2a){

					if((px = B(___1a0fb0h+0x2a*j+i))) write_b(___1a112ch__VESA101_ACTIVESCREEN_PTR+0x280*(0x13*B(esp+n)+0x57)+0xd9+0x280*j+i, px);
				}
			}
		}

		if((s_6c[B(esp+n)].damage != 0x64)&&((B(esp+n) != D(___1a1ef8h))||(D(___196ab0h) != 1))){

			s_6c[B(esp+n)].points += EasyPoints[n];
			___12e78h_cdecl(___1a10b8h, (font_props_t *)___185c7ah, fmtPoints(EasyPoints[n]), 0x280*(0x13*B(esp+n)+0x59)+0xe6);
		}

	}

	if(A1 == 0){
	
		___12e78h_cdecl(___1a1108h, (font_props_t *)___185c0bh, "Press any key to continue...", 0x46b6e);
		___12cb8h__VESA101_PRESENTSCREEN();
	}
}
