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
