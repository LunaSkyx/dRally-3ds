#ifndef __DRALLY_H
#define __DRALLY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <SDL2/SDL.h>

#include "types.h"

#if defined(__3DS__)
#include "platform_3ds/dr3_log.h"

/*
 * Native 400x240 widescreen race viewport on the 3DS top screen: renders a genuine 400x240 slice of
 * the track into BACKBUFFER (which already has a 512-byte stride: columns 0x60..0x1f0 = 400 px) and
 * blits 1:1 into the 400x240 top screen framebuffer with zero stretching.  PC / host builds keep the
 * original 320x200 race viewport.
 */
#if defined(DR3_USE_GFX) && !defined(DR3_HOST)
#define DR3_NATIVE_WIDESCREEN 1
#define DR3_RACE_W            400
#define DR3_RACE_H            240
#else
#define DR3_NATIVE_WIDESCREEN 0
#define DR3_RACE_W            320
#define DR3_RACE_H            200
#endif

/*
 * libctru's console is initialised on the bottom screen so the port can show the controls, the driver
 * standings and (in the profiler build) its statistics there.  That also means stdout draws onto that
 * screen, and the decompiled engine has 126 printf() leftovers - some fire over and over while racing
 * (e.g. "[TODO] IN instruction for Joystick/Gamepad"), which made the screen flicker.  Sending every
 * printf() to the log file keeps the bottom screen ours; nothing uses printf()'s return value, so the
 * replacement is safe.  The bottom screen code (dr3_bottom.c / dr3_prof.c) needs the real printf and
 * undefines this again.
 */
#define printf(...) dr3_log(__VA_ARGS__)
#else
#define DR3_NATIVE_WIDESCREEN 0
#define DR3_RACE_W            320
#define DR3_RACE_H            200
/*
 * Not the 3DS build (Linux, BSD, Windows): the port's helpers are not linked there, so they compile
 * away.  The shared engine files call them (log lines, profiler hooks), and this keeps those files
 * buildable on every platform without sprinkling #if defined(__3DS__) over the engine sources.
 */
#include "platform_3ds/dr3_prof.h"      /* no-ops unless DR3_PROFILE is defined */
#define dr3_log(...)            ((void)0)
#endif


typedef void (*void_cb)(void);

/*
 * ------------------------------------------------------------------------------------------------
 * The fourth difficulty this port adds ("30th Anniversary", see doc/3ds.md).  On that level the
 * adversary - the SPECIAL car - is a full championship participant: he has a seat in the roster,
 * drives his own car, rides along in every race, collects points like all the others (so he ends up
 * leading the championship) and the final challenge is offered from the top of the table on - first
 * place (he is the champion, now face him) or second place, right behind him.
 * Everything hangs off the difficulty value, so it travels with dr.cfg and with the save games.
 * ------------------------------------------------------------------------------------------------
 */
#define DR3_DIFFICULTY_ADVERSARY    3      /* = ANNIVERSARY in race___3f970h.c */
#define DR3_ADVERSARY_CAR           6      /* DELIVERATOR_ADVERSARY - the car the adversary drives */
#define DR3_ADVERSARY_RACER         18     /* seat he is created in (he moves up the table later) */
#define DR3_ADVERSARY_START_RANK    3      /* he starts third in points and works his way up */
#define DR3_ADVERSARY_CAR_PIC       5      /* the picture the standings show for him: the Deliverator,
                                              the SPECIAL being its black twin (carres.bpk holds six) */
#define DR3_LAST_CAR                6      /* the Anniversary is the last car of the shop list */

/*
 * The lap-record table has six rows and lives in dr.cfg, so its size must not change: an Anniversary
 * owner shares the Deliverator's row ("30th Anniversary", see doc/3ds.md).
 */
#define DR3_RECORD_CAR(car)         (((int)(car) < DR3_LAST_CAR) ? (int)(car) : (DR3_LAST_CAR-1))
#define DR3_ADVERSARY_SPEED         1.08f  /* the boss is a little quicker than the table says */
#define DR3_ADVERSARY_STEER         1.05f  /* ... and turns in a little sharper */
#define DR3_ADVERSARY_DARK          60     /* his pictures become this dark (%): black as night */

extern __DWORD__ ___196a94h_difficulty;
extern int       dr3_adversary_wanted;

#define dr3_adversary_active()      (dr3_adversary_wanted || (___196a94h_difficulty == DR3_DIFFICULTY_ADVERSARY))

/*
 * The fifth difficulty, "Chaos" (dr3_chaos.h): the same pattern as the fourth one above.  When the
 * player picks it, twenty cars race instead of the track's four - the grid placement, the colours and
 * the car count all hang off it (dr3_chaos_active).  For the *engine* it is not a level of its own:
 * the AI tables have a row per difficulty, and "Chaos" reuses the hardest one (DR3_DIFFICULTY_ROW),
 * so only the number of cars is new.
 */
#define DR3_DIFFICULTY_CHAOS        4      /* the fifth entry in the licence screen's list */
#define DR3_DIFFICULTY_ROW(d)       (((d) > DR3_DIFFICULTY_ADVERSARY) ? DR3_DIFFICULTY_ADVERSARY : (d))

extern int dr3_chaos_wanted;

#define dr3_chaos_active()          (dr3_chaos_wanted || (___196a94h_difficulty == DR3_DIFFICULTY_CHAOS))

/*
 * Wrecking an opponent pays on the Chaos difficulty (race___4dcach.c).  The payout pops up over the
 * wreck through the engine's own floating text (race___51eb4h.c, the one a picked up item uses),
 * which draws the bonus larger than an item's text and centres it on the wreck.  That renderer reads
 * the type of a slot one lower than the writer stores it (the item types are 1..8 there).  The kill
 * bonus is not an item, so it gets a number of its own - and it is read as
 * "case DR3_EFFECT_KILL_BONUS-1" for exactly that reason.
 */
#define DR3_EFFECT_KILL_BONUS       9

/*
 * The payout (race___4dcach.c) credits racer.money at once - the results screen adds the race income
 * to that same field instead of replacing it (___33010h.c: "money += bonus"), so the money a wreck
 * earned stays in the total it shows.  `cause` only names the hit that wrecked the car, for the log.
 */
void dr3_chaos_kill_bonus(int victim, const char * cause);

/*
 * The rank money of a chaos race lives in platform_3ds/dr3_payout.h - a ladder the host unit tests check
 * place by place (tests/test_dr3.c): the winner takes DR3_CHAOS_RANK_MONEY, the podium pays its jump
 * instead, every place above the last one pays DR3_CHAOS_RANK_STEP more than the one behind it, and the
 * last place pays nothing at all - which is what a car that was wrecked and pushed to the back takes home
 * (___33010h.c calls dr3_chaos_rank_money(place, cars) for it).
 *
 * The ladder *replaces* the race's own prize money: those tiers pay the first three places only
 * (750/3000/12000 depending on the race) and the switch that hands them out is skipped in chaos
 * (___33010h.c).  What it pays goes into racer.money and racer.income and shows up as the report's "Race
 * income" line.  The Arena (the fourth race) is "GLORY" and pays no money at all, so it is left out.
 */

/*
 * The points of a chaos race (___33010h.c).  Every place scores - all twenty cars are in the race - and the
 * spread of a single race stays narrow, so the front of the standings cannot pull away and catching up
 * stays possible: 1st +6, 2nd +5, 3rd +4, 4th +3, 5th +2 and every other place +1.  The normal races keep
 * their own tables (easy 3/2/1, medium 5/3/1, hard ...), which reach three places only; the results screens
 * skip them in chaos and show these instead (___36068h.c, ___36358h.c, ___36718h.c).
 */
#define DR3_CHAOS_POINTS_TOP        6
#define dr3_chaos_points(place)     ((((place) >= 1) && ((place) <= DR3_CHAOS_POINTS_TOP)) ? (DR3_CHAOS_POINTS_TOP + 1 - (place)) : 1)

/*
 * The race mode of a chaos race (___33010h.c, platform_3ds/dr3_log.c).  The chaos difficulty is the level
 * the player picks (dr3_chaos_active); on top of it this switch says *what kind* of race the next one is.
 * It sits next to the extra car switch, is read at every race setup (sdmc:/chaos_mode.txt, or the key
 * combo of platform_3ds/dr3_input.c) and only ever changes races that are chaos races - every other level
 * of the game runs exactly as the original does.
 *
 *     DR3_CHAOS_MODE_STANDARD   the whole standings on one grid, both payouts as they are
 *     DR3_CHAOS_MODE_DUEL       one rival, picked by rank: a two car race over DR3_CHAOS_DUEL_LAPS laps
 *                               whose winner takes Elo points that grow with the rank difference
 *
 * (A mode is a *rule set*: how many cars (duel vs. the whole field), how the race ends, what it pays.  The
 * next one - "destroy 'em all", no laps, last car standing - plugs into the same switch.)
 */
#define DR3_CHAOS_MODE_STANDARD     0
#define DR3_CHAOS_MODE_DUEL         1
#define DR3_CHAOS_MODE_DM           2
#define DR3_CHAOS_MODE_RANDOM       3      /* let the sign up screen spin for a race and a mode (___3266ch.c) */
#define DR3_CHAOS_MODE_ARENA        4      /* an epic free for all on an arena of its own (platform_3ds/dr3_arena.h) */
#define DR3_CHAOS_MODE_NUM          5

/*
 * The mode a chaos race starts in.  This build *is* about the wheel, so the selector spins out of the box:
 * the mode has to be one of the fixed ones (L+R+Y in the front end, or sdmc:/chaos_mode.txt) only when the
 * player wants one.  A savegame without the chaos difficulty never sees any of this (dr3_chaos_active).
 */
#if defined(DR3_CHAOS)
#define DR3_CHAOS_MODE_DEFAULT      DR3_CHAOS_MODE_RANDOM
#else
#define DR3_CHAOS_MODE_DEFAULT      DR3_CHAOS_MODE_STANDARD
#endif


/*
 * The Elo of a duel (___33010h.c).  The *winner* takes points that grow with the difference of the two
 * ranks, so beating somebody above you is worth more than beating the man behind:
 *
 *     gain = DR3_CHAOS_ELO_BASE + DR3_CHAOS_ELO_STEP * (rank of the rival - rank of the player)
 *
 * with the difference clamped to +/- DR3_CHAOS_ELO_MAX_DIFF (a rival twenty places up pays the same as one
 * DR3_CHAOS_ELO_MAX_DIFF places up).  Losing pays nothing - the points of the player are what the original
 * career hangs off (___3892ch.c reads them to decide whether the ending is played and whether the player
 * is "no longer last", ___196ab4h), so a duel can only ever move him up.  The duel's *money* is what makes
 * losing hurt: only the winner is paid, and the pot follows the rank of the rival (see below), so a duel
 * against a driver of his own class costs a race when it is lost.
 */
#define DR3_CHAOS_ELO_BASE          4
#define DR3_CHAOS_ELO_STEP          2
#define DR3_CHAOS_ELO_MAX_DIFF      6
#define DR3_CHAOS_DUEL_LAPS         2

/*
 * What a duel pays (___33010h.c).  Only a *win* pays, and the pot follows the rank of the rival - beating
 * a driver at the top of the standings is worth the most:
 *
 *     rival rank 1 .. DR3_CHAOS_DUEL_TOP_RANK    DR3_CHAOS_DUEL_POT_TOP
 *     rival rank .. DR3_CHAOS_DUEL_MID_RANK      DR3_CHAOS_DUEL_POT_MID
 *     every lower rival                          DR3_CHAOS_DUEL_POT_LOW
 *
 * Losing a duel pays nothing at all: the rank money ladder of a field race (platform_3ds/dr3_payout.h) does
 * not apply to a duel, a duel is worth a race and not a place in the field.  The Arena is the exception it
 * always was - "GLORY" pays no money (___33010h.c).
 */
#define DR3_CHAOS_DUEL_POT_TOP      5000
#define DR3_CHAOS_DUEL_POT_MID      2500
#define DR3_CHAOS_DUEL_POT_LOW      1000
#define DR3_CHAOS_DUEL_TOP_RANK     5
#define DR3_CHAOS_DUEL_MID_RANK     13

/*
 * "Destroy 'em all" (`DR3_CHAOS_MODE_DM`): the race has no finish line - nobody is done when the laps are up
 * (race___55ae0h.c) - and it ends when only one car is left standing.  That ending is the engine's own: it
 * counts the cars that are finished or destroyed (race_main.c, `___243d0ch`) and stops the race once all but
 * one of them are gone, so the mode only has to take the finish line away and pay the result.
 *
 * The survivor takes `DR3_CHAOS_DM_POT` and the winner's points (the first place of the chaos curve, +6);
 * everybody else scores nothing for the race - but every wreck a driver caused paid its kill money the moment
 * it happened (race___4dcach.c, `dr3_chaos_kill_money`), which is what the report shows as the race's bonus
 * income.  That is the "money for destroyed opponents" the mode keeps: it is earned wreck by wreck and is not
 * taken away again when the player himself is the one who is destroyed.
 *
 * That engine rule is not enough to *end* the mode.  It only counts a car as out when it is destroyed *and
 * standing still* (race_main.c wants `s_35e[n].__b0 <= 0.5f`), and the only other way out is the wreck of the
 * player - so one stuck or unreachable car keeps the race running forever, and a race the player survives can
 * hang.  The port therefore gives the mode a deadline (platform_3ds/dr3_dm.h - the file has the rule and the
 * tuning, this is the entry in the port's own list): three minutes into the race the **sudden death** starts,
 * every car loses one percent of its HP per second in the game's own unit (1024 HP, race___400ech.c) - the
 * player included - and as soon as only one car is left the port starts the same wind down the player's own
 * wreck uses.  A banner (`SUDDEN DEATH`, 18 px letters of the game's own font, outlined) says what is going on.
 * `sdmc:/chaos_dm.txt` holds the seconds until it starts; 0 switches it off and gives the old mode back.
 *
 * The sudden death alone would make camping the winning move (everybody loses the same HP per second, so
 * whoever takes no damage outlives the field and takes the pot), so the mode has a **lap duty** as well
 * (platform_3ds/dr3_dm.h): every minute of race time a car has to have finished one lap, and a car that has
 * not loses 25 percent of its HP (25600, the same 1024 HP per percent) at that check - four missed checks
 * and it is a wreck.  The laps are counted by the port itself (the engine's `Lap` field is clamped for the
 * HUD in a deathmatch), from the engine's own lap crossing: `s_35e[n].__0` goes back to 0 exactly then.
 * `sdmc:/chaos_dm_lap.txt` holds the seconds between two checks (0 switches the duty off, the harness uses
 * `dmlap <n>`).  Both, the drain and the duty, are gone the moment the race is over - and so are the port's
 * health lines and the banner (`dr3_chaos_race_over`: the RACE OVER screen and its fade to black must not
 * have anything of the port over them).
 */
#define DR3_CHAOS_DM_POT            5000

/*
 * The random selector (`DR3_CHAOS_MODE_RANDOM`, ___3266ch.c).  The race mode is a setting the player makes -
 * this one says "you choose": the sign up screen draws for one of the three rule sets *and* for one of the
 * three races, lands on both and leaves the registration itself to ENTER.  So everything that makes a sign up
 * a sign up (the car driving in, the sounds, the seat in the grid) stays untouched; only the *choice* is new.
 *
 * The screen is drawn in the two rounds the player sees, both over the boxes the selection already has
 * (`D(___185a50h)` is the highlighted race and the copper frame around it is `___27f80h_cdecl` /
 * `___281d0h_cdecl`): round one puts the three modes into the three boxes and lets the frame hop over them -
 * it starts fast and settles, which is what reads as a slot machine - and leaves the drawn mode on the screen
 * as a banner at the top; round two draws the race, and the race it lands on is what ENTER signs up for.
 *
 * Only races the player may sign up for are drawn (a free slot, `___1a1f64h`, and the class checks of the
 * ENTER branch), the three modes have the same chance each, and the draw is logged
 * (`[dr3] chaos: spin: landed on race N and mode M (NAME)`).
 */
#define DR3_CHAOS_SPIN_STEPS        18     /* boxes the race round passes before it lands      */
#define DR3_CHAOS_SPIN_MODE_STEPS   12     /* ... and the mode round in round 1                */
#define DR3_CHAOS_SPIN_DELAY_FIRST  1      /* frames per step when it is fast                  */
#define DR3_CHAOS_SPIN_DELAY_LAST   9      /* ... and when it settles (the ramp is quadratic)  */
#define DR3_CHAOS_SPIN_LAND_HOLD    26     /* frames a round is left alone before the next one */
                                           /* (or the registration) takes over                 */
#define DR3_CHAOS_SPIN_MODE_HOLD_S  3      /* seconds round 1 stays on its result before round 2
                                            * starts - long enough to read *which* mode was
                                            * drawn (frames: seconds * ___60458h, the rate the
                                            * front end runs at, ___60420h.c)                  */
/*
 * How many pickups a deathmatch / arena keeps on the track at once.  The original spawns its obstacles and
 * pickups in up to four slots (race___5209ch.c: `if((int)D(esp+0x18) < 4)`), which is nothing in a Last Car
 * Standing brawl of up to twenty armed cars: battle modes use all sixteen track powerup pads simultaneously.
 */
#define DR3_CHAOS_DM_PICKUPS_MAX    16
/*
 * The lap display of the race HUD draws the lap number from a table of ten 8x10 sprites
 * (`race___40f48h.c`, `___1de210h`, 0x320 bytes).  A deathmatch drives past that - nothing ends it - so its
 * counter and the sprite index both stop here instead of reading past the table.  The race itself does not
 * care: in a deathmatch only the last car standing ends it (race_main.c).
 */
#define DR3_CHAOS_HUD_LAP_MAX       9
/* how far from the player's own rank the rival of a duel may be (dr3_log.c: dr3_chaos_duel_pick) */
#define DR3_CHAOS_DUEL_RANK_WINDOW  3
/*
 * The rows of the roster screen ("PREPARE TO RACE", ___33010h.c).  The background art of that screen has four
 * framed columns with the numbers 1..4 in them, and the original game fills them with the four entries of the
 * event.  A chaos race is the whole standings on one grid, so the screen names the four *leaders* of the field
 * there instead - the cars that start from the first four places, which is the order the grid itself lines up
 * in (dr3_grid_order_by_rank, ___33010h.c).  A duel has two cars and draws two rows.
 */
#define DR3_ROSTER_ROWS             4

/*
 * What the kills of the running race paid so far (race___4dcach.c).  A wreck's money is credited to
 * racer.money and racer.income the moment it is earned; the report after the race shows it as the race's
 * own bonus income, which is what this counter carries:
 *
 *     dr3_chaos_kill_money()          the money this race's kills paid (the report's number)
 *     dr3_chaos_kill_money_reset()    a race starts (___33010h.c, right before race_main)
 *
 * The report reads it once, so a race that ends without a report adds nothing to the next one.
 */
int  dr3_chaos_kill_money(void);
void dr3_chaos_kill_money_reset(void);

/*
 * Who a wreck is credited to.  Every place that can take a car's HP tells the chaos code about it and
 * then asks whether the car just died on the player:
 *
 *     dr3_chaos_frame()               once per race frame (race___4dcach.c, its clock)
 *     dr3_chaos_player_damage(v)      the player's own damage took v's HP (bumper, gun)
 *     dr3_chaos_player_contact(a, b)  the player's car and a/b touched (a bump counts too)
 *     dr3_chaos_wreck(v, hp, cause)   v went from hp > 0 to 0: pay if the player was the last
 *                                     thing that happened to it (a couple of seconds)
 *
 * So a wreck pays whatever took the last HP - the player's bumper, his gun, or the car's own trouble
 * right after he hit or bumped it - and a wreck the player had no part in pays nothing.
 */
void dr3_chaos_frame(void);
void dr3_chaos_player_damage(int victim);
void dr3_chaos_player_contact(int a, int b);
void dr3_chaos_wreck(int victim, int hp_before, const char * cause);

#define S(s, type, offset)     (*(type *)((__POINTER__)(s)+(offset)))

#define Q(q)    (*(__QWORD__ *)(q))
#define D(d)	(*(__DWORD__ *)(d))
#define W(w)	(*(__WORD__ *)(w))
#define B(b)	(*(__BYTE__ *)(b))
#define P(p)    (*(__POINTER__ *)(p))

#if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
#define X(r)	((__WORD__ *)&r)[1]
#define H(r)	((__BYTE__ *)&r)[2]
#define L(r)	((__BYTE__ *)&r)[3]
#else
#define X(r)	((__WORD__ *)&r)[0]
#define H(r)	((__BYTE__ *)&r)[1]
#define L(r)	((__BYTE__ *)&r)[0]
#endif

FILE * strupr_fopen(const char * file_name, const char * mode);

#define sES 0
#define sCS 2
#define sSS 4
#define sDS 6
#define sFS 8
#define sGS 0xa

#define __ES(base)  W(base + sES)
#define __CS(base)  W(base + sCS)
#define __SS(base)  W(base + sSS)
#define __DS(base)  W(base + sDS)
#define __FS(base)  W(base + sFS)
#define __GS(base)  W(base + sGS)

#define rEAX    0
#define rEBX    4
#define rECX    8
#define rEDX    0xc
#define rESI    0x10
#define rEDI    0x14
#define rCF     0x18

#define __EAX(base) D(base + rEAX)
#define __AX(base)  W(base + rEAX)
#define __AL(base)  B(base + rEAX)
#define __EBX(base) D(base + rEBX)
#define __BX(base)  W(base + rEBX)
#define __BL(base)  B(base + rEBX)
#define __ECX(base) D(base + rECX)
#define __CX(base)  W(base + rECX)
#define __CL(base)  B(base + rECX)
#define __EDX(base) D(base + rEDX)
#define __DX(base)  W(base + rEDX)
#define __DL(base)  B(base + rEDX)
#define __ESI(base) D(base + rESI)
#define __SI(base)  W(base + rESI)
#define __EDI(base) D(base + rEDI)
#define __DI(base)  W(base + rEDI)
#define __CF(base)  D(base + rCF)

void ___idiv32(__POINTER__ eax, __POINTER__ edx, int dv);
void ___idiv16(__POINTER__ ax, __POINTER__ dx, short dv);
void ___imul32(__POINTER__ eax, __POINTER__ edx, int mp);



typedef double  m64fp;
typedef float   m32fp;

#define F32(f)  (*(m32fp *)(f))
#define F64(f)  (*(m64fp *)(f))

#define ST(n)       (*fpu_st(n))
#define FPUSH(d)    fpu_push((double)(d))
#define FPOP        fpu_pop
double * fpu_st(int);
void fpu_push(double);
double fpu_pop(void);

#define dR_PI   3.141592653589793238462643383279502884197169399375105820974

double create_double(int LSB, int, int, int, int, int, int, int MSB);

#include "types.h"


void incCounter(int n);
void decCounter(int n);
__DWORD__ getCounter(int n);
void resetCounter(int n);
void setCounter(int n, __DWORD__ val);


#define read_b(p)       (*(__BYTE__*)(p))
#define write_b(p,b)    (*(__BYTE__*)(p) = (b))   
void switch_b(__POINTER__, __POINTER__);






#define ___24e4d0h          CONNECTION_TYPE
#define ___185a1ch          ChatMaximized
#define ___632c4h           swap16
#define ___199fbch          MP_ERROR
#define ___243cd0h          SUPERGLOBAL___243cd0h
#define ___243898h          SUPERGLOBAL___243898h
#define ___243894h          SUPERGLOBAL___243894h
#define ___243cach          SUPERGLOBAL___243cach
#define ___243ce8h          MY_CAR_IDX
#define ___243cf4h          NUM_OF_CARS
#define ___243d28h          TRX_WIDTH
#define ___243d30h          TRX_WIDTH_QTR
#define ___243d04h          TRX_HEIGHT_QTR
#define ___243d2ch          TRX_HEIGHT
#define ___243d58h          TRX_IMA
#define ___243d60h          TRX_MAS
#define ___243d78h          TRX_VAI
#define ___243d54h          TRX_LR1
#define ___240a48h          VARJO_TAB
#define ___243078h          TRX_SKI_TAB
#define ___242d78h          TRX_BLO_TAB
#define ___241c78h          TRX_LIT_TAB
#define ___243dd0h          TRX_BPA
#define ___242578h          TRX_DRV_DAT
#define ___242178h          TRX_OHI_DAT
#define ___243d8ch          RACE_MINES1A_BPK
#define ___243d7ch          RACE_BLOWI_BPK
#define ___243298h          RACE_FLAMEX_BPK
#define ___243d88h          RACE_SHOTS_BPK
#define ___243d38h          RACE_OBSTACLE_BPK
#define ___243d40h          RACE_PEDESTR_BPK
#define ___243288h          RACE_SPLATX_BPK
#define ___243290h          RACE_ROCKETX_BPK
#define ___243d68h          RACE_BURN1A_BPK
#define ___243d64h          RACE_SMOKE_BPK
#define ___243d50h          GEN_FLA_BPK
#define ___243d6ch          GEN_LAM_BPK
#define ___243d4ch          GEN_MES_BPK
#define ___243d70h          CPY_GEN_MES_BPK
#define ___243d48h          DAMSLID_BPK
#define ___243d84h          RASTI1_BPK
#define ___243d3ch          GOALNUM2_BPK
#define ___243d34h          SIDEBOM1_BPK
#define ___1de920h          X___1de920h
#define ___1ded20h          Y___1ded20h
#define ___196dc8h          TRX_VIEWPORT_TL_X
#define ___196dcch          TRX_VIEWPORT_TL_Y
#define ___243c6ch          X1___243c6ch
#define ___243c68h          X2___243c68h
#define ___243c64h          X3___243c64h
#define ___243c78h          Y1___243c78h
#define ___243c70h          Y3___243c70h
#define ___243c74h          Y2___243c74h
#define ___196d8ch          CURRENT_VIEWPORT_CENTER_X
#define ___196d94h          CURRENT_VIEWPORT_CENTER_Y
#define ___196d98h          CURRENT_VIEWPORT_X
#define ___243c94h          NUM_OF_TEXTURES
#define ___243c88h          NUM_OF_OBJECTS
#define ___196d88h          CURRENT_VIEWPORT_W
#define ___196d90h          CURRENT_VIEWPORT_H
#define ___243c8ch          X___243c8ch
#define ___243c90h          Y___243c90h
#define ___243cc0h          X_Inc
#define ___243cc4h          Y_Inc
#define ___243cc8h          X_IncOld
#define ___243ccch          Y_IncOld
#define ___243d80h          BACKBUFFER
#define ___1a1094h          LAP_PREVIOUS_MIN
#define ___1a109ch          LAP_PREVIOUS_SEC
#define ___1a102ch          LAP_PREVIOUS_100
#define ___1a1034h          LAP_BEST_MIN
#define ___1a108ch          LAP_BEST_SEC
#define ___1a0ff4h          LAP_BEST_100
#define ___2438bch          LAP_RECORD_MIN
#define ___2438b8h          LAP_RECORD_SEC
#define ___2438c0h          LAP_RECORD_100
#define ___1a1134h          NUM_OF_LAPS
#define ___185a54h          MP_MONEY

#endif // __DRALLY_H
