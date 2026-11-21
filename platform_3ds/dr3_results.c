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
