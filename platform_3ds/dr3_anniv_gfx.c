#include "dr3_anniv_gfx.h"

#include <math.h>
#include <string.h>

/*
 * MENU.PAL layout (verified directly from MENU.BPA -> MENU.PAL):
 *   0x00..0x0f : 16-step pure black -> obsidian charcoal -> gunmetal -> chrome -> white
 *   0x10..0x1f : 16-step ember orange -> fiery gold -> white-gold
 *   0x22..0x2e : dark-blue blueprint grid background (preserved untouched)
 *   0x40..0x5f : 32-step live player car-paint ramp (scenecar.bpk & lotus.bpk)
 *   carres.bpk : 4 slices of 100x52 using 16-step paint ramps at 0x40, 0x50, 0xe0, 0xf0
 *                aligned at exact offset (dx = +3, dy = -5) from scenecar[5] (96x64).
 */

static uint8_t anniv_ramp_px(uint8_t ramp_base, int ramp_len, int num, int den)
{
    int idx;

    if (den <= 0) den = 1;
    idx = (num * (ramp_len - 1) + den / 2) / den;
    if (idx < 0) idx = 0;
    if (idx >= ramp_len) idx = ramp_len - 1;
    return (uint8_t)(ramp_base + idx);
}

static void anniv_put(uint8_t *buf, int w, int h, int sx, int sy, int ox, int oy, uint8_t col)
{
    int x = sx + ox;
    int y = sy + oy;

    if (x >= 0 && x < w && y >= 0 && y < h) {
        buf[y * w + x] = col;
    }
}

static void anniv_star_glint(uint8_t *buf, int w, int h, int sx, int sy, int ox, int oy,
                             uint8_t ramp_base, int ramp_len)
{
    uint8_t tip  = anniv_ramp_px(ramp_base, ramp_len, 11, 15);
    uint8_t mid  = anniv_ramp_px(ramp_base, ramp_len, 14, 15);

    anniv_put(buf, w, h, sx,     sy,     ox, oy, 0x0f);
    anniv_put(buf, w, h, sx - 1, sy,     ox, oy, mid);
    anniv_put(buf, w, h, sx + 1, sy,     ox, oy, mid);
    anniv_put(buf, w, h, sx,     sy - 1, ox, oy, mid);
    anniv_put(buf, w, h, sx,     sy + 1, ox, oy, mid);
    anniv_put(buf, w, h, sx - 2, sy,     ox, oy, tip);
    anniv_put(buf, w, h, sx + 2, sy,     ox, oy, tip);
    anniv_put(buf, w, h, sx,     sy - 2, ox, oy, tip);
    anniv_put(buf, w, h, sx,     sy + 2, ox, oy, tip);
}

static void anniv_gun_barrel(uint8_t *buf, int w, int h, int ox, int oy,
                             int x0, int y0, int x1, int y1)
{
    int steps = x1 - x0;
    int i;

    if (steps <= 0) return;

    for (i = 0; i <= steps; ++i) {
        int x = x0 + i;
        int y = y0 + (i * (y1 - y0) + steps / 2) / steps;
        uint8_t top_c = (i >= steps - 1) ? 0x1e : ((i >= steps - 3) ? 0x0b : 0x07);
        uint8_t mid_c = (i >= steps - 1) ? 0x19 : 0x04;
        uint8_t bot_c = (i >= steps - 1) ? 0x14 : 0x02;

        anniv_put(buf, w, h, x, y - 1, ox, oy, top_c);
        anniv_put(buf, w, h, x, y,     ox, oy, mid_c);
        anniv_put(buf, w, h, x, y + 1, ox, oy, bot_c);
    }
    /* hot muzzle glow */
    anniv_put(buf, w, h, x1 + 1, y1, ox, oy, 0x1f);
}

static void anniv_transform_portrait(const uint8_t *src, uint8_t *dst,
                                     int w, int h, int ox, int oy,
                                     uint8_t ramp_base, int ramp_len)
{
    int x, y;

    memcpy(dst, src, (size_t)(w * h));

    /* 1. Remap body paint ramp -> sculpted obsidian-carbon with live player-color racing trim */
    for (y = 0; y < h; ++y) {
        int sy = y - oy;

        for (x = 0; x < w; ++x) {
            uint8_t p = src[y * w + x];

            if ((int)p >= (int)ramp_base && (int)p < (int)ramp_base + ramp_len) {
                int sx  = x - ox;
                int lvl = ((int)(p - ramp_base) * 31) / (ramp_len - 1); /* normalized 0..31 */
                int is_stripe = 0;

                /* Inner pontoon racing stripe */
                if (sx >= 35 && sx <= 72) {
                    float cy = 24.0f + 0.35f * (float)(sx - 35);
                    if (fabsf((float)sy - cy) <= 1.35f) is_stripe = 1;
                }
                /* Outer far-side pontoon racing stripe */
                if (!is_stripe && sx >= 60 && sx <= 88) {
                    float cy = 18.5f + 0.28f * (float)(sx - 60);
                    if (fabsf((float)sy - cy) <= 1.30f) is_stripe = 1;
                }
                /* Side-skirt trim bar above front wheel */
                if (!is_stripe && sx >= 28 && sx <= 44) {
                    float cy = 33.5f + 0.25f * (float)(sx - 28);
                    if (fabsf((float)sy - cy) <= 1.10f) is_stripe = 1;
                }

                if (is_stripe) {
                    /* Live player color scaled brightly so stripes pop over obsidian */
                    int bright = 10 + (lvl * 21) / 31;
                    dst[y * w + x] = anniv_ramp_px(ramp_base, ramp_len, bright, 31);
                } else {
                    /* Glossy stealth obsidian-carbon ramp (0x01..0x0c) */
                    uint8_t obs;
                    if      (lvl < 4)  obs = 0x01;
                    else if (lvl < 10) obs = 0x02;
                    else if (lvl < 16) obs = 0x03;
                    else if (lvl < 21) obs = 0x04;
                    else if (lvl < 25) obs = 0x05;
                    else if (lvl < 28) obs = 0x07;
                    else if (lvl < 30) obs = 0x09;
                    else               obs = 0x0c;
                    dst[y * w + x] = obs;
                }
            }
        }
    }

    /* 2. Swept carbon-chrome GT rear wing + twin struts */
    for (y = 13; y <= 22; ++y) {
        anniv_put(dst, w, h, 12, y, ox, oy, 0x02);
        anniv_put(dst, w, h, 13, y, ox, oy, 0x05);
        anniv_put(dst, w, h, 14, y, ox, oy, 0x03);
    }
    for (y = 10; y <= 19; ++y) {
        anniv_put(dst, w, h, 24, y, ox, oy, 0x02);
        anniv_put(dst, w, h, 25, y, ox, oy, 0x05);
        anniv_put(dst, w, h, 26, y, ox, oy, 0x03);
    }

    for (x = 5; x <= 33; ++x) {
        int wy = 13 - ((x - 5) * 7 + 14) / 28;
        uint8_t hi = (x > 22) ? 0x0e : 0x0b;
        uint8_t md = (x > 22) ? 0x07 : 0x05;

        anniv_put(dst, w, h, x, wy - 1, ox, oy, hi);
        anniv_put(dst, w, h, x, wy,     ox, oy, md);
        anniv_put(dst, w, h, x, wy + 1, ox, oy, 0x02);
        anniv_put(dst, w, h, x, wy + 2, ox, oy, 0x01);
    }

    /* Wing endplates in live player color + chrome corner */
    {
        uint8_t ep_hi = anniv_ramp_px(ramp_base, ramp_len, 13, 15);
        uint8_t ep_lo = anniv_ramp_px(ramp_base, ramp_len, 8, 15);

        for (y = 11; y <= 16; ++y) {
            anniv_put(dst, w, h, 4, y, ox, oy, (y == 11) ? 0x0e : ep_hi);
            anniv_put(dst, w, h, 5, y, ox, oy, ep_lo);
        }
        for (y = 4; y <= 9; ++y) {
            anniv_put(dst, w, h, 33, y, ox, oy, (y == 4) ? 0x0e : ep_hi);
            anniv_put(dst, w, h, 34, y, ox, oy, ep_lo);
        }
    }

    /* 3. Twin heavy front autocannons */
    anniv_gun_barrel(dst, w, h, ox, oy, 66, 32, 82, 37);
    anniv_gun_barrel(dst, w, h, ox, oy, 76, 25, 90, 30);

    /* 4. Fiery amber/white headlights */
    anniv_put(dst, w, h, 44, 29, ox, oy, 0x0f);
    anniv_put(dst, w, h, 45, 29, ox, oy, 0x1e);
    anniv_put(dst, w, h, 44, 30, ox, oy, 0x1c);
    anniv_put(dst, w, h, 72, 18, ox, oy, 0x0f);
    anniv_put(dst, w, h, 73, 18, ox, oy, 0x1e);

    /* 5. Signature Death Rally star glints */
    anniv_star_glint(dst, w, h, 34,  6, ox, oy, ramp_base, ramp_len);
    anniv_star_glint(dst, w, h, 63, 41, ox, oy, ramp_base, ramp_len);
    anniv_star_glint(dst, w, h, 80, 40, ox, oy, ramp_base, ramp_len);
}

void dr3_anniv_build_scenecar(const uint8_t *deliv_96x64, uint8_t *out_96x64)
{
    if (!deliv_96x64 || !out_96x64) return;
    anniv_transform_portrait(deliv_96x64, out_96x64,
                             DR3_ANNIV_SCENECAR_W, DR3_ANNIV_SCENECAR_H,
                             0, 0, 0x40, 32);
}

void dr3_anniv_build_carres(const uint8_t *deliv_carres, uint8_t *out_carres)
{
    static const uint8_t slice_bases[4] = { 0x40, 0x50, 0xe0, 0xf0 };
    int s;

    if (!deliv_carres || !out_carres) return;

    for (s = 0; s < 4; ++s) {
        const uint8_t *src = deliv_carres + s * DR3_ANNIV_CARRES_SLICE;
        uint8_t       *dst = out_carres   + s * DR3_ANNIV_CARRES_SLICE;

        anniv_transform_portrait(src, dst,
                                 DR3_ANNIV_CARRES_W, DR3_ANNIV_CARRES_H,
                                 3, -5, slice_bases[s], 16);
    }
}

static void anniv_stamp_glyph(const uint8_t *carname_6slots, uint8_t *out,
                              int slot, int x0, int x1, int y0, int y1, int dst_x)
{
    const uint8_t *src_slot = carname_6slots + slot * DR3_ANNIV_CARNAME_SZ;
    int y, x;

    for (y = y0; y <= y1; ++y) {
        if (y < 0 || y >= DR3_ANNIV_CARNAME_H) continue;
        for (x = x0; x <= x1; ++x) {
            int dx = dst_x + (x - x0);
            uint8_t sp, dp;

            if (dx < 3 || dx > 92) continue;
            sp = src_slot[y * DR3_ANNIV_CARNAME_W + x];
            dp = out[y * DR3_ANNIV_CARNAME_W + dx];
            /* Preserve already-stamped glyph strokes when kerning overlaps flat bg (0x04/0x05) */
            if ((sp != 0x04 && sp != 0x05) || (dp == 0x04 || dp == 0x05)) {
                out[y * DR3_ANNIV_CARNAME_W + dx] = sp;
            }
        }
    }
}

void dr3_anniv_build_carname(const uint8_t *carname_6slots, uint8_t *out_96x16)
{
    /* Exact chrome glyph slices from original MENU.BPA/carname.bpk (slots 0..5) */
    static const struct { int slot, x0, x1, adv; } letters[10] = {
        { 0, 15, 23, 8 }, /* A (VAGABOND)    */
        { 0, 73, 80, 8 }, /* N (VAGABOND)    */
        { 0, 73, 80, 8 }, /* N (VAGABOND)    */
        { 5, 28, 31, 5 }, /* I (DELIVERATOR) */
        { 5, 33, 41, 8 }, /* V (DELIVERATOR) */
        { 5, 43, 49, 7 }, /* E (DELIVERATOR) */
        { 5, 51, 58, 8 }, /* R (DELIVERATOR) */
        { 2,  4, 11, 8 }, /* S (SENTINEL)    */
        { 0, 15, 23, 8 }, /* A (VAGABOND)    */
        { 5, 51, 58, 8 }  /* R (DELIVERATOR) */
    };
    const uint8_t *bg_slot;
    int x, y, i, cur_x;

    if (!carname_6slots || !out_96x16) return;

    /* Copy WRAITH (slot 4) for its exact 1996 chrome bezel border, then tile its clean interior */
    bg_slot = carname_6slots + 4 * DR3_ANNIV_CARNAME_SZ;
    memcpy(out_96x16, bg_slot, DR3_ANNIV_CARNAME_SZ);

    for (y = 2; y <= 13; ++y) {
        for (x = 3; x <= 92; ++x) {
            int tile_x = 4 + ((x - 3) & 7);
            out_96x16[y * DR3_ANNIV_CARNAME_W + x] = bg_slot[y * DR3_ANNIV_CARNAME_W + tile_x];
        }
    }

    /* Stamp "ANNIVERSAR" centered at x = 6 (total width 84px -> cols 6..90) */
    cur_x = 6;
    for (i = 0; i < 10; ++i) {
        anniv_stamp_glyph(carname_6slots, out_96x16,
                          letters[i].slot, letters[i].x0, letters[i].x1, 3, 13, cur_x);
        cur_x += letters[i].adv;
    }

    /* Compose 'Y' from upper V (rows 3..7) + centered lower I stem (rows 7..13) */
    anniv_stamp_glyph(carname_6slots, out_96x16, 5, 33, 41, 3, 7, cur_x);
    anniv_stamp_glyph(carname_6slots, out_96x16, 5, 28, 31, 7, 13, cur_x + 3);
}

void dr3_anniv_tint_spin(uint8_t *frame_96x64)
{
    int i;

    if (!frame_96x64) return;

    /*
     * Angle-invariant stealth finish for all 64 turntable frames of lotus.bpk:
     *   - shadows/midtones (0..22)  -> sculpted obsidian-carbon (0x01..0x05)
     *   - upper contour band (23..28) -> gleaming live player-color racing trim (0x4c..0x5c)
     *   - specular peaks (29..31)   -> chrome-white flash (0x0c..0x0e)
     */
    for (i = 0; i < DR3_ANNIV_SCENECAR_SZ; ++i) {
        uint8_t p = frame_96x64[i];

        if (p >= 0x40 && p <= 0x5f) {
            int lvl = (int)(p - 0x40);
            uint8_t out_c;

            if      (lvl <= 3)  out_c = 0x01;
            else if (lvl <= 9)  out_c = 0x02;
            else if (lvl <= 15) out_c = 0x03;
            else if (lvl <= 19) out_c = 0x04;
            else if (lvl <= 22) out_c = 0x05;
            else if (lvl <= 26) out_c = (uint8_t)(0x4c + (lvl - 23) * 3);
            else if (lvl <= 28) out_c = (uint8_t)(0x59 + (lvl - 27) * 3);
            else                out_c = (uint8_t)(0x0c + (lvl - 29));

            frame_96x64[i] = out_c;
        }
    }
}
