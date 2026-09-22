/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_minimap.h"

#include <stddef.h>
#include <string.h>

/* the small map: one byte per drawn pixel (class) and the color that goes with it, one row per
   DR3_MINIMAP_MAX_W (simple indexing) */
static uint8_t  dr3_map_bits[DR3_MINIMAP_MAX_W * DR3_MINIMAP_MAX_H];
static uint32_t dr3_map_col [DR3_MINIMAP_MAX_W * DR3_MINIMAP_MAX_H];
static uint32_t dr3_map_palette[256];        /* the track's own colors, scaled to 0..255 */
static int      dr3_map_have_palette;
static int      dr3_map_w, dr3_map_h;
static int      dr3_map_step;
static int      dr3_map_mask_w, dr3_map_mask_h;   /* size of the track the map was built from */
static int      dr3_map_ready;
static int      dr3_map_counts[4];

/*
 * The colors in the header are plain 0xRRGGBB; every canvas converts them for its own pixel format
 * (see DR3_CANVAS_* in dr3_minimap.h) - the top screen is 4 bytes per pixel, the bottom screen of
 * the 3DS is 2 (RGB565), because libctru's console switches it there.
 */

/* ------------------------------------------------------------------ data stage --- */

void dr3_minimap_reset(void)
{
    dr3_map_ready  = 0;
    dr3_map_w      = 0;
    dr3_map_h      = 0;
    dr3_map_step   = 0;
    dr3_map_mask_w = 0;
    dr3_map_mask_h = 0;
}

int  dr3_minimap_ready(void) { return dr3_map_ready; }
int  dr3_minimap_w(void)     { return dr3_map_w; }
int  dr3_minimap_h(void)     { return dr3_map_h; }
int  dr3_minimap_step(void)  { return dr3_map_step; }
const uint8_t *dr3_minimap_bits(void) { return dr3_map_bits; }

void dr3_minimap_class_counts(int counts[4])
{
    int i;

    for (i = 0; i < 4; ++i) counts[i] = dr3_map_counts[i];
}

/* Classifies one block of the track mask and derives its colour from the track's own image.
   See the header for the meaning of the nibble groups. */
typedef struct {
    int      cls;
    uint32_t color;      /* 0xRRGGBB */
} dr3_map_cell_t;

static uint32_t dr3_map_scale(uint32_t rgb, int num, int den)
{
    int r = (int)((rgb >> 16) & 0xffu);
    int g = (int)((rgb >> 8) & 0xffu);
    int b = (int)(rgb & 0xffu);

    r = (r * num) / den;
    g = (g * num) / den;
    b = (b * num) / den;

    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/*
 * The palette of a track image is 256 RGB triplets whose range is either 0..63 (VGA DAC) or 0..100
 * (percent).  Scaling the brightest entry to 255 makes the map look right in both cases.
 */
static void dr3_map_build_palette(const uint8_t *palette)
{
    int n, max = 0;

    dr3_map_have_palette = 0;
    if (!palette) return;

    for (n = 0; n < 0x300; ++n) {
        if (palette[n] > max) max = palette[n];
    }
    if (max <= 0) return;

    for (n = 0; n < 256; ++n) {
        const int r = ((int)palette[3 * n + 0] * 255) / max;
        const int g = ((int)palette[3 * n + 1] * 255) / max;
        const int b = ((int)palette[3 * n + 2] * 255) / max;

        dr3_map_palette[n] = ((uint32_t)(r > 255 ? 255 : r) << 16) |
                             ((uint32_t)(g > 255 ? 255 : g) << 8) |
                              (uint32_t)(b > 255 ? 255 : b);
    }

    dr3_map_have_palette = 1;
}

static dr3_map_cell_t dr3_map_analyse_block(const uint8_t *mask, const uint8_t *image,
                                            int mask_w, int mask_h, int x0, int y0, int step)
{
    static const uint32_t fallback[4] = { DR3_MAP_COL_BACKGROUND, DR3_MAP_COL_OFFROAD,
                                          DR3_MAP_COL_ROAD, DR3_MAP_COL_OTHER };
    dr3_map_cell_t cell;
    int            x, y, n = 0, hard = 0, soft = 0;
    unsigned long  all_r = 0, all_g = 0, all_b = 0;
    unsigned long  road_r = 0, road_g = 0, road_b = 0;

    for (y = y0; y < (y0 + step); ++y) {
        const uint8_t *row;
        const uint8_t *irow;

        if (y >= mask_h) break;

        row  = mask + (size_t)y * (size_t)mask_w;
        irow = (image && dr3_map_have_palette) ? (image + (size_t)y * (size_t)mask_w) : NULL;

        for (x = x0; x < (x0 + step); ++x) {
            int v, is_road;

            if (x >= mask_w) break;
            ++n;

            v       = row[x] & 0x0f;
            is_road = (v == 0x0f);

            if (is_road)    ++hard;
            else if (v < 4) ++soft;

            if (irow) {
                const uint32_t c = dr3_map_palette[irow[x]];

                all_r += (c >> 16) & 0xffu;
                all_g += (c >> 8) & 0xffu;
                all_b += c & 0xffu;

                if (is_road) {
                    road_r += (c >> 16) & 0xffu;
                    road_g += (c >> 8) & 0xffu;
                    road_b += c & 0xffu;
                }
            }
        }
    }

    cell.cls   = DR3_MAP_NONE;
    cell.color = DR3_MAP_COL_OTHER;

    if (n == 0) return cell;                       /* the block lies outside the mask */

    /* A road thinner than the block still has to be visible on the map, so a share of an eighth is
       already enough for it to win.  Everything else is decided by the dominant group. */
    if ((hard * 8) >= n)      cell.cls = DR3_MAP_ROAD;
    else if ((soft * 2) >= n) cell.cls = DR3_MAP_OFFROAD;
    else                      cell.cls = DR3_MAP_OTHER;

    if (image && dr3_map_have_palette) {
        /* start from the average of the whole block (a little darker, so the racing line stands out) */
        uint32_t base = ((uint32_t)(all_r / (unsigned long)n) << 16) |
                        ((uint32_t)(all_g / (unsigned long)n) << 8) |
                         (uint32_t)(all_b / (unsigned long)n);

        base = dr3_map_scale(base, 3, 4);

        if (hard > 0) {
            /* blend the brighter asphalt average in proportionally to its share of the block, so even
               a thin road shows up on the minimap */
            const uint32_t road = dr3_map_scale(((uint32_t)(road_r / (unsigned long)hard) << 16) |
                                                ((uint32_t)(road_g / (unsigned long)hard) << 8) |
                                                 (uint32_t)(road_b / (unsigned long)hard), 5, 4);
            const int      s = (hard * 256) / n;                   /* 0..256 */
            const int      br = (int)((base >> 16) & 0xffu), bg = (int)((base >> 8) & 0xffu);
            const int      bb = (int)(base & 0xffu);
            const int      rr = (int)((road >> 16) & 0xffu), rg = (int)((road >> 8) & 0xffu);
            const int      rb = (int)(road & 0xffu);

            base = ((uint32_t)((br * (256 - s) + rr * s) / 256) << 16) |
                   ((uint32_t)((bg * (256 - s) + rg * s) / 256) << 8) |
                    (uint32_t)((bb * (256 - s) + rb * s) / 256);
        }

        cell.color = base;
    }
    else {
        cell.color = fallback[cell.cls & 3];       /* no track image: the fixed scheme */
    }

    return cell;
}

int dr3_minimap_build(const uint8_t *mask, const uint8_t *image, const uint8_t *palette,
                      int mask_w, int mask_h)
{
    int step, step_y, ox, oy, x, y;

    dr3_minimap_reset();
    if (!mask || (mask_w <= 0) || (mask_h <= 0)) return 0;

    dr3_map_build_palette(palette);

    /* one step for both axes keeps the aspect ratio of the track */
    step   = (mask_w + DR3_MINIMAP_MAX_W - 1) / DR3_MINIMAP_MAX_W;
    step_y = (mask_h + DR3_MINIMAP_MAX_H - 1) / DR3_MINIMAP_MAX_H;
    if (step_y > step) step = step_y;
    if (step < 1)      step = 1;

    ox = (mask_w + step - 1) / step;
    oy = (mask_h + step - 1) / step;
    if (ox > DR3_MINIMAP_MAX_W) ox = DR3_MINIMAP_MAX_W;
    if (oy > DR3_MINIMAP_MAX_H) oy = DR3_MINIMAP_MAX_H;
    if ((ox <= 0) || (oy <= 0)) return 0;

    memset(dr3_map_counts, 0, sizeof(dr3_map_counts));

    for (y = 0; y < oy; ++y) {
        for (x = 0; x < ox; ++x) {
            const dr3_map_cell_t cell = dr3_map_analyse_block(mask, image, mask_w, mask_h,
                                                              x * step, y * step, step);
            const size_t         idx  = (size_t)y * DR3_MINIMAP_MAX_W + x;

            dr3_map_bits[idx] = (uint8_t)cell.cls;
            dr3_map_col [idx] = cell.color;
            ++dr3_map_counts[cell.cls & 3];
        }
    }

    dr3_map_w      = ox;
    dr3_map_h      = oy;
    dr3_map_step   = step;
    dr3_map_mask_w = mask_w;
    dr3_map_mask_h = mask_h;
    dr3_map_ready  = 1;

    return 1;
}

void dr3_minimap_project(float x, float y, int *mx, int *my)
{
    int px = 0, py = 0;

    if (dr3_map_ready && (dr3_map_mask_w > 0) && (dr3_map_mask_h > 0)) {
        px = (int)((x / (float)dr3_map_mask_w) * (float)dr3_map_w);
        py = (int)((y / (float)dr3_map_mask_h) * (float)dr3_map_h);

        if (px < 0)          px = 0;
        if (px >= dr3_map_w) px = dr3_map_w - 1;
        if (py < 0)          py = 0;
        if (py >= dr3_map_h) py = dr3_map_h - 1;
    }

    if (mx) *mx = px;
    if (my) *my = py;
}

/* ------------------------------------------------------------------ draw stage --- */

void dr3_canvas_px(const dr3_canvas_t *c, int x, int y, uint32_t rgb)
{
    ptrdiff_t off;

    if (!c || !c->px) return;
    if ((x < 0) || (y < 0) || (x >= c->w) || (y >= c->h)) return;

    /* signed arithmetic: the 3DS framebuffer has a negative y stride */
    off = (ptrdiff_t)y * (ptrdiff_t)c->stride_y + (ptrdiff_t)x * (ptrdiff_t)c->stride_x;

    switch (c->fmt) {
    case DR3_CANVAS_RGB565:
        ((uint16_t *)c->px)[off] = (uint16_t)(((((rgb >> 16) & 0xffu) >> 3) << 11) |
                                              ((((rgb >> 8) & 0xffu) >> 2) << 5) |
                                               (((rgb & 0xffu) >> 3)));
        break;

    case DR3_CANVAS_BGR888: {
        uint8_t *p = (uint8_t *)c->px + (off * 3);

        p[0] = (uint8_t)(rgb & 0xffu);
        p[1] = (uint8_t)((rgb >> 8) & 0xffu);
        p[2] = (uint8_t)((rgb >> 16) & 0xffu);
        break;
    }

    default:   /* DR3_CANVAS_RGBA8888: R in the lowest byte, then G, B and A */
        ((uint32_t *)c->px)[off] = ((rgb >> 16) & 0xffu) | ((rgb >> 8) & 0xff00u) |
                                   ((rgb & 0xffu) << 16) | 0xff000000u;
        break;
    }
}

void dr3_canvas_fill(const dr3_canvas_t *c, int x0, int y0, int w, int h, uint32_t rgb)
{
    int x, y;

    for (y = y0; y < (y0 + h); ++y) {
        for (x = x0; x < (x0 + w); ++x) dr3_canvas_px(c, x, y, rgb);
    }
}

void dr3_canvas_rect(const dr3_canvas_t *c, int x0, int y0, int w, int h, uint32_t rgb)
{
    if (!c || (w <= 0) || (h <= 0)) return;

    dr3_canvas_fill(c, x0, y0, w, 1, rgb);                 /* top    */
    dr3_canvas_fill(c, x0, y0 + h - 1, w, 1, rgb);         /* bottom */
    dr3_canvas_fill(c, x0, y0, 1, h, rgb);                 /* left   */
    dr3_canvas_fill(c, x0 + w - 1, y0, 1, h, rgb);         /* right  */
}

/* Draws the cars: the player as a yellow marker with a dark outline, everyone else red.  The
   non-player cars go first so that the player marker is never covered by one of them. */
static void dr3_minimap_draw_car(const dr3_canvas_t *c, int x0, int y0, int w, int h,
                                 const dr3_map_car_t *car)
{
    const int player_size = 5;
    const int other_size  = 3;
    int       mx, my, cx, cy, size;

    dr3_minimap_project(car->x, car->y, &mx, &my);

    /* map pixel -> canvas pixel, then centre the marker */
    cx   = x0 + (int)(((long)mx * (long)w) / (long)((dr3_map_w > 0) ? dr3_map_w : 1));
    cy   = y0 + (int)(((long)my * (long)h) / (long)((dr3_map_h > 0) ? dr3_map_h : 1));
    size = car->is_player ? player_size : other_size;

    if (car->is_player) {
        dr3_canvas_fill(c, cx - (size / 2) - 1, cy - (size / 2) - 1, size + 2, size + 2, 0x000000u);
        dr3_canvas_fill(c, cx - (size / 2), cy - (size / 2), size, size, DR3_MAP_COL_PLAYER);
    } else {
        dr3_canvas_fill(c, cx - (size / 2), cy - (size / 2), size, size, DR3_MAP_COL_CAR);
    }
}

static void dr3_minimap_draw_cars(const dr3_canvas_t *c, int x0, int y0, int w, int h,
                                  const dr3_map_car_t *cars, int n_cars)
{
    int i;

    for (i = 0; i < n_cars; ++i) {
        if (cars[i].valid && !cars[i].is_player) dr3_minimap_draw_car(c, x0, y0, w, h, &cars[i]);
    }

    for (i = 0; i < n_cars; ++i) {
        if (cars[i].valid && cars[i].is_player) dr3_minimap_draw_car(c, x0, y0, w, h, &cars[i]);
    }
}

int dr3_minimap_draw(const dr3_canvas_t *c, int x0, int y0, int w, int h,
                     const dr3_map_car_t *cars, int n_cars)
{
    const uint32_t background = DR3_MAP_COL_BACKGROUND;
    int x, y, dx = x0, dy = y0, dw = w, dh = h;

    if (!c || !c->px || (w <= 0) || (h <= 0)) return 0;

    dr3_canvas_fill(c, x0, y0, w, h, background);

    if (dr3_map_ready && (dr3_map_w > 0) && (dr3_map_h > 0)) {
        /* fit the map into the given rectangle without distorting it (the tracks are not square,
           e.g. 1016x716 or 960x600) and centre it there */
        dw = w;
        dh = (int)(((long)dr3_map_h * (long)w) / (long)dr3_map_w);

        if (dh > h) {
            dh = h;
            dw = (int)(((long)dr3_map_w * (long)h) / (long)dr3_map_h);
        }
        if (dw > w) dw = w;
        if (dh > h) dh = h;

        dx = x0 + (w - dw) / 2;
        dy = y0 + (h - dh) / 2;

        for (y = 0; y < dh; ++y) {
            const int       my   = (y * dr3_map_h) / dh;
            const uint8_t * row  = dr3_map_bits + (size_t)my * DR3_MINIMAP_MAX_W;
            const uint32_t *crow = dr3_map_col  + (size_t)my * DR3_MINIMAP_MAX_W;

            for (x = 0; x < dw; ++x) {
                const int idx = (x * dr3_map_w) / dw;

                if ((row[idx] & 3) == DR3_MAP_NONE) continue;      /* outside the track */
                dr3_canvas_px(c, dx + x, dy + y, crow[idx]);       /* the track's own colour */
            }
        }

        dr3_canvas_rect(c, dx, dy, dw, dh, DR3_MAP_COL_FRAME);

        if (cars && (n_cars > 0)) dr3_minimap_draw_cars(c, dx, dy, dw, dh, cars, n_cars);
    }
    else {
        /* no track loaded: just the empty frame, so the page still looks intentional */
        dr3_canvas_rect(c, x0, y0, w, h, DR3_MAP_COL_FRAME);
    }

    return 1;
}

int dr3_minimap_ascii(char *out, int out_size, int cols, int rows)
{
    static const char symbol[4] = { ' ', ':', '#', '.' };   /* NONE, OFFROAD, ROAD, OTHER */
    int               x, y, len = 0;

    if (!out || (out_size <= 0)) return 0;
    if (cols <= 0) cols = 40;
    if (rows <= 0) rows = 20;

    for (y = 0; y < rows; ++y) {
        const int my = dr3_map_ready ? (y * dr3_map_h) / rows : 0;

        for (x = 0; (x < cols) && (len < (out_size - 2)); ++x) {
            if (!dr3_map_ready) {
                out[len++] = ' ';
                continue;
            }

            out[len++] = symbol[dr3_map_bits[(size_t)my * DR3_MINIMAP_MAX_W +
                                            ((x * dr3_map_w) / cols)] & 3];
        }

        if (len < (out_size - 1)) out[len++] = '\n';
    }

    out[len] = 0;

    return len;
}
