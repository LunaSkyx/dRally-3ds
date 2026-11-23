#!/usr/bin/env python3
"""
Generates authentic Death Rally 30th Anniversary Nintendo 3DS icon, banner PNG, and banner WAV audio:
  - assets/3ds/icon.png        (48x48 Home Menu & Homebrew Launcher large icon)
  - assets/3ds/icon_small.png  (24x24 small icon)
  - assets/3ds/banner.png      (256x128 3DS Home Menu banner artwork)
  - assets/3ds/banner.wav      (2.4s 22050Hz 16-bit mono V8 engine rev + rock power chord)
"""
import math
import os
import struct
import sys
import wave
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_DIR = os.path.join(REPO, "assets", "3ds")
EMBLEM_SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(OUT_DIR, "icon.png")
LOGO_SHOT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(REPO, "logs", "az_runs", "verify_shop_loaded", "01_loaded_shop.png")

os.makedirs(OUT_DIR, exist_ok=True)


def make_icons():
    emblem = Image.open(EMBLEM_SRC).convert("RGBA")

    # High-contrast sharpen slightly so the twin miniguns, flames & rivets pop at 48x48
    emblem = ImageEnhance.Contrast(emblem).enhance(1.12)
    emblem = ImageEnhance.Color(emblem).enhance(1.10)

    # 48x48 large Home Menu icon
    icon48 = emblem.resize((48, 48), Image.Resampling.LANCZOS)
    icon48 = icon48.filter(ImageFilter.UnsharpMask(radius=0.8, percent=130, threshold=2))

    # Crisp 1px burnished gold inner trim + fiery "DR" / "30" micro corner badge? Keep it pure & bold!
    d = ImageDraw.Draw(icon48)
    # Subtle fiery gold corner sparks on the metallic bezel
    d.rectangle([0, 0, 47, 47], outline=(40, 44, 52, 255))
    d.rectangle([1, 1, 46, 46], outline=(168, 178, 192, 255))
    # Subtle gold accent corners
    for (x, y) in [(1, 1), (46, 1), (1, 46), (46, 46)]:
        d.point((x, y), fill=(255, 212, 72, 255))
    icon48.save(os.path.join(OUT_DIR, "icon.png"))

    # 24x24 small icon (cropped slightly tighter on the car so it stays unmistakable at 24x24)
    w, h = emblem.size
    m = int(w * 0.08)
    tight = emblem.crop((m, m, w - m, h - m)).resize((24, 24), Image.Resampling.LANCZOS)
    tight = tight.filter(ImageFilter.UnsharpMask(radius=0.7, percent=140, threshold=2))
    d2 = ImageDraw.Draw(tight)
    d2.rectangle([0, 0, 23, 23], outline=(150, 160, 175, 255))
    tight.save(os.path.join(OUT_DIR, "icon_small.png"))
    print("Saved icon.png (48x48) and icon_small.png (24x24)")


def draw_pixel_text(img, x, y, text, color, shadow=(0, 0, 0, 255), scale=1):
    # Minimal crisp 5x7 uppercase bitmap font for clean banner subtitle
    glyphs = {
        ' ': [0, 0, 0, 0, 0, 0, 0],
        '-': [0, 0, 0, 0x1F, 0, 0, 0],
        '.': [0, 0, 0, 0, 0, 0x0C, 0x0C],
        '0': [0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E],
        '3': [0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E],
        'A': [0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
        'B': [0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E],
        'C': [0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E],
        'D': [0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C],
        'E': [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F],
        'F': [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10],
        'G': [0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E],
        'H': [0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
        'I': [0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E],
        'L': [0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F],
        'M': [0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11],
        'N': [0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11],
        'O': [0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
        'P': [0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10],
        'R': [0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11],
        'S': [0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E],
        'T': [0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04],
        'U': [0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
        'V': [0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04],
        'W': [0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11],
        'X': [0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11],
        'Y': [0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04],
        'Z': [0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F],
    }
    d = ImageDraw.Draw(img)
    cx = x
    for ch in text.upper():
        rows = glyphs.get(ch, glyphs[' '])
        for gy, bits in enumerate(rows):
            for gx in range(5):
                if bits & (0x10 >> gx):
                    px, py = cx + gx * scale, y + gy * scale
                    if shadow:
                        d.rectangle([px + 1, py + 1, px + scale, py + scale], fill=shadow)
                    d.rectangle([px, py, px + scale - 1, py + scale - 1], fill=color)
        cx += 6 * scale


def make_banner():
    W, H = 256, 128
    banner = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = banner.load()

    # Rounded dark crimson sunburst backdrop (authentic 1996 Death Rally menu palette)
    for y in range(H):
        for x in range(W):
            dx = (x - W * 0.65) / (W * 0.75)
            dy = (y - H * 0.55) / (H * 0.75)
            dist = math.sqrt(dx * dx + dy * dy)
            t = max(0.0, 1.0 - dist)
            ang = math.atan2(dy, dx)
            ray = 0.5 + 0.5 * math.sin(ang * 14.0)
            r = int(18 + t * (62 + 24 * ray))
            g = int(3 + t * (10 + 5 * ray))
            b = int(4 + t * 8)
            if y & 1:
                r = int(r * 0.86)
                g = int(g * 0.86)
                b = int(b * 0.86)
            px[x, y] = (min(255, r), min(255, g), min(255, b), 255)

    # Paste our awesome Death Rally car emblem on the right side (cropped cleanly inside its steel frame)
    emblem = Image.open(EMBLEM_SRC).convert("RGBA")
    ew, eh = emblem.size
    m = int(ew * 0.085)
    car_crop = emblem.crop((m, m, ew - m, eh - m)).resize((106, 106), Image.Resampling.LANCZOS)
    # Soft left/top/bottom fade mask so the car blends seamlessly into the crimson sunburst
    mask = Image.new("L", (106, 106), 255)
    mpx = mask.load()
    for y in range(106):
        vy = 1.0
        if y < 10:
            vy = y / 10.0
        elif y > 95:
            vy = (105 - y) / 10.0
        for x in range(106):
            vx = min(1.0, x / 30.0)
            mpx[x, y] = int(255 * vx * vy)
    banner.paste(car_crop, (W - 109, 11), mask)

    # Extract authentic fiery chrome DEATH RALLY logo straight from our lossless game screenshot
    if os.path.exists(LOGO_SHOT):
        shot = Image.open(LOGO_SHOT).convert("RGBA")
        # Top logo occupies y = 3..33 on the 400x240 top screen
        logo_raw = shot.crop((8, 3, 392, 33))
        # Scale nicely to fit the left/top banner area with crisp aspect
        logo = logo_raw.resize((164, 28), Image.Resampling.LANCZOS)
        logo = ImageEnhance.Contrast(logo).enhance(1.18)
        logo = logo.filter(ImageFilter.UnsharpMask(radius=0.8, percent=125, threshold=2))
        # Knock out pure/near-black background smoothly so the fiery chrome logo floats directly on the sunburst
        lpx = logo.load()
        lw, lh = logo.size
        for ly in range(lh):
            for lx in range(lw):
                r, g, b, _ = lpx[lx, ly]
                mx = max(r, g, b)
                if mx <= 14:
                    a = 0
                elif mx < 64:
                    a = int((mx - 14) * 255 / 50.0)
                else:
                    a = 255
                lpx[lx, ly] = (r, g, b, a)
        banner.paste(logo, (10, 20), logo)

    d = ImageDraw.Draw(banner)
    # Fiery copper-gold accent bars under the logo
    d.rectangle([10, 50, 170, 51], fill=(255, 200, 64, 255))
    d.rectangle([10, 52, 170, 52], fill=(196, 68, 18, 255))

    # Crisp Death Rally Edition subtitle + badges on the left
    draw_pixel_text(banner, 14, 60, "30TH ANNIVERSARY", (255, 218, 82, 255), scale=1)
    draw_pixel_text(banner, 14, 72, "EDITION - 3DS", (232, 238, 246, 255), scale=1)

    # Mini fiery pill bar (centered: 20 chars * 6 - 1 = 119px inside 136px box)
    d.rectangle([12, 88, 148, 107], fill=(12, 22, 34, 235), outline=(218, 152, 42, 255))
    draw_pixel_text(banner, 21, 94, "CHAOS - ARENA - BOSS", (96, 224, 255, 255), scale=1)

    # Signature Death Rally riveted gunmetal steel outer frame + corner brackets
    d.rectangle([0, 0, W - 1, H - 1], outline=(24, 28, 34, 255))
    d.rectangle([1, 1, W - 2, H - 2], outline=(98, 106, 118, 255))
    d.rectangle([2, 2, W - 3, H - 3], outline=(42, 46, 54, 255))

    # 4 corner steel brackets with rivets
    arm = 14
    steel_hi = (182, 192, 206, 255)
    steel_mid = (110, 118, 130, 255)
    rivet = (255, 214, 80, 255)
    # TL
    d.rectangle([0, 0, arm, 3], fill=steel_mid)
    d.rectangle([0, 0, 3, arm], fill=steel_mid)
    d.line([(0, 0), (arm, 0)], fill=steel_hi)
    d.line([(0, 0), (0, arm)], fill=steel_hi)
    d.rectangle([1, 1, 2, 2], fill=rivet)
    # TR
    d.rectangle([W - 1 - arm, 0, W - 1, 3], fill=steel_mid)
    d.rectangle([W - 4, 0, W - 1, arm], fill=steel_mid)
    d.line([(W - 1 - arm, 0), (W - 1, 0)], fill=steel_hi)
    d.rectangle([W - 3, 1, W - 2, 2], fill=rivet)
    # BL
    d.rectangle([0, H - 4, arm, H - 1], fill=steel_mid)
    d.rectangle([0, H - 1 - arm, 3, H - 1], fill=steel_mid)
    d.line([(0, H - 1 - arm), (0, H - 1)], fill=steel_hi)
    d.rectangle([1, H - 3, 2, H - 2], fill=rivet)
    # BR
    d.rectangle([W - 1 - arm, H - 4, W - 1, H - 1], fill=steel_mid)
    d.rectangle([W - 4, H - 1 - arm, W - 1, H - 1], fill=steel_mid)
    d.rectangle([W - 3, H - 3, W - 2, H - 2], fill=rivet)

    banner.save(os.path.join(OUT_DIR, "banner.png"))
    print("Saved banner.png (256x128)")


def make_banner_wav():
    """Synthesizes a clean 2.0s 22050Hz 16-bit mono plucked-guitar + 3DS chime banner jingle."""
    import random

    sr = 22050
    dur = 2.0
    n = int(sr * dur)
    rng = random.Random(1996)

    def ks_pluck(freq, length_s, bright=0.45, decay=0.996):
        nsamp = int(length_s * sr)
        period = max(2, int(round(sr / freq)))
        exc = [rng.uniform(-1.0, 1.0) for _ in range(period)]
        for _ in range(3):
            exc = [0.5 * exc[k] + 0.5 * exc[k - 1] for k in range(period)]
        mean = sum(exc) / float(period)
        exc = [v - mean for v in exc]
        buf = [0.0] * (nsamp + period)
        buf[:period] = exc
        inv_b = 1.0 - bright
        for i in range(period, nsamp + period):
            buf[i] = decay * (bright * buf[i - period] + inv_b * buf[i - period - 1])
        out = buf[:nsamp]
        att = int(0.004 * sr)
        for i in range(min(att, nsamp)):
            out[i] *= i / float(att)
        return out

    def bell_chime(freq, length_s, amp=0.16):
        nsamp = int(length_s * sr)
        w1 = 2.0 * math.pi * freq / sr
        w2 = w1 * 2.0
        w3 = w1 * 3.0
        out = [0.0] * nsamp
        for i in range(nsamp):
            t = i / float(sr)
            env = (t / 0.012 if t < 0.012 else 1.0) * math.exp(-t * 2.9)
            sig = (
                0.65 * math.sin(w1 * i)
                + 0.25 * math.sin(w2 * i) * math.exp(-t * 4.2)
                + 0.10 * math.sin(w3 * i) * math.exp(-t * 6.5)
            )
            out[i] = sig * env * amp
        return out

    mix = [0.0] * n

    def add_at(t0, sig, gain=1.0):
        i0 = int(t0 * sr)
        i1 = min(n, i0 + len(sig))
        for k in range(i1 - i0):
            mix[i0 + k] += sig[k] * gain

    # Crisp 4-step ascending D-minor / D5 rock-arcade guitar sting
    add_at(0.04, ks_pluck(146.83, 0.52, bright=0.45, decay=0.994), 0.52)
    add_at(0.04, ks_pluck(293.66, 0.46, bright=0.50, decay=0.993), 0.36)
    add_at(0.18, ks_pluck(174.61, 0.52, bright=0.45, decay=0.994), 0.50)
    add_at(0.18, ks_pluck(220.00, 0.46, bright=0.50, decay=0.993), 0.36)
    add_at(0.32, ks_pluck(220.00, 0.56, bright=0.48, decay=0.995), 0.50)
    add_at(0.32, ks_pluck(261.63, 0.50, bright=0.52, decay=0.994), 0.38)

    # Triumphant ringing D5 chord + soft 3DS crystal bell shimmer
    add_at(0.47, ks_pluck(146.83, 1.48, bright=0.42, decay=0.9975), 0.58)
    add_at(0.47, ks_pluck(220.00, 1.48, bright=0.45, decay=0.9968), 0.46)
    add_at(0.47, ks_pluck(293.66, 1.48, bright=0.48, decay=0.9965), 0.52)
    add_at(0.49, ks_pluck(440.00, 1.42, bright=0.50, decay=0.9958), 0.38)
    add_at(0.51, ks_pluck(587.33, 1.38, bright=0.46, decay=0.9955), 0.34)
    add_at(0.47, bell_chime(587.33, 1.48, amp=0.16), 1.0)
    add_at(0.49, bell_chime(880.00, 1.42, amp=0.08), 1.0)

    # Gentle high-pass (~60Hz) to keep tiny 3DS speakers crisp and free of DC/sub-bass mud
    hp = [0.0] * n
    alpha = 0.985
    for i in range(1, n):
        hp[i] = alpha * (hp[i - 1] + mix[i] - mix[i - 1])

    # Smooth fade-out over the last 0.28s
    fo = int(0.28 * sr)
    for k in range(fo):
        hp[n - fo + k] *= ((fo - 1 - k) / float(fo)) ** 1.5

    # Normalize to comfortable 3DS Home Menu peak (~0.64, soft-knee headroom, zero clipping)
    pk = max(abs(v) for v in hp) or 1.0
    scale = 0.64 / pk
    samples = [int(max(-32767, min(32767, v * scale * 32767.0))) for v in hp]

    wav_path = os.path.join(OUT_DIR, "banner.wav")
    with wave.open(wav_path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sr)
        wf.writeframes(struct.pack("<" + "h" * len(samples), *samples))
    print(f"Saved banner.wav ({dur:.1f}s, {sr}Hz mono 16-bit PCM)")


if __name__ == "__main__":
    make_icons()
    make_banner()
    make_banner_wav()
