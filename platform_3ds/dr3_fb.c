/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_fb.h"
#include "dr3_blit.h"      /* dr3_carcolour_* - the colour of every car of a race (chaos build) */
#include "dr3_log.h"
#include "dr3_prof.h"
#include "dr3_scenario.h"
#if defined(DR3_CHAOS)
#include "dr3_chaos.h"     /* dr3_chaos_car_screen / dr3_chaos_car_col - the cars of the frame */
#endif

#include <3ds.h>
#include <stddef.h>
#include <string.h>

static uint32_t *dr3_fb;
static int       dr3_fb_w;
static int       dr3_fb_h;
static int       dr3_fb_ready;
static u64       dr3_last_swap_tick;
static uint32_t  dr3_top_offscreen[240 * 400] __attribute__((aligned(8)));

/*
 * Per-mode lookup tables.  The original inner loops computed "y * sh / dh" for every one of the
 * 96.000 pixels; an ARM11 integer division costs ~20-40 cycles, so that alone was milliseconds per
 * frame.  Everything that only depends on the source/target size is precomputed once here.
 */
#define DR3_MAP_MAX 512

static int      dr3_map_sw = -1;
static int      dr3_map_sh = -1;
static uint16_t dr3_sx[DR3_MAP_MAX];    /* target x -> source column (nearest)        */
static uint16_t dr3_sy[DR3_MAP_MAX];    /* target y -> source row (nearest)           */
static uint16_t dr3_fx0[DR3_MAP_MAX];   /* filtered: first source column of target x  */
static uint16_t dr3_fx1[DR3_MAP_MAX];   /* filtered: end column (exclusive)           */
static uint32_t dr3_finv[DR3_MAP_MAX];  /* filtered: 65536 / (fx1 - fx0) - must hold 65536! */

#if defined(DR3_CHAOS)
/*
 * The colours of the cars (dr3_chaos.h).  The palette has room for four colour ramps of ten entries
 * and the chaos build races up to twenty cars, so every car sprite keeps the palette's own ramp - all
 * ten shades - and the pixels the car sprites wrote are coloured here with the colour of that car's
 * driver.  *Where* those pixels are comes from the engine's own sprite blit (dr3_chaos_car_screen,
 * filled in race___4f300h.c, copied with the image in race_main.c), so no position is worked out here.
 */
/*
 * The colours of the cars, into the rotated framebuffer of the top screen.
 *
 * The pass itself (mask, shade ramps, colouring) is shared - it lives in platform_3ds/dr3_blit.c
 * because the PC window wants the very same thing (dr3_blit.h: dr3_carcolour_tint_cars).  What is
 * special about the 3DS here is only the target: the hardware framebuffer is stored rotated, so x
 * runs along a row (stride = the row width) and y runs backwards from its last row (stride -1).
 */
static void dr3_fb_tint_cars(uint32_t *dst_fb, const uint8_t *src, int sw, int sh, int src_pitch, int dw, int dh,
                             int row_off, const dr3_lut32_t *lut)
{
    dr3_carcolour_tint_cars(dst_fb + (dh - 1), dw, dh, dr3_fb_w, -1,
                            src, sw, sh, src_pitch, row_off, lut);
}

/*
 * The car mask is the engine's race screen (race_main.c copies it from BACKBUFFER together with the
 * image), so only a present of that very buffer may use it.  A present of anything else - the front
 * end, a screen the engine keeps around - would otherwise spend the mask on the wrong picture.  This
 * says which buffer was presented and what the mask held, the first few times.
 */
void dr3_fb_tint_log_present(const uint8_t *src, int sw, int sh, int src_pitch, int is_race)
{
    static int dr3_present_logged;
    int        marked = 0, k;

    if (!dr3_log_enabled() || (dr3_present_logged >= 12)) return;

    for (k = 0; k < DR3_CAR_MASK_W * DR3_CAR_MASK_H; ++k) if (dr3_chaos_car_screen[k]) ++marked;

    dr3_log("[dr3] chaos: present %d: %s (%p, %dx%d pitch %d), %d mark(s)",
            dr3_present_logged, is_race ? "the race screen" : "NOT the race screen",
            (const void *)src, sw, sh, src_pitch, marked);
    ++dr3_present_logged;
}
#endif

static void dr3_fb_build_maps(int sw, int sh, int dw, int dh)
{
    int i;

    if (dr3_map_sw == sw && dr3_map_sh == sh) return;

    for (i = 0; i < dw && i < DR3_MAP_MAX; ++i) {
        int x1;

        dr3_sx[i]  = (uint16_t)(((uint32_t)i * (uint32_t)sw) / (uint32_t)dw);
        dr3_fx0[i] = dr3_sx[i];
        x1         = (int)(((uint32_t)(i + 1) * (uint32_t)sw) / (uint32_t)dw);
        if (x1 <= dr3_fx0[i]) x1 = dr3_fx0[i] + 1;
        if (x1 > sw)          x1 = sw;
        dr3_fx1[i]  = (uint16_t)x1;
        dr3_finv[i] = 65536u / (uint32_t)(x1 - dr3_fx0[i]);
    }

    for (i = 0; i < dh && i < DR3_MAP_MAX; ++i) {
        dr3_sy[i] = (uint16_t)(((uint32_t)i * (uint32_t)sh) / (uint32_t)dh);
    }

    dr3_map_sw = sw;
    dr3_map_sh = sh;
}

int dr3_fb_init(void)
{
    uint16_t w = 0;
    uint16_t h = 0;
    uint32_t *fb;

    if (dr3_fb_ready) return 0;

    /* libctru does not double buffer the top screen by default: without it we would draw into the
       buffer that is currently on screen (heavy tearing) */
    gfxSetDoubleBuffering(GFX_TOP, true);

    /* the top screen frame buffer is stored rotated: 240 x 400 */
    fb = (uint32_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, &h);
    dr3_fb_w = (int)w;
    dr3_fb_h = (int)h;

    if (!fb || dr3_fb_w <= 0 || dr3_fb_h <= 0) {
        dr3_log("[dr3] gfx: no framebuffer (%dx%d)", dr3_fb_w, dr3_fb_h);
        return -1;
    }

    dr3_fb      = fb;
    dr3_fb_ready = 1;
    dr3_log("[dr3] gfx: direct output %dx%d (rotated top screen, double buffered)", dr3_fb_w, dr3_fb_h);
    return 0;
}

void dr3_fb_present(const uint8_t *src, int sw, int sh, int src_pitch,
                    const dr3_palette_t *pal, const dr3_lut32_t *lut, int filter)
{
    int         x, y;
    uint16_t    w = 0;
    uint16_t    h = 0;
    uint32_t *  fb;

    if (!dr3_fb_ready && (dr3_fb_init() != 0)) return;

    /* with double buffering enabled the back buffer alternates every frame - always ask for the
       current one instead of caching the pointer */
    fb = (uint32_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, &h);
    if (!fb) return;
    dr3_fb = fb;

    /* the image is written into the rotated buffer: source column -> hardware row */
    const int dw = dr3_fb_h;                  /* 400 */
    const int dh = dr3_fb_w;                  /* 240 */
    const uint32_t *lutpx = lut->px;
    uint32_t * const target = ((dr3_fb_w == 240) && (dr3_fb_h == 400)) ? dr3_top_offscreen : fb;

    if (dw > DR3_MAP_MAX || dh > DR3_MAP_MAX) return;

    dr3_fb_build_maps(sw, sh, dw, dh);

    /* the skip-blit variant measures the floor we cannot avoid (flush + swap only) */
    if (!dr3_prof_skip_blit()) {
        const uint64_t blit_t0 = dr3_prof_tick();
        const unsigned long long dr3_t_blit = dr3_chaos_tick();

        if (filter) {
            /* average the source columns a target pixel covers - the game's dither patterns alias into
               vertical stripes with plain nearest neighbour */
            for (x = 0; x < dw; ++x) {
                uint32_t *      drow   = target + (size_t)x * (size_t)dr3_fb_w + (dh - 1);
                const int       sx0    = dr3_fx0[x];
                const int       sx1    = dr3_fx1[x];
                const uint32_t  inv    = dr3_finv[x];
                const uint8_t * srow   = NULL;
                int             sy_cur = -1;
                const int       rs = lut->rs, gs = lut->gs, bs = lut->bs, as = lut->as;

                for (y = 0; y < dh; ++y) {
                    const int sy = dr3_sy[y];
                    uint32_t  r = 0, g = 0, b = 0;
                    const uint8_t *p;
                    const uint8_t *end;

                    if (sy != sy_cur) {          /* same source row -> same pixels, only re-base on change */
                        sy_cur = sy;
                        srow   = src + (size_t)sy * (size_t)src_pitch;
                    }

                    if ((sx1 - sx0) == 2) {
                        /* the common case (1.6:1 downscale): average two already converted pixels.
                           The packed average is exact for 8-bit channels and needs neither palette
                           lookups nor multiplications - the generic path cost 10 ms per frame. */
                        const uint32_t a = lutpx[srow[sx0]];
                        const uint32_t b = lutpx[srow[sx0 + 1]];

                        *drow-- = ((((a ^ b) & 0xFEFEFEFEu) >> 1) + (a & b)) | (0xFFu << as);
                        continue;
                    }

                    p   = srow + sx0;
                    end = srow + sx1;
                    while (p < end) {
                        const uint8_t idx = *p++;
                        r += pal->r[idx];
                        g += pal->g[idx];
                        b += pal->b[idx];
                    }

                    *drow-- = ((((r * inv) >> 16) << rs) | (((g * inv) >> 16) << gs) |
                               (((b * inv) >> 16) << bs) | (0xFFu << as));
                }
            }
        }
        else if ((sw == dw) && (sh == dh)) {
            /* native 1:1 400x240 race framebuffer: zero scaling, zero stretching, rotated direct copy */
            for (x = 0; x < dw; ++x) {
                uint32_t *      drow = target + (size_t)x * (size_t)dr3_fb_w + (dh - 1);
                const uint8_t * sptr = src + x;
                for (y = 0; y < dh; ++y) {
                    *drow-- = lutpx[*sptr];
                    sptr += src_pitch;
                }
            }
        }
        else if ((sw == 320) && (dw == 400) && (sh == dh)) {
            /* 320x240 VGA13 (cutscenes / info): 320 -> 400 columns, rows 1:1 */
            dr3_blit_rot_5_4(target, dr3_fb_w, dh, src, sw, src_pitch, lutpx);
        }
        else {
            for (x = 0; x < dw; ++x) {
                const int       sx     = dr3_sx[x];
                uint32_t *      drow   = target + (size_t)x * (size_t)dr3_fb_w + (dh - 1);
                const uint8_t * sptr   = src + sx;
                int             sy_cur = -1;

                for (y = 0; y < dh; ++y) {
                    const int sy = dr3_sy[y];
                    if (sy != sy_cur) {
                        sy_cur = sy;
                        sptr   = src + (size_t)sy * (size_t)src_pitch + sx;
                    }
                    *drow-- = lutpx[*sptr];      /* vertical stretch: same source pixel while sy is constant */
                }
            }
        }
        dr3_chaos_time(DR3_CT_BLIT, dr3_t_blit);
#if defined(DR3_CHAOS)
        /*
         * The cars of the chaos build are coloured here (dr3_chaos.h).  Only the race's own screen may
         * use the car mask - the mask is copied from BACKBUFFER together with that image - so the
         * present of any other buffer (the front end's 640x480, a leftover screen) leaves it alone.
         * The filtered path is the front end as well: it has no cars.
         */
        {
            extern unsigned char VGA13_ACTIVESCREEN_2[];
            extern unsigned char * VGA13_ACTIVESCREEN;   /* the game's own screen inside that buffer */
#if defined(__3DS__) && defined(DR3_USE_GFX) && !defined(DR3_HOST)
            extern unsigned char RACE400_ACTIVESCREEN[];
            const int             is_r400  = (sw == 400) && (sh == 240) && (src == (const uint8_t *)RACE400_ACTIVESCREEN);
#else
            const int             is_r400  = 0;
#endif
            const uint8_t * const vga13_lo = (const uint8_t *)VGA13_ACTIVESCREEN_2;
            const uint8_t * const vga13_hi = vga13_lo + 320 * 240;
            const int             is_race  = is_r400 || ((sw == 320) && (src >= vga13_lo) && (src < vga13_hi));
            int                   row_off  = 0;

            /*
             * Where the game's own screen sits inside what is presented.  Native 400x240 has row_off = 0.
             * Classic VGA13 (320x200 inside 320x240) starts 20 rows down.
             */
            if (!is_r400 && is_race && (src_pitch > 0) && ((const uint8_t *)VGA13_ACTIVESCREEN > src)) {
                const int off = (int)(((const uint8_t *)VGA13_ACTIVESCREEN - src) / src_pitch);

                if ((off > 0) && (off < DR3_CAR_MASK_H)) row_off = off;
            }

            dr3_fb_tint_log_present(src, sw, sh, src_pitch, is_race);
            if (!filter && is_race) {

                const unsigned long long dr3_t = dr3_chaos_tick();

                dr3_fb_tint_cars(target, src, sw, sh, src_pitch, dw, dh, row_off, lut);
                dr3_chaos_time(DR3_CT_TINT, dr3_t);
            }
        }
#endif
        if (target != fb) {
            /*
             * Pace hardware buffer swaps to the 3DS top screen's 60 Hz VBlank (~4,468,531 ARM11 ticks = 16.67 ms).
             * When a frame finishes faster than 16.67 ms (e.g. during the pre-race intro, standing start countdown,
             * or in the object-free Battle Arena where the engine's 70 Hz timer ticks every 14.28 ms), calling
             * gfxScreenSwapBuffers() twice before a hardware VBlank occurs would flip libctru's backbuffer pointer
             * back onto the live frontbuffer currently being scanned out to the LCD.
             */
            if (dr3_last_swap_tick != 0) {
                const u64 now_tick = svcGetSystemTick();
                if ((now_tick - dr3_last_swap_tick) < 4460000ull) {
                    gspWaitForVBlank();
                }
            }
            fb = (uint32_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, &h);
            if (!fb) return;
            dr3_fb = fb;
            memcpy(fb, target, (size_t)dr3_fb_w * (size_t)dr3_fb_h * sizeof(uint32_t));
        }
        dr3_prof_add(DR3_SLOT_BLIT, dr3_prof_us(blit_t0));
    }


    dr3_scenario_present(dr3_fb, dr3_fb_w, dr3_fb_h, lut);   /* frame counter + scripted screenshots */

    {
        const uint64_t           t      = dr3_prof_tick();
        const unsigned long long dr3_t  = dr3_chaos_tick();

        GSPGPU_FlushDataCache(dr3_fb, (u32)(dr3_fb_w * dr3_fb_h * 4));
        dr3_prof_add(DR3_SLOT_FLUSH, dr3_prof_us(t));
        dr3_chaos_time(DR3_CT_FLUSH, dr3_t);
    }
    {
        const uint64_t           t      = dr3_prof_tick();
        const unsigned long long dr3_t  = dr3_chaos_tick();

        gfxScreenSwapBuffers(GFX_TOP, false);
        dr3_last_swap_tick = svcGetSystemTick();
        dr3_prof_add(DR3_SLOT_SWAP, dr3_prof_us(t));
        dr3_chaos_time(DR3_CT_SWAP, dr3_t);
    }
}
