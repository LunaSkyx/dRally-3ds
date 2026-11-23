/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/* dr3_quick.c - see dr3_quick.h.  Adapted from dr3_host_quick_race() (dr3_host.c); read its comments there. */
#include "dr3_quick.h"
#include "drally.h"
#include "drally_structs_fixed.h"
#include "drally_structs_free.h"
#include "dr3_chaos.h"
#include "dr3_boss.h"
#include "dr3_log.h"

extern __BYTE__ ___1a1f64h[];   /* the track of each of the three races, then its signed up car count */
extern __BYTE__ ___1a0ef8h[];   /* the field: three races of four racers                              */
extern __BYTE__ ___1a1ef8h[];   /* the player's racer (an index into s_6c)                            */
extern __BYTE__ ___1a1028h[];   /* which of the three races is the next one                           */
extern __BYTE__ ___1a103ch[];   /* the player's car index inside that race                            */
extern __BYTE__ ___1a0f04h[];   /* the "this racer is in the event" flags (0x14 of them)             */
extern const char ___106cbh[19][4];
extern __POINTER__ ___1a0f8ch;  /* the PREPARE TO RACE banner, loaded by menu_main's ___2faf0h        */
extern __POINTER__ ___1a1114h;  /* the menu's decode buffer, allocated by ___2faf0h as well           */

void ___31588h(void);            /* a fresh event: three random tracks, empty field (___31588h.c) */
void ___33010h_cdecl(int);       /* "prepare to race" + race_main (___33010h.c)                   */

#define QLOG(...) dr3_log_write("[scn] quick: " __VA_ARGS__)

int dr3_quick_ready(void)
{
    const int player = (int)D(___1a1ef8h);

    return (___1a0f8ch != 0) && (___1a1114h != 0) && (player >= 0) && (player <= 0x13);
}

void dr3_quick_set_player_hp(int hp)
{
    extern __BYTE__ ___1de580h[];
    extern __DWORD__ MY_CAR_IDX;

    if (MY_CAR_IDX < DR3_MAX_CARS) D(___1de580h + 0x94 * MY_CAR_IDX + 0x18) = hp;
}

void dr3_quick_set_player_drug(int drug)
{
    extern __BYTE__ ___1e6ed0h[];
    extern __DWORD__ MY_CAR_IDX;
    struct_35e_t *s_35e = (struct_35e_t *)___1e6ed0h;

    if (MY_CAR_IDX < DR3_MAX_CARS) s_35e[MY_CAR_IDX].Drug = drug;
}

void dr3_quick_set_player_rank(int rank)
{
    extern __BYTE__ ___1a01e0h[];
    racer_t *s_6c = (racer_t *)___1a01e0h;
    const int player = (int)D(___1a1ef8h);
    int i;

    if ((player < 0) || (player > 0x13)) return;
    if (rank < 1) rank = 1;
    if (rank > 20) rank = 20;

    for (i = 0; i < 20; ++i) {
        if (i == player) continue;
        if ((int)s_6c[i].rank == rank) {
            s_6c[i].rank = s_6c[player].rank;
            break;
        }
    }
    s_6c[player].rank = rank;
}

void dr3_quick_set_boss_hp(int hp)
{
    extern __BYTE__ ___1de580h[];

    D(___1de580h + 0x18) = hp;
}

void dr3_quick_boss(int phase)
{
    extern __BYTE__ ___1a01e0h[];
    extern __BYTE__ ___185a14h_UseWeapons[];
    void ___31008h(void);
    racer_t *s_6c = (racer_t *)___1a01e0h;
    const int player = (int)D(___1a1ef8h);

    if ((player < 0) || (player > 0x13)) { QLOG("no racer loaded yet (player index %d)", player); return; }
    if ((phase < DR3_BOSS_PHASE_1) || (phase > DR3_BOSS_PHASE_3)) phase = DR3_BOSS_PHASE_1;

    ___196a94h_difficulty = DR3_DIFFICULTY_CHAOS;
    dr3_chaos_wanted      = 1;
    D(___185a14h_UseWeapons) = 1;

    /* equip player as #1 championship contender with a maxed Deliverator */
    s_6c[player].car         = 5;
    s_6c[player].engine      = 3;
    s_6c[player].tires       = 3;
    s_6c[player].armor       = 3;
    s_6c[player].damage      = 0;
    s_6c[player].rocket_fuel = 1;
    s_6c[player].spikes      = 1;
    s_6c[player].mines       = 6;
    if ((int)s_6c[player].money < 20000) s_6c[player].money = 20000;
    s_6c[player].points      = 999;
    s_6c[player].rank        = 1;

    ___31588h();
    dr3_boss_set_phase(phase);
    dr3_boss_save_sync();

    QLOG("quickboss phase %d (player %d, %s)", phase, player, s_6c[player].name);
    ___31008h();
    QLOG("quickboss returned (phase now %d)", dr3_boss_phase());
}

void dr3_quick_set_raced(void)
{
    extern int dr3_chaos_raced_force;

    dr3_chaos_raced_force = 1;
}

void dr3_quick_race(int slot, int track)
{
    const int player = (int)D(___1a1ef8h);
    int       i, tier, placed = 0, found = -1;

    if ((player < 0) || (player > 0x13)) { QLOG("no racer loaded yet (player index %d)", player); return; }
    if ((slot < 0) || (slot > 2)) slot = 0;

    /*
     * The sign up screen reads sdmc:/chaos_mode.txt when it opens (___3266ch.c); this start has no sign up
     * screen, so it reads it here.  Left to the race setup (___33010h.c) the file is read too late: that code
     * first remembers the mode "the wheel rolled" - the setting, RANDOM, when nothing rolled - and puts it back
     * over what the file said, so a duel asked for by the file ran as a plain twenty car race.
     */
    dr3_chaos_mode_reload();

    /* picking the Chaos level in the licence screen sets both (___3ab5ch.c); a direct start has to as well */
    ___196a94h_difficulty = DR3_DIFFICULTY_CHAOS;
    dr3_chaos_wanted      = 1;

    QLOG("player %d, race slot %d, difficulty chaos, %d extra car(s) asked for, mode %d",
         player, slot, dr3_chaos_extra_cars(), dr3_chaos_mode());

    ___31588h();                                    /* the event setup: three tracks, empty field */
    if (track >= 0) B(___1a1f64h + slot) = track;

    /* the field, filled by the picker's rule without its screen (dr3_host.c explains why not ___3079ch) */
    for (tier = 0; tier < 3; ++tier) {
        while (B(___1a1f64h + tier + 3) < 4) {
            int free_racer = -1;

            for (i = 0; i < 0x14; ++i) if (B(___1a0f04h + i) != 1) { free_racer = i; break; }
            if (free_racer < 0) break;

            B(___1a0f04h + free_racer) = 1;
            B(___1a0ef8h + 4 * tier + B(___1a1f64h + tier + 3)) = free_racer;
            B(___1a1f64h + tier + 3)++;
            ++placed;
        }
    }

    /* the player into the race he picked (his own place if he is in that field already) */
    for (i = 0; i < 4; ++i) if (B(___1a0ef8h + 4 * slot + i) == player) found = i;
    if (found < 0) { found = 3; B(___1a0ef8h + 4 * slot + found) = player; }
    D(___1a1028h) = slot;
    D(___1a103ch) = found;

    QLOG("%d car(s) placed, track %s, field %d/%d/%d, the player is car %d", placed,
         (B(___1a1f64h + slot) < 19) ? ___106cbh[B(___1a1f64h + slot)] : "??",
         (int)B(___1a1f64h + 3), (int)B(___1a1f64h + 4), (int)B(___1a1f64h + 5), (int)D(___1a103ch));

    ___33010h_cdecl(4);                             /* PREPARE TO RACE + race_main */
    QLOG("race_main returned");
}
