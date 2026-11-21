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
#include <math.h>

#include "../platform_3ds/dr3_anniv_gfx.h"
#include "../platform_3ds/dr3_anniv_prog.h"
#include "../platform_3ds/dr3_arena.h"
#include "../platform_3ds/dr3_blit.h"
#include "../platform_3ds/dr3_boss.h"
#include "../platform_3ds/dr3_cash.h"
#include "../platform_3ds/dr3_dm.h"
#include "../platform_3ds/dr3_grid.h"
#include "../platform_3ds/dr3_input_map.h"
#include "../platform_3ds/dr3_laptime.h"
#include "../platform_3ds/dr3_minimap.h"
#include "../platform_3ds/dr3_bottom_ui.h"
#include "../platform_3ds/dr3_payout.h"
#include "../platform_3ds/dr3_results.h"


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

    /* quick save / quick load: the front end's F2/F3 (___2a6a8h.c, used by the shop and the
       underground).  ZL/ZR for a New 3DS, X/Y so a plain 3DS has a default as well */
    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_ZL;
    dr3_input_scancodes(&st, set);
    CHECK(set[SDL_SCANCODE_F2], "ZL must quick save in the front end");
    CHECK(!set[SDL_SCANCODE_LSHIFT], "ZL must not boost outside a race");

    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_X;
    dr3_input_scancodes(&st, set);
    CHECK(set[SDL_SCANCODE_F2], "X must quick save on a 3DS without ZL/ZR");

    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_ZR;
    dr3_input_scancodes(&st, set);
    CHECK(set[SDL_SCANCODE_F3], "ZR must quick load in the front end");

    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_Y;
    dr3_input_scancodes(&st, set);
    CHECK(set[SDL_SCANCODE_F3], "Y must quick load on a 3DS without ZL/ZR");

    /* while racing the same buttons are boost and shoot, not save and load */
    dr3_input_set_context(1);
    memset(&st, 0, sizeof(st));
    st.held = DR3_PAD_ZL;
    dr3_input_scancodes(&st, set);
    CHECK(set[SDL_SCANCODE_LSHIFT], "ZL must boost in a race");
    CHECK(!set[SDL_SCANCODE_F2], "ZL must not quick save in a race");
    dr3_input_set_context(0);

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

    /* the on screen hint asks for the bound buttons by function name (see dr3_bottom.c) */
    {
        char b[32];

        CHECK(dr3_input_binding_name(b, sizeof(b), "QUICKSAVE") == 2, "quick save must have 2 buttons");
        CHECK(strcmp(b, "ZL, X") == 0, "quick save buttons: got '%s'", b);

        CHECK(dr3_input_binding_name(b, sizeof(b), "QUICKLOAD") == 2, "quick load must have 2 buttons");
        CHECK(strcmp(b, "ZR, Y") == 0, "quick load buttons: got '%s'", b);

        CHECK(dr3_input_binding_name(b, sizeof(b), "CONFIRM") == 1, "confirm must have one button");
        CHECK(strcmp(b, "A") == 0, "confirm button: got '%s'", b);

        CHECK(dr3_input_binding_name(b, sizeof(b), "GAS") == 3, "gas must have 3 buttons");
        CHECK(strcmp(b, "R, UP, STICK_UP") == 0, "gas buttons: got '%s'", b);

        CHECK(dr3_input_binding_name(b, sizeof(b), "NOSUCHFUNCTION") == 0,
              "an unknown function must return 0");
        CHECK(b[0] == 0, "an unknown function must leave the buffer empty");

        /* a short buffer stops instead of writing past its end */
        memset(b, 0x7f, sizeof(b));
        CHECK(dr3_input_binding_name(b, 6, "QUICKSAVE") == 2, "the short buffer must still be filled");
        CHECK(b[5] == 0, "the binding list must stay inside the buffer");
    }
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

    /* the shifted characters are reachable as shift + the key below them (the engine takes the
       character from its upper[] table then) - they used to be dropped, which is why a typed '!' or
       '?' never arrived */
    CHECK(dr3_char_to_scancode('!') == SDL_SCANCODE_1, "'!' -> SCANCODE_1");
    CHECK(dr3_char_needs_shift('!') == 1, "'!' must be sent with shift");
    CHECK(dr3_char_to_scancode('?') == SDL_SCANCODE_SLASH, "'?' -> SCANCODE_SLASH");
    CHECK(dr3_char_needs_shift('?') == 1, "'?' must be sent with shift");
    CHECK(dr3_char_to_scancode('_') == SDL_SCANCODE_MINUS, "'_' -> SCANCODE_MINUS");
    CHECK(dr3_char_needs_shift('_') == 1, "'_' must be sent with shift");
    CHECK(dr3_char_to_scancode(':') == SDL_SCANCODE_SEMICOLON, "':' -> SCANCODE_SEMICOLON");
    CHECK(dr3_char_to_scancode('+') == SDL_SCANCODE_KP_PLUS, "'+' -> the keypad key, unshifted");
    CHECK(dr3_char_needs_shift('-') == 0, "'-' must go without shift");
    CHECK(dr3_char_needs_shift('1') == 0, "'1' must go without shift");
    CHECK(dr3_char_needs_shift(39) == 0, "the apostrophe must go without shift");

    /* umlauts have no scan code at all (the engine's table ends at 0x7f): transliterate, do not drop */
    CHECK(dr3_char_to_scancode((char)0xfc) == SDL_SCANCODE_U, "u-umlaut -> SCANCODE_U");
    CHECK(dr3_char_to_scancode((char)0xdf) == SDL_SCANCODE_S, "sharp s -> SCANCODE_S");
    CHECK(dr3_char_needs_shift((char)0xfc) == 0, "a transliterated umlaut needs no shift");

    /* something no scancode can produce is rejected - the input layer logs it */
    CHECK(dr3_char_to_scancode((char)0x80) == -1, "0x80 has no scancode and must be rejected");

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

/* the marker colours of the cars: bright enough to see (the engine's car ramps are dark palette values) */
static void test_minimap_marker_colour(void)
{
    static uint32_t buf[32 * 32];
    static uint8_t  mask[8 * 8];
    dr3_canvas_t    c;
    dr3_map_car_t   cars[20];
    int             i, x, y, seen = 0;

    printf("- minimap: marker colours, a full field of cars, one paint per page\n");

    CHECK(dr3_minimap_marker_colour(0x000000u) == 0u, "black must stay 0 (the caller takes the default colour)");
    CHECK(dr3_minimap_marker_colour(0x3F0000u) == 0xFF0000u, "a dark red scales to full red: 0x%06X", dr3_minimap_marker_colour(0x3F0000u));
    CHECK(dr3_minimap_marker_colour(0x102030u) == 0x55AAFFu, "hue kept: 0x%06X", dr3_minimap_marker_colour(0x102030u));
    CHECK(dr3_minimap_marker_colour(0xFFFFFFu) == 0xFFFFFFu, "white stays white");

    for (y = 0; y < 8; ++y)
        for (x = 0; x < 8; ++x) mask[y * 8 + x] = (y == 4) ? 0x0F : 0x00;
    CHECK(dr3_minimap_build(mask, NULL, NULL, 8, 8) == 1, "build failed");

    memset(buf, 0, sizeof(buf));
    c.px = buf; c.fmt = DR3_CANVAS_RGBA8888; c.stride_x = 1; c.stride_y = 32; c.w = 32; c.h = 32;

    /* twenty cars, as many as a chaos field has: every one gets its marker and every marker is erased again */
    for (i = 0; i < 20; ++i) {
        cars[i].x = 0.5f + (float)(i % 7); cars[i].y = 0.5f + (float)(i / 7) * 2.0f;
        cars[i].color = 0x101010u; cars[i].is_player = (i == 19); cars[i].valid = 1;
    }
    CHECK(dr3_minimap_draw(&c, 0, 0, 32, 32, cars, 20) == 1, "full draw failed");
    for (i = 0; i < 32 * 32; ++i) seen += (buf[i] == minimap_color(0xFFFFFFu));
    CHECK(seen > 0, "no bright marker pixel on the canvas");

    for (i = 0; i < 20; ++i) cars[i].valid = 0;
    dr3_minimap_draw_incremental(&c, 0, 0, 32, 32, cars, 20);
    seen = 0;
    for (i = 0; i < 32 * 32; ++i) seen += (buf[i] == minimap_color(0xFFFFFFu)) || (buf[i] == minimap_color(DR3_MAP_COL_PLAYER));
    CHECK(seen == 0, "%d marker pixel(s) left after all 20 cars vanished (marker table too small?)", seen);

    /* after a full draw the incremental draw must not paint the band again: damage a road pixel, update, it stays */
    {
        const uint32_t road = minimap_color(DR3_MAP_COL_ROAD);
        int            at   = -1;

        for (i = 0; i < 32 * 32; ++i) if (buf[i] == road) { at = i; break; }
        CHECK(at >= 0, "no road pixel to test with");
        if (at >= 0) {
            buf[at] = 0x12345678u;
            dr3_minimap_draw_incremental(&c, 0, 0, 32, 32, cars, 20);
            CHECK(buf[at] == 0x12345678u, "the incremental draw repainted the band (a second visible paint)");
        }
    }

    /* a new map has to be painted in full again */
    CHECK(dr3_minimap_build(mask, NULL, NULL, 8, 8) == 1, "second build failed");
    memset(buf, 0, sizeof(buf));
    dr3_minimap_draw_incremental(&c, 0, 0, 32, 32, cars, 20);
    for (i = 0, seen = 0; i < 32 * 32; ++i) seen += (buf[i] == minimap_color(DR3_MAP_COL_ROAD));
    CHECK(seen > 0, "a new map was not painted by the first incremental draw");
}

/*
 * The chaos grid (platform_3ds/dr3_grid.c): where the extra cars of a bigger race stand.  These tests
 * use small synthetic tracks - the surface classification is the same as the minimap's, so a "road"
 * here is 0x0f (hard surface) and everything else cannot carry a car.
 */

static void grid_road(uint8_t *mask, int w, int h, int x0, int y0, int x1, int y1)
{
    int x, y;

    for (y = y0; y < y1; ++y)
        for (x = x0; x < x1; ++x) mask[y * w + x] = DR3_GRID_ROAD;
}

/*
 * Every returned spot has to be on the road and clear of all the others - judged with the placement's
 * own rule (dr3_grid_pair_ok: outside the engine's contact box, or as far apart as the original start
 * slots), so the engine never lets two of them collide (race___4dcach.c).
 */
static void grid_check_spots(const uint8_t *mask, int w, int h, const dr3_grid_spot_t *spot, int got,
                             const char * what)
{
    int i, j;

    for (i = 0; i < got; ++i) {

        const int x = (int)spot[i].x, y = (int)spot[i].y;

        CHECK((spot[i].side == DR3_GRID_BEHIND) || (spot[i].side == DR3_GRID_AHEAD),
              "%s: spot %d has side %d", what, i, spot[i].side);
        CHECK((x >= 0) && (y >= 0) && (x < w) && (y < h), "%s: spot %d at %d,%d is outside", what, i, x, y);
        if ((x < 0) || (y < 0) || (x >= w) || (y >= h)) continue;

        CHECK(mask[y * w + x] == DR3_GRID_ROAD, "%s: spot %d at %d,%d is not on the road (0x%02x)",
              what, i, x, y, mask[y * w + x]);

        for (j = 0; j < i; ++j) {

            const double dx = (double)x - (double)spot[j].x;
            const double dy = (double)y - (double)spot[j].y;

            CHECK(dr3_grid_pair_ok(spot[i].x, spot[i].y, (double)spot[i].dir, &spot[j]),
                  "%s: spots %d and %d stand %.1f px apart (dx %.0f dy %.0f, lane %d/%d) - the engine "
                  "would let them collide", what, j, i, sqrt(dx * dx + dy * dy), dx, dy,
                  spot[j].lane, spot[i].lane);
        }
    }
}

/*
 * The clearance rule itself.  It has to accept the layout the placement hands out (a second lane 36 px
 * across with a 20 px stagger, a whole row of 52 px along) and it has to reject what the field collided
 * with: two cars 28 px apart across and *no* stagger.  And a pair outside the engine's 40 px contact
 * box is always fine, whatever the sprites look like (race___4dcach.c only compares pixels inside it).
 *
 * The numbers are the ones the original tracks use for their own four slots: 34 px across, 18 px along
 * (TR6 slots (610,311) and (644,293)) - the rule is asked to be a little stricter than that.
 */
static void test_grid_pair_rule(void)
{
    dr3_grid_spot_t car;
    const double    dir = 180.0;      /* the direction of the TR6 grid (rot 48) */

    printf("- chaos grid: the clearance rule\n");

    memset(&car, 0, sizeof(car));
    car.x = 600.0f;
    car.y = 600.0f;
    car.dir = (float)dir;

    /* the direction vector of 180 deg is (0,-1) in the mask, so +x is across and -y is along */

    CHECK(dr3_grid_pair_ok(600.0 + 36.0, 600.0 - 20.0, dir, &car),
          "36 px across with a 20 px stagger was rejected");
    CHECK(dr3_grid_pair_ok(600.0 + 0.0, 600.0 - 52.0, dir, &car),
          "a whole row (52 px along) was rejected");
    CHECK(dr3_grid_pair_ok(600.0 + 40.0, 600.0, dir, &car),
          "a car 40 px across was rejected - outside the engine's contact box");
    CHECK(dr3_grid_pair_ok(600.0, 600.0 + 40.0, dir, &car),
          "a car 40 px along was rejected - outside the engine's contact box");
    CHECK(dr3_grid_pair_ok(600.0 + 200.0, 600.0, dir, &car), "a distant car was rejected");

    CHECK(!dr3_grid_pair_ok(600.0 + 28.0, 600.0, dir, &car),
          "28 px across without a stagger was accepted - that is the pair the field collided with");
    CHECK(!dr3_grid_pair_ok(600.0 + 30.0, 600.0 - 10.0, dir, &car),
          "30 px across with a 10 px stagger was accepted");
    CHECK(!dr3_grid_pair_ok(600.0 + 0.0, 600.0 - 40.0 + 1.0, dir, &car),
          "39 px along was accepted (that is inside the engine's contact box)");
    CHECK(!dr3_grid_pair_ok(600.0, 600.0, dir, &car), "two cars on the same spot were accepted");
}

static void test_grid_straight(void)
{
    static uint8_t  mask[900 * 100];
    dr3_grid_t      track;
    dr3_grid_car_t  grid[4];
    dr3_grid_spot_t spot[DR3_GRID_CARS];
    int             n, got, behind = 0, ahead = 0;

    printf("- chaos grid: a straight road\n");

    memset(mask, 0x00, sizeof(mask));                /* soft ground everywhere ...      */
    grid_road(mask, 900, 100, 10, 20, 890, 80);      /* ... with a corridor through it  */

    track.mask = mask; track.mask_w = 900; track.mask_h = 100;
    track.rot = 72;                                  /* Direction 270 -> the nose is +x */

    grid[0].x = 400.0f; grid[0].y = 35.0f;           /* the front row ... */
    grid[1].x = 400.0f; grid[1].y = 65.0f;
    grid[2].x = 370.0f; grid[2].y = 35.0f;           /* ... and the row behind it */
    grid[3].x = 370.0f; grid[3].y = 65.0f;

    got = dr3_grid_place(&track, grid, 4, spot, 8);
    CHECK(got > 0, "no extra car placed on a wide straight road");

    grid_check_spots(mask, 900, 100, spot, got, "straight road");

    for (n = 0; n < got; ++n) {

        if (spot[n].side == DR3_GRID_BEHIND) ++behind;
        else                                 ++ahead;

        /* the column keeps to the middle of the corridor (y 20..80 -> 50) */
        CHECK((spot[n].y > 42.0f) && (spot[n].y < 58.0f),
              "spot %d is not in the middle of the corridor (y %.0f)", n, (double)spot[n].y);

        /*
         * On a straight stretch the march goes straight and every car carries the starters' own frame.
         * Only the cars *behind* the grid are checked here: at the end of this dead-end corridor the march
         * turns round and puts the last cars *in front* of the grid (no real circuit has a corridor that
         * ends), and those point at their front-man the other way round - the host probe measures that
         * case on the real tracks instead ("cars standing more than 50 deg across the queue").
         */
        /*
         * ... and not within 60 px of the end of this stub of a corridor: right at the wall the road's
         * own line *is* a diagonal - it turns away from the wall and has more road than the road itself
         * - so those spots are no longer a straight stretch.  The real tracks are measured by the host
         * probe (grid_probe.exe), not by this synthetic corridor.
         */
        if ((spot[n].x > 70.0f) && (spot[n].x < 360.0f)) {

            const int frame = dr3_grid_img_index(spot[n].dir);
            int       off   = frame - (int)track.rot;

            if (off >  48) off -= 96;
            if (off < -48) off += 96;

            CHECK((off >= -6) && (off <= 6),
                  "spot %d at %.0f,%.0f faces frame %d, the starters frame %d (%d frames off)",
                  n, (double)spot[n].x, (double)spot[n].y, frame, track.rot, off);
        }
    }

    CHECK(behind > 0, "nothing placed behind the grid (%d went in front of it)", ahead);
    /*
     * Behind the grid first; where that road runs out the cars continue on the road *in front* of the
     * grid (dr3_grid.c).  They stand on the track, aligned with it and clear of every other car, which
     * is what a track with a short start stretch needs - sixteen of them are placed that way on Oasis
     * (host probe).  A car that finds no room at all keeps the start slot it shared.
     */
    CHECK((spot[0].side == DR3_GRID_BEHIND) && (spot[0].x < 370.0f),
          "the first extra has to stand behind the rear row (at %.0f)", (double)spot[0].x);

    /*
     * Behind means smaller x here (the nose is +x): the grid is filled row by row, two lanes at a time,
     * so every second spot of one lane is further back than the one before it.  A walk that had to turn
     * round (dr3_grid.c) fills the gaps it left, so the order is *not* simply monotonic - what has to
     * hold for every pair is the clearance rule, and grid_check_spots() above checks exactly that.
     */
}

/*
 * A road that bends back over the start line behind the grid - Oasis' first bend.  The walk may bow
 * forward over that line (DR3_GRID_ARC) while it follows the bend, but never far enough to wrap around
 * and hand out spots in front of the leader.  Before that allowance the walk stopped after a handful of
 * cars and the rest of the field was stacked on the last spot.
 */
static void test_grid_bend_back(void)
{
    static uint8_t  mask[520 * 300];
    dr3_grid_t      track;
    dr3_grid_car_t  grid[4];
    dr3_grid_spot_t spot[DR3_GRID_CARS];
    int             n, got, in_return = 0;

    printf("- chaos grid: a road that bends back behind the grid\n");

    memset(mask, 0x00, sizeof(mask));
    grid_road(mask, 520, 300,  60,  90, 500, 150);   /* the corridor the grid stands on   */
    grid_road(mask, 520, 300,  60,  30, 100,  90);   /* the bend up from its left end     */
    grid_road(mask, 520, 300,  60,  30, 500,  70);   /* ... and the way back to the right */

    track.mask = mask; track.mask_w = 520; track.mask_h = 300;
    track.rot = 72;                                  /* Direction 270 -> the nose is +x */

    grid[0].x = 200.0f; grid[0].y = 100.0f;
    grid[1].x = 200.0f; grid[1].y = 140.0f;
    grid[2].x = 170.0f; grid[2].y = 100.0f;
    grid[3].x = 170.0f; grid[3].y = 140.0f;

    got = dr3_grid_place(&track, grid, 4, spot, 8);
    CHECK(got >= 6, "only %d car(s) were placed around a bend that runs back over the start line", got);

    grid_check_spots(mask, 520, 300, spot, got, "bend back");

    for (n = 0; n < got; ++n) {

        CHECK(spot[n].side == DR3_GRID_BEHIND, "spot %d was not placed behind the grid", n);
        if (spot[n].x < 60.0f) CHECK(0, "spot %d at %.0f left the road", n, (double)spot[n].x);

        /*
         * The cars follow the bend: each one is turned with the road instead of being clamped to one
         * angle.  The queue here runs around a curve, so the facing changes from spot to spot - a queue
         * whose cars all carry the same angle is the "my car points the wrong way" report.
         */
        if ((n > 0) && (spot[n].lane == spot[n - 1].lane)) {

            double turned = (double)spot[n].dir - (double)spot[n - 1].dir;

            while (turned >  180.0) turned -= 360.0;
            while (turned < -180.0) turned += 360.0;

            CHECK((turned > -90.0) && (turned < 90.0),
                  "spot %d turned %.0f deg from the one before it - even a sharp bend stays under a right angle",
                  n, turned);
        }
    }

    /* the walk has to have reached the part of the road that runs back towards the grid */
    for (n = 0; n < got; ++n) if (spot[n].x > 120.0f) ++in_return;

    CHECK(in_return > 0, "the walk did not follow the bend back (%d of %d spots)", in_return, got);
}
static void test_grid_tight_start(void)
{
    static uint8_t  mask[220 * 160];
    dr3_grid_t      track;
    dr3_grid_car_t  grid[4];
    dr3_grid_spot_t spot[DR3_GRID_CARS];
    int             n, got;

    printf("- chaos grid: a start stretch with no room behind it\n");

    memset(mask, 0x00, sizeof(mask));
    grid_road(mask, 220, 200, 10, 20, 200, 140);     /* a wide road - room for two lanes - but only
                                                        about 120 px of it behind the grid */

    track.mask = mask; track.mask_w = 220; track.mask_h = 200;
    track.rot = 72;                                  /* Direction 270 -> the nose is +x */

    grid[0].x = 150.0f; grid[0].y = 70.0f;
    grid[1].x = 150.0f; grid[1].y = 90.0f;
    grid[2].x = 130.0f; grid[2].y = 70.0f;
    grid[3].x = 130.0f; grid[3].y = 90.0f;

    got = dr3_grid_place(&track, grid, 4, spot, 6);
    /*
     * This stretch carries two cars; the second lane and the road in front of the grid may add more, but
     * what this asserts is the point: nothing is stacked on one spot any more.
     */
    CHECK(got >= 2, "only %d car(s) were placed behind a short start stretch (one lane alone fits 2)", got);

    grid_check_spots(mask, 220, 200, spot, got, "tight start");

    /*
     * Behind the grid first, and the road in front of it once that runs out (dr3_grid.c) - what this
     * test is about is that every car stands on the road with a place of its own, and grid_check_spots()
     * above has just checked exactly that (the body on the road, clear of every other car).
     */
    CHECK((spot[0].side == DR3_GRID_BEHIND) || (spot[0].side == DR3_GRID_AHEAD),
          "spot 0 was placed nowhere");
}

static void test_grid_corner(void)
{
    static uint8_t  mask[300 * 300];
    dr3_grid_t      track;
    dr3_grid_car_t  grid[4];
    dr3_grid_spot_t spot[DR3_GRID_CARS];
    int             n, got, turned = 0;

    printf("- chaos grid: the road turns a corner\n");

    memset(mask, 0x00, sizeof(mask));
    grid_road(mask, 300, 300, 20, 150, 280, 190);    /* the corridor the grid stands on       */
    grid_road(mask, 300, 300, 10, 10, 70, 150);      /* ... and one going up from its left end */

    track.mask = mask; track.mask_w = 300; track.mask_h = 300;
    track.rot = 72;                                  /* the nose is +x */

    grid[0].x = 130.0f; grid[0].y = 155.0f;
    grid[1].x = 130.0f; grid[1].y = 185.0f;
    grid[2].x = 100.0f; grid[2].y = 155.0f;
    grid[3].x = 100.0f; grid[3].y = 185.0f;

    got = dr3_grid_place(&track, grid, 4, spot, 8);
    CHECK(got > 0, "no extra car placed on a road with a corner");

    grid_check_spots(mask, 300, 300, spot, got, "corner");

    for (n = 0; n < got; ++n) if (spot[n].y < 148.0f) ++turned;

    CHECK(turned > 0, "the walk did not follow the road around the corner (%d spots, %d turned)",
          got, turned);

    /*
     * The facing follows the road: on the corridor the grid stands on, a car faces exactly the way
     * the four starters do.  Right at the junction (the wide square where the two corridors meet) both
     * directions are road the walk can follow and the road itself does not say which one is "the" way,
     * so that strip is not asserted - on a real track a grid never sits in the middle of a junction.
     */
    for (n = 0; n < got; ++n) {

        if (spot[n].y > 158.0f) {

            const int img = dr3_grid_img_index(spot[n].dir);

            CHECK(img == track.rot, "spot %d at %.0f,%.0f faces img %d (want %d, the starters)",
                  n, (double)spot[n].x, (double)spot[n].y, img, track.rot);
        }
    }
}

/*
 * The facing of a placed car: Direction = 3.75 * ImgIndex and 96 frames around the circle, so the
 * helper has to round to the nearest frame and wrap - and the engine's own values have to come back
 * unchanged (that is the round trip the placement relies on).
 */
static void test_grid_facing(void)
{
    int i;

    printf("- chaos grid: the facing of a placed car\n");

    CHECK(dr3_grid_img_index(0.0f) == 0, "0 deg came out as %d", dr3_grid_img_index(0.0f));
    CHECK(dr3_grid_img_index(3.75f) == 1, "3.75 deg came out as %d", dr3_grid_img_index(3.75f));
    CHECK(dr3_grid_img_index(270.0f) == 72, "270 deg came out as %d", dr3_grid_img_index(270.0f));
    CHECK(dr3_grid_img_index(358.0f) == 95, "358 deg came out as %d", dr3_grid_img_index(358.0f));
    CHECK(dr3_grid_img_index(359.9f) == 0, "359.9 deg has to wrap to frame 0 (got %d)",
          dr3_grid_img_index(359.9f));
    CHECK(dr3_grid_img_index(-5.0f) == 0, "a negative direction came out as %d",
          dr3_grid_img_index(-5.0f));

    for (i = 0; i < 96; ++i) {

        const float dir = (float)(3.75 * (double)i);

        CHECK(dr3_grid_img_index(dir) == i, "frame %d did not round trip (%d)", i,
              dr3_grid_img_index(dir));
    }
}

static void test_grid_no_room(void)
{
    static uint8_t  mask[100 * 100];
    dr3_grid_t      track;
    dr3_grid_car_t  grid[4];
    dr3_grid_spot_t spot[DR3_GRID_CARS];
    int             n, got;

    printf("- chaos grid: nowhere to put a car\n");

    /* soft ground only - this is the surface the old placement mistook for a parking space */
    memset(mask, 0x00, sizeof(mask));
    track.mask = mask; track.mask_w = 100; track.mask_h = 100; track.rot = 72;

    grid[0].x = 50.0f; grid[0].y = 40.0f;
    grid[1].x = 50.0f; grid[1].y = 60.0f;
    grid[2].x = 35.0f; grid[2].y = 40.0f;
    grid[3].x = 35.0f; grid[3].y = 60.0f;

    got = dr3_grid_place(&track, grid, 4, spot, 4);
    CHECK(got == 0, "a track without road returned %d spots (the old placement put cars on soft ground)", got);

    /* a corridor that is too narrow for a car body */
    memset(mask, 0x00, sizeof(mask));
    grid_road(mask, 100, 100, 0, 49, 100, 55);       /* 6 px of road, the body needs 26 */

    got = dr3_grid_place(&track, grid, 4, spot, 4);
    CHECK(got == 0, "%d cars were placed in a corridor that is too narrow", got);

    /* and the arguments that make no sense at all */
    CHECK(dr3_grid_place(NULL, grid, 4, spot, 4) == 0, "a NULL track was accepted");

    track.mask = NULL;
    CHECK(dr3_grid_place(&track, grid, 4, spot, 4) == 0, "a NULL mask was accepted");

    track.mask = mask;
    CHECK(dr3_grid_place(&track, grid, 0, spot, 4) == 0, "an empty grid was accepted");
    CHECK(dr3_grid_place(&track, grid, 4, spot, 0) == 0, "max_cars 0 was accepted");
}

static void test_grid_off_road_slot(void)
{
    static uint8_t  mask[200 * 100];
    dr3_grid_t      track;
    dr3_grid_car_t  grid[4];
    dr3_grid_spot_t spot[DR3_GRID_CARS];
    int             got;

    printf("- chaos grid: a start slot a little off the road\n");

    memset(mask, 0x00, sizeof(mask));
    grid_road(mask, 200, 100, 10, 20, 190, 80);

    track.mask = mask; track.mask_w = 200; track.mask_h = 100;
    track.rot = 72;

    /* the four slots of this track sit on the soft ground next to the road */
    grid[0].x = 100.0f; grid[0].y = 16.0f;
    grid[1].x = 110.0f; grid[1].y = 16.0f;
    grid[2].x = 100.0f; grid[2].y = 84.0f;
    grid[3].x = 110.0f; grid[3].y = 84.0f;

    got = dr3_grid_place(&track, grid, 4, spot, 4);
    CHECK(got > 0, "a slot a few px off the road broke the placement");

    grid_check_spots(mask, 200, 100, spot, got, "off-road slot");
}

static void test_grid_standings_order(void)
{
    const int rank_a[5] = { 5, 1, 3, 2, 4 };      /* one of every position */
    const int rank_b[4] = { 0, 2, 0, 1 };         /* two cars without a rank yet */
    const int rank_c[4] = { 2, 2, 1, 1 };         /* ties stay in the order they came in */
    const int rank_d[4] = { 0, 0, 0, 0 };         /* nothing known yet - the old order */
    static const int want_a[5] = { 1, 3, 2, 4, 0 };
    static const int want_b[4] = { 3, 1, 0, 2 };
    static const int want_c[4] = { 2, 3, 0, 1 };
    static const int want_d[4] = { 0, 1, 2, 3 };
    int              order[DR3_GRID_CARS];
    int              n;

    printf("- chaos grid: the order the standings give\n");

    dr3_grid_order_by_rank(rank_a, 5, order);
    for (n = 0; n < 5; ++n)
        CHECK(order[n] == want_a[n], "order %d is car %d (want %d)", n, order[n], want_a[n]);

    dr3_grid_order_by_rank(rank_b, 4, order);
    for (n = 0; n < 4; ++n)
        CHECK(order[n] == want_b[n], "cars without a rank: order %d is car %d (want %d)",
              n, order[n], want_b[n]);

    dr3_grid_order_by_rank(rank_c, 4, order);
    for (n = 0; n < 4; ++n)
        CHECK(order[n] == want_c[n], "ties: order %d is car %d (want %d)", n, order[n], want_c[n]);

    dr3_grid_order_by_rank(rank_d, 4, order);
    for (n = 0; n < 4; ++n)
        CHECK(order[n] == want_d[n], "no ranks at all: order %d is car %d (want %d)",
              n, order[n], want_d[n]);

    dr3_grid_order_by_rank(NULL, 4, order);
    dr3_grid_order_by_rank(rank_a, 0, order);
}

static void test_carcolour(void)
{
    dr3_palette_t pal;
    dr3_lut32_t   lut;
    uint32_t      shades[2 * 10];
    uint32_t      dst[4 * 6];
    uint8_t       src[4 * 6];
    uint8_t       mask[4 * 6];
    int           i, n;

    printf("- car colours (the chaos build's twenty cars)\n");

    dr3_palette_reset(&pal);
    dr3_lut32_build(&lut, &pal, DR3_LUT_RGB, 0xFF);   /* memory order R,G,B,A -> r in the low byte */

    /* the ramp of ___4a064h_cdecl(): five darkened shades of the colour, then five lightened ones */
    dr3_carcolour_build(&shades[0], 200, 100, 50, &lut);
    dr3_carcolour_build(&shades[10], 50, 200, 100, &lut);

    CHECK((shades[0] & 0xffu) == 20, "shade 0 red is %u (want 20 = 0.1 * 200)", shades[0] & 0xffu);
    CHECK(((shades[4] >> 8) & 0xffu) == 82, "shade 4 green is %u (want 82 = 0.82 * 100)",
          (shades[4] >> 8) & 0xffu);
    CHECK((shades[5] & 0xffu) == 200, "shade 5 has to be the colour itself (got %u)", shades[5] & 0xffu);
    CHECK(((shades[9] >> 16) & 0xffu) == 60, "shade 9 blue is %u (want 60 = 50.4 + 0.2 * 50)",
          (shades[9] >> 16) & 0xffu);
    CHECK((shades[9] >> 24) == 0xFFu, "the alpha byte is not opaque: %08X", shades[9]);

    /*
     * The engine's own space: the colour arrives as a 6-bit VGA DAC value (carcol.pal, s_54 +0x34),
     * the ramp is built there and only then expanded - that is what keeps the light half of a ramp
     * bright, because its "12.6 * n" saturates a 6-bit entry at 63 = white.  Built from an already
     * expanded colour the same term stays tiny and the shine of the car comes out grey.
     */
    dr3_carcolour_build_vga6(&shades[0], 40, 14, 8, &lut);

    CHECK((shades[5] & 0xffu) == (uint32_t)((40 << 2) | (40 >> 4)),
          "the base of the vga6 ramp is %u (want %u)", shades[5] & 0xffu, (unsigned)((40 << 2) | (40 >> 4)));
    CHECK((shades[9] & 0xffu) > 200, "the shine is %u, it has to stay a bright near-white",
          shades[9] & 0xffu);
    CHECK((shades[9] & 0xffu) > (shades[5] & 0xffu) + 60,
          "the shine (%u) is not clearly brighter than the colour (%u)", shades[9] & 0xffu,
          shades[5] & 0xffu);

    dr3_carcolour_build_vga6(&shades[0], 63, 63, 63, &lut);
    CHECK((shades[9] & 0xffffffu) == 0xffffffu, "the shine of a saturated colour is %06X (want FFFFFF)",
          shades[9] & 0xffffffu);

    /*
     * The tint colours the pixels the engine marked - nothing else.  The mark holds the car index + 1,
     * and only values in the palette's car ramp are touched.
     */
    for (i = 0; i < 16; ++i) { src[i] = 0x30; dst[i] = 0xDEADBEEFu; mask[i] = 0; }
    src[0] = 0x0f;  mask[0] = 1;        /* car 0, the first shade      */
    src[4] = 0x18;  mask[4] = 2;        /* car 1, the last shade       */
    src[2] = 0x19;  mask[2] = 1;        /* marked, but just off the ramp */
    src[1] = 0x0f;  mask[1] = 0;        /* on the ramp, but not marked    */

    n = dr3_carcolour_tint_mask(dst, 4, 4, 1, 4, src, 4, 4, 4, mask, 4, 4, 4, 0, NULL, shades, 2);
    CHECK(n == 2, "tint wrote %d pixels (want 2)", n);
    CHECK(dst[0] == shades[0], "car 0's first shade is %08X (want %08X)", dst[0], shades[0]);
    CHECK(dst[4] == shades[10 + 9], "car 1's last shade is %08X (want %08X)", dst[4], shades[10 + 9]);
    CHECK(dst[2] == 0xDEADBEEFu, "a marked pixel outside the ramp was coloured: %08X", dst[2]);
    CHECK(dst[1] == 0xDEADBEEFu, "an unmarked ramp pixel was coloured: %08X", dst[1]);
    CHECK(dst[8] == 0xDEADBEEFu, "a track pixel was coloured: %08X", dst[8]);

    /* a mark of a car the caller did not hand in is ignored */
    for (i = 0; i < 16; ++i) dst[i] = 0xDEADBEEFu;
    mask[5] = 3;                                    /* a third car, but only two were handed in */
    src[5] = 0x0f;
    n = dr3_carcolour_tint_mask(dst, 4, 4, 1, 4, src, 4, 4, 4, mask, 4, 4, 4, 0, NULL, shades, 2);
    CHECK(n == 2, "a mark beyond the cars handed in coloured %d pixel(s) (want 2, not 3)", n);
    CHECK(dst[5] == 0xDEADBEEFu, "the mark of a car that was not handed in was coloured");

    /* the rotated target: x stride = the row length, y stride = -1 (the 3DS framebuffer) */
    for (i = 0; i < 16; ++i) dst[i] = 0xDEADBEEFu;
    n = dr3_carcolour_tint_mask(dst + 3, 4, 4, 4, -1, src, 4, 4, 4, mask, 4, 4, 4, 0, NULL, shades, 2);
    CHECK(n == 2, "the rotated target coloured %d pixel(s) (want 2)", n);
    CHECK(dst[3] == shades[0], "rotated (0,0) -> dst[3] is %08X (want %08X)", dst[3], shades[0]);
    CHECK(dst[2] == shades[10 + 9], "rotated (0,1) -> dst[2] is %08X (want %08X)", dst[2], shades[10 + 9]);

    /*
     * The presented buffer can be taller than the game's screen: VGA13 is 320x200 inside a 320x240
     * surface and starts 20 rows down, while the mask covers the game's rows only.  A mark for the
     * game's row r therefore belongs to the presented row r + mask_row_off - reading it without that
     * offset is what put the colours 20 rows above the cars.
     */
    for (i = 0; i < 24; ++i) { src[i] = 0x30; dst[i] = 0xDEADBEEFu; mask[i] = 0; }
    src[(1 + 2) * 4 + 1] = 0x0f;  mask[1 * 4 + 1] = 1;     /* game row 1 -> presented row 3 */
    src[(2 + 2) * 4 + 2] = 0x18;  mask[2 * 4 + 2] = 2;     /* game row 2 -> presented row 4 */

    n = dr3_carcolour_tint_mask(dst, 4, 6, 1, 4, src, 4, 6, 4, mask, 4, 4, 4, 2, NULL, shades, 2);
    CHECK(n == 2, "with the row offset %d pixel(s) were coloured (want 2)", n);
    CHECK(dst[3 * 4 + 1] == shades[0], "with the offset, presented row 3 is %08X (want %08X)",
          dst[3 * 4 + 1], shades[0]);
    CHECK(dst[4 * 4 + 2] == shades[10 + 9], "with the offset, presented row 4 is %08X (want %08X)",
          dst[4 * 4 + 2], shades[10 + 9]);
    CHECK(dst[1 * 4 + 1] == 0xDEADBEEFu, "the mark was read without the offset (row 1 was written)");
    CHECK(dst[2 * 4 + 2] == 0xDEADBEEFu, "the mark was read without the offset (row 2 was written)");

    /* a mark whose game row would be above the presented screen is not read at all */
    for (i = 0; i < 24; ++i) dst[i] = 0xDEADBEEFu;
    n = dr3_carcolour_tint_mask(dst, 4, 6, 1, 4, src, 4, 6, 4, mask, 4, 4, 4, 8, NULL, shades, 2);
    CHECK(n == 0, "an offset past the mask coloured %d pixel(s) (want 0)", n);

    /*
     * The area the race hands in: the maps the blit already computed, and the box the marks of the
     * frame live in.  The maps have to give the same pixels as the division does, and the box has to
     * keep the pass off the rest of the frame - walking all 96.000 target pixels for a handful of cars
     * cost 27 ms of a 52 ms frame, more than everything else the port does.
     */
    {
        const uint16_t       map_x[4] = { 0, 1, 2, 3 }, map_y[4] = { 0, 1, 2, 3 };
        dr3_carcolour_area_t area;

        for (i = 0; i < 24; ++i) { src[i] = 0x30; dst[i] = 0xDEADBEEFu; mask[i] = 0; }
        src[0] = 0x0f;               mask[0] = 1;               /* (0,0) */
        src[1 * 4 + 1] = 0x0f;       mask[1 * 4 + 1] = 1;       /* (1,1) */

        area.map_x = map_x;
        area.map_y = map_y;
        area.x0    = 1; area.y0 = 1; area.x1 = 3; area.y1 = 3;

        n = dr3_carcolour_tint_mask(dst, 4, 4, 1, 4, src, 4, 4, 4, mask, 4, 4, 4, 0, &area, shades, 2);
        CHECK(n == 1, "the area coloured %d pixel(s) (want 1 - only the mark inside the box)", n);
        CHECK(dst[1 * 4 + 1] == shades[0], "the mark inside the box is %08X (want %08X)",
              dst[1 * 4 + 1], shades[0]);
        CHECK(dst[0] == 0xDEADBEEFu, "a mark outside the box was coloured: %08X", dst[0]);

        area.x1 = -1;               /* no mark at all in this frame */
        n = dr3_carcolour_tint_mask(dst, 4, 4, 1, 4, src, 4, 4, 4, mask, 4, 4, 4, 0, &area, shades, 2);
        CHECK(n == 0, "an empty area still coloured %d pixel(s)", n);
    }

    /* nonsense arguments are refused */
    CHECK(dr3_carcolour_tint_mask(NULL, 4, 4, 1, 4, src, 4, 4, 4, mask, 4, 4, 4, 0, NULL, shades, 2) == 0,
          "a NULL target was accepted");
    CHECK(dr3_carcolour_tint_mask(dst, 4, 4, 1, 4, src, 4, 4, 4, NULL, 4, 4, 4, 0, NULL, shades, 2) == 0,
          "a NULL mask was accepted");
    CHECK(dr3_carcolour_tint_mask(dst, 0, 4, 1, 4, src, 4, 4, 4, mask, 4, 4, 4, 0, NULL, shades, 2) == 0,
          "an empty target was accepted");
    CHECK(dr3_carcolour_tint_mask(dst, 4, 4, 1, 4, src, 4, 4, 4, mask, 4, 4, 4, 0, NULL, shades, 0) == 0,
          "no cars was accepted");
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

/*
 * The rank money ladder of a chaos race (platform_3ds/dr3_payout.c, the numbers ___33010h.c pays with).
 * The table is the ladder the player asked for: $100 for every place above the last one, a jump at the
 * podium and nothing at all for the last place.
 */
static void test_payout(void)
{
    static const int want20[20] = {
        5000, 4000, 3000, 1600, 1500, 1400, 1300, 1200, 1100, 1000,
         900,  800,  700,  600,  500,  400,  300,  200,  100,    0
    };
    static const int want5[5] = { 5000, 4000, 3000, 100, 0 };
    static const int want4[4] = { 5000, 4000, 3000, 0 };
    static const int want3[3] = { 5000, 4000, 0 };
    static const int want2[2] = { 5000, 0 };
    int              place;

    printf("- the rank money of a chaos race\n");

    for (place = 1; place <= 20; ++place)
        CHECK(dr3_chaos_rank_money(place, 20) == want20[place - 1],
              "20 cars, place %d: got %d want %d", place,
              dr3_chaos_rank_money(place, 20), want20[place - 1]);

    for (place = 1; place <= 5; ++place)
        CHECK(dr3_chaos_rank_money(place, 5) == want5[place - 1],
              "5 cars, place %d: got %d want %d", place,
              dr3_chaos_rank_money(place, 5), want5[place - 1]);

    for (place = 1; place <= 4; ++place)
        CHECK(dr3_chaos_rank_money(place, 4) == want4[place - 1],
              "4 cars, place %d: got %d want %d", place,
              dr3_chaos_rank_money(place, 4), want4[place - 1]);

    for (place = 1; place <= 3; ++place)
        CHECK(dr3_chaos_rank_money(place, 3) == want3[place - 1],
              "3 cars, place %d: got %d want %d", place,
              dr3_chaos_rank_money(place, 3), want3[place - 1]);

    for (place = 1; place <= 2; ++place)
        CHECK(dr3_chaos_rank_money(place, 2) == want2[place - 1],
              "2 cars, place %d: got %d want %d", place,
              dr3_chaos_rank_money(place, 2), want2[place - 1]);

    /* a place the race never had - a wreck before the field was decided is place 0 */
    CHECK(dr3_chaos_rank_money(0, 20) == 0,   "place 0 pays nothing");
    CHECK(dr3_chaos_rank_money(-3, 20) == 0,  "a negative place pays nothing");
    CHECK(dr3_chaos_rank_money(21, 20) == 0,  "a place beyond the field pays nothing");
    CHECK(dr3_chaos_rank_money(1, 0) == 0,    "a race without cars pays nothing");
    CHECK(dr3_chaos_rank_money(1, 1) == 0,    "a one car race: its only place is the last one");

    /* the ladder never crosses itself: a better place never pays less than the one below it */
    for (place = 2; place <= 20; ++place)
        CHECK(dr3_chaos_rank_money(place - 1, 20) >= dr3_chaos_rank_money(place, 20),
              "place %d pays less than place %d", place - 1, place);
}

/*
 * The health line the tint pass draws over a car (platform_3ds/dr3_blit.c, dr3_carcolour_hp_line_shown).
 * It is only drawn while a car is damaged: below DR3_CHAOS_HP_LINE_BELOW percent of the HP the race set it
 * up with *and* above 0 HP.  A whole car gets no line - it would be the full length bar of a car nobody has
 * touched - and a wreck gets none either (the game draws a burning wreck for that).
 *
 * The numbers are the game's own: a race starts a car with 102400 HP (0x400 * (100 - damage), 100 = whole)
 * and the harness' "hp <car> <value>" starts one with less.
 */
static void test_hp_line(void)
{
    const int full = 102400;                                   /* 100% - a whole car   */
    const int at   = full / 100 * DR3_CHAOS_HP_LINE_BELOW;      /* the threshold itself */
    const int half = 51200;
    const int half_at = half / 100 * DR3_CHAOS_HP_LINE_BELOW;

    printf("- the health line over a car (the chaos build's HP bar)\n");

    /* a whole car, and a car exactly at the threshold: no line */
    CHECK(dr3_carcolour_hp_line_shown(full, full, 0) == 0, "a whole car got a line");
    CHECK(dr3_carcolour_hp_line_shown(at, full, 0) == 0, "%d%% (the threshold itself) got a line",
          DR3_CHAOS_HP_LINE_BELOW);

    /* one HP below it the line is there, down to the last HP */
    CHECK(dr3_carcolour_hp_line_shown(at - 1, full, 0) == 1, "one HP below the threshold got no line");
    CHECK(dr3_carcolour_hp_line_shown(half, full, 0) == 1, "half the HP got no line");
    CHECK(dr3_carcolour_hp_line_shown(1, full, 0) == 1, "the last HP got no line");

    /* a wreck, and a car the port cannot scale at all: no line */
    CHECK(dr3_carcolour_hp_line_shown(0, full, 0) == 0, "a wreck got a line");
    CHECK(dr3_carcolour_hp_line_shown(0, 0, 0) == 0, "a car without any HP got a line");
    CHECK(dr3_carcolour_hp_line_shown(5000, 0, 0) == 0, "a car with no scale at all got a line");

    /* the HP the race started him with is the scale: a car that starts at half is whole at half */
    CHECK(dr3_carcolour_hp_line_shown(half, half, 0) == 0, "a car that is whole at half HP got a line");
    CHECK(dr3_carcolour_hp_line_shown(half_at, half, 0) == 0, "half HP at the threshold got a line");
    CHECK(dr3_carcolour_hp_line_shown(half_at - 1, half, 0) == 1, "half HP below the threshold got no line");

    /* a pickup can push a car over the HP it started with - that is not "damaged" either */
    CHECK(dr3_carcolour_hp_line_shown(2 * full, full, 0) == 0, "a car above its starting HP got a line");

    /*
     * The player's own car never gets a line, whatever its HP is: the bar is there to read the *other*
     * cars.  The very same HP values do get a line for an opponent, so it is the flag that decides.
     */
    CHECK(dr3_carcolour_hp_line_shown(half, full, 1) == 0, "the player's own damaged car got a line");
    CHECK(dr3_carcolour_hp_line_shown(1, full, 1) == 0, "the player's own car at its last HP got a line");
    CHECK(dr3_carcolour_hp_line_shown(0, full, 1) == 0, "the player's own wreck got a line");
    CHECK(dr3_carcolour_hp_line_shown(full, full, 1) == 0, "the player's own whole car got a line");
    CHECK(dr3_carcolour_hp_line_shown(half, full, 0) != dr3_carcolour_hp_line_shown(half, full, 1),
          "the own car and an opponent with the same HP are not told apart");

    /* the threshold has to stay a usable percentage - the check that catches a retuned constant */
    CHECK((DR3_CHAOS_HP_LINE_BELOW > 0) && (DR3_CHAOS_HP_LINE_BELOW <= 100),
          "DR3_CHAOS_HP_LINE_BELOW is %d, it has to be a percentage (1..100)", DR3_CHAOS_HP_LINE_BELOW);
}

/*
 * The endgame of a deathmatch (platform_3ds/dr3_dm.h): the sudden death of "Destroy 'em all".  The race
 * clock is getCounter(1) - 70 ticks a second, started with the race - so everything here is written in
 * milliseconds of that clock, and the deadline is the switch (normally DR3_DM_SUDDEN_AT_S = 3 minutes).
 */
static void test_dm(void)
{
    const int at = DR3_DM_SUDDEN_AT_S;
    const unsigned long long at_ms = (unsigned long long)at * 1000ull;

    printf("- the sudden death of a deathmatch (the deadline, the drain, the banner)\n");

    /* the race clock: whole seconds, and the unit the tuning is written in */
    CHECK(dr3_dm_ticks_ms(0) == 0, "tick 0 is not the start of the race");
    CHECK(dr3_dm_ticks_ms(70) == 1000, "70 ticks are not a second");
    CHECK(dr3_dm_ticks_ms(700) == 10000, "700 ticks are not ten seconds");
    CHECK(dr3_dm_ticks_s(69) == 0, "69 ticks are not a whole second");
    CHECK(dr3_dm_ticks_s(70) == 1, "70 ticks are not one second");
    CHECK(dr3_dm_ticks_s(140) == 2, "140 ticks are not two seconds");

    /* before the deadline nothing happens at all */
    CHECK(dr3_dm_sudden_active(0, at) == 0, "the sudden death is active at the start of the race");
    CHECK(dr3_dm_sudden_active(at_ms - 1, at) == 0, "the sudden death is active one ms too early");
    CHECK(dr3_dm_sudden_active(at_ms, at) == 0, "the sudden death is active at the deadline itself");
    CHECK(dr3_dm_sudden_active(at_ms + 1, at) == 1, "the sudden death is not active after the deadline");
    CHECK(dr3_dm_sudden_percent(at_ms, at) == 0, "the deadline itself took HP");
    CHECK(dr3_dm_sudden_percent(at_ms + 999, at) == 0, "less than a second of sudden death took HP");

    /* one percent per whole second after it - and it keeps counting, the HP clamps at nothing */
    CHECK(dr3_dm_sudden_percent(at_ms + 1000, at) == 1, "one second of sudden death did not take 1 percent");
    CHECK(dr3_dm_sudden_percent(at_ms + 12500, at) == 12, "twelve seconds did not take 12 percent");
    CHECK(dr3_dm_sudden_percent(at_ms + 100 * 1000ull, at) == 100, "100 seconds did not take 100 percent");
    CHECK(dr3_dm_sudden_percent(at_ms + 150 * 1000ull, at) > 100, "a race far past the drain stops counting");

    /* what is still owed: what the deadline asks for minus what has been taken, never negative */
    CHECK(dr3_dm_sudden_due(at_ms + 1000, at, 0) == 1, "the first percent was not owed");
    CHECK(dr3_dm_sudden_due(at_ms + 1000, at, 1) == 0, "the first percent was owed twice");
    CHECK(dr3_dm_sudden_due(at_ms + 5500, at, 1) == 4, "five seconds did not owe four more percent");
    CHECK(dr3_dm_sudden_due(at_ms + 3000, at, -7) == 3, "a negative count of taken percent was not clamped");
    CHECK(dr3_dm_sudden_due(at_ms, at, 0) == 0, "the deadline itself owed percent");
    CHECK(dr3_dm_sudden_due(at_ms + 120000, 0, 5) == 0, "a switched off deadline still owes percent");

    /* the HP of a percent - the game's own unit (race___400ech.c: 100 - HP/1024) */
    CHECK(DR3_DM_HP_PER_PERCENT == 1024, "one percent of HP is not the engine's own 1024");
    CHECK(dr3_dm_hp_for_percent(1) == 1024, "one percent is not 1024 HP");
    CHECK(dr3_dm_hp_for_percent(50) == 51200, "half the HP is not 51200");
    CHECK(dr3_dm_hp_for_percent(100) == 102400, "the whole bar is not 102400");
    CHECK(dr3_dm_hp_for_percent(0) == 0, "no percent took HP");
    CHECK(dr3_dm_hp_for_percent(-3) == 0, "a negative percent took HP");

    /* the banner: up from the start of the sudden death for its hold, then blinking */
    CHECK(dr3_dm_banner_visible(at_ms - 1, at) == 0, "the banner is up before the sudden death starts");
    CHECK(dr3_dm_banner_visible(at_ms, at) == 0, "the banner is up at the deadline itself, before it starts");
    CHECK(dr3_dm_banner_visible(at_ms + 1, at) == 1, "the banner is not up once the sudden death runs");
    CHECK(dr3_dm_banner_visible(at_ms + DR3_DM_BANNER_HOLD_MS - 1, at) == 1,
          "the banner went away inside its hold");

    {
        unsigned long long ms   = at_ms + DR3_DM_BANNER_HOLD_MS;
        int                dark = 0, lit = 0, k;

        for (k = 0; k < 100; ++k, ms += DR3_DM_BANNER_BLINK_MS / 10) {

            if (dr3_dm_banner_visible(ms, at)) ++lit;
            else                               ++dark;
        }

        CHECK(dark > 0, "the banner does not blink after its hold");
        CHECK(lit > dark, "the banner is dark more often than it is up");
    }

    CHECK(dr3_dm_banner_visible(at_ms, 0) == 0, "a switched off deadline still shows the banner");

    /* the banner's text and the game's font: 6x6 glyphs, the characters from 0x20 */
    CHECK(dr3_dm_glyph_index(' ') == 0, "the space is not the first glyph of the font");
    CHECK(dr3_dm_glyph_index('A') == ('A' - 0x20), "the glyph of 'A' is not the one the font holds");
    CHECK(dr3_dm_glyph_index((char)0x1f) == -1, "a character below the font's first one has a glyph");
    CHECK(dr3_dm_glyph_index((char)(0x20 + DR3_DM_GLYPHS)) == -1, "a character past the last glyph has one");
    CHECK(dr3_dm_text_width("", DR3_DM_BANNER_SCALE) == 0, "an empty text is not zero pixels wide");
    CHECK(dr3_dm_text_width("A", 1) == 6, "one letter at scale 1 is not 6 pixels");
    CHECK(dr3_dm_text_width("A", 2) == 12, "one letter at scale 2 is not 12 pixels");
    CHECK(dr3_dm_text_width("AB", 1) == 13, "two letters at scale 1 are not 13 pixels (a blank column between)");
    CHECK(dr3_dm_text_width(DR3_DM_BANNER_LINE1, DR3_DM_BANNER_SCALE) > 0, "the first banner line has no width");
    CHECK(dr3_dm_text_width(DR3_DM_BANNER_LINE2, DR3_DM_BANNER_SCALE) > 0, "the second banner line has no width");
    /*
     * Both lines have to fit between the two HUD columns of the race screen (the viewport is 64..256 of the
     * 320 pixels wide screen) - that is why the banner is two lines at all (dr3_dm.h).
     */
    CHECK(dr3_dm_text_width(DR3_DM_BANNER_LINE1, DR3_DM_BANNER_SCALE) <= 256,
          "the first banner line reaches into the left or right HUD column");
    CHECK(dr3_dm_text_width(DR3_DM_BANNER_LINE2, DR3_DM_BANNER_SCALE) <= 256,
          "the second banner line reaches into the left or right HUD column");

    /* the tuning itself: three minutes, one percent a second, a banner that can be drawn and blink */
    CHECK(DR3_DM_SUDDEN_AT_S == 180, "the sudden death does not start after three minutes");
    CHECK(DR3_DM_SUDDEN_PERCENT_S == 1, "the sudden death does not take one percent a second");
    CHECK((DR3_DM_TICKS_PER_S > 0) && (DR3_DM_BANNER_SCALE > 0) && (DR3_DM_BANNER_BLINK_MS > 0) &&
          (DR3_DM_BANNER_HOLD_MS > 0) && (DR3_DM_GLYPH_BYTES >= DR3_DM_GLYPH_W * DR3_DM_GLYPH_H),
          "the tuning has a value that would divide by zero or draw nothing");

    /*
     * The lap duty - the answer to camping (dr3_dm.h): every minute of race time a car has to have finished
     * one lap, and a car that has not loses 25 percent of its HP at the check.
     */
    printf("- the lap duty of a deathmatch (against camping in a corner)\n");

    CHECK(DR3_DM_LAP_PER_INTERVAL == 1, "the lap duty does not ask for one lap");
    CHECK(DR3_DM_LAP_INTERVAL_S == 60, "the lap duty does not check every minute");
    CHECK(DR3_DM_LAP_PENALTY_PERCENT == 25, "the lap duty does not take 25 percent");
    CHECK(dr3_dm_hp_for_percent(DR3_DM_LAP_PENALTY_PERCENT) == 25600, "25 percent are not 25600 HP");
    CHECK((DR3_DM_LAP_PENALTY_PERCENT * 4) == 100, "four missed checks do not wreck a car");

    /* when a check is due: one every interval, none while the switch is off */
    CHECK(dr3_dm_lap_checks(0, 60) == 0, "a check is due at the start of the race");
    CHECK(dr3_dm_lap_checks(59999ull, 60) == 0, "a check is due before the first interval is over");
    CHECK(dr3_dm_lap_checks(60000ull, 60) == 1, "the first check is not due after a minute");
    CHECK(dr3_dm_lap_checks(120000ull, 60) == 2, "the second check is not due after two minutes");
    CHECK(dr3_dm_lap_checks(5000ull, 5) == 1, "a five second interval does not check after five seconds");
    CHECK(dr3_dm_lap_checks(1000ull * 24 * 3600, 60) == 1440, "a day of race time is not 1440 checks");
    CHECK(dr3_dm_lap_checks(1000ull * 60 * 60, 0) == 0, "a switched off lap duty still checks");
    CHECK(dr3_dm_lap_checks(0, 0) == 0, "a switched off lap duty checks at the start");

    /* who owes a penalty: whoever did not finish the lap(s) the interval asks for */
    CHECK(dr3_dm_lap_duty_missed(0, 0) == 1, "a car that never moved does not owe a penalty");
    CHECK(dr3_dm_lap_duty_missed(1, 0) == 0, "a car that finished a lap owes a penalty");
    CHECK(dr3_dm_lap_duty_missed(1, 1) == 1, "a car that stood still after a lap does not owe one");
    CHECK(dr3_dm_lap_duty_missed(3, 1) == 0, "two laps in one interval owe a penalty");
    CHECK(dr3_dm_lap_duty_missed(2, 3) == 1, "a counter that ran backwards did not clamp");
    CHECK(dr3_dm_lap_duty_missed(0, -5) == 1, "a negative count of laps at the check did not clamp");
    CHECK(dr3_dm_lap_duty_missed(-2, 0) == 1, "a negative lap count does not clamp to nothing");

    /*
     * Street Last Car Standing (DR3_CHAOS_MODE_DM) loot mix & goal-aware combat helpers
     */
    printf("- street deathmatch loot mix (mines, ammo, money) & combat AI decisions\n");
    {
        int count[11], r, k;

        for (r = 0; r < 11; ++r) count[r] = 0;
        for (r = 0; r < 100; ++r) {
            k = dr3_dm_item_for_roll(r);
            CHECK((k == DR3_ITEM_AMMO) || (k == DR3_ITEM_TURBO) || (k == DR3_ITEM_MONEY) ||
                  (k == DR3_ITEM_MONEY_BIG) || (k == DR3_ITEM_REPAIR_SMALL) ||
                  (k == DR3_ITEM_REPAIR_BIG) || (k == DR3_ITEM_MINE_CRATE),
                  "dm roll %d gave unexpected item %d", r, k);
            if ((k >= 1) && (k <= 10)) ++count[k];
        }

        CHECK(count[DR3_ITEM_AMMO] == 30,         "dm ammo: %d of 100", count[DR3_ITEM_AMMO]);
        CHECK(count[DR3_ITEM_MINE_CRATE] == 20,   "dm mine crates: %d of 100", count[DR3_ITEM_MINE_CRATE]);
        CHECK(count[DR3_ITEM_MONEY] == 14,        "dm money: %d of 100", count[DR3_ITEM_MONEY]);
        CHECK(count[DR3_ITEM_MONEY_BIG] == 6,     "dm big money: %d of 100", count[DR3_ITEM_MONEY_BIG]);
        CHECK(count[DR3_ITEM_TURBO] == 12,        "dm turbo: %d of 100", count[DR3_ITEM_TURBO]);
        CHECK(count[DR3_ITEM_REPAIR_SMALL] == 8,  "dm small repair: %d of 100", count[DR3_ITEM_REPAIR_SMALL]);
        CHECK(count[DR3_ITEM_REPAIR_BIG] == 10,   "dm big repair: %d of 100", count[DR3_ITEM_REPAIR_BIG]);
        CHECK(dr3_dm_item_for_roll(-10) == dr3_dm_item_for_roll(0), "dm roll clamps low");
        CHECK(dr3_dm_item_for_roll(999) == dr3_dm_item_for_roll(99), "dm roll clamps high");

        /* target scoring: wounded prey prioritized over full-HP prey; out-of-range/behind ignored */
        {
            const int s_full = dr3_dm_target_score(100.0f, DR3_DM_HP_MAX, DR3_DM_HP_MAX, 10.0f);
            const int s_low  = dr3_dm_target_score(100.0f, DR3_DM_HP_MAX / 5, DR3_DM_HP_MAX, 10.0f);
            const int s_far  = dr3_dm_target_score(200.0f, DR3_DM_HP_MAX, DR3_DM_HP_MAX, 10.0f);

            CHECK(s_full > 0, "valid forward target gets positive score");
            CHECK(s_low > s_full, "low-HP target (%d) must outrank full-HP target (%d)", s_low, s_full);
            CHECK(s_full > s_far, "closer target (%d) must outrank distant target (%d)", s_full, s_far);
            CHECK(dr3_dm_target_score(300.0f, DR3_DM_HP_MAX, DR3_DM_HP_MAX, 0.0f) == 0, "beyond range ignored");
            CHECK(dr3_dm_target_score(120.0f, DR3_DM_HP_MAX, DR3_DM_HP_MAX, 120.0f) == 0, "behind ignored");
            CHECK(dr3_dm_target_score(80.0f, 0, DR3_DM_HP_MAX, 0.0f) == 0, "wreck ignored");
        }

        /* pickup want ranges: low ammo/HP/mines drive longer detours; full HP skips repair */
        CHECK(dr3_dm_pickup_want_range(DR3_ITEM_AMMO, 0, DR3_DM_HP_MAX, DR3_DM_HP_MAX, 3, DR3_DM_AMMO_MAX) >= 200,
              "empty ammo seeks ammo crate from afar");
        CHECK(dr3_dm_pickup_want_range(DR3_ITEM_MINE_CRATE, DR3_DM_AMMO_MAX, DR3_DM_HP_MAX, DR3_DM_HP_MAX, 0, DR3_DM_AMMO_MAX) >= 200,
              "0 mines seeks street mine crate");
        CHECK(dr3_dm_pickup_want_range(DR3_ITEM_REPAIR_BIG, DR3_DM_AMMO_MAX, DR3_DM_HP_MAX, DR3_DM_HP_MAX, 3, DR3_DM_AMMO_MAX) == 0,
              "full HP ignores repair");
        CHECK(dr3_dm_pickup_want_range(DR3_ITEM_REPAIR_BIG, DR3_DM_AMMO_MAX, DR3_DM_HP_MAX / 4, DR3_DM_HP_MAX, 3, DR3_DM_AMMO_MAX) >= 200,
              "damaged car seeks big repair");
        CHECK(dr3_dm_pickup_want_range(DR3_ITEM_MONEY_BIG, DR3_DM_AMMO_MAX, DR3_DM_HP_MAX, DR3_DM_HP_MAX, 3, DR3_DM_AMMO_MAX) >
              dr3_dm_pickup_want_range(DR3_ITEM_MONEY, DR3_DM_AMMO_MAX, DR3_DM_HP_MAX, DR3_DM_HP_MAX, 3, DR3_DM_AMMO_MAX),
              "big cash has greater pull than small cash");
    }
}

/*
 * The four rows of a results panel (platform_3ds/dr3_results.c, dr3_results.h).
 *
 * The panel draws one racer per row - the name, the portrait, the car picture and the rank, with the row as
 * the pitch of the picture - so every row has to be a racer of the race and no racer may own two of them.
 * Both went wrong on the 3DS: the "MEDIUM RACE RESULTS" panel of a chaos race showed "JANE HONDA" in three
 * of its four rows, while the race only gave him place 1.
 */
static int results_rows_sane(const int *rows, int racers)
{
    int a, b;

    for (a = 0; a < DR3_RESULTS_ROWS; ++a) {

        if ((rows[a] < 0) || (rows[a] >= racers)) return 0;

        for (b = a + 1; b < DR3_RESULTS_ROWS; ++b) if (rows[a] == rows[b]) return 0;
    }

    return 1;
}

static void test_results_rows(void)
{
    int seat[20], place[20], rows[DR3_RESULTS_ROWS];
    int n;

    printf("- results panel rows\n");

    /* a four car race: the field is the entry list and every car of it finishes - the row is its place */
    for (n = 0; n < 20; ++n) { seat[n] = -1; place[n] = 0; }
    seat[0] = 7;  place[0] = 3;
    seat[1] = 2;  place[1] = 1;
    seat[2] = 11; place[2] = 4;
    seat[3] = 5;  place[3] = 2;
    dr3_results_rows(rows, DR3_RESULTS_RACERS, seat, place, 4);
    CHECK(rows[0] == 2 && rows[1] == 5 && rows[2] == 7 && rows[3] == 11,
          "4 car race: got %d,%d,%d,%d want 2,5,7,11", rows[0], rows[1], rows[2], rows[3]);

    /* the 3DS case: twenty cars from the standings, the winner somewhere in the middle of the field */
    for (n = 0; n < 20; ++n) { seat[n] = 19 - n; place[n] = 0; }
    place[17] = 1;
    place[3]  = 2;
    place[12] = 3;
    place[8]  = 4;
    dr3_results_rows(rows, DR3_RESULTS_RACERS, seat, place, 20);
    CHECK(rows[0] == seat[17] && rows[1] == seat[3] && rows[2] == seat[12] && rows[3] == seat[8],
          "chaos field: got %d,%d,%d,%d want %d,%d,%d,%d", rows[0], rows[1], rows[2], rows[3],
          seat[17], seat[3], seat[12], seat[8]);
    CHECK(results_rows_sane(rows, DR3_RESULTS_RACERS), "chaos field: four rows, four different racers");

    /* a duel: two cars, two places - the rows the race did not fill name racers of their own */
    for (n = 0; n < 20; ++n) { seat[n] = -1; place[n] = 0; }
    seat[0] = 4; place[0] = 2;
    seat[1] = 9; place[1] = 1;
    dr3_results_rows(rows, DR3_RESULTS_RACERS, seat, place, 2);
    CHECK(rows[0] == 9 && rows[1] == 4, "duel: got %d,%d want 9,4", rows[0], rows[1]);
    CHECK(results_rows_sane(rows, DR3_RESULTS_RACERS), "duel: the two empty rows are racers of their own");

    /* a race that only filled one place, and a table with the same place on two cars */
    for (n = 0; n < 20; ++n) { seat[n] = 19 - n; place[n] = 0; }
    place[5] = 2;
    dr3_results_rows(rows, DR3_RESULTS_RACERS, seat, place, 20);
    CHECK(rows[1] == seat[5], "only place 2: row 1 is car 5 (got %d want %d)", rows[1], seat[5]);
    CHECK(results_rows_sane(rows, DR3_RESULTS_RACERS), "only place 2: the other rows are racers of their own");

    for (n = 0; n < 20; ++n) { seat[n] = n; place[n] = 0; }
    place[6] = 1;
    place[2] = 1;      /* two cars carry the winner's place */
    dr3_results_rows(rows, DR3_RESULTS_RACERS, seat, place, 20);
    CHECK(rows[0] == seat[2], "two cars with place 1: the first one keeps the row (got %d want %d)",
          rows[0], seat[2]);
    CHECK(results_rows_sane(rows, DR3_RESULTS_RACERS), "two cars with place 1: still four different racers");

    /* garbage in: a place the panel does not have, a car whose racer is not one of the twenty */
    for (n = 0; n < 20; ++n) { seat[n] = -1; place[n] = 0; }
    seat[0] = 99; place[0] = 1;
    seat[1] = 3;  place[1] = 77;
    seat[2] = 8;  place[2] = 1;
    dr3_results_rows(rows, DR3_RESULTS_RACERS, seat, place, 3);
    CHECK(rows[0] == 8, "garbage: the only usable winner (got %d want 8)", rows[0]);
    CHECK(results_rows_sane(rows, DR3_RESULTS_RACERS), "garbage: the rows are still four different racers");

    /* nothing to go by at all: the rows are racers, and fewer racers than rows stay inside the table */
    dr3_results_rows(rows, DR3_RESULTS_RACERS, NULL, NULL, 0);
    CHECK(results_rows_sane(rows, DR3_RESULTS_RACERS), "no race at all: four different racers");
    dr3_results_rows(rows, 0, NULL, NULL, 0);
    CHECK(results_rows_sane(rows, DR3_RESULTS_RACERS), "a wild racer count means the %d of the game",
          DR3_RESULTS_RACERS);
    dr3_results_rows(rows, 3, NULL, NULL, 0);
    for (n = 0; n < DR3_RESULTS_ROWS; ++n)
        CHECK((rows[n] >= 0) && (rows[n] < 3), "three racers and four rows: row %d in range (got %d)",
              n, rows[n]);
}



/* ------------------------------------------------------------- the frame time work (perf harness) --- */

static unsigned g_rng = 12345u;
static unsigned rnd(void) { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

/* the 320 -> 400 race blit must write exactly what the general nearest neighbour loop writes */
static void test_blit_rot_5_4(void)
{
    enum { SW = 320, DW = 400, DH = 240, PITCH = 352 };
    static uint8_t  src[PITCH * DH];
    static uint32_t lut[256], fast[DW * DH], ref[DW * DH];
    int x, y, i;

    for (i = 0; i < 256; ++i) lut[i] = 0xFF000000u | (unsigned)(i * 0x010203u) ^ (unsigned)(i << 3);
    for (i = 0; i < (int)sizeof(src); ++i) src[i] = (uint8_t)rnd();
    memset(fast, 0xAA, sizeof(fast));

    /* reference: the loop of dr3_fb_present - target (x, y) = fb[x * DH + (DH - 1 - y)], source column x * SW / DW */
    for (x = 0; x < DW; ++x)
        for (y = 0; y < DH; ++y)
            ref[x * DH + (DH - 1 - y)] = lut[src[(size_t)y * PITCH + (size_t)(x * SW / DW)]];

    dr3_blit_rot_5_4(fast, DH, DH, src, SW, PITCH, lut);

    for (i = 0; i < DW * DH; ++i)
        if (fast[i] != ref[i]) { CHECK(0, "rot_5_4: pixel %d differs: 0x%08X vs 0x%08X", i, fast[i], ref[i]); break; }
    CHECK(1, "rot_5_4 compared %d pixels", DW * DH);

    /* the group mapping the function is built on */
    {
        static const int off[5] = { 0, 0, 1, 2, 3 };

        for (x = 0; x < DW; ++x)
            CHECK((x * 4 / 5) == (4 * (x / 5) + off[x % 5]), "group mapping of column %d", x);
    }
}

/* the word wise mask scan against the byte wise one it replaced */
static void ref_mask_bounds(const uint8_t *mask, int w, int h, int pitch, int *any, int box[4])
{
    int r, c;

    *any = 0;
    box[0] = w; box[1] = h; box[2] = -1; box[3] = -1;
    for (r = 0; r < h; ++r)
        for (c = 0; c < w; ++c)
            if (mask[(size_t)r * pitch + c]) {
                *any = 1;
                if (c < box[0]) box[0] = c;
                if (r < box[1]) box[1] = r;
                if (c > box[2]) box[2] = c;
                if (r > box[3]) box[3] = r;
            }
}

static void test_mask_bounds(void)
{
    enum { W = 320, H = 200 };
    static uint32_t buf32[(W * H) / 4 + 4];         /* 4-byte aligned, like dr3_chaos_car_screen */
    uint8_t        *mask = (uint8_t *)buf32;
    int             t, k;

    for (t = 0; t < 400; ++t) {

        int any_f, any_r, bf[4] = { -7, -7, -7, -7 }, br[4], n = (t % 7 == 0) ? 0 : (int)(rnd() % 6);

        memset(mask, 0, W * H);
        for (k = 0; k < n; ++k) {                   /* a few small blobs, sometimes single pixels at the edges */
            const int cx = (int)(rnd() % W), cy = (int)(rnd() % H), bw = 1 + (int)(rnd() % 40), bh = 1 + (int)(rnd() % 30);
            int       x, y;

            for (y = cy; y < cy + bh && y < H; ++y)
                for (x = cx; x < cx + bw && x < W; ++x) mask[y * W + x] = (uint8_t)(1 + rnd() % 20);
        }
        if (t % 11 == 0) { mask[0] = 1; mask[W * H - 1] = 2; }          /* the very first and last byte */

        any_f = dr3_carcolour_mask_bounds(mask, W, H, W, bf);
        ref_mask_bounds(mask, W, H, W, &any_r, br);

        CHECK(any_f == any_r, "mask bounds %d: any %d vs %d", t, any_f, any_r);
        if (any_r) CHECK(!memcmp(bf, br, sizeof(bf)), "mask bounds %d: box %d,%d,%d,%d vs %d,%d,%d,%d", t,
                         bf[0], bf[1], bf[2], bf[3], br[0], br[1], br[2], br[3]);
        else       CHECK(bf[0] == -7, "mask bounds %d: an empty mask must leave the box alone", t);
    }

    /* an unaligned mask takes the byte path and must agree as well */
    {
        int any_f, any_r, bf[4], br[4];

        memset(mask, 0, W * H + 4);
        mask[1 + 77 * W + 200] = 3;
        any_f = dr3_carcolour_mask_bounds(mask + 1, W, H, W, bf);
        ref_mask_bounds(mask + 1, W, H, W, &any_r, br);
        CHECK(any_f && any_r && !memcmp(bf, br, sizeof(bf)), "unaligned mask bounds");
    }
}

/* the tint walk that only visits the box against the walk over every row it replaced */
static int ref_tint_mask(uint32_t *dst, int dw, int dh, int sx_stride, int sy_stride,
                         const uint8_t *src, int sw, int sh, int src_pitch,
                         const uint8_t *mask, int mw, int mh, int mp, int row_off,
                         const dr3_carcolour_area_t *area, const uint32_t *shades, int n_cars)
{
    int x, y, n = 0;

    if (area && (area->x1 < area->x0)) return 0;
    for (x = 0; x < dw; ++x) {
        const int sx = (int)area->map_x[x];

        if (sx >= mw) continue;
        if ((sx < area->x0) || (sx > area->x1)) continue;
        for (y = 0; y < dh; ++y) {
            const int sy = (int)area->map_y[y];
            const int my = sy - row_off;
            uint8_t   m;

            if ((my < 0) || (my >= mh)) continue;
            if ((my < area->y0) || (my > area->y1)) continue;
            m = mask[(size_t)my * mp + (size_t)sx];
            if (!m || (m > n_cars)) continue;
            {
                const uint8_t v = src[(size_t)sy * src_pitch + (size_t)sx];

                if ((v < DR3_CARCOLOUR_FIRST) || (v > DR3_CARCOLOUR_LAST)) continue;
                dst[(ptrdiff_t)sx_stride * x + (ptrdiff_t)sy_stride * y] = shades[(m - 1) * 10 + (v - DR3_CARCOLOUR_FIRST)];
                ++n;
            }
        }
    }
    (void)sw; (void)sh;
    return n;
}

static void tint_mask_box_run(int SH_)
{
    enum { SW = 320, DW = 400, DH = 240, MW = 320, MH = 200, OFF = 20, NC = 20 };
    const int SH = SH_;
    static uint8_t  src[SW * 240], mask[MW * MH];
    static uint32_t a[DW * DH], b[DW * DH], shades[NC * 10];
    static uint16_t mx[DW], my[DH];
    int             t, i, x, y;

    for (x = 0; x < DW; ++x) mx[x] = (uint16_t)((unsigned)x * SW / DW);
    for (y = 0; y < DH; ++y) my[y] = (uint16_t)((unsigned)y * SH / DH);
    for (i = 0; i < NC * 10; ++i) shades[i] = 0xFF000000u | (unsigned)(i * 0x010305u);

    for (t = 0; t < 150; ++t) {

        dr3_carcolour_area_t area;
        int                  n_f, n_r, box[4], k, nb = (int)(rnd() % 5);

        for (i = 0; i < SW * 240; ++i) src[i] = (uint8_t)(DR3_CARCOLOUR_FIRST + rnd() % 14);   /* ramp and beyond it */
        memset(mask, 0, sizeof(mask));
        for (k = 0; k < nb; ++k) {
            const int cx = (int)(rnd() % MW), cy = (int)(rnd() % MH), bw = 1 + (int)(rnd() % 50), bh = 1 + (int)(rnd() % 40);

            for (y = cy; y < cy + bh && y < MH; ++y)
                for (x = cx; x < cx + bw && x < MW; ++x) mask[y * MW + x] = (uint8_t)(1 + rnd() % NC);
        }

        area.map_x = mx; area.map_y = my;
        area.x0 = MW; area.y0 = MH; area.x1 = -1; area.y1 = -1;
        if (dr3_carcolour_mask_bounds(mask, MW, MH, MW, box)) { area.x0 = box[0]; area.y0 = box[1]; area.x1 = box[2]; area.y1 = box[3]; }

        memset(a, 0x11, sizeof(a)); memset(b, 0x11, sizeof(b));
        n_f = dr3_carcolour_tint_mask(a + (DH - 1), DW, DH, DH, -1, src, SW, SH, SW, mask, MW, MH, MW, OFF, &area, shades, NC);
        n_r = ref_tint_mask(b + (DH - 1), DW, DH, DH, -1, src, SW, SH, SW, mask, MW, MH, MW, OFF, &area, shades, NC);

        CHECK(n_f == n_r, "tint (sh %d) %d: %d pixels vs %d", SH, t, n_f, n_r);
        CHECK(!memcmp(a, b, sizeof(a)), "tint (sh %d) %d: the image differs", SH, t);
    }
}

static void test_tint_mask_box(void)
{
    tint_mask_box_run(240);      /* rows 1:1 - the row walk (the race screen) */
    tint_mask_box_run(200);      /* a stretched source - the general walk     */
}

/*
 * The arena (platform_3ds/dr3_arena.h): where the cars start, which way they turn, which way is free, who is
 * ranked where, what a place pays and which item the dice give.  The engine side (the AI itself) is not here -
 * it needs the race; what it *decides with* is.
 */

/* a 300 x 200 room with a wall of 20 round it and a pillar in the middle - the shape of the arena in small */
#define ARN_W 300
#define ARN_H 200

static int arn_solid(void *ctx, int x, int y)
{
    (void)ctx;

    if (x < 20 || y < 20 || x >= ARN_W - 20 || y >= ARN_H - 20) return 1;
    if (x >= 140 && x < 160 && y >= 90 && y < 110) return 1;         /* the pillar */

    return 0;
}

static void test_arena(void)
{
    int   place[16], hp[16], died[16], seen[16];
    int   x, y, img, n, k, a, c, count[11];
    float h;

    printf("arena\n");

    /* --- the corners: the first four places are the four corners, every place is its own spot */
    {
        int px[16], py[16];

        for (n = 0; n < 16; ++n) {

            dr3_arena_spawn(n, 960, 600, &x, &y, &img);
            px[n] = x; py[n] = y;

            CHECK((x >= 60) && (x <= 960 - 60) && (y >= 60) && (y <= 600 - 60), "place %d is not inside the arena: %d,%d", n, x, y);
            CHECK((img >= 0) && (img < 96), "place %d: picture %d", n, img);

            for (k = 0; k < n; ++k)
                CHECK((px[k] != x) || (py[k] != y), "place %d and %d share the spot %d,%d", k, n, x, y);
        }

        dr3_arena_spawn(0, 960, 600, &x, &y, &img);  CHECK((x < 480) && (y < 300), "place 0 is not top left: %d,%d", x, y);
        dr3_arena_spawn(1, 960, 600, &x, &y, &img);  CHECK((x > 480) && (y < 300), "place 1 is not top right: %d,%d", x, y);
        dr3_arena_spawn(2, 960, 600, &x, &y, &img);  CHECK((x < 480) && (y > 300), "place 2 is not bottom left: %d,%d", x, y);
        dr3_arena_spawn(3, 960, 600, &x, &y, &img);  CHECK((x > 480) && (y > 300), "place 3 is not bottom right: %d,%d", x, y);

        /* the cars face the middle: the picture's heading is within one step (3.75 deg) of the bearing of the centre */
        for (n = 0; n < 16; ++n) {

            float want, have, diff;

            dr3_arena_spawn(n, 960, 600, &x, &y, &img);
            want = dr3_arena_bearing((float)(480 - x), (float)(300 - y));
            have = 3.75f * (float)img;
            diff = dr3_arena_wrap180(have - want);
            if (diff < 0) diff = -diff;

            CHECK(diff <= 3.75f, "place %d faces %.1f, the middle is at %.1f", n, (double)have, (double)want);
        }

        /* the same corners on the generator's numbers (scripts/arena/make_arena.py: anchor 88, step 56) */
        dr3_arena_spawn(0, 960, 600, &x, &y, &img);  CHECK((x == 88) && (y == 88), "place 0 at %d,%d, make_arena.py says 88,88", x, y);
        dr3_arena_spawn(4, 960, 600, &x, &y, &img);  CHECK((x == 144) && (y == 88), "place 4 at %d,%d, make_arena.py says 144,88", x, y);
        dr3_arena_spawn(8, 960, 600, &x, &y, &img);  CHECK((x == 88) && (y == 144), "place 8 at %d,%d, make_arena.py says 88,144", x, y);
        dr3_arena_spawn(3, 960, 600, &x, &y, &img);  CHECK((x == 872) && (y == 512), "place 3 at %d,%d, make_arena.py says 872,512", x, y);
    }

    /* --- the engine's compass: Direction D moves along (-sin D, -cos D) */
    CHECK(fabsf(dr3_arena_bearing(0.0f, -1.0f) - 0.0f) < 0.01f,   "up is 0, got %f", (double)dr3_arena_bearing(0.0f, -1.0f));
    CHECK(fabsf(dr3_arena_bearing(-1.0f, 0.0f) - 90.0f) < 0.01f,  "left is 90, got %f", (double)dr3_arena_bearing(-1.0f, 0.0f));
    CHECK(fabsf(dr3_arena_bearing(0.0f, 1.0f) - 180.0f) < 0.01f,  "down is 180, got %f", (double)dr3_arena_bearing(0.0f, 1.0f));
    CHECK(fabsf(dr3_arena_bearing(1.0f, 0.0f) - 270.0f) < 0.01f,  "right is 270, got %f", (double)dr3_arena_bearing(1.0f, 0.0f));
    CHECK(dr3_arena_bearing(0.0f, 0.0f) == 0.0f, "no direction is 0");

    CHECK(fabsf(dr3_arena_wrap180(350.0f) - (-10.0f)) < 0.001f, "350 wraps to -10");
    CHECK(fabsf(dr3_arena_wrap180(-190.0f) - 170.0f) < 0.001f, "-190 wraps to 170");
    CHECK(fabsf(dr3_arena_wrap180(180.0f) - 180.0f) < 0.001f, "180 stays");

    /* --- steering: the goal to the left of the car (Direction grows) = STEER_LEFT (+1) */
    CHECK(dr3_arena_steer(0.0f, 30.0f, 4.0f) == 1,    "goal 30 deg left: steer left");
    CHECK(dr3_arena_steer(0.0f, 330.0f, 4.0f) == -1,  "goal 30 deg right: steer right");
    CHECK(dr3_arena_steer(0.0f, 3.0f, 4.0f) == 0,     "within the dead zone: straight on");
    CHECK(dr3_arena_steer(350.0f, 10.0f, 4.0f) == 1,  "across the 0/360 seam: 20 deg left");
    CHECK(dr3_arena_steer(10.0f, 350.0f, 4.0f) == -1, "across the seam the other way");

    /* --- room: the free run along a heading */
    k = dr3_arena_room(arn_solid, NULL, 150.0f, 150.0f, 0.0f, 92);                                 /* heading 0 = up: the pillar's bottom edge is at y 110 */
    CHECK((k >= 30) && (k <= 36), "room up to the pillar: %d (36 px, in steps of 6)", k);
    k = dr3_arena_room(arn_solid, NULL, 150.0f, 175.0f, 180.0f, 92);                               /* down: the wall starts at y 180 */
    CHECK(k == 0, "room down at the wall: %d", k);
    k = dr3_arena_room(arn_solid, NULL, 100.0f, 100.0f, 270.0f, 92);                               /* right: the pillar at x 140 */
    CHECK((k >= 30) && (k <= 42), "room right to the pillar: %d", k);
    k = dr3_arena_room(arn_solid, NULL, 60.0f, 60.0f, 270.0f, 92);                                 /* right along the free upper lane */
    CHECK(k == 92, "free lane: %d", k);

    /* --- the plan: goal behind the pillar -> a heading that has room; goal in the open -> the goal itself */
    {
        int blocked;

        h = dr3_arena_plan(arn_solid, NULL, 100.0f, 100.0f, 270.0f, 270.0f, &blocked, 52);        /* the goal is right of the pillar, straight ahead */
        CHECK(blocked, "the pillar is closer than the brake distance, the car must be told");
        a = dr3_arena_room(arn_solid, NULL, 100.0f, 100.0f, h, DR3_ARENA_LOOK);
        CHECK(a >= DR3_ARENA_PASSABLE, "the planner chose %.0f deg with %d px of room", (double)h, a);
        CHECK(fabsf(dr3_arena_wrap180(h - 270.0f)) > 5.0f, "the planner went round the pillar: %.0f", (double)h);

        h = dr3_arena_plan(arn_solid, NULL, 60.0f, 60.0f, 270.0f, 270.0f, &blocked, 52);          /* free lane */
        CHECK(!blocked, "the free lane is not blocked");
        CHECK(fabsf(dr3_arena_wrap180(h - 270.0f)) < 0.01f, "in the open the goal is the heading: %.1f", (double)h);

        /* a car pressed into a corner picks the way out, not the wall */
        h = dr3_arena_plan(arn_solid, NULL, 30.0f, 30.0f, 90.0f, 90.0f, &blocked, 52);
        a = dr3_arena_room(arn_solid, NULL, 30.0f, 30.0f, h, DR3_ARENA_LOOK);
        CHECK(a > 0, "the corner: heading %.0f with %d px of room", (double)h, a);
        CHECK((h >= 0.0f) && (h < 360.0f), "the plan is a heading 0..360: %f", (double)h);
    }

    /* --- the sights: the machine gun hits what is in front, within range and a 20 px box */
    CHECK(dr3_arena_in_sights(100, 100, 0.0f, 100, 40, 128, 20),   "straight ahead, 60 px: hit");
    CHECK(dr3_arena_in_sights(100, 100, 0.0f, 112, 40, 128, 20),   "12 px to the side: hit");
    CHECK(!dr3_arena_in_sights(100, 100, 0.0f, 140, 40, 128, 20),  "40 px to the side: miss");
    CHECK(!dr3_arena_in_sights(100, 100, 0.0f, 100, 200, 128, 20), "behind: miss");
    CHECK(!dr3_arena_in_sights(100, 100, 0.0f, 100, -100, 128, 20),"200 px ahead: out of range");
    CHECK(dr3_arena_in_sights(100, 100, 90.0f, 40, 100, 128, 20),  "facing left, target left: hit");
    CHECK(!dr3_arena_in_sights(100, 100, 90.0f, 100, 40, 128, 20), "facing left, target above: miss");

    /* --- line of sight: clear lane vs blocked by the middle pillar (140..160, 90..110) */
    CHECK(dr3_arena_los(arn_solid, NULL, 60, 60, 240, 60),    "clear upper lane: LOS ok");
    CHECK(!dr3_arena_los(arn_solid, NULL, 100, 100, 200, 100),"across the pillar: LOS blocked");
    CHECK(!dr3_arena_los(arn_solid, NULL, 150, 50, 150, 150), "vertically through the pillar: LOS blocked");
    CHECK(dr3_arena_los(arn_solid, NULL, 100, 100, 105, 100), "point-blank: LOS ok");

    /* --- behind */
    CHECK(dr3_arena_behind(100, 100, 0.0f, 100, 140, 110, 40.0f),  "chaser right behind: yes");
    CHECK(!dr3_arena_behind(100, 100, 0.0f, 100, 60, 110, 40.0f),  "target ahead: no");
    CHECK(!dr3_arena_behind(100, 100, 0.0f, 100, 260, 110, 40.0f), "behind but too far: no");
    CHECK(!dr3_arena_behind(100, 100, 0.0f, 160, 100, 110, 40.0f), "to the side: no");

    /* --- the dice: every roll gives one of the seven arena items (ammo, mine crates, cash, turbo, repairs) */
    for (n = 0; n < 11; ++n) count[n] = 0;

    for (n = 0; n < 100; ++n) {

        k = dr3_arena_item_for_roll(n);
        CHECK((k == DR3_ITEM_AMMO) || (k == DR3_ITEM_TURBO) || (k == DR3_ITEM_MINE_CRATE) ||
              (k == DR3_ITEM_MONEY) || (k == DR3_ITEM_MONEY_BIG) ||
              (k == DR3_ITEM_REPAIR_SMALL) || (k == DR3_ITEM_REPAIR_BIG), "roll %d gave the item %d", n, k);
        if ((k >= 1) && (k <= 10)) ++count[k];
    }

    CHECK(count[DR3_ITEM_AMMO] == 30,         "ammunition: %d of 100", count[DR3_ITEM_AMMO]);
    CHECK(count[DR3_ITEM_MINE_CRATE] == 20,   "mine crates: %d of 100", count[DR3_ITEM_MINE_CRATE]);
    CHECK(count[DR3_ITEM_MONEY] == 10,        "money: %d of 100", count[DR3_ITEM_MONEY]);
    CHECK(count[DR3_ITEM_MONEY_BIG] == 4,     "big money: %d of 100", count[DR3_ITEM_MONEY_BIG]);
    CHECK(count[DR3_ITEM_TURBO] == 12,        "turbo: %d of 100", count[DR3_ITEM_TURBO]);
    CHECK(count[DR3_ITEM_REPAIR_SMALL] == 8,  "small repairs: %d of 100", count[DR3_ITEM_REPAIR_SMALL]);
    CHECK(count[DR3_ITEM_REPAIR_BIG] == 16,   "big repairs: %d of 100", count[DR3_ITEM_REPAIR_BIG]);
    CHECK(count[DR3_ITEM_MINE_CRATE] >= 20,   "the arena has to have real mine crates to collect");
    CHECK(dr3_arena_item_for_roll(-5) == dr3_arena_item_for_roll(0),   "a roll below 0 is 0");
    CHECK(dr3_arena_item_for_roll(500) == dr3_arena_item_for_roll(99), "a roll above 99 is 99");

    /* --- grudge rival picker: never picks player, hits every AI car across rolls, safe on edge cases */
    {
        int hit[12], r, g;
        memset(hit, 0, sizeof(hit));
        for (r = 0; r < 11; ++r) {
            g = dr3_arena_pick_grudge(12, 3, r);
            CHECK((g >= 0) && (g < 12) && (g != 3), "grudge roll %d picked %d (player 3)", r, g);
            if ((g >= 0) && (g < 12)) ++hit[g];
        }
        CHECK(hit[3] == 0, "grudge picker never selects player");
        for (r = 0; r < 12; ++r) if (r != 3) CHECK(hit[r] == 1, "AI car %d hit %d time(s)", r, hit[r]);
        CHECK(dr3_arena_pick_grudge(12, 3, -4) >= 0 && dr3_arena_pick_grudge(12, 3, -4) != 3, "negative roll safe");
        CHECK(dr3_arena_pick_grudge(1, 0, 0) == -1, "single car has no grudge rival");
        CHECK(dr3_arena_pick_grudge(0, 0, 0) == -1, "empty field has no grudge rival");
    }

    /* --- the prizes: three of them, falling, nothing below */
    CHECK(dr3_arena_prize(1) == DR3_ARENA_PRIZE_1, "1st");
    CHECK(dr3_arena_prize(2) == DR3_ARENA_PRIZE_2, "2nd");
    CHECK(dr3_arena_prize(3) == DR3_ARENA_PRIZE_3, "3rd");
    CHECK(dr3_arena_prize(4) == 0, "4th gets nothing");
    CHECK(dr3_arena_prize(12) == 0, "12th gets nothing");
    CHECK(dr3_arena_prize(0) == 0, "no place 0");
    CHECK((DR3_ARENA_PRIZE_1 > DR3_ARENA_PRIZE_2) && (DR3_ARENA_PRIZE_2 > DR3_ARENA_PRIZE_3) && (DR3_ARENA_PRIZE_3 > 0), "the ladder falls");
    CHECK(DR3_ARENA_PRIZE_1 > 5000, "an arena pays more than the pot of a deathmatch ($5000)");

    /* --- the field */
    CHECK(dr3_arena_extras(4, 0) == 0,   "extras off: the event's cars only");
    CHECK(dr3_arena_extras(4, 18) == 8,  "extras asked for 18: the arena stops at %d cars (4 + 8)", DR3_ARENA_CARS);
    CHECK(dr3_arena_extras(4, 3) == 3,   "extras asked for 3 fit");
    CHECK(dr3_arena_extras(12, 5) == 0,  "a full field gets no more");
    CHECK(dr3_arena_extras(20, 5) == 0,  "more cars than the arena holds: no extras, never negative");

    /* --- the standings: survivors by HP, then the wrecks, the last to die first */
    for (n = 0; n < 8; ++n) { hp[n] = 0; died[n] = 0; }
    hp[0] = 50000; hp[3] = 90000; hp[5] = 20000;                 /* the survivors: 3 > 0 > 5 */
    died[1] = 400; died[2] = 900; died[4] = 100; died[6] = 700; died[7] = 300;

    dr3_arena_rank(8, hp, died, place);

    CHECK(place[3] == 1, "most HP is first: %d", place[3]);
    CHECK(place[0] == 2, "second HP is second: %d", place[0]);
    CHECK(place[5] == 3, "least HP of the survivors is third: %d", place[5]);
    CHECK(place[2] == 4, "the last wreck (900) is fourth: %d", place[2]);
    CHECK(place[6] == 5, "died at 700: fifth: %d", place[6]);
    CHECK(place[1] == 6, "died at 400: sixth: %d", place[1]);
    CHECK(place[7] == 7, "died at 300: seventh: %d", place[7]);
    CHECK(place[4] == 8, "died first: last: %d", place[4]);

    for (n = 0; n < 9; ++n) seen[n] = 0;
    for (n = 0; n < 8; ++n) if ((place[n] >= 1) && (place[n] <= 8)) ++seen[place[n]];
    for (n = 1; n <= 8; ++n) CHECK(seen[n] == 1, "place %d is given %d time(s)", n, seen[n]);

    /* ties: the same HP -> the lower index; the same death tick -> the lower index; and it is still a permutation */
    for (n = 0; n < 6; ++n) { hp[n] = (n < 2) ? 777 : 0; died[n] = (n >= 2 && n < 4) ? 50 : 60 + n; }
    dr3_arena_rank(6, hp, died, place);
    CHECK((place[0] == 1) && (place[1] == 2), "equal HP: the lower index first: %d %d", place[0], place[1]);
    CHECK(place[2] < place[3], "same death tick: the lower index first: %d %d", place[2], place[3]);
    for (n = 0; n < 7; ++n) seen[n] = 0;
    for (n = 0; n < 6; ++n) if ((place[n] >= 1) && (place[n] <= 6)) ++seen[place[n]];
    for (n = 1; n <= 6; ++n) CHECK(seen[n] == 1, "tie case: place %d is given %d time(s)", n, seen[n]);

    /* a single car, and nobody left */
    hp[0] = 1; dr3_arena_rank(1, hp, died, place);
    CHECK(place[0] == 1, "alone: first");
    for (n = 0; n < 4; ++n) { hp[n] = 0; died[n] = 10 * (n + 1); }
    dr3_arena_rank(4, hp, died, place);
    CHECK((place[3] == 1) && (place[0] == 4), "all wrecked: the last one standing up longest wins: %d %d", place[3], place[0]);

    /* --- the closing ring: grace, linear shrink, clamped end, outside test, drain, AI safe margin, banner */
    {
        const int grace  = DR3_ARENA_RING_GRACE_S * 70;
        const int end_t  = (DR3_ARENA_RING_GRACE_S + DR3_ARENA_RING_SHRINK_S) * 70;
        const int mid_t  = grace + (DR3_ARENA_RING_SHRINK_S * 70) / 2;
        const char *l1 = NULL, *l2 = NULL;
        int blink_on = 0, blink_off = 0, t;

        CHECK(dr3_arena_ring_phase(0) == 0,           "phase 0 at tick 0");
        CHECK(dr3_arena_ring_phase(grace) == 0,       "phase 0 at end of grace");
        CHECK(dr3_arena_ring_phase(grace + 1) == 1,   "phase 1 right after grace");
        CHECK(dr3_arena_ring_phase(mid_t) == 1,       "phase 1 mid-shrink");
        CHECK(dr3_arena_ring_phase(end_t) == 2,       "phase 2 at end of shrink");
        CHECK(dr3_arena_ring_phase(end_t + 500) == 2, "phase 2 long after shrink");

        CHECK(dr3_arena_ring_radius(0) == DR3_ARENA_RING_START_R,       "full radius at start");
        CHECK(dr3_arena_ring_radius(grace) == DR3_ARENA_RING_START_R,   "full radius through grace");
        CHECK(dr3_arena_ring_radius(end_t) == DR3_ARENA_RING_END_R,     "end radius when closed");
        CHECK(dr3_arena_ring_radius(end_t + 999) == DR3_ARENA_RING_END_R, "end radius stays clamped");
        CHECK(dr3_arena_ring_radius(mid_t) == (DR3_ARENA_RING_START_R + DR3_ARENA_RING_END_R) / 2,
              "midpoint radius: got %d", dr3_arena_ring_radius(mid_t));

        CHECK(!dr3_arena_ring_outside(480.0f, 300.0f, 480, 300, 120), "centre is inside");
        CHECK(!dr3_arena_ring_outside(600.0f, 300.0f, 480, 300, 120), "on the circle is inside");
        CHECK(dr3_arena_ring_outside(601.0f, 300.0f, 480, 300, 120),  "past the circle is outside");
        CHECK(!dr3_arena_ring_outside(88.0f, 88.0f, 480, 300, DR3_ARENA_RING_START_R), "corners start inside");

        k = dr3_arena_ring_drain(DR3_ARENA_HP_MAX);
        CHECK((k > 0) && (k * 70 >= DR3_ARENA_HP_MAX * DR3_ARENA_RING_DRAIN_PCT / 100 - 70) &&
              (k * 70 <= DR3_ARENA_HP_MAX * DR3_ARENA_RING_DRAIN_PCT / 100),
              "drain per frame %d (~%d%%/s)", k, DR3_ARENA_RING_DRAIN_PCT);
        CHECK(dr3_arena_ring_drain(1) >= 1, "drain never rounds down to zero");

        /* AI safe radius: stays inside the ring, and at the final radius still clears the 58 px centre hub */
        CHECK(dr3_arena_ring_safe_r(DR3_ARENA_RING_START_R) == DR3_ARENA_RING_START_R - DR3_ARENA_RING_SAFE,
              "safe_r at full radius");
        CHECK(dr3_arena_ring_safe_r(DR3_ARENA_RING_END_R) > 75 &&
              dr3_arena_ring_safe_r(DR3_ARENA_RING_END_R) < DR3_ARENA_RING_END_R,
              "safe_r at final radius (%d) must clear the hub and stay inside %d",
              dr3_arena_ring_safe_r(DR3_ARENA_RING_END_R), DR3_ARENA_RING_END_R);

        /* banner: off during grace, "THE RING / CLOSES" for 5 s, blinking "GET INSIDE / THE RING" when outside */
        CHECK(dr3_arena_banner(grace, 1, &l1, &l2) == 0, "no banner during grace");
        CHECK(dr3_arena_banner(grace + 10, 0, &l1, &l2) == 1, "closing announcement shown");
        CHECK(l1 && l2 && (strcmp(l1, "THE RING") == 0) && (strcmp(l2, "CLOSES") == 0), "closing banner text");
        CHECK(dr3_dm_text_width(l1, DR3_DM_BANNER_SCALE) <= 256, "closing line 1 fits viewport");
        CHECK(dr3_dm_text_width(l2, DR3_DM_BANNER_SCALE) <= 256, "closing line 2 fits viewport");
        CHECK(dr3_arena_banner(grace + 6 * 70, 0, &l1, &l2) == 0, "announcement ends after 5 s");

        for (t = grace + 1; t < grace + 100; ++t) {
            const int b = dr3_arena_banner(t, 1, &l1, &l2);

            if (b == 2) {
                ++blink_on;
                CHECK(l1 && l2 && (strcmp(l1, "GET INSIDE") == 0) && (strcmp(l2, "THE RING") == 0), "outside banner text");
                CHECK(dr3_dm_text_width(l1, DR3_DM_BANNER_SCALE) <= 256, "outside line 1 fits viewport");
                CHECK(dr3_dm_text_width(l2, DR3_DM_BANNER_SCALE) <= 256, "outside line 2 fits viewport");
            }
            else if (b == 0) ++blink_off;
            else CHECK(0, "outside banner returned unexpected kind %d", b);
        }
        CHECK((blink_on > 0) && (blink_off > 0), "outside banner blinks (on %d, off %d)", blink_on, blink_off);
    }

    /* --- minimap closing ring overlay: drawn, shrinks incrementally without leaks, clears cleanly */
    {
        static uint32_t mbuf[64 * 40];
        static uint8_t  mmask[960 * 600];
        dr3_canvas_t    mc;
        const uint32_t  rcol = minimap_color(DR3_MAP_COL_RING);
        int             i, cnt1 = 0, cnt2 = 0, cnt0 = 0;

        memset(mmask, 0x0f, sizeof(mmask));
        CHECK(dr3_minimap_build(mmask, NULL, NULL, 960, 600) == 1, "minimap build for ring");
        memset(mbuf, 0, sizeof(mbuf));
        mc.px = mbuf; mc.fmt = DR3_CANVAS_RGBA8888; mc.stride_x = 1; mc.stride_y = 64; mc.w = 64; mc.h = 40;

        dr3_minimap_set_ring(240);
        CHECK(dr3_minimap_draw(&mc, 0, 0, 64, 40, NULL, 0) == 1, "minimap draw with ring");
        for (i = 0; i < 64 * 40; ++i) cnt1 += (mbuf[i] == rcol);
        CHECK(cnt1 > 20, "ring pixels drawn on minimap: %d", cnt1);

        dr3_minimap_set_ring(120);
        CHECK(dr3_minimap_draw_incremental(&mc, 0, 0, 64, 40, NULL, 0) == 1, "minimap shrink ring");
        for (i = 0; i < 64 * 40; ++i) cnt2 += (mbuf[i] == rcol);
        CHECK((cnt2 > 10) && (cnt2 < cnt1), "smaller ring has fewer pixels: %d vs %d", cnt2, cnt1);

        dr3_minimap_set_ring(0);
        CHECK(dr3_minimap_draw_incremental(&mc, 0, 0, 64, 40, NULL, 0) == 1, "minimap clear ring");
        for (i = 0; i < 64 * 40; ++i) cnt0 += (mbuf[i] == rcol);
        CHECK(cnt0 == 0, "all ring pixels erased when cleared: %d left", cnt0);
        dr3_minimap_reset();
    }


    c = 0; (void)c;
}

static void test_chaos_boss(void)
{
    int order[DR3_GRID_CARS];
    int rank[DR3_GRID_CARS];
    int i, hits;

    printf("- chaos adversary join, car selection, and pole ordering\n");

    /* join chance & mode eligibility */
    CHECK(dr3_chaos_boss_should_join(0, 0, 25) == 1, "standard mode roll 0 < 25 joins");
    CHECK(dr3_chaos_boss_should_join(24, 0, 25) == 1, "standard mode roll 24 < 25 joins");
    CHECK(dr3_chaos_boss_should_join(25, 0, 25) == 0, "standard mode roll 25 >= 25 does not join");
    CHECK(dr3_chaos_boss_should_join(99, 0, 100) == 1, "standard mode 100%% always joins");
    CHECK(dr3_chaos_boss_should_join(0, 0, 0) == 0, "standard mode 0%% never joins");
    CHECK(dr3_chaos_boss_should_join(0, 2, 25) == 1, "DM mode eligible");
    CHECK(dr3_chaos_boss_should_join(0, 3, 25) == 1, "RANDOM (first-race standard) eligible");
    CHECK(dr3_chaos_boss_should_join(0, 4, 25) == 1, "ARENA mode eligible");
    CHECK(dr3_chaos_boss_should_join(0, 1, 100) == 0, "DUEL mode never spawns adversary");
    CHECK(dr3_chaos_boss_should_join(0, -1, 100) == 0, "invalid negative mode rejected");
    CHECK(dr3_chaos_boss_should_join(0, 5, 100) == 0, "invalid high mode rejected");

    hits = 0;
    for (i = 0; i < 100; ++i) hits += dr3_chaos_boss_should_join(i, 4, DR3_CHAOS_BOSS_CHANCE_DEFAULT);
    CHECK(hits == DR3_CHAOS_BOSS_CHANCE_DEFAULT, "default chance hits %d/100 (want %d)", hits, DR3_CHAOS_BOSS_CHANCE_DEFAULT);

    /* car slot selection: never replaces the player, prefers last slot of field */
    CHECK(dr3_chaos_boss_pick_car(20, 4, 3) == 19, "20-car standard picks slot 19");
    CHECK(dr3_chaos_boss_pick_car(12, 4, 0) == 11, "12-car arena picks slot 11");
    CHECK(dr3_chaos_boss_pick_car(4, 4, 3) == 2, "4-car field with player in 3 picks 2");
    CHECK(dr3_chaos_boss_pick_car(4, 4, 0) == 3, "4-car field with player in 0 picks 3");
    CHECK(dr3_chaos_boss_pick_car(2, 2, 1) == 0, "2-car field with player in 1 picks 0");
    CHECK(dr3_chaos_boss_pick_car(1, 1, 0) == -1, "1-car field refuses boss");
    CHECK(dr3_chaos_boss_pick_car(0, 0, 0) == -1, "empty field refuses boss");

    /* pole-position ordering: leader_car is strictly order[0], everyone else keeps rank order */
    for (i = 0; i < 6; ++i) rank[i] = 6 - i;   /* car 5 is rank 1, car 4 is rank 2, ..., car 0 is rank 6 */
    dr3_grid_order_with_leader(rank, 6, 2, order);
    CHECK(order[0] == 2, "leader car 2 promoted to pole (got %d)", order[0]);
    CHECK(order[1] == 5, "rank 1 car 5 moves to P2 (got %d)", order[1]);
    CHECK(order[2] == 4, "rank 2 car 4 moves to P3 (got %d)", order[2]);
    CHECK(order[3] == 3, "rank 3 car 3 moves to P4 (got %d)", order[3]);
    CHECK(order[4] == 1, "rank 5 car 1 stays P5 (got %d)", order[4]);
    CHECK(order[5] == 0, "rank 6 car 0 stays P6 (got %d)", order[5]);

    /* leader_car = -1 behaves identically to dr3_grid_order_by_rank */
    dr3_grid_order_with_leader(rank, 6, -1, order);
    CHECK(order[0] == 5 && order[1] == 4 && order[5] == 0, "no leader keeps pure standings order");

    /* leader_car already at rank 1 stays pole with no duplication */
    rank[11] = 1;
    for (i = 0; i < 11; ++i) rank[i] = i + 1;  /* tie at rank 1 between car 0 and car 11 */
    dr3_grid_order_with_leader(rank, 12, 11, order);
    CHECK(order[0] == 11, "boss in slot 11 beats rank-1 tie for pole (got %d)", order[0]);
    CHECK(order[1] == 0, "car 0 (rank 1) shifts to P2 (got %d)", order[1]);
    CHECK(order[2] == 1, "car 1 (rank 2) shifts to P3 (got %d)", order[2]);
}

static void test_boss_gauntlet(void)
{
    int           i, j, x, y, img;
    int           px[21], py[21];
    unsigned char unk0[12];

    printf("- 30th Anniversary 3-phase final boss gauntlet\n");

    /* phase titles, subtitles, bonuses, and transitions */
    CHECK(strstr(dr3_boss_phase_title(DR3_BOSS_PHASE_1), "PHASE 1") != NULL, "phase 1 title: %s", dr3_boss_phase_title(DR3_BOSS_PHASE_1));
    CHECK(strstr(dr3_boss_phase_title(DR3_BOSS_PHASE_2), "PHASE 2") != NULL, "phase 2 title: %s", dr3_boss_phase_title(DR3_BOSS_PHASE_2));
    CHECK(strstr(dr3_boss_phase_title(DR3_BOSS_PHASE_3), "FINAL") != NULL,   "phase 3 title: %s", dr3_boss_phase_title(DR3_BOSS_PHASE_3));
    CHECK(dr3_boss_phase_subtitle(DR3_BOSS_PHASE_1)[0] != 0, "phase 1 subtitle non-empty");
    CHECK(dr3_boss_phase_subtitle(DR3_BOSS_PHASE_2)[0] != 0, "phase 2 subtitle non-empty");
    CHECK(dr3_boss_phase_subtitle(DR3_BOSS_PHASE_3)[0] != 0, "phase 3 subtitle non-empty");

    CHECK(dr3_boss_phase_bonus(DR3_BOSS_PHASE_1) == DR3_BOSS_P1_BONUS, "phase 1 bonus $%d", dr3_boss_phase_bonus(DR3_BOSS_PHASE_1));
    CHECK(dr3_boss_phase_bonus(DR3_BOSS_PHASE_2) == DR3_BOSS_P2_BONUS, "phase 2 bonus $%d", dr3_boss_phase_bonus(DR3_BOSS_PHASE_2));
    CHECK(dr3_boss_phase_bonus(DR3_BOSS_PHASE_3) == 0, "phase 3 pays glory ($0)");

    CHECK(dr3_boss_next_phase(DR3_BOSS_PHASE_1, 1) == DR3_BOSS_PHASE_2, "win P1 -> P2");
    CHECK(dr3_boss_next_phase(DR3_BOSS_PHASE_2, 1) == DR3_BOSS_PHASE_3, "win P2 -> P3");
    CHECK(dr3_boss_next_phase(DR3_BOSS_PHASE_3, 1) == DR3_BOSS_PHASE_NONE, "win P3 -> complete");
    CHECK(dr3_boss_next_phase(DR3_BOSS_PHASE_1, 0) == DR3_BOSS_PHASE_NONE, "lose P1 -> reset");
    CHECK(dr3_boss_next_phase(DR3_BOSS_PHASE_2, 0) == DR3_BOSS_PHASE_NONE, "lose P2 -> reset");
    CHECK(dr3_boss_next_phase(DR3_BOSS_PHASE_3, 0) == DR3_BOSS_PHASE_NONE, "lose P3 -> reset");

    /* Phase 1: 1 player + 20 Vagabonds = 21 distinct, in-bounds spawns on 960x600 TRA */
    for (i = 0; i < DR3_BOSS_P1_CARS; ++i) {
        dr3_boss_swarm_spawn(i, 960, 600, &x, &y, &img);
        px[i] = x; py[i] = y;
        CHECK((x >= 50) && (x <= 910) && (y >= 50) && (y <= 550), "swarm[%d] in bounds: %d,%d", i, x, y);
        CHECK((img >= 0) && (img < 96), "swarm[%d] valid img: %d", i, img);
        for (j = 0; j < i; ++j) {
            const int dx = px[i] - px[j], dy = py[i] - py[j];
            CHECK((dx * dx + dy * dy) >= (42 * 42), "swarm[%d](%d,%d) too close to [%d](%d,%d)", i, px[i], py[i], j, px[j], py[j]);
        }
    }

    /* Phase 3: dramatic north-vs-south face-off across central hub */
    {
        int bx, by, bimg, mx, my, mimg;
        dr3_boss_phase3_spawn(1, 960, 600, &bx, &by, &bimg);
        dr3_boss_phase3_spawn(0, 960, 600, &mx, &my, &mimg);
        CHECK((bx == 480) && (mx == 480), "phase 3 cars centred horizontally: %d, %d", bx, mx);
        CHECK(by < 300 && my > 300, "boss north (%d) vs player south (%d)", by, my);
        CHECK((bimg >= 0 && bimg < 96) && (mimg >= 0 && mimg < 96), "phase 3 valid headings");
    }

    /* Phase 3 enrage & closing ring math */
    CHECK(!dr3_boss_p3_enraged(DR3_BOSS_P3_HP_MAX, DR3_BOSS_P3_HP_MAX), "full HP not enraged");
    CHECK(!dr3_boss_p3_enraged(DR3_BOSS_P3_HP_MAX / 2 + 1, DR3_BOSS_P3_HP_MAX), "just above 50 pct not enraged");
    CHECK(dr3_boss_p3_enraged(DR3_BOSS_P3_HP_MAX / 2, DR3_BOSS_P3_HP_MAX), "at 50 pct enraged");
    CHECK(dr3_boss_p3_enraged(1, DR3_BOSS_P3_HP_MAX), "1 HP enraged");
    CHECK(!dr3_boss_p3_enraged(0, DR3_BOSS_P3_HP_MAX), "dead boss not enraged");

    CHECK(dr3_boss_p3_ring_radius(0) == DR3_BOSS_P3_RING_START_R, "enrage tick 0 = start R");
    CHECK(dr3_boss_p3_ring_radius(DR3_BOSS_P3_RING_SHRINK_S * 70) == DR3_BOSS_P3_RING_END_R, "enrage end = 185");
    CHECK(dr3_boss_p3_ring_radius(DR3_BOSS_P3_RING_SHRINK_S * 70 + 999) == DR3_BOSS_P3_RING_END_R, "enrage ring clamped");

    /* Savegame persistence in racer_t.unk0[12] + backwards compatibility with old zeroed saves */
    memset(unk0, 0, sizeof(unk0));
    CHECK(dr3_boss_decode_save(unk0) == DR3_BOSS_PHASE_NONE, "old zeroed save decodes to NONE");

    dr3_boss_encode_save(unk0, DR3_BOSS_PHASE_2);
    CHECK(dr3_boss_decode_save(unk0) == DR3_BOSS_PHASE_2, "round-trip PHASE 2");
    dr3_boss_encode_save(unk0, DR3_BOSS_PHASE_3);
    CHECK(dr3_boss_decode_save(unk0) == DR3_BOSS_PHASE_3, "round-trip PHASE 3");
    dr3_boss_encode_save(unk0, DR3_BOSS_PHASE_NONE);
    CHECK(dr3_boss_decode_save(unk0) == DR3_BOSS_PHASE_NONE, "reset clears save tag");
}

static void test_bottom_ui(void)
{
    static uint16_t fb565[240 * 320 + 64];
    static uint8_t  mask[32 * 32];
    dr3_canvas_t    c;
    int             rows[12], n, i;

    printf("- bottom UI (race standings + Death Rally menu)\n");

    /* 1. Windowing helper: <= max_rows returns every row */
    n = dr3_ui_select_window(4, 2, 11, rows);
    CHECK(n == 4 && rows[0] == 0 && rows[3] == 3, "4 cars fit without windowing");

    /* 12 cars, player 1st -> top 11 */
    n = dr3_ui_select_window(12, 0, 11, rows);
    CHECK(n == 11 && rows[0] == 0 && rows[10] == 10, "12 cars player 1st -> 0..10");

    /* 12 cars, player 12th (idx 11) -> top 7, ellipsis (-1), then 9, 10, 11 */
    n = dr3_ui_select_window(12, 11, 11, rows);
    CHECK(n == 11, "12 cars player 12th returns 11 rows (got %d)", n);
    CHECK(rows[0] == 0 && rows[6] == 6 && rows[7] == -1, "top 7 + separator");
    CHECK(rows[8] == 9 && rows[9] == 10 && rows[10] == 11, "tail window includes player at 11 (%d,%d,%d)", rows[8], rows[9], rows[10]);

    /* 20 cars, player 15th (idx 14) -> top 7, -1, 13, 14, 15 */
    n = dr3_ui_select_window(20, 14, 11, rows);
    CHECK(n == 11 && rows[7] == -1 && rows[8] == 13 && rows[9] == 14 && rows[10] == 15,
          "20 cars player 15th centered in tail (%d,%d,%d)", rows[8], rows[9], rows[10]);

    /* 2. Sorting: race mode sorts by position; battle mode puts alive + boss/player ahead of wrecks */
    {
        dr3_ui_racer_t list[4];
        memset(list, 0, sizeof(list));
        snprintf(list[0].name, sizeof(list[0].name), "THIRD");  list[0].position = 3; list[0].hp = 50000;
        snprintf(list[1].name, sizeof(list[1].name), "FIRST");  list[1].position = 1; list[1].hp = 90000;
        snprintf(list[2].name, sizeof(list[2].name), "WRECK");  list[2].position = 4; list[2].hp = 0;
        snprintf(list[3].name, sizeof(list[3].name), "SECOND"); list[3].position = 2; list[3].hp = 80000; list[3].is_player = 1;

        dr3_ui_sort_racers(list, 4, 0);
        CHECK(list[0].position == 1 && list[1].position == 2 && list[2].position == 3 && list[3].position == 4,
              "race standings sorted 1..4");

        list[1].hp = 0; /* player wrecked */
        dr3_ui_sort_racers(list, 4, 1);
        CHECK(list[0].hp > 0 && list[1].hp > 0 && list[2].hp == 0 && list[3].hp == 0,
              "battle sort puts alive cars before wrecks");
    }

    /* 3. Render race dashboard (4-car, 12-car Escort, 2-car Boss Phase 3) into rotated RGB565 buffer
          with canary guard words at the end to prove zero out-of-bounds writes */
    memset(mask, 0x0f, sizeof(mask));
    CHECK(dr3_minimap_build(mask, NULL, NULL, 32, 32) == 1, "minimap build for bottom UI");

    for (i = 0; i < 64; ++i) fb565[240 * 320 + i] = 0xBEEFu;
    memset(fb565, 0, 240 * 320 * sizeof(uint16_t));
    c.px       = (uint8_t *)fb565 + (240 - 1) * 2;
    c.fmt      = DR3_CANVAS_RGB565;
    c.stride_x = 240;
    c.stride_y = -1;
    c.w        = 320;
    c.h        = 240;

    {
        dr3_ui_race_t r;
        dr3_map_car_t mc[12];
        int           lit = 0;

        memset(&r, 0, sizeof(r));
        memset(mc, 0, sizeof(mc));
        snprintf(r.title, sizeof(r.title), "PHASE 2/3: THE ESCORT");
        snprintf(r.clock_str, sizeof(r.clock_str), "LAP 0:18.42");
        r.mode_kind  = DR3_UI_RACE_BOSS_P2;
        r.player_pos = 12;
        r.player_lap = 1;
        r.total_laps = 9;
        r.speed_mph  = 155;
        r.hull_pct   = 88;
        r.turbo_pct  = 75;
        r.ammo_pct   = 90;
        r.weapons_on = 1;
        r.mines      = 8;
        r.n_racers   = 12;
        for (i = 0; i < 12; ++i) {
            snprintf(r.racers[i].name, sizeof(r.racers[i].name), "DRIVER %d", i + 1);
            r.racers[i].position  = i + 1;
            r.racers[i].lap       = 1;
            r.racers[i].hp        = 80000;
            r.racers[i].hp_max    = 102400;
            r.racers[i].color     = 0xFF4020u;
            r.racers[i].is_player = (i == 11);
            mc[i].x = (float)(4 + i * 2); mc[i].y = 16.0f; mc[i].valid = 1; mc[i].is_player = (i == 11);
        }

        dr3_ui_draw_race(&c, &r, mc, 12, 1);
        dr3_ui_draw_race(&c, &r, mc, 12, 0); /* incremental update */

        for (i = 0; i < 240 * 320; ++i) lit += (fb565[i] != 0);
        CHECK(lit > 60000, "race dashboard lit most pixels (%d)", lit);

        /* Also exercise 4-car classic & 2-car Boss Phase 3 paths */
        r.mode_kind = DR3_UI_RACE_NORMAL; r.n_racers = 4;
        dr3_ui_draw_race(&c, &r, mc, 4, 1);

        r.mode_kind = DR3_UI_RACE_BOSS_P3; r.n_racers = 2;
        r.racers[0].is_boss = 1; r.boss_hp = 120000; r.boss_hp_max = DR3_BOSS_P3_HP_MAX; r.boss_enraged = 1;
        dr3_ui_draw_race(&c, &r, mc, 2, 1);
    }

    /* 4. Render menu command center (unloaded main menu + loaded career) */
    {
        dr3_ui_menu_t m;
        int           lit = 0;

        memset(&m, 0, sizeof(m));
        snprintf(m.badge, sizeof(m.badge), "30TH CHAOS");
        snprintf(m.mode_name, sizeof(m.mode_name), "RANDOM");
        m.music_pct = 80; m.sfx_pct = 100; m.weapons_on = 1; m.shadows_on = 1; m.textures_on = 1;
        dr3_ui_draw_menu(&c, &m);

        m.has_career = 1;
        snprintf(m.player_name, sizeof(m.player_name), "CHAMPION");
        snprintf(m.car_name, sizeof(m.car_name), "DELIVERATOR");
        m.cash = 999999; m.rank = 1; m.wins = 12; m.races = 14;
        m.engine = 4; m.engine_max = 4; m.tires = 3; m.tires_max = 4; m.armor = 3; m.armor_max = 4; m.hull_pct = 92;
        m.has_mines = 1; m.has_spikes = 1; m.boss_phase = 2; m.chaos_active = 1;
        m.n_standings = 15;
        for (i = 0; i < 15; ++i) {
            snprintf(m.standings[i].name, sizeof(m.standings[i].name), "RACER %d", i + 1);
            m.standings[i].points    = 100 - i * 5;
            m.standings[i].color     = 0x20B0FFu;
            m.standings[i].is_player = (i == 12);
        }
        memset(fb565, 0, 240 * 320 * sizeof(uint16_t));
        dr3_ui_draw_menu(&c, &m);
        for (i = 0; i < 240 * 320; ++i) lit += (fb565[i] != 0);
        CHECK(lit > 60000, "menu command center lit most pixels (%d)", lit);
    }

    for (i = 0; i < 64; ++i) CHECK(fb565[240 * 320 + i] == 0xBEEFu, "canary intact at %d", i);
    dr3_minimap_reset();
}

static void test_cash(void)
{
    enum { W = 400, H = 400, WQ = W / 4, HQ = H / 4 };
    static unsigned char mask[W * H];
    static unsigned char lr1[WQ * HQ];
    int out_x[DR3_CASH_MAX], out_y[DR3_CASH_MAX];
    int avoid_x[2] = { 100, 140 }, avoid_y[2] = { 100, 100 };
    int pad_x[2]   = { 220, 300 }, pad_y[2]   = { 200, 300 };
    int n, i, j;

    printf("- off-line street cash ($) placement for normal races\n");

    /* center lane distance: lr1=0/16 off-track (-1); lr1=8 AI center (0); lr1=12/13 road shoulder (4/5) */
    CHECK(dr3_cash_lr1_dev(0) == -1,  "lr1=0 is off-track (-1)");
    CHECK(dr3_cash_lr1_dev(16) == -1, "lr1=16 is off-track (-1)");
    CHECK(dr3_cash_lr1_dev(8) == 0,   "lr1=8 is AI center line (dev 0)");
    CHECK(dr3_cash_lr1_dev(7) == 1,   "lr1=7 is inside AI lane (dev 1)");
    CHECK(dr3_cash_lr1_dev(12) == 4,  "lr1=12 is off-line shoulder (dev 4)");
    CHECK(dr3_cash_lr1_dev(2) == 6,   "lr1=2 is outer road edge (dev 6)");

    /* build synthetic track: outer border walls (<4), a walled-off unreachable plaza on the right (x >= 320),
       drivable road interior (x < 312), center strip lr1=8, road shoulders lr1=13 */
    memset(mask, 0x0f, sizeof(mask));
    for (i = 0; i < W; ++i) {
        for (j = 0; j < 20; ++j) {
            mask[j * W + i] = 0;
            mask[(H - 1 - j) * W + i] = 0;
        }
    }
    /* solid barrier wall at x = 312..319 separating an unreachable decorative plaza on x = 320..399 */
    for (j = 0; j < H; ++j) {
        for (i = 312; i < 320; ++i) mask[j * W + i] = 0;
    }
    memset(lr1, 8, sizeof(lr1));
    for (j = 0; j < HQ; ++j) {
        for (i = 0; i < WQ; ++i) {
            if ((i < 30) || (i >= 62)) lr1[j * WQ + i] = 13;
        }
    }

    CHECK(!dr3_cash_spot_ok(mask, W, H, 100, 20), "wall edge rejected");
    CHECK(dr3_cash_spot_ok(mask, W, H, 80, 120),  "open drivable spot accepted");

    n = dr3_cash_place(mask, lr1, W, H, avoid_x, avoid_y, 2, pad_x, pad_y, 2, out_x, out_y, DR3_CASH_MAX);
    CHECK(n >= 6, "placed %d off-line cash spots (want >= 6)", n);

    for (i = 0; i < n; ++i) {
        const int qx = out_x[i] >> 2, qy = out_y[i] >> 2;
        CHECK(out_x[i] < 312, "spot %d (%d,%d) must NOT be behind the barrier wall", i, out_x[i], out_y[i]);
        CHECK(dr3_cash_spot_ok(mask, W, H, out_x[i], out_y[i]), "spot %d (%d,%d) drivable", i, out_x[i], out_y[i]);
        CHECK(dr3_cash_lr1_dev(lr1[qy * WQ + qx]) >= 3, "spot %d (%d,%d) is off AI center line", i, out_x[i], out_y[i]);
        for (j = 0; j < i; ++j) {
            const int dx = out_x[i] - out_x[j], dy = out_y[i] - out_y[j];
            CHECK((dx * dx + dy * dy) >= (DR3_CASH_MIN_SEP * DR3_CASH_MIN_SEP),
                  "spots %d and %d spaced >= %d px", i, j, DR3_CASH_MIN_SEP);
        }
        for (j = 0; j < 2; ++j) {
            const int dx = out_x[i] - pad_x[j], dy = out_y[i] - pad_y[j];
            CHECK((dx * dx + dy * dy) >= (DR3_CASH_PAD_CLEAR * DR3_CASH_PAD_CLEAR), "clear of track pad %d", j);
        }
        for (j = 0; j < 2; ++j) {
            const int dx = out_x[i] - avoid_x[j], dy = out_y[i] - avoid_y[j];
            CHECK((dx * dx + dy * dy) >= (DR3_CASH_CAR_CLEAR * DR3_CASH_CAR_CLEAR), "clear of start car %d", j);
        }
    }

    /* pure AI-center-only track places 0 off-line cash spots */
    memset(lr1, 8, sizeof(lr1));
    CHECK(dr3_cash_place(mask, lr1, W, H, NULL, NULL, 0, NULL, NULL, 0, out_x, out_y, DR3_CASH_MAX) == 0,
          "center-line-only track places 0 off-line spots");

    /* street cash unit values & unconditional payout (kept even if lapped / last / leading points) */
    CHECK(dr3_cash_pickup_value(0, 20, 0) == 50,  "easy tier rank 20 -> $50");
    CHECK(dr3_cash_pickup_value(0, 1,  0) == 50,  "easy tier rank 1 -> $50");
    CHECK(dr3_cash_pickup_value(1, 20, 0) == 60,  "medium tier rank 20 -> $60");
    CHECK(dr3_cash_pickup_value(1, 15, 0) == 120, "medium tier rank 15 -> $120");
    CHECK(dr3_cash_pickup_value(1, 10, 0) == 200, "medium tier rank 10 -> $200");
    CHECK(dr3_cash_pickup_value(1, 5,  0) == 260, "medium tier rank 5 -> $260");
    CHECK(dr3_cash_pickup_value(2, 20, 0) == 80,  "hard tier rank 20 -> $80");
    CHECK(dr3_cash_pickup_value(2, 11, 0) == 150, "hard tier rank 11 -> $150");
    CHECK(dr3_cash_pickup_value(2, 6,  0) == 300, "hard tier rank 6 -> $300");
    CHECK(dr3_cash_pickup_value(2, 1,  0) == 500, "hard tier rank 1 -> $500");
    CHECK(dr3_cash_pickup_value(3, 1,  0) == 0,   "final glory tier (not leading) -> $0");
    CHECK(dr3_cash_pickup_value(0, 1,  1) == 400, "leading points -> $400 (0x190)");
    CHECK(dr3_cash_pickup_value(2, 1,  1) == 400, "leading points hard tier -> $400");
    CHECK(dr3_cash_pickup_value(3, 1,  1) == 400, "leading points final tier -> $400");
    CHECK(dr3_cash_pickup_value(1, 0,  0) == 260, "rank 0 clamped to 1 -> $260");
    CHECK(dr3_cash_pickup_value(2, 99, 0) == 80,  "rank 99 clamped to 20 -> $80");

    CHECK(dr3_cash_payout(0, 50) == 0,       "0 pickups pays $0");
    CHECK(dr3_cash_payout(-4, 50) == 0,      "negative pickups pays $0");
    CHECK(dr3_cash_payout(12, 0) == 0,       "0 unit value pays $0");
    CHECK(dr3_cash_payout(12, 50) == 600,    "12 easy street $ pickups -> $600");
    CHECK(dr3_cash_payout(13, 60) == 780,    "1 big cash (10) + 3 small ($) at $60 -> $780");
    CHECK(dr3_cash_payout(5, 400) == 2000,   "5 pickups while leading points -> $2000");

    /* 30th Anniversary keeps collected $ even if lapped/last; classic difficulties keep original rules */
    CHECK(dr3_cash_player_keeps(1, 0, 0) == 1, "30th Anniv: normal finish keeps street $");
    CHECK(dr3_cash_player_keeps(1, 1, 0) == 1, "30th Anniv: lapped keeps street $");
    CHECK(dr3_cash_player_keeps(1, 0, 1) == 1, "30th Anniv: last/wrecked keeps street $");
    CHECK(dr3_cash_player_keeps(1, 1, 1) == 1, "30th Anniv: lapped AND last keeps street $");
    CHECK(dr3_cash_player_keeps(0, 0, 0) == 1, "Classic: non-lapped top 3 keeps street $");
    CHECK(dr3_cash_player_keeps(0, 1, 0) == 0, "Classic: lapped forfeits street $");
    CHECK(dr3_cash_player_keeps(0, 0, 1) == 0, "Classic: 4th/last forfeits street $");
    CHECK(dr3_cash_player_keeps(0, 1, 1) == 0, "Classic: lapped AND 4th/last forfeits street $");

    /* Arena trashed grace window: ~3s wait before Race Over, cancelled if revived by repair pickup */
    CHECK(DR3_ARENA_SPECTATE_S == 3, "Arena trashed grace window is 3 seconds");
    CHECK(dr3_arena_player_trashed_out(0, 500, 0) == 0,     "no death timestamp -> not out");
    CHECK(dr3_arena_player_trashed_out(0, 500, 500) == 0,   "0 ticks trashed -> still in grace window");
    CHECK(dr3_arena_player_trashed_out(0, 500 + 209, 500) == 0, "2.98s trashed -> still in grace window");
    CHECK(dr3_arena_player_trashed_out(0, 500 + 210, 500) == 1, "3.00s trashed -> triggers Race Over");
    CHECK(dr3_arena_player_trashed_out(20480, 500 + 300, 500) == 0, "revived by repair pickup -> Race Over cancelled");

    /* VAI checkpoint alignment: guarantees placed grid cars never start facing backwards */
    {
        uint8_t vai[64 * 64];
        int     gx, gy;
        float   dir;

        /* checkpoints increase downward (y increasing: 1..32; 0 is off-track), forward is DOWN (dir=180) */
        for (gy = 0; gy < 64; ++gy) {
            for (gx = 0; gx < 64; ++gx) vai[gy * 64 + gx] = (uint8_t)(1 + (gy / 2));
        }

        dir = 180.0f; /* nose = 0 deg -> +y (downward, increasing checkpoints) */
        CHECK(dr3_grid_align_vai(vai, 64, 64, 32, 128.0f, 128.0f, &dir) == 0, "forward car left unchanged");
        CHECK(fabsf(dir - 180.0f) < 1.0f, "forward dir stays 180");

        dir = 0.0f;   /* nose = 180 deg -> -y (upward, decreasing checkpoints = WRONG WAY!) */
        CHECK(dr3_grid_align_vai(vai, 64, 64, 32, 128.0f, 128.0f, &dir) == 1, "backwards car flipped +180");
        CHECK(fabsf(dir - 180.0f) < 1.0f, "flipped dir becomes 180 (got %.1f)", (double)dir);

        /* wrap-around across finish line: checkpoint 31 -> 32 -> 1 -> 2 moving downward */
        for (gy = 0; gy < 64; ++gy) {
            uint8_t ck = (uint8_t)(1 + ((30 + (gy / 8)) % 32));
            for (gx = 0; gx < 64; ++gx) vai[gy * 64 + gx] = ck;
        }
        dir = 0.0f; /* facing upward across wrap-around = wrong way */
        CHECK(dr3_grid_align_vai(vai, 64, 64, 32, 128.0f, 72.0f, &dir) == 1, "wrap-around backwards car flipped");
        CHECK(fabsf(dir - 180.0f) < 1.0f, "wrap-around flipped dir becomes 180");
    }
}

static void test_anniv_gfx(void)
{
    static uint8_t scenecar_in[DR3_ANNIV_SCENECAR_SZ];
    static uint8_t scenecar_out[DR3_ANNIV_SCENECAR_SZ];
    static uint8_t carname_in[6 * DR3_ANNIV_CARNAME_SZ];
    static uint8_t carname_out[DR3_ANNIV_CARNAME_SZ];
    static uint8_t carres_in[DR3_ANNIV_CARRES_SZ];
    static uint8_t carres_out[DR3_ANNIV_CARRES_SZ];
    static uint8_t spin_frame[DR3_ANNIV_SCENECAR_SZ];
    int x, y, s, i;
    int obsidian_cnt = 0, stripe_cnt = 0, wing_cnt = 0, bg_preserved = 1;

    printf("anniv_gfx\n");

    /* Null safety */
    dr3_anniv_build_scenecar(NULL, NULL);
    dr3_anniv_build_carname(NULL, NULL);
    dr3_anniv_build_carres(NULL, NULL);
    dr3_anniv_tint_spin(NULL);

    /* Synthetic Deliverator scenecar: blueprint bg (0x24) + car body paint (0x40..0x5f) */
    memset(scenecar_in, 0x24, sizeof(scenecar_in));
    for (y = 18; y < 52; ++y) {
        for (x = 12; x < 90; ++x) {
            scenecar_in[y * DR3_ANNIV_SCENECAR_W + x] = (uint8_t)(0x40 + ((x + y) & 31));
        }
    }

    dr3_anniv_build_scenecar(scenecar_in, scenecar_out);

    /* Top-right corner far from car/wing should preserve blueprint grid background */
    for (y = 0; y < 4; ++y) {
        for (x = 80; x < 96; ++x) {
            if (scenecar_out[y * DR3_ANNIV_SCENECAR_W + x] != 0x24) bg_preserved = 0;
        }
    }
    CHECK(bg_preserved, "scenecar preserves blueprint background outside car/wing");

    /* Body pixels remapped to obsidian (0x01..0x0c) + live player-color stripes (0x40..0x5f) */
    for (y = 18; y < 52; ++y) {
        for (x = 12; x < 90; ++x) {
            uint8_t p = scenecar_out[y * DR3_ANNIV_SCENECAR_W + x];
            if (p >= 0x01 && p <= 0x0c) ++obsidian_cnt;
            if (p >= 0x40 && p <= 0x5f) ++stripe_cnt;
        }
    }
    CHECK(obsidian_cnt > 1500, "scenecar body has obsidian-carbon pixels (got %d)", obsidian_cnt);
    CHECK(stripe_cnt > 60,     "scenecar body has live player-color racing trim (got %d)", stripe_cnt);

    /* Rear wing drawn above car body (rows 5..14, cols 5..33) */
    for (y = 5; y <= 14; ++y) {
        for (x = 5; x <= 33; ++x) {
            if (scenecar_out[y * DR3_ANNIV_SCENECAR_W + x] != 0x24) ++wing_cnt;
        }
    }
    CHECK(wing_cnt > 80, "scenecar draws swept GT rear wing (got %d pixels)", wing_cnt);

    /* Carres 4 slices: each slice must remap its own 16-entry paint ramp (0x40, 0x50, 0xe0, 0xf0) */
    {
        static const uint8_t bases[4] = { 0x40, 0x50, 0xe0, 0xf0 };
        for (s = 0; s < 4; ++s) {
            uint8_t *sl = carres_in + s * DR3_ANNIV_CARRES_SLICE;
            memset(sl, 0x24, DR3_ANNIV_CARRES_SLICE);
            for (y = 13; y < 46; ++y) {
                for (x = 15; x < 93; ++x) {
                    sl[y * DR3_ANNIV_CARRES_W + x] = (uint8_t)(bases[s] + ((x + y) & 15));
                }
            }
        }
        dr3_anniv_build_carres(carres_in, carres_out);
        for (s = 0; s < 4; ++s) {
            const uint8_t *sl = carres_out + s * DR3_ANNIV_CARRES_SLICE;
            int obs = 0, trim = 0;
            for (i = 0; i < DR3_ANNIV_CARRES_SLICE; ++i) {
                if (sl[i] >= 0x01 && sl[i] <= 0x0c) ++obs;
                if ((int)sl[i] >= (int)bases[s] && (int)sl[i] < (int)bases[s] + 16) ++trim;
            }
            CHECK(obs > 1200, "carres slice %d has obsidian body (%d)", s, obs);
            CHECK(trim > 60,  "carres slice %d keeps its own live paint ramp 0x%02x (%d)", s, bases[s], trim);
        }
    }

    /* Carname: bezel from slot 4 + glyph strokes stamped across cols 6..89 */
    memset(carname_in, 0x04, sizeof(carname_in));
    /* Give slot 4 a distinct border value 0x09 and slots 0, 2, 5 bright glyph strokes 0x0f */
    for (x = 0; x < DR3_ANNIV_CARNAME_W; ++x) {
        carname_in[4 * DR3_ANNIV_CARNAME_SZ + x] = 0x09;
        carname_in[4 * DR3_ANNIV_CARNAME_SZ + 15 * DR3_ANNIV_CARNAME_W + x] = 0x09;
    }
    for (y = 4; y <= 11; ++y) {
        for (x = 4; x < 90; ++x) {
            carname_in[0 * DR3_ANNIV_CARNAME_SZ + y * DR3_ANNIV_CARNAME_W + x] = 0x0f;
            carname_in[2 * DR3_ANNIV_CARNAME_SZ + y * DR3_ANNIV_CARNAME_W + x] = 0x0f;
            carname_in[5 * DR3_ANNIV_CARNAME_SZ + y * DR3_ANNIV_CARNAME_W + x] = 0x0f;
        }
    }
    dr3_anniv_build_carname(carname_in, carname_out);
    CHECK(carname_out[0] == 0x09 && carname_out[15 * DR3_ANNIV_CARNAME_W] == 0x09,
          "carname preserves metallic bezel from slot 4");
    {
        int bright = 0;
        for (y = 4; y <= 11; ++y)
            for (x = 6; x < 90; ++x)
                if (carname_out[y * DR3_ANNIV_CARNAME_W + x] == 0x0f) ++bright;
        CHECK(bright > 400, "carname stamps ANNIVERSARY glyph pixels (%d)", bright);
    }

    /* Spin frame tint: preserves non-paint pixels, remaps 0x40..0x5f to obsidian + player trim + chrome */
    memset(spin_frame, 0x26, sizeof(spin_frame));
    for (i = 0; i < 32; ++i) spin_frame[100 + i] = (uint8_t)(0x40 + i);
    dr3_anniv_tint_spin(spin_frame);
    CHECK(spin_frame[0] == 0x26, "spin tint leaves blueprint bg untouched");
    CHECK(spin_frame[100 + 0] == 0x01 && spin_frame[100 + 18] == 0x04, "spin tint darkens body to obsidian");
    CHECK(spin_frame[100 + 25] >= 0x40 && spin_frame[100 + 25] <= 0x5f, "spin tint keeps live player-color contour trim");
    CHECK(spin_frame[100 + 31] >= 0x0c && spin_frame[100 + 31] <= 0x0f, "spin tint adds chrome specular peak");
}

static void test_anniv_prog(void)
{
    /* Standard starting 20-racer championship cars & ranks (player at seat 19: Vagabond, rank 20) */
    static const int cars[20]  = { 5,5,5, 4,4,4,4, 3,3,3, 2,2,2,2, 1,1,1, 0,0, 0 };
    static const int ranks[20] = { 1,2,3, 4,5,6,7, 8,9,10, 11,12,13,14, 15,16,17, 18,19, 20 };
    int out[20];
    int n, i, has_deliverator = 0, has_wraith = 0;
    const char *spk = NULL, *msg = NULL;
    int style = -1;

    printf("anniv_prog\n");

    /* Progression tier curve */
    CHECK(dr3_anniv_tier(0, 20) == 0, "Vagabond rank 20 -> tier 0 (Rookie)");
    CHECK(dr3_anniv_tier(1, 15) == 0, "Dervish rank 15 -> tier 0 (Rookie)");
    CHECK(dr3_anniv_tier(2, 18) == 1, "Sentinel -> tier 1 (Contender)");
    CHECK(dr3_anniv_tier(0, 13) == 1, "Rank 13 -> tier 1 (Contender)");
    CHECK(dr3_anniv_tier(3, 12) == 2, "Shrieker -> tier 2 (Elite)");
    CHECK(dr3_anniv_tier(2, 8)  == 2, "Rank 8 -> tier 2 (Elite)");
    CHECK(dr3_anniv_tier(4, 10) == 3, "Wraith -> tier 3 (Full Mayhem)");
    CHECK(dr3_anniv_tier(0, 5)  == 3, "Top 6 -> tier 3 (Full Mayhem)");

    /* Escalating grid size: 8 -> 12 -> 16 -> 20 (capped at 16 in Arena) */
    CHECK(dr3_anniv_field_size(0, 0) == 8  && dr3_anniv_field_size(0, 1) == 8,  "Tier 0: 8 cars (Race/DM/Arena)");
    CHECK(dr3_anniv_field_size(1, 0) == 12 && dr3_anniv_field_size(1, 1) == 12, "Tier 1: 12 cars (Race/DM/Arena)");
    CHECK(dr3_anniv_field_size(2, 0) == 16 && dr3_anniv_field_size(2, 1) == 16, "Tier 2: 16 cars (Race/DM/Arena)");
    CHECK(dr3_anniv_field_size(3, 0) == 20 && dr3_anniv_field_size(3, 1) == 16, "Tier 3: 20 cars (16 Arena)");

    /* Progressive AI pace & Boss invasion chance */
    CHECK(dr3_anniv_ai_difficulty_row(0) == 1, "Tier 0 AI pace = I Live to Ride");
    CHECK(dr3_anniv_ai_difficulty_row(1) == 2 && dr3_anniv_ai_difficulty_row(2) == 2, "Tier 1-2 AI pace = Petrol");
    CHECK(dr3_anniv_ai_difficulty_row(3) == 3, "Tier 3 AI pace = full Anniversary");
    CHECK(dr3_anniv_boss_chance(0) == 0,  "Tier 0: 0% Adversary invasion");
    CHECK(dr3_anniv_boss_chance(1) == 10, "Tier 1: 10% Adversary invasion");
    CHECK(dr3_anniv_boss_chance(2) == 18, "Tier 2: 18% Adversary invasion");
    CHECK(dr3_anniv_boss_chance(3) == 25, "Tier 3: 25% Adversary invasion");

    /* 1v1 Challenge duel laps: 3 early -> 4 mid-late -> 5 top tier */
    CHECK(dr3_anniv_duel_laps(0) == 3, "Tier 0 duel = 3 laps");
    CHECK(dr3_anniv_duel_laps(1) == 3, "Tier 1 duel = 3 laps");
    CHECK(dr3_anniv_duel_laps(2) == 4, "Tier 2 duel = 4 laps");
    CHECK(dr3_anniv_duel_laps(3) == 5, "Tier 3 duel = 5 laps");
    CHECK(DR3_CASH_MAX == 10, "Off-line street cash cap halved to 10");

    /* Underdog bounty */
    CHECK(dr3_anniv_underdog_bonus(0, 0) == 0,   "Same class -> $0 underdog bonus");
    CHECK(dr3_anniv_underdog_bonus(2, 1) == 0,   "Lower class -> $0 underdog bonus");
    CHECK(dr3_anniv_underdog_bonus(0, 1) == 150, "Vagabond vs Dervish -> +$150");
    CHECK(dr3_anniv_underdog_bonus(0, 5) == 750, "Vagabond vs Deliverator -> +$750");

    /* Speed/class-matched field selection: Vagabond Tier 0 gets 8 matched cars (0 Deliverators, 0 Wraiths!) */
    n = dr3_anniv_select_field(19, 8, cars, ranks, 20, out);
    CHECK(n == 8 && out[0] == 19, "Tier 0 field selects 8 cars with player at [0]");
    for (i = 0; i < n; ++i) {
        if (cars[out[i]] == 5) has_deliverator = 1;
        if (cars[out[i]] == 4) has_wraith = 1;
    }
    CHECK(!has_deliverator && !has_wraith, "Tier 0 Vagabond field excludes Deliverators and Wraiths");

    /* Tier 3 selects all 20 unique seats */
    n = dr3_anniv_select_field(19, 20, cars, ranks, 20, out);
    CHECK(n == 20 && out[0] == 19, "Tier 3 field selects all 20 racers");

    /* Mode wheel weighting */
    {
        int cnt[5] = { 0, 0, 0, 0, 0 };
        for (i = 0; i < 100; ++i) ++cnt[dr3_anniv_pick_mode(0, i)];
        CHECK(cnt[0] == 35 && cnt[1] == 30 && cnt[2] == 20 && cnt[4] == 15,
              "Tier 0 wheel weights: 35/30/20/15 (got %d/%d/%d/%d)", cnt[0], cnt[1], cnt[2], cnt[4]);
        memset(cnt, 0, sizeof(cnt));
        for (i = 0; i < 100; ++i) ++cnt[dr3_anniv_pick_mode(3, i)];
        CHECK(cnt[0] == 25 && cnt[1] == 25 && cnt[2] == 25 && cnt[4] == 25,
              "Tier 3 wheel weights: equal 25/25/25/25");
    }

    /* Shuffle-bag: every 4 spins visits all 4 modes, and never repeats back-to-back */
    {
        int prev = 0; /* simulate Race 1 = STANDARD */
        int round;
        dr3_anniv_mode_bag_reset(prev);
        for (round = 0; round < 20; ++round) {
            int seen[5] = { 0, 0, 0, 0, 0 };
            for (i = 0; i < 4; ++i) {
                int m = dr3_anniv_mode_bag_next((int (*)(void))rnd);
                CHECK(m != prev, "shuffle-bag round %d step %d never repeats mode %d", round, i, prev);
                CHECK(m == 0 || m == 1 || m == 2 || m == 4, "shuffle-bag returns valid mode (%d)", m);
                seen[m]++;
                prev = m;
            }
            CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1 && seen[4] == 1,
                  "shuffle-bag round %d contains all 4 modes once (%d/%d/%d/%d)",
                  round, seen[0], seen[1], seen[2], seen[4]);
        }
    }

    /* Radio callout state machine: uppercasing, duration, cooldown, urgent override */
    dr3_anniv_radio_clear();
    CHECK(dr3_anniv_radio_query(100, &spk, &msg, &style) == 0, "radio starts silent");
    CHECK(dr3_anniv_radio_push("Farmer Ted:", "Eat my dust!", DR3_ANNIV_RADIO_RIVAL, 0, 200) == 1, "normal push accepted");
    CHECK(dr3_anniv_radio_query(250, &spk, &msg, &style) == 1 && strcmp(spk, "FARMER TED:") == 0 && strcmp(msg, "EAT MY DUST!") == 0,
          "radio uppercases speaker & line");
    CHECK(dr3_anniv_radio_push("Pit Radio:", "Spam", DR3_ANNIV_RADIO_PIT, 0, 300) == 0, "normal spam blocked during cooldown");
    CHECK(dr3_anniv_radio_push("Pit Radio:", "Giant Slayer!", DR3_ANNIV_RADIO_GOLD, 1, 350) == 1, "urgent event overrides after 2s");
    CHECK(dr3_anniv_radio_query(350 + DR3_ANNIV_RADIO_HOLD_TICKS + 1, &spk, &msg, &style) == 0, "radio expires cleanly");
}

int main(void)
{
    printf("dRally 3DS port - host tests\n\n");
    test_blit_rot_5_4();
    test_mask_bounds();
    test_tint_mask_box();
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
    test_minimap_marker_colour();
    test_grid_pair_rule();
    test_grid_straight();
    test_grid_corner();
    test_grid_bend_back();
    test_grid_tight_start();
    test_grid_facing();
    test_grid_no_room();
    test_grid_off_road_slot();
    test_grid_standings_order();
    test_carcolour();
    test_laptime();
    test_payout();
    test_hp_line();
    test_dm();
    test_results_rows();
    test_arena();
    test_chaos_boss();
    test_boss_gauntlet();
    test_bottom_ui();
    test_cash();
    test_anniv_gfx();
    test_anniv_prog();


    printf("\n%d checks, %d failures\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
