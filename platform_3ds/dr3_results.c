/*
 * dr3_results.c - the row mapping of the results panels, see dr3_results.h.
 *
 * Portable on purpose (no engine globals, no screen, no libc): the mapping is what went wrong on the 3DS -
 * the same racer in several rows - and it is checkable without a console or a race (tests/test_dr3.c).
 */
#include <stddef.h>   /* NULL - MSVC pulls this in transitively, GCC does not */
#include "dr3_results.h"

/* is this racer already in one of the rows? */
static int dr3_results_taken(const int *rows, int count, int racer)
{
	int r;

	for (r = 0; r < count; ++r) if (rows[r] == racer) return 1;

	return 0;
}

void dr3_results_rows(int *rows, int racers, const int *car_seat, const int *car_place, int cars)
{
	int r, car;

	if (rows == NULL) return;

	if ((racers <= 0) || (racers > DR3_RESULTS_RACERS)) racers = DR3_RESULTS_RACERS;
	if (cars < 0) cars = 0;

	for (r = 0; r < DR3_RESULTS_ROWS; ++r) rows[r] = -1;

	/*
	 * The places first: the racer of the car that finished place n owns row n-1.  The first car that
	 * carries a place gets it - a table with the same place twice (a stale one from an older race, the
	 * chaos fields reuse the entries) cannot put the same racer into two rows.
	 */
	if (car_place != NULL) {

		for (car = 0; car < cars; ++car) {

			const int place = car_place[car];
			const int seat  = (car_seat != NULL) ? car_seat[car] : -1;

			if ((place < 1) || (place > DR3_RESULTS_ROWS)) continue;
			if ((seat < 0) || (seat >= racers)) continue;
			if (rows[place - 1] >= 0) continue;

			rows[place - 1] = seat;
		}
	}

	/*
	 * What is left over: the cars of the field in the order they started (a race that gave fewer places
	 * than the panel has rows - a duel, a wreck that ended a race early), and behind that any racer that
	 * is not in the list yet.  A row is never left unset: the panels index s_6c[] with it and use it as a
	 * row pitch, so it has to be a racer.
	 */
	for (r = 0; r < DR3_RESULTS_ROWS; ++r) {

		int seat;

		if (rows[r] >= 0) continue;

		if ((car_seat != NULL) && (r < cars)) {

			seat = car_seat[r];

			if ((seat >= 0) && (seat < racers) && !dr3_results_taken(rows, DR3_RESULTS_ROWS, seat)) {
				rows[r] = seat;
				continue;
			}
		}

		for (seat = 0; seat < racers; ++seat) {

			if (dr3_results_taken(rows, DR3_RESULTS_ROWS, seat)) continue;

			rows[r] = seat;
			break;
		}

		/* no racer left at all (only possible with fewer racers than rows): the first one will do */
		if (rows[r] < 0) rows[r] = 0;
	}
}

static int dr3_race_car_better(const dr3_race_car_t *a, const dr3_race_car_t *b)
{
	const int a_dead = (!a->finished && (a->hp <= 0));
	const int b_dead = (!b->finished && (b->hp <= 0));

	if (a_dead != b_dead) return !a_dead;

	if (a_dead) {
		if (a->dead_order != b->dead_order) return a->dead_order > b->dead_order;
		return a->old_pos < b->old_pos;
	}

	if (a->lap != b->lap) return a->lap > b->lap;

	if (a->finished != b->finished) return a->finished > b->finished;

	if (a->finished) {
		if (a->finish_seq != b->finish_seq) {
			if (a->finish_seq <= 0) return 0;
			if (b->finish_seq <= 0) return 1;
			return a->finish_seq < b->finish_seq;
		}
		return a->old_pos < b->old_pos;
	}

	if (a->checkpoint != b->checkpoint) return a->checkpoint > b->checkpoint;

	return a->old_pos < b->old_pos;
}

void dr3_race_rank_positions(const dr3_race_car_t *cars, int count, int *out_pos)
{
	int order[64];
	int i, j;

	if (!cars || !out_pos || (count <= 0)) return;
	if (count > 64) count = 64;

	for (i = 0; i < count; ++i) order[i] = i;

	/* Stable insertion sort: count <= 30, preserves index order on identical keys */
	for (i = 1; i < count; ++i) {
		const int key = order[i];

		j = i - 1;
		while ((j >= 0) && dr3_race_car_better(&cars[key], &cars[order[j]])) {
			order[j + 1] = order[j];
			--j;
		}
		order[j + 1] = key;
	}

	for (i = 0; i < count; ++i) {
		out_pos[order[i]] = i + 1;
	}
}

int dr3_race_update_checkpoint(int cur_chk, int nchk, int v1, int v2, int v_mid)
{
	const int samples[3] = { v1, v2, v_mid };
	int max_jump;
	int best;
	int i;

	if (nchk <= 1) return cur_chk;
	if (cur_chk < 0) cur_chk = 0;
	if (cur_chk >= nchk) return nchk;

	/* Allow up to +3 checkpoint catch-up (~10% of a lap) when bumped across a corner/curb seam */
	max_jump = (nchk >= 8) ? 3 : 1;
	best = cur_chk;

	for (i = 0; i < 3; ++i) {
		const int v = samples[i];
		const int diff = v - cur_chk;

		if ((v <= 0) || (v > nchk)) continue;

		if ((diff >= 1) && (diff <= max_jump)) {
			if (v > best) best = v;
		} else if ((nchk >= 6) && (cur_chk >= (nchk - 2)) && ((v == 1) || (v == 2))) {
			/*
			 * Car was at the final sector (nchk - 1 or nchk - 2) and stepped completely across a thin
			 * VAI == nchk finish line directly into VAI == 1..2: credit nchk so race___55ae0h counts
			 * the completed lap on this tick (and advances 0 -> 1 on the next tick).
			 */
			best = nchk;
		}
	}

	return best;
}

static float dr3_wall_isqrt_approx(float x)
{
	float half;
	int i;

	if (x <= 1e-6f) return 0.0f;
	/* 3 steps of Newton-Raphson around an integer-aliased or simple initial guess */
	half = 0.5f * x;
	/* Portable initial guess for 1/sqrt(x) over [1, 1e8] */
	{
		float g = 1.0f;
		float t = x;
		while (t >= 4.0f) { t *= 0.25f; g *= 0.5f; }
		while (t < 1.0f)  { t *= 4.0f;  g *= 2.0f; }
		for (i = 0; i < 4; ++i) {
			g = g * (1.5f - half * g * g);
		}
		return g;
	}
}

void dr3_wall_scan_sprite(
	const unsigned char *sprite,
	const unsigned char *trx_mas,
	int trx_w, int trx_h,
	int top_left_x, int top_left_y,
	dr3_wall_hit_t *out)
{
	int count = 0;
	int first_val = 0xf;
	int first_i = 20;
	int first_j = 20;
	int sum_dx = 0;
	int sum_dy = 0;
	int i, j;

	if (!out) return;

	out->count = 0;
	out->first_val = 0xf;
	out->first_i = 20;
	out->first_j = 20;
	out->mean_dx = 0;
	out->mean_dy = 0;
	out->sum_dx = 0;
	out->sum_dy = 0;

	if (!sprite || !trx_mas || (trx_w <= 0) || (trx_h <= 0)) return;

	for (j = 0; j < 40; ++j) {
		const int ty = top_left_y + j;
		const unsigned char *sp_row = sprite + 40 * j;
		const int y_oob = ((ty < 0) || (ty >= trx_h));
		const unsigned char *mas_row = y_oob ? NULL : (trx_mas + trx_w * ty);

		for (i = 0; i < 40; ++i) {
			int cell;

			if ((signed char)sp_row[i] <= 3) continue;

			if (y_oob) {
				cell = 0;
			} else {
				const int tx = top_left_x + i;
				if ((tx < 0) || (tx >= trx_w)) {
					cell = 0;
				} else {
					cell = (int)((signed char)(mas_row[tx] & 0xf));
				}
			}

			if (cell < 4) {
				if (count == 0) {
					first_val = cell;
					first_i = i;
					first_j = j;
				}
				++count;
				sum_dx += (i - 20);
				sum_dy += (j - 20);
			}
		}
	}

	out->count = count;
	out->first_val = first_val;
	out->first_i = first_i;
	out->first_j = first_j;
	out->sum_dx = sum_dx;
	out->sum_dy = sum_dy;
	if (count > 0) {
		int mdx = sum_dx / count;
		int mdy = sum_dy / count;
		if ((mdx == 0) && (mdy == 0)) {
			mdx = first_i - 20;
			mdy = first_j - 20;
			if ((mdx == 0) && (mdy == 0)) mdy = -1;
		}
		out->mean_dx = mdx;
		out->mean_dy = mdy;
	}
}

static void dr3_wall_outward_normal(const dr3_wall_hit_t *hit, float vx, float vy, float *out_nx, float *out_ny)
{
	float wx = (float)(-hit->sum_dx);
	float wy = (float)(-hit->sum_dy);
	float len2 = wx * wx + wy * wy;

	if (len2 < 1.0f) {
		wx = (float)(-(hit->first_i - 20));
		wy = (float)(-(hit->first_j - 20));
		len2 = wx * wx + wy * wy;
	}
	if (len2 < 1.0f) {
		wx = -vx;
		wy = -vy;
		len2 = wx * wx + wy * wy;
	}
	if (len2 < 1e-4f) {
		*out_nx = 0.0f;
		*out_ny = -1.0f;
		return;
	}
	{
		const float inv = dr3_wall_isqrt_approx(len2);
		*out_nx = wx * inv;
		*out_ny = wy * inv;
	}
}

int dr3_wall_resolve_step(
	const unsigned char *sprite,
	const unsigned char *trx_mas,
	int trx_w, int trx_h,
	float *io_x, float *io_y,
	float *io_vx, float *io_vy,
	float *io_fc, float *io_f100,
	float *io_speed,
	int stuck_counter,
	dr3_wall_hit_t *out_initial_hit,
	dr3_wall_hit_t *out_final_hit)
{
	/* 8 unit compass vectors for ring fallback */
	static const float ring_dx[8] = {
		 1.0f,  0.70710678f,  0.0f, -0.70710678f,
		-1.0f, -0.70710678f,  0.0f,  0.70710678f
	};
	static const float ring_dy[8] = {
		 0.0f,  0.70710678f,  1.0f,  0.70710678f,
		 0.0f, -0.70710678f, -1.0f, -0.70710678f
	};
	static const float ring_radii[5] = { 3.0f, 6.0f, 11.0f, 17.0f, 25.0f };

	dr3_wall_hit_t init_hit;
	dr3_wall_hit_t cand_hit;
	dr3_wall_hit_t best_hit;
	float x, y, vx, vy;
	float nx, ny;
	float dot_v;
	float slide_vx, slide_vy;
	float best_x, best_y;
	int probes = 0;

	if (!io_x || !io_y || !io_vx || !io_vy) return 0;

	x = *io_x;
	y = *io_y;
	vx = *io_vx;
	vy = *io_vy;

	dr3_wall_scan_sprite(
		sprite, trx_mas, trx_w, trx_h,
		(int)((double)x + (double)vx - 20.0),
		(int)((double)y + (double)vy - 20.0),
		&init_hit);

	if (out_initial_hit) *out_initial_hit = init_hit;
	if (out_final_hit)   *out_final_hit   = init_hit;

	if (init_hit.count == 0) return 0;

	/* Outward normal pointing from wall centroid toward open road */
	dr3_wall_outward_normal(&init_hit, vx, vy, &nx, &ny);

	/*
	 * Reflect any inward knockback velocity (io_fc, io_f100) away from the wall so consecutive frames
	 * never double-negate back into the wall.
	 */
	if (io_fc && io_f100) {
		const float dot_fc = (*io_fc) * nx + (*io_f100) * ny;
		if (dot_fc < 0.0f) {
			*io_fc   -= 1.45f * dot_fc * nx;
			*io_f100 -= 1.45f * dot_fc * ny;
		}
		if ((stuck_counter > 0) && (((*io_fc) * nx + (*io_f100) * ny) < 0.5f)) {
			*io_fc   += 0.5f * nx;
			*io_f100 += 0.5f * ny;
		}
	}

	/*
	 * Dampen engine speed (__b0) on wall contact without zeroing it completely, so a player or AI car
	 * rubbing a wall in a 10-car pack keeps steering and throttle authority to drive out.
	 */
	if (io_speed) {
		if (stuck_counter <= 0) {
			*io_speed *= (float)(7.0 / 8.0);
		} else {
			*io_speed *= 0.65f;
		}
	}

	/* Tangential slide vector: remove the inward component of (vx, vy) */
	dot_v = vx * nx + vy * ny;
	if (dot_v < 0.0f) {
		slide_vx = (vx - 1.15f * dot_v * nx) * 0.88f;
		slide_vy = (vy - 1.15f * dot_v * ny) * 0.88f;
	} else {
		slide_vx = vx * 0.88f;
		slide_vy = vy * 0.88f;
	}

	/*
	 * 1. Try tangential slide (and tangential slide + 2px outward normal clearance for stair-stepped
	 *    diagonal walls in TRX_MAS).
	 */
	if ((slide_vx * slide_vx + slide_vy * slide_vy) > 0.01f) {
		++probes;
		dr3_wall_scan_sprite(
			sprite, trx_mas, trx_w, trx_h,
			(int)((double)x + (double)slide_vx - 20.0),
			(int)((double)y + (double)slide_vy - 20.0),
			&cand_hit);
		if (cand_hit.count == 0) {
			*io_vx = slide_vx;
			*io_vy = slide_vy;
			if (out_final_hit) *out_final_hit = cand_hit;
			return probes;
		}

		++probes;
		dr3_wall_scan_sprite(
			sprite, trx_mas, trx_w, trx_h,
			(int)((double)x + (double)slide_vx + (double)(nx * 2.0f) - 20.0),
			(int)((double)y + (double)slide_vy + (double)(ny * 2.0f) - 20.0),
			&cand_hit);
		if (cand_hit.count == 0) {
			*io_vx = slide_vx + nx * 2.0f;
			*io_vy = slide_vy + ny * 2.0f;
			if (out_final_hit) *out_final_hit = cand_hit;
			return probes;
		}
	}

	/*
	 * 2. Check current position (x, y) before step: if (x, y) itself is wall-free, stay at (x, y)
	 *    (or slide along whichever single axis is clear) instead of stepping into the wall!
	 */
	++probes;
	dr3_wall_scan_sprite(
		sprite, trx_mas, trx_w, trx_h,
		(int)((double)x - 20.0),
		(int)((double)y - 20.0),
		&best_hit);
	best_x = x;
	best_y = y;

	if (best_hit.count == 0) {
		/* Try single-axis slide in X or Y so axis-aligned walls still allow smooth sliding */
		if ((slide_vx * slide_vx) > 0.04f) {
			++probes;
			dr3_wall_scan_sprite(
				sprite, trx_mas, trx_w, trx_h,
				(int)((double)x + (double)slide_vx - 20.0),
				(int)((double)y - 20.0),
				&cand_hit);
			if (cand_hit.count == 0) {
				*io_vx = slide_vx;
				*io_vy = 0.0f;
				if (out_final_hit) *out_final_hit = cand_hit;
				return probes;
			}
		}
		if ((slide_vy * slide_vy) > 0.04f) {
			++probes;
			dr3_wall_scan_sprite(
				sprite, trx_mas, trx_w, trx_h,
				(int)((double)x - 20.0),
				(int)((double)y + (double)slide_vy - 20.0),
				&cand_hit);
			if (cand_hit.count == 0) {
				*io_vx = 0.0f;
				*io_vy = slide_vy;
				if (out_final_hit) *out_final_hit = cand_hit;
				return probes;
			}
		}

		*io_vx = 0.0f;
		*io_vy = 0.0f;
		if (out_final_hit) *out_final_hit = best_hit;
		return probes;
	}

	/*
	 * 3. Current position (x, y) is already overlapping a wall (e.g. rotated 40x40 sprite near a wall
	 *    or pushed in by a 10-car pileup).  If (x + vx, y + vy) actually had fewer overlapping pixels,
	 *    start the outward ejection from whichever of (x, y) and (x + vx, y + vy) is cleaner.
	 */
	if (init_hit.count < best_hit.count) {
		best_x = x + vx;
		best_y = y + vy;
		best_hit = init_hit;
	}

	/*
	 * Phase A: Iterative gradient/normal ejection (up to 6 steps).
	 * Each step recomputes the exact outward normal from the remaining overlapping pixels and steps
	 * 2..5 px outward until count == 0.
	 */
	{
		float cur_x = best_x;
		float cur_y = best_y;
		dr3_wall_hit_t cur_hit = best_hit;
		int step;

		for (step = 0; step < 6; ++step) {
			float step_nx, step_ny;
			const float step_dist = (cur_hit.count > 45) ? 5.0f : ((cur_hit.count > 12) ? 3.5f : 2.0f);
			float cand_x, cand_y;

			dr3_wall_outward_normal(&cur_hit, vx, vy, &step_nx, &step_ny);
			cand_x = cur_x + step_nx * step_dist;
			cand_y = cur_y + step_ny * step_dist;

			++probes;
			dr3_wall_scan_sprite(
				sprite, trx_mas, trx_w, trx_h,
				(int)((double)cand_x - 20.0),
				(int)((double)cand_y - 20.0),
				&cand_hit);

			if (cand_hit.count == 0) {
				*io_x = cand_x;
				*io_y = cand_y;
				*io_vx = 0.0f;
				*io_vy = 0.0f;
				if (out_final_hit) *out_final_hit = cand_hit;
				return probes;
			}

			if (cand_hit.count < best_hit.count) {
				best_x = cand_x;
				best_y = cand_y;
				best_hit = cand_hit;
				cur_x = cand_x;
				cur_y = cand_y;
				cur_hit = cand_hit;
			} else {
				break;
			}
		}
	}

	/*
	 * Phase B: Corner / pinch fallback (8-way ring search around best_x, best_y at radii 3..25 px,
	 * testing directions aligned with the outward normal first, capped at 16 total probes per frame).
	 */
	if (best_hit.count > 0) {
		float base_nx, base_ny;
		int r_idx, d_idx;

		dr3_wall_outward_normal(&best_hit, vx, vy, &base_nx, &base_ny);

		/* Find compass direction index 0..7 closest to (base_nx, base_ny) */
		{
			int best_dir = 0;
			float best_dot = -2.0f;
			for (d_idx = 0; d_idx < 8; ++d_idx) {
				const float d = ring_dx[d_idx] * base_nx + ring_dy[d_idx] * base_ny;
				if (d > best_dot) {
					best_dot = d;
					best_dir = d_idx;
				}
			}

			for (r_idx = 0; (r_idx < 5) && (probes < 16); ++r_idx) {
				static const int dir_offsets[8] = { 0, 1, 7, 2, 6, 3, 5, 4 };
				const float rad = ring_radii[r_idx];

				for (d_idx = 0; (d_idx < 8) && (probes < 16); ++d_idx) {
					const int dir = (best_dir + dir_offsets[d_idx]) & 7;
					const float cand_x = best_x + ring_dx[dir] * rad;
					const float cand_y = best_y + ring_dy[dir] * rad;

					++probes;
					dr3_wall_scan_sprite(
						sprite, trx_mas, trx_w, trx_h,
						(int)((double)cand_x - 20.0),
						(int)((double)cand_y - 20.0),
						&cand_hit);

					if (cand_hit.count == 0) {
						*io_x = cand_x;
						*io_y = cand_y;
						*io_vx = 0.0f;
						*io_vy = 0.0f;
						if (out_final_hit) *out_final_hit = cand_hit;
						return probes;
					}

					if (cand_hit.count < best_hit.count) {
						best_x = cand_x;
						best_y = cand_y;
						best_hit = cand_hit;
					}
				}
			}
		}
	}

	*io_x = best_x;
	*io_y = best_y;
	*io_vx = 0.0f;
	*io_vy = 0.0f;
	if (out_final_hit) *out_final_hit = best_hit;
	return probes;
}

static int dr3_wall_cell_solid(const unsigned char *trx_mas, int trx_w, int trx_h, int x, int y)
{
	if (!trx_mas || (trx_w <= 0) || (trx_h <= 0)) return 0;
	if ((x < 0) || (x >= trx_w) || (y < 0) || (y >= trx_h)) return 1;
	return ((signed char)(trx_mas[trx_w * y + x] & 0xf)) < 4;
}

static int dr3_wall_side_blocked_x(
	const unsigned char *trx_mas, int trx_w, int trx_h,
	float cx, float cy, float dir_x, int wall_flag, int wall_dx)
{
	static const int offs[5] = { -12, -6, 0, 6, 12 };
	int px, i;

	if ((wall_flag != 0) && (((float)wall_dx * dir_x) > 0.0f)) return 1;
	if (!trx_mas) return 0;

	px = (int)((double)cx + (double)(dir_x * 16.0f));
	for (i = 0; i < 5; ++i) {
		const int py = (int)cy + offs[i];
		if (dr3_wall_cell_solid(trx_mas, trx_w, trx_h, px, py)) return 1;
	}
	return 0;
}

static int dr3_wall_side_blocked_y(
	const unsigned char *trx_mas, int trx_w, int trx_h,
	float cx, float cy, float dir_y, int wall_flag, int wall_dy)
{
	static const int offs[5] = { -12, -6, 0, 6, 12 };
	int py, i;

	if ((wall_flag != 0) && (((float)wall_dy * dir_y) > 0.0f)) return 1;
	if (!trx_mas) return 0;

	py = (int)((double)cy + (double)(dir_y * 16.0f));
	for (i = 0; i < 5; ++i) {
		const int px = (int)cx + offs[i];
		if (dr3_wall_cell_solid(trx_mas, trx_w, trx_h, px, py)) return 1;
	}
	return 0;
}

void dr3_wall_separate_pair(
	const unsigned char *trx_mas,
	int trx_w, int trx_h,
	float *io_xn, float *io_yn,
	float *io_fcn, float *io_f100n,
	int wall_n, int wall_dx_n, int wall_dy_n,
	float *io_xk, float *io_yk,
	float *io_fck, float *io_f100k,
	int wall_k, int wall_dx_k, int wall_dy_k,
	int do_nudge)
{
	float dir_xn, dir_yn;
	int blk_xn, blk_xk, blk_yn, blk_yk;

	if (!io_xn || !io_yn || !io_xk || !io_yk) return;

	/* Direction Car n would be pushed away from Car k (Car k is pushed in -dir_n) */
	dir_xn = ((double)(*io_xn) < (double)(*io_xk)) ? -1.0f : 1.0f;
	dir_yn = ((double)(*io_yn) < (double)(*io_yk)) ? -1.0f : 1.0f;

	blk_xn = dr3_wall_side_blocked_x(trx_mas, trx_w, trx_h, *io_xn, *io_yn,  dir_xn, wall_n, wall_dx_n);
	blk_xk = dr3_wall_side_blocked_x(trx_mas, trx_w, trx_h, *io_xk, *io_yk, -dir_xn, wall_k, wall_dx_k);
	blk_yn = dr3_wall_side_blocked_y(trx_mas, trx_w, trx_h, *io_xn, *io_yn,  dir_yn, wall_n, wall_dy_n);
	blk_yk = dr3_wall_side_blocked_y(trx_mas, trx_w, trx_h, *io_xk, *io_yk, -dir_yn, wall_k, wall_dy_k);

	/*
	 * X separation & rigid wall backstop reflection:
	 * If Car n is backed against a wall in dir_xn, Car n refuses to yield into the wall, transfers the
	 * full 1.2 px rebound to Car k, and reflects any inward knockback impulse back onto Car k.
	 */
	if (blk_xn && !blk_xk) {
		*io_xk = (float)((double)(*io_xk) + (double)(-dir_xn * 1.2f));
		if (io_fcn && io_fck && (((*io_fcn) * dir_xn) > 0.0f)) {
			*io_fck += -0.75f * (*io_fcn);
			*io_fcn  = -0.25f * (*io_fcn);
		}
	} else if (blk_xk && !blk_xn) {
		*io_xn = (float)((double)(*io_xn) + (double)(dir_xn * 1.2f));
		if (io_fcn && io_fck && (((*io_fck) * (-dir_xn)) > 0.0f)) {
			*io_fcn += -0.75f * (*io_fck);
			*io_fck  = -0.25f * (*io_fck);
		}
	} else if (do_nudge && !blk_xn && !blk_xk) {
		*io_xn = (float)((double)(*io_xn) + (double)(dir_xn * 0.6));
		*io_xk = (float)((double)(*io_xk) - (double)(dir_xn * 0.6));
	}

	/* Y separation & rigid wall backstop reflection */
	if (blk_yn && !blk_yk) {
		*io_yk = (float)((double)(*io_yk) + (double)(-dir_yn * 1.2f));
		if (io_f100n && io_f100k && (((*io_f100n) * dir_yn) > 0.0f)) {
			*io_f100k += -0.75f * (*io_f100n);
			*io_f100n  = -0.25f * (*io_f100n);
		}
	} else if (blk_yk && !blk_yn) {
		*io_yn = (float)((double)(*io_yn) + (double)(dir_yn * 1.2f));
		if (io_f100n && io_f100k && (((*io_f100k) * (-dir_yn)) > 0.0f)) {
			*io_f100n += -0.75f * (*io_f100k);
			*io_f100k  = -0.25f * (*io_f100k);
		}
	} else if (do_nudge && !blk_yn && !blk_yk) {
		*io_yn = (float)((double)(*io_yn) + (double)(dir_yn * 0.6));
		*io_yk = (float)((double)(*io_yk) - (double)(dir_yn * 0.6));
	}
}
