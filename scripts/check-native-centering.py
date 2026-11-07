#!/usr/bin/env python3
"""Measure ink in actual Timer captures; this does not simulate a renderer."""
from pathlib import Path
import hashlib,json
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
VERSION=json.loads((ROOT/'package.json').read_text())['version']
OUT=ROOT/'docs/qa'/VERSION
checks=[]
def check(role,file,box,background,max_offset=1.5):
    path=OUT/'native-ui-system'/file
    image=Image.open(path).convert('RGB');left,top,right,bottom=box
    points=[]
    for y in range(top,bottom):
        for x in range(left,right):
            color=image.getpixel((x,y))
            neutral=max(color)-min(color)<5
            ink=max(color)<60 if background is None else max(abs(v-background) for v in color)>35
            if neutral and ink:points.append((x,y))
    assert points,role
    first=min(y for x,y in points);last=max(y for x,y in points)
    offset=(first+last-top-bottom+1)/2
    item={'role':role,'capture':file,'captureSha256':hashlib.sha256(path.read_bytes()).hexdigest(),
      'frame':box,'inkTop':first,'inkBottom':last,'topInset':first-top,
      'bottomInset':bottom-1-last,'verticalCentreOffset':offset}
    assert item['topInset']>=2 and item['bottomInset']>=2,item
    assert abs(offset)<=max_offset,item
    checks.append(item);return item

check('short selected countdown and detail','list-light.png',(13,40,140,94),0)
check('short countdown and detail','list-light.png',(13,96,140,150),255)
check('long countdown and detail','list-light.png',(13,152,140,206),255)
check('Edit remaining selected','options-dark.png',(12,58,162,99),255)
check('Reset','options-dark.png',(12,101,162,142),0)
check('Delete option','options-dark.png',(12,144,162,185),0)
check('Keep selected','delete-light-safe-default.png',(4,143,162,175),0)
check('Delete choice','delete-light-safe-default.png',(4,179,162,211),255)
check('editor selected digits','new-editor-dark.png',(61,104,105,149),255)
check('editor HRS','new-editor-dark.png',(5,150,55,174),0)
check('editor MIN','new-editor-dark.png',(58,150,108,174),0)
check('editor SEC','new-editor-dark.png',(111,150,161,174),0)
check('critical Delete warning','critical-delete-light.png',(4,32,162,73),None)
check('critical list warning','critical-list-dark.png',(4,187,162,228),None)
title=check('header title','list-light.png',(8,0,94,32),255)
clock=check('header clock','list-light.png',(98,0,162,32),255)
assert clock['inkBottom']-clock['inkTop']+1>=18,clock
assert title['verticalCentreOffset']==clock['verticalCentreOffset']
for file in ('ready-light.png','remaining-editor-light.png','alarm-dark-treatment.png'):
    image=Image.open(OUT/'native-ui-system'/file).convert('RGB')
    background=image.getpixel((0,0))[0]
    label=check('descender header '+file,file,(8,0,98,32),background)
    clock=check('clock '+file,file,(100,0,162,32),background)
    assert abs(label['verticalCentreOffset']-clock['verticalCentreOffset'])<=.5
report={'result':'PASS','version':VERSION,'scope':'Unmodified Emery PNGs: group centring, actual ink margins and readable clock height. Text-only interiors exclude progress bars and physical action icons.','checks':checks}
(OUT/'centering.json').write_text(json.dumps(report,indent=2)+'\n')
print('Native ink centring PASS:',len(checks),'roles; top clock ink18px, shared optical centre')
