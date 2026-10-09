"""Rounded card silhouettes using ordinary untextured, vertex-colored panes.

Window frames use texture-specific stretching; they are unsuitable for a new
card silhouette. Fine horizontal bands approximate circular corners, with
one-unit alpha ramps on the contour instead of large staircase steps.
"""
from math import ceil, sqrt

BAND_HEIGHT = .75
EDGE_FADE = 1.0
LAYERS = (
    ('P_CardRim', 94, 66, 15, (255, 255, 255, 255)),
    ('P_CardEdge', 82, 54, 9, (53, 57, 48, 255)),
    ('P_CardFace', 76, 48, 6, (84, 183, 120, 255)),
)


def rounded_rectangle(width, height, radius, color):
    """Yield (x, y, width, height, LT/RT/LB/RB colors), centered at the origin."""
    assert 0 < radius < min(width, height) / 2
    clear = (*color[:3], 0)
    solid = (color,) * 4

    def band(y, extent, thickness):
        # Interior and two non-overlapping alpha ramps. Put the 50% contour at
        # the analytic boundary; shared band edges meet without double blending.
        inner = extent - EDGE_FADE / 2
        yield (0, y, 2 * inner, thickness, solid)
        yield (-extent, y, EDGE_FADE, thickness, (clear, color, clear, color))
        yield (extent, y, EDGE_FADE, thickness, (color, clear, color, clear))

    yield from band(0, width / 2, height - 2 * radius)
    count = ceil(radius / BAND_HEIGHT)
    step = radius / count
    for i in range(count):
        distance = (i + .5) * step
        extent = width / 2 - radius + sqrt(radius * radius - distance * distance)
        y = height / 2 - radius + distance
        yield from band(y, extent, step)
        yield from band(-y, extent, step)
    # Fade the straight top/bottom edges as well, including when the icon tilts.
    for sign in (-1, 1):
        colors = (clear, clear, color, color) if sign > 0 else (color, color, clear, clear)
        yield (0, sign * (height / 2 + EDGE_FADE / 2), width - 2 * radius,
               EDGE_FADE, colors)
