#include "drally.h"
#include "platform_3ds/dr3_chaos.h"
#if defined(DR3_CHAOS)
#include "platform_3ds/dr3_results.h"
#endif
#include "sfx.h"
#include "drally_structs_free.h"

	extern int NUM_OF_CARS;
	extern __BYTE__ ___1e6ed0h[];
	extern __POINTER__ TRX_VAI;
	extern int TRX_WIDTH_QTR;
	extern __BYTE__ ___243cech[];
	extern __BYTE__ ___196dd4h[];
	extern __BYTE__ ___1de580h[];
	extern int MY_CAR_IDX;
	extern int NUM_OF_LAPS;
	extern __BYTE__ ___196df0h[];
	extern int LAP_PREVIOUS_MIN;
	extern int LAP_PREVIOUS_SEC;
	extern int LAP_BEST_MIN;
	extern int LAP_PREVIOUS_100;
	extern int LAP_BEST_SEC;
	extern int LAP_BEST_100;
	extern int LAP_RECORD_MIN;
	extern int LAP_RECORD_SEC;
	extern int LAP_RECORD_100;
	extern char ___19bd64h[16];
	extern __BYTE__ ___243ca0h[];
	extern __BYTE__ ___243cb8h[];
	extern __BYTE__ ___243cdch[];
	extern __BYTE__ ___1de7d0h[];
	extern __BYTE__ ___196ab0h[];
#if defined(DR3_CHAOS)
	extern __BYTE__ ___243ce4h[];
	extern int ___243178h[DR3_MAX_CARS];
#endif
#if defined(DR_MULTIPLAYER)
	extern __DWORD__ ___19bd60h;
#endif // DR_MULTIPLAYER

void dRally_Sound_pushEffect(__BYTE__ channel, __BYTE__ n, __DWORD__ unk, __DWORD__ a0, __DWORD__ a1, __DWORD__ a2);

#if defined(DR3_CHAOS)
static int dr3_race_finish_seq[DR3_MAX_CARS];
static int dr3_race_finish_next;

void dr3_race_finish_reset(void){

	int n;

	dr3_race_finish_next = 0;
	for(n = 0; n < DR3_MAX_CARS; ++n) dr3_race_finish_seq[n] = 0;
}

void dr3_race_rank_refresh(void){

	dr3_race_car_t  cars[DR3_MAX_CARS];
	int             pos[DR3_MAX_CARS];
	int             n, m, count, dead_n;
	struct_35e_t *  s_35e;

	/* Battle Arena ranks strictly by survival order (dr3_arena_note_deaths / dr3_arena_final_places) */
	if(dr3_chaos_arena()) return;

	count = NUM_OF_CARS;
	if(count <= 0) return;
	if(count > DR3_MAX_CARS) count = DR3_MAX_CARS;

	s_35e  = (struct_35e_t *)___1e6ed0h;
	dead_n = (int)D(___243ce4h);
	if(dead_n < 0) dead_n = 0;
	if(dead_n > DR3_MAX_CARS) dead_n = DR3_MAX_CARS;

	for(n = 0; n < count; ++n){

		int dead_ord = -1;

		for(m = 0; m < dead_n; ++m){

			if(___243178h[m] == n){ dead_ord = m; break; }
		}

		cars[n].hp         = (int)D(___1de580h + 0x94 * n + 0x18);
		cars[n].lap        = (int)s_35e[n].Lap;
		cars[n].checkpoint = (int)s_35e[n].__0;
		cars[n].finished   = (int)s_35e[n].Finished;
		cars[n].finish_seq = dr3_race_finish_seq[n];
		cars[n].dead_order = dead_ord;
		cars[n].old_pos    = (int)s_35e[n].Position;
	}

	dr3_race_rank_positions(cars, count, pos);

	for(n = 0; n < count; ++n){

		s_35e[n].Position = (__BYTE__)pos[n];
	}
}
#endif

// POSITION, LAP COUNTER
void race___55ae0h(void){

    int             i, j, n, LapPrevious, LapBest, LapRecord;
	double 	        d_tmp;
	__DWORD__ 	    eax, ebx, ecx, edx, ebp;
	__BYTE__ 	    esp[0xc];
    struct_54_t *   s_54;
    struct_94_t *   s_94;
    struct_35e_t *  s_35e;


    s_54 = (struct_54_t *)___1de7d0h;
    s_94 = (struct_94_t *)___1de580h;
    s_35e = (struct_35e_t *)___1e6ed0h;

    n = -1;
    while(++n < NUM_OF_CARS){

        ecx = B(TRX_VAI+TRX_WIDTH_QTR*((int)s_35e[n].Y1__112>>0x2)+((int)s_35e[n].X1__10e>>0x2));
        edx = B(TRX_VAI+TRX_WIDTH_QTR*((int)s_35e[n].Y2__122>>0x2)+((int)s_35e[n].X2__11e>>0x2));
        D(esp+0x4) = B(TRX_VAI+TRX_WIDTH_QTR*((int)s_35e[n].YLocation>>0x2)+((int)s_35e[n].XLocation>>0x2));

#if defined(DR3_CHAOS)
        s_35e[n].__0 = (__DWORD__)dr3_race_update_checkpoint(
            (int)s_35e[n].__0, (int)D(___243cech),
            (int)ecx, (int)edx, (int)D(esp+0x4));
#else
        ebx = s_35e[n].__0+1;

        if(ecx == ebx){

            s_35e[n].__0++;
        }
        else {

            if(edx == ebx) s_35e[n].__0++;
        }

        if(D(esp+0x4) == D(___243cech)){
            
            if((int)D(___243cech) > (int)s_35e[n].__0) s_35e[n].__0 = 0;
        }
#endif
    }

#if !defined(DR3_CHAOS)
    j = -1;
    while(++j < NUM_OF_CARS){

        i = -1;
        while(++i < NUM_OF_CARS){

            if(s_35e[i].Finished == 0){

                if(i != j){

                    if((int)s_35e[j].__0 > (int)s_35e[i].__0){

                        if(s_35e[j].Lap >= s_35e[i].Lap){

                            if(s_35e[j].Position > s_35e[i].Position){

                                B(esp+0x4) = s_35e[j].Position;
                                s_35e[j].Position = s_35e[i].Position;
                                s_35e[i].Position = B(esp+4);
                            }
                        }
                    }
                }
            }
        }
    }
#endif

    n = -1;
    while(++n < NUM_OF_CARS){

        if(s_35e[n].__0 == D(___243cech)){

            if(D(___196dd4h) != 0){
            
                if(s_35e[n].Finished == 0){
                    s_35e[n].Finished = 1;
#if defined(DR3_CHAOS)
                    if((n >= 0) && (n < DR3_MAX_CARS) && (dr3_race_finish_seq[n] == 0))
                        dr3_race_finish_seq[n] = ++dr3_race_finish_next;
#endif
                }
                s_94[n].__4 = 0.0f;
            }
            else {
                    
                s_35e[n].Lap++;

                if(n == MY_CAR_IDX){
            
                    if(!dr3_chaos_battle() && (s_35e[n].Lap == NUM_OF_LAPS)){
                    
                        dRally_Sound_pushEffect(2, SFX_FINAL_LAP, 0, 0x10000, 0x50000, 0x8000);
                        D(___196df0h) = 210;
                    }

                    LapPrevious = 6000*LAP_PREVIOUS_MIN+100*LAP_PREVIOUS_SEC+LAP_PREVIOUS_100;
                    LapBest = 6000*LAP_BEST_MIN+100*LAP_BEST_SEC+LAP_BEST_100;
                    LapRecord = 6000*LAP_RECORD_MIN+100*LAP_RECORD_SEC+LAP_RECORD_100;

                    if((LapPrevious < LapBest)||(LapBest == 0)){
            
                        LAP_BEST_MIN = LAP_PREVIOUS_MIN;
                        LAP_BEST_SEC = LAP_PREVIOUS_SEC;
                        LAP_BEST_100 = LAP_PREVIOUS_100;
                    }

                    if((LapPrevious < LapRecord)||(LapRecord == 0)){

                        if((___19bd64h[2] != 0x30)&&(D(___196df0h) == 0)) dRally_Sound_pushEffect(2, SFX_LAP_RECORD, 0, 0x10000, 0x50000, 0x8000);

                        LAP_RECORD_MIN = LAP_PREVIOUS_MIN;
                        LAP_RECORD_SEC = LAP_PREVIOUS_SEC;
                        LAP_RECORD_100 = LAP_PREVIOUS_100;
                    }
                    else {

                        D(___196df0h) = 0;
                    }

                    D(___243cb8h) = D(___243ca0h);
                    D(___243cdch) = 210;
                    LAP_PREVIOUS_100 = 0;
                    LAP_PREVIOUS_SEC = 0;
                    LAP_PREVIOUS_MIN = 0;
                    D(___243ca0h) = 0;
                }
            }

            s_35e[n].__0 = 0;
            if(s_35e[n].Lap > NUM_OF_LAPS){

                /*
                 * A deathmatch / arena has no finish line (drally.h: DR3_CHAOS_MODE_DM): the lap counter keeps
                 * running (clamped to DR3_CHAOS_HUD_LAP_MAX for the HUD digit sprites), top speed stays intact
                 * (zeroing s_94[n].__4 would freeze every car after lap N!), and the finish flag stays 0 - only
                 * the last car standing ends the battle.
                 */
                if(dr3_chaos_battle()){
                    if((int)s_35e[n].Lap > DR3_CHAOS_HUD_LAP_MAX) s_35e[n].Lap = DR3_CHAOS_HUD_LAP_MAX;
                }
                else {
                    if(s_35e[n].Finished == 0){
                        s_35e[n].Finished = 1;
#if defined(DR3_CHAOS)
                        if((n >= 0) && (n < DR3_MAX_CARS) && (dr3_race_finish_seq[n] == 0))
                            dr3_race_finish_seq[n] = ++dr3_race_finish_next;
#endif
                    }
                    s_35e[n].Lap = NUM_OF_LAPS;

                    if(n == MY_CAR_IDX){
                    
                        s_35e[n].Drug = 0;
                        D(___243cdch) = 9999;
                    }

                    s_94[n].__4 = 0.0f;
                    D(___196dd4h) = 1;

#if defined(DR3_CHAOS)
                    if(!strcmp(s_54[n].__0, "DUKE NUKEM")&&(dr3_race_finish_seq[n] == 1)) dRally_Sound_pushEffect(2, SFX_HAIL_TO_THE_KING, 0, 0x10000, 0x50000, 0x8000);
#else
                    if(!strcmp(s_54[n].__0, "DUKE NUKEM")&&(s_35e[n].Position == 1)) dRally_Sound_pushEffect(2, SFX_HAIL_TO_THE_KING, 0, 0x10000, 0x50000, 0x8000);
#endif
                }
            }
        }
    }

#if defined(DR3_CHAOS)
    dr3_race_rank_refresh();
#endif

#if defined(DR_MULTIPLAYER)
    if(___19bd60h == 0){
#endif // DR_MULTIPLAYER

        if((s_94[0].__0 != 6) && !dr3_chaos_battle()){

            ebp = 0;
            n = -1;
            while(++n < NUM_OF_CARS){
#if defined(DR3_CHAOS)
                if((int)s_94[n].__18 <= 0) continue;
#endif
                edx = s_35e[n].__0+D(___243cech)*s_35e[n].Lap;
                if(((int)edx > (int)ebp)&&(n != MY_CAR_IDX)) ebp = edx;
            }

            eax = s_35e[MY_CAR_IDX].__0+D(___243cech)*(s_35e[MY_CAR_IDX].Lap+1);

            if(((int)ebp > (int)eax)&&(D(___196ab0h) == 0)
#if defined(DR3_CHAOS)
               &&(s_35e[MY_CAR_IDX].Finished == 0)&&((int)s_94[MY_CAR_IDX].__18 > 0)
#endif
            ){

                D(___196ab0h) = 1;
                dRally_Sound_pushEffect(2, SFX_YOUVE_BEEN_LAPPED, 0, 0x10000, 0x50000, 0x8000);

                /*
                 * The only trace of this flag: it leads to the boss' "You were lapped, you loser!" screen in
                 * the menu (shop_main.c:234/489).  Chaos does not show that screen, so a run can be read for
                 * this line instead of for the picture.
                 */
                dr3_log("[dr3] chaos: lapped: best other car at %d, the player at %d (in chaos the menu screen of the boss stays out)",
                        (int)ebp, (int)eax);
            }

            if(((int)ebp < (int)eax)&&(D(___196ab0h) == 1)) D(___196ab0h) = 0;
        }
#if defined(DR_MULTIPLAYER)
    }
#endif // DR_MULTIPLAYER
}
