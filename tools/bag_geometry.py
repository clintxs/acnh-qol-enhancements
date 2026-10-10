"""Rounded trapezoid basket with the existing hollow semicircle handle.

Horizontal samples are baked into one texture, never emitted as drawing panes.
"""
from math import sqrt
MINT=(123,255,208,255)
BODY_TOP_HALF=17.0
BODY_BOTTOM_HALF=11.0
BODY_HEIGHT=17.0
HANDLE_RADIUS=11.0
HANDLE_INNER_RADIUS=7.0
BASELINE=4.0
STRIP=.5
ICON_SCALE=1.25
RIM_RADIUS=3.0

def _unscaled_segments():
    # Circle fillets tangent to the horizontal rim/base and diagonal side.
    slope=(BODY_TOP_HALF-BODY_BOTTOM_HALF)/BODY_HEIGHT
    normal=sqrt(1+slope*slope)
    top_center=BODY_TOP_HALF-slope*RIM_RADIUS-RIM_RADIUS*normal
    bottom_center=BODY_TOP_HALF-slope*(BODY_HEIGHT-RIM_RADIUS)-RIM_RADIUS*normal
    top_join=RIM_RADIUS+RIM_RADIUS*slope/normal
    bottom_join=BODY_HEIGHT-RIM_RADIUS+RIM_RADIUS*slope/normal
    for i in range(round(BODY_HEIGHT/STRIP)):
        depth=(i+.5)*STRIP
        if depth < top_join:
            half=top_center+sqrt(max(0,RIM_RADIUS**2-(depth-RIM_RADIUS)**2))
        elif depth > bottom_join:
            half=bottom_center+sqrt(max(0,RIM_RADIUS**2-(depth-(BODY_HEIGHT-RIM_RADIUS))**2))
        else:
            half=BODY_TOP_HALF-slope*depth
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
