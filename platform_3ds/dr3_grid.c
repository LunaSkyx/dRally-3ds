/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_grid.h"

#include <math.h>

/* why the last walk stopped (dr3_grid.h) - the caller logs it, so a short placement names its limit */
static int dr3_grid_why = DR3_GRID_NO_ROAD;

/*
 * The last-resort pass: the strict rule (dr3_grid_pair_ok) is what keeps a grid collision free, but on
 * a start stretch that is barely wider than a car (Oasis: 78 px) there is simply not enough room for
 * twenty cars at that distance.  When every strict walk has run out, this flag relaxes the rule to
 * DR3_GRID_TIGHT px - every car still gets its *own* place on the road and stands along it, instead of
 * the rest of the field piling up on one start slot.  A car touching its neighbour is nothing next to
 * six cars standing on the same spot.
 */
#define DR3_GRID_TIGHT    34
static int dr3_grid_tight = 0;

/* ------------------------------------------------------------------- the road --- */

static int dr3_grid_road(const dr3_grid_t *t, int x, int y)
{
    if ((x < 0) || (y < 0) || (x >= t->mask_w) || (y >= t->mask_h)) return 0;

    return (int)(t->mask[(size_t)x + (size_t)t->mask_w * (size_t)y] & 0x0f) == DR3_GRID_ROAD;
}

/*
 * How far the road reaches from (x,y) along (dx,dy), at most max px.  The surface is a pixel map, so
 * the line is walked in whole pixel steps.
 */
static int dr3_grid_reach(const dr3_grid_t *t, int x, int y, double dx, double dy, int max)
{
    double px = (double)x, py = (double)y;
    int    n = 0;

    while (n < max) {

        px += dx; py += dy;

        if (!dr3_grid_road(t, (int)(px + 0.5), (int)(py + 0.5))) break;
        ++n;
    }

    return n;
}

/*
 * The world vector of one of the engine's angles.  The engine keeps its own angle in "Direction" and
 * moves the cars with (sin, cos * 5/6) of it (race___4c434h.c), so a direction in the mask has to be
 * converted the same way - otherwise the walk would drift sideways on a diagonal road.
 */
static void dr3_grid_vector(double a, double *dx, double *dy)
{
    const double rad = a * 3.14159265358979 / 180.0;
    const double x   = sin(rad), y = cos(rad) * 5.0 / 6.0;
    const double len = sqrt(x * x + y * y);

    *dx = x / len;
    *dy = y / len;
}

static double dr3_grid_norm360(double a)
{
    while (a < 0.0)    a += 360.0;
    while (a >= 360.0) a -= 360.0;

    return a;
}

/*
 * Does the car body fit at (x,y)?  Centre and the four corners of the DR3_GRID_HALF_LEN x
 * DR3_GRID_HALF_WID box, turned into the car's own direction - the sprite is 40x40, the body inside
 * it is smaller, and the road of the narrowest track is about 56 px wide.
 */
static int dr3_grid_fits(const dr3_grid_t *t, int x, int y, double nx, double ny)
{
    static const int corner[5][2] = { {0, 0}, {-1, -1}, {1, -1}, {1, 1}, {-1, 1} };
    const double     sx = -ny, sy = nx;
    int              i;

    for (i = 0; i < 5; ++i) {

        const double dx = (double)(corner[i][0] * DR3_GRID_HALF_LEN) * nx +
                          (double)(corner[i][1] * DR3_GRID_HALF_WID) * sx;
        const double dy = (double)(corner[i][0] * DR3_GRID_HALF_LEN) * ny +
                          (double)(corner[i][1] * DR3_GRID_HALF_WID) * sy;

        if (!dr3_grid_road(t, (int)((double)x + dx + 0.5), (int)((double)y + dy + 0.5))) return 0;
    }

    return 1;
}

/* where the last walk of dr3_grid_place() stood when it gave up - the log and the host probe read it */
static double dr3_grid_last_x, dr3_grid_last_y, dr3_grid_last_dir;

/* counters for the host probe: what a march did before it stopped (logs/grid_probe.c) */
static int dr3_grid_dbg[8];             /* 0 steps, 1 spots asked for, 2 last-fit failures,
                                           3 last-clearance rejections, 4 advance: little road ahead,
                                           5 advance: step off the road, 6 fits said no (total),
                                           7 clearance said no (total)                            */

/* the path the last march took (whole pixels, oldest first) - the host probe draws it */
#define DR3_GRID_DBG_PATH 64
static short dr3_grid_dbg_path[DR3_GRID_DBG_PATH][2];
static int   dr3_grid_dbg_path_n;

int dr3_grid_dbg_count(int which)
{
    if ((which < 0) || (which > 7)) return 0;

    return dr3_grid_dbg[which];
}

int dr3_grid_dbg_path_at(int i, int *x, int *y)
{
    if ((i < 0) || (i >= dr3_grid_dbg_path_n)) return 0;

    if (x) *x = (int)dr3_grid_dbg_path[i][0];
    if (y) *y = (int)dr3_grid_dbg_path[i][1];

    return 1;
}

void dr3_grid_last_walk(double *x, double *y, double *dir)
{
    if (x)   *x   = dr3_grid_last_x;
    if (y)   *y   = dr3_grid_last_y;
    if (dir) *dir = dr3_grid_last_dir;
}

/* ------------------------------------------------------------------- the walk --- */

/*
 * One walk step: the point moves DR3_GRID_STEP px along the road, the heading may follow the road by
 * DR3_GRID_TURN degrees, and the point is pulled towards the middle of the corridor (so a column of
 * cars keeps to the middle of the road on a wide one).
 *
 * What counts is the road the NEW point offers, not the one the current point offers: at a right
 * angle corner the road straight ahead is gone within one step, and only the look from the new point
 * shows the corridor that continues around the corner.  Returns 0 when the road really ends.
 */
static int dr3_grid_step(const dr3_grid_t *t, double *px, double *py, double *ph, int step)
{
    double best = 1.0e30, bx = *px, by = *py, bh = *ph;
    int    d, ok = 0;

    /*
     * Which way the march goes on: of the directions the heading may take (DR3_GRID_TURN degrees to
     * either side) the one promising the most road wins, and turning costs one px of that room per
     * degree - so a bend moves the march, a two pixel wider spot does not.  It is asked at the *new*
     * point, because at a right angle corner the road directly ahead is gone within one step.
     */
    for (d = -DR3_GRID_TURN; d <= DR3_GRID_TURN; d += 5) {

        double       dx, dy;
        int          x1, y1, fwd, back;
        double       score;
        const double a = dr3_grid_norm360(*ph + (double)d);

        dr3_grid_vector(a, &dx, &dy);

        x1 = (int)(*px + (double)step * dx + 0.5);
        y1 = (int)(*py + (double)step * dy + 0.5);

        if (!dr3_grid_road(t, x1, y1)) continue;

        fwd  = dr3_grid_reach(t, x1, y1, dx, dy, DR3_GRID_REACH);
        back = dr3_grid_reach(t, x1, y1, -dx, -dy, DR3_GRID_REACH);

        if (fwd < DR3_GRID_SIDE) continue;

        /*
         * A turn costs DR3_GRID_TURN_COST px of room per degree - three times what a direction can gain
         * from a corner: at the end of a stretch a diagonal reaches further than the road itself and the
         * march drifted into it (the same artefact that made cars face 20 degrees off the road).  A real
         * bend still moves the march, because there the road directly ahead is gone within a step.
         */
        score = (double)DR3_GRID_TURN_COST * (double)((d < 0) ? -d : d)
              - (double)((fwd < back) ? fwd : back);

        if (score < best) {

            best = score;
            bx = (double)x1;
            by = (double)y1;
            bh = a;
            ok = 1;
        }
    }

    if (!ok) return 0;

    /*
     * Pull the point towards the middle of the corridor it landed in, so a column of cars keeps to the
     * middle of the road.  The shift is tried in halving steps and only taken where the road continues:
     * a point near the kerb has an asymmetric corridor and must not be pushed off the road.
     */
    {
        double dx, dy;
        int    left, right, shift, s;

        dr3_grid_vector(bh, &dx, &dy);

        left  = dr3_grid_reach(t, (int)(bx + 0.5), (int)(by + 0.5), -dy,  dx, 40);
        right = dr3_grid_reach(t, (int)(bx + 0.5), (int)(by + 0.5),  dy, -dx, 40);
        shift = (left - right) / 2;

        for (s = shift; s != 0; s = (s > 1) ? (s / 2) : ((s < -1) ? (s / 2) : 0)) {

            const double sx = -dy, sy = dx;
            const double tx = bx + (double)s * sx;
            const double ty = by + (double)s * sy;

            if (dr3_grid_road(t, (int)(tx + 0.5), (int)(ty + 0.5))) { bx = tx; by = ty; break; }
        }
    }

    *px = bx;
    *py = by;
    *ph = bh;

    return 1;
}

/*
 * The axis of the road at (x,y) - the direction the track runs there.  It is measured, not guessed: of
 * all the directions that are a *continuation* of the way we are travelling (DR3_GRID_AXIS_TURN degrees
 * to either side is as much as a car can turn in one DR3_GRID_STEP px step) the one with the most road
 * on both sides of it wins, because that is the direction a car standing here drives off in.
 *
 * The band matters: asked for *any* direction, the measurement picked the one across the road where a
 * walk had drifted towards the kerb - the road to the side is wider there than the road ahead - and the
 * march turned sideways off the track.  A road only ever runs on from where it came from.
 *
 * `dir` receives that axis, and the return value is the room the road gives along it (the shorter of the
 * two runs) - the caller walks on in that direction and stands its cars along it.
 */
static int dr3_grid_axis(const dr3_grid_t *t, double x, double y, double near, double *dir)
{
    const int xi = (int)(x + 0.5), yi = (int)(y + 0.5);
    int       d, room_best = -1;
    double    dir_best = dr3_grid_norm360(near), score_best = 1.0e30;
    double    dir_fwd = dir_best, score_fwd = 1.0e30;
    int       room_fwd = -1;

    for (d = -DR3_GRID_AXIS_TURN; d <= DR3_GRID_AXIS_TURN; d += 2) {

        double       dx, dy;
        int          fwd, back, room, turn;
        double       score;
        const double a = dr3_grid_norm360(near + (double)d);

        dr3_grid_vector(a, &dx, &dy);

        fwd  = dr3_grid_reach(t, xi, yi,  dx,  dy, DR3_GRID_REACH);
        back = dr3_grid_reach(t, xi, yi, -dx, -dy, DR3_GRID_REACH);

        room = (fwd < back) ? fwd : back;
        turn = (d < 0) ? -d : d;

        /*
         * The road's own axis: the direction with the most road on *both* sides of it is the way the
         * track runs, and a turn costs one px of that room per degree - so a wide spot two degrees off
         * does not swing the march, and a bend does move it.  Weighing raw room instead (turning only
         * as a tie break) let the axis jump a couple of degrees every step and the march zigzagged on
         * wide parts of the track without ever getting anywhere (host probe).
         */
        score = (double)turn - (double)room;

        if (score < score_best) { score_best = score; room_best = room; dir_best = a; }

        /*
         * ... and the same for the road *ahead* alone, used where no direction has road on both sides
         * (a kerb, a junction, the stump behind a start line): the march then follows the road in front
         * of it instead of stopping there.
         */
        score = (double)turn - (double)fwd;

        if (score < score_fwd) { score_fwd = score; room_fwd = fwd; dir_fwd = a; }
    }

    if ((room_best < DR3_GRID_SIDE) && (room_fwd > room_best)) {

        dir_best  = dir_fwd;
        room_best = room_fwd;
    }

    if (room_best < 0) return 0;

    *dir = dir_best;

    return room_best;
}

/*
 * The line of the road at (x,y), measured *without* a cone: of all the directions the one with the most
 * road on both sides of it wins - that is the way the track runs at this very spot, however hard it bends
 * there.  `near` is only used to keep the answer steady (of two equally roomy lines the one closer to
 * `near` wins) and to pick the end of the line, so on a straight piece of road the answer is the value
 * the four starters carry.
 *
 * The *marching* step deliberately uses the cone above - a march has to keep its direction and not turn
 * off into a side road.  For the *facing of a car* the cone was wrong: on Oasis the queue runs around a
 * bend where the road is more than DR3_GRID_AXIS_TURN off the start direction, so every car past that
 * bend was clamped to the edge of the cone (the log showed "facing 142 deg (img 38)" where the road runs
 * the other way) and stood across the road.
 */
static int dr3_grid_axis_line(const dr3_grid_t *t, double x, double y, double near, double *dir)
{
    const int xi = (int)(x + 0.5), yi = (int)(y + 0.5);
    double    ndx, ndy, best_gap = 1.0e30, line = dr3_grid_norm360(near);
    int       a, room_best = -1;

    dr3_grid_vector(near, &ndx, &ndy);

    for (a = 0; a < 180; a += 2) {

        double dx, dy, dot, gap;
        int    fwd, back, room;

        dr3_grid_vector((double)a, &dx, &dy);

        /*
         * Note on the reach: DR3_GRID_FACE_REACH (400 px) was tried here and came out *worse* - at the
         * stub of a dead end the diagonal has more road than the road itself however far the look is,
         * and on the real tracks it moved a few cars off their line (host probe).  DR3_GRID_REACH
         * (90 px) with the tie break towards `near` is what measures every one of the ten tracks clean.
         */
        fwd  = dr3_grid_reach(t, xi, yi,  dx,  dy, DR3_GRID_REACH);
        back = dr3_grid_reach(t, xi, yi, -dx, -dy, DR3_GRID_REACH);

        room = (fwd < back) ? fwd : back;

        if (room < room_best) continue;

        dot = dx * ndx + dy * ndy;                  /* a line has two ends: either one matches `near` */
        if (dot < 0.0) dot = -dot;
        gap = 1.0 - dot;

        if ((room > room_best) || (gap < best_gap)) {

            room_best = room;
            best_gap  = gap;
            line      = (double)a;
        }
    }

    if (room_best < 0) return 0;

    /* of the two ends of the line, the one that faces the way the starters face */
    {
        double ldx, ldy;

        dr3_grid_vector(line, &ldx, &ldy);

        if ((ldx * ndx + ldy * ndy) < 0.0) line = dr3_grid_norm360(line + 180.0);
    }

    *dir = line;

    return room_best;
}

int dr3_grid_pair_ok(double x, double y, double dir, const dr3_grid_spot_t *car)
{
    double dx, dy;
    double ex, ey, along, across;

    if (!car) return 1;

    ex = (double)car->x - x;
    ey = (double)car->y - y;

    /*
     * The last-resort pass: its rule is the engine's own contact box.  A pair that is at least
     * DR3_GRID_CONTACT px apart in *one* of the track's axes is never even looked at by race___4dcach.c
     * (it tests "|dx| < 40 && |dy| < 40" first and only then compares sprite pixels), so a field placed
     * with that rule cannot collide.  It is the smallest relaxation that can be proven safe - the strict
     * rule above asks for much more and is what keeps the field looking like a grid.
     */
    if (dr3_grid_tight)
        return ((fabs(ex) >= (double)DR3_GRID_CONTACT) || (fabs(ey) >= (double)DR3_GRID_CONTACT));

    /* outside the box the engine never even looks at the pair */
    if ((fabs(ex) >= (double)DR3_GRID_CONTACT) || (fabs(ey) >= (double)DR3_GRID_CONTACT)) return 1;

    dr3_grid_vector(dir, &dx, &dy);

    along  = ex * dx + ey * dy;             /* along the car ...  */
    across = -ex * dy + ey * dx;            /* ... and across it  */

    if (along  < 0.0) along  = -along;
    if (across < 0.0) across = -across;

    if (along >= ((double)DR3_GRID_CLEAR - DR3_GRID_SLACK)) return 1;                   /* a row away */
    return (across >= ((double)DR3_GRID_LANE    - DR3_GRID_SLACK)) &&
           (along  >= ((double)DR3_GRID_STAGGER - DR3_GRID_SLACK));
}

/*
 * Marches down the road from where the walk stands, hands out a spot every `gap` px, and stops at a
 * real dead end.  The road's own axis is *measured* at every point (dr3_grid_axis), the point is kept
 * in the middle of the corridor (or at the lane offset, dr3_grid_centre) and the car that is handed out
 * is aligned to that axis - so every car stands on the road, along the road, facing the way the track
 * runs there.  A spot that is not clear of the cars placed before it (dr3_grid_pair_ok) or where the
 * body does not fit is skipped by walking on, so the next car simply stands further along.
 *
 * `nx`, `ny`, `ref_proj` and `side` are the grid's own axis and side and are no longer needed: the axis
 * is measured from the road itself, and staying on the right side comes from walking in the direction
 * the road runs (away from the grid) instead of from a projection on the grid's line.
 */
static int dr3_grid_walk_to(const dr3_grid_t *t, double *px, double *py, double *ph,
                            const dr3_grid_spot_t *cars, int cars_n, int gap, int *dist)
{
    double travelled = 0.0;
    const double sx0 = *px, sy0 = *py;  /* where the march started (to notice a way back to it) */

    dr3_grid_last_x   = *px;
    dr3_grid_last_y   = *py;
    dr3_grid_last_dir = *ph;
    dr3_grid_dbg_path_n = 0;            /* the path of *this* walk (host probe) */

    while (travelled < (double)DR3_GRID_LIMIT) {

        int i, clear = 1, stepped = 0, s;

        /*
         * A DR3_GRID_STEP px step is the normal one.  Where the road bends tighter than that (a
         * hairpin, the inside of a tight corner) no direction at that step is road any more - the walk
         * then retries with half and a quarter of it instead of giving up, so it keeps following the
         * road backwards and the field stays spread out.
         */
        for (s = DR3_GRID_STEP; s >= 2; s /= 2)
            if (dr3_grid_step(t, px, py, ph, s)) { stepped = s; break; }

        /*
         * Never flip +180 deg back down the road we just came from: reversing the march walked right
         * back past the start line on tight tracks (Utopia, Snake Alley, Oasis) and stamped cars
         * facing 180 deg backwards into oncoming traffic.  Stop cleanly so dr3_grid_place() can fall
         * back to the other lane or tighter spacing behind the grid.
         */
        if (!stepped) {

            dr3_grid_why = DR3_GRID_NO_ROAD;        /* the road really ends here */
            return 0;
        }

        ++dr3_grid_dbg[0];

        dr3_grid_last_x   = *px;
        dr3_grid_last_y   = *py;
        dr3_grid_last_dir = *ph;

        if (dr3_grid_dbg_path_n < DR3_GRID_DBG_PATH) {

            dr3_grid_dbg_path[dr3_grid_dbg_path_n][0] = (short)(int)(*px + 0.5);
            dr3_grid_dbg_path[dr3_grid_dbg_path_n][1] = (short)(int)(*py + 0.5);
            ++dr3_grid_dbg_path_n;
        }

        /*
         * How far the march may go.  It used to be a *projection* on the grid's line ("bow at most
         * DR3_GRID_ARC px forward of it") and that is what stopped the field on Oasis with the whole
         * circuit still unused: the road there bends back over that line early.  What matters is the
         * distance along the road - twenty cars need about 20 * 50 px - so the cap is the path length
         * (DR3_GRID_LIMIT), plus one real guard: a march that comes back to where it started has followed
         * the road around the grid, and a car handed out from there would stand in front of the leader.
         */
        if ((travelled > 300.0) &&
            (((*px - sx0) * (*px - sx0) + (*py - sy0) * (*py - sy0)) < (double)(DR3_GRID_CLEAR * DR3_GRID_CLEAR))) {

            dr3_grid_why = DR3_GRID_BENT_BACK;
            return 0;
        }

        travelled += (double)stepped;
        if (travelled < (double)gap) continue;

        ++dr3_grid_dbg[1];                       /* a spot is being asked for here */
        dr3_grid_dbg[2] = 0;
        dr3_grid_dbg[3] = 0;

        {
            double axis = *ph, adx, ady;
            int    room, k;

            /*
             * The car is aligned with the *measured line* of the road where it stands - the line with the
             * most road on both sides of it, no cone (dr3_grid_axis_line).  It is oriented the way the
             * march is travelling: the march follows the road away from the start line, so its heading
             * says which end of the line is the one the race drives.  The march heading alone is only an
             * average over one DR3_GRID_STEP px step and stands up to DR3_GRID_TURN degrees across the road
             * at a hairpin apex (host probe: 68 deg); the measured line is the track itself.
             */
            {
                double hdx, hdy;

                /*
                 * The measured line, and only that: every attempt to second-guess it with the march
                 * heading was measured and lost (see the note in dr3_grid_axis_line).  Where the
                 * measurement finds nothing the march heading is the answer.
                 */
                if (!dr3_grid_axis_line(t, *px, *py, *ph, &axis)) axis = dr3_grid_norm360(*ph);

                dr3_grid_vector(axis, &hdx, &hdy);

                room = dr3_grid_reach(t, (int)(*px + 0.5), (int)(*py + 0.5), hdx, hdy, DR3_GRID_REACH);

                adx = hdx; ady = hdy;
            }

            /*
             * The clearance rule (dr3_grid_pair_ok) is asked about the car the way it will actually
             * stand - the rounded position and this axis.  Measuring along and across the *car* instead
             * of along the axes of the track is what keeps two cars of a grid side by side from touching
             * the engine's contact box.  Asking with another heading than the one that is stamped is
             * what let a colliding pair through (host probe: TR1, TR4, TR9).
             */
            for (k = 0; k < cars_n; ++k) {

                if (!dr3_grid_pair_ok((double)(int)(*px + 0.5), (double)(int)(*py + 0.5), axis, &cars[k])) {

                    clear = 0;
                    ++dr3_grid_dbg[3];
                    ++dr3_grid_dbg[7];
                    break;
                }
            }

            if (clear && (room >= DR3_GRID_SIDE) &&
                dr3_grid_fits(t, (int)(*px + 0.5), (int)(*py + 0.5), adx, ady)) {

                *ph = axis;

                if (dist) *dist = (int)travelled;
                return 1;
            }

            ++dr3_grid_dbg[2];
            ++dr3_grid_dbg[6];
        }
    }

    dr3_grid_why = DR3_GRID_NO_ROOM;            /* it walked the whole limit and never fitted */
    return 0;
}

/* ------------------------------------------------------------------ the grid --- */

/*
 * A starter that stands on the soft ground is pulled onto the nearest road pixel (a few px at most):
 * the walk has to start on the road, and a track whose slot is a little off is still placed properly.
 */
static void dr3_grid_snap(const dr3_grid_t *t, double *px, double *py)
{
    const int x0 = (int)(*px + 0.5), y0 = (int)(*py + 0.5);
    int       r;

    if (dr3_grid_road(t, x0, y0)) return;

    for (r = 1; r <= 12; ++r) {

        int dx, dy;

        for (dy = -r; dy <= r; ++dy) {

            for (dx = -r; dx <= r; ++dx) {

                if ((dx > -r) && (dx < r) && (dy > -r) && (dy < r)) continue;   /* only the ring */

                if (dr3_grid_road(t, x0 + dx, y0 + dy)) {

                    *px = (double)(x0 + dx);
                    *py = (double)(y0 + dy);
                    return;
                }
            }
        }
    }
}

int dr3_grid_place(const dr3_grid_t *track, const dr3_grid_car_t *grid, int grid_n,
                   dr3_grid_spot_t *out, int max_cars)
{
    dr3_grid_spot_t cars[DR3_GRID_CARS];        /* the grid plus everything placed so far */
    double          nose[2];
    double          nose_a;
    double          bx = 0.0, by = 0.0, bh = 0.0;    /* set below: dr3_grid_snap() and the heading */
    double          rear = 1.0e30;
    /*
     * The facing comes from the march, not from the queue: the march follows the road, so its heading is
     * the way the road runs where the car stands, and it always travels against the race - it walks the lap
     * backwards from the start line.  Taking the facing from the queue instead (pointing each car at the car
     * in front) read the lane reference across a hairpin and turned half the field by 180 degrees.
     */
    int             n, cars_n, placed = 0;

    if (!track || !track->mask || !grid || !out) return 0;
    if ((grid_n <= 0) || (max_cars <= 0)) return 0;

    dr3_grid_tight = 0;
    if (grid_n > DR3_GRID_CARS) grid_n = DR3_GRID_CARS;


    if ((grid_n + max_cars) > DR3_GRID_CARS) max_cars = DR3_GRID_CARS - grid_n;

    /* the direction the grid points at (the nose is Direction + 180 in the engine's own terms) */
    nose_a = dr3_grid_norm360(DR3_GRID_ROT_DEG * (double)track->rot + 180.0);
    dr3_grid_vector(nose_a, &nose[0], &nose[1]);

    bx = by = 0.0;

    for (n = 0; n < grid_n; ++n) {

        const double proj = (double)grid[n].x * nose[0] + (double)grid[n].y * nose[1];

        cars[n].x    = grid[n].x;
        cars[n].y    = grid[n].y;
        cars[n].dir  = (float)(DR3_GRID_ROT_DEG * (double)track->rot);   /* the slot's own facing */
        cars[n].side = DR3_GRID_BEHIND;
        cars[n].lane = 0;
        cars[n].dist = 0;

        if (proj < rear)  { rear = proj; bx = (double)grid[n].x; by = (double)grid[n].y; }
    }

    cars_n = grid_n;

    dr3_grid_snap(track, &bx, &by);             /* from the rear-most starter, walking back */

    bh = dr3_grid_norm360(nose_a + 180.0);      /* away from the grid, to the back */

    /*
     * NOTE (found with the PC harness, platform_3ds/dr3_host.c):
     *
     * There used to be a block right here that compared the road ahead of and behind the grid and turned
     * the march direction by 180 degrees if it pointed against the road.  It did not work: it read bx, by
     * and bh before dr3_grid_snap() and this line had set them (MSVC reports C4700, GCC with -O2 says
     * nothing), and it only ever wrote bh - the variable set right above, one line before the walk uses
     * it.  So the direction the walk actually used was always nose_a + 180, and the block was dead code
     * with undefined behaviour.
     *
     * It is gone instead of moved: putting it below changes where every placed car stands (the host tests
     * "chaos grid: the road turns a corner" then fail), so it was not what the grid does.  If the "does
     * the way to the back really have road?" check is wanted, it has to be moved down here *and* the
     * expectation of those tests has to be looked at again.
     */

    /*
     * Two lanes, filled row by row: the same march down the road, once along the middle of it and once
     * DR3_GRID_LANE px beside the middle.  A single column ran out of road on the tracks whose start
     * stretch is short - on "Toxic Dump" several cars ended up on the same start slot.  Because the lanes
     * alternate, the order of the standings holds: the best rank in front, the next one beside it, then
     * the row behind - a real grid, and never a car in front of the leader.
     */
    {
        double lx = bx, ly = by, lh = bh;       /* the second lane */
        int    lane2 = 0;                       /* which side it runs on, 0 = none */

        {
            double dx, dy;

            dr3_grid_vector(bh, &dx, &dy);

            lx = bx - (double)DR3_GRID_LANE * dy;      /* across the heading ... */
            ly = by + (double)DR3_GRID_LANE * dx;

            /* the whole car body has to fit there, not just the point - the road edge is one pixel of
               road but no second lane */
            if (dr3_grid_fits(track, (int)(lx + 0.5), (int)(ly + 0.5), dx, dy))
                lane2 = 1;
            else {

                lx = bx + (double)DR3_GRID_LANE * dy;  /* ... and the other way round */
                ly = by - (double)DR3_GRID_LANE * dx;

                if (dr3_grid_fits(track, (int)(lx + 0.5), (int)(ly + 0.5), dx, dy)) lane2 = -1;
                else { lx = bx; ly = by; }             /* no room for a second lane */
            }
        }

        for (n = 0; n < max_cars; ++n) {

            const int    gap = (n == 0) ? DR3_GRID_FIRST : DR3_GRID_GAP;
            const int    want_lane = (n & 1) && lane2;
            double *     px  = want_lane ? &lx : &bx;
            double *     py  = want_lane ? &ly : &by;
            double *     ph  = want_lane ? &lh : &bh;
            int          dist = 0;

            if (!dr3_grid_walk_to(track, px, py, ph, cars, cars_n, gap, &dist)) {

                /*
                 * That lane is out of road.  The other one may still have room, and then the same march
                 * with the tighter spacing - but *never* the road in front of the grid: the field is a
                 * queue behind the start line, rank 1 closest to it, and a car placed past the line would
                 * cross it before the leader (the "I am rank 20 and I started in front" report).  The
                 * march follows the road backwards around the circuit, so a whole field fits behind the
                 * line even where the stretch right behind the grid is short.
                 */
                if ((px != &bx) && dr3_grid_walk_to(track, &bx, &by, &bh, cars, cars_n, gap, &dist)) {

                    px = &bx; py = &by; ph = &bh;
                }
                else if ((px != &lx) && lane2 &&
                         dr3_grid_walk_to(track, &lx, &ly, &lh, cars, cars_n, gap, &dist)) {

                    px = &lx; py = &ly; ph = &lh;
                }
                /*
                 * Nothing fits at the strict distance.  Rather than piling the rest of the field onto one
                 * start slot, this last pass walks the same roads with DR3_GRID_TIGHT px between the cars:
                 * every one of them gets a place of its own on the road and stands along the road.
                 */
                else {

                    dr3_grid_tight = 1;

                    if (dr3_grid_walk_to(track, &bx, &by, &bh, cars, cars_n, DR3_GRID_TIGHT, &dist)) {
                        px = &bx; py = &by; ph = &bh;
                    }
                    else if (lane2 && dr3_grid_walk_to(track, &lx, &ly, &lh, cars, cars_n, DR3_GRID_TIGHT, &dist)) {
                        px = &lx; py = &ly; ph = &lh;
                    }
                    else { dr3_grid_tight = 0; break; }

                    dr3_grid_tight = 0;
                }
            }

            out[placed].side = DR3_GRID_BEHIND;
            out[placed].lane = (px == &lx) ? 1 : 0;
            out[placed].x    = (float)(int)(*px + 0.5);
            out[placed].y    = (float)(int)(*py + 0.5);

            /*
             * The car is aligned with the ideal line: the march follows the road, so its heading is the way
             * the road runs where the car stands - and it always travels against the race, because it walks
             * the lap backwards from the start line, which makes the race direction here the heading + 180.
             * The engine stores Direction the other way round from the nose as well (the starters carry
             * 3.75 * rot and their nose is that + 180), so the two flips cancel and the value to store is
             * the march heading itself.  On the start straight that is exactly the starters own value.
             */
            out[placed].dir = (float)dr3_grid_norm360(*ph);

            out[placed].dist = dist;

            cars[cars_n].x    = out[placed].x;
            cars[cars_n].y    = out[placed].y;
            cars[cars_n].dir  = out[placed].dir;
            cars[cars_n].side = out[placed].side;
            cars[cars_n].lane = out[placed].lane;
            cars[cars_n].dist = dist;
            ++cars_n;

            ++placed;
        }
    }

    return placed;
}

int dr3_grid_last_reason(void)
{
    return dr3_grid_why;
}

int dr3_grid_fits_at(const dr3_grid_t *track, double x, double y, double heading)
{
    double dx, dy;

    dr3_grid_vector(heading, &dx, &dy);

    if (dr3_grid_fits(track, (int)(x + 0.5), (int)(y + 0.5), dx, dy)) return -1;

    /* which corner is not on the road (for the host probe) */
    {
        static const int corner[5][2] = { {0, 0}, {-1, -1}, {1, -1}, {1, 1}, {-1, 1} };
        const double     sx = -dy, sy = dx;
        int              i;

        for (i = 0; i < 5; ++i) {

            const double cx = (double)(corner[i][0] * DR3_GRID_HALF_LEN) * dx +
                              (double)(corner[i][1] * DR3_GRID_HALF_WID) * sx;
            const double cy = (double)(corner[i][0] * DR3_GRID_HALF_LEN) * dy +
                              (double)(corner[i][1] * DR3_GRID_HALF_WID) * sy;

            if (!dr3_grid_road(track, (int)(x + cx + 0.5), (int)(y + cy + 0.5))) return i;
        }
    }

    return 5;
}

/*
 * What the march sees at one point: the axis of the road there and the room it has along it.  The host
 * probe (logs/grid_probe.c) prints it for the four start slots when a placement comes up short - a walk
 * that stops on the first step is read from here.
 */
int dr3_grid_axis_at(const dr3_grid_t *track, double x, double y, double heading, double *dir)
{
    return dr3_grid_axis(track, x, y, heading, dir);
}

int dr3_grid_road_line_at(const dr3_grid_t *track, double x, double y, double heading, double *dir)
{
    return dr3_grid_axis_line(track, x, y, heading, dir);
}

int dr3_grid_img_index(float dir)
{
    int img;

    if (dir < 0.0f) dir = 0.0f;

    img = (int)((double)dir / DR3_GRID_ROT_DEG + 0.5);

    return img % 96;
}



/* --------------------------------------------------------------- the standings --- */

void dr3_grid_order_by_rank(const int *rank, int cars, int *order)
{
    int i, j;

    if (!rank || !order || (cars <= 0)) return;
    if (cars > DR3_GRID_CARS) cars = DR3_GRID_CARS;

    for (i = 0; i < cars; ++i) order[i] = -1;

    for (i = 0; i < cars; ++i) {

        int best = -1, best_key = 0;

        for (j = 0; j < cars; ++j) {

            const int key = (rank[j] > 0) ? rank[j] : DR3_GRID_RANK_NONE;
            int       k, taken = 0;

            for (k = 0; k < i; ++k) if (order[k] == j) { taken = 1; break; }
            if (taken) continue;

            /* strictly smaller, so equal ranks keep the order they came in with */
            if ((best < 0) || (key < best_key)) { best = j; best_key = key; }
        }

        order[i] = best;
    }
}

int dr3_chaos_boss_should_join(int roll_0_99, int mode, int chance_pct)
{
    /* modes: 0 = STANDARD, 1 = DUEL (excluded), 2 = DM, 3 = RANDOM (first-race standard), 4 = ARENA */
    if ((mode < 0) || (mode > 4) || (mode == 1)) return 0;
    if (chance_pct <= 0) return 0;
    if (chance_pct >= 100) return 1;

    if (roll_0_99 < 0) roll_0_99 = -roll_0_99;
    roll_0_99 %= 100;

    return (roll_0_99 < chance_pct) ? 1 : 0;
}

int dr3_chaos_boss_pick_car(int field_n, int num_cars, int player_car)
{
    int i;

    (void)num_cars;
    if ((field_n < 2) || (field_n > DR3_GRID_CARS)) return -1;

    for (i = field_n - 1; i >= 0; --i)
        if (i != player_car) return i;

    return -1;
}

void dr3_grid_order_with_leader(const int *rank, int cars, int leader_car, int *order)
{
    int i, pos = -1;

    if (!rank || !order || (cars <= 0)) return;
    if (cars > DR3_GRID_CARS) cars = DR3_GRID_CARS;

    dr3_grid_order_by_rank(rank, cars, order);

    if ((leader_car < 0) || (leader_car >= cars)) return;

    for (i = 0; i < cars; ++i)
        if (order[i] == leader_car) { pos = i; break; }

    if (pos <= 0) return;

    for (i = pos; i > 0; --i)
        order[i] = order[i - 1];

    order[0] = leader_car;
}

int dr3_grid_align_vai(const uint8_t *vai, int wq, int hq, int nchk,
                       float x, float y, float *dir)
{
    static const double probes[3] = { 24.0, 36.0, 48.0 };
    double              nx, ny;
    int                 i, vote = 0;

    if (!vai || !dir || (wq <= 0) || (hq <= 0) || (nchk < 4)) return 0;

    /* car physics nose vector is Direction + 180 (race___4c434h.c) */
    dr3_grid_vector(dr3_grid_norm360((double)*dir + 180.0), &nx, &ny);

    for (i = 0; i < 3; ++i) {
        const int x1 = ((int)((double)x + probes[i] * nx + 0.5)) >> 2;
        const int y1 = ((int)((double)y + probes[i] * ny + 0.5)) >> 2;
        const int x0 = ((int)((double)x - probes[i] * nx + 0.5)) >> 2;
        const int y0 = ((int)((double)y - probes[i] * ny + 0.5)) >> 2;
        int       c1, c0, d;

        if ((x1 < 0) || (y1 < 0) || (x1 >= wq) || (y1 >= hq)) continue;
        if ((x0 < 0) || (y0 < 0) || (x0 >= wq) || (y0 >= hq)) continue;

        c1 = (int)vai[y1 * wq + x1];
        c0 = (int)vai[y0 * wq + x0];
        /* TRX_VAI == 0 is off-track / wall; valid checkpoints are 1..nchk (race___55ae0h.c) */
        if ((c1 <= 0) || (c0 <= 0) || (c1 > nchk) || (c0 > nchk)) continue;

        d = c1 - c0;
        if (d > nchk / 2)       d -= nchk;
        else if (d < -nchk / 2) d += nchk;

        if (d > 0)      ++vote;
        else if (d < 0) --vote;
    }

    if (vote < 0) {
        *dir = (float)dr3_grid_norm360((double)*dir + 180.0);
        return 1;
    }

    return 0;
}

