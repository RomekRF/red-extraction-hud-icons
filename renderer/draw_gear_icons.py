# Copyright (c) 2026 RomekRF.
# SPDX-License-Identifier: MPL-2.0
"""Draws Red Extraction's own gear icons (flat white, transparent) as PNGs. The drawings are RomekRF's own, released
under CC BY 4.0 (see LICENSE-CC-BY-4.0.txt); this script that draws them is MPL-2.0.

    python draw_gear_icons.py                  128 px PNGs in drawn_icons/128
    python draw_gear_icons.py --size 256 --out drawn_icons/256   any size; drawn at 4x and reduced
"""
import argparse
import os

KEYS = ("medkit", "armor", "backpack_small", "backpack_medium", "backpack_large", "revive", "mission",
        "health_shot", "munitions_box", "armor_box", "att:mag", "att:optic", "att:muzzle",
        "loot:valuable", "loot:material", "loot:medical", "loot:part", "loot:document", "loot:supply",
        "jetpack_lift", "jetpack_survey", "jetpack_assault", "jetpack_nano")


def draw_gear(key, path, size=128, ss=4):
    """A flat white icon with cut-outs, drawn at `ss` times the size and reduced."""
    from PIL import Image, ImageDraw
    n = size * ss
    mask = Image.new("L", (n, n), 0)
    d = ImageDraw.Draw(mask)
    u = n / 512.0  # drawn on a 512 grid

    def box(x0, y0, x1, y1, scale=1.0):
        c = 256
        return [(c + (x0 - c) * scale) * u, (c + (y0 - c) * scale) * u, (c + (x1 - c) * scale) * u,
                (c + (y1 - c) * scale) * u]

    if key == "medkit":
        d.rounded_rectangle(box(200, 60, 312, 130), radius=24 * u, outline=255, width=int(22 * u))
        d.rounded_rectangle(box(56, 110, 456, 430), radius=48 * u, fill=255)
        d.rectangle(box(216, 170, 296, 370), fill=0)
        d.rectangle(box(156, 230, 356, 310), fill=0)
    elif key == "armor":
        d.polygon([(150 * u, 64 * u), (362 * u, 64 * u), (430 * u, 140 * u), (430 * u, 380 * u), (256 * u, 456 * u),
                   (82 * u, 380 * u), (82 * u, 140 * u)], fill=255)
        d.line([(160 * u, 150 * u), (352 * u, 150 * u)], fill=0, width=int(20 * u))
    elif key.startswith("backpack"):
        k = {"backpack_small": 0.72, "backpack_medium": 0.86, "backpack_large": 1.0}[key]
        d.rounded_rectangle(box(196, 40, 316, 150, k), radius=40 * u * k, outline=255, width=int(24 * u * k))
        d.rounded_rectangle(box(116, 110, 396, 470, k), radius=70 * u * k, fill=255)
        d.rounded_rectangle(box(166, 290, 346, 430, k), radius=30 * u * k, outline=0, width=int(18 * u * k))
        strap = box(166, 350, 346, 350, k)
        d.line([(strap[0], strap[1]), (strap[2], strap[3])], fill=0, width=int(14 * u * k))
    elif key == "revive":
        d.ellipse(box(48, 48, 464, 464), fill=255)
        d.line([(96 * u, 270 * u), (180 * u, 270 * u), (220 * u, 170 * u), (280 * u, 370 * u), (320 * u, 270 * u),
                (416 * u, 270 * u)], fill=0, width=int(30 * u), joint="curve")
    elif key == "mission":
        # A document with a folded corner and its lines cut out, as DMZ draws an intel item (slice M2).
        d.polygon([(112 * u, 48 * u), (320 * u, 48 * u), (400 * u, 128 * u), (400 * u, 464 * u), (112 * u, 464 * u)],
                  fill=255)
        d.line([(320 * u, 48 * u), (320 * u, 128 * u), (400 * u, 128 * u)], fill=0, width=int(18 * u), joint="curve")
        for y0, x1 in ((120, 264), (200, 344), (264, 344), (328, 344), (392, 280)):
            d.rounded_rectangle(box(168, y0, x1, y0 + 26), radius=10 * u, fill=0)
    elif key == "health_shot":
        # An auto-injector pen, drawn level and turned 35 degrees: needle, body with a cut window, cap, plunger.
        d.rectangle(box(52, 248, 112, 264), fill=255)
        d.rounded_rectangle(box(108, 206, 372, 306), radius=26 * u, fill=255)
        d.rounded_rectangle(box(176, 232, 300, 280), radius=10 * u, fill=0)
        d.rectangle(box(226, 240, 250, 272), fill=255)
        d.rectangle(box(222, 244, 254, 268), fill=255)
        d.rounded_rectangle(box(368, 220, 424, 292), radius=12 * u, fill=255)
        d.rounded_rectangle(box(420, 236, 468, 276), radius=8 * u, fill=255)
        mask = mask.rotate(35, resample=Image.BICUBIC, center=(n / 2, n / 2))
        d = ImageDraw.Draw(mask)
    elif key in ("munitions_box", "armor_box"):
        # A crate with its handle and lid line; rounds cut in the front, or a shield for the armour box.
        d.rounded_rectangle(box(196, 92, 316, 172), radius=22 * u, outline=255, width=int(22 * u))
        d.rounded_rectangle(box(64, 160, 448, 448), radius=28 * u, fill=255)
        d.line([(64 * u, 224 * u), (448 * u, 224 * u)], fill=0, width=int(16 * u))
        if key == "munitions_box":
            for x in (140, 234, 328):
                d.rounded_rectangle(box(x, 300, x + 46, 404), radius=6 * u, fill=0)
                d.polygon([(x * u, 304 * u), ((x + 46) * u, 304 * u), ((x + 23) * u, 254 * u)], fill=0)
        else:
            d.polygon([(186 * u, 258 * u), (326 * u, 258 * u), (326 * u, 340 * u), (256 * u, 412 * u),
                       (186 * u, 340 * u)], fill=0)
    elif key == "att:mag":
        # A curved box magazine with its baseplate and a window.
        d.polygon([(196 * u, 52 * u), (316 * u, 52 * u), (330 * u, 150 * u), (372 * u, 290 * u), (430 * u, 420 * u),
                   (300 * u, 468 * u), (250 * u, 330 * u), (210 * u, 160 * u)], fill=255)
        d.line([(286 * u, 448 * u), (420 * u, 398 * u)], fill=0, width=int(14 * u))
        d.rounded_rectangle(box(232, 92, 292, 126), radius=8 * u, fill=0)
    elif key == "att:optic":
        # A scope: eyepiece, tube with turrets, objective bell, mount.
        d.rounded_rectangle(box(48, 214, 112, 286), radius=10 * u, fill=255)
        d.rounded_rectangle(box(96, 222, 392, 278), radius=20 * u, fill=255)
        d.polygon([(380 * u, 214 * u), (464 * u, 176 * u), (464 * u, 324 * u), (380 * u, 286 * u)], fill=255)
        d.rectangle(box(222, 170, 282, 226), fill=255)
        d.rectangle(box(160, 274, 196, 334), fill=255)
        d.rectangle(box(300, 274, 336, 334), fill=255)
        d.rectangle(box(132, 326, 364, 356), fill=255)
        d.line([(170 * u, 250 * u), (300 * u, 250 * u)], fill=0, width=int(10 * u))
    elif key == "att:muzzle":
        # A suppressor: the thread, the can with two rings cut.
        d.rectangle(box(24, 228, 70, 284), fill=255)
        d.rounded_rectangle(box(62, 196, 456, 316), radius=44 * u, fill=255)
        for x in (150, 372):
            d.line([(x * u, 206 * u), (x * u, 306 * u)], fill=0, width=int(14 * u))
    elif key.startswith("jetpack_"):
        # A back plate with its straps, the tanks on it and a nozzle under each; the kind's own detail cut or added.
        d.rounded_rectangle(box(150, 120, 362, 360), radius=30 * u, fill=255)
        d.line([(200 * u, 120 * u), (200 * u, 70 * u)], fill=255, width=int(18 * u))
        d.line([(312 * u, 120 * u), (312 * u, 70 * u)], fill=255, width=int(18 * u))
        if key == "jetpack_survey":
            d.rounded_rectangle(box(176, 96, 336, 400), radius=70 * u, fill=255)
            d.line([(256 * u, 150 * u), (256 * u, 360 * u)], fill=0, width=int(14 * u))
            d.line([(340 * u, 110 * u), (420 * u, 40 * u)], fill=255, width=int(12 * u))
            d.ellipse(box(408, 28, 436, 56), fill=255)
            d.polygon([(206 * u, 400 * u), (306 * u, 400 * u), (256 * u, 476 * u)], fill=255)
        else:
            for x0 in (96, 316):
                d.rounded_rectangle(box(x0, 100, x0 + 100, 400), radius=48 * u, fill=255)
                d.line([((x0 + 50) * u, 150 * u), ((x0 + 50) * u, 360 * u)], fill=0, width=int(12 * u))
                d.polygon([((x0 + 14) * u, 400 * u), ((x0 + 86) * u, 400 * u), ((x0 + 50) * u, 470 * u)], fill=255)
            if key == "jetpack_assault":
                d.polygon([(96 * u, 300 * u), (40 * u, 420 * u), (96 * u, 380 * u)], fill=255)
                d.polygon([(416 * u, 300 * u), (472 * u, 420 * u), (416 * u, 380 * u)], fill=255)
                d.polygon([(226 * u, 360 * u), (286 * u, 360 * u), (256 * u, 470 * u)], fill=255)
            elif key == "jetpack_nano":
                import math
                hexagon = [((256 + 52 * math.cos(math.radians(60 * i + 30))) * u,
                            (240 + 52 * math.sin(math.radians(60 * i + 30))) * u) for i in range(6)]
                d.polygon(hexagon, fill=0)
                inner = [((256 + 28 * math.cos(math.radians(60 * i + 30))) * u,
                          (240 + 28 * math.sin(math.radians(60 * i + 30))) * u) for i in range(6)]
                d.polygon(inner, fill=255)
    elif key == "loot:valuable":
        # A cut gem: crown and pavilion, the facets cut.
        d.polygon([(150 * u, 110 * u), (362 * u, 110 * u), (448 * u, 200 * u), (256 * u, 440 * u), (64 * u, 200 * u)],
                  fill=255)
        d.line([(64 * u, 200 * u), (448 * u, 200 * u)], fill=0, width=int(14 * u))
        d.line([(200 * u, 110 * u), (170 * u, 200 * u), (256 * u, 440 * u), (342 * u, 200 * u), (312 * u, 110 * u)],
               fill=0, width=int(12 * u))
    elif key == "loot:material":
        # Three ingots stacked: two below, one across them.
        for x0, y0 in ((60, 300), (262, 300), (160, 160)):
            d.polygon([((x0 + 30) * u, y0 * u), ((x0 + 160) * u, y0 * u), ((x0 + 190) * u, (y0 + 110) * u),
                       (x0 * u, (y0 + 110) * u)], fill=255)
            d.line([((x0 + 34) * u, (y0 + 30) * u), ((x0 + 156) * u, (y0 + 30) * u)], fill=0, width=int(12 * u))
    elif key == "loot:medical":
        # A capsule turned 45 degrees, its halves split, a cross on the upper half.
        d.rounded_rectangle(box(56, 186, 456, 326), radius=70 * u, fill=255)
        d.line([(256 * u, 186 * u), (256 * u, 326 * u)], fill=0, width=int(16 * u))
        d.rectangle(box(132, 238, 196, 274), fill=0)
        d.rectangle(box(146, 224, 182, 288), fill=0)
        mask = mask.rotate(45, resample=Image.BICUBIC, center=(n / 2, n / 2))
        d = ImageDraw.Draw(mask)
    elif key == "loot:part":
        # A cog: eight teeth round a ring, the hub cut out.
        import math
        cx = cy = 256
        teeth = []
        for i in range(16):
            a = math.pi * 2 * i / 16
            r = 210 if i % 2 == 0 else 160
            for da in (-0.13, 0.13):
                teeth.append(((cx + r * math.cos(a + da)) * u, (cy + r * math.sin(a + da)) * u))
        d.polygon(teeth, fill=255)
        d.ellipse(box(176, 176, 336, 336), fill=0)
    elif key == "loot:document":
        # A clipboard: the board, its clip, the lines of a log cut out.
        d.rounded_rectangle(box(96, 72, 416, 464), radius=26 * u, fill=255)
        d.rounded_rectangle(box(186, 40, 326, 112), radius=18 * u, fill=0)
        d.rounded_rectangle(box(202, 52, 310, 100), radius=12 * u, fill=255)
        for y0, x1 in ((170, 352), (234, 352), (298, 352), (362, 280)):
            d.rounded_rectangle(box(160, y0, x1, y0 + 26), radius=10 * u, fill=0)
    elif key == "loot:supply":
        # A canister: its cap, the body, two bands cut.
        d.rounded_rectangle(box(200, 40, 312, 104), radius=14 * u, fill=255)
        d.rounded_rectangle(box(110, 96, 402, 468), radius=46 * u, fill=255)
        for y in (190, 380):
            d.line([(110 * u, y * u), (402 * u, y * u)], fill=0, width=int(18 * u))
    else:
        raise SystemExit(f"no drawing for {key}")
    mask = mask.resize((size, size), Image.LANCZOS)
    img = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    img.putalpha(mask)
    img.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=128)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "drawn_icons", "128"))
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    for key in KEYS:
        path = os.path.join(args.out, key.replace(":", "_") + ".png")
        draw_gear(key, path, size=args.size)
        print(key, "->", path)


if __name__ == "__main__":
    main()
