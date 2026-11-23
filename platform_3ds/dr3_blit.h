/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_blit.h - fast 8-bit indexed -> 32-bit RGBx conversion for the Nintendo 3DS build.
 *
 * SDL2's 3DS video driver is a frame-buffer driver (see SDL_n3dsframebuffer.c) and only the
 * *software* renderer is available, so the hot path of dRally (presenting a 320x200 / 320x240
 * 8-bit paletted screen) must convert indices to 32-bit pixels by itself.
 *
 * Palette -> 32-bit look-up table plus an integer-only nearest-neighbour / centred scaler.
 * Everything here is platform independent and is unit-tested on the host (tests/test_dr3.c).
 */
#ifndef DR3_BLIT_H
#define DR3_BLIT_H

#include <stdint.h>

#define DR3_LUT_BGR 0   /* memory order B,G,R,A (== SDL_PIXELFORMAT_ARGB8888 masks) */
#define DR3_LUT_RGB 1   /* memory order R,G,B,A (== SDL_PIXELFORMAT_ABGR8888 masks) */

#define DR3_SCALE_STRETCH 0  /* fill the whole target (nearest neighbour) */
#define DR3_SCALE_CENTER  1  /* 1:1, centred, borders filled with the clear colour */
#define DR3_SCALE_FILTER  2  /* fill the whole target, box-filtered (use when downscaling) */

#define DR3_SCREEN_W 400  /* 3DS top screen */
#define DR3_SCREEN_H 240

typedef struct {
    uint8_t r[256];
    uint8_t g[256];
    uint8_t b[256];
} dr3_palette_t;

typedef struct {
    uint32_t px[256];
    int      rs, gs, bs, as;   /* channel shifts of the target format (for filtered scaling) */
} dr3_lut32_t;

void dr3_palette_reset(dr3_palette_t *pal);
void dr3_palette_set(dr3_palette_t *pal, int index, uint8_t r, uint8_t g, uint8_t b);

/*
 * The colour layout of the destination can be given two ways:
 *   dr3_lut32_build()        convenience for the two common byte orders
 *   dr3_lut32_build_masks()  exact SDL masks - use this with a real target surface
 *                            (e.g. SDL_PIXELFORMAT_RGBA8888 on the 3DS) so no assumption is made
 */
void dr3_lut32_build(dr3_lut32_t *lut, const dr3_palette_t *pal, int order, uint8_t alpha);
void dr3_lut32_build_masks(dr3_lut32_t *lut, const dr3_palette_t *pal,
                           uint32_t r_mask, uint32_t g_mask, uint32_t b_mask, uint32_t a_mask);

/*
 * src        : 8-bit indexed pixels
 * dst        : 32-bit pixels, dst_pitch_px = pixels per row (not bytes)
 * clear      : packed 32-bit value used for the border in DR3_SCALE_CENTER mode
 *
 * Returns 0 on success, -1 if the mode cannot be satisfied (e.g. source larger than target).
 */
int dr3_blit8_lut32(const uint8_t *src, int sw, int sh, int src_pitch,
                    const dr3_lut32_t *lut,
                    uint32_t *dst, int dw, int dh, int dst_pitch_px,
                    int mode, uint32_t clear);

/* Box-filtered variant of the scaled blit: averages the source pixels that fall into each target
   pixel.  Used when the source is larger than the target (e.g. 640x480 VESA -> 400x240), because
   plain nearest-neighbour turns the game's dithered shading into vertical stripes. */
int dr3_blit8_filter(const uint8_t *src, int sw, int sh, int src_pitch,
                     const dr3_palette_t *pal, const dr3_lut32_t *lut,
                     uint32_t *dst, int dw, int dh, int dst_pitch_px);

/* ------------------------------------------------------------------ car colours --- */

/*
 * The palette has room for four car colour ramps of ten entries (0x0f..0x36, see ___4a064h.c) and
 * that is what the four car race uses.  The chaos build races up to twenty cars, which cannot each
 * have ten entries of their own - the track and its textures already use most of the 256 (measured,
 * logs/palette_probe.c).  So every car's sprite keeps the palette's own ramp and the *port*
 * re-colours those pixels with the colour of that car's driver while it presents the frame: all ten
 * shades stay, only the colour changes.  The engine reports where each car was drawn (dr3_chaos.h);
 * the game's screen is 8-bit and the colour pixels of a car are exactly FIRST..LAST.
 */
#define DR3_CARCOLOUR_FIRST  0x0f    /* the car sprite's colour ramp in the palette   */
#define DR3_CARCOLOUR_LAST   0x18

/* one car's ten shades in the target format - the ramp ___4a064h_cdecl() builds (five darkened and
   five lightened shades of the colour), packed the way the LUT's other pixels are */
/*
 * Builds a car's ten shade ramp (five darkened shades, then five lightened ones - the arithmetic of
 * ___4a064h_cdecl).  `r`, `g`, `b` are 0..255 screen values here; dr3_carcolour_build_vga6() below is
 * the one the race uses, because the engine's colours arrive in the palette's own 6-bit space.
 */
/*
 * The race's own blit, 320 -> 400 columns (the engine's 320x240 screen on the 3DS top screen), into the rotated
 * hardware framebuffer: target (x, y) is fb[x * col_stride + (dh - 1 - y)], its source column is x * 4 / 5
 * (the nearest neighbour mapping of dr3_blit8_lut32) and the rows are 1:1.
 *
 * Every five target columns are four source columns (x = 5k + 0..4 take 4k + 0, 0, 1, 2, 3), so a group is read
 * once - four source pixels, four table lookups - and written five times.  The general loop does a lookup and
 * a row mapping per *target* pixel.  The result is identical to it (tests/test_dr3.c checks every pixel).
 *   sw must be a multiple of 4; the target is sw * 5 / 4 columns wide.
 */
void dr3_blit_rot_5_4(uint32_t *fb, int col_stride, int dh, const uint8_t *src, int sw, int src_pitch,
                      const uint32_t *lut);

/*
 * The box the marks of a car mask live in: box[0..3] = x0, y0, x1, y1 (inclusive).  Returns 1 when the mask
 * has a mark at all, 0 (box untouched) when it is empty.  Walks the mask a word at a time where it can - the
 * byte wise walk of the empty rows cost 1.4 ms of every 14 ms frame on the console.
 */
int dr3_carcolour_mask_bounds(const uint8_t *mask, int w, int h, int pitch, int box[4]);

void dr3_carcolour_build(uint32_t out[10], int r, int g, int b, const dr3_lut32_t *lut);

/*
 * The same ramp for a colour in the engine's 6-bit palette space (0..63): the light half saturates at
 * 63 = white, so the shine of a car stays bright instead of turning grey (see dr3_blit.c).
 */
void dr3_carcolour_build_vga6(uint32_t out[10], int r, int g, int b, const dr3_lut32_t *lut);

/*
 * Sets the race intro / exit fade state for dr3_carcolour_tint_cars() (race___45d3ch.c):
 *   brightness_256 : 0..256 overall brightness scale (256 = 100% normal race brightness)
 *   color_256      : 0..256 opponent colour saturation (0 = grayscale matching intro track, 256 = full driver colour)
 *   player_flash   : 0..256 player car highlight flash during intro (0 = normal, 256 = 1.7x bright highlight)
 */
void dr3_carcolour_set_fade(int brightness_256, int color_256, int player_flash);

/*
 * Re-colours the colour pixels of the cars of the frame: for every source pixel whose mask byte is set
 * (the engine marked every pixel its car sprite blit wrote - dr3_chaos.h) and whose value is in the
 * palette's car ramp, the matching shade of that car's colour is written.  The mapping from the source
 * screen to the target is the same nearest-neighbour one the blit uses (target x covers source column
 * x*sw/dw).  dst points at the target pixel of (0,0) and dst_stride_x / dst_stride_y describe the
 * target (the 3DS framebuffer is stored rotated: x stride = the row, y stride = -1).
 *
 *   mask    : one byte per source pixel, 0 = no car, otherwise the car index + 1
 *   mask    : one byte per pixel of the game's own screen (dr3_chaos_car_screen), 0 = no car,
 *             n = the pixel belongs to car n-1
 *   mask_row_off : the row inside src where the game's screen starts.  VGA13 sits in a 320x240
 *             surface, 20 rows below its top - the mask covers the game's rows only, so the row the
 *             mask is read at is "the row of src - mask_row_off".
 *   area    : where to look, and how to map a target pixel back to the source ("NULL" = everywhere,
 *             computing the mapping with a division each).  The race passes the maps the blit already
 *             precomputed and the box the marks of the frame live in - the whole frame only holds cars
 *             in a small part of it, and a division per pixel for 96.000 pixels dwarfed everything
 *             else the port does for the cars.
 *   shades  : n_cars * 10 packed colours, one ramp per car (dr3_carcolour_build)
 *
 * Returns how many pixels it wrote.
 */
typedef struct {
    /*
     * The tables of the port (dr3_fb.c: static uint16_t dr3_sx[] / dr3_sy[]).  The type has to be
     * exactly theirs: declared as int* the compiler only *warned*, the tint read each entry as a pair
     * of uint16 and the cars came out in one colour - the bug this type now makes impossible.
     */
    const uint16_t *map_x;       /* target column -> source column (per target column)   */
    const uint16_t *map_y;       /* target row    -> source row    (per target row)      */
    int        x0, y0, x1, y1;   /* the cells of the mask worth looking at, in the mask's
                                    own coordinates; x1 < x0 means "nothing to do"        */
} dr3_carcolour_area_t;

int dr3_carcolour_tint_mask(uint32_t *dst, int dw, int dh, int dst_stride_x, int dst_stride_y,
                            const uint8_t *src, int sw, int sh, int src_pitch,
                            const uint8_t *mask, int mask_w, int mask_h, int mask_pitch,
                            int mask_row_off, const dr3_carcolour_area_t *area,
                            const uint32_t *shades, int n_cars);

/*
 * The whole pass for one presented frame: takes the car mask the engine filled for this race screen
 * (dr3_chaos.h: dr3_chaos_car_screen / dr3_chaos_car_col), works out where its marks are, builds every
 * car's ten shades (dr3_carcolour_build_vga6) and colours those pixels in the target with
 * dr3_carcolour_tint_mask.  Nothing else is needed to give twenty cars twenty colours, which is why the
 * 3DS top screen and the PC window share it:
 *
 *   * dst / dst_stride_x / dst_stride_y - the target.  The 3DS framebuffer is stored rotated (stride x =
 *     its row width, stride y = -1 from the last row), a linear window surface is (1, its pitch in
 *     pixels).
 *   * row_off - the row inside src where the game's own screen starts: VGA13 sits 20 rows below the top
 *     of the 320x240 surface and the mask covers the game's rows only (so the mask row is "src row -
 *     row_off").
 *   * The mapping is the nearest-neighbour one of dr3_blit8_lut32() (target x covers source column
 *     x*sw/dw), built here once and kept until the sizes change - the 640x480 front end is filtered and
 *     has no cars, so this only ever runs for a race.
 *
 * Returns the number of pixels that were coloured (0 if the mask was empty).  Logs what it saw and did
 * for the first presents of a race while the port's log is on.
 */
int dr3_carcolour_tint_cars(uint32_t *dst, int dw, int dh, int dst_stride_x, int dst_stride_y,
                            const uint8_t *src, int sw, int sh, int src_pitch, int row_off,
                            const dr3_lut32_t *lut);

/*
 * The health line the tint pass draws over a car that has taken damage (dr3_chaos.h has the line's
 * geometry).  It is only drawn where it says something: while the car's HP is *below* this share of the HP
 * it started the race with.  A car that is still whole would show the full length bar of a car nobody has
 * touched, and a wreck (0 HP) would show a single pixel of the darkest shade - both get no line at all.
 *
 * The number sits next to the function that answers the question, not in dr3_chaos.h with the line's
 * geometry, because that function is portable and the host tests build it: the test project does not
 * define DR3_CHAOS, and dr3_chaos.h hides its tuning behind that switch.
 */
#define DR3_CHAOS_HP_LINE_BELOW 90

/*
 * May the health line of a car be drawn?  hp_max is dr3_chaos_hp_max, the HP the car had when the race
 * started, hp_now what it is down to, and own_car says whether the car is the *player's own* one.  His car
 * never gets a line, whatever its HP is: the bar is there to read the *other* cars (which of them is nearly
 * done), while his own damage is what the game's own HUD shows him (the DAMAGE panel, s_54[n].__10 from
 * race___400ech.c).  Every number is the engine's own,
 * so the answer is plain integer arithmetic - the 3DS and the PC decide the same way, and tests/test_dr3.c
 * checks the boundaries (DR3_CHAOS_HP_LINE_BELOW percent exactly, a wreck, a car with no scale at all, the
 * own car of the player).
 */
int dr3_carcolour_hp_line_shown(int hp_now, int hp_max, int own_car);

#endif /* DR3_BLIT_H */
