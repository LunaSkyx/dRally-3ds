#include "drally.h"

	extern __BYTE__ * BACKBUFFER;
	extern __BYTE__ ___1d7810h[];

static void ___424c8h_cdecl(int offset, const char * A2){

	int 	n, x, y;
	__BYTE__ 	px;
	const int row0 = offset / 0x200;
	const int col0 = offset % 0x200;

	n = -1;
	while(A2[++n]){

		__BYTE__* s = &___1d7810h[0x24 * (A2[n] - 0x20)];
		const int cx = col0 + 6 * n;
		__BYTE__* d = &BACKBUFFER[offset + 6 * n];
		y = 6;
		do {
			const int ry = row0 + (6 - y);
			x = 6;
			do {
				const int rx = cx + (6 - x);
				if ((px = *s++) != 0) {
					if ((ry >= 0) && (ry < DR3_RACE_H) && (rx >= 0x60) && (rx < (0x60 + DR3_RACE_W))) *d = px;
				}
				d++;
			} while (--x);
			d += (0x200 - 6);
		} while (--y);
	}
}

void ___424c8h_v2(const char * s, int x, int y){ ___424c8h_cdecl(0x200*y+x, s); }
