/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_log.h - tiny file logger for the Nintendo 3DS build.
 *
 * Azahar is a GUI application, so the guest's stdout never reaches a console we can read.  This
 * logger appends to an absolute path on the SD card (sdmc:/drally_3ds.log), which is a normal
 * folder on the PC (%APPDATA%\azahar\sdmc) - so printf-style debugging works from the host.
 */
#ifndef DR3_LOG_H
#define DR3_LOG_H

void dr3_log(const char *fmt, ...);

/* line counter so we can see how far the engine got even if the log is huge */
int  dr3_log_lines(void);

/*
 * The phases of a race frame the timing switch (sdmc:/chaos_time.txt) reports: tick() before the
 * section, time(slot, tick) after it (dr3_log.c keeps the sums and writes them once a second).
 *
 * The slots overlap on purpose where one phase contains another (FRAME contains everything of the
 * render part, PRESENT contains the copy steps and the port's blit, PER_CAR contains the six per car
 * loops) - so the numbers are read as "how much of the second goes into this", not as a sum.
 */
#define DR3_CT_PER_CAR   0    /* every per car loop of the frame (helper01 over the cars)   */
#define DR3_CT_COLLISION 1    /* race___4dcach                                              */
#define DR3_CT_SKIDS     2    /* race___4ee9ch + race___4f030h                              */
#define DR3_CT_TRACKCOPY 3    /* TRX_IMA -> BACKBUFFER                                      */
#define DR3_CT_SPECT     4    /* race___52d7ch                                              */
#define DR3_CT_DUST      5    /* race___50ef4h, per car                                     */
#define DR3_CT_SHADOW    6    /* race___4f170h                                              */
#define DR3_CT_CARS      7    /* race___4f300h                                              */
#define DR3_CT_EXPLOS    8    /* race___51ce0h                                              */
#define DR3_CT_WEAPONS   9    /* race___50a48h + race___51204h + race___50ba4h, per car      */
#define DR3_CT_OBJSORT  10    /* race___53310h  (painter's sort of the objects)              */
#define DR3_CT_OBJDRAW  11    /* race___53464h  (project + rasterise them)                    */
#define DR3_CT_HUD      12    /* race___51eb4h + race___56594h + race___42218h + race___40f48h */
#define DR3_CT_PRESENT  13    /* copy into VGA13 + the car mask + __VGA13_PRESENTSCREEN__     */
#define DR3_CT_FRAME    14    /* the whole loop body (physics and render, incl. present)     */

/* the six per car loops, one by one (all of them are inside PER_CAR as well) */
#define DR3_CT_PC_SPRITE  15  /* race___4b62ch  - sprite/image of every car                 */
#define DR3_CT_PC_PHYS    16  /* race___4c434h  - the car physics/AI                        */
#define DR3_CT_PC_SKID    17  /* race___514d8h  - skid marks                                */
#define DR3_CT_PC_A       18  /* race___52ac4h                                              */
#define DR3_CT_PC_B       19  /* race___526ach                                              */
#define DR3_CT_PC_OFFROAD 20  /* race___4ff50h  - the machine gun: bullets, hits, wreck payout */

/* the race screen: every step of the present, and the port's own blit (inside PRESENT) */
#define DR3_CT_COPY     21    /* BACKBUFFER -> VGA13_ACTIVESCREEN                           */
#define DR3_CT_MASKCOPY 22    /* the car mask, the same rows and offset                     */
#define DR3_CT_BLIT     23    /* the index -> 32 bit conversion into the 3DS framebuffer    */
#define DR3_CT_TINT     24    /* colouring the car pixels of the frame                      */
#define DR3_CT_FLUSH    25    /* GSPGPU_FlushDataCache                                      */
#define DR3_CT_SWAP     26    /* gfxScreenSwapBuffers                                       */

/* everything of the frame before the render part (the physics side) and its frame limiter */
#define DR3_CT_PRE      27
#define DR3_CT_WAIT     28

/* the steps of the car colour pass (dr3_carcolour_tint_cars, dr3_blit.c) - inside DR3_CT_TINT */
#define DR3_CT_TINT_SCAN   29   /* the mask scan: where the marks of the frame are       */
#define DR3_CT_TINT_SHADES 30   /* the shade ramps of every car                          */
#define DR3_CT_TINT_MASK   31   /* walking the box and writing the car pixels            */
#define DR3_CT_TINT_HP     32   /* the health lines                                      */

#if (defined(__3DS__) || defined(DR3_HOST)) && defined(DR3_CHAOS)
/*
 * Chaos test build (-DDR3_CHAOS, dr3_chaos.h): a capped step trace for the paths outside the race loop
 * (the post race screens and the front end), so a hang or a crash shows how far the game got.
 *
 * On the PC (-DDR3_HOST, see dr3_host.h) the same diagnostics are what the harness reads, so this
 * header hands them out there as well - the no-ops below are then only for the builds that have neither.
 */
void dr3_chaos_trace(const char *what, int n);

/* flushes the log - used by the heap guard report in drmemory.c */
void dr3_log_flush(void);

/*
 * The log switch: sdmc:/chaos_log.txt holds one number - 0 (the default) silences the whole log, 1 turns
 * everything back on.  It is read before the first line and again at every race setup, so it can be flipped
 * from the PC while the game runs.  While it is 0 the expensive diagnostics (the frame dumps, the stripe
 * checksums, the band measurement, the step trace) are not even computed.
 */
int  dr3_log_enabled(void);
void dr3_log_quiet_reload(void);

/*
 * Watchdog for the flickering lines above the car: the drawing helpers call this for every pixel they
 * write, and it logs the ones that land in the rows just above the player's sprite (nothing legitimate
 * draws there), with the helper's name and the value.  See gfx.c and race___4f300h.c.
 */
void dr3_chaos_watch(int slot, const char *who, int x, int y, int value);

/*
 * The remap filler (___5e769h, gfx.c) is called from the track shadows (race___4f170h.c), the object
 * shadows (race___53464h.c) and the car lights (race___4f300h.c).  Each caller names itself here before
 * the call, so the watchdog's "remap" line says which of them wrote over the car.
 */
void dr3_chaos_remap_who(const char *who);
const char *dr3_chaos_remap_who_get(void);

/*
 * The band above the player's car: start() samples the rows above the sprite, mark() compares them again
 * after one pass of the frame loop and logs what that pass wrote there - that names the layer the grey
 * lines come from (see dr3_log.c).
 */
void dr3_chaos_band_start(void);
void dr3_chaos_band_mark(const char *pass);

/* frame time breakdown (see dr3_log.c): tick() before the section, time(slot, tick) after it */
unsigned long long dr3_chaos_tick(void);
void               dr3_chaos_time(int slot, unsigned long long t0);

/*
 * The per frame recorder of the scenario harness ("perf start" / "perf report NAME", dr3_scenario.h): every
 * phase above is summed per frame and the report names the percentiles, the stutter frames and what cost
 * more than usual in the worst ones.  Needs the timing switch (sdmc:/chaos_time.txt = 1) - az_run.ps1 -Perf.
 */
void dr3_chaos_perf_start(void);
void dr3_chaos_perf_report(const char *label);

/*
 * The timing switch (sdmc:/chaos_time.txt, 0/1): with it on, dr3_chaos_time() writes the frame time
 * breakdown - and *only* that, the file is opened even though the main log switch (chaos_log.txt) is
 * off, so none of the expensive diagnostics (band, watchdog, dumps, step trace) run.  Measuring the
 * frame while a watchdog watches every pixel is not a measurement.
 */
int  dr3_log_timing(void);

/* writes a line although the main log switch is off (the timing breakdown and the port profiler) */
void dr3_log_write(const char *fmt, ...);
#else
#define dr3_chaos_trace(what, n) ((void)0)
#define dr3_chaos_perf_start()   ((void)0)
#define dr3_chaos_perf_report(l) ((void)(l))
#define dr3_log_flush()          ((void)0)
#define dr3_log_enabled()        (1)
#define dr3_log_quiet_reload()   ((void)0)
#define dr3_log_timing()         (0)
#define dr3_log_write(...)       ((void)0)
/*
 * The no-ops consume their arguments: the call sites in race_main.c declare "const unsigned long long
 * t = dr3_chaos_tick();" and hand it to dr3_chaos_time().  Without this every build that is not the
 * chaos test build reports those variables as unused - and a warning in the port code is a failure
 * here now, so the timing must not produce any.
 */
#define dr3_chaos_tick()         (0ull)
#define dr3_chaos_time(slot, t0) ((void)(slot), (void)(t0))
#define dr3_chaos_watch(slot, who, x, y, value) ((void)0)
#define dr3_chaos_remap_who(who) ((void)0)
#define dr3_chaos_remap_who_get() ("")
#define dr3_chaos_band_start()   ((void)0)
#define dr3_chaos_band_mark(pass) ((void)0)
#endif

#endif /* DR3_LOG_H */
