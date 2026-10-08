"""Add a geometric circular bag badge; preserve original panes and resources."""
from pathlib import Path
from compression import zstd
import struct, hashlib, json
from layout_io import *
from bag_geometry import segments, MINT
ROOT = Path(__file__).resolve().parents[1]
ASSETS = ('LBtnListShopTailorBtn', 'LShopBuyTailorCounter')
PANE_TYPES = {b'pan1',b'pic1',b'txt1',b'bnd1',b'prt1',b'wnd1'}

def text_pane(template, name, text, x, y, width, height, font_size, visible):
    pane=bytearray(template)
    pane[12:36]=name.encode().ljust(24,b'\0')
    pane[8]=(pane[8]|2|1) if visible else (pane[8]|2)&~1
    # Preserve the native template origin; icon uses the original icon origin below.
    struct.pack_into('<2f',pane,44,x,y)
    struct.pack_into('<2f',pane,76,width,height)
    pane[92]=0;pane[93]=2
    struct.pack_into('<2f',pane,112,font_size,font_size)
    struct.pack_into('<I',pane,128,0) # No native message lookup overwrites text.
    start=u32(pane,100)
    payload=(text+'\0').encode('utf-16-le')
    capacity=max(len(payload),4)
    pane=pane[:start]+payload+bytes(capacity-len(payload))
    pane.extend(bytes((-len(pane))%4))
    struct.pack_into('<2H',pane,84,capacity,len(payload))
    struct.pack_into('<I',pane,4,len(pane))
    return bytes(pane)

def edit_layout(data, name):
    sections=layout_sections(data)
    panes={pane_name(s):s for s in sections if s[:4] in PANE_TYPES}
    if name==ASSETS[0]:
        # Use an untextured material and mint-colored rectangle vertices.
        # The native renderer scales the vector outline with the item row.
        material_index=next(i for i,s in enumerate(sections) if s[:4]==b'mat1')
        section=sections[material_index]
        count=struct.unpack_from('<H',section,8)[0]
        offsets=list(struct.unpack_from(f'<{count}I',section,12))+[len(section)]
        records=[section[a:b] for a,b in zip(offsets,offsets[1:])]
        fill=struct.unpack_from('<H',panes['T_Price_01'],88)[0]
        material=bytearray(records[fill])
        assert len(material)==44
        material[:28]=b'BagOutlineSolid'.ljust(28,b'\0')
        material[36:40]=bytes(4)
        material[40:44]=bytes([255]*4)
        records.append(bytes(material))
        rebuilt=bytearray(section[:12])
        struct.pack_into('<H',rebuilt,8,len(records))
        offset=12+4*len(records)
        for record in records:
            rebuilt.extend(struct.pack('<I',offset));offset+=len(record)
        rebuilt.extend(b''.join(records));struct.pack_into('<I',rebuilt,4,len(rebuilt))
        sections[material_index]=bytes(rebuilt)
        strokes=[]
        for i,(x,y,width,height,angle) in enumerate(segments()):
            pane=bytearray(panes['P_Equip_00'][:104])
            pane[12:36]=f'P_BagStroke_{i:02}'.encode().ljust(24,b'\0')
            pane[8]=3;pane[9]=0;pane[10]=255
            struct.pack_into('<3f',pane,44,x,y,0)
            struct.pack_into('<3f',pane,56,0,0,angle)
            struct.pack_into('<2f',pane,68,1,1)
            struct.pack_into('<2f',pane,76,width,height)
            pane[84:100]=bytes(MINT)*4
            struct.pack_into('<HBB',pane,100,count,0,0) # no texture coordinates
            struct.pack_into('<I',pane,4,len(pane))
            strokes.append(bytes(pane))
        # Clone the worn marker's circle and use a separate visibility group.
        # Its original parent is animated from alpha zero; our group starts opaque.
        group=bytearray(panes['N_Equip_00'])
        group[12:36]=b'N_Bag_00'.ljust(24,b'\0')
        group[8]=(group[8]|2)&~1
        group[10]=255
        # Native equip animation grows the original from 0.5. Our new group
        # has no native animation; use its full-size resting scale directly.
        struct.pack_into('<2f',group,68,1.0,1.0)
        struct.pack_into('<2f',group,44,-80,60)
        background=bytearray(panes['P_EquipStripe_00'])
        background[12:36]=b'P_BagCircle_00'.ljust(24,b'\0')
        additions=[bytes(group),struct.pack('<4sI',b'pas1',8),bytes(background),
                   *strokes,struct.pack('<4sI',b'pae1',8)]
        parent='N_Scale_00'
    else:
        raise ValueError('Only the item-row layout is modified')
    parents=[];last=None;insertion=None
    for i,s in enumerate(sections):
        if s[:4] in PANE_TYPES:last=pane_name(s)
        elif s[:4]==b'pas1':parents.append(last)
        elif s[:4]==b'pae1':
            if parents.pop()==parent:insertion=i
    assert insertion is not None and not parents
    original=list(sections)
    sections[insertion:insertion]=additions
    output=bytearray(data[:20])+b''.join(sections)
    struct.pack_into('<I',output,12,len(output));struct.pack_into('<H',output,16,len(sections))
    rebuilt=layout_sections(output)
    assert rebuilt[insertion:insertion+len(additions)]==additions
    assert rebuilt[:insertion]+rebuilt[insertion+len(additions):]==original
    return bytes(output)

def patch(data, name):
    start,entries=archive_entries(data);out=bytearray(data[:start]);count=0
    for e in sorted(entries,key=lambda e:e.start):
        content=e.content
        if content.startswith(b'SARC'):
            content,n=patch(content,name);count+=n
        elif e.name==f'blyt/{name}.bflyt':content=edit_layout(content,name);count+=1
        align=min((start+e.start)&-(start+e.start),4096)
        out.extend(bytes((-len(out))%align));offset=len(out)-start
        out.extend(content);struct.pack_into('<2I',out,e.node_offset+8,offset,len(out)-start)
    struct.pack_into('<I',out,8,len(out))
    return bytes(out),count

def build():
    hashes=json.loads((ROOT/'tools/layout_hashes.json').read_text())
    results={}
    for name in ASSETS:
        packed=(ROOT.parent/'vanilla-3.0.3/Layout'/f'{name}.Nin_NX_NVN.zs').read_bytes()
        assert hashlib.sha256(packed).hexdigest()==hashes[name], 'Unsupported original layout'
        if name==ASSETS[1]:
            # Ship vanilla counter bytes so extracting over an older prototype
            # also removes its experimental status pane.
            results[f'romfs/Layout/{name}.Nin_NX_NVN.zs']=packed
            continue
        before=zstd.decompress(packed);after,count=patch(before,name);assert count==1
        old,new=resources(before),resources(after)
        assert old.keys()==new.keys()
        changes=[key for key in old if old[key]!=new[key]]
        assert len(changes)==1 and changes[0].endswith(f'blyt/{name}.bflyt')
        output=zstd.compress(after);assert zstd.decompress(output)==after
        results[f'romfs/Layout/{name}.Nin_NX_NVN.zs']=output
    return results
if __name__=='__main__':
    for name,data in build().items():
        dest=ROOT/'out'/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
        print(dest)
