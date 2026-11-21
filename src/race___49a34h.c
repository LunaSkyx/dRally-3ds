#include "drally.h"
#include "platform_3ds/dr3_chaos.h"
#include "platform_3ds/dr3_dm.h"    /* dr3_dm_reset() - the race clock the sudden death counts from */

	extern __BYTE__ ___243d14h[];
	extern int TRX_VIEWPORT_TL_X;
	extern int TRX_VIEWPORT_TL_Y;
	extern int Y_Inc;
	extern int X_Inc;
	extern int Y_IncOld;
	extern int X_IncOld;
	extern int Y___243c90h;
	extern int X___243c8ch;
	extern int ___243cbch;
	extern __BYTE__ MY_CAR_IDX[];
	extern __BYTE__ ___1de580h[];
	extern int CURRENT_VIEWPORT_W;
	extern int CURRENT_VIEWPORT_CENTER_X;
	extern int CURRENT_VIEWPORT_H;
	extern int CURRENT_VIEWPORT_CENTER_Y;
	extern int CURRENT_VIEWPORT_X;
	extern __BYTE__ ___243cf0h[];
	extern __BYTE__ ___1df520h[];
	extern int ___243178h[DR3_MAX_CARS];
	extern __BYTE__ ___243ce4h[];
	extern __BYTE__ ___243d08h[];
	extern __BYTE__ ___196df4h[];
	extern __BYTE__ ___196dbch[];
	extern __BYTE__ ___196dc0h[];
	extern __BYTE__ ___196dc4h[];
	extern __BYTE__ ___196db8h[];
	extern __BYTE__ ___2438c8h[];
	extern __BYTE__ ___196dd4h[];
	extern __BYTE__ ___196dd8h[];
	extern __BYTE__ ___196de0h[];
	extern __BYTE__ ___196de8h[];
	extern __BYTE__ ___196dech[];
	extern __BYTE__ ___196df0h[];
	extern __BYTE__ ___196de4h[];
	extern __BYTE__ ___243ce0h[];
	extern __BYTE__ ___243cdch[];
	extern __BYTE__ ___243ca0h[];
	extern __BYTE__ ___243cb8h[];
	extern __BYTE__ ___243cd4h[];
	extern __BYTE__ ___196df8h[];

void race___49a34h(void){

	__DWORD__ 	n;

	D(___243d14h) = 0;
	TRX_VIEWPORT_TL_X = 0;
	TRX_VIEWPORT_TL_Y = 0;
	Y_Inc = 0;
	X_Inc = 0;
	Y_IncOld = 0;
	X_IncOld = 0;
	Y___243c90h = 0;
	X___243c8ch = 0;
	___243cbch = 0;
	F32(___243cf0h) = 3.75/(double)F32(___1de580h+0x94*D(MY_CAR_IDX)+0x14);
#if DR3_NATIVE_WIDESCREEN
	CURRENT_VIEWPORT_W = DR3_RACE_W;
	CURRENT_VIEWPORT_H = DR3_RACE_H;
	CURRENT_VIEWPORT_CENTER_X = DR3_RACE_W / 2;
	CURRENT_VIEWPORT_CENTER_Y = DR3_RACE_H / 2;
	CURRENT_VIEWPORT_X = 0;
#elif defined(__3DS__) && defined(DR3_USE_GFX)
	CURRENT_VIEWPORT_W = 320;
	CURRENT_VIEWPORT_H = 200;
	CURRENT_VIEWPORT_CENTER_X = 160;
	CURRENT_VIEWPORT_CENTER_Y = 100;
	CURRENT_VIEWPORT_X = 0;
#else
	CURRENT_VIEWPORT_W = 256;
	CURRENT_VIEWPORT_H = 200;
	CURRENT_VIEWPORT_CENTER_X = 128;
	CURRENT_VIEWPORT_CENTER_Y = 100;
	CURRENT_VIEWPORT_X = 64;
#endif
	resetCounter(6);

	n = -1;
	while(++n < DR3_MAX_MINES){

		D(___1df520h+0x10*n+8) = -1;
		D(___1df520h+0x10*n+0xc) = 0;
	}

	D(___243ce4h) = 0;
	/* the list of cars that already finished (race___56170h.c); four in the original, more in the
	   chaos test build - the entries behind the counter have to be -1 there as well */
	n = -1;
	while(++n < DR3_MAX_CARS) ___243178h[n] = -1;
	D(___196df4h) = 0;
	D(___196dbch) = 0;
	D(___196dc0h) = 0;
	D(___196dc4h) = 0;
	D(___196db8h) = 0;
	D(___2438c8h) = 0;
	D(___196dd4h) = 0;
	D(___196dd8h) = 0;
	D(___196de0h) = 0;
	D(___196de8h) = 0;
	D(___196dech) = 0;
	D(___196df0h) = 0;
	D(___196de4h) = 0;
	D(___243ce0h) = 0;
	resetCounter(1);
	D(___243cdch) = 0;
	D(___243ca0h) = 0;
	D(___243cb8h) = 0;
	D(___243cd4h) = 0;
	D(___243d08h) = 4;
	D(___196df8h) = 0x28000;
	resetCounter(5);
#if defined(DR3_CHAOS)
	/*
	 * A race starts here: the sudden death of a deathmatch (platform_3ds/dr3_dm.h) counts from *this*
	 * moment, not from the race setup - the roster and the sign up screen in between are not race time.
	 * The clock it reads is the one started just above (getCounter(1), 70 ticks a second).
	 */
	dr3_dm_reset();
#endif
}
