"""Rasterize the card and its backing as two standard RGBA picture textures.

Coordinates use the same layout units as the original vector design. Sampling
averages premultiplied color, then stores straight RGBA for the native material.
No Pillow/image editor or game asset is required to generate these pixels.
"""
from functools import lru_cache

PIXELS_PER_UNIT = 2
CANVAS = (128, 96)
WIDTH, HEIGHT = (size * PIXELS_PER_UNIT for size in CANVAS)
NAMES = ('QoLShoppingCardBacking^w', 'QoLShoppingCardIcon^w')
WHITE = (255, 255, 255, 255)
INK = (10, 12, 8, 255)
FACE = (84, 183, 120, 255)
CREAM = (255, 241, 195, 255)
CHIP = (240, 204, 101, 255)


def inside(x, y, width, height, radius=0):
    if abs(x) > width / 2 or abs(y) > height / 2:
        return False
    dx = max(abs(x) - (width / 2 - radius), 0)
    dy = max(abs(y) - (height / 2 - radius), 0)
    return dx * dx + dy * dy <= radius * radius


def color_at(x, y, backing):
    if backing:
        return WHITE if inside(x, y, 94, 66, 15) else None
    if not inside(x, y, 82, 54, 9):
        return None
    color = INK
    if inside(x, y, 76, 48, 6):
        color = FACE
        if inside(x, y - 10, 76, 8):
            color = CREAM
        elif inside(x + 23, y + 7, 14, 12):
            color = CHIP
        elif inside(x - 16, y + 12, 22, 3) or inside(x - 9, y + 19, 36, 3):
            color = CREAM
    return color


@lru_cache(maxsize=2)
def pixels(backing):
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
                    color = color_at(x, y, backing)
                    if color:
                        red += color[0]; green += color[1]; blue += color[2]; count += 1
            if count:
                rgba = (round(red / count), round(green / count), round(blue / count), round(255 * count / total))
                position = 4 * (row * WIDTH + column)
                output[position:position + 4] = bytes(rgba)
    return bytes(output)
