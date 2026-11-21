"""make_arena.py - builds TRA.BPA, the track of the chaos mode ARENA (platform_3ds/dr3_arena.h), from scratch.

    python -I scripts/arena/make_arena.py <game dir> [--preview out.png]

What a track is (found by reading race___42824h.c / race___49bd4h.c and decoding TR1..TR9, doc/arena.md has the
long version) - a BPA with these entries, every .BPK compressed with the game's LZW (bpa.py):

    TRx-IMA.BPK   RIX3 picture: 10 byte header ("RIX3", w, h, 0xaf 0), a 768 byte palette (6 bit VGA) and w*h
                  pixels, row after row.  The art.
    TRx-MAS.BPK   the same layout; the pixels are the SURFACE MASK, the low nibble says what the ground is
                  (race___4c434h.c): 0..3 solid (a wall: cars bounce), 4 rough (-3 % speed), 5 a slope that pushes
                  right, 6 mud (-11 %), 7 gravel (-5 %), 0xa/0xc small pushes, 0xb boost strip (+1.3 %), 0xd oil
                  (the car swims), 0xe sand (-2.5 %), 0xf hard road (nothing).
    TRx-VAI.BPK   a quarter size picture (w/4 x h/4): the zone number of every 4x4 block - the checkpoints
                  of the lap counter and of the race AI.  The arena has no laps: all zero.
    TRx-LR1.BPK   a quarter size picture: how far the road is (the race AI keeps to it).  Unused: all zero.
    TRx-SHA.BPK   the shadow polygons of the track objects (two counts, then arrays) - none here.
    TRx-SCE.BPK   the 3D objects (buildings...) and their textures - none here (two zero bytes).
    TRx-INF.BIN   w, h, zone count, 4 start slots (x, y, rot: the car sprite, 3.75 deg a step), 16 pickup places
                  (x, y), 20 spectators (x, y, type, rot).  Only the first 12 pickup places are ever used.
    TRx-SKI/BLO/LIT.TAB, -DRV.DAT, -OHI.DAT, -FLIP.PAL   colour remaps, the race AI's per zone tuning, the palette
                  of the reversed track.

The palette: entries 0..85 are the game's own shared ones (HUD, the four car colour ramps at 15..54 that
race___4a25ch.c rebuilds, effects) - they are copied from TR1 of the game dir; 86..255 are this arena's own
colours, found by quantising the art.  The mask and the art are drawn from the same shapes below, so they
cannot disagree.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bpa import read_bpa, write_bpa, bpk_encode, bpk_decode  # noqa: E402
from PIL import Image, ImageDraw, ImageFilter, ImageChops  # noqa: E402

# ----------------------------------------------------------------------------------------------- the layout
W, H = 960, 600
CX, CY = W // 2, H // 2
SS = 2                                     # art is drawn at 2x and brought down: smooth edges
FIRST_OWN = 86                             # first palette entry the arena may paint with

# mask values (low nibble, race___4c434h.c)
M_SOLID, M_ROUGH, M_MUD, M_GRAVEL, M_BOOST, M_OIL, M_HARD = 0, 4, 6, 7, 0xB, 0xD, 0xF

WALL = 44                                  # outer wall thickness

# four fold symmetric: (x, y) given for the top left quadrant, mirrored by mirror4()
# Upgraded open-flow arena: wider corner exits and cleaner central combat ring so both
# the 1v20 Vagabond swarm and the 1.5x Giant Boss Deliverator maneuver freely without wedging.
BUNKERS = [  # (x0, y0, x1, y1) solid blocks, TL quadrant
    (226, 158, 326, 184),
    (226, 158, 252, 232),
]
PILLARS = [(376, 110, 18)]                 # (x, y, r)
BAY_SHIELDS = [(WALL, 214, 116, 234)]      # shorter shield: wide smooth exit from every corner bay
CENTER_R = 52
MUD = []                                   # no slowdown surfaces: the fight is not about the floor
OIL = [(332, 300, 26, 18)]                 # (x, y, rx, ry), mirrored in x
BOOST_NS = [(456, 108, 504, 174)]          # N/S strips, mirrored in y
BOOST_EW = [(108, 276, 198, 324)]          # E/W strips, mirrored in x
GRAVEL = []                                # (was a rough patch at the middle of the long walls)

# the places the 16 pickups spawn at (x, y); every one is checked to be free
# inner ring pickups stay comfortably inside the Phase 3 closing ring (r = 185)
PICKUPS = [
    (480, 206), (480, 394), (392, 300), (568, 300),          # round the centre (inside Phase 3 final ring)
    (286, 132), (674, 132), (286, 468), (674, 468),          # the quadrants
    (480, 82),  (480, 518), (146, 300), (814, 300),          # the middles of the four sides
    (406, 212), (554, 212), (406, 388), (554, 388),          # the inner ring diagonals (inside Phase 3 final ring)
]

# the spawn bay of every corner: anchor and the four slots (inwards), cars face the centre
BAY_ANCHOR = (88, 88)
BAY_SLOT_STEP = 56
SPAWN_SLOTS = 4                            # per corner: 16 places in all


def mirror4(r):
    """a rectangle (x0,y0,x1,y1) of the top left quadrant -> the four of the arena"""
    x0, y0, x1, y1 = r
    return [(x0, y0, x1, y1), (W - x1, y0, W - x0, y1), (x0, H - y1, x1, H - y0), (W - x1, H - y1, W - x0, H - y0)]


def mirror_x(r):
    x0, y0, x1, y1 = r
    return [(x0, y0, x1, y1), (W - x1, y0, W - x0, y1)]


def mirror_y(r):
    x0, y0, x1, y1 = r
    return [(x0, y0, x1, y1), (x0, H - y1, x1, H - y0)]


def spawn_places():
    """(x, y, img) for 4 corners x SPAWN_SLOTS - the ones dr3_arena.h repeats"""
    import math
    out = []
    ax, ay = BAY_ANCHOR
    for corner in range(4):
        sx = 1 if corner in (0, 2) else -1
        sy = 1 if corner in (0, 1) else -1
        for k in range(SPAWN_SLOTS):
            x = (ax if sx > 0 else W - ax) + sx * BAY_SLOT_STEP * (k % 2)
            y = (ay if sy > 0 else H - ay) + sy * BAY_SLOT_STEP * (k // 2)
            dx, dy = CX - x, CY - y
            deg = math.degrees(math.atan2(-dx, -dy)) % 360.0          # Direction: see doc/arena.md
            out.append((x, y, int(round(deg / 3.75)) % 96))
    return out


# --------------------------------------------------------------------------------------------- drawing helpers
class Arena:
    def __init__(self):
        self.mask = Image.new('L', (W, H), M_HARD)
        self.md = ImageDraw.Draw(self.mask)
        self.art = Image.new('RGB', (W * SS, H * SS), (60, 56, 54))
        self.ad = ImageDraw.Draw(self.art, 'RGBA')

    # --- floor
    def floor(self):
        a, d = self.art, self.ad
        # the Bayer-like repeating grain keeps the LZW happy and the floor from looking flat
        bayer = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
        for y in range(0, H * SS, SS):
            for x in range(0, W * SS, SS):
                g = bayer[(y // SS) & 3][(x // SS) & 3] - 7
                d.rectangle([x, y, x + SS - 1, y + SS - 1], fill=(64 + g, 59 + g, 56 + g))
        # a soft pool of light in the middle, darker rim
        glow = Image.new('L', (W * SS, H * SS), 0)
        gd = ImageDraw.Draw(glow)
        gd.ellipse([CX * SS - 330 * SS, CY * SS - 230 * SS, CX * SS + 330 * SS, CY * SS + 230 * SS], fill=110)
        glow = glow.filter(ImageFilter.GaussianBlur(70 * SS // 2))
        warm = Image.new('RGB', a.size, (118, 90, 66))
        a.paste(Image.composite(warm, a, glow.point(lambda v: v * 3 // 5)))
        self.ad = ImageDraw.Draw(self.art, 'RGBA')
        d = self.ad
        # plates: a seam every 64 px and rivets at the crossings
        for gx in range(WALL, W - WALL + 1, 64):
            d.line([gx * SS, WALL * SS, gx * SS, (H - WALL) * SS], fill=(30, 27, 26, 150), width=SS)
        for gy in range(WALL, H - WALL + 1, 64):
            d.line([WALL * SS, gy * SS, (W - WALL) * SS, gy * SS], fill=(30, 27, 26, 150), width=SS)
        for gx in range(WALL, W - WALL + 1, 64):
            for gy in range(WALL, H - WALL + 1, 64):
                d.ellipse([gx * SS - 2, gy * SS - 2, gx * SS + 2, gy * SS + 2], fill=(150, 140, 125, 180))

    # --- surfaces (mask value + art)
    def patch_rect(self, r, m, col, stripes=None):
        x0, y0, x1, y1 = r
        self.md.rectangle([x0, y0, x1 - 1, y1 - 1], fill=m)
        self.ad.rectangle([x0 * SS, y0 * SS, x1 * SS, y1 * SS], fill=col)
        if stripes:
            for k in range(x0 - (y1 - y0), x1, 10):
                self.ad.line([max(x0, k) * SS, y1 * SS - (max(x0, k) - k) * SS if False else y1 * SS,
                              min(x1, k + (y1 - y0)) * SS, y0 * SS + (0 if k + (y1 - y0) <= x1 else 0)],
                             fill=stripes, width=SS)

    def patch_ellipse(self, e, m, col, rim=None):
        x, y, rx, ry = e
        self.md.ellipse([x - rx, y - ry, x + rx, y + ry], fill=m)
        self.ad.ellipse([(x - rx) * SS, (y - ry) * SS, (x + rx) * SS, (y + ry) * SS], fill=col)
        if rim:
            self.ad.ellipse([(x - rx) * SS, (y - ry) * SS, (x + rx) * SS, (y + ry) * SS], outline=rim, width=SS)

    def boost(self, r, vertical):
        x0, y0, x1, y1 = r
        self.md.rectangle([x0, y0, x1 - 1, y1 - 1], fill=M_BOOST)
        d = self.ad
        d.rectangle([x0 * SS, y0 * SS, x1 * SS, y1 * SS], fill=(14, 62, 92, 255))
        # chevrons pointing along the strip
        step = 14
        if vertical:
            for yy in range(y0 + 6, y1 - 6, step):
                d.polygon([((x0 + 6) * SS, (yy + 8) * SS), (((x0 + x1) // 2) * SS, yy * SS), ((x1 - 6) * SS, (yy + 8) * SS),
                           ((x1 - 6) * SS, (yy + 14) * SS), (((x0 + x1) // 2) * SS, (yy + 6) * SS), ((x0 + 6) * SS, (yy + 14) * SS)],
                          fill=(70, 200, 240, 255))
        else:
            for xx in range(x0 + 6, x1 - 6, step):
                d.polygon([((xx + 8) * SS, (y0 + 6) * SS), (xx * SS, ((y0 + y1) // 2) * SS), ((xx + 8) * SS, (y1 - 6) * SS),
                           ((xx + 14) * SS, (y1 - 6) * SS), ((xx + 6) * SS, ((y0 + y1) // 2) * SS), ((xx + 14) * SS, (y0 + 6) * SS)],
                          fill=(70, 200, 240, 255))
        d.rectangle([x0 * SS, y0 * SS, x1 * SS, y1 * SS], outline=(120, 230, 255, 255), width=SS)

    # --- solid things: a block throws a shadow, has a lit top with a bevel
    def block(self, r, hazard=False):
        x0, y0, x1, y1 = r
        self.md.rectangle([x0, y0, x1 - 1, y1 - 1], fill=M_SOLID)
        d = self.ad
        for k, a in ((14, 40), (9, 50), (5, 60)):                    # soft drop shadow to the lower right
            d.rectangle([(x0 + 3) * SS, (y0 + 3) * SS, (x1 + k) * SS, (y1 + k) * SS], fill=(0, 0, 0, a))
        d.rectangle([x0 * SS, y0 * SS, x1 * SS, y1 * SS], fill=(86, 92, 100, 255))
        d.rectangle([(x0 + 3) * SS, (y0 + 3) * SS, (x1 - 3) * SS, (y1 - 3) * SS], fill=(112, 120, 130, 255))
        d.rectangle([(x0 + 6) * SS, (y0 + 6) * SS, (x1 - 6) * SS, (y1 - 6) * SS], fill=(100, 108, 118, 255))
        d.line([x0 * SS, y0 * SS, x1 * SS, y0 * SS], fill=(190, 198, 208, 255), width=SS * 2)
        d.line([x0 * SS, y0 * SS, x0 * SS, y1 * SS], fill=(170, 178, 188, 255), width=SS * 2)
        d.line([x0 * SS, y1 * SS, x1 * SS, y1 * SS], fill=(40, 44, 50, 255), width=SS * 2)
        d.line([x1 * SS, y0 * SS, x1 * SS, y1 * SS], fill=(48, 52, 58, 255), width=SS * 2)
        if hazard:
            self.hazard_band(x0, y1 - 14, x1, y1 - 6)

    def hazard_band(self, x0, y0, x1, y1):
        d = self.ad
        d.rectangle([x0 * SS, y0 * SS, x1 * SS, y1 * SS], fill=(24, 24, 24, 255))
        for k in range(x0 - (y1 - y0), x1, 16):
            d.polygon([(max(x0, k) * SS, y1 * SS), (min(x1, k + 8) * SS, y1 * SS),
                       (min(x1, k + 8 + (y1 - y0)) * SS, y0 * SS), (max(x0, k + (y1 - y0)) * SS, y0 * SS)],
                      fill=(232, 190, 30, 255))

    def disc(self, x, y, r, col_top=(112, 120, 130)):
        self.md.ellipse([x - r, y - r, x + r, y + r], fill=M_SOLID)
        d = self.ad
        for k, a in ((12, 40), (8, 50), (4, 60)):
            d.ellipse([(x - r + 3) * SS, (y - r + 3) * SS, (x + r + k) * SS, (y + r + k) * SS], fill=(0, 0, 0, a))
        d.ellipse([(x - r) * SS, (y - r) * SS, (x + r) * SS, (y + r) * SS], fill=(86, 92, 100, 255))
        d.ellipse([(x - r + 3) * SS, (y - r + 3) * SS, (x + r - 3) * SS, (y + r - 3) * SS], fill=col_top + (255,))
        d.ellipse([(x - r + 8) * SS, (y - r + 8) * SS, (x + r - 8) * SS, (y + r - 8) * SS], fill=(100, 108, 118, 255))
        d.arc([(x - r + 2) * SS, (y - r + 2) * SS, (x + r - 2) * SS, (y + r - 2) * SS], 180, 270, fill=(200, 208, 218, 255), width=SS * 2)

    def outer_wall(self):
        t = WALL
        d = self.ad
        self.md.rectangle([0, 0, W - 1, t - 1], fill=M_SOLID)
        self.md.rectangle([0, H - t, W - 1, H - 1], fill=M_SOLID)
        self.md.rectangle([0, 0, t - 1, H - 1], fill=M_SOLID)
        self.md.rectangle([W - t, 0, W - 1, H - 1], fill=M_SOLID)
        for r in ((0, 0, W, t), (0, H - t, W, H), (0, 0, t, H), (W - t, 0, W, H)):
            d.rectangle([r[0] * SS, r[1] * SS, r[2] * SS, r[3] * SS], fill=(70, 76, 84, 255))
        # inner lip: lit on the lower and right edges of the top/left walls, shaded on the others
        d.rectangle([4 * SS, 4 * SS, (W - 4) * SS, (H - 4) * SS], outline=(110, 118, 128, 255), width=SS * 3)
        d.rectangle([(t - 8) * SS, (t - 8) * SS, (W - t + 8) * SS, (H - t + 8) * SS], outline=(46, 50, 56, 255), width=SS * 8)
        d.rectangle([(t - 3) * SS, (t - 3) * SS, (W - t + 3) * SS, (H - t + 3) * SS], outline=(190, 198, 208, 255), width=SS * 2)
        # hazard band along the floor edge of every wall
        self.hazard_band(t, t, W - t, t + 8)
        self.hazard_band(t, H - t - 8, W - t, H - t)
        for (x0, y0, x1, y1) in ((t, t, t + 8, H - t), (W - t - 8, t, W - t, H - t)):
            for yy in range(y0, y1, 16):
                d.polygon([(x0 * SS, yy * SS), (x1 * SS, (yy + 8) * SS), (x1 * SS, (yy + 16) * SS), (x0 * SS, (yy + 8) * SS)],
                          fill=(232, 190, 30, 255))
            d.rectangle([x0 * SS, y0 * SS, x1 * SS, y1 * SS], outline=(24, 24, 24, 255), width=SS)
        # the hazard band is part of the wall for the cars? no - it is paint on the floor (mask stays hard)

    def pads(self):
        """the four spawn pads: a coloured ring and plate under the slots, and the centre emblem"""
        d = self.ad
        cols = [(200, 50, 40), (50, 110, 210), (60, 170, 70), (220, 170, 30)]
        ax, ay = BAY_ANCHOR
        for corner in range(4):
            sx = 1 if corner in (0, 2) else -1
            sy = 1 if corner in (0, 1) else -1
            cx = (ax if sx > 0 else W - ax) + sx * BAY_SLOT_STEP // 2
            cy = (ay if sy > 0 else H - ay) + sy * BAY_SLOT_STEP // 2
            c = cols[corner]
            for r, a in ((92, 60), (80, 90)):
                d.rounded_rectangle([(cx - r) * SS, (cy - r) * SS, (cx + r) * SS, (cy + r) * SS], radius=14 * SS,
                                    fill=c + (a,))
            d.rounded_rectangle([(cx - 92) * SS, (cy - 92) * SS, (cx + 92) * SS, (cy + 92) * SS], radius=14 * SS,
                                outline=c + (255,), width=SS * 3)
            # a marked parking slot under every car of the corner
            for k in range(SPAWN_SLOTS):
                px = (ax if sx > 0 else W - ax) + sx * BAY_SLOT_STEP * (k % 2)
                py = (ay if sy > 0 else H - ay) + sy * BAY_SLOT_STEP * (k // 2)
                d.rectangle([(px - 24) * SS, (py - 24) * SS, (px + 24) * SS, (py + 24) * SS], outline=c + (255,), width=SS * 2)
                d.rectangle([(px - 20) * SS, (py - 20) * SS, (px + 20) * SS, (py + 20) * SS], fill=(20, 20, 20, 70))
        # the emblem: concentric orange rings round the centre pillar
        for r, a in ((150, 36), (118, 52), (92, 70)):
            d.ellipse([(CX - r) * SS, (CY - r) * SS, (CX + r) * SS, (CY + r) * SS], fill=(235, 110, 30, a))
        d.ellipse([(CX - 150) * SS, (CY - 150) * SS, (CX + 150) * SS, (CY + 150) * SS], outline=(250, 160, 50, 255), width=SS * 3)
        for k in range(8):
            import math
            a0 = math.radians(k * 45 + 22.5)
            d.line([(CX + 70 * math.cos(a0)) * SS, (CY + 70 * math.sin(a0)) * SS,
                    (CX + 146 * math.cos(a0)) * SS, (CY + 146 * math.sin(a0)) * SS], fill=(250, 160, 50, 255), width=SS * 2)

    def stains(self):
        d = self.ad
        import random
        rnd = random.Random(7)
        for _ in range(26):
            x, y = rnd.randint(WALL + 20, W - WALL - 20), rnd.randint(WALL + 20, H - WALL - 20)
            r = rnd.randint(10, 30)
            d.ellipse([(x - r) * SS, (y - r * 0.6) * SS, (x + r) * SS, (y + r * 0.6) * SS], fill=(20, 18, 18, 60))

    def pickup_marks(self):
        """a faint ring under every pickup place so the player sees where things appear"""
        d = self.ad
        for (x, y) in PICKUPS:
            d.ellipse([(x - 14) * SS, (y - 14) * SS, (x + 14) * SS, (y + 14) * SS], outline=(220, 200, 90, 150), width=SS * 2)


def build():
    A = Arena()
    A.floor()
    A.stains()
    A.pads()
    A.outer_wall()
    # surfaces under the solid things
    for r in GRAVEL:
        for rr in mirror_x(r):
            A.patch_rect(rr, M_GRAVEL, (118, 100, 76, 255))
            A.patch_rect(rr, M_GRAVEL, (118, 100, 76, 255))
    for r in MUD:
        for rr in mirror_y(r):
            A.patch_rect(rr, M_MUD, (58, 42, 28, 255))
            A.ad.rectangle([rr[0] * SS, rr[1] * SS, rr[2] * SS, rr[3] * SS], outline=(98, 72, 44, 255), width=SS * 2)
    for e in OIL:
        for xs in (1, -1):
            for ys in (1, -1):
                ex = e[0] if xs > 0 else W - e[0]
                ey = e[1] + ys * 46
                A.patch_ellipse((ex, ey, e[2], e[3]), M_OIL, (14, 10, 22, 255), rim=(70, 40, 110, 255))
    for r in BOOST_NS:
        for rr in mirror_y(r):
            A.boost(rr, vertical=True)
    for r in BOOST_EW:
        for rr in mirror_x(r):
            A.boost(rr, vertical=False)
    A.pickup_marks()
    # solid things on top
    for r in BUNKERS:
        for rr in mirror4(r):
            A.block(rr, hazard=True)
    for r in BAY_SHIELDS:
        for rr in mirror4(r):
            A.block(rr, hazard=True)
    for (x, y, rad) in PILLARS:
        for (mx_, my_) in ((x, y), (W - x, y), (x, H - y), (W - x, H - y)):
            A.disc(mx_, my_, rad)
    A.disc(CX, CY, CENTER_R)
    return A


def check_layout(mask):
    """everything that must be driveable is - a wrong number here is a bug in the layout, not in the engine"""
    px = mask.load()
    bad = []
    for (x, y) in PICKUPS:
        for dx in (-9, 0, 9):
            for dy in (-9, 0, 9):
                if (px[x + dx, y + dy] & 15) < 4:
                    bad.append(('pickup', x, y))
    for (x, y, img) in spawn_places():
        for dx in (-22, 0, 22):
            for dy in (-22, 0, 22):
                if (px[x + dx, y + dy] & 15) < 4:
                    bad.append(('spawn', x, y))
    return bad


def hdr(w, h):
    return b'RIX3' + struct.pack('<HH', w, h) + b'\xaf\x00'


def build_shape(mask, menu_pal_raw):
    """Build TRA-SHP.BPK (360x274, MENU.PAL indices 2..47): the 3D chrome-slab loading screen preview matching
    TSHAPE01..19.BPK in MENU.BPA, with our arena's exact layout embossed on the top face and CHAOS ARENA on the front."""
    import math
    from PIL import ImageFont

    SW, SH = 360, 274
    # MENU.PAL indices 2..15 = metallic greys, 34..47 = blueprint cyan-blue ramp (exact range of TSHAPE01..19.BPK)
    grey_idxs = list(range(2, 16))
    blue_idxs = list(range(34, 48))

    def clamp(v, lo, hi):
        return lo if v < lo else hi if v > hi else v

    def grey_idx(lum):
        return grey_idxs[clamp(int(round(lum * (len(grey_idxs) - 1))), 0, len(grey_idxs) - 1)]

    def blue_idx(lum):
        return blue_idxs[clamp(int(round(lum * (len(blue_idxs) - 1))), 0, len(blue_idxs) - 1)]

    # --- heightmap of the arena (960x600): solid walls/bunkers/pillars/hub are raised (1.0), floor is recessed (0.0)
    mpx = mask.load()
    hmap = Image.new('L', (W, H), 0)
    hpx = hmap.load()
    for y in range(H):
        for x in range(W):
            if (mpx[x, y] & 15) < 4:
                hpx[x, y] = 255
    # blurred heightmap produces smooth 3D bevel slopes on every wall, bunker, shield, pillar and the centre hub
    hblur = hmap.filter(ImageFilter.GaussianBlur(5))
    hb = hblur.load()

    # subtle floor details (centre emblem ring + corner bays)
    detail = Image.new('L', (W, H), 0)
    dd = ImageDraw.Draw(detail)
    dd.ellipse([CX - 150, CY - 150, CX + 150, CY + 150], outline=180, width=5)
    dd.ellipse([CX - 118, CY - 118, CX + 118, CY + 118], outline=120, width=3)
    ax, ay = BAY_ANCHOR
    for corner in range(4):
        sx = 1 if corner in (0, 2) else -1
        sy = 1 if corner in (0, 1) else -1
        cx = (ax if sx > 0 else W - ax) + sx * BAY_SLOT_STEP // 2
        cy = (ay if sy > 0 else H - ay) + sy * BAY_SLOT_STEP // 2
        dd.rounded_rectangle([cx - 88, cy - 88, cx + 88, cy + 88], radius=12, outline=150, width=4)
    dpx = detail.load()

    # --- front-face embossed title ("CHAOS ARENA") rendered at 2x then height-bevelled
    FW, FH = 310, 34
    text_mask = Image.new('L', (FW * 2, FH * 2), 0)
    td = ImageDraw.Draw(text_mask)
    font = None
    for fname in (r'C:\Windows\Fonts\arialbd.ttf', r'C:\Windows\Fonts\impact.ttf', r'C:\Windows\Fonts\verdanab.ttf'):
        if os.path.exists(fname):
            font = ImageFont.truetype(fname, 48)
            break
    if font is None:
        font = ImageFont.load_default()
    label = 'CHAOS ARENA'
    bbox = td.textbbox((0, 0), label, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    # render tight, then stretch horizontally across the slab face like Remedy's TSHAPE lettering
    tight = Image.new('L', (max(1, tw + 8), max(1, th + 8)), 0)
    ImageDraw.Draw(tight).text((4 - bbox[0], 4 - bbox[1]), label, fill=255, font=font)
    stretched = tight.resize((FW * 2 - 12, FH * 2 - 10), Image.LANCZOS)
    text_mask.paste(stretched, (6, 5))
    text_small = text_mask.resize((FW, FH), Image.LANCZOS)
    text_blur = text_small.filter(ImageFilter.GaussianBlur(1.4))
    tp = text_small.load()
    tb = text_blur.load()

    # --- assemble 360x274 indexed image
    out = bytearray(SW * SH)
    bayer = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))

    # top & bottom metallic border rows (matching TSHAPE19.BPK's exact 20px top / 18px bottom chrome frame)
    top_bar = [4, 6, 4, 3, 3, 3, 3, 3, 3, 3, 4, 3, 4, 4, 5, 4, 5, 5, 7, 10]
    bot_bar = [9, 6, 4, 3, 3, 3, 4, 3, 4, 4, 5, 4, 5, 5, 7, 10, 7, 4]

    # slab geometry (matches TSHAPE01 / TSHAPE19 proportions)
    Y_TOP, Y_MID, Y_BOT = 58, 192, 228
    TOP_X0, TOP_X1 = 48.0, 312.0
    MID_X0, MID_X1 = 10.0, 350.0
    BOT_X0, BOT_X1 = 22.0, 338.0

    for y in range(SH):
        row_off = y * SW
        if y < len(top_bar):
            col = top_bar[y]
            for x in range(SW):
                c = col
                if x < 2 or x >= SW - 2:
                    c = max(2, c - 1)
                out[row_off + x] = c
            continue
        if y >= SH - len(bot_bar):
            col = bot_bar[y - (SH - len(bot_bar))]
            for x in range(SW):
                c = col
                if x < 2 or x >= SW - 2:
                    c = max(2, c - 1)
                out[row_off + x] = c
            continue

        # 1. Blueprint cyan-blue grid background (bright near top/bottom bars, darker in middle)
        d_edge = min(y - len(top_bar), (SH - len(bot_bar) - 1) - y)
        bg_lum = 0.04 + 0.56 * math.exp(-d_edge / 11.0)
        gy_line = ((y - 20) % 9 == 0)
        for x in range(SW):
            gx_line = (x % 9 == 0)
            lum = bg_lum
            if gx_line and gy_line:
                lum += 0.30
            elif gx_line or gy_line:
                lum += 0.14
            out[row_off + x] = blue_idx(lum)

        # 2. Top perspective face of the 3D chrome slab (with our arena embossed on it)
        if Y_TOP <= y < Y_MID:
            v = (y - Y_TOP) / float(Y_MID - Y_TOP)
            # mild perspective foreshortening along v
            vp = (v * 0.72) / (1.0 - 0.28 * v)
            lx = TOP_X0 + (MID_X0 - TOP_X0) * v
            rx = TOP_X1 + (MID_X1 - TOP_X1) * v
            ix0 = max(0, int(math.floor(lx)))
            ix1 = min(SW - 1, int(math.ceil(rx)))
            for x in range(ix0, ix1 + 1):
                u = (x - lx) / max(1.0, (rx - lx))
                if u < 0.0 or u > 1.0:
                    continue
                # map slab interior (with a gleaming chrome bezel rim) to arena coordinates 0..W-1, 0..H-1
                mx_f = (u - 0.045) / 0.91
                my_f = (vp - 0.055) / 0.89
                # metallic chrome sheen gradient across the slab
                sheen = 0.78 - 0.34 * u - 0.16 * v + 0.22 * math.sin((u * 2.4 - v * 1.1) * math.pi)
                sheen = clamp(sheen, 0.32, 0.98)

                if 0.0 <= mx_f < 1.0 and 0.0 <= my_f < 1.0:
                    ax_i = clamp(int(mx_f * (W - 1)), 1, W - 2)
                    ay_i = clamp(int(my_f * (H - 1)), 1, H - 2)
                    solid = (mpx[ax_i, ay_i] & 15) < 4
                    # directional bevel lighting from heightmap gradient (light from top-left)
                    gx = (hb[ax_i + 1, ay_i] - hb[ax_i - 1, ay_i]) / 255.0
                    gy = (hb[ax_i, ay_i + 1] - hb[ax_i, ay_i - 1]) / 255.0
                    slope = (gx * 0.75 + gy * 0.65) * 2.8
                    if solid:
                        # raised chrome wall / bunker / pillar / centre hub
                        lum = clamp(sheen + slope * 0.55 + 0.08, 0.12, 1.0)
                    else:
                        # recessed dark asphalt arena floor (with subtle Bayer grain like TSHAPE road)
                        grain = (bayer[y & 3][x & 3] - 7.5) * 0.008
                        emb = (dpx[ax_i, ay_i] / 255.0) * 0.16
                        # edge bevel where wall drops into floor
                        bevel = clamp(slope * 0.48, -0.18, 0.55)
                        lum = clamp(0.15 + grain + emb + bevel, 0.02, 0.85)
                else:
                    # outer chrome slab bevel border
                    lum = sheen
                    if y <= Y_TOP + 1 or x <= ix0 + 1:
                        lum = min(1.0, lum + 0.22)
                    elif x >= ix1 - 1:
                        lum = max(0.10, lum - 0.25)

                out[row_off + x] = grey_idx(lum)

        # 3. Front vertical face of the slab with embossed "CHAOS ARENA" chrome lettering
        elif Y_MID <= y < Y_BOT:
            v = (y - Y_MID) / float(Y_BOT - Y_MID)
            lx = MID_X0 + (BOT_X0 - MID_X0) * v
            rx = MID_X1 + (BOT_X1 - MID_X1) * v
            ix0 = max(0, int(math.floor(lx)))
            ix1 = min(SW - 1, int(math.ceil(rx)))
            ty = clamp(int(v * (FH - 1)), 1, FH - 2)
            for x in range(ix0, ix1 + 1):
                u = (x - lx) / max(1.0, (rx - lx))
                if u < 0.0 or u > 1.0:
                    continue
                # brushed chrome front face (bright highlight edge at top, subtle metallic sheen across width)
                base = 0.68 - 0.18 * v + 0.16 * math.sin((u * 1.9 + 0.2) * math.pi)
                if y == Y_MID:
                    base = min(1.0, base + 0.25)
                elif y >= Y_BOT - 2:
                    base = max(0.08, base - 0.22)

                tx = clamp(int(u * (FW - 1)), 1, FW - 2)
                ink = tp[tx, ty] / 255.0
                if ink > 0.15:
                    # deeply carved dark metallic letters with gleaming specular bevel on bottom-right edge
                    gx = (tb[tx + 1, ty] - tb[tx - 1, ty]) / 255.0
                    gy = (tb[tx, ty + 1] - tb[tx, ty - 1]) / 255.0
                    bevel = (gx * 0.7 + gy * 0.7) * 1.4
                    lum = clamp(0.16 + bevel + 0.12 * (1.0 - ink), 0.0, 0.95)
                else:
                    # bright outer lip right below/right of the carved letters
                    gx = (tb[tx + 1, ty] - tb[tx - 1, ty]) / 255.0
                    gy = (tb[tx, ty + 1] - tb[tx, ty - 1]) / 255.0
                    rim = max(0.0, -(gx * 0.7 + gy * 0.7)) * 0.55
                    lum = clamp(base + rim, 0.08, 1.0)

                out[row_off + x] = grey_idx(lum)

    return bytes(out)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    gamedir = sys.argv[1]
    preview = None
    if '--preview' in sys.argv:
        preview = sys.argv[sys.argv.index('--preview') + 1]

    A = build()
    art = A.art.resize((W, H), Image.LANCZOS)
    mask = A.mask

    bad = check_layout(mask)
    if bad:
        print('LAYOUT PROBLEMS:', bad)
        return 2

    base = read_bpa(os.path.join(gamedir, 'TR1.BPA'))
    base_pal = bpk_decode(base['TR1-IMA.BPK'])[10:10 + 768]
    menu_bpa_path = os.path.join(gamedir, 'MENU.BPA')
    menu_pal = read_bpa(menu_bpa_path)['MENU.PAL'] if os.path.exists(menu_bpa_path) else bytes(768)
    shp = build_shape(mask, menu_pal)

    own = FIRST_OWN
    q = art.quantize(colors=256 - own, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    qpal = q.getpalette()[:(256 - own) * 3]
    pal = bytearray(base_pal)
    for i in range(256 - own):
        for c in range(3):
            pal[3 * (own + i) + c] = min(63, qpal[3 * i + c] >> 2)
    idx = bytes(b + own for b in q.tobytes())

    # what the palette really shows (6 bit rounding included) - for the preview
    rgb_pal = [(pal[3 * i] * 255 // 63, pal[3 * i + 1] * 255 // 63, pal[3 * i + 2] * 255 // 63) for i in range(256)]

    def nearest(rgb, lo=own):
        best, bd = lo, 1 << 30
        for i in range(lo, 256):
            p = rgb_pal[i]
            dd = (p[0] - rgb[0]) ** 2 + (p[1] - rgb[1]) ** 2 + (p[2] - rgb[2]) ** 2
            if dd < bd:
                best, bd = i, dd
        return best

    # the skid mark table: every colour -> the nearest darker colour of the arena's own set
    ski = bytearray(range(256))
    for i in range(256):
        if i >= own:
            r, g, b = rgb_pal[i]
            ski[i] = nearest((r * 45 // 100, g * 45 // 100, b * 45 // 100))
        else:
            ski[i] = i
    ident = bytes(range(256))

    ima = hdr(W, H) + bytes(pal) + idx
    mas = hdr(W, H) + bytes(pal) + mask.tobytes()
    qw, qh = W // 4, H // 4
    vai = hdr(qw, qh) + bytes(768) + bytes(qw * qh)
    lr1 = hdr(qw, qh) + bytes(768) + bytes(qw * qh)

    sp = spawn_places()
    inf = struct.pack('<3I', W, H, 20)
    for k in range(4):
        x, y, img = sp[k * SPAWN_SLOTS]
        inf += struct.pack('<3I', x, y, img)
    for k in range(16):
        x, y = PICKUPS[k] if k < len(PICKUPS) else (0, 0)
        inf += struct.pack('<2I', x, y)
    for k in range(20):
        inf += struct.pack('<4I', 1, 1, 0, 1)
    assert len(inf) == 508, len(inf)

    sha = struct.pack('<2I', 0, 0)
    sce = b'\x00\x00'

    entries = [
        ('TRA-SCE.BPK', bpk_encode(sce)),
        ('TRA-SHA.BPK', bpk_encode(sha)),
        ('TRA-LIT.TAB', ident),
        ('TRA-SKI.TAB', bytes(ski)),
        ('TRA-LR1.BPK', bpk_encode(lr1)),
        ('TRA-MAS.BPK', bpk_encode(mas)),
        ('TRA-IMA.BPK', bpk_encode(ima)),
        ('TRA-VAI.BPK', bpk_encode(vai)),
        ('TRA-BLO.TAB', ident),
        ('TRA-INF.BIN', inf),
        ('TRA-OHI.DAT', base['TR1-OHI.DAT']),
        ('TRA-DRV.DAT', base['TR1-DRV.DAT']),
        ('TRA-FLIP.PAL', bytes(pal)),
        ('TRA-SHP.BPK', bpk_encode(shp)),
    ]
    out = os.path.join(gamedir, 'TRA.BPA')
    write_bpa(out, entries)
    for k, v in entries:
        print('  %-14s %7d bytes' % (k, len(v)))
    big = max(len(bpk_encode(ima)), len(bpk_encode(mas)))
    print('TRA.BPA written: %s (%d bytes); the largest entry is %d of the 307200 the loader buffer holds' %
          (out, os.path.getsize(out), big))
    assert big < 300000, 'the picture does not fit the loader buffer (___1a54d0h, 0x4b000 bytes)'

    # a roundtrip through the game's decoder, plus the preview (art as the game will draw it, mask beside it)
    chk = bpk_decode(read_bpa(out)['TRA-IMA.BPK'])
    assert chk[:10 + 768] == (hdr(W, H) + bytes(pal)) and chk[0x30a:] == idx, 'IMA roundtrip failed'
    if preview:
        shown = Image.frombytes('P', (W, H), idx)
        shown.putpalette(b''.join(bytes(c) for c in rgb_pal))
        mcol = {0: (0, 0, 0), 4: (255, 255, 0), 6: (150, 90, 30), 7: (200, 180, 120), 0xB: (0, 200, 255),
                0xD: (160, 40, 200), 0xF: (70, 70, 70)}
        mi = Image.new('RGB', (W, H))
        mp = mask.load()
        pp = mi.load()
        for y in range(H):
            for x in range(W):
                pp[x, y] = mcol.get(mp[x, y] & 15, (255, 0, 255))
        d = ImageDraw.Draw(mi)
        for (x, y) in PICKUPS:
            d.ellipse([x - 8, y - 8, x + 8, y + 8], outline=(255, 255, 255))
        for (x, y, img) in spawn_places():
            d.rectangle([x - 4, y - 4, x + 4, y + 4], outline=(255, 80, 80))
        sheet = Image.new('RGB', (W * 2 + 8, H), (20, 20, 20))
        sheet.paste(shown.convert('RGB'), (0, 0))
        sheet.paste(mi, (W + 8, 0))
        sheet.save(preview)
        print('preview:', preview)
    return 0


if __name__ == '__main__':
    sys.exit(main())
