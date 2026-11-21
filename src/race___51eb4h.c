#include "drally.h"
#include "drally_structs_free.h"
#if defined(DR3_CHAOS)
#include "platform_3ds/dr3_cash.h"
#endif

	extern __BYTE__ ___1f2488h[];
	extern __BYTE__ ___1de7d0h[];
	extern int TRX_VIEWPORT_TL_X;
	extern int CURRENT_VIEWPORT_X;
	extern int TRX_VIEWPORT_TL_Y;
	extern __BYTE__ ___243334h[];

char * itoa_watcom106(int value, char * buffer, int radix);
void ___424c8h_v2(const char *, int x, int y);

/*
 * The kill bonus' floating text (drally.h: DR3_EFFECT_KILL_BONUS): the engine's own font, twice the
 * size.  An item's text is 6x6 pixels per character (___1d7810h, one 0x24 byte block per character
 * from the space on), which is little for an amount that should be readable at a glance in the middle
 * of a race - so every pixel of it is drawn as a DR3_KILL_BONUS_TEXT_SCALE x DR3_KILL_BONUS_TEXT_SCALE
 * block.
 *
 * The placement is dr3_kill_bonus_place()'s: centred on the wreck and kept inside the 320x200 of the
 * screen.  The engine's item text is drawn at x + 0x60 with only x tested against the screen, so an
 * item's popup often sits half off the right edge - the bonus must not, because a payout that is never
 * seen is no feedback at all.
 */
#define DR3_KILL_BONUS_TEXT_SCALE   2
#define DR3_KILL_BONUS_TEXT_W       6
#define DR3_KILL_BONUS_TEXT_H       6
#define DR3_KILL_BONUS_SCREEN_W     DR3_RACE_W
#define DR3_KILL_BONUS_SCREEN_H     DR3_RACE_H
/*
 * The text is placed in the space the *world* is positioned in - that is the space `x` is in (the
 * engine's own screen x of a car: `__4 = XLocation - TRX_VIEWPORT_TL_X + CURRENT_VIEWPORT_X`,
 * race___4f300h.c): centred on the wreck, kept on the screen, and to the right of the in-race HUD
 * (its car list ends at x = 75 in that space - measured on a host frame).
 *
 * The text blitter, however, writes at the raw *buffer* position (___424c8h_cdecl), while the world
 * is drawn through a blit whose left edge is CURRENT_VIEWPORT_X + 0x60 (race___4f300h.c, helper01).
 * The same x is therefore DR3_KILL_BONUS_TEXT_OBJ = 0x60 further right for the text.  The engine's own
 * item popups have exactly that offset (`x + 0x60`, the `case 2:` above); my first version dropped it,
 * and the "$" landed 96 px left of its wreck (measured on a host frame: the text at x = 76..118 while
 * its car was drawn at 174..214).  Do not remove it again.
 */
#define DR3_KILL_BONUS_TEXT_OBJ     0x60
#define DR3_KILL_BONUS_TEXT_HUD     0x50    /* the in-race car list's right edge, in the same space */

/*
 * Places the text and says whether it is on the screen at all: a text whose wreck has left the view is
 * not drawn, exactly like the engine's own item text (the guard above race___51eb4h's switch).  The
 * horizontal clamp is the exception - an amount whose wreck slid behind the in-race HUD stays readable at
 * the HUD's edge, because the wreck itself is covered by the HUD in that moment: "on the wreck" is not a
 * place one can see there.
 */
static int dr3_kill_bonus_place(const char * s, int x, int y, int * out_x, int * out_y){

	const int hud_l = (CURRENT_VIEWPORT_X > 0) ? DR3_KILL_BONUS_TEXT_HUD : 4;
	int len = (int)strlen(s);
	int w   = len*DR3_KILL_BONUS_TEXT_W*DR3_KILL_BONUS_TEXT_SCALE;
	int h   = DR3_KILL_BONUS_TEXT_H*DR3_KILL_BONUS_TEXT_SCALE;
	int px  = x - w/2;                       /* x is the wreck's centre */
	int ty  = y - h - 2;                     /* and the text sits above it */

	if(px < hud_l) px = hud_l;                                              /* never under the HUD ...  */
	if(px + w > DR3_KILL_BONUS_SCREEN_W) px = DR3_KILL_BONUS_SCREEN_W - w;  /* ... and never off screen */
	if(px < hud_l) px = hud_l;

	if((ty < 0)||(ty + h > DR3_KILL_BONUS_SCREEN_H)) return 0;             /* the wreck left the view  */

	*out_x = px + DR3_KILL_BONUS_TEXT_OBJ;   /* the buffer position the text blitter wants */
	*out_y = ty;

	return 1;
}

static void dr3_kill_bonus_text(const char * s, int x, int y){

	extern __BYTE__ * BACKBUFFER;
	extern __BYTE__ ___1d7810h[];

	int i, gx, gy, dx, dy;

	for(i = 0; s[i]; ++i){

		const __BYTE__ * glyph = &___1d7810h[0x24*(s[i]-0x20)];       /* 6x6 pixels, row by row */

		for(gy = 0; gy < DR3_KILL_BONUS_TEXT_H; ++gy)

			for(gx = 0; gx < DR3_KILL_BONUS_TEXT_W; ++gx){

				const __BYTE__ px = glyph[DR3_KILL_BONUS_TEXT_W*gy+gx];

				if(px == 0) continue;

				for(dy = 0; dy < DR3_KILL_BONUS_TEXT_SCALE; ++dy)

					for(dx = 0; dx < DR3_KILL_BONUS_TEXT_SCALE; ++dx)
						BACKBUFFER[0x200*(y+gy*DR3_KILL_BONUS_TEXT_SCALE+dy)
						          + x+(i*DR3_KILL_BONUS_TEXT_W+gx)*DR3_KILL_BONUS_TEXT_SCALE+dx] = px;
			}
	}
}


void race___51eb4h(void){

	int 			n, x, y;
	char 			buffer[8];
	struct_54_t * 	s_54;


	s_54 = (struct_54_t *)___1de7d0h;

	n = -1;
	while(++n < 0x14){          /* all twenty effect slots: a chaos race can need every one of them */

		const int type = (int)D(0x120*n+___1f2488h+0x18);

		if((int)D(0x120*n+___1f2488h+0x14) <= 0) continue;                  /* no text in this slot */

		/*
		 * The item popups carry amounts that come out of the race's own cash (s_54[0].__50, set with
		 * the track setup in ___33010h.c), so the original code shows them only in a race that pays
		 * at all.  The chaos kill bonus (race___4dcach.c) brings its own amount in the slot and is
		 * paid in every race, so it is drawn whenever its slot lives.
		 */
		if((type != DR3_EFFECT_KILL_BONUS)&&(type != 5)&&(type != 8)&&(type != 10)&&((int)s_54[0].__50 <= 0)) continue;

		x = CURRENT_VIEWPORT_X+D(0x120*n+___1f2488h)-TRX_VIEWPORT_TL_X;
		y = D(0x120*n+___1f2488h+4)+((int)D(0x120*n+___1f2488h+0x14)>>3)-TRX_VIEWPORT_TL_Y-4;

		if((x >= -18 + CURRENT_VIEWPORT_X)&&(x < DR3_RACE_W)&&(y >= 6)&&(y < DR3_RACE_H)){

			switch(type-1){
			case 2:
				___424c8h_v2(itoa_watcom106(s_54[0].__50, strcpy(buffer, "$")+1, 0xa)-1, x+0x60, y-6);
				break;
			case DR3_EFFECT_KILL_BONUS-1:
				{
					/*
					 * A wrecked opponent pays in the "Chaos" difficulty (race___4dcach.c).  The amount
					 * rides in the slot's own field (+0x1c), exactly like a picked up item's does, so it
					 * drifts with the wreck - and the life is the pickup's 0x8c too (race___526ach.c),
					 * because the writer sets that one as well.  Drawn twice the size of an item's text
					 * and centred on the wreck (dr3_kill_bonus_place/text above), which is what makes it
					 * readable in the middle of a pack - and always inside the screen.
					 *
					 * The log prints the *screen* position first (the space `x`, the wreck's own x, is in)
					 * and the buffer position the blitter was given second.
					 */
					const char *  money = itoa_watcom106(D(0x120*n+___1f2488h+0x1c), strcpy(buffer, "$")+1, 0xa)-1;
					int           bx, by;

					if(dr3_kill_bonus_place(money, x, y, &bx, &by)){

						dr3_kill_bonus_text(money, bx, by);

						dr3_log("[dr3] chaos: kill bonus popup: %s at screen %d,%d (buffer %d,%d, wreck %d, slot %d, life %d, scale %d)",
						        money, bx - DR3_KILL_BONUS_TEXT_OBJ, by, bx, by, x, n,
						        (int)D(0x120*n+___1f2488h+0x14), DR3_KILL_BONUS_TEXT_SCALE);
					}
				}
				break;
			case 6:
				___424c8h_v2(itoa_watcom106(0xa*s_54[0].__50, strcpy(buffer, "$")+1, 0xa)-1, x+0x60, y-6);
				break;
			case 4:
			case 7:
				___424c8h_v2(strcat(itoa_watcom106(D(0x120*n+___1f2488h+0x1c), buffer, 0xa), "%"), x+0x60, y-6);
				break;
			case 9:
				___424c8h_v2("+3 MINES", x+0x60, y-6);
				break;
			default:
				break;
			}
		}

		D(0x120*n+___1f2488h+0x14) -= D(___243334h);
	}

#if defined(DR3_CHAOS)
	dr3_cash_draw_popups();
#endif
}
