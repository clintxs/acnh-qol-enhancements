"""Rasterize the card and its backing as two standard RGBA picture textures.

Coordinates use the same layout units as the original vector design. Sampling
averages premultiplied color, then stores straight RGBA for the native material.
No Pillow/image editor or game asset is required to generate these pixels.
"""
from dataclasses import dataclass
from functools import lru_cache

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


# Linear RGBA, as used by the native materials and RGBA8_UNORM textures.
# Gold/cream are the original T_Bell_00 and W_BaseL_00LT material colors.
# Gold's lighter card face matches the bell bag in the supplied screenshot:
# approximately sRGB (249,191,116), converted to linear (242,133,45).
PALETTES = {
    'green': Palette((40,116,82,255), (255,255,255,255), (12,50,42,255),
                     (84,183,120,255), (255,241,195,255), (240,204,101,255)),
    'gold': Palette((130,97,16,255), (255,241,195,255), (70,48,6,255),
                    (242,133,45,255), (255,241,195,255), (240,204,101,255)),
}


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
            color = palette.detail
        elif inside(x + 23, y + 7, 14, 12):
            color = palette.chip
        elif inside(x - 16, y + 4.5, 22, 3) or inside(x - 9, y + 11.5, 36, 3):
            color = palette.detail
    return color


@lru_cache(maxsize=4)
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
