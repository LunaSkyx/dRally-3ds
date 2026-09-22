/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_minimap.h - track minimap for the bottom screen of the Nintendo 3DS.
 *
 * The engine keeps a surface mask of the loaded track (TRX_MAS, one byte per track pixel, low
 * nibble = surface) plus the position of every car (struct_35e_t.XLocation/YLocation in the same
 * pixel space).  That is enough to draw a readable minimap on the second screen - the screen the
 * original game never used.
 *
 * The mask nibble has three useful groups (see race___4ff50h.c: "< 4" is the soft ground with dust,
 * race___54668h.c: "== 0xf" is the hard surface skid marks are drawn on):
 *
 *     0xf        hard surface   -> DR3_MAP_ROAD
 *     0x0 .. 0x3 soft ground    -> DR3_MAP_OFFROAD
 *     0x4 .. 0xe everything else-> DR3_MAP_OTHER
 *
 * Everything in this file is plain C without any platform header, so the scaling, the
 * classification and the drawing are unit-tested on the host (tests/test_dr3.c, exactly like
 * dr3_blit.c).  Only the glue that hands over the framebuffer lives on the 3DS.
 */
#ifndef DR3_MINIMAP_H
#define DR3_MINIMAP_H

#include <stdint.h>

#define DR3_MINIMAP_MAX_W 320      /* the bottom screen is 320x240 ... */
#define DR3_MINIMAP_MAX_H 224      /* ... minus a text line top and bottom */

/* classes stored in the small map bitmap */
#define DR3_MAP_NONE     0
#define DR3_MAP_OFFROAD  1
#define DR3_MAP_ROAD     2
#define DR3_MAP_OTHER    3

/*
 * A drawing target.  The 3DS framebuffer is stored rotated, so x and y need different strides -
 * and y runs backwards in it.  Keeping that in the canvas (instead of in the drawing code) means
 * the drawing itself is portable and testable.
 */
typedef struct {
    uint32_t *px;        /* pixel that belongs to (0, 0) of the canvas */
    int       stride_x;  /* pixels to the next x (240 on the bottom screen) */
    int       stride_y;  /* pixels to the next y (-1 on the bottom screen) */
    int       w, h;      /* size of the canvas in pixels */
} dr3_canvas_t;

typedef struct {
    float x, y;          /* car position in track pixels (TRX_WIDTH/TRX_HEIGHT space) */
    int   is_player;
    int   valid;
} dr3_map_car_t;

/* ------------------------------------------------------------------ data stage --- */

/*
 * Builds the small map from the engine's track mask.  "mask" may be NULL (nothing loaded).
 * step is chosen so that the result fits DR3_MINIMAP_MAX_W x DR3_MINIMAP_MAX_H.  Returns 1 when a
 * map was built, 0 otherwise.
 */
int  dr3_minimap_build(const uint8_t *mask, int mask_w, int mask_h);

/* Drops the map (call it when the track is freed, so no stale pointer is used). */
void dr3_minimap_reset(void);

int            dr3_minimap_ready(void);
int            dr3_minimap_w(void);
int            dr3_minimap_h(void);
int            dr3_minimap_step(void);
const uint8_t *dr3_minimap_bits(void);

/* Number of map pixels per class (counts[DR3_MAP_*]) - used for the log line. */
void dr3_minimap_class_counts(int counts[4]);

/* Track pixel -> map pixel.  Clamped to the map, so a car that leaves the mask stays visible. */
void dr3_minimap_project(float x, float y, int *mx, int *my);

/* ----------------------------------------------------------------- draw stage --- */

void dr3_canvas_fill(const dr3_canvas_t *c, int x0, int y0, int w, int h, uint32_t color);
void dr3_canvas_rect(const dr3_canvas_t *c, int x0, int y0, int w, int h, uint32_t color);
void dr3_canvas_px(const dr3_canvas_t *c, int x, int y, uint32_t color);

/*
 * Draws the map into the rectangle (x0, y0, w, h) of the canvas and puts the cars on top.
 * Returns 1 when something was drawn.
 */
int  dr3_minimap_draw(const dr3_canvas_t *c, int x0, int y0, int w, int h,
                      const dr3_map_car_t *cars, int n_cars);

/* ASCII preview of the map (for drally_3ds.log) - '#' road, ':' soft ground, '.' other, ' ' none.
   Returns the number of characters written. */
int  dr3_minimap_ascii(char *out, int out_size, int cols, int rows);

/* packed colors used by the minimap (RGB888, see dr3_minimap.c for the framebuffer order) */
#define DR3_MAP_COL_BACKGROUND 0x000A0A14u
#define DR3_MAP_COL_ROAD       0x00D2D2DCu
#define DR3_MAP_COL_OFFROAD    0x00264A28u
#define DR3_MAP_COL_OTHER      0x00404048u
#define DR3_MAP_COL_FRAME      0x006080A0u
#define DR3_MAP_COL_PLAYER     0x00FFE020u
#define DR3_MAP_COL_CAR        0x00E03030u

#endif /* DR3_MINIMAP_H */
