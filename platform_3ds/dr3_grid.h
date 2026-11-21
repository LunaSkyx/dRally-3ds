/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_grid.h - where the extra cars of the chaos build stand (DR3_CHAOS).
 *
 * A track file brings four start slots and nothing more (race___49bd4h.c: TRn-INF.BIN), and the
 * engine's whole race is built around four cars.  A race with more than four cars therefore has to
 * put the extras somewhere on the road itself - and doing that is what this file is for.
 *
 * "On the road" is the engine's own surface mask TRX_MAS (one byte per track pixel, low nibble =
 * surface).  The classification is the one the rest of the port uses (dr3_minimap.h): 0xf is the hard
 * surface, 0x0..0x3 is the soft ground next to it, everything else is scenery or a wall.  What the
 * collision code tests and what the old placement looked at differ exactly there: the old placement
 * searched for (mask & 0xf) < 4, which is the SOFT GROUND - that is why the extra cars stood in the
 * grass (and, where the ground ends, outside the track) instead of on it.
 *
 * This placement walks the road instead of scanning for a free cell:
 *
 *   * the four starters give the grid: the facing all four share (start rotation, Direction =
 *     DR3_GRID_ROT_DEG * rot) says which way the grid points, and the projection of the four
 *     positions on that direction says which starter is the rear-most and which is the front one;
 *   * from the rear-most starter a walk steps DR3_GRID_STEP px at a time along the middle of the
 *     road, following it (the heading may turn up to DR3_GRID_TURN degrees per step, and a step is
 *     only taken where the road continues - so the walk goes around a corner instead of into a wall);
 *   * every DR3_GRID_GAP px the walk is asked for a spot: the car body (DR3_GRID_HALF_LEN x
 *     DR3_GRID_HALF_WID box) has to be on the road and DR3_GRID_CLEAR away from every car placed
 *     before it.  A spot that is not clear is not used - the walk simply goes on, so the next car
 *     stands further back instead of on top of another one;
 *   * when the road behind the grid runs out (the walk hits a real dead end), a second walk starts
 *     in front of the front-most starter and continues there.
 *
 * The result is a column of cars along the road - behind the grid if the track gives the room, in
 * front of it if it does not - and every one of them stands on the hard surface.
 *
 * Plain C without any platform header, so the walk is unit-tested on the host (tests/test_dr3.c).
 */
#ifndef DR3_GRID_H
#define DR3_GRID_H

#include <stdint.h>

/* which side of the grid a spot is on - the log names it */
#define DR3_GRID_BEHIND 0       /* behind the rear-most starter */
#define DR3_GRID_AHEAD  1       /* in front of the front-most starter */

/* the surface value the road is made of (low nibble of TRX_MAS) */
#define DR3_GRID_ROAD   0x0f

/* Direction = DR3_GRID_ROT_DEG * ImgIndex - the engine's own factor (race___4a25ch.c) */
#define DR3_GRID_ROT_DEG 3.75

#define DR3_GRID_STEP     6      /* px the walk moves per step                                */
#define DR3_GRID_TURN     90     /* deg the heading may follow the road per step              */
#define DR3_GRID_TURN_COST 1     /* px of room a degree of turning has to be worth (dr3_grid.c) */
#define DR3_GRID_AXIS_TURN 40    /* deg the road's axis may differ from the way the march is
                                    travelling: a road runs *on* from where it came from, and asked
                                    for any direction the axis was found *across* the road at a kerb
                                    (more room to the side than ahead) - the march turned sideways  */
#define DR3_GRID_FACE_TURN 90    /* deg a car may be turned from the starters' direction when it is
                                    placed: wide enough for a queue that runs around a bend, and
                                    narrow enough that a car can never come out facing backwards  */
#define DR3_GRID_FACE_REACH 400  /* px the road is measured along a candidate direction when a car's
                                    facing is worked out: long enough that the road's own line always
                                    carries more of it than a diagonal that leaves through a corner */
/*

 * How far two cars have to stay apart, so that the engine never lets them touch (race___4dcach.c):
 *
 *   * the engine's own contact test only even looks at a pair whose distance is under 40 px in *both*
 *     axes (it then compares the pixels of the two sprites).  Outside that box the two cars simply
 *     cannot collide, whatever they look like - that is DR3_GRID_CONTACT;
 *   * inside it, the four start slots of the original tracks are the yardstick: they stand 34 px apart
 *     across the road and 18 px apart along it (TR6: (610,311) (644,293) (628,264) (662,246) - 38.5 px
 *     between neighbours, dx 34 / dy 18), and the original game starts and races with exactly that, so
 *     the engine tolerates it.  DR3_GRID_LANE and DR3_GRID_STAGGER are both 2 px above that;
 *   * along the road a car keeps a whole row: the original's rows are 47 px apart (slot (610,311) to
 *     (628,264)), DR3_GRID_CLEAR is 48.
 *
 * A grid whose every pair satisfies this stands as safe as the original four - and the log called the
 * earlier 28 px side by side out: two cars abreast, no stagger, 28 px apart, is inside the box on a
 * diagonal road (19.8 px per axis) and the sprites overlap.
 */
#define DR3_GRID_CONTACT  40     /* px of the engine's own car-against-car box                */
#define DR3_GRID_LANE     36     /* px apart *across* the road when standing beside another   */
#define DR3_GRID_STAGGER  20     /* px apart *along* the road when standing beside another    */
#define DR3_GRID_CLEAR    48     /* px apart along the road when standing behind another      */
#define DR3_GRID_SLACK   0.5     /* px of tolerance on those distances: the spots are handed
                                    out and rounded to whole pixels, and a pair sitting exactly on
                                    the limit comes out a hair below it after the trigonometry      */
#define DR3_GRID_GAP      52     /* px between two cars of the extra grid                     */
#define DR3_GRID_FIRST    46     /* px the first extra stands from the grid                   */
#define DR3_GRID_SIDE     30     /* px of road that has to stay ahead (else: dead end)        */
#define DR3_GRID_REACH    90     /* how far a step looks along and across the road            */
#define DR3_GRID_LIMIT   3000    /* px a march may travel.  The field is laid out along the road,
                                    so twenty cars need about 20 * 50 px; a march that comes back
                                    to where it started is the other guard (dr3_grid.c)            */
#define DR3_GRID_HALF_LEN 16     /* the car body that has to fit: 32 x 26 px around its centre*/
#define DR3_GRID_HALF_WID 13
#define DR3_GRID_CARS     40     /* the most cars this placement can hand out: four starters plus
                                    the extras (the chaos build asks for sixteen, a test build up to
                                    thirty - the field has to fit in these arrays)              */

/* the track the grid is placed on */
typedef struct {
    const uint8_t *mask;         /* TRX_MAS, one byte per track pixel                         */
    int            mask_w;
    int            mask_h;
    int            rot;          /* the start rotation (ImgIndex) the four slots share        */
} dr3_grid_t;

typedef struct {
    float x, y;                  /* position in track pixels                                  */
} dr3_grid_car_t;

typedef struct {
    float x, y;                  /* where the car stands                                      */
    /*
     * The angle the car has to face there, in the engine's own "Direction" unit (degrees): the walk
     * follows the road, so a car standing around a corner gets the facing of the road *there* and not
     * the one of the start line.  The engine's Direction points away from the car's nose - the slots
     * drive off towards Direction + 180 (race___4c434h.c) - and dr3_grid_spot_dir() below hands out
     * exactly the value the slots of the same spot carry, so the extras stand the way the starters do.
     */
    float dir;
    int   side;                  /* DR3_GRID_BEHIND / DR3_GRID_AHEAD                          */
    int   lane;                  /* 0 = the centre of the road, 1 = the second lane beside it  */
    int   dist;                  /* px the walk travelled for it (0 when there was no spot)    */
} dr3_grid_spot_t;

/*
 * The sprite frame (ImgIndex, 0..95) for one of the engine's directions: 96 frames of 3.75 degrees,
 * and Direction = 3.75 * ImgIndex (race___4a25ch.c).  Sets the facing of a car that the walk placed:
 * ImgOffset = 0x640 * ImgIndex + 0x25800 * car (race___4a25ch.c, race___4af3ch.c).
 */
int dr3_grid_img_index(float dir);

/*
 * Places up to max_cars cars around the grid of grid_n starters (their positions are the ones the
 * track file set, so at least the four the engine uses).  out[] receives the spots that were found,
 * in placement order, and the return value is how many there are.  Fewer than max_cars means the road
 * ran out on both ends of the grid - the caller leaves the rest of the cars on the slots they share,
 * which is the position the engine itself considers valid.  Nothing is written when the track has no
 * mask, no grid or no road at all (return 0).
 */
int dr3_grid_place(const dr3_grid_t *track, const dr3_grid_car_t *grid, int grid_n,
                   dr3_grid_spot_t *out, int max_cars);

/*
 * May a car stand at (x,y) facing `dir` without touching `car`?  This is the rule the placement uses
 * for every pair of cars (and the host tests check the finished grid with it), and it is made to be at
 * least as friendly as the engine's own four start slots - see the block above DR3_GRID_CONTACT.
 * `dir` is the car's own Direction (the heading of the walk), because the distance that counts is the
 * one *along and across the car*, not the one along the axes of the track.
 */
int dr3_grid_pair_ok(double x, double y, double dir, const dr3_grid_spot_t *car);

/*
 * Where the last walk ended when it gave up (track pixels and its heading in degrees).  A placement
 * that comes up short can be looked at exactly where it stopped - the host probe draws the road there.
 */
void dr3_grid_last_walk(double *x, double *y, double *dir);

int dr3_grid_axis_at(const dr3_grid_t *track, double x, double y, double heading, double *dir);

/*
 * The line of the road where a car stands, in full (no cone): `dir` receives the direction, oriented the
 * way the four starters face for a car whose start direction is `heading`, and the return value is the
 * room the road has along it.  This is what a placed car is aligned with - the host probe and the tests
 * compare every spot against it (dr3_grid.c).
 */
int dr3_grid_road_line_at(const dr3_grid_t *track, double x, double y, double heading, double *dir);

/*
 * What the last march did: 0 = steps taken, 1 = spots asked for, 2 = times the car body did not fit,
 * 3 = times the clearance rule said no.  The host probe prints them when a placement comes up short.
 */
int dr3_grid_dbg_count(int which);

/*
 * The path the last march took: `i` counts from 0 (where the march started) upwards, up to a few dozen
 * steps.  The host probe draws it, so a march that ends somewhere odd can be looked at.
 */
int dr3_grid_dbg_path_at(int i, int *x, int *y);

/* -1 when the car body fits at (x,y) facing `heading`, else the corner that is off the road (probe) */
int dr3_grid_fits_at(const dr3_grid_t *track, double x, double y, double heading);

/*
 * Why the last placement stopped short of the cars it was asked for.  The caller logs it: on Oasis the
 * walk stopped after four cars and the log had to say whether the road or the car body was the limit.
 */
#define DR3_GRID_NO_ROAD      0    /* the walk found no direction with road ahead of it */
#define DR3_GRID_NO_ROOM      1    /* it walked on, but the car body never fitted       */
#define DR3_GRID_BENT_BACK    2    /* the road bent back over the start line            */
int dr3_grid_last_reason(void);

/* a car without a standings position (rank 0) sorts behind every car that has one */
#define DR3_GRID_RANK_NONE 0x1000

/*
 * The order the cars of a race stand in: order[0] is the car with the best standings position (the
 * leader), order[1] the second and so on, so the grid can be handed out in that order and the field
 * starts the way the standings read.  `rank` is the standings position per car (1 = leader, 0 = none
 * yet); ties and cars without a rank keep the order they came in with, so the result is stable.  The
 * caller passes cars <= DR3_GRID_CARS entries.
 */
void dr3_grid_order_by_rank(const int *rank, int cars, int *order);

/*
 * Chaos Adversary invader helpers (portable, unit-tested on host without engine globals):
 *   - DR3_CHAOS_BOSS_CHANCE_DEFAULT: default percent chance (25%) per eligible Chaos race.
 *   - dr3_chaos_boss_should_join: returns 1 when the roll wins in an eligible Chaos mode (STANDARD, DM,
 *     ARENA, or RANDOM running as a standard race; never DUEL).
 *   - dr3_chaos_boss_pick_car: picks which car slot (0..field_n-1) the Adversary takes - the last slot
 *     of the field when it is not the player's car (so the race's own 0..NumCars-1 stay untouched when
 *     extra cars are present), otherwise the highest non-player slot, or -1 if the field has < 2 cars.
 *   - dr3_grid_order_with_leader: orders by standings rank like dr3_grid_order_by_rank, then promotes
 *     `leader_car` (when 0 <= leader_car < cars) to order[0] (pole position / front of the pack) while
 *     keeping every other car in its exact relative order behind it.
 */
#define DR3_CHAOS_BOSS_CHANCE_DEFAULT 25

int  dr3_chaos_boss_should_join(int roll_0_99, int mode, int chance_pct);
int  dr3_chaos_boss_pick_car(int field_n, int num_cars, int player_car);
void dr3_grid_order_with_leader(const int *rank, int cars, int leader_car, int *order);

/*
 * Verify & align a placed car's facing `dir` (engine Direction degrees; nose = dir + 180) against
 * the track's quarter-res checkpoint map (`vai`, wq x hq, `nchk` total checkpoints).  If checkpoints
 * decrease along the car's nose (facing wrong way), flips `*dir` by +180 degrees and returns 1.
 */
int  dr3_grid_align_vai(const uint8_t *vai, int wq, int hq, int nchk,
                        float x, float y, float *dir);

#endif /* DR3_GRID_H */
