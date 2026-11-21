/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_payout.h"

/*
 * The ladder of dr3_payout.h as one function.  The step is the base of every place; the podium pays its
 * jump *instead* of it, but only when the jump is the larger of the two - so a field that is too small to
 * make a step of 1600 (a four car race, a race with the extras switched off) still pays the jump, and the
 * numbers can be retuned without the ladder crossing itself.
 */
int dr3_chaos_rank_money(int place, int cars)
{
    int prize;

    if (cars < 1) return 0;
    if ((place < 1) || (place > cars)) return 0;

    /* the last place pays nothing: that is the place a wrecked car ends up in */
    if (place >= cars) return 0;

    prize = DR3_CHAOS_RANK_STEP * (cars - place);

    switch (place) {

        case 1:

            if (prize < DR3_CHAOS_RANK_MONEY) prize = DR3_CHAOS_RANK_MONEY;
            break;

        case 2:

            if (prize < DR3_CHAOS_RANK_PODIUM_2) prize = DR3_CHAOS_RANK_PODIUM_2;
            break;

        case 3:

            if (prize < DR3_CHAOS_RANK_PODIUM_3) prize = DR3_CHAOS_RANK_PODIUM_3;
            break;

        default:

            break;
    }

    return prize;
}
