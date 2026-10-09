"""Preview actual packaged card pixels; native oval/font/GPU still need playtest.

Requires Pillow from the artifact runtime. Decodes the exported BNTX byte order,
resolves each PIC's material texture index and uses its serialized dimensions.
The oval is illustrative, not a recreation of the game's renderer.
"""
from pathlib import Path
import argparse
import base64
import json
import struct
import subprocess
import sys
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]


def decode(data, width, height):
    output = bytearray(len(data))
    for address in range(0,len(data),4):
        gob, local = divmod(address,512)
        byte_x = (local & 15) | ((local >> 1) & 16) | ((local >> 3) & 32)
        x = (gob % (width//16))*16 + byte_x//4
        y = (gob//(width//16))*8 + ((local >> 4) & 1) + ((local >> 5) & 6)
        position = 4*(y*width+x)
        output[position:position+4] = data[address:address+4]
    return Image.frombytes('RGBA',(width,height),bytes(output))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    parser.add_argument('--layout-python',default='python3')
    args = parser.parse_args()
    # The default Python has zstd; the artifact Python has Pillow.
    code = """import sys,json,base64
sys.path.insert(0,'tools')
from compression import zstd
import build_shopping_card_layout as c
from layout_io import resources
r=resources(zstd.decompress(next(iter(c.build().values()))))
print(json.dumps({k:base64.b64encode(v).decode() for k,v in r.items() if k.endswith(('.bflyt','.bntx'))}))
"""
    resources = {k:base64.b64decode(v) for k,v in json.loads(subprocess.check_output(
        [args.layout_python,'-c',code],cwd=ROOT)).items()}
    sys.path.insert(0,str(ROOT/'tools'))
    from shopping_bntx import textures
    from layout_io import layout_sections,pane_name
    sections = layout_sections(next(v for k,v in resources.items() if k.endswith('.bflyt')))
    panes = {pane_name(p):p for p in sections if p[:4] in (b'pic1',b'pan1')}
    material_table = next(p for p in sections if p[:4] == b'mat1')
    count = struct.unpack_from('<H',material_table,8)[0]
    offsets = (*struct.unpack_from(f'<{count}I',material_table,12),len(material_table))
    materials = [material_table[a:b] for a,b in zip(offsets,offsets[1:])]
    table = next(p for p in sections if p[:4] == b'txl1')
    names = [table[12+o:table.index(0,12+o)].decode() for o in
             struct.unpack_from(f'<{struct.unpack_from("<H",table,8)[0]}I',table,12)]
    images = {t.name:t for t in textures(next(v for k,v in resources.items() if k.endswith('.bntx')))}
    scale = 4
    preview = Image.new('RGBA',(400*scale,200*scale),(244,232,197,255))
    def draw_picture(name):
        pane = panes[name]
        material = materials[struct.unpack_from('<H',pane,100)[0]]
        texture = images[names[struct.unpack_from('<H',material,44)[0]]]
        width,height = struct.unpack_from('<2I',texture.info,0x24)
        tile = decode(texture.data,width,height)
        # Linear UI colors are converted to sRGB on the output framebuffer.
        def srgb(value):
            v=value/255
            return round(255*(12.92*v if v<=.0031308 else 1.055*v**(1/2.4)-.055))
        channels = tile.split()
        tile = Image.merge('RGBA',(*[c.point(srgb) for c in channels[:3]],channels[3]))
        w,h = struct.unpack_from('<2f',pane,76)
        tile = tile.resize((round(w*scale),round(h*scale)),Image.Resampling.LANCZOS)
        parent = panes['N_CardBacking_00' if name == 'P_CardRim_00' else 'N_CardIcon_00']
        angle = struct.unpack_from('<f',parent,64)[0]
        tile = tile.rotate(angle,resample=Image.Resampling.BICUBIC,expand=True)
        preview.alpha_composite(tile,(round(200*scale-tile.width/2),round(68*scale-tile.height/2)))
    draw_picture('P_CardRim_00')
    draw = ImageDraw.Draw(preview)
    draw.ellipse((47*scale,58*scale,353*scale,186*scale),fill='white')
    draw.ellipse((55*scale,66*scale,345*scale,178*scale),fill=(110,180,154,255))
    draw_picture('P_CardIcon_00')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    preview.resize((800,400),Image.Resampling.LANCZOS).convert('RGB').save(args.output)
    print(args.output)

if __name__ == '__main__': main()
