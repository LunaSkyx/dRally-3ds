#include "drally.h"

	extern __BYTE__ ___243334h[];
	extern __BYTE__ ___243328h[];
	extern __BYTE__ ___243324h[];
	extern __POINTER__ BACKBUFFER;
	extern __POINTER__ GEN_FLA_BPK;
	extern __BYTE__ ___24332ch[];

// FINISH LINE FLAG
void race___4207ch(void){

	__DWORD__ 	eax, ebx, ecx, edx, edi, esi, ebp;
	int 	i, j;
	

	if((int)(D(___243328h)-D(___243334h)) <= 0){

		D(___243328h) = 0;
	}
	else {
	
		D(___243328h) -= D(___243334h);
	}

	edx = (int)(D(___243324h)+0x8000)>>0x10;

	{
		const int x0 = (0x60 + DR3_RACE_W - 0x50) + (int)D(___243328h);
		int cw = (0x60 + DR3_RACE_W) - x0;
		if (cw > 0x50) cw = 0x50;
		if (cw > 0) {
			__BYTE__* s = GEN_FLA_BPK + 0x12c0 * edx;
			__BYTE__* d = BACKBUFFER + x0;
			j = 0x3c;
			do {
				memcpy(d, s, (size_t)cw);
				s += 0x50;
				d += 0x200;
			} while (--j);
		}
	}

	D(___243324h) += D(___24332ch);
	eax = (int)(D(___243324h)+0x8000)>>0x10;

	if((int)eax > 0x3f) D(___243324h) = 0;
}
