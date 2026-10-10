"""Bake the existing basket silhouette into one antialiased UI picture.

The silhouette is sampled at its intended layout size. A single picture
avoids multiplying draw constants/vertices for every bagged row.
White RGB with coverage alpha lets the native pane supply the mint tint.
"""
from math import ceil
from bag_geometry import segments

NAME = 'QoLBagBasket^s'
SIZE = 128
CANVAS = 64.0
SAMPLES = 4

def pixels():
    side = SIZE * SAMPLES
    scale = side / CANVAS
    mask = bytearray(side * side)
    for x, y, width, height, angle in segments():
        assert angle == 0
        left = max(0, ceil((x - width / 2 + CANVAS / 2) * scale - .5))
        right = min(side, ceil((x + width / 2 + CANVAS / 2) * scale - .5))
        top = max(0, ceil((CANVAS / 2 - y - height / 2) * scale - .5))
        bottom = min(side, ceil((CANVAS / 2 - y + height / 2) * scale - .5))
        for row in range(top, bottom):
            mask[row * side + left:row * side + right] = bytes([1]) * (right - left)
    result = bytearray()
    for y in range(SIZE):
        for x in range(SIZE):
            coverage = sum(mask[(y * SAMPLES + dy) * side + x * SAMPLES + dx]
                           for dy in range(SAMPLES) for dx in range(SAMPLES))
            result.extend((255,255,255,round(255 * coverage / (SAMPLES * SAMPLES))))
    return bytes(result)
