/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_blit.h"

#include <stddef.h>   /* size_t - MSVC pulls this in transitively, GCC does not */
#include <string.h>   /* memset - the histogram of the car tint's log line */

void dr3_palette_reset(dr3_palette_t *pal)
{
    int n;
    for (n = 0; n < 256; ++n) {
        pal->r[n] = (uint8_t)n;
        pal->g[n] = (uint8_t)n;
        pal->b[n] = (uint8_t)n;
    }
}

void dr3_palette_set(dr3_palette_t *pal, int index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index < 0 || index > 255) return;
    pal->r[index] = r;
    pal->g[index] = g;
    pal->b[index] = b;
}

/* Number of trailing zero bits of an SDL channel mask (0 for an empty mask). */
static int dr3_mask_shift(uint32_t mask)
{
    int shift = 0;
    if (!mask) return 0;
    while (!(mask & 1u)) {
        mask >>= 1;
        ++shift;
    }
    return shift;
}

void dr3_lut32_build_masks(dr3_lut32_t *lut, const dr3_palette_t *pal,
                           uint32_t r_mask, uint32_t g_mask, uint32_t b_mask, uint32_t a_mask)
{
    const int      rs = dr3_mask_shift(r_mask);
    const int      gs = dr3_mask_shift(g_mask);
    const int      bs = dr3_mask_shift(b_mask);
    const int      as = dr3_mask_shift(a_mask);
    const uint32_t a_bits = a_mask ? (0xFFu << as) : 0u;   /* no alpha bits -> no alpha value */
    int            n;

    lut->rs = rs;
    lut->gs = gs;
    lut->bs = bs;
    lut->as = as;

    for (n = 0; n < 256; ++n) {
        lut->px[n] = ((uint32_t)pal->r[n] << rs) |
                     ((uint32_t)pal->g[n] << gs) |
                     ((uint32_t)pal->b[n] << bs) |
                     a_bits;
    }
}

void dr3_lut32_build(dr3_lut32_t *lut, const dr3_palette_t *pal, int order, uint8_t alpha)
{
    /* memory order B,G,R,A  -> B in the low byte (SDL_PIXELFORMAT_ARGB8888 masks)
       memory order R,G,B,A  -> R in the low byte (SDL_PIXELFORMAT_ABGR8888 masks) */
    const uint32_t alpha_mask = (uint32_t)alpha << 24;

    if (order == DR3_LUT_RGB)
        dr3_lut32_build_masks(lut, pal, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, alpha_mask);
    else
        dr3_lut32_build_masks(lut, pal, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, alpha_mask);
}

static void dr3_fill32(uint32_t *dst, int dw, int dh, int pitch, uint32_t color)
{
    int x, y;
    for (y = 0; y < dh; ++y) {
        uint32_t *row = dst + (size_t)y * (size_t)pitch;
        for (x = 0; x < dw; ++x) row[x] = color;
    }
}

/* Box-filtered scaling: averages every source pixel that falls into a target pixel.  Keeps the
   game's dithered shading intact (plain nearest-neighbour turns it into vertical stripes when
   downscaling, e.g. the 640x480 VESA menu -> 400x240). */
int dr3_blit8_filter(const uint8_t *src, int sw, int sh, int src_pitch,
                     const dr3_palette_t *pal, const dr3_lut32_t *lut,
                     uint32_t *dst, int dw, int dh, int dst_pitch_px)
{
    int x, y;

    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return -1;

    for (y = 0; y < dh; ++y) {
        const int sy0 = (int)(((uint32_t)y * (uint32_t)sh) / (uint32_t)dh);
        int       sy1 = (int)(((uint32_t)(y + 1) * (uint32_t)sh) / (uint32_t)dh);
        uint32_t *drow = dst + (size_t)y * (size_t)dst_pitch_px;

        if (sy1 <= sy0) sy1 = sy0 + 1;

        for (x = 0; x < dw; ++x) {
            const int sx0 = (int)(((uint32_t)x * (uint32_t)sw) / (uint32_t)dw);
            int       sx1 = (int)(((uint32_t)(x + 1) * (uint32_t)sw) / (uint32_t)dw);
            uint32_t  r = 0, g = 0, b = 0, n = 0;
            int       sx, sy;

            if (sx1 <= sx0) sx1 = sx0 + 1;

            for (sy = sy0; sy < sy1 && sy < sh; ++sy) {
                const uint8_t *srow = src + (size_t)sy * (size_t)src_pitch;
                for (sx = sx0; sx < sx1 && sx < sw; ++sx) {
                    const uint8_t idx = srow[sx];
                    r += pal->r[idx];
                    g += pal->g[idx];
                    b += pal->b[idx];
                    ++n;
                }
            }

            if (!n) { drow[x] = 0; continue; }

            {
                /* multiply by a reciprocal instead of dividing: ARM11 has no fast integer divide
                   and this loop runs for every pixel of the 640x480 VESA menus */
                const uint32_t inv = 65536u / n;

                r = (r * inv) >> 16;
                g = (g * inv) >> 16;
                b = (b * inv) >> 16;
            }

            drow[x] = (r << lut->rs) | (g << lut->gs) | (b << lut->bs) | (0xFFu << lut->as);
        }
    }
    return 0;
}

int dr3_blit8_lut32(const uint8_t *src, int sw, int sh, int src_pitch,
                    const dr3_lut32_t *lut,
                    uint32_t *dst, int dw, int dh, int dst_pitch_px,
                    int mode, uint32_t clear)
{
    int x, y;

    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return -1;

    if (mode == DR3_SCALE_CENTER) {
        const int off_x = (dw - sw) / 2;
        const int off_y = (dh - sh) / 2;

        if (sw > dw || sh > dh) return -1;
        dr3_fill32(dst, dw, dh, dst_pitch_px, clear);

        for (y = 0; y < sh; ++y) {
            const uint8_t *srow = src + (size_t)y * (size_t)src_pitch;
            uint32_t      *drow = dst + (size_t)(y + off_y) * (size_t)dst_pitch_px + off_x;
            for (x = 0; x < sw; ++x) drow[x] = lut->px[srow[x]];
        }
        return 0;
    }

    for (y = 0; y < dh; ++y) {
        const int      sy   = (int)(((uint32_t)y * (uint32_t)sh) / (uint32_t)dh); /* nearest */
        const uint8_t *srow = src + (size_t)sy * (size_t)src_pitch;
        uint32_t      *drow = dst + (size_t)y * (size_t)dst_pitch_px;
        for (x = 0; x < dw; ++x) {
            const int sx = (int)(((uint32_t)x * (uint32_t)sw) / (uint32_t)dw);
            drow[x] = lut->px[srow[sx]];
        }
    }
    return 0;
}

void dr3_blit_rot_5_4(uint32_t *fb, int col_stride, int dh, const uint8_t *src, int sw, int src_pitch,
                      const uint32_t *lut)
{
    int k, y;

    for (k = 0; k < sw / 4; ++k) {

        const uint8_t *s  = src + 4 * k;
        uint32_t      *o0 = fb + (size_t)(5 * k) * (size_t)col_stride + (dh - 1);
        uint32_t      *o1 = o0 + col_stride;
        uint32_t      *o2 = o1 + col_stride;
        uint32_t      *o3 = o2 + col_stride;
        uint32_t      *o4 = o3 + col_stride;

        for (y = 0; y < dh; ++y) {

            const uint32_t a = lut[s[0]], b = lut[s[1]], c = lut[s[2]], d = lut[s[3]];

            *o0-- = a;  *o1-- = a;  *o2-- = b;  *o3-- = c;  *o4-- = d;
            s += src_pitch;
        }
    }
}

#if defined(__GNUC__)
typedef uint32_t __attribute__((may_alias)) dr3_u32_alias;      /* the mask is bytes, read here as words */
#else
typedef uint32_t dr3_u32_alias;
#endif

int dr3_carcolour_mask_bounds(const uint8_t *mask, int w, int h, int pitch, int box[4])
{
    const int aligned = (((uintptr_t)mask & 3u) == 0) && ((pitch & 3) == 0) && ((w & 3) == 0);
    int       r, any = 0;
    int       bx0 = w, by0 = h, bx1 = -1, by1 = -1;

    for (r = 0; r < h; ++r) {

        const uint8_t *row = mask + (size_t)r * (size_t)pitch;
        int            c0 = 0, c1 = w - 1;

        if (aligned) {

            const dr3_u32_alias *wp = (const dr3_u32_alias *)row;
            const int            nw = w / 4;
            int                  i = 0, j = nw - 1;

            while ((i < nw) && !wp[i]) ++i;
            if (i == nw) continue;                          /* an empty row: w / 4 word reads, no more */
            while (!wp[j]) --j;                             /* there is a marked word: j >= i          */

            c0 = 4 * i;      while (!row[c0]) ++c0;
            c1 = 4 * j + 3;  while (!row[c1]) --c1;
        }
        else {

            while ((c0 <= c1) && !row[c0]) ++c0;
            if (c0 > c1) continue;
            while ((c1 > c0) && !row[c1]) --c1;
        }

        any = 1;
        if (r < by0) by0 = r;
        if (r > by1) by1 = r;
        if (c0 < bx0) bx0 = c0;
        if (c1 > bx1) bx1 = c1;
    }

    if (any) { box[0] = bx0; box[1] = by0; box[2] = bx1; box[3] = by1; }
    return any;
}

/* ------------------------------------------------------------------ car colours --- */

void dr3_carcolour_build(uint32_t out[10], int r, int g, int b, const dr3_lut32_t *lut)
{
    int i;

    for (i = 0; i < 10; ++i) {

        /* the same arithmetic as ___4a064h_cdecl(): five darkened shades, then five lightened ones */
        double val[3];
        int    px[3], c;

        val[0] = (i < 5) ? (0.18 * i + 0.1) * (double)r
                         : 12.6 * (double)(i - 5) + (1.0 - 0.2 * (double)(i - 5)) * (double)r;
        val[1] = (i < 5) ? (0.18 * i + 0.1) * (double)g
                         : 12.6 * (double)(i - 5) + (1.0 - 0.2 * (double)(i - 5)) * (double)g;
        val[2] = (i < 5) ? (0.18 * i + 0.1) * (double)b
                         : 12.6 * (double)(i - 5) + (1.0 - 0.2 * (double)(i - 5)) * (double)b;

        for (c = 0; c < 3; ++c) {

            px[c] = (int)val[c];
            if (px[c] < 0)   px[c] = 0;
            if (px[c] > 255) px[c] = 255;
        }

        out[i] = ((uint32_t)px[0] << lut->rs) | ((uint32_t)px[1] << lut->gs) |
                 ((uint32_t)px[2] << lut->bs) | (0xFFu << lut->as);
    }
}

/*
 * The same ramp, built the way the engine builds it.  ___4a064h_cdecl writes its two halves into 6-bit
 * VGA DAC entries, so the "+12.6 * n" of the light half saturates the entry at 63 - white - and that
 * is what makes the shine on a car.  A colour that arrives in that 6-bit space (carcol.pal, the bytes
 * at s_54 +0x34) therefore has to be turned into screen values *after* the ramp is built, exactly like
 * a palette entry is: (v<<2)|(v>>4) (__DISPLAY_SET_PALETTE_COLOR, drally_linux_c.c).  Building it from
 * an already expanded colour keeps 12.6 * n tiny - the shine comes out grey and the whole car flat,
 * which is what the twenty car colours looked like before this.
 */
void dr3_carcolour_build_vga6(uint32_t out[10], int r, int g, int b, const dr3_lut32_t *lut)
{
    int i;

    for (i = 0; i < 10; ++i) {

        double val[3];
        int    px[3], c;

        val[0] = (i < 5) ? (0.18 * i + 0.1) * (double)r
                         : 12.6 * (double)(i - 5) + (1.0 - 0.2 * (double)(i - 5)) * (double)r;
        val[1] = (i < 5) ? (0.18 * i + 0.1) * (double)g
                         : 12.6 * (double)(i - 5) + (1.0 - 0.2 * (double)(i - 5)) * (double)g;
        val[2] = (i < 5) ? (0.18 * i + 0.1) * (double)b
                         : 12.6 * (double)(i - 5) + (1.0 - 0.2 * (double)(i - 5)) * (double)b;

        for (c = 0; c < 3; ++c) {

            int v = (int)val[c];

            if (v < 0)  v = 0;
            if (v > 63) v = 63;                  /* the entry the engine writes into is 6 bits */
            px[c] = (v << 2) | (v >> 4);         /* ... and this is how the port shows it      */
        }

        out[i] = ((uint32_t)px[0] << lut->rs) | ((uint32_t)px[1] << lut->gs) |
                 ((uint32_t)px[2] << lut->bs) | (0xFFu << lut->as);
    }
}

#define DR3_TINT_INV_MAX 2048           /* widest source the row walk handles (the engine's screen is 320) */

int dr3_carcolour_tint_mask(uint32_t *dst, int dw, int dh, int dst_stride_x, int dst_stride_y,
                            const uint8_t *src, int sw, int sh, int src_pitch,
                            const uint8_t *mask, int mask_w, int mask_h, int mask_pitch,
                            int mask_row_off, const dr3_carcolour_area_t *area,
                            const uint32_t *shades, int n_cars)
{
    int x, y, n = 0;
    int xlo, xhi, ylo, yhi;

    if (!dst || !src || !mask || !shades || (n_cars <= 0)) return 0;
    if ((dw <= 0) || (dh <= 0) || (sw <= 0) || (sh <= 0)) return 0;
    if (area && (area->x1 < area->x0)) return 0;          /* the frame has no car pixels */

    /*
     * Only the columns and rows of the box are walked.  The old loop visited all 240 rows of every column that
     * is inside the box and threw 5/6 of them away one by one (1.7 ms of the 14 ms frame).  The maps never
     * decrease, so the first and last target column/row that can land in the box are found from the ends -
     * the checks inside the loops stay as they were, this only shortens the loops.
     */
    xlo = 0; xhi = dw - 1; ylo = 0; yhi = dh - 1;
    if (area && area->map_x) {
        while ((xlo <= xhi) && ((int)area->map_x[xlo] < area->x0)) ++xlo;
        while ((xhi >= xlo) && ((int)area->map_x[xhi] > area->x1)) --xhi;
    }
    if (area && area->map_y) {
        while ((ylo <= yhi) && (((int)area->map_y[ylo] - mask_row_off) < area->y0)) ++ylo;
        while ((yhi >= ylo) && (((int)area->map_y[yhi] - mask_row_off) > area->y1)) --yhi;
    }

    /*
     * The common case - rows 1:1 (the race screen: 240 -> 240) - walks the MASK, not the target: for every row of
     * the box the marked pixels are found (empty words are skipped four bytes at a time) and each one is written
     * to the one or two target columns its source column covers.  A bunched pack of twenty cars has a box that is
     * most of the screen, but only a fraction of the box is car - walking every target pixel of it cost 7 ms of
     * a 29 ms frame.  The image (and the count) is the same as the general walk below gives.
     */
    if (area && area->map_x && area->map_y && (dh == sh) && (sw <= DR3_TINT_INV_MAX) &&
        (area->map_y[0] == 0) && (area->map_y[dh - 1] == dh - 1)) {

        static uint16_t inv_first[DR3_TINT_INV_MAX], inv_cnt[DR3_TINT_INV_MAX];
        const int       aligned = (((uintptr_t)mask & 3u) == 0) && ((mask_pitch & 3) == 0);
        int             sx, my;

        memset(inv_cnt, 0, sizeof(uint16_t) * (size_t)sw);
        for (x = 0; x < dw; ++x) {

            const int s = (int)area->map_x[x];

            if (s >= sw) continue;
            if (!inv_cnt[s]) inv_first[s] = (uint16_t)x;
            ++inv_cnt[s];
        }

        for (my = area->y0; my <= area->y1; ++my) {

            const int      sy = my + mask_row_off;
            const uint8_t *mrow, *srow;

            if ((my < 0) || (my >= mask_h) || (sy < 0) || (sy >= sh)) continue;

            mrow = mask + (size_t)my * (size_t)mask_pitch;
            srow = src + (size_t)sy * (size_t)src_pitch;

            for (sx = area->x0; sx <= area->x1; ++sx) {

                uint8_t m, v;

                if (aligned && !(sx & 3) && ((sx + 3) <= area->x1) && !*(const dr3_u32_alias *)(mrow + sx)) { sx += 3; continue; }
                if (sx >= mask_w) break;

                m = mrow[sx];
                if (!m || (m > n_cars)) continue;

                v = srow[sx];
                if ((v < DR3_CARCOLOUR_FIRST) || (v > DR3_CARCOLOUR_LAST)) continue;

                {
                    const uint32_t col = shades[(m - 1) * 10 + (v - DR3_CARCOLOUR_FIRST)];
                    int            k;

                    for (k = 0; k < (int)inv_cnt[sx]; ++k)
                        dst[(ptrdiff_t)dst_stride_x * (inv_first[sx] + k) + (ptrdiff_t)dst_stride_y * sy] = col;
                    n += (int)inv_cnt[sx];
                }
            }
        }

        return n;
    }

    for (x = xlo; x <= xhi; ++x) {

        /* the blit's own mapping, precomputed when the caller has it (area->map_x) */
        const int sx = area && area->map_x ? (int)area->map_x[x]
                                           : (int)(((uint32_t)x * (uint32_t)sw) / (uint32_t)dw);

        if (sx >= mask_w) continue;
        if (area && ((sx < area->x0) || (sx > area->x1))) continue;

        for (y = ylo; y <= yhi; ++y) {

            const int sy = area && area->map_y ? (int)area->map_y[y]
                                               : (int)(((uint32_t)y * (uint32_t)sh) / (uint32_t)dh);
            const int my = sy - mask_row_off;   /* the mask holds the game's own screen */
            uint8_t   m;

            /*
             * The presented buffer can be taller than the game's screen (VGA13 lives inside a 320x240
             * surface, 20 rows down) and the mask only covers the game's own rows - so the row has to
             * be translated before the mask is read, and the bounds are checked before that read.
             */
            if ((my < 0) || (my >= mask_h)) continue;
            if (area && ((my < area->y0) || (my > area->y1))) continue;

            m = mask[(size_t)my * (size_t)mask_pitch + (size_t)sx];
            if (!m || (m > n_cars)) continue;

            {
                const uint8_t v = src[(size_t)sy * (size_t)src_pitch + (size_t)sx];

                if ((v < DR3_CARCOLOUR_FIRST) || (v > DR3_CARCOLOUR_LAST)) continue;

                dst[(ptrdiff_t)dst_stride_x * x + (ptrdiff_t)dst_stride_y * y] =
                    shades[(m - 1) * 10 + (v - DR3_CARCOLOUR_FIRST)];
                ++n;
            }
        }
    }

    return n;
}

/*
 * May the health line of a car be drawn?  It is a *portable* function on purpose (dr3_blit.h has the
 * threshold, DR3_CHAOS_HP_LINE_BELOW), so it sits outside the DR3_CHAOS block the tint pass below lives in:
 * the host tests build this file without DR3_CHAOS and check the two boundaries (tests/test_dr3.c).
 *
 * hp_now is s_94[car].__18 as the race has it now, hp_max the value the race set up for that car
 * (dr3_chaos_hp_max).  A whole car and a wreck are not drawn: the line only says something while a car is
 * damaged, and "0 HP" is a wreck that the game shows as a burning wreck anyway.  The *player's* own car is
 * the third case that gets no line: the bar is there to read the *other* cars, and his own damage is what
 * the game's own HUD shows him - the DAMAGE panel (s_54[n].__10, race___400ech.c), the smoke over his car
 * and the warning at low HP (race___40864h.c).
 */
int dr3_carcolour_hp_line_shown(int hp_now, int hp_max, int own_car)
{
    if (own_car) return 0;                              /* the player's own car - the HUD's DAMAGE panel has it */

    if (hp_max <= 0) return 0;                          /* nothing to scale to */
    if (hp_now <= 0) return 0;                          /* a wreck - no line   */

    return ((hp_now * 100 / hp_max) < DR3_CHAOS_HP_LINE_BELOW);
}

/* ------------------------------------------------- the cars of a frame, coloured --- */

#if defined(DR3_CHAOS)

#include "dr3_chaos.h"     /* dr3_chaos_car_screen / dr3_chaos_car_col / the HP line - the cars of the frame */
#include "dr3_dm.h"        /* the sudden death banner of a deathmatch (drally.h: DR3_CHAOS_MODE_DM) */
#include "dr3_arena.h"     /* the closing ring banner of the arena (drally.h: DR3_CHAOS_MODE_ARENA) */
#include "dr3_boss.h"      /* 3-phase Final Boss Gauntlet banners & Phase 3 Boss HP bar */
#include "dr3_anniv_prog.h" /* 30th Anniversary in-race radio & rival dialogue overlay */
#include "dr3_log.h"


/* the cars of the running race (s_94, 0x94 bytes each) - the HP of a car sits at +0x18 (dr3_chaos.h) */
extern unsigned char ___1de580h[];

/* the game's own font: 64 glyphs of 6x6 pixels, one 0x24 bytes, for the characters from 0x20 */
extern unsigned char ___1d7810h[];

/*
 * The sudden death banner of a deathmatch (platform_3ds/dr3_dm.h, drally.h: DR3_CHAOS_MODE_DM).
 *
 * The text is set in the game's own font (___1d7810h - 6x6 pixels per glyph, the characters from 0x20) and
 * scanned out at DR3_DM_BANNER_SCALE: 3 means 18 pixels tall letters, in two lines so they stay between the
 * two HUD columns of the race screen (dr3_dm.h says why).  Every ink pixel of a glyph becomes a scale x
 * scale block; each line is written eight times one pixel around itself in an outline colour and then once
 * in the ink colour, which is what gives a banner its fat, outlined look.
 *
 * Drawn into the port's own image, exactly like the health line above, so the 3DS and the PC show the very
 * same banner and no entry of the game's palette has to be kept free for it.  The coordinates are the
 * game's own (320x200), mapped into the target the way the car mask is.
 */
#define DR3_DM_BANNER_Y      18      /* the row of the game's screen the first line starts at */
#define DR3_DM_BANNER_LINE_H 20      /* the rows between the two lines (18 px letters + a gap) */
#define DR3_DM_BANNER_INK    ((uint32_t)0xff << 16 | (uint32_t)0xe6 << 8 | (uint32_t)0x46)

static void dr3_dm_banner_text(uint32_t *dst, int dw, int dh, int dst_stride_x, int dst_stride_y,
                               int sw, int sh, int row_off, uint32_t col,
                               int x0, int y0, const char *text, int scale)
{
    int k, i, j;

    for (k = 0; text[k]; ++k) {

        const int g  = dr3_dm_glyph_index(text[k]);
        const int gx = x0 + k * DR3_DM_GLYPH_ADVANCE * scale;

        if (g < 0) continue;                            /* a space - the font has no glyph for it */

        for (j = 0; j < DR3_DM_GLYPH_H; ++j) {
            for (i = 0; i < DR3_DM_GLYPH_W; ++i) {

                int tx0, tx1, ty0, ty1, bx, by;

                /* SMALFO4A.BPK stores the lit letter strokes in 0x0a..0x0e and its built-in drop shadow in
                   0x05..0x08; skipping the shadow keeps letters crisp instead of turning them into blocks. */
                if (___1d7810h[DR3_DM_GLYPH_BYTES * g + DR3_DM_GLYPH_W * j + i] < 0x0a) continue;

                /* the glyph pixel as a block of target pixels, clipped and written straight */
                tx0 = (gx + i * scale) * dw / sw;
                tx1 = (gx + (i + 1) * scale - 1) * dw / sw;
                ty0 = ((y0 + j * scale) + row_off) * dh / sh;
                ty1 = (((y0 + (j + 1) * scale) - 1) + row_off) * dh / sh;

                for (by = ty0; by <= ty1; ++by) {

                    if ((by < 0) || (by >= dh)) continue;

                    for (bx = tx0; bx <= tx1; ++bx) {

                        if ((bx < 0) || (bx >= dw)) continue;

                        dst[(ptrdiff_t)dst_stride_x * bx + (ptrdiff_t)dst_stride_y * by] = col;
                    }
                }
            }
        }
    }
}

static void dr3_banner_lines(uint32_t *dst, int dw, int dh, int dst_stride_x, int dst_stride_y,
                             int sw, int sh, int row_off, const char *l0, const char *l1, uint32_t ink, uint32_t dark);

static void dr3_dm_banner(uint32_t *dst, int dw, int dh, int dst_stride_x, int dst_stride_y,
                          int sw, int sh, int row_off, const dr3_lut32_t *lut)
{
    const unsigned long long ms = dr3_dm_banner_ms();
    static int               logged;                    /* the "it is drawn" line: once per process */
    const char *             lines[2];
    const uint32_t           ink  = ((DR3_DM_BANNER_INK >> 16) & 0xff) << lut->rs
                                  | ((DR3_DM_BANNER_INK >>  8) & 0xff) << lut->gs
                                  | ((DR3_DM_BANNER_INK >>  0) & 0xff) << lut->bs
                                  | ((uint32_t)0xff << lut->as);
    const uint32_t           dark = (uint32_t)0xff << lut->as;   /* opaque black */
    int                      l, dx, dy;

    /*
     * The race is over (dr3_chaos.h: `___243d14h`): the RACE OVER screen and its fade to black follow, and
     * no banner of the port belongs over that - whatever ended the race.
     */
    if (dr3_chaos_race_over()) return;

    if (!dr3_dm_banner_visible(ms, dr3_dm_sudden_s())) return;

    lines[0] = DR3_DM_BANNER_LINE1;
    lines[1] = DR3_DM_BANNER_LINE2;

    if (!logged && dr3_log_enabled()) {

        logged = 1;
        dr3_log("[dr3] chaos: deathmatch: the '%s %s' banner is drawn (%d and %d game pixel(s) wide, scale %d, "
                "row %d of the %dx%d screen, race age %d s)",
                lines[0], lines[1], dr3_dm_text_width(lines[0], DR3_DM_BANNER_SCALE),
                dr3_dm_text_width(lines[1], DR3_DM_BANNER_SCALE), DR3_DM_BANNER_SCALE,
                DR3_DM_BANNER_Y, sw, sh, (int)(ms / 1000ull));
    }

    dr3_banner_lines(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off, lines[0], lines[1], ink, dark);
}

/* the two lines of a banner: outlined, centred, in the game's own font (the sudden death's and the arena's ring banner) */
static void dr3_banner_lines(uint32_t *dst, int dw, int dh, int dst_stride_x, int dst_stride_y,
                             int sw, int sh, int row_off, const char *l0, const char *l1, uint32_t ink, uint32_t dark)
{
    const char *lines[2];
    int         l, dx, dy;

    lines[0] = l0;
    lines[1] = l1;

    for (l = 0; l < 2; ++l) {

        const int x0 = (sw - dr3_dm_text_width(lines[l], DR3_DM_BANNER_SCALE)) / 2;
        const int y0 = DR3_DM_BANNER_Y + l * DR3_DM_BANNER_LINE_H;

        /* the outline first - the line written around itself - then the letters on top of it */
        for (dy = -1; dy <= 1; ++dy) {
            for (dx = -1; dx <= 1; ++dx) {

                if (!dx && !dy) continue;

                dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                                   dark, x0 + dx, y0 + dy, lines[l], DR3_DM_BANNER_SCALE);
            }
        }

        dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                           ink, x0, y0, lines[l], DR3_DM_BANNER_SCALE);
    }
}

/*
 * The closing ring banner of the arena (platform_3ds/dr3_arena.h): "THE RING / CLOSES" in yellow when the ring
 * starts to shrink, and "GET INSIDE / THE RING" (blinking, red) while the player's own car is outside it.
 * Centred in the race viewport (x = 64..319, right of the 64 px left HUD column) so the wider warning stays off
 * the HUD.
 */
#define DR3_ARENA_BANNER_WARN_INK   ((uint32_t)0xff << 16 | (uint32_t)0x38 << 8 | (uint32_t)0x30)
#define DR3_ARENA_BANNER_GRUDGE_INK ((uint32_t)0xff << 16 | (uint32_t)0x5c << 8 | (uint32_t)0x20)

static void dr3_arena_ring_banner(uint32_t *dst, int dw, int dh, int dst_stride_x, int dst_stride_y,
                                  int sw, int sh, int row_off, const dr3_lut32_t *lut)
{
    extern int   CURRENT_VIEWPORT_X;
    static int   logged_mask;
    const char * lines[2];
    int          kind, l, dx, dy, scale, line_h;
    uint32_t     rgb, ink, dark;
    const int    vp_x = ((CURRENT_VIEWPORT_X > 0) && (CURRENT_VIEWPORT_X < sw)) ? CURRENT_VIEWPORT_X : 0;
    const int    vp_w = sw - vp_x;

    if (dr3_chaos_race_over()) { logged_mask = 0; return; }

    kind = dr3_arena_banner_now(&lines[0], &lines[1]);
    if (!kind) return;

    rgb  = (kind == 2) ? DR3_ARENA_BANNER_WARN_INK
         : (kind == 3) ? DR3_ARENA_BANNER_GRUDGE_INK
         : DR3_DM_BANNER_INK;
    ink  = ((rgb >> 16) & 0xff) << lut->rs
         | ((rgb >>  8) & 0xff) << lut->gs
         | ((rgb >>  0) & 0xff) << lut->bs
         | ((uint32_t)0xff << lut->as);
    dark = (uint32_t)0xff << lut->as;

    scale = DR3_DM_BANNER_SCALE;
    if ((dr3_dm_text_width(lines[0], scale) > vp_w - 8) || (dr3_dm_text_width(lines[1], scale) > vp_w - 8)) {
        scale = 2;
    }
    line_h = (scale == DR3_DM_BANNER_SCALE) ? DR3_DM_BANNER_LINE_H : (DR3_DM_GLYPH_H * scale + 5);

    if (!(logged_mask & (1 << kind)) && dr3_log_enabled()) {

        logged_mask |= (1 << kind);
        dr3_log("[dr3] chaos: arena: banner '%s %s' drawn (kind %d, scale %d, widths %d and %d px)",
                lines[0], lines[1], kind, scale,
                dr3_dm_text_width(lines[0], scale),
                dr3_dm_text_width(lines[1], scale));
    }

    for (l = 0; l < 2; ++l) {

        const int x0 = vp_x + (vp_w - dr3_dm_text_width(lines[l], scale)) / 2;
        const int y0 = DR3_DM_BANNER_Y + l * line_h;

        for (dy = -1; dy <= 1; ++dy) {
            for (dx = -1; dx <= 1; ++dx) {

                if (!dx && !dy) continue;

                dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                                   dark, x0 + dx, y0 + dy, lines[l], scale);
            }
        }

        dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                           ink, x0, y0, lines[l], scale);
    }
}

static void dr3_boss_overlay(uint32_t *dst, int dw, int dh, int dst_stride_x, int dst_stride_y,
                             int sw, int sh, int row_off, const dr3_lut32_t *lut)
{
    extern int  CURRENT_VIEWPORT_X;
    const char *lines[2];
    const int   vp_x = ((CURRENT_VIEWPORT_X > 0) && (CURRENT_VIEWPORT_X < sw)) ? CURRENT_VIEWPORT_X : 0;
    const int   vp_w = sw - vp_x;
    int         hp_now = 0, hp_max = 0, enraged = 0;

    if (dr3_chaos_race_over()) return;

    /* White-gold detonation shockwave flash when the Giant Adversary explodes */
    {
        const int flash = dr3_boss_death_flash();
        if (flash > 0) {
            const int tx0 = (vp_x * dw) / sw;
            int bx, by;
            for (by = 0; by < dh; ++by) {
                for (bx = tx0; bx < dw; ++bx) {
                    uint32_t *p = &dst[(ptrdiff_t)dst_stride_x * bx + (ptrdiff_t)dst_stride_y * by];
                    const uint32_t c = *p;
                    int r = (int)((c >> lut->rs) & 0xffu);
                    int g = (int)((c >> lut->gs) & 0xffu);
                    int b = (int)((c >> lut->bs) & 0xffu);
                    r = r + ((255 - r) * flash) / 255;
                    g = g + ((242 - g) * flash) / 255;
                    b = b + ((190 - b) * flash) / 255;
                    *p = ((uint32_t)r << lut->rs) | ((uint32_t)g << lut->gs)
                       | ((uint32_t)b << lut->bs) | ((uint32_t)0xff << lut->as);
                }
            }
        }
    }

    /* Phase 3 dedicated Giant Boss HP Bar across the top of the race viewport */
    if (dr3_boss_hud_bar(&hp_now, &hp_max, &enraged) && (hp_max > 0)) {
        const uint32_t dark   = (uint32_t)0xff << lut->as;
        const uint32_t border = ((uint32_t)0xd8 << lut->rs) | ((uint32_t)0xc8 << lut->gs) | ((uint32_t)0x90 << lut->bs) | ((uint32_t)0xff << lut->as);
        const uint32_t bg     = ((uint32_t)0x28 << lut->rs) | ((uint32_t)0x08 << lut->gs) | ((uint32_t)0x08 << lut->bs) | ((uint32_t)0xff << lut->as);
        const uint32_t fill   = enraged
                              ? (((uint32_t)0xff << lut->rs) | ((uint32_t)0x24 << lut->gs) | ((uint32_t)0x18 << lut->bs) | ((uint32_t)0xff << lut->as))
                              : (((uint32_t)0xd0 << lut->rs) | ((uint32_t)0x20 << lut->gs) | ((uint32_t)0xff << lut->bs) | ((uint32_t)0xff << lut->as));
        const char    *lbl    = enraged ? "ADVERSARY ENRAGED!" : "ADVERSARY";
        const int      lw     = dr3_dm_text_width(lbl, 1);
        const int      bar_w  = 180;
        const int      bar_h  = 7;
        const int      gx0    = vp_x + (vp_w - bar_w) / 2;
        const int      gy0    = 11;
        const int      tx0    = (gx0 * dw) / sw;
        const int      tx1    = ((gx0 + bar_w) * dw) / sw;
        const int      ty0    = ((gy0 + row_off) * dh) / sh;
        const int      ty1    = ((gy0 + bar_h + row_off) * dh) / sh;
        const int      fill_x = tx0 + 1 + ((tx1 - tx0 - 2) * hp_now) / hp_max;
        int            bx, by;

        dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                           dark, vp_x + (vp_w - lw) / 2 + 1, 4, lbl, 1);
        dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                           border, vp_x + (vp_w - lw) / 2, 3, lbl, 1);

        for (by = ty0; by <= ty1; ++by) {
            if ((by < 0) || (by >= dh)) continue;
            for (bx = tx0; bx <= tx1; ++bx) {
                uint32_t c;
                if ((bx < 0) || (bx >= dw)) continue;
                if ((bx == tx0) || (bx == tx1) || (by == ty0) || (by == ty1)) c = border;
                else if (bx < fill_x) c = fill;
                else c = bg;
                dst[(ptrdiff_t)dst_stride_x * bx + (ptrdiff_t)dst_stride_y * by] = c;
            }
        }
    }

    /* Phase opening & Enrage banners */
    {
        const int kind = dr3_boss_banner_now(&lines[0], &lines[1]);

        if (kind) {
            const uint32_t rgb  = (kind == 2) ? DR3_ARENA_BANNER_WARN_INK : DR3_DM_BANNER_INK;
            const uint32_t ink  = ((rgb >> 16) & 0xff) << lut->rs
                                | ((rgb >>  8) & 0xff) << lut->gs
                                | ((rgb >>  0) & 0xff) << lut->bs
                                | ((uint32_t)0xff << lut->as);
            const uint32_t dark = (uint32_t)0xff << lut->as;
            const int      y_base = dr3_boss_is_final_duel() ? 26 : DR3_DM_BANNER_Y;
            int            l, dx, dy;

            for (l = 0; l < 2; ++l) {
                const int x0 = vp_x + (vp_w - dr3_dm_text_width(lines[l], DR3_DM_BANNER_SCALE)) / 2;
                const int y0 = y_base + l * DR3_DM_BANNER_LINE_H;

                for (dy = -1; dy <= 1; ++dy) {
                    for (dx = -1; dx <= 1; ++dx) {
                        if (!dx && !dy) continue;
                        dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                                           dark, x0 + dx, y0 + dy, lines[l], DR3_DM_BANNER_SCALE);
                    }
                }
                dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                                   ink, x0, y0, lines[l], DR3_DM_BANNER_SCALE);
            }
        }
    }
}


/* the player's own car: race_main.c writes it, bss.c holds it as a plain value (not an array) */
extern int MY_CAR_IDX;

/*
 * The index of the player's own car, or -1 when there is none (a frame of a screen that is not a race).
 * Same read and same guard as dr3_bottom.c and dr3_host.c use: a stale value from a race that is over must
 * not name a car.
 */
static int dr3_own_car(void)
{
    return ((MY_CAR_IDX >= 0) && (MY_CAR_IDX < DR3_MAX_CARS)) ? (int)MY_CAR_IDX : -1;
}

/*
 * The mapping the tint colours through: the nearest-neighbour one of dr3_blit8_lut32() (target x covers
 * source column x*sw/dw).  Built once per size - the 96.000 pixels of a frame are what made a division
 * per pixel worth a table on the 3DS, and the front end (640x480, filtered, no cars) never gets here.
 */
#define DR3_TINT_MAP_MAX 2048   /* the 3DS writes 400 columns, a PC window can be wider */
static int      dr3_tint_map_sw = -1;
static int      dr3_tint_map_sh = -1;
static int      dr3_tint_map_dw = -1;
static int      dr3_tint_map_dh = -1;
static uint16_t dr3_tint_sx[DR3_TINT_MAP_MAX];
static uint16_t dr3_tint_sy[DR3_TINT_MAP_MAX];
static uint16_t dr3_tint_inv_first[DR3_TINT_INV_MAX];
static uint16_t dr3_tint_inv_cnt[DR3_TINT_INV_MAX];

static int           dr3_shades_valid;
static int           dr3_shades_rs = -1, dr3_shades_gs = -1, dr3_shades_bs = -1, dr3_shades_as = -1;
static int           dr3_fade_bright = 256, dr3_fade_color = 256, dr3_fade_flash = 0;
static int           dr3_shades_bright = -1, dr3_shades_color = -1, dr3_shades_flash = -1, dr3_shades_my_car = -2;
static unsigned char dr3_shades_col[DR3_MAX_CARS][3];
static uint32_t      dr3_shades_cache[DR3_MAX_CARS * 10];

void dr3_carcolour_set_fade(int brightness_256, int color_256, int player_flash)
{
    if (brightness_256 < 0)   brightness_256 = 0;
    if (brightness_256 > 256) brightness_256 = 256;
    if (color_256 < 0)        color_256 = 0;
    if (color_256 > 256)      color_256 = 256;
    if (player_flash < 0)     player_flash = 0;
    if (player_flash > 256)   player_flash = 256;

    dr3_fade_bright = brightness_256;
    dr3_fade_color  = color_256;
    dr3_fade_flash  = player_flash;
}

static void dr3_carcolour_build_vga6_fade(uint32_t out[10], int r, int g, int b, const dr3_lut32_t *lut,
                                          int bright_256, int color_256, int flash_256)
{
    int i;

    if ((bright_256 >= 256) && (color_256 >= 256) && (flash_256 <= 0)) {
        dr3_carcolour_build_vga6(out, r, g, b, lut);
        return;
    }

    for (i = 0; i < 10; ++i) {

        double val[3];
        int    px[3], c;

        val[0] = (i < 5) ? (0.18 * i + 0.1) * (double)r
                         : 12.6 * (double)(i - 5) + (1.0 - 0.2 * (double)(i - 5)) * (double)r;
        val[1] = (i < 5) ? (0.18 * i + 0.1) * (double)g
                         : 12.6 * (double)(i - 5) + (1.0 - 0.2 * (double)(i - 5)) * (double)g;
        val[2] = (i < 5) ? (0.18 * i + 0.1) * (double)b
                         : 12.6 * (double)(i - 5) + (1.0 - 0.2 * (double)(i - 5)) * (double)b;

        for (c = 0; c < 3; ++c) {
            int v = (int)val[c];
            if (v < 0)  v = 0;
            if (v > 63) v = 63;
            px[c] = v;
        }

        if (color_256 < 256) {
            const int lum = (299 * px[0] + 588 * px[1] + 113 * px[2]) / 1000;
            for (c = 0; c < 3; ++c)
                px[c] = (px[c] * color_256 + lum * (256 - color_256) + 128) >> 8;
        }

        if (flash_256 > 0) {
            const int keep = 256 - flash_256;
            for (c = 0; c < 3; ++c) {
                int v = (keep * px[c] + (17 * flash_256 * px[c]) / 10 + 128) >> 8;
                if (v > 63) v = 63;
                px[c] = v;
            }
        }

        if (bright_256 < 256) {
            for (c = 0; c < 3; ++c)
                px[c] = (px[c] * bright_256) >> 8;
        }

        for (c = 0; c < 3; ++c) {
            int v = px[c];
            if (v < 0)  v = 0;
            if (v > 63) v = 63;
            px[c] = (v << 2) | (v >> 4);
        }

        out[i] = ((uint32_t)px[0] << lut->rs) | ((uint32_t)px[1] << lut->gs) |
                 ((uint32_t)px[2] << lut->bs) | (0xFFu << lut->as);
    }
}

int dr3_carcolour_tint_cars(uint32_t *dst, int dw, int dh, int dst_stride_x, int dst_stride_y,
                            const uint8_t *src, int sw, int sh, int src_pitch, int row_off,
                            const dr3_lut32_t *lut)
{
    static int dr3_tint_logged;
    static int hp_box[DR3_MAX_CARS][4];
    dr3_carcolour_area_t area;
    const int  my_car = dr3_own_car();
    int        i, n = 0, x, y, any = 0, hp_box_done = 0;
    int        bx0 = DR3_CAR_MASK_W, by0 = DR3_CAR_MASK_H, bx1 = -1, by1 = -1, r;

    if ((dw <= 0) || (dh <= 0) || (sw <= 0) || (sh <= 0)) return 0;
    if ((dw > DR3_TINT_MAP_MAX) || (dh > DR3_TINT_MAP_MAX)) return 0;

    if ((dr3_tint_map_sw != sw) || (dr3_tint_map_sh != sh) ||
        (dr3_tint_map_dw != dw) || (dr3_tint_map_dh != dh)) {

        for (x = 0; x < dw; ++x) dr3_tint_sx[x] = (uint16_t)(((uint32_t)x * (uint32_t)sw) / (uint32_t)dw);
        for (y = 0; y < dh; ++y) dr3_tint_sy[y] = (uint16_t)(((uint32_t)y * (uint32_t)sh) / (uint32_t)dh);

        if (sw <= DR3_TINT_INV_MAX) {
            memset(dr3_tint_inv_cnt, 0, sizeof(uint16_t) * (size_t)sw);
            for (x = 0; x < dw; ++x) {
                const int s = (int)dr3_tint_sx[x];

                if (s >= sw) continue;
                if (!dr3_tint_inv_cnt[s]) dr3_tint_inv_first[s] = (uint16_t)x;
                ++dr3_tint_inv_cnt[s];
            }
        }

        dr3_tint_map_sw = sw;
        dr3_tint_map_sh = sh;
        dr3_tint_map_dw = dw;
        dr3_tint_map_dh = dh;
    }

    const unsigned long long dr3_t_scan = dr3_chaos_tick();

    /*
     * Where the marks of this frame are.  The whole screen only holds car pixels in a small part of it,
     * and walking all 96.000 target pixels for a handful of cars cost 27 ms of every 52 ms frame on the
     * 3DS - more than everything else the port did.  A row-wise scan of the mask finds the box the marks
     * live in (the mask is 64.000 bytes, this is a fraction of a millisecond) and the tint then only
     * looks there.  An empty mask comes out as "nothing to do" and skips the pass entirely.
     */
    {
        int box[4];

        any = dr3_carcolour_mask_bounds(dr3_chaos_car_screen, DR3_CAR_MASK_W, DR3_CAR_MASK_H, DR3_CAR_MASK_W, box);
        if (any) {
            bx0 = box[0]; by0 = box[1]; bx1 = box[2]; by1 = box[3];
        }
    }

    dr3_chaos_time(DR3_CT_TINT_SCAN, dr3_t_scan);

    if (!any) {
        dr3_dm_banner(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off, lut);
        dr3_arena_ring_banner(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off, lut);
        dr3_boss_overlay(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off, lut);
        return 0;
    }

    area.map_x = dr3_tint_sx;
    area.map_y = dr3_tint_sy;
    area.x0    = bx0;
    area.y0    = by0;
    area.x1    = bx1;
    area.y1    = by1;

    const unsigned long long dr3_t_shades = dr3_chaos_tick();

    {
        const int lut_changed = !dr3_shades_valid || (dr3_shades_rs != lut->rs) ||
                                (dr3_shades_gs != lut->gs) || (dr3_shades_bs != lut->bs) ||
                                (dr3_shades_as != lut->as) || (dr3_shades_bright != dr3_fade_bright) ||
                                (dr3_shades_color != dr3_fade_color) || (dr3_shades_flash != dr3_fade_flash) ||
                                (dr3_shades_my_car != my_car);

        if (lut_changed) {
            dr3_shades_rs = lut->rs;
            dr3_shades_gs = lut->gs;
            dr3_shades_bs = lut->bs;
            dr3_shades_as = lut->as;
            dr3_shades_bright = dr3_fade_bright;
            dr3_shades_color  = dr3_fade_color;
            dr3_shades_flash  = dr3_fade_flash;
            dr3_shades_my_car = my_car;
            dr3_shades_valid = 1;
        }

        for (i = 0; i < DR3_MAX_CARS; ++i) {

            const unsigned char *c = dr3_chaos_car_col[i];
            const int is_own = (i == my_car);
            const int car_col_256   = is_own ? 256 : dr3_fade_color;
            const int car_flash_256 = is_own ? dr3_fade_flash : 0;

            if (!lut_changed && (dr3_shades_col[i][0] == c[0]) &&
                (dr3_shades_col[i][1] == c[1]) && (dr3_shades_col[i][2] == c[2])) continue;

            dr3_shades_col[i][0] = c[0];
            dr3_shades_col[i][1] = c[1];
            dr3_shades_col[i][2] = c[2];

            /*
             * The colour comes in the engine's own space (carcol.pal / s_54 +0x34, 0..63) and the ramp has
             * to be built *there*: the light half of it saturates that space at 63 = white, which is the
             * shine on the car (___4a064h_cdecl).  dr3_carcolour_build_vga6_fade expands it the way the port
             * shows a palette entry while following the race intro/outro fade & player highlight flash.
             */
            dr3_carcolour_build_vga6_fade(&dr3_shades_cache[i * 10], c[0], c[1], c[2], lut,
                                          dr3_fade_bright, car_col_256, car_flash_256);
        }
    }

    dr3_chaos_time(DR3_CT_TINT_SHADES, dr3_t_shades);
    const unsigned long long dr3_t_mask = dr3_chaos_tick();

    for (i = 0; i < DR3_MAX_CARS; ++i) {
        hp_box[i][0] = DR3_CAR_MASK_W; hp_box[i][1] = DR3_CAR_MASK_H;
        hp_box[i][2] = -1;             hp_box[i][3] = -1;
    }

    /*
     * Fused 1:1 row race fast path: tints car pixels AND computes per-car hp_box[m] in one walk of
     * dr3_chaos_car_screen, using the cached inverse X map and cached shade ramps.
     */
    if ((dh == sh) && (sw <= DR3_TINT_INV_MAX) &&
        (dr3_tint_sy[0] == 0) && (dr3_tint_sy[dh - 1] == dh - 1)) {

        const int aligned = (((uintptr_t)dr3_chaos_car_screen & 3u) == 0) && ((DR3_CAR_MASK_W & 3) == 0);
        int       sx, my;

        for (my = by0; my <= by1; ++my) {

            const int      sy   = my + row_off;
            const uint8_t *mrow = dr3_chaos_car_screen + (size_t)my * (size_t)DR3_CAR_MASK_W;
            const uint8_t *srow = ((sy >= 0) && (sy < sh)) ? (src + (size_t)sy * (size_t)src_pitch) : NULL;

            for (sx = bx0; sx <= bx1; ++sx) {

                uint8_t m;
                int     car;

                if (aligned && !(sx & 3) && ((sx + 3) <= bx1) && !*(const dr3_u32_alias *)(mrow + sx)) {
                    sx += 3;
                    continue;
                }

                m = mrow[sx];
                if (!m || (m > DR3_MAX_CARS)) continue;

                car = (int)m - 1;
                if (sx < hp_box[car][0]) hp_box[car][0] = sx;
                if (my < hp_box[car][1]) hp_box[car][1] = my;
                if (sx > hp_box[car][2]) hp_box[car][2] = sx;
                if (my > hp_box[car][3]) hp_box[car][3] = my;

                if (srow && (sx < sw)) {
                    const uint8_t v = srow[sx];

                    if ((v >= DR3_CARCOLOUR_FIRST) && (v <= DR3_CARCOLOUR_LAST)) {
                        const uint32_t col = dr3_shades_cache[car * 10 + (v - DR3_CARCOLOUR_FIRST)];
                        const int      cnt = (int)dr3_tint_inv_cnt[sx];
                        int            k;
                        uint32_t      *dp  = dst + (ptrdiff_t)dst_stride_x * dr3_tint_inv_first[sx]
                                                 + (ptrdiff_t)dst_stride_y * sy;

                        for (k = 0; k < cnt; ++k) {
                            *dp = col;
                            dp += dst_stride_x;
                        }
                        n += cnt;
                    }
                }
            }
        }

        hp_box_done = 1;
    }
    else {
        n = dr3_carcolour_tint_mask(dst, dw, dh, dst_stride_x, dst_stride_y, src, sw, sh, src_pitch,
                                    dr3_chaos_car_screen, DR3_CAR_MASK_W, DR3_CAR_MASK_H, DR3_CAR_MASK_W,
                                    row_off, &area, dr3_shades_cache, DR3_MAX_CARS);
    }

    dr3_chaos_time(DR3_CT_TINT_MASK, dr3_t_mask);
    const unsigned long long dr3_t_hp = dr3_chaos_tick();

    /*
     * The health line over the cars (dr3_chaos.h: DR3_CHAOS_HP_LINE_...).  A small line above a car that has
     * taken damage: full length and green while the car is nearly whole, shrinking and turning over yellow
     * to red as it loses HP, scaled to the HP the car started the race with (dr3_chaos_hp_max, which is -1
     * until the first present of a race and learns the HP the cars have then - which is what makes a car the
     * harness' own "hp <car> <value>" weakened one that is "whole at 12%").  A car that is whole and a wreck
     * get no line at all (dr3_carcolour_hp_line_shown, platform_3ds/dr3_blit.h).
     *
     * It is drawn *here*, into the port's own image and not into the engine's screen: that is where the
     * cars are already marked per pixel (dr3_chaos_car_screen holds the car index + 1), the target format
     * is known (the LUT carries its channel shifts) and the 3DS and the PC get the very same line - no
     * entry of the game's palette has to be kept free for it.
     */
    {
        /*
         * What the line did for each car in the previous frame: 0 = no line (the car is whole), 1 = line
         * drawn (it is damaged), 2 = no line because the car is a wreck, 3 = no line because it is the
         * *player's own* car (the HUD's DAMAGE panel is his) and 4 = no line because the race is over.
         * A race starts at 0, and a *change* is the
         * only moment worth a log line (the whole table is logged for the first presents of a race, see
         * below) - that log is what a run is checked with:
         *
         *     [dr3] chaos: hp line: car 3 hp 51200/51200 (100%) off      <- whole (its own scale)
         *     [dr3] chaos: hp line: car 3 hp 39936/51200 ( 78%) shown    <- damaged: the line is drawn
         *     [dr3] chaos: hp line: car 3 hp     0/51200 (  0%) wrecked  <- a wreck: no line
         *     [dr3] chaos: hp line: car 7 hp 12288/51200 ( 24%) own      <- the player's own car: no line
         *     [dr3] chaos: hp line: car 7 hp 12288/51200 ( 24%) over     <- RACE OVER: every line hidden
         */
        static int hp_state_last[DR3_MAX_CARS];
        const int  my_car = dr3_own_car();
        /*
         * Whether the race is over (dr3_chaos.h: `___243d14h`): the RACE OVER screen and its fade to black
         * come next and *no* line of the port belongs over them, however the race ended.  Every car on the
         * screen goes to "over" once - so the change is in the log - and the summary line below says how
         * many lines were hidden.
         */
        static int hp_over_logged;
        const int  over = dr3_chaos_race_over();
        int        m, bx, by, hidden = 0;

        if (!over) hp_over_logged = 0;

        /* the box of every car, from its own marks (only when the fused path above did not build it) */
        if (!hp_box_done) {
            for (r = by0; r <= by1; ++r) {

                const uint8_t *row = dr3_chaos_car_screen + (size_t)r * (size_t)DR3_CAR_MASK_W;
                const int      row_aligned = (((uintptr_t)row & 3u) == 0);

                for (x = bx0; x <= bx1; ++x) {

                    int v;

                    if (row_aligned && !(x & 3) && ((x + 3) <= bx1) && !*(const dr3_u32_alias *)(row + x)) { x += 3; continue; }

                    v = (int)row[x] - 1;

                    if ((v < 0) || (v >= DR3_MAX_CARS)) continue;

                    if (x < hp_box[v][0]) hp_box[v][0] = x;
                    if (r < hp_box[v][1]) hp_box[v][1] = r;
                    if (x > hp_box[v][2]) hp_box[v][2] = x;
                    if (r > hp_box[v][3]) hp_box[v][3] = r;
                }
            }
        }

        for (m = 0; m < DR3_MAX_CARS; ++m) {

            /* s_94[m].__18 - the port has no D() of its own, so this is a plain 32 bit read */
            const int hp_now = (int)(*(const uint32_t *)(const void *)(___1de580h + 0x94*m + 0x18));
            int       hp_max = dr3_chaos_hp_max[m];
            int       cx, cy, len, lh, x0, y0, pct, rr, gg;
            uint32_t  col;

            if (hp_box[m][2] < 0) continue;                 /* this car is not on the screen */

            if (hp_now > hp_max) { dr3_chaos_hp_max[m] = hp_now; hp_max = hp_now; }
            if (hp_max <= 0) continue;                      /* nothing to scale to */

            {
                /*
                 * Five cases, and only one of them is drawn: a whole car (no line - it would be the full
                 * length bar of a car nobody has touched), a damaged car (the line), a wreck (no line
                 * either), the player's own car (no line either - the bar is there to read the *other*
                 * cars) and the race over (no line at all: the RACE OVER screen follows).  The first four
                 * are what dr3_carcolour_hp_line_shown() answers; "over" is the engine's own state.
                 */
                const int own   = (m == my_car);
                const int line  = !over && !dr3_boss_is_giant_car(m) && dr3_carcolour_hp_line_shown(hp_now, hp_max, own);
                const int state = over ? 4 : (own ? 3 : ((hp_now <= 0) ? 2 : (line ? 1 : 0)));

                if (state == 4) ++hidden;

                if ((state != hp_state_last[m]) && dr3_log_enabled())
                    dr3_log("[dr3] chaos: hp line: car %2d hp %6d/%6d (%3d%%) %s", m, hp_now, hp_max,
                            (hp_now > 0) ? (hp_now * 100 / hp_max) : 0,
                            (state == 4) ? "over"
                            : (state == 3) ? "own"
                            : (state == 2) ? "wrecked" : (state ? "shown" : "off"));

                hp_state_last[m] = state;

                if (!line) continue;                   /* whole, wrecked, the own car or the race over */
            }

            pct = (hp_now > 0) ? (hp_now * 100 / hp_max) : 0;

            /* green over yellow to red - the red channel rises and the green one falls */
            if (pct >= 50) { rr = (100 - pct) * 2 * 255 / 100; gg = 255; }
            else           { rr = 255; gg = pct * 2 * 255 / 100; }

            col = ((uint32_t)rr << lut->rs) | ((uint32_t)gg << lut->gs) | ((uint32_t)0xff << lut->as);

            lh  = (DR3_CHAOS_HP_LINE_H * dh) / sh;
            if (lh < 1) lh = 1;

            len = (DR3_CHAOS_HP_LINE_MAX * pct / 100) * dw / sw;
            if (len < 1) len = 1;

            cx = ((hp_box[m][0] + hp_box[m][2]) / 2) * dw / sw;
            cy = ((hp_box[m][1] + row_off) * dh) / sh - ((DR3_CHAOS_HP_LINE_GAP * dh) / sh) - lh;

            x0 = cx - len / 2;
            y0 = cy;

            for (by = y0; by < y0 + lh; ++by) {

                if ((by < 0) || (by >= dh)) continue;

                for (bx = x0; bx < x0 + len; ++bx) {

                    if ((bx < 0) || (bx >= dw)) continue;

                    dst[(ptrdiff_t)dst_stride_x * bx + (ptrdiff_t)dst_stride_y * by] = col;
                }
            }
        }

        /*
         * One line for the whole frame when the race is over: how many lines were hidden by it (dr3_chaos.h).
         * The per car "over" lines above say *which* car lost its line - this says that the port noticed.
         */
        if (over && !hp_over_logged && dr3_log_enabled()) {

            hp_over_logged = 1;
            dr3_log("[dr3] chaos: hp line: the race is over (___243d14h != 0) - %d health line(s) of the screen "
                    "and the sudden death banner are hidden from here on", hidden);
        }
    }

    dr3_chaos_time(DR3_CT_TINT_HP, dr3_t_hp);

    /*
     * The sudden death banner (platform_3ds/dr3_dm.h, drally.h: DR3_CHAOS_MODE_DM).  It is drawn in this
     * pass because this is where the port owns the image: the target, its size and the LUT's channel shifts
     * are known here, and the 3DS top screen and the PC window both come through it.  A deathmatch in its
     * sudden death is the only race that has one - for any other frame dr3_dm_banner_visible() says no and
     * nothing is drawn (and once the race is over the banner is gone as well, dr3_dm_banner()).
     */
    dr3_dm_banner(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off, lut);

    dr3_arena_ring_banner(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off, lut);
    dr3_boss_overlay(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off, lut);

    /* 30th Anniversary in-race radio / rival trash-talk bar (bottom of viewport, never covers car or top banners) */
    {
        extern int  CURRENT_VIEWPORT_X;
        const char *spk = NULL;
        const char *msg = NULL;
        int         style = 0;

        if (dr3_anniv_radio_now(&spk, &msg, &style) && spk && msg) {
            const int vp_x = ((CURRENT_VIEWPORT_X > 0) && (CURRENT_VIEWPORT_X < sw)) ? CURRENT_VIEWPORT_X : 0;
            const int vp_w = sw - vp_x;
            const int w0   = dr3_dm_text_width(spk, 1);
            const int w1   = dr3_dm_text_width(msg, 1);
            int       bw   = ((w0 > w1) ? w0 : w1) + 14;
            int       bh   = 21;
            int       gx0, gy0, tx0, tx1, ty0, ty1, bx, by;
            uint32_t  acc_rgb, acc_col, border_col, msg_col, dark_col;

            if (bw > vp_w - 8) bw = vp_w - 8;
            gx0 = vp_x + (vp_w - bw) / 2;
            gy0 = 175;

            acc_rgb = (style == DR3_ANNIV_RADIO_RIVAL)     ? 0xff9828u
                    : (style == DR3_ANNIV_RADIO_GOLD)      ? 0xffe448u
                    : (style == DR3_ANNIV_RADIO_ADVERSARY) ? 0xd840ffu
                    :                                        0x38dcffu;

            acc_col    = (((acc_rgb >> 16) & 0xffu) << lut->rs)
                       | (((acc_rgb >>  8) & 0xffu) << lut->gs)
                       | (((acc_rgb >>  0) & 0xffu) << lut->bs)
                       | ((uint32_t)0xff << lut->as);
            border_col = ((uint32_t)0x38 << lut->rs) | ((uint32_t)0x40 << lut->gs)
                       | ((uint32_t)0x48 << lut->bs) | ((uint32_t)0xff << lut->as);
            msg_col    = ((uint32_t)0xf4 << lut->rs) | ((uint32_t)0xf4 << lut->gs)
                       | ((uint32_t)0xe8 << lut->bs) | ((uint32_t)0xff << lut->as);
            dark_col   = (uint32_t)0xff << lut->as;

            tx0 = (gx0 * dw) / sw;
            tx1 = ((gx0 + bw) * dw) / sw;
            ty0 = ((gy0 + row_off) * dh) / sh;
            ty1 = ((gy0 + bh + row_off) * dh) / sh;

            /* Semi-transparent dark radio box with colored 3px left speaker bar */
            for (by = ty0; by <= ty1; ++by) {
                if ((by < 0) || (by >= dh)) continue;
                for (bx = tx0; bx <= tx1; ++bx) {
                    uint32_t *p;
                    if ((bx < 0) || (bx >= dw)) continue;
                    p = &dst[(ptrdiff_t)dst_stride_x * bx + (ptrdiff_t)dst_stride_y * by];
                    if (bx <= tx0 + 2) {
                        *p = acc_col;
                    } else if ((by == ty0) || (by == ty1) || (bx == tx1)) {
                        *p = border_col;
                    } else {
                        const uint32_t c = *p;
                        const uint32_t r = ((c >> lut->rs) & 0xffu) >> 2;
                        const uint32_t g = ((c >> lut->gs) & 0xffu) >> 2;
                        const uint32_t b = ((c >> lut->bs) & 0xffu) >> 2;
                        *p = (r << lut->rs) | (g << lut->gs) | (b << lut->bs) | dark_col;
                    }
                }
            }

            /* Speaker header + crisp dialogue line with 1px shadow */
            dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                               dark_col, gx0 + 8, gy0 + 4, spk, 1);
            dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                               acc_col,  gx0 + 7, gy0 + 3, spk, 1);

            dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                               dark_col, gx0 + 8, gy0 + 13, msg, 1);
            dr3_dm_banner_text(dst, dw, dh, dst_stride_x, dst_stride_y, sw, sh, row_off,
                               msg_col,  gx0 + 7, gy0 + 12, msg, 1);
        }
    }

    /*
     * The data this whole thing is checked with, while the log is on: what the port received (the
     * race's own mask and colours, filled by the engine) and what it did with it.  Logged for the first
     * presents of a race - a race is the only time the colours are set.
     */
    if (dr3_log_enabled() && ((dr3_chaos_car_col[0][0] | dr3_chaos_car_col[0][1] | dr3_chaos_car_col[0][2]) != 0)
        && (dr3_tint_logged < 6)) {

        int histogram[256];
        int marked = 0, in_ramp = 0, marked_ramp = 0, k, most = 0;

        memset(histogram, 0, sizeof(histogram));

        for (k = 0; k < DR3_CAR_MASK_W * DR3_CAR_MASK_H; ++k) {

            const int     sx = k % DR3_CAR_MASK_W, sy = k / DR3_CAR_MASK_W + row_off;
            const uint8_t v  = ((sy < sh) ? src[sy * src_pitch + sx] : 0);

            if (!dr3_chaos_car_screen[k]) continue;

            ++marked;
            if ((v >= DR3_CARCOLOUR_FIRST) && (v <= DR3_CARCOLOUR_LAST)) ++marked_ramp;
            ++histogram[v];
        }

        for (k = 0; k < 256; ++k) {

            if ((k >= DR3_CARCOLOUR_FIRST) && (k <= DR3_CARCOLOUR_LAST)) in_ramp += histogram[k];
            if (histogram[k] > histogram[most]) most = k;
        }

        dr3_log("[dr3] chaos: cars: %d pixel(s) marked, %d coloured, %d of the marks on the ramp 0x%02x..0x%02x, ramp pixels on screen %d, most common marked value 0x%02x (%d) (game row = src row - %d)",
                marked, n, marked_ramp, DR3_CARCOLOUR_FIRST, DR3_CARCOLOUR_LAST, in_ramp, most,
                histogram[most], row_off);

        /*
         * And the health line: which car got one and which did not, with the two numbers the decision is
         * made of.  A car that is whole, one that is damaged, a wreck and the *player's own* car are the
         * four cases, and only the damaged car is drawn (dr3_carcolour_hp_line_shown).  Every change after
         * this is logged where the line is drawn - the two together are the trail a run is checked with.
         */
        for (k = 0; k < DR3_MAX_CARS; ++k) {

            const int hp_now = (int)(*(const uint32_t *)(const void *)(___1de580h + 0x94*k + 0x18));
            const int hp_max = dr3_chaos_hp_max[k];
            const int own    = (k == my_car);
            const int over   = dr3_chaos_race_over();

            if (hp_max <= 0) continue;                    /* this car never raced */

            dr3_log("[dr3] chaos: hp line: car %2d hp %6d/%6d (%3d%%) %s", k, hp_now, hp_max,
                    (hp_now > 0) ? (hp_now * 100 / hp_max) : 0,
                    over ? "over"
                         : own ? "own"
                               : (hp_now <= 0) ? "wrecked"
                                               : (dr3_carcolour_hp_line_shown(hp_now, hp_max, 0) ? "shown" : "off"));
        }

        ++dr3_tint_logged;
    }

    /*
     * The mask is NOT cleared here: the engine owns it.  race___4f300h clears it at the start of every
     * car pass and race_main.c copies the fresh one together with the image, so the marks stay valid
     * until the next car pass.  Clearing it here would let a present that is not the race screen (a
     * duplicate flip, the front end) throw the marks away before the race screen gets to use them -
     * which is exactly what made the cars keep the palette's own colour.
     */
    return n;
}

#endif /* DR3_CHAOS */


