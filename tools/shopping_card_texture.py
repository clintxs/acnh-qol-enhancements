"""Rasterize the card and its backing as two standard RGBA picture textures.

Coordinates use the same layout units as the original vector design. Sampling
averages premultiplied color, then stores straight RGBA for the native material.
No Pillow/image editor or game asset is required to generate these pixels.
"""
from dataclasses import dataclass, replace
from functools import lru_cache

# Packaging uses this reviewed palette; legacy helper defaults remain stable.
DEFAULT_PALETTE = 'island-inverted'
COLOR_LABELS = {
    'green': 'ADB Teal',
    'gold': 'Bell Gold',
    'hotel-blue': 'Hotel Blue',
    'poki-red': 'Poki Red',
    'island': 'Island',
    'island-inverted': 'Island Inverted',
}

PIXELS_PER_UNIT = 2
CANVAS = (128, 96)
WIDTH, HEIGHT = (size * PIXELS_PER_UNIT for size in CANVAS)
NAMES = ('QoLShoppingCardBacking^w', 'QoLShoppingCardIcon^w')


@dataclass(frozen=True)
class Palette:
    counter: tuple
    rim: tuple
    edge: tuple
    face: tuple
    detail: tuple
    chip: tuple
    stripe: tuple | None = None
    background: tuple | None = None
    balance: tuple | None = None
    caption: tuple | None = None


def linear_rgba(red, green, blue):
    def channel(value):
        value /= 255
        return round(255 * (value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4))
    return tuple(channel(value) for value in (red, green, blue)) + (255,)


# Switch Joy-Con reference colors and their lighter card tints, in sRGB.
ISLAND_BLUE = linear_rgba(106, 189, 196)  # #6ABDC4
ISLAND_CARD_BLUE = linear_rgba(155, 212, 217)  # #9BD4D9, visibly lighter than the outline.
ISLAND_CREAM = linear_rgba(255, 249, 227)  # #FFF9E3, sampled from the pockets UI screenshot.
POCKET_BALLOON_TEAL = linear_rgba(81, 188, 171)  # #51BCAB, user's item-name balloon sample.
INVERTED_CARD_TEAL = linear_rgba(151, 215, 205)  # #97D7CD, balloon teal blended 40% toward white.


# Linear RGBA, as used by the native materials and RGBA8_UNORM textures.
# Gold/cream are the original T_Bell_00 and W_BaseL_00LT material colors.
# Gold's card face uses the requested sRGB #F9C687 (249,198,135),
# converted to linear (242,144,62) for the native texture material.
PALETTES = {
    'green': Palette((40,116,82,255), (255,255,255,255), (12,50,42,255),
                     (84,183,120,255), (255,241,195,255), (240,204,101,255)),
    'gold': Palette((130,97,16,255), (255,241,195,255), (70,48,6,255),
                    (242,144,62,255), (255,241,195,255), (240,204,101,255)),
    # Screenshot samples: hotel counter #4575F5, ticket #7094F9 and red #EB4E4C.
    'hotel-blue': Palette((15,45,233,255), (255,241,195,255), (9,20,58,255),
                          (41,76,242,255), (255,241,195,255), (255,241,195,255),
                          stripe=(212,19,18,255)),
    # Poki bag sample #F8A486, paired with a deeper warm-red counter/edge.
    'poki-red': Palette((177,32,25,255), (255,241,195,255), (58,13,9,255),
                        (239,95,61,255), (255,241,195,255), (240,204,101,255)),
    'island': Palette(ISLAND_BLUE, ISLAND_BLUE, linear_rgba(63,142,150),
                      ISLAND_CARD_BLUE, ISLAND_CREAM, ISLAND_CREAM,
                      stripe=ISLAND_CREAM, background=ISLAND_CREAM, balance=ISLAND_BLUE,
                      caption=ISLAND_CREAM),
}

# The inverted UI pairs the pockets balloon teal with a lighter card face.
PALETTES['island-inverted'] = replace(
    PALETTES['island'], counter=POCKET_BALLOON_TEAL, rim=ISLAND_CREAM, face=INVERTED_CARD_TEAL,
    background=POCKET_BALLOON_TEAL, balance=ISLAND_CREAM, caption=POCKET_BALLOON_TEAL)


def inside(x, y, width, height, radius=0):
    if abs(x) > width / 2 or abs(y) > height / 2:
        return False
    dx = max(abs(x) - (width / 2 - radius), 0)
    dy = max(abs(y) - (height / 2 - radius), 0)
    return dx * dx + dy * dy <= radius * radius


def color_at(x, y, backing, palette):
    if backing:
        return palette.rim if inside(x, y, 94, 66, 15) else None
    if not inside(x, y, 82, 54, 9):
        return None
    color = palette.edge
    if inside(x, y, 76, 48, 6):
        color = palette.face
        if inside(x, y - 10, 76, 8):
            color = palette.stripe or palette.detail
        elif inside(x + 23, y + 7, 14, 12):
            color = palette.chip
        elif inside(x - 16, y + 4.5, 22, 3) or inside(x - 9, y + 11.5, 36, 3):
            color = palette.detail
    return color


@lru_cache(maxsize=16)
def pixels(backing, palette_name='green'):
    palette = PALETTES[palette_name]
    samples = 4
    total = samples * samples
    output = bytearray(WIDTH * HEIGHT * 4)
    for row in range(HEIGHT):
        for column in range(WIDTH):
            red = green = blue = count = 0
            for sy in range(samples):
                y = CANVAS[1] / 2 - (row + (sy + .5) / samples) / PIXELS_PER_UNIT
                for sx in range(samples):
                    x = (column + (sx + .5) / samples) / PIXELS_PER_UNIT - CANVAS[0] / 2
                    color = color_at(x, y, backing, palette)
                    if color:
                        red += color[0]; green += color[1]; blue += color[2]; count += 1
            if count:
                rgba = (round(red / count), round(green / count), round(blue / count), round(255 * count / total))
                position = 4 * (row * WIDTH + column)
                output[position:position + 4] = bytes(rgba)
    return bytes(output)
