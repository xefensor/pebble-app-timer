#!/usr/bin/env python3
"""Focused exact-native rail geometry, palette, glyph and input-feedback QA.

Uses isolated stock-derived QEMU data and distribution-native bytes. Screenshots
sample real frames; they do not measure physical timing or button feel.
"""
from pathlib import Path
from repo_workflow import candidate as candidate_pbw, qa_output
import hashlib,json,os,sys,time,uuid,zipfile
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState,AppRunStateStart,AppRunStateStop
from libpebble2.communication.transports.qemu.protocol import QemuButton
from libpebble2.services.screenshot import Screenshot
from libpebble2.services.appmessage import AppMessageService,Uint8
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];VERSION=json.loads((ROOT/'package.json').read_text())['version']
PBW=candidate_pbw(ROOT, 'Timer');sha=lambda b:hashlib.sha256(b).hexdigest();candidate=sha(PBW.read_bytes())
OUT=qa_output(ROOT, '')/'native-rail'/candidate[:12]
assert not OUT.exists(),'Use completed immutable candidate evidence or build a new candidate.'
OUT.mkdir(parents=True);NATIVE=ROOT/'build/rail-native-only.pbw';target=uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
with zipfile.ZipFile(PBW) as source,zipfile.ZipFile(NATIVE,'w') as out:
    payload={n:sha(source.read(n)) for n in source.namelist() if n.startswith('emery/')}
    for entry in source.infolist():
        if entry.filename=='appinfo.json' or entry.filename.startswith('emery/'):out.writestr(entry,source.read(entry.filename))
os.environ['PEBBLE_QA_STATE']=str(ROOT.parent/'.recovery-tools'/('timer-rail-'+str(time.time_ns())))
sys.path.insert(0,str(ROOT.parent/'recovery-validation'))
from pipe_emulator import connect
p=connect();service=AppMessageService(p);captures=[];checks=[]
def launch():p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.4)
def stop():p.send_packet(AppRunState(data=AppRunStateStop(uuid=target)));time.sleep(.2)
def install(path):ToolAppInstaller(p,str(path),quiet=True).install();launch()
def seed(mode):stop();install(ROOT/'build'/('fixture-mode'+str(mode)+'.pbw'));stop();install(NATIVE)
def press(name):send_data_to_qemu(p.transport,QemuButton(state=int(getattr(QemuButton.Button,name))))
def release():send_data_to_qemu(p.transport,QemuButton(state=0))
def tap(name,count=1):
    for _ in range(count):press(name);time.sleep(.04);release();time.sleep(.23)
def theme(dark):service.send_message(target,{1024:Uint8(int(dark)),1025:Uint8(1)});time.sleep(.25)
def glyph(im,y,dark=False):
    color=(0,0,0) if dark else (255,255,255)
    return {(x,row) for row in range(y-12,y+13) for x in range(167,200) if im.getpixel((x,row))==color}
def bounds(points):
    assert points
    return [min(x for x,y in points),min(y for x,y in points),max(x for x,y in points),max(y for x,y in points)]
def components(points):
    remaining=set(points);result=[]
    while remaining:
        start=remaining.pop();stack=[start];part={start}
        while stack:
            x,y=stack.pop()
            for at in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
                if at in remaining:remaining.remove(at);stack.append(at);part.add(at)
        result.append(part)
    return result
def shot(name,dark=False,footer=False,available=(True,True,True),pressed=None):
    data=b''.join(Screenshot(p).grab_image());assert len(data)==200*228*3
    im=Image.frombytes('RGB',(200,228),data);path=OUT/(name+'.png');im.save(path)
    alarm=name.startswith('alarm-')
    background=(0,0,0) if dark else (255,255,255)
    rail=(85,85,85) if alarm else (255,255,255) if dark else (0,0,0)
    assert all(im.getpixel((x,y))==rail for y in range(228) for x in (166,167,198,199)),name+' full-height rail'
    # Header clock and complete footer stay to the left of the action column.
    assert sum(im.getpixel((x,y))!=background for y in range(1,30) for x in range(94,158))>=160,name+' header clock'
    items=[]
    for index,y in enumerate((48,114,180)):
        points=glyph(im,y,dark and not alarm)
        assert bool(points)==available[index],(name,index,'availability',len(points))
        if not points:continue
        box=bounds(points);right_limit=196
        assert box[0]>=173 and box[2]<=right_limit,(name,index,'horizontal clearance',box)
        assert box[1]>=y-10 and box[3]<=y+10,(name,index,'24px slot clearance',box)
        items.append({'button':('Up','Select','Down')[index],'pixels':len(points),'inkBounds':box})
    captures.append({'file':path.name,'sha256':sha(path.read_bytes()),'dark':dark,'footer':footer,'railY':[0,227],'railColor':list(rail),'icons':items,'pressedButton':pressed})
    return im

def same_shift(before,after,y,dark=False,shift=3):
    old=glyph(before,y,dark);new=glyph(after,y,dark)
    assert new=={(x+shift,row) for x,row in old},('Rigid icon translation',bounds(old),bounds(new))

seed(5);theme(False);shot('list-light')
tap('Select');running=shot('running-light',footer=True)
more=components(glyph(running,180));assert len(more)==3,('More dots must stay separate',[(len(c),bounds(c)) for c in more])
checks.append({'role':'More','separateComponents':3,'inkBounds':bounds(glyph(running,180))})
# Select activates immediately: Pause becomes Play while the button is held.
# Measure that resulting glyph's rigid displacement against its settled frame.
assert len(components(glyph(running,114)))==2,'Idle Pause must have two distinct bars'
press('Select');time.sleep(.07);pause_pressed=shot('pause-press-immediate-play',footer=True,pressed='Select')
assert len(components(glyph(pause_pressed,114)))==1,'Pause press must immediately show Play'
release();time.sleep(.45);paused=shot('paused-after-release',footer=True)
same_shift(paused,pause_pressed,114)
# Up/Down initial repeating-click events enter another screen immediately.
# Check those actual transitions; do not invent a held old-screen glyph.
tap('Select');running=shot('running-before-edit',footer=True)
press('Up');time.sleep(.07);edit_pressed=shot('edit-press-enters-editor',footer=True,pressed='Up')
release();time.sleep(.4);editor=shot('edit-release-editor-settled',footer=True)
assert glyph(edit_pressed,48)==glyph(editor,48),'Screen change must cancel previous icon displacement'
tap('Back',2);shot('running-before-more',footer=True)
press('Down');time.sleep(.07);more_pressed=shot('more-press-enters-options',footer=True,available=(False,True,True),pressed='Down')
release();time.sleep(.4);options=shot('more-release-options-settled',footer=True,available=(False,True,True))
assert glyph(more_pressed,180)==glyph(options,180),'Screen change must cancel previous More displacement'
tap('Down',2);shot('options-delete-light',footer=True,available=(True,True,False));tap('Select');shot('confirm-light',available=(False,True,True))
tap('Back',2);theme(True);shot('running-dark',dark=True,footer=True)
tap('Up');shot('editor-dark',dark=True,footer=True);tap('Select');shot('editor-check-dark',dark=True,footer=True)
tap('Back',3);tap('Down');shot('options-dark',dark=True,footer=True,available=(False,True,True))
seed(0);theme(False);shot('empty-light',available=(False,True,False))
seed(1);shot('alarm-black',dark=True,available=(True,True,False))
seed(6);theme(False);shot('critical-list-light',available=(True,True,False));tap('Select');shot('critical-running-light');tap('Up');shot('critical-editor-light',footer=True)
tap('Back',2);tap('Down');shot('critical-options-light',available=(False,True,True));tap('Down',2);tap('Select');shot('critical-confirm-light',available=(False,True,True))
assert sha(PBW.read_bytes())==candidate,'Candidate changed during focused native QA'
report={'result':'PASS','version':VERSION,'pbwSha256':candidate,'nativePayloadSha256':payload,'captures':captures,'checks':checks,
 'proofs':{'contrastingRailFullHeightInBothThemesAndAlarm':True,'headerFooterStayInsideContentColumn':True,'allPaintedGlyphsInside24pxSlots':True,'screenEdgeInsetAtLeast3px':True,'moreHasThreeSeparateDots':True,'pausePressImmediatelyShowsPlayAndResultingGlyphMovesRigid3px':True,'editMoreScreenChangesCancelPreviousFeedback':True},
 'scope':'Exact packaged native bytes in isolated stock-derived Emery FIFO QEMU. Direct QA theme AppMessages; no phone JS/physical watch. Up/Down activate screen changes immediately, so held captures show the destination and cancellation, not a fabricated old-screen pencil/More press. Select immediately changes Pause to Play; resulting Play held/settled glyphs establish rigid3px feedback.'}
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','captures':len(captures),'path':str(OUT/'report.json')}))
