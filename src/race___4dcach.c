#include "drally.h"
#include "drmath.h"
#include "drally_structs_free.h"
#include "platform_3ds/dr3_chaos.h"   /* DR3_MAX_CARS - the kill bonus' blame arrays */
#include "platform_3ds/dr3_boss.h"
#include "platform_3ds/dr3_anniv_prog.h"
#include "platform_3ds/dr3_results.h"

	extern __BYTE__ NUM_OF_CARS[];
	extern __BYTE__ ___1e6ed0h[];
	extern __POINTER__ ___243d74h;
	extern int TRX_WIDTH;
	extern int TRX_HEIGHT;
	extern __POINTER__ TRX_MAS;
	extern __BYTE__ ___1de580h[];
	extern __BYTE__ ___2438cch[];
	extern __BYTE__ ___1a01e0h[];        /* the racer table - the money lives there */
	extern __BYTE__ ___1f2488h[];        /* the effect slots the floating text uses */
	extern __BYTE__ ___1de7d0h[];        /* s_54: the cars (names, positions, model) */
	/*
	 * The player's seat, and an ARRAY (bss.c: `__BYTE__ ___1a1ef8h[4]`) in every other file of the
	 * tree: declared as a *pointer*, `D(___1a1ef8h)` does not read the seat but dereferences the seat
	 * value as an address - the very first kill bonus of a host run died in "invalid read of 0x13" on
	 * exactly that (doc/host_harness.md).  The other tables the payout needs are the racer list
	 * itself (___1a01e0h, the money), the effect slots (___1f2488h, the popup's text), s_54
	 * (___1de7d0h, the names and the car model) and s_35e (___1e6ed0h, the cars of the race).
	 */
	extern __BYTE__ ___1a1ef8h[];
	extern int      MY_CAR_IDX;          /* the player's car - as an int *value* (bss.c) */


static __DWORD__ helper00(int i, float f){ 
	
	return 1+!((((double)i)*((double)f)) > 0.0);
}

static double helper01(__DWORD__ val){

	__DWORD__	eax, edx, cf;
	__BYTE__ 	esp[4];

	eax = val;
	edx = eax;
	edx = (int)edx>>0x1f;
	cf = !!(edx&0x40000000);
	edx <<= 0x2;
	eax -= edx+cf;
	eax = (int)eax>>0x2;

	F32(esp) = (float)(double)(int)eax;
	if((int)D(esp) > 0x40400000) D(esp) = 0x40400000;		// 3.0

	return (double)F32(esp);
}

/*
 * "Chaos" difficulty: wrecking an opponent pays (drally.h: dr3_chaos_active).
 *
 * The payout follows the *player's* damage wherever it is dealt: the collision damage at
 * the end of this file, the machine gun (race___4ff50h.c) - and every other place that can take a
 * car's last HP (the boost and a spectator, race___4c434h.c / race___52ac4h.c) asks here too, so no
 * way of dying is left out.  A wreck the player had no part in pays nothing, which is what the blame
 * window below is for: a car the player hit or bumped a moment ago is still "his" wreck.
 *
 * The amount follows the *victim's* car class (the picture number at s_54 +0x30, the field the boss
 * check uses too) and it pops up over the wreck: the engine's own floating text, effect type
 * DR3_EFFECT_KILL_BONUS (race___51eb4h.c) with the amount in the slot's field, drawn larger than an
 * item's text and centred on the wreck.  Which slot it uses is in the payout below.
 */
/*
 * How long (in race frames, about two seconds) a hit or a bump stays the reason a car died: long
 * enough for a car the player rammed to die of it - it may take it a moment to reach whatever
 * finishes it - and short enough that a wreck of its own much later is no longer his.
 */
#define DR3_KILLBONUS_BLAME 120

/*
 * The blame clock.  race___4dcach() runs once per frame (race_main.c), so counting frames here is
 * what the window needs - and the clock starts far above every "never touched" stamp (the stamps
 * start at zero, the clock does not), so a car the player never came near is never credited to him.
 */
static int dr3_chaos_clock = 0x40000000;
static int dr3_chaos_blame[DR3_MAX_CARS];    /* frame of the player's last hit/bump, per car */

/*
 * The money the kills of this race paid (drally.h: dr3_chaos_kill_money).  Every payout adds its amount
 * here as well as to racer.money/racer.income, so the report after the race can show the kills next to
 * the rank money (___33010h.c reads it and clears it with the next race starting).
 */
static int dr3_chaos_kill_sum = 0;

int dr3_chaos_kill_money(void){ return dr3_chaos_kill_sum; }

void dr3_chaos_kill_money_reset(void){ dr3_chaos_kill_sum = 0; }

void dr3_chaos_frame(void){ ++dr3_chaos_clock; }

/* the player's own damage took this car's HP (the bumper here, the gun in race___4ff50h.c) */
void dr3_chaos_player_damage(int victim){

	if((victim >= 0)&&(victim < DR3_MAX_CARS)) dr3_chaos_blame[victim] = dr3_chaos_clock;
}

/*
 * The player's car and another one touched: the sprites overlap, whether or not that took HP, because
 * a car the player pushed off the road or into the pack is his wreck too.  One of the two is the
 * player's car, so this is only ever called for a pair he is in (race___4dcach.c).
 */
void dr3_chaos_player_contact(int a, int b){

	/* MY_CAR_IDX is the player's car as an int *value* in this file - no D() around it (see above) */
	if(MY_CAR_IDX == a) dr3_chaos_player_damage(b);
	else if(MY_CAR_IDX == b) dr3_chaos_player_damage(a);
}

/*
 * One car just went from alive to wrecked.  Every place that takes HP calls this right after its own
 * write, with the HP it saw before it (`hp_before`) and a name for itself: the payout happens when the
 * player's car was the last thing that happened to the victim.
 */
void dr3_chaos_wreck(int victim, int hp_before, const char * cause){

	if(!dr3_chaos_active()) return;

	/* the player's own wreck pays nothing, and neither does a car that is not in this race */
	if((victim < 0)||(victim >= DR3_MAX_CARS)||(victim == MY_CAR_IDX)) return;

	/* wrecked in an earlier frame (or already dead then), or still alive: not this call's business */
	if((hp_before <= 0)||((int)D(___1de580h+0x94*victim+0x18) > 0)) return;

	if((dr3_chaos_clock - dr3_chaos_blame[victim]) >= DR3_KILLBONUS_BLAME){

		dr3_log("[dr3] chaos: %s (car %d) wrecked by %s - no kill bonus (the player had no part in it)",
		        (const char *)(___1de7d0h + 0x54*victim), victim, cause);
		return;
	}

	dr3_chaos_kill_bonus(victim, cause);
}


void dr3_chaos_kill_bonus(int victim, const char * cause){

	/* one per car model, in the order of e_car (race___3f970h.c); the boss is the last one */
	static const int reward[7] = { 50, 75, 150, 200, 300, 500, 1000 };

	/* the tables this needs are the ones declared at the top of the file (s_35e: ___1e6ed0h) */

	const struct_35e_t * s_35e = (const struct_35e_t *)___1e6ed0h;
	int                  model  = (int)D(___1de7d0h + 0x54*victim + 0x30);
	int                  seat   = (int)D(___1a1ef8h);
	int                  amount, money_before, income_before, at;
	int                  i;

	int                  player_model = (int)D(___1de7d0h + 0x54*MY_CAR_IDX + 0x30);
	int                  underdog = 0;

	if (model < 0)  model = 0;
	if (model > 6)  model = 6;
	if (dr3_chaos_active()) {
		underdog = dr3_anniv_underdog_bonus_ex(player_model, model, dr3_extreme_active());
	}
	amount = reward[model] + underdog;

	/*
	 * The money of the player's seat in the racer table (drally_structs_fixed.h: racer_t, 0x6c bytes
	 * per seat, money at +0x30 - the same field the shop and the results screens use).
	 */
	money_before = (int)D(___1a01e0h + 0x6c*seat + 0x30);
	D(___1a01e0h + 0x6c*seat + 0x30) = money_before + amount;

	/*
	 * The same money is income: racer_t +0x58 (drally_structs_fixed.h) is the field the report's
	 * "Total income" reads (___36adch.c) and the counter dr3_chaos_kill_money() is what the report
	 * shows as the race's own bonus income next to the rank money.  The shop keeps working off
	 * racer.money, so the two fields are written together and the payout is never counted twice.
	 */
	income_before = (int)D(___1a01e0h + 0x6c*seat + 0x58);
	D(___1a01e0h + 0x6c*seat + 0x58) = income_before + amount;
	dr3_chaos_kill_sum += amount;

	/*
	 * The floating "$" over the wreck goes into one of the four slots the items never use (0x10..0x13):
	 * the track file hands positions to the sixteen item slots (race___49bd4h.c), the respawns and the
	 * pickups live in those (race___5209ch.c, race___526ach.c) and race___496b0h.c resets the whole
	 * pool when a race starts.  A text written into an item slot would move that item's respawn to the
	 * wreck - the item is put back at the position the slot holds.
	 */
	at = -1;
	for (i = 0x10; i < 0x14; ++i)
		if ((int)D(___1f2488h + 0x120*i + 0x14) <= 0) { at = i; break; }

	/* all four busy (four wrecks within the life of one text): the oldest one gives way */
	if (at < 0) {

		at = 0x10;

		for (i = 0x11; i < 0x14; ++i)
			if ((int)D(___1f2488h + 0x120*i + 0x14) < (int)D(___1f2488h + 0x120*at + 0x14)) at = i;
	}

	{
		__BYTE__ * slot = ___1f2488h + 0x120 * at;

		D(slot + 0x00) = (int)s_35e[victim].XLocation;
		D(slot + 0x04) = (int)s_35e[victim].YLocation;
		D(slot + 0x08) = 0;              /* the item type: a text slot owns no item            */
		D(slot + 0x14) = 0x8c;           /* the life a picked up item's text gets (race___526ach.c) */
		D(slot + 0x18) = DR3_EFFECT_KILL_BONUS;
		D(slot + 0x1C) = amount;
	}

	dr3_anniv_radio_on_kill((const char *)(___1de7d0h + 0x54*victim), model, player_model, amount, underdog);

	dr3_log("[dr3] chaos: kill bonus: %s (car %d, model %d) wrecked by %s pays $%d (underdog +$%d) into seat %d: "
	        "money %d -> %d, income %d -> %d, race kills $%d, text slot %d",
	        (const char *)(___1de7d0h + 0x54*victim), victim, model, cause, amount, underdog, seat,
	        money_before, money_before + amount, income_before, income_before + amount,
	        dr3_chaos_kill_sum, at);
}

// RACE COLLISIONS
void race___4dcach(void){

	__DWORD__ 		eax, edx;
	__BYTE__ 		esp[0xd4];
	int 			bl1, bl2;
	int 			n, k, i, j, l;
	struct_35e_t * 	s_35e;


	s_35e = (struct_35e_t *)___1e6ed0h;

	/* the chaos battle bonus' clock: this function runs once per race frame (race_main.c) */
	dr3_chaos_frame();

	n = -1;
	while(++n < (int)D(NUM_OF_CARS)){

		s_35e[n].__fc = (float)(double)s_35e[n].__fc/1.15;
		s_35e[n].__100 = (float)(double)s_35e[n].__100/1.15;
		s_35e[n].__104 = (float)(double)s_35e[n].__104/1.2;
	}
	
	n = -1;
	while(++n < (int)(D(NUM_OF_CARS)-1)){

		k = n;
		while(++k < (int)D(NUM_OF_CARS)){

			D(esp+0x68) = (int)((double)s_35e[k].XLocation-(double)s_35e[n].XLocation);
			D(esp+0x64) = (int)((double)s_35e[k].YLocation-(double)s_35e[n].YLocation);

			if((dRMath_abs_i(D(esp+0x68)) < 39)&&(dRMath_abs_i(D(esp+0x64)) < 39)){
			
				D(esp+0x94) = 0;

				j = 0;
				while(++j < (int)(0x28-dRMath_abs_i(D(esp+0x64)))){

					i = -1;
					while(++i < (int)(0x28-dRMath_abs_i(D(esp+0x68)))){

						if(B(___243d74h+0x28*(j+dRMath_abspos_i(D(esp+0x64)))+s_35e[n].ImgOffset+i+dRMath_abspos_i(D(esp+0x68))) > 3){

							if(B(___243d74h+0x28*(j+dRMath_absneg_i(D(esp+0x64)))+s_35e[k].ImgOffset+i+dRMath_absneg_i(D(esp+0x68))) > 3){

								D(esp+0x94) = 1;
								s_35e[n].__172 = i+dRMath_abspos_i(D(esp+0x68))-0x14;
								s_35e[n].__176 = j+dRMath_abspos_i(D(esp+0x64))-0x14;
								s_35e[k].__172 = i+dRMath_absneg_i(D(esp+0x68))-0x14;
								s_35e[k].__176 = j+dRMath_absneg_i(D(esp+0x64))-0x14;
							}
						}
					}
				}

				/* Giant Boss 1.5x outer bumper contact: ensures the extra visual hull collides solidly */
				if((D(esp+0x94) == 0) && (dr3_boss_is_giant_car(n) || dr3_boss_is_giant_car(k))){
					const int cdx = (int)D(esp+0x68), cdy = (int)D(esp+0x64);
					if((cdx*cdx + cdy*cdy) < (31*31)){
						D(esp+0x94) = 1;
						s_35e[n].__172 = cdx / 2;
						s_35e[n].__176 = cdy / 2;
						s_35e[k].__172 = -cdx / 2;
						s_35e[k].__176 = -cdy / 2;
					}
				}

				if(D(esp+0x94) == 1){

					/*
					 * The sprites overlap: a car the player bumps is his business from here on, whether or
					 * not the bump takes HP (dr3_chaos_wreck pays a wreck it has a part in).
					 */
					if(dr3_chaos_active()) dr3_chaos_player_contact(n, k);

					{
						const int wall_n = (int)s_35e[n].__14;
						const int wall_dx_n = (int)s_35e[n].__172;
						const int wall_dy_n = (int)s_35e[n].__176;
						const int wall_k = (int)s_35e[k].__14;
						const int wall_dx_k = (int)s_35e[k].__172;
						const int wall_dy_k = (int)s_35e[k].__176;
						__DWORD__ h_n18, h_n1c, h_k18, h_k1c;

						D(esp+0x28) = (int)(((double)(int)(0-s_35e[n].__172)*((double)s_35e[k].YLocation-(double)s_35e[k].__152))+(((double)s_35e[k].XLocation-(double)s_35e[k].__14e)*(double)(int)s_35e[n].__176));

						s_35e[n].__100 = (float)(0.7*(double)((float)(((double)s_35e[k].YLocation-(double)s_35e[k].__152)-((double)s_35e[n].YLocation-(double)s_35e[n].__152)))+(double)s_35e[n].__100);

						s_35e[n].__fc = (float)((0.7*(((double)s_35e[k].XLocation-(double)s_35e[k].__14e)-((double)s_35e[n].XLocation-(double)s_35e[n].__14e)))+(double)s_35e[n].__fc);

						D(esp+0x2c) = (int)((((double)s_35e[n].YLocation-(double)s_35e[n].__152)*(double)(int)(0-s_35e[k].__172))+(((double)s_35e[n].XLocation-(double)s_35e[n].__14e)*(double)(int)s_35e[k].__176));

						h_n18 = helper00(s_35e[n].__172, s_35e[n].__15a);
						h_n1c = helper00(s_35e[n].__176, s_35e[n].__15e);
						if (s_35e[n].__18 != 1) s_35e[n].__18 = h_n18;
						if (s_35e[n].__1c != 1) s_35e[n].__1c = h_n1c;
						
						s_35e[k].__fc = (float)((0.7*(((double)s_35e[n].XLocation-(double)s_35e[n].__14e)-((double)s_35e[k].XLocation-(double)s_35e[k].__14e)))+(double)s_35e[k].__fc);

						s_35e[k].__100 = (float)((0.7*(((double)s_35e[n].YLocation-(double)s_35e[n].__152)-((double)s_35e[k].YLocation-(double)s_35e[k].__152)))+(double)s_35e[k].__100);
						
						h_k18 = helper00(s_35e[k].__172, s_35e[k].__15a);
						h_k1c = helper00(s_35e[k].__176, s_35e[k].__15e);
						if (s_35e[k].__18 != 1) s_35e[k].__18 = h_k18;
						if (s_35e[k].__1c != 1) s_35e[k].__1c = h_k1c;

						bl1 = !((h_n18 != 1)||(h_n18 != h_k18));
						bl2 = !((h_n1c != 1)||(h_n1c != h_k1c));

						if(bl1||bl2){

							if((double)s_35e[n].Direction <= (double)s_35e[k].Direction){

								s_35e[n].__104 = (float)((double)s_35e[n].__104+2.0);
								s_35e[k].__104 = (float)((double)s_35e[k].__104-2.0);
							}
							else {

								F32(esp+0x54) = (float)((double)s_35e[n].Direction-(double)s_35e[k].Direction);
								F32(esp+0x3c) = (0.0 <= (double)F32(esp+0x54))?F32(esp+0x54):(float)(-1.0*(double)F32(esp+0x54));

								if((int)D(esp+0x3c) >= 0x42c80000){	// 100.0

									s_35e[n].__104 = (float)((double)s_35e[n].__104+2.0);
									s_35e[k].__104 = (float)((double)s_35e[k].__104-2.0);
								}
								else {

									s_35e[n].__104 = (float)((double)s_35e[n].__104-2.0);
									s_35e[k].__104 = (float)((double)s_35e[k].__104+2.0);
								}
							}
						}

						/*
						 * Wall-aware car-to-car separation & rigid wall backstop reflection:
						 * Even if (bl1 || bl2) is 0 (e.g. one car is pinned stationary against a wall while
						 * 8 cars ram it), dr3_wall_separate_pair ensures the wall-pinned car cannot be
						 * shoved into solid wall ((TRX_MAS & 0xf) < 4) and rebounds the outer car away.
						 */
						dr3_wall_separate_pair(
							(const unsigned char *)TRX_MAS,
							TRX_WIDTH, TRX_HEIGHT,
							&s_35e[n].XLocation, &s_35e[n].YLocation,
							&s_35e[n].__fc, &s_35e[n].__100,
							wall_n, wall_dx_n, wall_dy_n,
							&s_35e[k].XLocation, &s_35e[k].YLocation,
							&s_35e[k].__fc, &s_35e[k].__100,
							wall_k, wall_dx_k, wall_dy_k,
							(bl1 || bl2));
					}

					F32(esp+0x30) = (float)dRMath_abs((double)s_35e[n].Direction-(double)s_35e[k].Direction);

					if(((int)D(esp+0x30) > 0x42340000 /*45.0*/)&&((int)D(esp+0x30) < 0x439d8000 /*315.0*/)){
						
						if(((int)D(esp+0x30) < 0x43070000 /*135.0*/)||((int)D(esp+0x30) > 0x43610000 /*225.0*/)){

							s_35e[n].__104 = (float)((double)s_35e[n].__104+helper01(D(esp+0x28)));
							s_35e[k].__104 = (float)((double)s_35e[k].__104+helper01(D(esp+0x2c)));
						}
					}

					D(esp+0x4c) = 0;

					if((D(___1de580h+0x94*n+0x24) != 0)&&(s_35e[k].Finished == 0)){

						eax = (int)(double)s_35e[k].XLocation-(int)((double)s_35e[n].XLocation+(double)s_35e[n].__cc);
						edx = (int)(double)s_35e[k].YLocation-(int)((double)s_35e[n].YLocation+(double)s_35e[n].__d0);	

						if((dRMath_abs_i(eax) < 0x14)&&(dRMath_abs_i(edx) < 0x14)){

							if(B(___243d74h+s_35e[k].ImgOffset+(int)eax+0x28*(edx+0x14)+0x14) > 3) D(esp+0x4c) = 1;
						}

						eax = (int)(double)s_35e[k].XLocation-(int)((double)s_35e[n].XLocation+(double)s_35e[n].__c4);
						edx = (int)(double)s_35e[k].YLocation-(int)((double)s_35e[n].YLocation+(double)s_35e[n].__c8);

						if((dRMath_abs_i(eax) < 0x14)&&(dRMath_abs_i(edx) < 0x14)){

							if(B(___243d74h+s_35e[k].ImgOffset+(int)eax+(int)(0x28*(edx+0x14))+0x14) > 3) D(esp+0x4c) = 1;
						}

						if(D(esp+0x4c) == 1){

							const int dr3_hp_before = (int)D(___1de580h+0x94*k+0x18);

							D(___2438cch) = (int)((double)(int)(0xc00-3*D(___1de580h+0x94*k+0x1c))*dRMath_sum2squares((double)s_35e[k].__fc, (double)s_35e[k].__100));
							if((int)D(___2438cch) > 0x2710) D(___2438cch) = 0x2710;
							D(___1de580h+0x94*k+0x18) -= D(___2438cch);
							if((int)D(___1de580h+0x94*k+0x18) < 0) D(___1de580h+0x94*k+0x18) = 0;

							/*
							 * The collision code visits every pair once, always with n < k, and it is
							 * n's own front wheel points (__c4/__cc - the fields race___4c434h.c
							 * fills) that have to poke into k's picture for the damage to happen: n
							 * is the one who ran into k, and only k can lose HP here.  The chaos
							 * battle bonus covers both orders - n == the player pays for the ram,
							 * and a car that dies of anything else right after the player touched
							 * it pays too, because dr3_chaos_player_contact() above marked the bump.
							 */
							if(n == MY_CAR_IDX) dr3_chaos_player_damage(k);

							dr3_chaos_wreck(k, dr3_hp_before,
							                (n == MY_CAR_IDX) ? "the player's bumper" : "an opponent's bumper");
						}
					}
				}
			}
		}
	}
}
