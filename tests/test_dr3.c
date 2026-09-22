/*
 * test_dr3.c - host unit tests for the portable part of the 3DS port.
 *
 * Runs on Windows (MSVC) so the error-prone logic (palette -> 32-bit LUT, scaling, pad ->
 * scancode mapping) is verified long before a devkitARM toolchain is available.
 *
 * Build/run:  powershell -ExecutionPolicy Bypass -File tests/build_tests.ps1
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../platform_3ds/dr3_blit.h"
#include "../platform_3ds/dr3_input_map.h"
#include "../platform_3ds/dr3_laptime.h"
#include "../platform_3ds/dr3_minimap.h"

static int g_failed;
static int g_checks;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        ++g_checks;                                       \
        if (!(cond)) {                                    \
            ++g_failed;                                   \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static void test_lut(void)
{
    dr3_palette_t pal;
    dr3_lut32_t   lut;
    int           n;

    printf("- LUT build\n");
    dr3_palette_reset(&pal);
    dr3_palette_set(&pal, 0, 0x11, 0x22, 0x33);

    /* BGR memory order: byte0=B, byte1=G, byte2=R  ->  little endian value 0x00RRGGBB */
    dr3_lut32_build(&lut, &pal, DR3_LUT_BGR, 0xFF);
    CHECK(lut.px[0] == 0xFF112233u, "BGR order: got 0x%08X want 0xFF112233 (bytes 33,22,11)",
          lut.px[0]);
    CHECK(lut.px[255] == 0xFFFFFFFFu, "grey ramp end: got 0x%08X", lut.px[255]);

    /* RGB memory order: byte0=R, byte1=G, byte2=B  ->  little endian value 0x00BBGGRR */
    dr3_lut32_build(&lut, &pal, DR3_LUT_RGB, 0x00);
    CHECK(lut.px[0] == 0x00332211u, "RGB order/alpha: got 0x%08X want 0x00332211 (bytes 11,22,33)",
          lut.px[0]);

    dr3_lut32_build(&lut, &pal, DR3_LUT_RGB, 0xFF);
    for (n = 1; n < 256; ++n)
        CHECK(lut.px[n] != lut.px[n - 1], "duplicate LUT entry at %d", n);
}

static void test_blit_center(void)
{
    uint8_t       src[4 * 2]; /* 4x2 image */
    uint32_t      dst[8 * 4]; /* 8x4 target */
    dr3_lut32_t   lut;
    dr3_palette_t pal;

    printf("- blit, centred 1:1\n");
    dr3_palette_reset(&pal);
    dr3_lut32_build(&lut, &pal, DR3_LUT_RGB, 0xFF);

    memset(src, 0, sizeof(src));
    src[0] = 0; src[1] = 1; src[2] = 2; src[3] = 3;
    src[4] = 4; src[5] = 5; src[6] = 6; src[7] = 7;

    memset(dst, 0xAB, sizeof(dst));
    CHECK(dr3_blit8_lut32(src, 4, 2, 4, &lut, dst, 8, 4, 8, DR3_SCALE_CENTER, 0xDEADBEEFu) == 0,
          "blit returned an error");

    /* 4x2 image into an 8x4 target -> top-left offset (2,1); borders keep the clear colour */
    CHECK(dst[0] == 0xDEADBEEFu, "border not cleared: 0x%08X", dst[0]);
    CHECK(dst[1 * 8 + 2] == lut.px[0], "pixel(0,0) wrong: 0x%08X", dst[1 * 8 + 2]);
    CHECK(dst[2 * 8 + 5] == lut.px[7], "pixel(3,1) wrong: 0x%08X", dst[2 * 8 + 5]);
    CHECK(dst[1 * 8 + 1] == 0xDEADBEEFu, "left border overwritten: 0x%08X", dst[1 * 8 + 1]);
    CHECK(dst[3 * 8 + 2] == 0xDEADBEEFu, "bottom border overwritten: 0x%08X", dst[3 * 8 + 2]);

    CHECK(dr3_blit8_lut32(src, 4, 2, 4, &lut, dst, 2, 2, 2, DR3_SCALE_CENTER, 0) == -1,
          "expected a refusal when the source is larger than the target");
}

static void test_blit_stretch(void)
{
    static uint8_t  src[320 * 200];
    static uint32_t dst[400 * 240];
    dr3_lut32_t     lut;
    dr3_palette_t   pal;
    int             x, y;

    printf("- blit, 320x200 -> 400x240 stretch\n");
    dr3_palette_reset(&pal);
    dr3_lut32_build(&lut, &pal, DR3_LUT_BGR, 0xFF);

    for (y = 0; y < 200; ++y)
        for (x = 0; x < 320; ++x)
            src[y * 320 + x] = (uint8_t)((x ^ y) & 0xFF);

    CHECK(dr3_blit8_lut32(src, 320, 200, 320, &lut, dst, 400, 240, 400, DR3_SCALE_STRETCH, 0) == 0,
          "stretch blit failed");

    CHECK(dst[0] == lut.px[src[0]], "top-left mismatch");
    CHECK(dst[400 - 1] == lut.px[src[319]], "top-right mismatch: 0x%08X vs 0x%08X",
          dst[400 - 1], lut.px[src[319]]);
    CHECK(dst[239 * 400] == lut.px[src[199 * 320]], "bottom-left mismatch");
    CHECK(dst[239 * 400 + 399] == lut.px[src[199 * 320 + 319]], "bottom-right mismatch");

    for (y = 0; y < 240; ++y)
        for (x = 0; x < 400; ++x)
            if (dst[y * 400 + x] == 0) { CHECK(0, "untouched pixel at %d,%d", x, y); y = 240; break; }
}

static void expect_scan(const dr3_pad_state_t *st, int want, const char *what)
{
    uint8_t set[SDL_NUM_SCANCODES];
    int     n = dr3_input_scancodes(st, set);
    CHECK(n == want, "%s: %d scancodes set, expected %d", what, n, want);
}

static void test_input_map(void)
{
    dr3_pad_state_t st;
    uint8_t         set[SDL_NUM_SCANCODES];

    printf("- pad -> scancode mapping\n");

    memset(&st, 0, sizeof(st));
    expect_scan(&st, 0, "idle");

    /* R = gas -> 'A' (dRally's default), L = brake -> 'Z': race functions */
    dr3_input_set_context(1);
    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_R;
    dr3_input_scancodes(&st, set);
    CHECK(set[SDL_SCANCODE_A] == 1, "R must press 'A' (gas) in a race");
    CHECK(set[SDL_SCANCODE_Z] == 0, "R must not press 'Z'");
    dr3_input_set_context(0);

    /* front end (default context): A confirms, B selects - exactly one key each */
    dr3_input_set_context(0);

    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_A;
    dr3_input_scancodes(&st, set);
    expect_scan(&st, 1, "A button (front end)");
    CHECK(set[SDL_SCANCODE_RETURN], "A must map to RETURN (confirm) in the front end");

    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_B;
    dr3_input_scancodes(&st, set);
    CHECK(set[SDL_SCANCODE_SPACE], "B must map to SPACE (select) in the front end");

    /* race: A horn, B boost, Y shoot, X mine (STILL one key each, so dialogues keep working) */
    dr3_input_set_context(1);

    memset(&st, 0, sizeof(st)); st.held = DR3_PAD_A; dr3_input_scancodes(&st, set);
    expect_scan(&st, 2, "A button (race)");
    CHECK(set[SDL_SCANCODE_SPACE], "A must map to SPACE (horn) in a race");
    CHECK(set[SDL_SCANCODE_RETURN], "A must also send RETURN so the race start dialogues confirm");

    memset(&st, 0, sizeof(st)); st.held = DR3_PAD_B; dr3_input_scancodes(&st, set);
    CHECK(set[SDL_SCANCODE_LSHIFT], "B must map to LSHIFT (boost) in a race");

    memset(&st, 0, sizeof(st)); st.held = DR3_PAD_Y; dr3_input_scancodes(&st, set);
    CHECK(set[SDL_SCANCODE_LCTRL], "Y must map to LCTRL (shoot) in a race");

    memset(&st, 0, sizeof(st)); st.held = DR3_PAD_X; dr3_input_scancodes(&st, set);
    CHECK(set[SDL_SCANCODE_LALT], "X must map to LALT (drop mine) in a race");

    dr3_input_set_context(0);      /* back to the front end for the rest of the checks */

    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_UP;
    expect_scan(&st, 1, "d-pad up (front end)");

    memset(&st, 0, sizeof(st));
    st.cpad_x = -1;
    expect_scan(&st, 2, "circle pad left");

    /* gas + steering come from the same button/stick while racing */
    dr3_input_set_context(1);
    memset(&st, 0, sizeof(st));
    st.held   = DR3_PAD_R;
    st.cpad_x = -1;
    expect_scan(&st, 3, "gas + steer left");
    dr3_input_set_context(0);

    /* X/Y are only mapped while racing */
    dr3_input_set_context(1);
    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_X;
    expect_scan(&st, 1, "X button (race)");
    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_Y;
    expect_scan(&st, 1, "Y button (race)");
    dr3_input_set_context(0);
    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_B;
    expect_scan(&st, 1, "B button");
    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_START;
    expect_scan(&st, 1, "START button");

    memset(&st, 0, sizeof(st));
    st.cpad_y = 1;
    dr3_input_set_context(1);              /* stick up = gas while racing */
    expect_scan(&st, 1, "circle pad up (gas)");
    dr3_input_set_context(0);

    /* quit combo */
    memset(&st, 0, sizeof(st));
    CHECK(!dr3_input_quit_combo(&st), "empty pad must not quit");
    st.held = DR3_PAD_L | DR3_PAD_R;
    CHECK(!dr3_input_quit_combo(&st), "L+R must not quit (START is required)");
    st.held = DR3_PAD_L | DR3_PAD_R | DR3_PAD_START;
    CHECK(dr3_input_quit_combo(&st), "L+R+START must quit");

    CHECK(strcmp(dr3_scancode_name(SDL_SCANCODE_A), "A (gas)") == 0,
          "scancode name lookup failed");
}

static void test_lut_masks(void)
{
    dr3_palette_t pal;
    dr3_lut32_t   lut, lut2;
    uint32_t      v;

    printf("- LUT from SDL masks\n");
    dr3_palette_reset(&pal);
    dr3_palette_set(&pal, 0, 0x11, 0x22, 0x33);

    /* the 3DS framebuffer is SDL_PIXELFORMAT_RGBA8888:
       R=0xFF000000 G=0x00FF0000 B=0x0000FF00 A=0x000000FF */
    dr3_lut32_build_masks(&lut, &pal, 0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0x000000FFu);
    CHECK(lut.px[0] == 0x112233FFu, "RGBA8888 masks: got 0x%08X want 0x112233FF", lut.px[0]);

    /* ... which in little endian memory is A,B,G,R = FF,33,22,11 */
    v = lut.px[0];
    CHECK(((v >> 0) & 0xFF) == 0xFF, "byte0 (A) = 0x%02X", (v >> 0) & 0xFF);
    CHECK(((v >> 8) & 0xFF) == 0x33, "byte1 (B) = 0x%02X", (v >> 8) & 0xFF);
    CHECK(((v >> 16) & 0xFF) == 0x22, "byte2 (G) = 0x%02X", (v >> 16) & 0xFF);
    CHECK(((v >> 24) & 0xFF) == 0x11, "byte3 (R) = 0x%02X", (v >> 24) & 0xFF);

    /* the convenience orders must match the equivalent SDL masks */
    dr3_lut32_build(&lut2, &pal, DR3_LUT_BGR, 0xFF);
    dr3_lut32_build_masks(&lut, &pal, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
    CHECK(lut2.px[0] == lut.px[0], "DR3_LUT_BGR != ARGB8888 masks (0x%08X vs 0x%08X)",
          lut2.px[0], lut.px[0]);

    dr3_lut32_build(&lut2, &pal, DR3_LUT_RGB, 0xFF);
    dr3_lut32_build_masks(&lut, &pal, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0xFF000000u);
    CHECK(lut2.px[0] == lut.px[0], "DR3_LUT_RGB != ABGR8888 masks (0x%08X vs 0x%08X)",
          lut2.px[0], lut.px[0]);

    /* empty masks must not invent an alpha value (palette entry 0 is black after a reset) */
    dr3_palette_reset(&pal);
    dr3_lut32_build_masks(&lut, &pal, 0, 0, 0, 0);
    CHECK(lut.px[0] == 0x00000000u, "empty masks: 0x%08X", lut.px[0]);
}

static void test_text_and_filter(void)
{
    dr3_palette_t pal;
    dr3_lut32_t   lut;
    uint8_t       src[4 * 2];
    uint32_t      dst[2 * 1];
    int           i;

    printf("- software keyboard text + filtered downscale\n");

    /* characters typed on the 3DS keyboard must become the scancodes the engine maps back to chars */
    CHECK(dr3_char_to_scancode('A') == SDL_SCANCODE_A, "'A' -> SCANCODE_A");
    CHECK(dr3_char_to_scancode('z') == SDL_SCANCODE_Z, "'z' -> SCANCODE_Z (case folded)");
    CHECK(dr3_char_to_scancode('5') == SDL_SCANCODE_5, "'5' -> SCANCODE_5");
    CHECK(dr3_char_to_scancode('0') == SDL_SCANCODE_0, "'0' -> SCANCODE_0");
    CHECK(dr3_char_to_scancode(' ') == SDL_SCANCODE_SPACE, "' ' -> SCANCODE_SPACE");
    CHECK(dr3_char_to_scancode('-') == SDL_SCANCODE_MINUS, "'-' -> SCANCODE_MINUS");
    CHECK(dr3_char_to_scancode('?') == -1, "'?' has no scancode and must be rejected");

    /* box filter: a 4x2 image of two colours downscaled to 2x1 must average, not pick a pixel */
    dr3_palette_reset(&pal);
    dr3_palette_set(&pal, 1, 0, 0, 0);      /* black */
    dr3_palette_set(&pal, 2, 200, 100, 50); /* a colour */
    dr3_lut32_build_masks(&lut, &pal, 0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0x000000FFu);

    for (i = 0; i < 4; ++i) { src[i] = 1; src[4 + i] = 2; }   /* rows: black / colour */

    CHECK(dr3_blit8_filter(src, 4, 2, 4, &pal, &lut, dst, 2, 1, 2) == 0, "filter blit failed");
    CHECK(((dst[0] >> 24) & 0xFF) == 100, "filtered red: %u (expected the average 100)",
          (dst[0] >> 24) & 0xFF);
    CHECK(((dst[0] >> 16) & 0xFF) == 50, "filtered green: %u", (dst[0] >> 16) & 0xFF);
    CHECK(((dst[0] >> 8) & 0xFF) == 25, "filtered blue: %u", (dst[0] >> 8) & 0xFF);

    CHECK(dr3_blit8_filter(src, 4, 2, 4, &pal, &lut, dst, 1, 1, 1) == 0, "1x1 filter failed");
    CHECK(((dst[0] >> 24) & 0xFF) == 100, "1x1 filtered red: %u", (dst[0] >> 24) & 0xFF);
}

/* ----------------------------------------------------------------------------------- minimap --- */

/* Packs 0xRRGGBB the way the minimap writes it into the framebuffer: RGBA8 in memory (R in the
   lowest byte), which is what SDL's n3ds video driver initialises both screens with.  Deliberately
   an independent copy - if the port changes the order, this test has to fail. */
static uint32_t minimap_color(uint32_t rgb)
{
    return ((rgb >> 16) & 0xFFu) | ((rgb >> 8) & 0xFF00u) | ((rgb & 0xFFu) << 16) | 0xFF000000u;
}

/* the same for a 16-bit target (RGB565), which is what the 3DS bottom screen uses */
static uint16_t minimap_color565(uint32_t rgb)
{
    return (uint16_t)((((((rgb >> 16) & 0xFFu) >> 3)) << 11) |
                      (((((rgb >> 8) & 0xFFu) >> 2)) << 5) |
                        (((rgb & 0xFFu) >> 3)));
}

static void test_minimap_build(void)
{
    uint8_t mask[8 * 8];
    int     counts[4];
    int     x, y;

    printf("- minimap: mask -> map\n");

    /* row 4 is the road, the top left pixel is "other", everything else is soft ground */
    for (y = 0; y < 8; ++y)
        for (x = 0; x < 8; ++x) mask[y * 8 + x] = (y == 4) ? 0x0F : 0x00;
    mask[0] = 0x07;

    CHECK(dr3_minimap_build(mask, NULL, NULL, 8, 8) == 1, "build refused an 8x8 track");
    CHECK(dr3_minimap_ready() == 1, "map not marked ready");
    CHECK((dr3_minimap_w() == 8) && (dr3_minimap_h() == 8), "map size %dx%d (want 8x8)",
          dr3_minimap_w(), dr3_minimap_h());
    CHECK(dr3_minimap_step() == 1, "step %d for a small track (want 1)", dr3_minimap_step());

    dr3_minimap_class_counts(counts);
    CHECK(counts[DR3_MAP_ROAD] == 8, "road pixels %d (want 8)", counts[DR3_MAP_ROAD]);
    CHECK(counts[DR3_MAP_OFFROAD] == 55, "soft pixels %d (want 55)", counts[DR3_MAP_OFFROAD]);
    CHECK(counts[DR3_MAP_OTHER] == 1, "other pixels %d (want 1)", counts[DR3_MAP_OTHER]);
    CHECK(counts[DR3_MAP_NONE] == 0, "empty pixels %d (want 0)", counts[DR3_MAP_NONE]);

    CHECK(dr3_minimap_bits()[4 * DR3_MINIMAP_MAX_W + 5] == DR3_MAP_ROAD, "road pixel not classified");
    CHECK(dr3_minimap_bits()[2 * DR3_MINIMAP_MAX_W + 5] == DR3_MAP_OFFROAD, "ground pixel not classified");
    CHECK(dr3_minimap_bits()[0] == DR3_MAP_OTHER, "other pixel not classified");

    {
        int mx = -1, my = -1;

        dr3_minimap_project(4.0f, 4.0f, &mx, &my);
        CHECK((mx == 4) && (my == 4), "project(4,4) -> (%d,%d)", mx, my);

        dr3_minimap_project(-100.0f, -100.0f, &mx, &my);
        CHECK((mx == 0) && (my == 0), "project must clamp at 0 -> (%d,%d)", mx, my);

        dr3_minimap_project(1.0e9f, 1.0e9f, &mx, &my);
        CHECK((mx == 7) && (my == 7), "project must clamp at the map border -> (%d,%d)", mx, my);
    }

    /* nothing loaded: no map, and an old map must be gone */
    dr3_minimap_reset();
    CHECK(dr3_minimap_ready() == 0, "reset did not drop the map");
    CHECK(dr3_minimap_build(NULL, NULL, NULL, 8, 8) == 0, "build accepted a NULL mask");
    CHECK(dr3_minimap_build(mask, NULL, NULL, 0, 8) == 0, "build accepted a 0 width");
    CHECK(dr3_minimap_build(mask, NULL, NULL, 8, 0) == 0, "build accepted a 0 height");
}

static void test_minimap_downscale(void)
{
    static uint8_t mask[1024 * 1024];
    int            x, y, my;

    printf("- minimap: downscaling a large track\n");

    memset(mask, 0x00, sizeof(mask));
    for (y = 0; y < 1024; ++y) {
        mask[y * 1024 + 100] = 0x0F;      /* a 2 px wide road running the full height */
        mask[y * 1024 + 101] = 0x0F;
    }

    CHECK(dr3_minimap_build(mask, NULL, NULL, 1024, 1024) == 1, "build refused a 1024x1024 track");

    /* step = max(ceil(1024/320), ceil(1024/224)) = max(4, 5) = 5  ->  205x205 map */
    CHECK(dr3_minimap_step() == 5, "step %d for a 1024 track (want 5)", dr3_minimap_step());
    CHECK((dr3_minimap_w() == 205) && (dr3_minimap_h() == 205), "map %dx%d (want 205x205)",
          dr3_minimap_w(), dr3_minimap_h());
    CHECK(dr3_minimap_w() <= DR3_MINIMAP_MAX_W, "map wider than the buffer (%d)", dr3_minimap_w());
    CHECK(dr3_minimap_h() <= DR3_MINIMAP_MAX_H, "map higher than the buffer (%d)", dr3_minimap_h());

    /* even a 2 px road has to survive the 5x downscale: map column 100/5 = 20 */
    for (my = 20; my < 40; ++my)
        CHECK(dr3_minimap_bits()[my * DR3_MINIMAP_MAX_W + 20] == DR3_MAP_ROAD,
              "the thin road was lost at map row %d", my);

    (void)x;
}

static void test_minimap_canvas(void)
{
    uint32_t     buf[4 * 4];
    dr3_canvas_t c;

    printf("- minimap: canvas primitives\n");

    memset(buf, 0x11, sizeof(buf));
    c.px       = buf;
    c.fmt      = DR3_CANVAS_RGBA8888;
    c.stride_x = 1;                 /* a plain linear layout */
    c.stride_y = 4;
    c.w        = 4;
    c.h        = 4;

    dr3_canvas_px(&c, 1, 2, 0x112233u);            /* colors are plain 0xRRGGBB */
    CHECK(buf[2 * 4 + 1] == minimap_color(0x112233u), "pixel not written: 0x%08X", buf[2 * 4 + 1]);

    dr3_canvas_px(&c, -1, 0, 0x445566u);
    dr3_canvas_px(&c, 0, -1, 0x445566u);
    dr3_canvas_px(&c, 4, 0, 0x445566u);
    dr3_canvas_px(&c, 0, 4, 0x445566u);

    {
        int i, bad = 0;

        for (i = 0; i < 16; ++i) {
            if ((buf[i] != 0x11111111u) && (buf[i] != minimap_color(0x112233u))) ++bad;
        }
        CHECK(bad == 0, "%d out-of-bounds writes leaked into the buffer", bad);
    }

    /* The 3DS framebuffer is stored rotated and y runs backwards - the canvas has to index that
       correctly (a negative stride must never be used as an unsigned value).  Mock: 3 screen columns
       of 3 pixels, stored as a 4 x 3 buffer - the real bottom screen is 320 x 240 in a 240 x 320
       buffer. */
    {
        uint32_t rot[4 * 3];

        memset(rot, 0x22, sizeof(rot));
        c.px       = rot + 2;                       /* last pixel of a buffer row */
        c.fmt      = DR3_CANVAS_RGBA8888;
        c.stride_x = 4;
        c.stride_y = -1;
        c.w        = 3;
        c.h        = 3;

        dr3_canvas_px(&c, 0, 0, 0x000001u);         /* -> rot[2]        */
        dr3_canvas_px(&c, 0, 2, 0x000002u);         /* -> rot[0]        */
        dr3_canvas_px(&c, 2, 1, 0x000003u);         /* -> rot[2*4 + 1]  */

        CHECK(rot[2] == minimap_color(0x000001u), "rotated (0,0) -> rot[2] is 0x%08X", rot[2]);
        CHECK(rot[0] == minimap_color(0x000002u), "rotated (0,2) -> rot[0] is 0x%08X", rot[0]);
        CHECK(rot[2 * 4 + 1] == minimap_color(0x000003u), "rotated (2,1) is 0x%08X", rot[2 * 4 + 1]);
    }

    c.px = buf; c.fmt = DR3_CANVAS_RGBA8888; c.stride_x = 1; c.stride_y = 4; c.w = 4; c.h = 4;

    memset(buf, 0, sizeof(buf));
    dr3_canvas_fill(&c, 0, 0, 4, 2, 0x0000AAu);
    CHECK((buf[0] == minimap_color(0x0000AAu)) && (buf[3] == minimap_color(0x0000AAu)) &&
          (buf[4] == minimap_color(0x0000AAu)) && (buf[7] == minimap_color(0x0000AAu)),
          "fill did not cover two rows");
    CHECK(buf[8] == 0u, "fill wrote past its height");

    memset(buf, 0, sizeof(buf));
    dr3_canvas_rect(&c, 0, 0, 4, 4, 0x0000BBu);
    CHECK((buf[0] == minimap_color(0x0000BBu)) && (buf[3] == minimap_color(0x0000BBu)) &&
          (buf[15] == minimap_color(0x0000BBu)), "rect corners missing");
    CHECK(buf[5] == 0u, "rect painted its inside");
}

static void test_minimap_draw(void)
{
    static uint32_t buf[16 * 16];
    static uint8_t  mask[8 * 8];
    dr3_canvas_t    c;
    dr3_map_car_t   cars[2];
    char            ascii[512];
    int             x, y;

    printf("- minimap: drawing, cars and the log preview\n");

    for (y = 0; y < 8; ++y)
        for (x = 0; x < 8; ++x) mask[y * 8 + x] = (y == 4) ? 0x0F : 0x00;

    CHECK(dr3_minimap_build(mask, NULL, NULL, 8, 8) == 1, "build failed");

    memset(buf, 0, sizeof(buf));
    c.px = buf; c.fmt = DR3_CANVAS_RGBA8888; c.stride_x = 1; c.stride_y = 16; c.w = 16; c.h = 16;

    cars[0].x = 2.5f; cars[0].y = 6.5f; cars[0].color = 0; cars[0].is_player = 0; cars[0].valid = 1;
    cars[1].x = 4.5f; cars[1].y = 4.5f; cars[1].color = 0; cars[1].is_player = 1; cars[1].valid = 1;

    CHECK(dr3_minimap_draw(&c, 0, 0, 16, 16, cars, 2) == 1, "draw failed");

    /* the 8x8 map is scaled 2x: map pixel (x,y) -> canvas (2x..2x+1, 2y..2y+1).
       Probes stay clear of the car markers. */
    CHECK(buf[8 * 16 + 3] == minimap_color(DR3_MAP_COL_ROAD), "road not drawn: 0x%08X",
          buf[8 * 16 + 3]);
    CHECK(buf[2 * 16 + 2] == minimap_color(DR3_MAP_COL_OFFROAD), "ground not drawn: 0x%08X",
          buf[2 * 16 + 2]);
    CHECK(buf[0] == minimap_color(DR3_MAP_COL_OFFROAD), "no frame expected: 0x%08X", buf[0]);
    CHECK(buf[15 * 16 + 15] == minimap_color(DR3_MAP_COL_OFFROAD), "bottom right is not the map: 0x%08X",
          buf[15 * 16 + 15]);

    /* the player: map (4,4) -> canvas (8,8), a 5x5 marker in the fallback yellow with a dark, rounded
       outline around it */
    CHECK(buf[8 * 16 + 8] == minimap_color(DR3_MAP_COL_PLAYER), "player marker: 0x%08X",
          buf[8 * 16 + 8]);
    CHECK(buf[5 * 16 + 6] == minimap_color(DR3_MAP_COL_OUTLINE), "player marker has no outline: 0x%08X",
          buf[5 * 16 + 6]);
    CHECK(buf[5 * 16 + 5] == minimap_color(DR3_MAP_COL_OFFROAD),
          "the outline corner should be left out (rounded marker): 0x%08X", buf[5 * 16 + 5]);
    CHECK(buf[4 * 16 + 4] == minimap_color(DR3_MAP_COL_OFFROAD), "player marker too large: 0x%08X",
          buf[4 * 16 + 4]);

    /* another car: map (2,6) -> canvas (4,12), 3x3 - and it must not cover the player */
    CHECK(buf[12 * 16 + 4] == minimap_color(DR3_MAP_COL_CAR), "car marker: 0x%08X", buf[12 * 16 + 4]);

    /* a marker in the driver's own colour when one is given */
    cars[1].color = 0x123456u;
    CHECK(dr3_minimap_draw(&c, 0, 0, 16, 16, cars, 2) == 1, "draw with a car colour failed");
    CHECK(buf[8 * 16 + 8] == minimap_color(0x123456u), "player colour ignored: 0x%08X",
          buf[8 * 16 + 8]);
    cars[1].color = 0;

    /* without a loaded track: plain background, and it must not crash */
    dr3_minimap_reset();
    memset(buf, 0, sizeof(buf));
    CHECK(dr3_minimap_draw(&c, 0, 0, 16, 16, cars, 2) == 1, "draw without a track failed");
    CHECK(buf[8 * 16 + 8] == minimap_color(DR3_MAP_COL_BACKGROUND), "background missing: 0x%08X",
          buf[8 * 16 + 8]);
    CHECK(buf[0] == minimap_color(DR3_MAP_COL_BACKGROUND), "background missing top left");
    CHECK(buf[15 * 16 + 15] == minimap_color(DR3_MAP_COL_BACKGROUND), "background missing bottom right");

    /* invalid cars must be skipped instead of drawn at (0,0) */
    memset(buf, 0, sizeof(buf));
    cars[0].valid = 0;
    cars[1].valid = 0;
    CHECK(dr3_minimap_build(mask, NULL, NULL, 8, 8) == 1, "second build failed");
    dr3_minimap_draw(&c, 0, 0, 16, 16, cars, 2);
    CHECK(buf[0 * 16 + 5] != minimap_color(DR3_MAP_COL_CAR), "an invalid car was drawn");
    cars[0].valid = 1;
    cars[1].valid = 1;

    /* the ASCII preview that goes into drally_3ds.log */
    CHECK(dr3_minimap_ascii(ascii, sizeof(ascii), 8, 8) > 0, "ascii preview is empty");
    CHECK(strstr(ascii, "########") != NULL, "no road row in the ascii preview:\n%s", ascii);
    CHECK(ascii[strlen(ascii) - 1] == '\n', "ascii preview does not end with a newline");

    dr3_minimap_reset();
    CHECK(dr3_minimap_ascii(ascii, sizeof(ascii), 4, 2) > 0, "ascii preview without a map is empty");
    CHECK(strstr(ascii, "#") == NULL, "ascii preview without a map shows a road");
}

static void test_minimap_letterbox(void)
{
    static uint32_t buf[16 * 16];
    static uint8_t  mask[8 * 4];
    dr3_canvas_t    c;
    int             x, y;

    printf("- minimap: letterboxing a wide track\n");

    for (y = 0; y < 4; ++y)
        for (x = 0; x < 8; ++x) mask[y * 8 + x] = (y == 2) ? 0x0F : 0x00;

    CHECK(dr3_minimap_build(mask, NULL, NULL, 8, 4) == 1, "build failed for an 8x4 track");
    CHECK((dr3_minimap_w() == 8) && (dr3_minimap_h() == 4), "map %dx%d (want 8x4)",
          dr3_minimap_w(), dr3_minimap_h());

    memset(buf, 0, sizeof(buf));
    c.px = buf; c.fmt = DR3_CANVAS_RGBA8888; c.stride_x = 1; c.stride_y = 16; c.w = 16; c.h = 16;
    CHECK(dr3_minimap_draw(&c, 0, 0, 16, 16, NULL, 0) == 1, "draw failed");

    /* an 8x4 map in a 16x16 rectangle: 16x8, centred -> rows 4..11, map pixel (x,y) -> (2x, 4+2y) */
    CHECK(buf[4 * 16 + 0] == minimap_color(DR3_MAP_COL_OFFROAD), "letterbox top row missing: 0x%08X",
          buf[4 * 16 + 0]);
    CHECK(buf[11 * 16 + 15] == minimap_color(DR3_MAP_COL_OFFROAD), "letterbox bottom row missing: 0x%08X",
          buf[11 * 16 + 15]);
    CHECK(buf[0] == minimap_color(DR3_MAP_COL_BACKGROUND), "no letterbox background above");
    CHECK(buf[15 * 16 + 5] == minimap_color(DR3_MAP_COL_BACKGROUND), "no letterbox background below");
    CHECK(buf[8 * 16 + 5] == minimap_color(DR3_MAP_COL_ROAD), "road not drawn in the letterbox: 0x%08X",
          buf[8 * 16 + 5]);
}

static void test_minimap_pixel_formats(void)
{
    static uint16_t rgb565[4 * 4];
    static uint8_t  bgr888[4 * 4 * 3];
    static uint8_t  mask[8 * 8];
    dr3_canvas_t    c;
    int             x, y;

    printf("- minimap: RGB565 and 24-bit targets (the 3DS bottom screen is 16-bit)\n");

    for (y = 0; y < 8; ++y)
        for (x = 0; x < 8; ++x) mask[y * 8 + x] = (y == 4) ? 0x0F : 0x00;

    CHECK(dr3_minimap_build(mask, NULL, NULL, 8, 8) == 1, "build failed");

    /* RGB565: 2 bytes per pixel - the format the 3DS bottom screen really uses (consoleInit switches
       it there).  A 4-byte store would overwrite two pixels at once, which is the bug this test
       exists for. */
    memset(rgb565, 0xAA, sizeof(rgb565));
    c.px = rgb565; c.fmt = DR3_CANVAS_RGB565; c.stride_x = 1; c.stride_y = 4; c.w = 4; c.h = 4;

    dr3_canvas_px(&c, 1, 2, 0xD2D2DCu);                 /* the road colour */
    CHECK(rgb565[2 * 4 + 1] == minimap_color565(0xD2D2DCu),
          "RGB565 packing: 0x%04X (want 0x%04X)", rgb565[2 * 4 + 1], minimap_color565(0xD2D2DCu));
    CHECK(rgb565[2 * 4 + 2] == 0xAAAAu, "RGB565 write touched the next pixel: 0x%04X",
          rgb565[2 * 4 + 2]);

    /* a whole draw into a 16-bit target: map and no damage outside */
    memset(rgb565, 0, sizeof(rgb565));
    CHECK(dr3_minimap_draw(&c, 0, 0, 4, 4, NULL, 0) == 1, "draw into an RGB565 target failed");
    CHECK(rgb565[2 * 4 + 1] == minimap_color565(DR3_MAP_COL_ROAD),
          "road missing in the RGB565 target: 0x%04X", rgb565[2 * 4 + 1]);
    CHECK(rgb565[0] == minimap_color565(DR3_MAP_COL_OFFROAD),
          "map missing in the RGB565 target: 0x%04X", rgb565[0]);

    /* 24-bit BGR888 (GSP_BGR8_OES): 3 bytes per pixel, memory order B, G, R */
    memset(bgr888, 0x11, sizeof(bgr888));
    c.px = bgr888; c.fmt = DR3_CANVAS_BGR888; c.stride_x = 1; c.stride_y = 4; c.w = 4; c.h = 4;

    dr3_canvas_px(&c, 1, 1, 0x112233u);
    CHECK((bgr888[(1 * 4 + 1) * 3 + 0] == 0x33u) && (bgr888[(1 * 4 + 1) * 3 + 1] == 0x22u) &&
          (bgr888[(1 * 4 + 1) * 3 + 2] == 0x11u), "BGR888 byte order: %02X %02X %02X",
          bgr888[(1 * 4 + 1) * 3 + 0], bgr888[(1 * 4 + 1) * 3 + 1], bgr888[(1 * 4 + 1) * 3 + 2]);
    CHECK(bgr888[(1 * 4 + 2) * 3 + 0] == 0x11u, "BGR888 write touched the next pixel");
}

static void test_minimap_track_colors(void)
{
    static uint8_t  mask[4 * 4];          /* row 2 is the road, the rest is soft ground */
    static uint8_t  image[4 * 4];         /* road pixels use palette 1, ground palette 2 */
    static uint8_t  palette[0x300];       /* entry 0 is the brightest -> sets the scale */
    static uint32_t buf[4 * 4];
    dr3_canvas_t    c;
    int             x, y;

    printf("- minimap: colours taken from the track image\n");

    memset(palette, 0, sizeof(palette));
    palette[0 * 3 + 0] = 200;                                     /* 200 -> scale 255/200 */
    palette[1 * 3 + 0] = 100; palette[1 * 3 + 1] = 100; palette[1 * 3 + 2] = 100;
    palette[2 * 3 + 0] = 0;   palette[2 * 3 + 1] = 100; palette[2 * 3 + 2] = 0;

    for (y = 0; y < 4; ++y) {
        for (x = 0; x < 4; ++x) {
            const int road = (y == 2);

            mask [y * 4 + x] = (uint8_t)(road ? 0x0F : 0x00);
            image[y * 4 + x] = (uint8_t)(road ? 1 : 2);
        }
    }

    CHECK(dr3_minimap_build(mask, image, palette, 4, 4) == 1, "build with a track image failed");

    memset(buf, 0, sizeof(buf));
    c.px = buf; c.fmt = DR3_CANVAS_RGBA8888; c.stride_x = 1; c.stride_y = 4; c.w = 4; c.h = 4;
    CHECK(dr3_minimap_draw(&c, 0, 0, 4, 4, NULL, 0) == 1, "draw failed");

    /* palette 1 = (100,100,100) scales to 127 and is brightened by 5/4 on the road = 158 */
    CHECK(buf[2 * 4 + 1] == minimap_color(0x9E9E9Eu), "road colour: 0x%08X (want 0x%08X)",
          buf[2 * 4 + 1], minimap_color(0x9E9E9Eu));
    /* palette 2 = (0,100,0) scales to green 127 and is darkened by 3/4 off the road = 95 */
    CHECK(buf[1 * 4 + 1] == minimap_color(0x005F00u), "ground colour: 0x%08X (want 0x%08X)",
          buf[1 * 4 + 1], minimap_color(0x005F00u));

    /* without a palette the fixed fallback scheme has to come back */
    CHECK(dr3_minimap_build(mask, image, NULL, 4, 4) == 1, "build without a palette failed");
    memset(buf, 0, sizeof(buf));
    CHECK(dr3_minimap_draw(&c, 0, 0, 4, 4, NULL, 0) == 1, "draw failed");
    CHECK(buf[2 * 4 + 1] == minimap_color(DR3_MAP_COL_ROAD), "fallback road colour: 0x%08X",
          buf[2 * 4 + 1]);
}

static void test_minimap_incremental(void)
{
    static uint32_t buf[32 * 32];
    static uint8_t  mask[8 * 8];
    dr3_canvas_t    c;
    dr3_map_car_t   cars[1];
    int             x, y;

    printf("- minimap: incremental update (only the markers are repainted)\n");

    for (y = 0; y < 8; ++y)
        for (x = 0; x < 8; ++x) mask[y * 8 + x] = (y == 4) ? 0x0F : 0x00;
    CHECK(dr3_minimap_build(mask, NULL, NULL, 8, 8) == 1, "build failed");

    memset(buf, 0, sizeof(buf));
    c.px = buf; c.fmt = DR3_CANVAS_RGBA8888; c.stride_x = 1; c.stride_y = 32; c.w = 32; c.h = 32;

    /* the first incremental call has no previous state and paints the whole thing */
    cars[0].x = 2.5f; cars[0].y = 2.5f; cars[0].color = 0; cars[0].is_player = 1; cars[0].valid = 1;
    CHECK(dr3_minimap_draw_incremental(&c, 0, 0, 32, 32, cars, 1) == 1, "first incremental failed");
    /* an 8x8 map in a 32x32 rectangle: scale 4, so map (2,2) -> canvas (8,8) */
    CHECK(buf[8 * 32 + 8] == minimap_color(DR3_MAP_COL_PLAYER), "marker missing: 0x%08X",
          buf[8 * 32 + 8]);

    /* move the car: the old marker pixels must be restored from the map and the new marker drawn */
    cars[0].x = 6.5f; cars[0].y = 6.5f;
    dr3_minimap_draw_incremental(&c, 0, 0, 32, 32, cars, 1);
    CHECK(buf[8 * 32 + 8] == minimap_color(DR3_MAP_COL_OFFROAD),
          "old marker position not restored: 0x%08X", buf[8 * 32 + 8]);
    CHECK(buf[24 * 32 + 24] == minimap_color(DR3_MAP_COL_PLAYER), "new marker missing: 0x%08X",
          buf[24 * 32 + 24]);
    /* a road pixel away from both markers is untouched (the static map is not repainted at all) */
    CHECK(buf[16 * 32 + 4] == minimap_color(DR3_MAP_COL_ROAD), "road damaged by the update: 0x%08X",
          buf[16 * 32 + 4]);

    /* the car disappears -> the marker has to vanish completely */
    cars[0].valid = 0;
    dr3_minimap_draw_incremental(&c, 0, 0, 32, 32, cars, 1);
    CHECK(buf[24 * 32 + 24] == minimap_color(DR3_MAP_COL_OFFROAD), "marker not removed: 0x%08X",
          buf[24 * 32 + 24]);
}

static void test_laptime(void)
{
    char t[16];

    printf("- lap time formatting\n");

    /* the engine's triples: minutes / seconds / hundredths */
    dr3_laptime_format(t, sizeof(t), 0, 0, 0);
    CHECK(strcmp(t, "0:00.00") == 0, "zero time: got '%s'", t);

    dr3_laptime_format(t, sizeof(t), 1, 2, 3);
    CHECK(strcmp(t, "1:02.03") == 0, "m:ss.cc padding: got '%s'", t);

    dr3_laptime_format(t, sizeof(t), 12, 59, 99);
    CHECK(strcmp(t, "12:59.99") == 0, "two digit minutes: got '%s'", t);

    /* hundredths and seconds roll over instead of printing a field the row cannot show */
    dr3_laptime_format(t, sizeof(t), 0, 75, 123);
    CHECK(strcmp(t, "1:16.23") == 0, "roll over: got '%s'", t);

    /* negative input is clamped - a time row must never show '-' */
    dr3_laptime_format(t, sizeof(t), -1, -2, -3);
    CHECK(strcmp(t, "0:00.00") == 0, "negative input: got '%s'", t);

    /* the tick variant uses the engine's own arithmetic: 70 ticks = 1 s, 1.42 * rest = hundredths */
    dr3_laptime_format_ticks(t, sizeof(t), 0);
    CHECK(strcmp(t, "0:00.00") == 0, "ticks 0: got '%s'", t);

    dr3_laptime_format_ticks(t, sizeof(t), 70);
    CHECK(strcmp(t, "0:01.00") == 0, "ticks 70: got '%s'", t);

    dr3_laptime_format_ticks(t, sizeof(t), 69);
    CHECK(strcmp(t, "0:00.97") == 0, "ticks 69 (1.42 * 69 = 97): got '%s'", t);

    dr3_laptime_format_ticks(t, sizeof(t), 70 * 60);
    CHECK(strcmp(t, "1:00.00") == 0, "ticks of one minute: got '%s'", t);

    dr3_laptime_format_ticks(t, sizeof(t), (70 * 119) + 69);
    CHECK(strcmp(t, "1:59.97") == 0, "ticks 1:59.97: got '%s'", t);

    dr3_laptime_format_ticks(t, sizeof(t), -5);
    CHECK(strcmp(t, "0:00.00") == 0, "negative ticks: got '%s'", t);

    /* "nothing driven yet" is what the times row prints as dashes, and it has to be as wide as a
       real time so the three columns of the row stay aligned */
    CHECK(dr3_laptime_is_set(0, 0, 0) == 0, "an all zero triple counts as set");
    CHECK(dr3_laptime_is_set(0, 0, 1) == 1, "one hundredth is not seen as set");

    dr3_laptime_format_or_unset(t, sizeof(t), 0, 0, 0);
    CHECK(strcmp(t, DR3_LAPTIME_UNSET) == 0, "unset time: got '%s'", t);
    CHECK(strlen(DR3_LAPTIME_UNSET) == strlen("0:42.90"), "unset time is not as wide as a real one");

    dr3_laptime_format_or_unset(t, sizeof(t), 0, 42, 90);
    CHECK(strcmp(t, "0:42.90") == 0, "set time: got '%s'", t);

    /* a short buffer is truncated and terminated, never overrun */
    memset(t, 0x7f, sizeof(t));
    t[sizeof(t) - 1] = 0;
    dr3_laptime_format_ticks(t, 5, 70 * 119);
    CHECK((strcmp(t, "1:59") == 0) && (t[4] == 0), "short buffer: got '%s'", t);
}

int main(void)
{
    printf("dRally 3DS port - host tests\n\n");
    test_lut();
    test_lut_masks();
    test_blit_center();
    test_blit_stretch();
    test_input_map();
    test_text_and_filter();
    test_minimap_build();
    test_minimap_downscale();
    test_minimap_canvas();
    test_minimap_draw();
    test_minimap_letterbox();
    test_minimap_pixel_formats();
    test_minimap_track_colors();
    test_minimap_incremental();
    test_laptime();

    printf("\n%d checks, %d failures\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
