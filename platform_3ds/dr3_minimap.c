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

/* ------------------------------------------------------------- band + cell helpers --- */

/* Fits the map into the given rectangle without distorting it (the tracks are not square, e.g.
   1016x716 or 960x600) and centres it there. */
static void dr3_map_band(int x0, int y0, int w, int h, int *dx, int *dy, int *dw, int *dh)
{
    int bw = w, bh = h;

    if (dr3_map_ready && (dr3_map_w > 0) && (dr3_map_h > 0)) {
        bh = (int)(((long)dr3_map_h * (long)w) / (long)dr3_map_w);

        if (bh > h) {
            bh = h;
            bw = (int)(((long)dr3_map_w * (long)h) / (long)dr3_map_h);
        }
        if (bw > w) bw = w;
        if (bh > h) bh = h;
    }

    *dx = x0 + (w - bw) / 2;
    *dy = y0 + (h - bh) / 2;
    *dw = bw;
    *dh = bh;
}

/* Colour of a map cell (background when the cell lies outside the track). */
static uint32_t dr3_map_cell_rgb(int mx, int my)
{
    size_t idx;

    if (!dr3_map_ready || (mx < 0) || (my < 0) || (mx >= dr3_map_w) || (my >= dr3_map_h)) {
        return DR3_MAP_COL_BACKGROUND;
    }

    idx = (size_t)my * DR3_MINIMAP_MAX_W + (size_t)mx;
    if ((dr3_map_bits[idx] & 3) == DR3_MAP_NONE) return DR3_MAP_COL_BACKGROUND;

    return dr3_map_col[idx];
}

/* Restores one canvas pixel to what the map has there (used to erase a marker). */
static void dr3_map_repaint_px(const dr3_canvas_t *c, int dx, int dy, int dw, int dh, int cx, int cy)
{
    int mx, my;

    if ((dw <= 0) || (dh <= 0) || (cx < dx) || (cy < dy) || (cx >= (dx + dw)) || (cy >= (dy + dh))) {
        dr3_canvas_px(c, cx, cy, DR3_MAP_COL_BACKGROUND);
        return;
    }

    mx = ((cx - dx) * dr3_map_w) / dw;
    my = ((cy - dy) * dr3_map_h) / dh;

    dr3_canvas_px(c, cx, cy, dr3_map_cell_rgb(mx, my));
}

/* ---------------------------------------------------------------------- markers --- */

/* the markers remembered from the last draw, so an update can erase exactly those pixels */
#define DR3_MARKER_MAX 4

static int dr3_marker_n;
static int dr3_marker_box[DR3_MARKER_MAX][4];      /* x, y, w, h in canvas pixels */

/* A blob with the corners left out - a 5x5 square reads as a square, this reads as a car dot. */
static void dr3_canvas_blob(const dr3_canvas_t *c, int x, int y, int size, uint32_t rgb)
{
    int i, j;

    for (j = 0; j < size; ++j) {
        for (i = 0; i < size; ++i) {
            const int corner = ((i == 0) || (i == (size - 1))) && ((j == 0) || (j == (size - 1)));

            if (corner) continue;
            dr3_canvas_px(c, x + i, y + j, rgb);
        }
    }
}

/*
 * The player gets a rounded marker in his own car colour (the engine's per-driver colours live in
 * menu_main.c's ___1a0fb8h table) with a dark outline; the others stay red and smaller.
 */
static void dr3_minimap_draw_car(const dr3_canvas_t *c, int dx, int dy, int dw, int dh,
                                 const dr3_map_car_t *car, int slot)
{
    const int player_size = 5;
    const int other_size  = 3;
    const int outline     = car->is_player ? 1 : 0;
    uint32_t  rgb         = car->color ? car->color
                                       : (car->is_player ? DR3_MAP_COL_PLAYER : DR3_MAP_COL_CAR);
    int       mx, my, cx, cy, size;

    dr3_minimap_project(car->x, car->y, &mx, &my);

    /* map pixel -> canvas pixel, then centre the marker */
    cx   = dx + (int)(((long)mx * (long)dw) / (long)((dr3_map_w > 0) ? dr3_map_w : 1));
    cy   = dy + (int)(((long)my * (long)dh) / (long)((dr3_map_h > 0) ? dr3_map_h : 1));
    size = car->is_player ? player_size : other_size;

    if (outline) {
        dr3_canvas_blob(c, cx - (size / 2) - 1, cy - (size / 2) - 1, size + 2, DR3_MAP_COL_OUTLINE);
    }
    dr3_canvas_blob(c, cx - (size / 2), cy - (size / 2), size, rgb);

    if ((slot >= 0) && (slot < DR3_MARKER_MAX)) {
        dr3_marker_box[slot][0] = cx - (size / 2) - outline;
        dr3_marker_box[slot][1] = cy - (size / 2) - outline;
        dr3_marker_box[slot][2] = size + (2 * outline);
        dr3_marker_box[slot][3] = size + (2 * outline);
    }
}

static void dr3_minimap_draw_cars(const dr3_canvas_t *c, int dx, int dy, int dw, int dh,
                                  const dr3_map_car_t *cars, int n_cars)
{
    int i, slot = 0;

    dr3_marker_n = 0;

    /* the others first, so the player marker can never be covered by one of them */
    for (i = 0; i < n_cars; ++i) {
        if (cars[i].valid && !cars[i].is_player) {
            dr3_minimap_draw_car(c, dx, dy, dw, dh, &cars[i], slot++);
        }
    }

    for (i = 0; i < n_cars; ++i) {
        if (cars[i].valid && cars[i].is_player) {
            dr3_minimap_draw_car(c, dx, dy, dw, dh, &cars[i], slot++);
        }
    }

    dr3_marker_n = slot;
}

/* ---------------------------------------------------------------- full and partial --- */

int dr3_minimap_draw(const dr3_canvas_t *c, int x0, int y0, int w, int h,
                     const dr3_map_car_t *cars, int n_cars)
{
    int x, y, dx, dy, dw, dh;

    if (!c || !c->px || (w <= 0) || (h <= 0)) return 0;

    dr3_canvas_fill(c, x0, y0, w, h, DR3_MAP_COL_BACKGROUND);

    dr3_map_band(x0, y0, w, h, &dx, &dy, &dw, &dh);

    if (dr3_map_ready && (dr3_map_w > 0) && (dr3_map_h > 0)) {
        for (y = 0; y < dh; ++y) {
            const int       my  = (y * dr3_map_h) / dh;
            const uint8_t * row = dr3_map_bits + (size_t)my * DR3_MINIMAP_MAX_W;

            for (x = 0; x < dw; ++x) {
                const int idx = (x * dr3_map_w) / dw;

                if ((row[idx] & 3) == DR3_MAP_NONE) continue;     /* outside the track */
                dr3_canvas_px(c, dx + x, dy + y, dr3_map_cell_rgb(idx, my));
            }
        }
    }

    dr3_marker_n = 0;
    if (cars && (n_cars > 0) && dr3_map_ready) dr3_minimap_draw_cars(c, dx, dy, dw, dh, cars, n_cars);

    return 1;
}

int dr3_minimap_draw_incremental(const dr3_canvas_t *c, int x0, int y0, int w, int h,
                                 const dr3_map_car_t *cars, int n_cars)
{
    static int have_state;
    int        i, dx, dy, dw, dh;

    if (!c || !c->px || (w <= 0) || (h <= 0)) return 0;

    if (!have_state || !dr3_map_ready) {
        have_state = 1;
        return dr3_minimap_draw(c, x0, y0, w, h, cars, n_cars);
    }

    dr3_map_band(x0, y0, w, h, &dx, &dy, &dw, &dh);

    /* put the map back where the markers were, then draw them at their new place */
    for (i = 0; i < dr3_marker_n; ++i) {
        int px, py;

        for (py = dr3_marker_box[i][1]; py < (dr3_marker_box[i][1] + dr3_marker_box[i][3]); ++py) {
            for (px = dr3_marker_box[i][0]; px < (dr3_marker_box[i][0] + dr3_marker_box[i][2]); ++px) {
                dr3_map_repaint_px(c, dx, dy, dw, dh, px, py);
            }
        }
    }

    dr3_marker_n = 0;
    if (cars && (n_cars > 0) && dr3_map_ready) dr3_minimap_draw_cars(c, dx, dy, dw, dh, cars, n_cars);

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
