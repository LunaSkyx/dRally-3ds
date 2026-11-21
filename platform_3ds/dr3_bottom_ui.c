/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_bottom_ui.h"

#include <stdio.h>
#include <string.h>

/* ===================================================================== Death Rally palette === */

#define COL_BLACK       0x000000u
#define COL_BG_DARK     0x0E0203u
#define COL_BG_MID      0x280507u
#define COL_BG_GLOW     0x3C070Au

#define COL_PANEL_BG    0x0B080Au
#define COL_PANEL_ALT   0x140E11u
#define COL_PLAYER_BG   0x3A2408u
#define COL_BOSS_BG     0x2A0A32u

#define COL_GRID_BG     0x071522u
#define COL_GRID_LINE   0x102A42u

#define COL_STEEL_HI    0xACB4C0u
#define COL_STEEL_MID   0x626A74u
#define COL_STEEL_DARK  0x363B42u
#define COL_STEEL_LO    0x1D2025u
#define COL_RIVET       0xDCE2ECu

#define COL_GOLD_HI     0xFFE264u
#define COL_GOLD_MID    0xE89C24u
#define COL_GOLD_LO     0x884810u
#define COL_FIRE_ORANGE 0xE86418u
#define COL_FIRE_RED    0xB82012u

#define COL_TXT_WHITE   0xF2F4F8u
#define COL_TXT_SILVER  0xC4CCD6u
#define COL_TXT_DIM     0x7E8894u
#define COL_TXT_GOLD    0xFFD448u
#define COL_TXT_COPPER  0xE89448u
#define COL_TXT_GREEN   0x3CE860u
#define COL_TXT_CYAN    0x48D8FFu
#define COL_TXT_AMBER   0xFFA828u
#define COL_TXT_RED     0xFF3C34u
#define COL_TXT_PURP    0xDC58FFu

#define COL_KEY_BG      0x242830u
#define COL_KEY_HI      0x7C8490u
#define COL_KEY_LO      0x121418u

/* ========================================================================= 5x7 bitmap font === */

/* Printable ASCII 0x20..0x5F (64 glyphs: space, punctuation, digits, uppercase A..Z, symbols).
   Lowercase letters map automatically to uppercase for authentic crisp Death Rally styling.
   Each glyph is 7 rows; bit 4 = leftmost pixel, bit 0 = rightmost pixel. */
static const uint8_t dr3_font5x7[64][7] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* ' ' */
    { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 }, /* '!' */
    { 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* '"' */
    { 0x0A, 0x1F, 0x0A, 0x0A, 0x1F, 0x0A, 0x00 }, /* '#' */
    { 0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04 }, /* '$' */
    { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 }, /* '%' */
    { 0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D }, /* '&' */
    { 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* ''' */
    { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 }, /* '(' */
    { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 }, /* ')' */
    { 0x00, 0x0A, 0x04, 0x1F, 0x04, 0x0A, 0x00 }, /* '*' */
    { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 }, /* '+' */
    { 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08 }, /* ',' */
    { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 }, /* '-' */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06 }, /* '.' */
    { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 }, /* '/' */
    { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, /* '0' */
    { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E }, /* '1' */
    { 0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F }, /* '2' */
    { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E }, /* '3' */
    { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, /* '4' */
    { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E }, /* '5' */
    { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E }, /* '6' */
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 }, /* '7' */
    { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, /* '8' */
    { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C }, /* '9' */
    { 0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00 }, /* ':' */
    { 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x08 }, /* ';' */
    { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 }, /* '<' */
    { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 }, /* '=' */
    { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 }, /* '>' */
    { 0x0E, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04 }, /* '?' */
    { 0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E }, /* '@' */
    { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, /* 'A' */
    { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E }, /* 'B' */
    { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E }, /* 'C' */
    { 0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C }, /* 'D' */
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F }, /* 'E' */
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 }, /* 'F' */
    { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F }, /* 'G' */
    { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, /* 'H' */
    { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E }, /* 'I' */
    { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C }, /* 'J' */
    { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 }, /* 'K' */
    { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F }, /* 'L' */
    { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 }, /* 'M' */
    { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 }, /* 'N' */
    { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, /* 'O' */
    { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 }, /* 'P' */
    { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D }, /* 'Q' */
    { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 }, /* 'R' */
    { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E }, /* 'S' */
    { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 }, /* 'T' */
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, /* 'U' */
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 }, /* 'V' */
    { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 }, /* 'W' */
    { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 }, /* 'X' */
    { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 }, /* 'Y' */
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F }, /* 'Z' */
    { 0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E }, /* '[' */
    { 0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01 }, /* '\' */
    { 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E }, /* ']' */
    { 0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00 }, /* '^' */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F }  /* '_' */
};

static int dr3_ui_glyph_index(char ch)
{
    unsigned char u = (unsigned char)ch;

    if ((u >= 'a') && (u <= 'z')) u = (unsigned char)(u - 'a' + 'A');
    if ((u < 0x20u) || (u > 0x5Fu)) return 0;
    return (int)(u - 0x20u);
}

static void dr3_ui_draw_glyph(const dr3_canvas_t *c, int x, int y, char ch, uint32_t color)
{
    const uint8_t *rows = dr3_font5x7[dr3_ui_glyph_index(ch)];
    int            gy, gx;

    for (gy = 0; gy < 7; ++gy) {
        const uint8_t bits = rows[gy];
        if (!bits) continue;
        for (gx = 0; gx < 5; ++gx) {
            if (bits & (uint8_t)(0x10u >> gx)) dr3_canvas_px(c, x + gx, y + gy, color);
        }
    }
}

void dr3_ui_draw_text(const dr3_canvas_t *c, int x, int y, const char *s, uint32_t color, int shadow)
{
    int cx = x;

    if (!c || !s) return;
    while (*s) {
        if (shadow) dr3_ui_draw_glyph(c, cx + 1, y + 1, *s, COL_BLACK);
        dr3_ui_draw_glyph(c, cx, y, *s, color);
        cx += 6;
        ++s;
    }
}

static void dr3_ui_draw_text_bold(const dr3_canvas_t *c, int x, int y, const char *s, uint32_t color)
{
    int cx = x;

    if (!c || !s) return;
    while (*s) {
        dr3_ui_draw_glyph(c, cx + 2, y + 1, *s, COL_BLACK);
        dr3_ui_draw_glyph(c, cx + 1, y + 1, *s, COL_BLACK);
        dr3_ui_draw_glyph(c, cx,     y,     *s, color);
        dr3_ui_draw_glyph(c, cx + 1, y,     *s, color);
        cx += 7;
        ++s;
    }
}

static int dr3_ui_text_w(const char *s)
{
    const int n = s ? (int)strlen(s) : 0;
    return (n > 0) ? (n * 6 - 1) : 0;
}

static void dr3_ui_draw_text_r(const dr3_canvas_t *c, int right_x, int y, const char *s, uint32_t color, int shadow)
{
    dr3_ui_draw_text(c, right_x - dr3_ui_text_w(s), y, s, color, shadow);
}

static void dr3_ui_draw_text_c(const dr3_canvas_t *c, int x0, int w, int y, const char *s, uint32_t color, int shadow)
{
    const int tw = dr3_ui_text_w(s);
    dr3_ui_draw_text(c, x0 + (w - tw) / 2, y, s, color, shadow);
}

static void dr3_ui_draw_text_trunc(const dr3_canvas_t *c, int x, int y, const char *s, int max_chars, uint32_t color, int shadow)
{
    char buf[40];
    int  len;

    if (!s || (max_chars <= 0)) return;
    if (max_chars >= (int)sizeof(buf)) max_chars = (int)sizeof(buf) - 1;
    len = (int)strlen(s);
    if (len > max_chars) len = max_chars;
    memcpy(buf, s, (size_t)len);
    buf[len] = 0;
    dr3_ui_draw_text(c, x, y, buf, color, shadow);
}

/* ================================================================ Death Rally chrome helpers === */

static uint32_t dr3_ui_lerp_rgb(uint32_t a, uint32_t b, int num, int den)
{
    const int ar = (int)((a >> 16) & 0xffu), ag = (int)((a >> 8) & 0xffu), ab = (int)(a & 0xffu);
    const int br = (int)((b >> 16) & 0xffu), bg = (int)((b >> 8) & 0xffu), bb = (int)(b & 0xffu);
    int       r, g, bl;

    if (den <= 0) return a;
    if (num < 0) num = 0;
    if (num > den) num = den;
    r  = ar + ((br - ar) * num) / den;
    g  = ag + ((bg - ag) * num) / den;
    bl = ab + ((bb - ab) * num) / den;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

/* Authentic Death Rally crimson vertical gradient with subtle scanline texture */
static void dr3_ui_draw_backdrop(const dr3_canvas_t *c)
{
    int y;

    for (y = 0; y < DR3_UI_H; ++y) {
        /* peak crimson glow around y = 85 (behind the main panels), fading to burgundy-black at edges */
        int      dist = y - 85;
        int      t;
        uint32_t col;

        if (dist < 0) dist = -dist;
        t = 160 - dist;
        if (t < 0) t = 0;
        col = dr3_ui_lerp_rgb(COL_BG_DARK, COL_BG_GLOW, t, 160);
        if (y & 1) col = dr3_ui_lerp_rgb(COL_BLACK, col, 13, 16);
        dr3_canvas_fill(c, 0, y, DR3_UI_W, 1, col);
    }
}

/* Brushed metallic header strip with fiery copper-gold accent bars */
static void dr3_ui_draw_header_bar(const dr3_canvas_t *c, int h)
{
    int y;

    dr3_canvas_fill(c, 0, 0, DR3_UI_W, 1, COL_FIRE_ORANGE);
    dr3_canvas_fill(c, 0, 1, DR3_UI_W, 1, COL_GOLD_HI);
    for (y = 2; y < h - 2; ++y) {
        const uint32_t col = dr3_ui_lerp_rgb(0x3C4048u, 0x16181Du, y - 2, (h > 4) ? (h - 4) : 1);
        dr3_canvas_fill(c, 0, y, DR3_UI_W, 1, col);
    }
    dr3_canvas_fill(c, 0, h - 2, DR3_UI_W, 1, COL_GOLD_MID);
    dr3_canvas_fill(c, 0, h - 1, DR3_UI_W, 1, COL_FIRE_RED);
}

/* Signature 1996 Death Rally panel: dark or blue-grid interior + beveled border + riveted L-brackets */
static void dr3_ui_draw_panel(const dr3_canvas_t *c, int x, int y, int w, int h, int blue_grid, int gold_frame)
{
    const uint32_t hi  = gold_frame ? COL_GOLD_HI  : COL_STEEL_HI;
    const uint32_t mid = gold_frame ? COL_GOLD_MID : COL_STEEL_MID;
    const uint32_t lo  = gold_frame ? COL_GOLD_LO  : COL_STEEL_LO;
    const int      arm = 7;

    if ((w < 8) || (h < 8)) return;

    /* interior fill */
    dr3_canvas_fill(c, x + 2, y + 2, w - 4, h - 4, blue_grid ? COL_GRID_BG : COL_PANEL_BG);

    if (blue_grid) {
        int gx, gy;
        for (gx = x + 8; gx < x + w - 2; gx += 8) dr3_canvas_fill(c, gx, y + 2, 1, h - 4, COL_GRID_LINE);
        for (gy = y + 8; gy < y + h - 2; gy += 8) dr3_canvas_fill(c, x + 2, gy, w - 4, 1, COL_GRID_LINE);
    }

    /* outer recessed metallic border */
    dr3_canvas_rect(c, x,     y,     w,     h,     lo);
    dr3_canvas_rect(c, x + 1, y + 1, w - 2, h - 2, gold_frame ? COL_GOLD_LO : COL_STEEL_DARK);

    /* top-left bracket */
    dr3_canvas_fill(c, x,     y,     arm, 2, mid);
    dr3_canvas_fill(c, x,     y,     2, arm, mid);
    dr3_canvas_fill(c, x,     y,     arm, 1, hi);
    dr3_canvas_fill(c, x,     y,     1, arm, hi);
    dr3_canvas_px(c, x + 1, y + 1, COL_RIVET);

    /* top-right bracket */
    dr3_canvas_fill(c, x + w - arm, y, arm, 2, mid);
    dr3_canvas_fill(c, x + w - 2,   y, 2, arm, mid);
    dr3_canvas_fill(c, x + w - arm, y, arm, 1, hi);
    dr3_canvas_px(c, x + w - 2, y + 1, COL_RIVET);

    /* bottom-left bracket */
    dr3_canvas_fill(c, x, y + h - 2,   arm, 2, mid);
    dr3_canvas_fill(c, x, y + h - arm, 2, arm, mid);
    dr3_canvas_fill(c, x, y + h - arm, 1, arm, hi);
    dr3_canvas_px(c, x + 1, y + h - 2, COL_RIVET);

    /* bottom-right bracket */
    dr3_canvas_fill(c, x + w - arm, y + h - 2,   arm, 2, mid);
    dr3_canvas_fill(c, x + w - 2,   y + h - arm, 2, arm, mid);
    dr3_canvas_px(c, x + w - 2, y + h - 2, COL_RIVET);
}

/* Subheader title bar inside a panel */
static void dr3_ui_draw_subhdr(const dr3_canvas_t *c, int x, int y, int w, const char *left, const char *right, uint32_t lcol, uint32_t rcol)
{
    dr3_canvas_fill(c, x, y, w, 11, 0x1E2228u);
    dr3_canvas_fill(c, x, y, w, 1, COL_STEEL_MID);
    dr3_canvas_fill(c, x, y + 10, w, 1, COL_GOLD_LO);
    if (left)  dr3_ui_draw_text(c, x + 4, y + 2, left, lcol, 1);
    if (right) dr3_ui_draw_text_r(c, x + w - 4, y + 2, right, rcol, 1);
}

/* Small metallic button keycap badge, returns width consumed */
static int dr3_ui_draw_keycap(const dr3_canvas_t *c, int x, int y, const char *label, uint32_t text_col)
{
    const int tw = dr3_ui_text_w(label);
    const int bw = tw + 6;

    dr3_canvas_fill(c, x, y, bw, 10, COL_KEY_BG);
    dr3_canvas_fill(c, x, y, bw, 1, COL_KEY_HI);
    dr3_canvas_fill(c, x, y, 1, 10, COL_KEY_HI);
    dr3_canvas_fill(c, x, y + 9, bw, 1, COL_KEY_LO);
    dr3_canvas_fill(c, x + bw - 1, y, 1, 10, COL_KEY_LO);
    dr3_ui_draw_text(c, x + 3, y + 2, label, text_col, 1);
    return bw;
}

/* Smooth or segmented gauge bar with recessed frame */
static void dr3_ui_draw_bar(const dr3_canvas_t *c, int x, int y, int w, int h, int pct, uint32_t col_fill, uint32_t col_bg, int segmented)
{
    int inner_w, fill_w, px;

    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    if ((w < 4) || (h < 3)) return;

    dr3_canvas_rect(c, x, y, w, h, COL_STEEL_MID);
    inner_w = w - 2;
    fill_w  = (inner_w * pct + 50) / 100;
    if ((pct > 0) && (fill_w == 0)) fill_w = 1;
    if (fill_w > inner_w) fill_w = inner_w;

    dr3_canvas_fill(c, x + 1, y + 1, inner_w, h - 2, col_bg);
    if (fill_w > 0) {
        const uint32_t hi = dr3_ui_lerp_rgb(col_fill, 0xFFFFFFu, 1, 4);
        dr3_canvas_fill(c, x + 1, y + 1, fill_w, h - 2, col_fill);
        if (h >= 5) dr3_canvas_fill(c, x + 1, y + 1, fill_w, 1, hi);
    }

    if (segmented) {
        for (px = 5; px < inner_w; px += 6) dr3_canvas_fill(c, x + 1 + px, y + 1, 1, h - 2, COL_BLACK);
    }
}

static uint32_t dr3_ui_hp_color(int pct)
{
    if (pct > 60) return COL_TXT_GREEN;
    if (pct > 30) return COL_TXT_AMBER;
    return COL_TXT_RED;
}

/* Fiery 4-slot Death Rally upgrade pips (just like Shop ENG/TIR/ARM meters) */
static void dr3_ui_draw_upgrade_pips(const dr3_canvas_t *c, int x, int y, int level, int max_level)
{
    static const uint32_t slot_col[4] = { 0xB82414u, 0xE05818u, 0xF09820u, 0xFFE058u };
    int                   i;

    if (max_level < 1) max_level = 1;
    if (max_level > 4) max_level = 4;

    for (i = 0; i < 4; ++i) {
        const int sx = x + i * 21;
        const int sw = 19;
        const int sh = 7;

        if (i >= max_level) {
            dr3_canvas_fill(c, sx, y, sw, sh, 0x0C131Cu);
            dr3_canvas_rect(c, sx, y, sw, sh, 0x1A2838u);
        }
        else if (i < level) {
            const uint32_t col = slot_col[i];
            dr3_canvas_fill(c, sx, y, sw, sh, col);
            dr3_canvas_fill(c, sx, y, sw, 1, dr3_ui_lerp_rgb(col, 0xFFFFFFu, 2, 5));
            dr3_canvas_fill(c, sx, y + sh - 1, sw, 1, dr3_ui_lerp_rgb(col, COL_BLACK, 1, 2));
            dr3_canvas_rect(c, sx, y, sw, sh, COL_STEEL_MID);
        }
        else {
            dr3_canvas_fill(c, sx, y, sw, sh, 0x22262Cu);
            dr3_canvas_rect(c, sx, y, sw, sh, COL_STEEL_DARK);
        }
    }
}

static void dr3_ui_ordinal(char *out, size_t n, int rank)
{
    const int   last = rank % 10;
    const char *suf  = ((last == 1) && (rank != 11)) ? "ST" :
                       ((last == 2) && (rank != 12)) ? "ND" :
                       ((last == 3) && (rank != 13)) ? "RD" : "TH";
    snprintf(out, n, "%d%s", rank, suf);
}

static uint32_t dr3_ui_rank_color(int rank, int is_player, int wrecked)
{
    if (wrecked)    return COL_TXT_RED;
    if (is_player)  return COL_TXT_GOLD;
    if (rank == 1)  return COL_TXT_GOLD;
    if (rank == 2)  return COL_TXT_WHITE;
    if (rank == 3)  return COL_TXT_COPPER;
    return COL_TXT_SILVER;
}

/* =============================================================== standings sorting & windowing === */

static int dr3_ui_racer_better(const dr3_ui_racer_t *a, const dr3_ui_racer_t *b, int battle_mode)
{
    if (battle_mode) {
        const int a_alive = (a->hp > 0);
        const int b_alive = (b->hp > 0);
        if (a_alive != b_alive) return a_alive > b_alive;
        if (a_alive) {
            /* in battle mode, keep the player visible near the top when HP ties or Swarm runs */
            if (a->is_boss != b->is_boss) return a->is_boss > b->is_boss;
            if (a->is_player != b->is_player) return a->is_player > b->is_player;
            if (a->hp != b->hp) return a->hp > b->hp;
        }
    }
    if (a->position != b->position) {
        if (a->position <= 0) return 0;
        if (b->position <= 0) return 1;
        return a->position < b->position;
    }
    if (a->is_player != b->is_player) return a->is_player > b->is_player;
    return a->hp > b->hp;
}

void dr3_ui_sort_racers(dr3_ui_racer_t *list, int n, int battle_mode)
{
    int i, j;

    if (!list || (n <= 1)) return;
    for (i = 1; i < n; ++i) {
        const dr3_ui_racer_t key = list[i];
        for (j = i - 1; (j >= 0) && dr3_ui_racer_better(&key, &list[j], battle_mode); --j) {
            list[j + 1] = list[j];
        }
        list[j + 1] = key;
    }
}

int dr3_ui_select_window(int total, int player_idx, int max_rows, int *out_idx)
{
    int n = 0, i;

    if (!out_idx || (total <= 0) || (max_rows <= 0)) return 0;

    if (total <= max_rows) {
        for (i = 0; i < total; ++i) out_idx[n++] = i;
        return n;
    }

    /* Player already sits inside the top block -> show top max_rows cleanly */
    if ((player_idx < 0) || (player_idx < (max_rows - 1))) {
        for (i = 0; i < max_rows; ++i) out_idx[n++] = i;
        return n;
    }

    /* Need a split view: top leaders, an ellipsis separator (-1), and a window around the player */
    if (max_rows <= 4) {
        for (i = 0; i < max_rows - 1; ++i) out_idx[n++] = i;
        out_idx[n++] = player_idx;
        return n;
    }

    {
        const int tail_n = (max_rows >= 8) ? 3 : 2;
        const int head_n = max_rows - 1 - tail_n;
        int       start  = player_idx - (tail_n / 2);

        if (start + tail_n > total) start = total - tail_n;
        if (start <= head_n) {
            /* Adjacent: no gap needed, just show consecutive rows 0..max_rows-1 */
            for (i = 0; i < max_rows; ++i) out_idx[n++] = i;
            return n;
        }

        for (i = 0; i < head_n; ++i) out_idx[n++] = i;
        out_idx[n++] = -1;                           /* separator row */
        for (i = 0; i < tail_n; ++i) out_idx[n++] = start + i;
    }

    return n;
}

/* ============================================================================ RACE DASHBOARD === */

static uint32_t dr3_ui_swatch_color(uint32_t rgb, int is_player, int is_boss)
{
    if (is_boss) return COL_TXT_PURP;
    if (rgb != 0u) {
        const uint32_t bright = dr3_minimap_marker_colour(rgb);
        return bright ? bright : rgb;
    }
    return is_player ? DR3_MAP_COL_PLAYER : DR3_MAP_COL_CAR;
}

static void dr3_ui_draw_race_standings(const dr3_canvas_t *c, const dr3_ui_race_t *st)
{
    dr3_ui_racer_t sorted[DR3_UI_MAX_RACERS];
    const int      px = 169, py = 22, pw = 147, ph = 164;
    const int      ix = px + 4, iw = pw - 8;
    const int      battle = (st->mode_kind == DR3_UI_RACE_ARENA) ||
                            (st->mode_kind == DR3_UI_RACE_DM) ||
                            (st->mode_kind == DR3_UI_RACE_BOSS_P1) ||
                            (st->mode_kind == DR3_UI_RACE_BOSS_P3);
    const int      gold_frame = (st->mode_kind >= DR3_UI_RACE_BOSS_P1);
    int            n = st->n_racers;
    int            player_sorted = -1;
    int            i;
    char           hdr_r[20];
    const char    *hdr_l;

    if (n < 0) n = 0;
    if (n > DR3_UI_MAX_RACERS) n = DR3_UI_MAX_RACERS;
    if (n > 0) memcpy(sorted, st->racers, (size_t)n * sizeof(sorted[0]));
    dr3_ui_sort_racers(sorted, n, battle);

    for (i = 0; i < n; ++i) {
        if (sorted[i].is_player) { player_sorted = i; break; }
    }

    dr3_ui_draw_panel(c, px, py, pw, ph, 0, gold_frame);

    if (st->mode_kind == DR3_UI_RACE_BOSS_P3)      hdr_l = "FINAL DUEL";
    else if (st->mode_kind == DR3_UI_RACE_BOSS_P1) hdr_l = "THE SWARM";
    else if (battle)                               hdr_l = "SURVIVORS";
    else                                           hdr_l = "STANDINGS";

    if (battle) snprintf(hdr_r, sizeof(hdr_r), "%d/%d ALIVE", st->alive_count, n);
    else        snprintf(hdr_r, sizeof(hdr_r), "%d CARS", n);

    dr3_ui_draw_subhdr(c, ix, py + 3, iw, hdr_l, hdr_r, COL_TXT_GOLD, COL_TXT_SILVER);

    if (n <= 0) {
        dr3_ui_draw_text_c(c, ix, iw, py + 76, "NO RACERS", COL_TXT_DIM, 1);
        return;
    }

    /* --- 2-car Duel / Giant Boss Luxury Cards --- */
    if (n <= 2) {
        for (i = 0; i < n; ++i) {
            const dr3_ui_racer_t *r    = &sorted[i];
            const int             ry   = py + 22 + i * 68;
            const int             rh   = 60;
            const int             hp_p = (r->hp_max > 0 && r->hp > 0) ? ((r->hp * 100 + r->hp_max / 2) / r->hp_max) : 0;
            const uint32_t        sw   = dr3_ui_swatch_color(r->color, r->is_player, r->is_boss);
            char                  ord[12], tag[16], pct[10];

            dr3_canvas_fill(c, ix, ry, iw, rh, r->is_boss ? COL_BOSS_BG : (r->is_player ? COL_PLAYER_BG : COL_PANEL_ALT));
            dr3_canvas_rect(c, ix, ry, iw, rh, r->is_player ? COL_GOLD_MID : (r->is_boss ? COL_TXT_PURP : COL_STEEL_DARK));
            dr3_canvas_fill(c, ix + 2, ry + 2, 4, rh - 4, sw);

            dr3_ui_ordinal(ord, sizeof(ord), (r->position > 0) ? r->position : (i + 1));
            dr3_ui_draw_text(c, ix + 10, ry + 6, ord, dr3_ui_rank_color(r->position, r->is_player, r->hp <= 0), 1);

            if (r->finished)      snprintf(tag, sizeof(tag), "FINISHED");
            else if (r->hp <= 0)  snprintf(tag, sizeof(tag), "WRECKED!");
            else if (r->is_boss)  snprintf(tag, sizeof(tag), st->boss_enraged ? "ENRAGED!" : "GIANT BOSS");
            else if (battle)      snprintf(tag, sizeof(tag), "FIGHTING");
            else                  snprintf(tag, sizeof(tag), "LAP %d/%d", r->lap, st->total_laps);

            dr3_ui_draw_text_r(c, ix + iw - 4, ry + 6, tag,
                               r->finished ? COL_TXT_GOLD : (r->hp <= 0 ? COL_TXT_RED : (r->is_boss && st->boss_enraged ? COL_TXT_RED : COL_TXT_SILVER)), 1);

            dr3_ui_draw_text_trunc(c, ix + 10, ry + 21, r->name[0] ? r->name : "RACER", 18,
                                   r->is_player ? COL_TXT_GOLD : (r->is_boss ? COL_TXT_PURP : COL_TXT_WHITE), 1);

            snprintf(pct, sizeof(pct), "%d%%", hp_p);
            dr3_ui_draw_text(c, ix + 10, ry + 35, "HULL", COL_TXT_DIM, 1);
            dr3_ui_draw_text_r(c, ix + iw - 4, ry + 35, pct, r->is_boss ? (st->boss_enraged ? COL_TXT_RED : COL_TXT_PURP) : dr3_ui_hp_color(hp_p), 1);
            dr3_ui_draw_bar(c, ix + 10, ry + 46, iw - 14, 9, hp_p,
                            r->is_boss ? (st->boss_enraged ? COL_TXT_RED : COL_TXT_PURP) : dr3_ui_hp_color(hp_p),
                            0x1A0C10u, 1);
        }
        return;
    }

    /* --- 3..4-car Classic Race Spacious Cards --- */
    if (n <= 4) {
        for (i = 0; i < n; ++i) {
            const dr3_ui_racer_t *r    = &sorted[i];
            const int             ry   = py + 19 + i * 35;
            const int             rh   = 32;
            const int             hp_p = (r->hp_max > 0 && r->hp > 0) ? ((r->hp * 100 + r->hp_max / 2) / r->hp_max) : 0;
            const uint32_t        sw   = dr3_ui_swatch_color(r->color, r->is_player, r->is_boss);
            char                  ord[8], tag[12];

            dr3_canvas_fill(c, ix, ry, iw, rh, r->is_player ? COL_PLAYER_BG : ((i & 1) ? COL_PANEL_ALT : COL_PANEL_BG));
            dr3_canvas_rect(c, ix, ry, iw, rh, r->is_player ? COL_GOLD_MID : COL_STEEL_LO);
            dr3_canvas_fill(c, ix + 2, ry + 2, 4, rh - 4, sw);

            dr3_ui_ordinal(ord, sizeof(ord), (r->position > 0) ? r->position : (i + 1));
            dr3_ui_draw_text(c, ix + 9, ry + 5, ord, dr3_ui_rank_color(r->position, r->is_player, r->hp <= 0), 1);
            dr3_ui_draw_text_trunc(c, ix + 31, ry + 5, r->name[0] ? r->name : "RACER", 11,
                                   r->hp <= 0 ? COL_TXT_DIM : (r->is_player ? COL_TXT_GOLD : COL_TXT_WHITE), 1);

            if (r->finished)      snprintf(tag, sizeof(tag), "FIN");
            else if (r->hp <= 0)  snprintf(tag, sizeof(tag), "OUT");
            else if (battle)      snprintf(tag, sizeof(tag), "%d%%", hp_p);
            else                  snprintf(tag, sizeof(tag), "L%d", r->lap);

            dr3_ui_draw_text_r(c, ix + iw - 4, ry + 5, tag,
                               r->finished ? COL_TXT_GOLD : (r->hp <= 0 ? COL_TXT_RED : COL_TXT_SILVER), 1);

            dr3_ui_draw_bar(c, ix + 9, ry + 18, iw - 13, 8, hp_p,
                            r->is_boss ? COL_TXT_PURP : dr3_ui_hp_color(hp_p), 0x1A0C10u, 1);
        }
        return;
    }

    /* --- 5..21-car Compact Windowed Standings (11 crisp rows, always shows leaders + player) --- */
    {
        int rows[11];
        const int shown = dr3_ui_select_window(n, player_sorted, 11, rows);

        for (i = 0; i < shown; ++i) {
            const int ry = py + 17 + i * 13;

            if (rows[i] < 0) {
                dr3_canvas_fill(c, ix, ry, iw, 12, COL_PANEL_BG);
                dr3_ui_draw_text_c(c, ix, iw, ry + 2, ". . . . .", COL_TXT_DIM, 0);
                continue;
            }

            {
                const dr3_ui_racer_t *r    = &sorted[rows[i]];
                const int             pos  = (r->position > 0) ? r->position : (rows[i] + 1);
                const int             hp_p = (r->hp_max > 0 && r->hp > 0) ? ((r->hp * 100 + r->hp_max / 2) / r->hp_max) : 0;
                const uint32_t        sw   = dr3_ui_swatch_color(r->color, r->is_player, r->is_boss);
                char                  pbuf[8];

                if (r->is_player) {
                    dr3_canvas_fill(c, ix, ry, iw, 12, COL_PLAYER_BG);
                    dr3_canvas_fill(c, ix, ry, iw, 1, COL_GOLD_LO);
                    dr3_canvas_fill(c, ix, ry + 11, iw, 1, COL_GOLD_LO);
                }
                else if (i & 1) {
                    dr3_canvas_fill(c, ix, ry, iw, 12, COL_PANEL_ALT);
                }

                /* car colour bar */
                dr3_canvas_fill(c, ix + 1, ry + 2, 3, 8, sw);

                /* rank number (2 chars right-aligned, 12px) */
                snprintf(pbuf, sizeof(pbuf), "%2d", pos);
                dr3_ui_draw_text(c, ix + 6, ry + 3, pbuf, dr3_ui_rank_color(pos, r->is_player, r->hp <= 0), 1);

                /* driver name (11 chars max = 66px) */
                dr3_ui_draw_text_trunc(c, ix + 21, ry + 3, r->name[0] ? r->name : "RACER", 11,
                                       r->hp <= 0 ? COL_TXT_DIM : (r->is_player ? COL_TXT_GOLD : (r->is_boss ? COL_TXT_PURP : COL_TXT_WHITE)), 1);

                /* right status column (x+92..137, 45px) */
                if (r->finished) {
                    dr3_ui_draw_text_r(c, ix + iw - 2, ry + 3, "FIN", COL_TXT_GOLD, 1);
                }
                else if (r->hp <= 0) {
                    dr3_ui_draw_text_r(c, ix + iw - 2, ry + 3, "OUT", COL_TXT_RED, 1);
                }
                else if (battle) {
                    dr3_ui_draw_bar(c, ix + 94, ry + 3, 42, 6, hp_p,
                                    r->is_boss ? COL_TXT_PURP : dr3_ui_hp_color(hp_p), 0x1A0C10u, 0);
                }
                else {
                    char lbuf[6];
                    snprintf(lbuf, sizeof(lbuf), "L%d", r->lap);
                    dr3_ui_draw_text(c, ix + 91, ry + 3, lbuf, COL_TXT_SILVER, 1);
                    dr3_ui_draw_bar(c, ix + 109, ry + 3, 27, 6, hp_p,
                                    r->is_boss ? COL_TXT_PURP : dr3_ui_hp_color(hp_p), 0x1A0C10u, 0);
                }
            }
        }
    }
}

static void dr3_ui_draw_race_map_footer(const dr3_canvas_t *c, const dr3_ui_race_t *st)
{
    const int fx = 8, fy = 154, fw = 154;

    dr3_canvas_fill(c, fx, fy - 1, fw, 1, COL_STEEL_MID);
    dr3_canvas_fill(c, fx, fy, fw, 28, COL_PANEL_BG);

    if (st->mode_kind == DR3_UI_RACE_BOSS_P3) {
        if (st->boss_defeated) {
            dr3_ui_draw_text_c(c, fx, fw, fy + 4,  "ADVERSARY DESTROYED!", COL_TXT_GOLD, 1);
            dr3_ui_draw_text_c(c, fx, fw, fy + 16, "HAIL TO THE KING!",    COL_TXT_AMBER, 1);
        }
        else {
            const int pct = (st->boss_hp_max > 0 && st->boss_hp > 0) ? ((st->boss_hp * 100) / st->boss_hp_max) : 0;
            char      rbuf[16];
            snprintf(rbuf, sizeof(rbuf), st->boss_enraged ? "ENRAGED %d%%" : "BOSS %d%%", pct);
            dr3_ui_draw_text(c, fx + 2, fy + 3, "ADVERSARY", st->boss_enraged ? COL_TXT_RED : COL_TXT_PURP, 1);
            dr3_ui_draw_text_r(c, fx + fw - 2, fy + 3, rbuf, st->boss_enraged ? COL_TXT_RED : COL_TXT_GOLD, 1);
            dr3_ui_draw_bar(c, fx + 2, fy + 14, fw - 4, 10, pct,
                            st->boss_enraged ? COL_TXT_RED : COL_TXT_PURP, 0x220818u, 1);
        }
    }
    else if (st->mode_kind == DR3_UI_RACE_BOSS_P1) {
        const int tot = (st->swarm_total > 0) ? st->swarm_total : 20;
        const int pct = (st->swarm_left * 100) / tot;
        char      rbuf[24];
        snprintf(rbuf, sizeof(rbuf), "%d/%d", st->swarm_left, tot);
        dr3_ui_draw_text(c, fx + 2, fy + 3, "HOSTILES LEFT", COL_TXT_GOLD, 1);
        dr3_ui_draw_text_r(c, fx + fw - 2, fy + 3, rbuf, COL_TXT_RED, 1);
        dr3_ui_draw_bar(c, fx + 2, fy + 14, fw - 4, 10, pct, COL_TXT_RED, 0x1E0808u, 1);
    }
    else if ((st->mode_kind == DR3_UI_RACE_ARENA) || (st->mode_kind == DR3_UI_RACE_DM)) {
        char abuf[24];
        snprintf(abuf, sizeof(abuf), "ALIVE: %d / %d", st->alive_count, st->n_racers);
        dr3_ui_draw_text_c(c, fx, fw, fy + 3, abuf, COL_TXT_GOLD, 1);
        if (st->ring_state == 2)      dr3_ui_draw_text_c(c, fx, fw, fy + 16, "!! GET INSIDE RING !!", COL_TXT_RED,   1);
        else if (st->ring_state == 1) dr3_ui_draw_text_c(c, fx, fw, fy + 16, "RING CLOSING - STAY IN", COL_TXT_AMBER, 1);
        else                          dr3_ui_draw_text_c(c, fx, fw, fy + 16, "LAST CAR STANDING",      COL_TXT_GREEN, 1);
    }
    else if (st->new_record_flash) {
        char buf[32];
        snprintf(buf, sizeof(buf), "NEW RECORD %s!", st->rec_str);
        dr3_ui_draw_text_c(c, fx, fw, fy + 4,  "*** TRACK RECORD ***", COL_TXT_GOLD, 1);
        dr3_ui_draw_text_c(c, fx, fw, fy + 16, buf,                    COL_TXT_WHITE, 1);
    }
    else {
        char l1[32], l2[32];
        snprintf(l1, sizeof(l1), "LAST %s", st->last_str[0] ? st->last_str : "--:--.--");
        snprintf(l2, sizeof(l2), "BEST %s", st->best_str[0] ? st->best_str : "--:--.--");
        dr3_ui_draw_text(c, fx + 2, fy + 4,  l1, COL_TXT_SILVER, 1);
        dr3_ui_draw_text(c, fx + 2, fy + 16, l2, COL_TXT_GOLD,   1);
        if (st->rec_str[0]) {
            char r1[16];
            snprintf(r1, sizeof(r1), "%s", st->rec_str);
            dr3_ui_draw_text_r(c, fx + fw - 2, fy + 4,  "REC", COL_TXT_DIM, 1);
            dr3_ui_draw_text_r(c, fx + fw - 2, fy + 16, r1,    COL_TXT_CYAN, 1);
        }
    }
}

static void dr3_ui_draw_race_telemetry(const dr3_canvas_t *c, const dr3_ui_race_t *st)
{
    const int tx = 4, ty = 188, tw = 312, th = 48;
    const int battle = (st->mode_kind == DR3_UI_RACE_ARENA) ||
                       (st->mode_kind == DR3_UI_RACE_BOSS_P1) ||
                       (st->mode_kind == DR3_UI_RACE_BOSS_P3);
    char      ord[16], pstr[28], lstr[24], sstr[16], hstr[16], mstr[16];
    int       i, dots;

    dr3_ui_draw_panel(c, tx, ty, tw, th, 0, (st->mode_kind >= DR3_UI_RACE_BOSS_P1));

    /* horizontal mid divider */
    dr3_canvas_fill(c, tx + 4, ty + 22, tw - 8, 1, COL_STEEL_DARK);

    /* vertical dividers on Row 1 */
    dr3_canvas_fill(c, 90,  ty + 4, 1, 16, COL_STEEL_DARK);
    dr3_canvas_fill(c, 164, ty + 4, 1, 16, COL_STEEL_DARK);
    dr3_canvas_fill(c, 232, ty + 4, 1, 16, COL_STEEL_DARK);

    /* Cell 1: POS */
    dr3_ui_ordinal(ord, sizeof(ord), (st->player_pos > 0) ? st->player_pos : 1);
    snprintf(pstr, sizeof(pstr), "%s/%d", ord, (st->n_racers > 0) ? st->n_racers : 4);
    dr3_ui_draw_text(c, 10, ty + 8, "POS", COL_TXT_DIM, 1);
    dr3_ui_draw_text(c, 33, ty + 8, pstr, dr3_ui_rank_color(st->player_pos, 0, st->hull_pct <= 0), 1);

    /* Cell 2: LAP or ALIVE */
    if (battle) {
        snprintf(lstr, sizeof(lstr), "%d/%d", st->alive_count, st->n_racers);
        dr3_ui_draw_text(c, 96,  ty + 8, "ALV", COL_TXT_DIM, 1);
        dr3_ui_draw_text(c, 120, ty + 8, lstr,  COL_TXT_GOLD, 1);
    }
    else if (st->mode_kind == DR3_UI_RACE_DM) {
        snprintf(lstr, sizeof(lstr), "%d", st->player_lap);
        dr3_ui_draw_text(c, 96,  ty + 8, "LAP", COL_TXT_DIM, 1);
        dr3_ui_draw_text(c, 120, ty + 8, lstr,  COL_TXT_WHITE, 1);
    }
    else {
        snprintf(lstr, sizeof(lstr), "%d/%d", st->player_lap, (st->total_laps > 0) ? st->total_laps : 1);
        dr3_ui_draw_text(c, 96,  ty + 8, "LAP", COL_TXT_DIM, 1);
        dr3_ui_draw_text(c, 120, ty + 8, lstr,  COL_TXT_WHITE, 1);
    }

    /* Cell 3: SPEED MPH */
    snprintf(sstr, sizeof(sstr), "%3d", (st->speed_mph < 0) ? 0 : st->speed_mph);
    dr3_ui_draw_text(c, 170, ty + 8, sstr,  COL_TXT_CYAN, 1);
    dr3_ui_draw_text(c, 193, ty + 8, "MPH", COL_TXT_DIM, 1);

    /* Cell 4: MINES (visual hazard orbs + numeric count) */
    dr3_ui_draw_text(c, 238, ty + 8, "MNE", COL_TXT_DIM, 1);
    dots = (st->mines > 5) ? 5 : ((st->mines < 0) ? 0 : st->mines);
    for (i = 0; i < 5; ++i) {
        const int mx = 260 + i * 7;
        const int my = ty + 9;
        if (i < dots) {
            dr3_canvas_fill(c, mx, my, 5, 5, COL_FIRE_ORANGE);
            dr3_canvas_fill(c, mx + 1, my + 1, 3, 3, COL_GOLD_HI);
        }
        else {
            dr3_canvas_fill(c, mx, my, 5, 5, 0x201C20u);
            dr3_canvas_rect(c, mx, my, 5, 5, COL_STEEL_DARK);
        }
    }
    snprintf(mstr, sizeof(mstr), "%d", (st->mines < 0) ? 0 : st->mines);
    dr3_ui_draw_text_r(c, 310, ty + 8, mstr, (st->mines > 0) ? COL_TXT_GOLD : COL_TXT_DIM, 1);

    /* Row 2 (y = ty + 28): HULL, TURBO, AMMO gauges */
    {
        const int ry = ty + 29;
        snprintf(hstr, sizeof(hstr), "%3d%%", st->hull_pct);
        dr3_ui_draw_text(c, 10, ry + 1, "HUL", COL_TXT_DIM, 1);
        dr3_ui_draw_bar(c, 31, ry, 50, 9, st->hull_pct, dr3_ui_hp_color(st->hull_pct), 0x1A0A0Au, 1);
        dr3_ui_draw_text(c, 84, ry + 1, hstr, dr3_ui_hp_color(st->hull_pct), 1);

        dr3_ui_draw_text(c, 118, ry + 1, "TRB", COL_TXT_DIM, 1);
        dr3_ui_draw_bar(c, 139, ry, 68, 9, st->turbo_pct, COL_TXT_CYAN, 0x0A1822u, 1);

        dr3_ui_draw_text(c, 215, ry + 1, "AMO", COL_TXT_DIM, 1);
        if (st->weapons_on) {
            dr3_ui_draw_bar(c, 236, ry, 74, 9, st->ammo_pct, COL_TXT_AMBER, 0x201408u, 1);
        }
        else {
            dr3_canvas_fill(c, 236, ry, 74, 9, 0x161418u);
            dr3_canvas_rect(c, 236, ry, 74, 9, COL_STEEL_DARK);
            dr3_ui_draw_text_c(c, 236, 74, ry + 1, "OFF", COL_TXT_DIM, 0);
        }
    }
}

void dr3_ui_draw_race(const dr3_canvas_t *c, const dr3_ui_race_t *st,
                      const dr3_map_car_t *map_cars, int n_map_cars, int draw_map)
{
    if (!c || !st) return;

    if (draw_map) {
        /* Full redraw: crimson backdrop + left minimap panel frame + static track map */
        dr3_ui_draw_backdrop(c);
        dr3_ui_draw_panel(c, 4, 22, 162, 164, 0, (st->mode_kind >= DR3_UI_RACE_BOSS_P1));
        dr3_canvas_rect(c, DR3_UI_RACE_MAP_X - 1, DR3_UI_RACE_MAP_Y - 1,
                        DR3_UI_RACE_MAP_W + 2, DR3_UI_RACE_MAP_H + 2, COL_STEEL_DARK);
        dr3_minimap_draw(c, DR3_UI_RACE_MAP_X, DR3_UI_RACE_MAP_Y,
                         DR3_UI_RACE_MAP_W, DR3_UI_RACE_MAP_H, map_cars, n_map_cars);
    }
    else {
        /* Incremental tick: keep backdrop & static track map, only move car markers & closing ring */
        dr3_minimap_draw_incremental(c, DR3_UI_RACE_MAP_X, DR3_UI_RACE_MAP_Y,
                                     DR3_UI_RACE_MAP_W, DR3_UI_RACE_MAP_H, map_cars, n_map_cars);
    }

    /* Header bar */
    dr3_ui_draw_header_bar(c, 20);
    dr3_ui_draw_text_trunc(c, 7, 6, st->title[0] ? st->title : "DEATH RALLY", 28,
                           (st->mode_kind >= DR3_UI_RACE_BOSS_P1) ? COL_TXT_GOLD : COL_TXT_WHITE, 1);
    dr3_ui_draw_text_r(c, 313, 6, st->clock_str[0] ? st->clock_str : "TAP:OFF",
                       (st->clock_str[0] == 'T' && st->clock_str[1] == 'A') ? COL_TXT_DIM : COL_TXT_GOLD, 1);

    /* Left panel footer (lap times / boss HP / swarm / arena status) */
    dr3_ui_draw_race_map_footer(c, st);

    /* Right panel: Live Race Standings */
    dr3_ui_draw_race_standings(c, st);

    /* Bottom strip: Vehicle Telemetry */
    dr3_ui_draw_race_telemetry(c, st);
}

/* ========================================================================= MENU COMMAND CENTER === */

static void dr3_ui_draw_gear_pill(const dr3_canvas_t *c, int x, int y, const char *lbl, int active, uint32_t on_col)
{
    const int w = dr3_ui_text_w(lbl) + 6;
    dr3_canvas_fill(c, x, y, w, 10, active ? 0x241808u : 0x101822u);
    dr3_canvas_rect(c, x, y, w, 10, active ? on_col : 0x223448u);
    dr3_ui_draw_text(c, x + 3, y + 2, lbl, active ? on_col : 0x3E5266u, active ? 1 : 0);
}

static void dr3_ui_draw_menu_garage(const dr3_canvas_t *c, const dr3_ui_menu_t *st)
{
    const int gx = 4, gy = 26, gw = 312, gh = 50;

    dr3_ui_draw_panel(c, gx, gy, gw, gh, 1, (st->boss_phase > 0) || st->cheat_active);

    if (!st->has_career) {
        dr3_ui_draw_text_c(c, gx, gw, gy + 8,  "30TH ANNIVERSARY EDITION  -  3DS", COL_TXT_GOLD, 1);
        dr3_ui_draw_text_c(c, gx, gw, gy + 21, "START NEW CAREER OR LOAD SAVEGAME", COL_TXT_WHITE, 1);
        dr3_ui_draw_text_c(c, gx, gw, gy + 35, "FULLSCREEN RACE  -  TAP BOTTOM TO DARKEN", COL_TXT_CYAN, 1);
        return;
    }

    /* Left side: Driver, Car, Cash, Rank, W/R, Underground Gear */
    {
        char top[32], mid[36];
        const uint32_t sw = dr3_ui_swatch_color(st->player_color, 1, 0);

        dr3_canvas_fill(c, gx + 6, gy + 6, 4, 9, sw);
        snprintf(top, sizeof(top), "%s", st->player_name[0] ? st->player_name : "DRIVER");
        dr3_ui_draw_text_trunc(c, gx + 13, gy + 7, top, 11, COL_TXT_GOLD, 1);
        if (st->car_name[0]) {
            dr3_ui_draw_text_r(c, gx + 172, gy + 7, st->car_name, COL_TXT_SILVER, 1);
        }

        snprintf(mid, sizeof(mid), "$%d", st->cash);
        dr3_ui_draw_text(c, gx + 6, gy + 20, mid, COL_TXT_GREEN, 1);

        if (st->rank > 0) {
            snprintf(mid, sizeof(mid), "#%d", st->rank);
            dr3_ui_draw_text(c, gx + 82, gy + 20, mid, COL_TXT_GOLD, 1);
        }

        snprintf(mid, sizeof(mid), "W:%d R:%d", st->wins, st->races);
        dr3_ui_draw_text_r(c, gx + 172, gy + 20, mid, COL_TXT_SILVER, 1);

        /* Underground gear status pills */
        dr3_ui_draw_gear_pill(c, gx + 6,   gy + 34, "MNE", st->has_mines,    COL_TXT_AMBER);
        dr3_ui_draw_gear_pill(c, gx + 34,  gy + 34, "SPK", st->has_spikes,   COL_TXT_CYAN);
        dr3_ui_draw_gear_pill(c, gx + 62,  gy + 34, "RKT", st->has_rocket,   COL_TXT_RED);
        dr3_ui_draw_gear_pill(c, gx + 90,  gy + 34, "SAB", st->has_sabotage, COL_TXT_PURP);
        if (st->cheat_active) {
            dr3_ui_draw_gear_pill(c, gx + 122, gy + 34, "GODMODE", 1, COL_TXT_GOLD);
        }
        else if (st->boss_phase > 0) {
            char btag[20];
            snprintf(btag, sizeof(btag), "BOSS %d/3", st->boss_phase);
            dr3_ui_draw_gear_pill(c, gx + 118, gy + 34, btag, 1, COL_TXT_RED);
        }
    }

    /* Vertical steel divider */
    dr3_canvas_fill(c, gx + 177, gy + 4, 1, gh - 8, COL_STEEL_MID);

    /* Right side: Authentic segmented ENG / TIR / ARM + HULL bars */
    {
        const int rx = gx + 182;
        char      hstr[8];

        dr3_ui_draw_text(c, rx, gy + 6,  "ENG", COL_TXT_SILVER, 1);
        dr3_ui_draw_upgrade_pips(c, rx + 22, gy + 6,  st->engine, st->engine_max);

        dr3_ui_draw_text(c, rx, gy + 16, "TIR", COL_TXT_SILVER, 1);
        dr3_ui_draw_upgrade_pips(c, rx + 22, gy + 16, st->tires,  st->tires_max);

        dr3_ui_draw_text(c, rx, gy + 26, "ARM", COL_TXT_SILVER, 1);
        dr3_ui_draw_upgrade_pips(c, rx + 22, gy + 26, st->armor,  st->armor_max);

        dr3_ui_draw_text(c, rx, gy + 37, "HUL", COL_TXT_SILVER, 1);
        dr3_ui_draw_bar(c, rx + 22, gy + 36, 56, 8, st->hull_pct, dr3_ui_hp_color(st->hull_pct), 0x160A0Cu, 1);
        snprintf(hstr, sizeof(hstr), "%3d%%", st->hull_pct);
        dr3_ui_draw_text_r(c, gx + gw - 6, gy + 37, hstr, dr3_ui_hp_color(st->hull_pct), 1);
    }
}

static void dr3_ui_draw_menu_controls_options(const dr3_canvas_t *c, const dr3_ui_menu_t *st)
{
    static const struct { const char *k1; const char *a1; const char *k2; const char *a2; } rows[5] = {
        { "R/UP", "GAS",   "L/DN", "BRK"  },
        { "DPAD", "STEER", "A",    "HORN" },
        { "B/ZL", "BOOST", "Y/ZR", "GUN"  },
        { "X",    "MINE",  "ST",   "PAUS" },
        { "A/B",  "MENU",  "SEL",  "KYBD" }
    };
    const int px = 4, py = 79, pw = 154, ph = 132;
    const int ix = px + 4, iw = pw - 8;
    int       i;

    dr3_ui_draw_panel(c, px, py, pw, ph, 0, 0);
    dr3_ui_draw_subhdr(c, ix, py + 3, iw, "CONTROLS", "3DS PAD", COL_TXT_GOLD, COL_TXT_DIM);

    /* Controls keycap grid */
    for (i = 0; i < 5; ++i) {
        const int ry = py + 17 + i * 12;
        int       w1, w2;

        w1 = dr3_ui_draw_keycap(c, ix + 1, ry, rows[i].k1, COL_TXT_GOLD);
        dr3_ui_draw_text(c, ix + 3 + w1, ry + 2, rows[i].a1, COL_TXT_WHITE, 1);

        w2 = dr3_ui_draw_keycap(c, ix + 75, ry, rows[i].k2, COL_TXT_GOLD);
        dr3_ui_draw_text(c, ix + 77 + w2, ry + 2, rows[i].a2, COL_TXT_WHITE, 1);
    }

    /* Live Options section */
    {
        const int oy = py + 79;
        char      mbuf[8], sbuf[8];

        dr3_ui_draw_subhdr(c, ix, oy, iw, "OPTIONS", st->chaos_active ? "L+R+Y MODE" : "SETTINGS", COL_TXT_GOLD, COL_TXT_DIM);

        snprintf(mbuf, sizeof(mbuf), "%3d%%", st->music_pct);
        dr3_ui_draw_text(c, ix + 2, oy + 14, "MUS", COL_TXT_DIM, 1);
        dr3_ui_draw_bar(c, ix + 23, oy + 14, 34, 7, st->music_pct, COL_TXT_AMBER, 0x1A120Cu, 0);
        dr3_ui_draw_text(c, ix + 60, oy + 14, mbuf, COL_TXT_SILVER, 1);
        dr3_ui_draw_text(c, ix + 92, oy + 14, "WPN:", COL_TXT_DIM, 1);
        dr3_ui_draw_text_r(c, ix + iw - 2, oy + 14, st->weapons_on ? "ON" : "OFF",
                           st->weapons_on ? COL_TXT_GREEN : COL_TXT_RED, 1);

        snprintf(sbuf, sizeof(sbuf), "%3d%%", st->sfx_pct);
        dr3_ui_draw_text(c, ix + 2, oy + 26, "SFX", COL_TXT_DIM, 1);
        dr3_ui_draw_bar(c, ix + 23, oy + 26, 34, 7, st->sfx_pct, COL_TXT_CYAN, 0x0C161Eu, 0);
        dr3_ui_draw_text(c, ix + 60, oy + 26, sbuf, COL_TXT_SILVER, 1);
        dr3_ui_draw_text(c, ix + 92, oy + 26, "SHD:", COL_TXT_DIM, 1);
        dr3_ui_draw_text_r(c, ix + iw - 2, oy + 26, st->shadows_on ? "ON" : "OFF",
                           st->shadows_on ? COL_TXT_GREEN : COL_TXT_DIM, 1);

        dr3_ui_draw_text(c, ix + 2, oy + 38, "MODE:", COL_TXT_DIM, 1);
        dr3_ui_draw_text_trunc(c, ix + 34, oy + 38, st->mode_name[0] ? st->mode_name : "CLASSIC", 9, COL_TXT_GOLD, 1);
        dr3_ui_draw_text(c, ix + 92, oy + 38, "TEX:", COL_TXT_DIM, 1);
        dr3_ui_draw_text_r(c, ix + iw - 2, oy + 38, st->textures_on ? "ON" : "OFF",
                           st->textures_on ? COL_TXT_GREEN : COL_TXT_DIM, 1);
    }
}

static void dr3_ui_draw_menu_standings(const dr3_canvas_t *c, const dr3_ui_menu_t *st)
{
    const int px = 161, py = 79, pw = 155, ph = 132;
    const int ix = px + 4, iw = pw - 8;
    int       i;

    dr3_ui_draw_panel(c, px, py, pw, ph, 0, 0);

    if (st->n_standings <= 0) {
        dr3_ui_draw_subhdr(c, ix, py + 3, iw, "30TH ANNIV.", "GUIDE", COL_TXT_GOLD, COL_TXT_DIM);
        dr3_ui_draw_text(c, ix + 4, py + 21, "- 20-CAR CHAOS RACES", COL_TXT_GOLD,   1);
        dr3_ui_draw_text(c, ix + 4, py + 34, "- DUEL / DESTROY ALL", COL_TXT_SILVER, 1);
        dr3_ui_draw_text(c, ix + 4, py + 47, "- BATTLE ARENA + RING", COL_TXT_SILVER, 1);
        dr3_ui_draw_text(c, ix + 4, py + 60, "- 3-PHASE BOSS FINAL", COL_TXT_AMBER,  1);
        dr3_ui_draw_text(c, ix + 4, py + 73, "- MINE CRATE PICKUPS", COL_TXT_SILVER, 1);
        dr3_canvas_fill(c, ix + 2, py + 86, iw - 4, 1, COL_STEEL_DARK);
        dr3_ui_draw_text(c, ix + 4, py + 93,  "RANKINGS APPEAR",    COL_TXT_DIM, 1);
        dr3_ui_draw_text(c, ix + 4, py + 105, "ONCE CAREER LOADS.", COL_TXT_DIM, 1);
        return;
    }

    dr3_ui_draw_subhdr(c, ix, py + 3, iw, "RANKINGS", "PTS", COL_TXT_GOLD, COL_TXT_DIM);

    {
        int player_idx = -1;
        int rows[10];
        int shown;

        for (i = 0; i < st->n_standings; ++i) {
            if (st->standings[i].is_player) { player_idx = i; break; }
        }
        shown = dr3_ui_select_window(st->n_standings, player_idx, 10, rows);

        for (i = 0; i < shown; ++i) {
            const int ry = py + 17 + i * 11;

            if (rows[i] < 0) {
                dr3_ui_draw_text_c(c, ix, iw, ry + 2, ". . . . .", COL_TXT_DIM, 0);
                continue;
            }

            {
                const dr3_ui_standing_t *s  = &st->standings[rows[i]];
                const int                rk = rows[i] + 1;
                const uint32_t           sw = dr3_ui_swatch_color(s->color, s->is_player, 0);
                char                     rbuf[16], pbuf[16];

                if (s->is_player) {
                    dr3_canvas_fill(c, ix, ry, iw, 10, COL_PLAYER_BG);
                    dr3_canvas_fill(c, ix, ry, iw, 1, COL_GOLD_LO);
                }
                else if (i & 1) {
                    dr3_canvas_fill(c, ix, ry, iw, 10, COL_PANEL_ALT);
                }

                dr3_canvas_fill(c, ix + 1, ry + 1, 3, 8, sw);

                snprintf(rbuf, sizeof(rbuf), "%2d", rk);
                dr3_ui_draw_text(c, ix + 6, ry + 2, rbuf, dr3_ui_rank_color(rk, s->is_player, 0), 1);

                dr3_ui_draw_text_trunc(c, ix + 21, ry + 2, s->name, 14,
                                       s->is_player ? COL_TXT_GOLD : COL_TXT_WHITE, 1);

                snprintf(pbuf, sizeof(pbuf), "%d", s->points);
                dr3_ui_draw_text_r(c, ix + iw - 2, ry + 2, pbuf,
                                   s->is_player ? COL_TXT_GOLD : COL_TXT_SILVER, 1);
            }
        }
    }
}

static void dr3_ui_draw_menu_footer(const dr3_canvas_t *c, const dr3_ui_menu_t *st)
{
    const int fx = 4, fy = 214, fw = 312, fh = 23;
    int       x  = fx + 6;
    const char *qs = st->qsave_key[0] ? st->qsave_key : "ZL,X";
    const char *ql = st->qload_key[0] ? st->qload_key : "ZR,Y";

    dr3_ui_draw_panel(c, fx, fy, fw, fh, 0, 0);

    x += dr3_ui_draw_keycap(c, x, fy + 6, qs, COL_TXT_GOLD) + 3;
    dr3_ui_draw_text(c, x, fy + 8, "SAVE", COL_TXT_SILVER, 1);
    x += 28;

    x += dr3_ui_draw_keycap(c, x, fy + 6, ql, COL_TXT_GOLD) + 3;
    dr3_ui_draw_text(c, x, fy + 8, "LOAD", COL_TXT_SILVER, 1);
    x += 28;

    x += dr3_ui_draw_keycap(c, x, fy + 6, "L+R+ST", COL_TXT_AMBER) + 3;
    dr3_ui_draw_text(c, x, fy + 8, "QUIT", COL_TXT_SILVER, 1);

    dr3_ui_draw_text_r(c, fx + fw - 6, fy + 8, "A:OK B:NEXT", COL_TXT_DIM, 1);
}

void dr3_ui_draw_menu(const dr3_canvas_t *c, const dr3_ui_menu_t *st)
{
    if (!c || !st) return;

    /* Crimson sunburst backdrop + metallic Death Rally header */
    dr3_ui_draw_backdrop(c);
    dr3_ui_draw_header_bar(c, 23);

    dr3_ui_draw_text_bold(c, 8, 7, "DEATH RALLY", COL_TXT_GOLD);

    if (st->badge[0]) {
        const int bw = dr3_ui_text_w(st->badge) + 8;
        int       bx = 94 + (168 - bw) / 2;
        if (bx < 92) bx = 92;
        dr3_canvas_fill(c, bx, 5, bw, 12, (st->boss_phase > 0) ? 0x4A0C0Cu : 0x1A2430u);
        dr3_canvas_rect(c, bx, 5, bw, 12, (st->boss_phase > 0) ? COL_TXT_RED : COL_GOLD_MID);
        dr3_ui_draw_text(c, bx + 4, 8, st->badge, (st->boss_phase > 0) ? COL_TXT_GOLD : COL_TXT_WHITE, 1);
    }
    dr3_ui_draw_text_r(c, 313, 8, "TAP:OFF", COL_TXT_DIM, 1);

    /* Top garage/driver card */
    dr3_ui_draw_menu_garage(c, st);

    /* Middle split: Left = Controls & Live Options, Right = Championship Standings */
    dr3_ui_draw_menu_controls_options(c, st);
    dr3_ui_draw_menu_standings(c, st);

    /* Footer bar */
    dr3_ui_draw_menu_footer(c, st);
}
