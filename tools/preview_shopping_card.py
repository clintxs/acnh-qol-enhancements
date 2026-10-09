"""Preview the exported card PIC geometry; this is not a game renderer.

Requires Pillow and NumPy (available in Codex's bundled artifact runtime).
Native oval, font and animation rendering still require an Eden playtest.
"""
from pathlib import Path
import argparse
import sys
import struct
from PIL import Image, ImageDraw
import numpy as np

# The artifact runtime has Pillow; the default Python has archive compression.
# Generate/decompress the layout there, then read the serialized BFLYT here.
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--layout-python', default='python3', help='Python with compression.zstd')
    args = parser.parse_args()
    code = """import sys
sys.path.insert(0, 'tools')
from compression import zstd
import build_shopping_card_layout as c
from layout_io import resources
r = resources(zstd.decompress(next(iter(c.build().values()))))
sys.stdout.buffer.write(next(v for k,v in r.items() if k.endswith('.bflyt')))
"""
    data = subprocess.check_output([args.layout_python, '-c', code], cwd=ROOT)
    sys.path.insert(0, str(ROOT / 'tools'))
    from layout_io import layout_sections, pane_name
    panes = {pane_name(p): p for p in layout_sections(data) if p[:4] in (b'pic1', b'pan1')}
    scale = 4
    size = (160 * scale, 120 * scale)

    def layer(prefixes):
        canvas = Image.new('RGBA', size)
        for name, p in panes.items():
            if not any(name.startswith(prefix + '_') for prefix in prefixes): continue
            x,y = struct.unpack_from('<2f', p,44)
            w,h = struct.unpack_from('<2f', p,76)
            left=(x-w/2+80)*scale; top=(60-y-h/2)*scale
            right=left+w*scale; bottom=top+h*scale
            x0,y0=int(np.floor(left)),int(np.floor(top))
            x1,y1=int(np.ceil(right)),int(np.ceil(bottom))
            xx,yy=np.meshgrid(np.arange(x0,x1)+.5,np.arange(y0,y1)+.5)
            u=np.clip((xx-left)/(right-left),0,1)[...,None]
            v=np.clip((yy-top)/(bottom-top),0,1)[...,None]
            c=np.frombuffer(p[84:100],dtype=np.uint8).reshape(4,4).astype(float)
            values=(1-u)*(1-v)*c[0]+u*(1-v)*c[1]+(1-u)*v*c[2]+u*v*c[3]
            coverage=np.clip(np.minimum(xx+.5,right)-np.maximum(xx-.5,left),0,1)*np.clip(np.minimum(yy+.5,bottom)-np.maximum(yy-.5,top),0,1)
            values[...,3] *= coverage
            tile=Image.fromarray(np.rint(values).astype(np.uint8))
            canvas.alpha_composite(tile,(x0,y0))
        return canvas

    back=layer(('P_CardRim',))
    front=layer(('P_CardEdge','P_CardFace','P_CardShape'))
    # Show the backing occlusion and foreground using an illustrative oval.
    preview=Image.new('RGBA',(400*scale,200*scale),(244,232,197,255))
    back=back.rotate(6,resample=Image.Resampling.BICUBIC)
    front=front.rotate(6,resample=Image.Resampling.BICUBIC)
    location=(120*scale,0)
    preview.alpha_composite(back,location)
    draw=ImageDraw.Draw(preview)
    draw.ellipse((47*scale,58*scale,353*scale,186*scale),fill='white')
    draw.ellipse((55*scale,66*scale,345*scale,178*scale),fill=(40,116,82,255))
    preview.alpha_composite(front,location)
    preview=preview.resize((800,400),Image.Resampling.LANCZOS)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    preview.convert('RGB').save(args.output)
    print(args.output)

if __name__ == '__main__': main()
