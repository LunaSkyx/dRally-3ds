/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_chaos.h - the chaos test build (-DDR3_CHAOS): eight extra cars on the grid.
 *
 * The original engine has room for four cars per race and that limit is baked into the memory layout:
 * the cars of the running race (___1e6ed0h), s_94 (___1de580h), s_54 (___1de7d0h) and the car index
 * tables in bss.c are all sized for four.  This build sizes those arrays with DR3_MAX_CARS instead;
 * without -DDR3_CHAOS that is 4, so every array gets exactly the size it had before and the release
 * build is unchanged.
 *
 * Only the *race* grows.  The front end, the results and the in-race car list draw into fixed rows and
 * buffers, so they keep DR3_HUD_CARS entries.  The extra grid entries come from racers that are not in
 * the race yet (___33010h.c) and share the four start slots the track file provides (race___49bd4h.c).
 *
 * Only the files that take part in it include this header - and only they are recompiled when
 * -DDR3_CHAOS is flipped.
 */
#ifndef DR3_CHAOS_H
#define DR3_CHAOS_H

#define DR3_HUD_CARS    4                   /* rows the in-race car list has room for */
#define DR3_CHAOS_EXTRA 8                   /* how many cars the test build adds on top */

#if defined(DR3_CHAOS)
#define DR3_MAX_CARS    (DR3_HUD_CARS + DR3_CHAOS_EXTRA)
/*
 * The in-race car list stops where the list ends (its rows start at 0x20*n and the fourth row already
 * ends at row 177 of the 200).  A race with fewer cars keeps drawing exactly as many rows as it has,
 * so a two car duel still shows two - this only cuts down the chaos grid.
 */
#define DR3_HUD_LOOP_LIMIT(cars) (((cars) < DR3_HUD_CARS) ? (cars) : DR3_HUD_CARS)
#else
#define DR3_MAX_CARS    DR3_HUD_CARS
/* the release build keeps the original expression, so the generated code does not change at all */
#define DR3_HUD_LOOP_LIMIT(cars) (cars)
#endif

#endif /* DR3_CHAOS_H */
