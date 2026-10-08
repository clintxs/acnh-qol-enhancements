"""Basket silhouette: solid lower semicircle and hollow upper semicircle.

Horizontal native rectangle strips preserve the curved silhouette without a
font, texture replacement, or an opaque fill inside the handle.
"""
from math import sqrt
MINT=(123,255,208,255)
BODY_RADIUS=17.0
HANDLE_RADIUS=11.0
HANDLE_INNER_RADIUS=7.0
BASELINE=4.0
STRIP=.5
ICON_SCALE=1.25
RIM_RADIUS=3.0

def _unscaled_segments():
    # Flat rim at the baseline, rounded bowl below it.
    for i in range(round(BODY_RADIUS/STRIP)):
        depth=(i+.5)*STRIP
        # Radius-three fillet tangent to both the flat rim and circular bowl.
        # The fillet center is inside the original circle by RIM_RADIUS.
        join_depth=BODY_RADIUS*RIM_RADIUS/(BODY_RADIUS-RIM_RADIUS)
        if depth < join_depth:
            cx=sqrt(BODY_RADIUS**2-2*BODY_RADIUS*RIM_RADIUS)
            half=cx+sqrt(max(0,RIM_RADIUS**2-(depth-RIM_RADIUS)**2))
        else:
            half=sqrt(BODY_RADIUS**2-depth**2)
        yield (0,BASELINE-depth,2*half,STRIP+.01,0)
    # True cutout: no panes are drawn in the interior of the handle.
    for i in range(round(HANDLE_RADIUS/STRIP)):
        height=(i+.5)*STRIP
        outer=sqrt(HANDLE_RADIUS**2-height**2)
        inner=sqrt(max(0,HANDLE_INNER_RADIUS**2-height**2))
        if inner:
            width=outer-inner;center=(outer+inner)/2
            for sign in (-1,1):yield (sign*center,BASELINE+height,width,STRIP+.01,0)
        else:
            yield (0,BASELINE+height,outer*2,STRIP+.01,0)

def segments():
    for x,y,w,h,angle in _unscaled_segments():
        yield (x*ICON_SCALE,y*ICON_SCALE,w*ICON_SCALE,h*ICON_SCALE,angle)
