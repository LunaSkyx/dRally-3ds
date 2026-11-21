/*
 * dr3_results.h - the four rows of a results panel ("EASY/MEDIUM/HARD RACE RESULTS").
 *
 * The three panels of the game (___36068h.c / ___36358h.c / ___36718h.c) draw four rows, and every row is
 * one racer: the name, the portrait, the car picture and the rank of the row come out of s_6c[row], and the
 * row is also the pitch of the picture (0x13*row).  The original loops build those four rows from the race
 * table: they search the *first four cars* for the one that finished 1st..4th and use the position of the
 * car as the index into the entry list of the race.  In a four car race that is the same thing - the field
 * *is* the entry list and every car of it finishes.
 *
 * A chaos race is not that: the field is lined up from the standings (twenty cars, ___33010h.c), so the four
 * best places belong to cars that are anywhere in the field, and a row none of the first four cars filled
 * keeps whatever the stack had in it.  That is the bug this module fixes: the same racer in several rows
 * (on the 3DS: three rows of "JANE HONDA" in the "MEDIUM RACE RESULTS" panel of a race he only won once).
 *
 * dr3_results_rows() does the same thing the way the result really reads: the racer of the car that
 * finished place n goes into row n-1, taken over *every* car of the field, and a row the race left empty is
 * filled with a racer that is not in the list yet - never with a second copy of somebody.
 */
#ifndef DR3_RESULTS_H
#define DR3_RESULTS_H

/* the rows a panel of the original draws */
#define DR3_RESULTS_ROWS 4

/*
 * How many racers the game has (s_6c[0] .. s_6c[0x13], the standings of a career).  A row is an index into
 * that table, so nothing outside of it may end up here - the panels use it as a row pitch as well.
 */
#define DR3_RESULTS_RACERS 0x14

/*
 * Fill the four rows of a panel.
 *
 *  rows[DR3_RESULTS_ROWS]  out: the racer of every row, in the order the panel draws them (row 0 is the
 *                               winner).  Never out of 0..racers-1, and no racer twice.
 *  racers                  how many racers the caller has (DR3_RESULTS_RACERS); 0 or a wild value means 20.
 *  car_seat[cars]          which racer sits in car n (dr3_chaos_car_seat in the game, dr3_chaos.h)
 *  car_place[cars]         the place car n finished in, 1 is the winner, < 1 for a car without a place
 *  cars                    how many cars the field of the race had (NUM_OF_CARS in the game)
 *
 * car_seat and car_place may be NULL (then every row comes from the fallback), which is what the unit tests
 * use; the game always passes both.
 */
void dr3_results_rows(int *rows, int racers, const int *car_seat, const int *car_place, int cars);

#endif /* DR3_RESULTS_H */
