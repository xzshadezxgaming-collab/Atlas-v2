#!/usr/bin/env python3
"""Builds Atlas's sprite pack from the hand-authored pixel art below.

Every sprite is a grid of palette characters ('.' = transparent). Most
sprites are authored as fill colours only and get a dark outline added
automatically, so shapes stay consistent and edits stay small. The
output is 24-bit BMPs keyed on magenta (what Texture2D expects) in
Assets/Sprites, plus Assets/Sprites/sprites.ini with each sprite's file
and pivot.

    python3 Tools/art/make_sprites.py [--preview sheet.png]

Requires Pillow.
"""

import argparse
import os
import sys

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_DIR = os.path.join(ROOT, "Assets", "Sprites")

KEY = (255, 0, 255)
OUTLINE = (24, 22, 27)

# Shared colours. Individual sprites add or override entries.
BASE = {
    "k": OUTLINE,
    # gunmetal ramp, dark to light
    "1": (36, 36, 42),
    "2": (52, 53, 61),
    "3": (70, 72, 83),
    "4": (98, 101, 115),
    "5": (138, 142, 156),
    "6": (182, 186, 198),
    # wood
    "w": (82, 60, 40),
    "W": (112, 82, 54),
    "y": (142, 106, 66),
    # olive drab
    "o": (60, 70, 46),
    "O": (82, 94, 62),
    "p": (108, 122, 82),
    "P": (136, 150, 104),
    # red / orange / yellow
    "r": (128, 38, 32),
    "R": (184, 58, 44),
    "e": (226, 100, 60),
    "Y": (214, 170, 52),
    "Z": (248, 214, 110),
    # glass and lights
    "c": (96, 170, 206),
    "C": (186, 236, 252),
    "g": (110, 220, 120),
    "b": (70, 120, 210),
    # tan canvas
    "t": (118, 104, 74),
    "T": (156, 140, 100),
    "u": (186, 170, 126),
    # white
    "h": (236, 236, 240),
}


def pad(rows, width=None):
    width = width or max(len(r) for r in rows)
    out = []
    for r in rows:
        if len(r) > width:
            raise ValueError("row too long (%d > %d): %r" % (len(r), width, r))
        out.append(r + "." * (width - len(r)))
    return out


def outline(rows, diagonal=False):
    """Adds a 1px OUTLINE ring around every filled pixel (grid grows by 2)."""
    h = len(rows)
    w = len(rows[0])
    grid = [["."] * (w + 2) for _ in range(h + 2)]
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            grid[y + 1][x + 1] = ch
    res = [r[:] for r in grid]
    n4 = [(1, 0), (-1, 0), (0, 1), (0, -1)]
    n8 = n4 + [(1, 1), (1, -1), (-1, 1), (-1, -1)]
    for y in range(h + 2):
        for x in range(w + 2):
            if grid[y][x] != ".":
                continue
            for dx, dy in (n8 if diagonal else n4):
                yy, xx = y + dy, x + dx
                if 0 <= yy < h + 2 and 0 <= xx < w + 2 and grid[yy][xx] not in ".k":
                    res[y][x] = "k"
                    break
    return ["".join(r) for r in res]


def overlay(rows, x0, y0, block):
    """Writes block over rows at (x0, y0); spaces keep what's there."""
    rows = [list(r) for r in rows]
    for dy, line in enumerate(block):
        for dx, ch in enumerate(line):
            if ch != " ":
                rows[y0 + dy][x0 + dx] = ch
    return ["".join(r) for r in rows]


def pixels(img):
    return [img.getpixel((x, y)) for y in range(img.height) for x in range(img.width)]


def to_image(rows, palette):
    rows = pad(rows)
    img = Image.new("RGB", (len(rows[0]), len(rows)), KEY)
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch == ".":
                continue
            if ch not in palette:
                raise KeyError("no colour for %r" % ch)
            img.putpixel((x, y), palette[ch])
    return img


SPRITES = []  # (name, image, pivot_x, pivot_y)


def add(name, rows, palette=None, pivot=(0, 0), auto_outline=True, diagonal=False):
    pal = dict(BASE)
    pal.update(palette or {})
    rows = pad(rows)
    px, py = pivot
    if auto_outline:
        rows = outline(rows, diagonal)
        px += 1
        py += 1
    SPRITES.append((name, to_image(rows, pal), px, py))


# ---------------------------------------------------------------------------
# Bodies (32x40, facing right). The torso/arm/hip layout matches the
# original Player.bmp: shoulders at y=23, hips at y=38, face on the right.
# ---------------------------------------------------------------------------

RIFLEMAN = [
    "................................",
    "................................",
    "..........kkkkkkkkkkkkkkkk......",
    "..........kaaaaaaaabbbbbbk......",
    "........kkkbbcccccbbbbbbbkkk....",
    "........kcccccccccbbbbbddddk....",
    "........kccccbbbbbbbbbbddddk....",
    "........kcccaabbbbbbbbbddddk....",
    "........kccccbbbbbbbbbbddddk....",
    "........kbbbbbbbbbbbbbbddddkk...",
    "........kbbbbbbbbbbbbbbbbbbbk...",
    "........kdddddddddddddddddddk...",
    "........kkkkkeeeffffffeeeekkk...",
    "............keeeffffffeeeek.....",
    "............keeeeeffffghiik.....",
    "............keeefeffffggiik.....",
    "...kkkkkkkk.keeeeefffffiiek.....",
    "...kjjllllk.keeeffffffffffk.....",
    "...kjjmmllk.keeefffffffeefk.....",
    "...kjjmmllkkkeeeeeeeeeeeeek.....",
    "...kjjmmnoookkkkeeeeeekkkkk.....",
    "...kjjjjnoookkkkeeeeeekkkk......",
    "...kjjmlnoppqqqqqqqqqqqqqk......",
    "...kjjllnoppqrrqqqqqqqqqqk......",
    "...kjjllnoppssrrssssssttpk......",
    "...kjjllnoppsssrrsssssttpk......",
    "...kjjjjnoppssssrrssssttpk......",
    "...kjjmlnoppsssssrrsssttpk......",
    "...kjjllnoppssssssuuuuutpk......",
    "...kjjllnoppsuuuusvvvvvtpk......",
    "...kjjllwwppsvvvvsvvxvvtpk......",
    "...kjjllwwppsvxvvsvvvvvtpk......",
    "...kkyyywwppsvvvvsvvvvvrsk......",
    "....kyyywwppsssssssssssrrk......",
    "....kyyyykzzzzzzxxxzzzzzzk......",
    "....kkyykkzzzzzzxxxzzzzzzk......",
    ".....kkkkkAABBBBBBBBBBBBBk......",
    ".........kAABBBBBBBBBBBBBk......",
    ".........kAABBBBBBBBBBBBBk......",
    ".........kAABBBBBBBBBBBBBk......",
]

BODY_PAL = {
    "a": (150, 165, 118), "b": (92, 108, 70), "c": (122, 140, 92),
    "d": (64, 76, 50), "e": (178, 128, 86), "f": (216, 166, 112),
    "g": (28, 28, 34), "h": (240, 240, 244), "i": (232, 188, 134),
    "j": (62, 66, 74), "l": (88, 92, 100), "m": (114, 118, 128),
    "n": (52, 60, 42), "o": (66, 76, 52), "p": (70, 80, 56),
    "q": (118, 130, 92), "r": (54, 48, 42), "s": (98, 110, 76),
    "t": (110, 122, 86), "u": (104, 92, 70), "v": (84, 74, 56),
    "w": (56, 50, 42), "x": (168, 158, 120), "y": (50, 52, 58),
    "z": (48, 44, 40), "A": (66, 70, 82), "B": (86, 90, 104),
}


def body_rifleman():
    add("body_rifleman", RIFLEMAN, BODY_PAL, pivot=(0, 0), auto_outline=False)


def body_scout():
    rows = list(RIFLEMAN)
    # Soft cap with a long brim and goggles instead of the helmet.
    rows = overlay(rows, 8, 0, [
        "......................",
        "......................",
        "......................",
        "......................",
        ".......kkkkkkkkk......",
        "......kTTTTTTTuuk.....",
        ".....kTTTTTTTTTTTk....",
        ".....kttTTTTTTTTTk....",
        "....kGGGGGGGGGGcCGk...",
        "....kGGGGGGGGGGccGk...",
        "....kttttttttttttttkk.",
        "....kkkkkkkkkkkkkkkkkk",
        "....keeeffffffeeeek...",
    ])
    # Radio pack with a whip antenna instead of the big rucksack.
    rows = overlay(rows, 3, 0, [
        "..R",
        "..R",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D",
        "..D.....",
        "..D.....",
        "kkDkkkk ",
        "kjjjllk ",
    ])
    rows = overlay(rows, 4, 21, [
        "jLjl",
        "jjjl",
        "jDDl",
        "jDDl",
        "jjjj",
    ])
    # Neck scarf.
    rows = overlay(rows, 13, 20, [
        "  FFFFFFFF",
        "FFFFFFFFFFE",
        "EFFE",
    ])
    pal = dict(BODY_PAL)
    pal.update({
        "G": (40, 36, 30), "c": (110, 200, 226), "C": (210, 248, 255),
        "R": (220, 60, 48), "D": (40, 40, 46), "L": (120, 236, 120),
        "F": (160, 64, 50), "E": (118, 44, 36),
        "j": (88, 96, 78), "l": (112, 120, 100), "m": (138, 146, 122),
        "q": (160, 150, 108), "s": (134, 124, 88), "t": (148, 138, 98),
        "p": (110, 102, 72), "o": (98, 92, 66), "n": (82, 76, 54),
        "r": (74, 60, 44), "u": (116, 100, 72), "v": (94, 80, 58),
        "x": (200, 188, 140), "y": (70, 76, 62),
    })
    # The cap was drawn with t/T/u, which the torso also uses, so give the
    # head rows their own letters.
    rows = [r.replace("t", "1").replace("T", "2").replace("u", "3")
            if i < 12 else r for i, r in enumerate(rows)]
    pal.update({"1": (124, 110, 76), "2": (164, 148, 104), "3": (196, 182, 136)})
    add("body_scout", rows, pal, auto_outline=False)


def body_heavy():
    # Full steel suit: domed helmet with a visor slit, grilled jaw guard,
    # layered chest plate and a compact armoured jetpack.
    rows = [
        "................................",
        "............kkkkkkkkkk..........",
        "..........kk6655555554kk........",
        ".........k65555555555554k.......",
        "........k4555555555555544k......",
        "........k44444444444444443k.....",
        "........k4444444kkkkkkkkkkk.....",
        "........k3333333k111111111k.....",
        "........k3333333kOPPPPPPPOk.....",
        "........k3333333k111111111k.....",
        "........k22222222kkkkkkkkkk.....",
        "........kk22222233333333333k....",
        ".........k2222233k1k1k1k13k.....",
        ".........k2222233333333333k.....",
        "..........kk222222222222kk......",
        "............kkk1111111kk........",
        "...kkkkkkk....k1111111k.........",
        "...k55444k..kkk2222222kkk.......",
        "...k43333k.k2222222222222k......",
        "...k43113kk555555555555555k.....",
        "...k43113k5444444444444444k.....",
        "...k43333k5433333333333332k.....",
        "...k43333k4333333333333332k.....",
        "...k43113k4344433344443332k.....",
        "...k43113k4333333333333332k.....",
        "...k43333k4322222222222221k.....",
        "...k43333k4344433344443332k.....",
        "...k43333k4333333333333332k.....",
        "...k44333k3322222222222221k.....",
        "...kk2222k3344433344443332k.....",
        "....k1kk1k3333333333333332k.....",
        "....kZkkZk2222222222222221k.....",
        "....kZk.kZk111111111111111k.....",
        ".....k...kzzzzzzxxxzzzzzzzk.....",
        ".........kzzzzzzxxxzzzzzzzk.....",
        ".........kkkkkkkkkkkkkkkkkk.....",
        ".........kAABBBBBBBBBBBBBk......",
        ".........kAABBBBBBBBBBBBBk......",
        ".........kAABBBBBBBBBBBBBk......",
        ".........kAABBBBBBBBBBBBBk......",
    ]
    pal = dict(BODY_PAL)
    pal.update({
        "1": (40, 42, 50), "2": (62, 66, 78), "3": (92, 98, 112),
        "4": (120, 126, 140), "5": (152, 158, 172), "6": (196, 202, 214),
        "O": (230, 110, 50), "P": (255, 196, 110), "Z": (120, 110, 100),
        "z": (40, 40, 46), "x": (150, 150, 160),
    })
    add("body_heavy", rows, pal, auto_outline=False)


# ---------------------------------------------------------------------------
# Weapons (facing right, drawn rotated around the pivot, which sits where
# the hand grips). The muzzle is BarrelLength pixels ahead of the pivot
# along the pivot's row. Authored without outline; one is added.
# ---------------------------------------------------------------------------

def weapons():
    # SMG, BarrelLength 14: pivot (5,3) -> muzzle col 19.
    add("weapon_smg", [
        "....5555556........",
        "33.444444444455....",
        "3.3444444444433333.",
        "333333333333332222.",
        "w...3.11..3........",
        "....2.11...........",
        "....2.11...........",
        "......11...........",
    ], pivot=(5, 3))

    # Shotgun, BarrelLength 16: pivot (7,2) -> muzzle col 23.
    add("weapon_shotgun", [
        "........444444555555555",
        "wwWWy..34444444444444444",
        "wWWWWyy3333333333333333",
        ".wWWWWW222..WWyyyyWW...",
        "...wWW..3.....WWWW.....",
        "........2..............",
    ], pivot=(7, 2))

    # Rifle, BarrelLength 20: pivot (8,2) -> muzzle col 28.
    add("weapon_rifle", [
        ".........55556...............",
        "wWWyy...44444444yyyyy........",
        "wWWWWyyy333333333WWWWW4444444",
        ".wwWWWW.333222222wwww.2222222",
        "....ww..2.11..........1......",
        "........2.11.................",
        "..........11.................",
    ], pivot=(8, 2))

    # Sniper, BarrelLength 24: pivot (9,4) -> muzzle col 33; long scope.
    add("weapon_sniper", [
        "..........1CCc1444441c1...........",
        ".........1111111111111............",
        "...........2.....2................",
        "OOppp...O44444444pppp.............",
        "OOOOOppp433333333OOOOO444444444455",
        ".OOOOOOO2222222222ooo.22222222222.",
        ".o..ooo.2.1...1.......1...........",
        ".....o..2.1...1.......1...........",
        "........2.....1.......1...........",
    ], pivot=(9, 4))

    # Rocket launcher, BarrelLength 20: a shoulder tube; pivot (13,4)
    # -> muzzle col 33. The tube runs back over the shoulder.
    add("weapon_launcher", [
        "...............2222..............",
        "..............25552..............",
        "1OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOR",
        "1ppppppPPPPPPpppppppppppppppppppR",
        "1OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOR",
        "1oooooooooooooooooooooooooooooooR",
        "........Y.Y.2.11.................",
        "............2.11.................",
        "..............11.................",
    ], pivot=(13, 4))

    # Flamethrower, BarrelLength 17: pivot (6,3) -> muzzle col 23.
    add("weapon_flamer", [
        ".......4444444444..1",
        "..3333344444444443311",
        ".3444443333333333333",
        "3433333222222222222245",
        "3RRRe.2.11..........",
        "3RRRR.2.11..........",
        "3rRRR...11..........",
        ".rrr................",
    ], pivot=(6, 3))

    # Digger, BarrelLength 12: pivot (4,3) -> muzzle col 16.
    add("weapon_digger", [
        ".yyyyyyy.........",
        "yyyyyyyyy4.......",
        "WWWWWWWWW456.....",
        "YkYkYkYkY4455566C",
        "WWWWWWWWW456.....",
        "wwwwwwwww4.......",
        ".ww.11...........",
        "....11...........",
    ], pivot=(4, 3))

    # Shovel, BarrelLength 17: pivot (5,2) -> blade centre col 22.
    add("weapon_shovel", [
        "......................555.",
        "..................4..55665",
        "WW..yyyyyyyyyyyyy44445565.",
        "WWyyWWWWWWWWWWWWWW4445555",
        "WW..wwwwwwwwwwwwwww444445.",
        "..................4..4444.",
        "......................44..",
    ], pivot=(4, 3))


# ---------------------------------------------------------------------------
# Vehicles, payloads and projectiles.
# ---------------------------------------------------------------------------

def vehicles():
    # Drop ship, nose right, centred pivot. Team stripe colour 's'/'S'.
    ship = [
        "......................55444..................",
        "...44444444444444444444443333kk..............",
        "..43333333333333333333333333333344...........",
        ".SSSSSSSSSSSSSSSSSSSSSSSSSSSSSS3333CCc.......",
        "4ssssssssssssssssssssssssssssss33333cCCc.....",
        "433333333333333333333333333333333333cccCc....",
        "43332222333332222233333222223333333333333333.",
        "4332..12233321..12233321..122333333333333322.",
        "433222222333222222233222222333333333333332222",
        ".2222222222222222222222222222222222222222222.",
        "..11111111..............................111..",
        "...1111.....................................",
    ]
    add("ship_team0", ship, {"s": (70, 96, 140), "S": (110, 140, 190)}, pivot=(22, 6))
    add("ship_team1", ship, {"s": (140, 50, 42), "S": (196, 80, 62)}, pivot=(22, 6))

    # Hover drone, rotor housings either side, sensor eye facing right.
    drone = [
        "4444444.....4444444",
        "..33.....5.....33..",
        "..33..4455544..33..",
        "..3333444444443333.",
        "....334444444CCc...",
        "....33333333cccL...",
        ".....2222222222....",
        "......2..1..2......",
    ]
    add("drone_team0", drone, {"L": (90, 170, 255)}, pivot=(9, 4))
    add("drone_team1", drone, {"L": (255, 80, 60)}, pivot=(9, 4))

    # Folded drone case (payload), weapon case, supply crate, pod.
    add("crate_drone", [
        "222222222222222222",
        "..44444444444444..",
        "..45555555555554..",
        "..43333333333334..",
        "..4333333LL33334..",
        "..43333333333334..",
        "..44444444444444..",
        "..2..........2....",
    ], {"L": (120, 200, 255)}, pivot=(9, 8))
    add("crate_weapon", [
        "......1111......",
        "PPPPPPPOOPPPPPPP",
        "OOOOOOOooOOOOOOO",
        "OOOO4OOooOO4OOOO",
        "OOOOOOOooOOOOOOO",
        "OOOOOOOooOOOOOOO",
        "OYYYYYYooYYYYYYO",
        "OkYkYkYooYkYkYkO",
        "OOOOOOOooOOOOOOO",
        "ooooooooooooooooo"[:16],
    ], pivot=(8, 10))
    add("crate_supply", [
        "yyyyyyyyyyyyyyyy",
        "yWWWWWWwwWWWWWWy",
        "yWyWWWWwwWWWWyWy",
        "yWWyWWWwwWWWyWWy",
        "ywwwwwwRRwwwwwwy",
        "ywwwwwRhhRwwwwwy",
        "yWWWWWWRRWWWWWWy",
        "yWWWyWWwwWWyWWWy",
        "yWWyWWWwwWWWyWWy",
        "yWyWWWWwwWWWWyWy",
        "yWWWWWWwwWWWWWWy",
        "wwwwwwwwwwwwwwww",
    ], {"h": (236, 236, 240)}, pivot=(8, 12))
    add("medkit", [
        "...1111...",
        "hhhhhhhhhh",
        "hhhhRRhhhh",
        "hhhRRRRhhh",
        "hhhhRRhhhh",
        "5555555555",
    ], {"h": (226, 226, 232)}, pivot=(5, 6))

    # Projectiles.
    add("rocket", [
        "Z.ooOOOOrRR.",
        "ZYoOPPPPRRRR",
        "Z.ooOOOOrRR.",
    ], pivot=(6, 1))
    add("grenade", [
        ".1.",
        "OPO",
        "OOO",
        ".o.",
    ], pivot=(1, 2))


# ---------------------------------------------------------------------------
# HUD icons (screen space, 10x10-ish, outlined).
# ---------------------------------------------------------------------------

def icons():
    add("icon_health", [
        "..RRR..",
        "..ReR..",
        "RRReRRR",
        "Reeeeee",
        "RRReRRR",
        "..ReR..",
        "..RRR..",
    ], {"e": (255, 150, 130)})
    add("icon_fuel", [
        "..Z...",
        ".ZZ...",
        ".ZeZ..",
        "ZeeZ.Z",
        "ZeRRZZ",
        "ZeRRRZ",
        ".ZRRZ.",
    ], {"Z": (255, 214, 120), "e": (250, 160, 70), "R": (230, 90, 50)})
    add("icon_ammo", [
        ".Y..Y..Y.",
        "YZYYZYYZY",
        "YZYYZYYZY",
        "YYYYYYYYY",
        "333333333",
        "444444444",
    ])
    add("icon_gold", [
        "..YYY..",
        ".YZZZY.",
        "YZYYYZY",
        "YZYZYZY",
        "YZYYYZY",
        ".YZZZY.",
        "..YYY..",
    ], {"Y": (196, 150, 40), "Z": (252, 220, 110)})
    add("icon_brain", [
        "..pppp...",
        ".pPPPPpp.",
        "pPpPPpPPp",
        "pPPpPPpPp",
        "pPpPPPpPp",
        ".ppPpPpp.",
        "...ppp...",
    ], {"p": (190, 100, 130), "P": (240, 160, 180)})


def weapon_icons(max_w=20, max_h=9):
    """Small HUD icons for the item slots, shrunk from the weapon art.

    Each colour pixel is averaged from its source block (box filter) and
    a pixel is kept when most of the block was opaque, then the outline
    is drawn fresh so the silhouette stays crisp.
    """
    for name, img, _, _ in list(SPRITES):
        if not name.startswith("weapon_"):
            continue
        src = img.convert("RGBA")
        src.putdata([(0, 0, 0, 0) if p[:3] == KEY or p[:3] == OUTLINE else p
                     for p in pixels(src)])
        # crop away the old outline ring
        src = src.crop((1, 1, src.width - 1, src.height - 1))
        scale = min(1.0, max_w / src.width, max_h / src.height)
        w = max(1, round(src.width * scale))
        h = max(1, round(src.height * scale))
        rows = []
        pal = {}
        for y in range(h):
            row = ""
            for x in range(w):
                x0, x1 = int(x / scale), max(int(x / scale) + 1, int((x + 1) / scale))
                y0, y1 = int(y / scale), max(int(y / scale) + 1, int((y + 1) / scale))
                acc = [0, 0, 0]
                n = 0
                total = 0
                for yy in range(y0, min(y1, src.height)):
                    for xx in range(x0, min(x1, src.width)):
                        total += 1
                        p = src.getpixel((xx, yy))
                        if p[3]:
                            n += 1
                            for i in range(3):
                                acc[i] += p[i]
                if n * 2 >= total and n:
                    colour = tuple(a // n for a in acc)
                    key = chr(0x100 + len(pal))
                    for k, v in pal.items():
                        if v == colour:
                            key = k
                            break
                    pal[key] = colour
                    row += key
                else:
                    row += "."
            rows.append(row)
        add("icon_" + name, rows, pal)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preview", help="also write a contact sheet PNG here")
    args = parser.parse_args()

    body_rifleman()
    body_scout()
    body_heavy()
    weapons()
    vehicles()
    icons()
    weapon_icons()

    os.makedirs(OUT_DIR, exist_ok=True)
    lines = [
        "; Generated by Tools/art/make_sprites.py - edit the art there.",
        "; File is relative to this folder. PivotX/PivotY is the pixel the",
        "; sprite is positioned and rotated around.",
        "",
    ]
    for name, img, px, py in SPRITES:
        img.save(os.path.join(OUT_DIR, name + ".bmp"))
        lines += ["[%s]" % name, "File = %s.bmp" % name,
                  "PivotX = %d" % px, "PivotY = %d" % py, ""]
    with open(os.path.join(OUT_DIR, "sprites.ini"), "w", newline="\n") as f:
        f.write("\n".join(lines))

    if args.preview:
        scale = 6
        pad_px = 8
        x = pad_px
        rowh = 0
        sheet_w = 1400
        placed = []
        y = pad_px
        for name, img, px, py in SPRITES:
            w, h = img.width * scale, img.height * scale
            if x + w > sheet_w:
                x = pad_px
                y += rowh + pad_px
                rowh = 0
            placed.append((img, x, y, px, py))
            x += w + pad_px
            rowh = max(rowh, h)
        sheet = Image.new("RGB", (sheet_w, y + rowh + pad_px), (70, 74, 88))
        for img, x, y, px, py in placed:
            rgba = img.convert("RGBA")
            data = [(0, 0, 0, 0) if p[:3] == KEY else p for p in pixels(rgba)]
            rgba.putdata(data)
            big = rgba.resize((img.width * scale, img.height * scale), Image.NEAREST)
            sheet.paste(big, (x, y), big)
            # pivot marker
            for d in range(-2, 3):
                sheet.putpixel((x + px * scale + scale // 2 + d, y + py * scale + scale // 2), (0, 255, 0))
                sheet.putpixel((x + px * scale + scale // 2, y + py * scale + scale // 2 + d), (0, 255, 0))
        sheet.save(args.preview)

    print("wrote %d sprites to %s" % (len(SPRITES), OUT_DIR))


if __name__ == "__main__":
    sys.exit(main())
