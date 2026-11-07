#!/usr/bin/env python3
"""Measure actual final Timer PNG ink; no host-renderer approximation."""
from pathlib import Path
from repo_workflow import candidate as candidate_pbw, qa_output
import hashlib,json
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
VERSION=json.loads((ROOT/'package.json').read_text())['version']
OUT=qa_output(ROOT, '')
UI=OUT/'native-ui-system'
checks=[]
def check(role,file,box,background,max_offset=1.5,min_inset=2):
    path=UI/file;im=Image.open(path).convert('RGB');left,top,right,bottom=box
    points=[]
    for y in range(top,bottom):
        for x in range(left,right):
            rgb=im.getpixel((x,y));neutral=max(rgb)-min(rgb)<5
            ink=max(abs(v-background) for v in rgb)>35
            if neutral and ink:points.append((x,y))
    assert points,role
    first=min(y for x,y in points);last=max(y for x,y in points)
    offset=(first+last-top-bottom+1)/2
    item={'role':role,'capture':file,'captureSha256':hashlib.sha256(path.read_bytes()).hexdigest(),
          'frame':box,'inkTop':first,'inkBottom':last,'topInset':first-top,
          'bottomInset':bottom-1-last,'verticalCentreOffset':offset}
    assert item['topInset']>=min_inset and item['bottomInset']>=min_inset,item
    assert abs(offset)<=max_offset,item
    checks.append(item);return item

check('selected countdown/state/saved-duration group','list-light.png',(10,50,148,130),0)
check('second countdown/state/saved-duration group','list-light.png',(10,138,148,218),255)
for role,y in [('Edit remaining',68),('Reset',112),('Delete option',156)]:
    check(role,'options-dark.png',(9,y,157,y+38),255 if y==68 else 0)
check('Keep selected','delete-light-safe-default.png',(6,160,160,190),0)
check('Delete choice','delete-light-safe-default.png',(6,196,160,226),255)
check('editor selected digits','new-editor-dark.png',(61,96,105,148),255)
for role,x in [('HRS',5),('MIN',58),('SEC',111)]:
    check('editor '+role,'new-editor-dark.png',(x,154,x+50,178),0)
for file in ['new-editor-dark.png','options-dark.png']:
    check('complete footer '+file,file,(6,198,160,228),0)
for file,top in [('critical-delete-light.png',32),('critical-list-dark.png',174)]:
    check('critical primary '+file,file,(6,top+3,160,top+26),170)
    # The two text lines share the complete 54px warning surface. A descender
    # can use 20px of a 23px line; verify its cell and the full band's padding.
    check('Keep Timer open '+file,file,(6,top+28,160,top+51),170,min_inset=1)
    check('complete warning group '+file,file,(6,top,160,top+54),170,min_inset=3)
for file in ['list-light.png','ready-light.png','remaining-editor-light.png','alarm-dark-treatment.png']:
    background=Image.open(UI/file).convert('RGB').getpixel((0,0))[0]
    title=check('header title '+file,file,(8,0,90,31),background)
    clock=check('header clock '+file,file,(94,0,158,31),background)
    assert clock['inkBottom']-clock['inkTop']+1>=19,clock
    assert abs(title['verticalCentreOffset']-clock['verticalCentreOffset'])<=.5
report={'result':'PASS','version':VERSION,
 'pbwSha256':json.loads((UI/'report.json').read_text())['pbwSha256'],
 'scope':'Exact Emery PNG interiors: complete text groups, option/confirmation/editor cells, warning lines and full-width footers. Header clock actual ink19px. Bounding boxes exclude physical action icons and progress rails. No physical-watch/phone-rendering proof.',
 'checks':checks}
(OUT/'centering.json').write_text(json.dumps(report,indent=2)+'\n')
print('Native ink centering PASS:',len(checks),'roles')
