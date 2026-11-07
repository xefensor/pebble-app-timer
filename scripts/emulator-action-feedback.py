#!/usr/bin/env python3
"""Verify each physical action independently in actual packaged native pixels.

Uses the isolated FIFO helper and the documented mode-5 stock-data fixture.
No real phone/watch is used. Keep each icon available while pressing it.
"""
from pathlib import Path
from repo_workflow import candidate as candidate_pbw, qa_output
import hashlib,json,os,sys,time,uuid,zipfile
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState,AppRunStateStart,AppRunStateStop
from libpebble2.communication.transports.qemu.protocol import QemuButton
from libpebble2.services.screenshot import Screenshot
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
VERSION=json.loads((ROOT/'package.json').read_text())['version']
OUT=qa_output(ROOT, '')/os.environ.get('TIMER_ACTION_QA_DIR','native-actions')
OUT.mkdir(parents=True,exist_ok=True)
os.environ['PEBBLE_QA_STATE']=str(ROOT.parent/'.recovery-tools'/('emulator-timer-actions-'+str(time.time_ns())))
sys.path.insert(0,str(ROOT.parent/'recovery-validation'))
from pipe_emulator import connect
p=connect();target=uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
pbw=candidate_pbw(ROOT, 'Timer');native=ROOT/'build/action-feedback-native-only.pbw'
sha=lambda data:hashlib.sha256(data).hexdigest()
candidate_sha=sha(pbw.read_bytes())
with zipfile.ZipFile(pbw) as archive,zipfile.ZipFile(native,'w') as output:
    payload={n:sha(archive.read(n)) for n in archive.namelist() if n.startswith('emery/')}
    for entry in archive.infolist():
        if entry.filename=='appinfo.json' or entry.filename.startswith('emery/'):
            output.writestr(entry,archive.read(entry.filename))
def launch():p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.5)
def stop():p.send_packet(AppRunState(data=AppRunStateStop(uuid=target)));time.sleep(.3)
def install(path):ToolAppInstaller(p,str(path),quiet=True).install();launch()
def press(name):send_data_to_qemu(p.transport,QemuButton(state=int(getattr(QemuButton.Button,name))))
def release():send_data_to_qemu(p.transport,QemuButton(state=0))
def tap(name):press(name);time.sleep(.035);release();time.sleep(.4)
captures=[]
def shot(name):
    data=b''.join(Screenshot(p).grab_image());assert len(data)==200*228*3
    path=OUT/(name+'.png');Image.frombytes('RGB',(200,228),data).save(path)
    captures.append({'file':path.name,'sha256':sha(path.read_bytes())});return data
def ink(data,y):
    # Light mode has white icons on the full-height black rail.
    points=[(x,row) for row in range(max(32,y-13),y+14) for x in range(167,200)
            if data[(row*200+x)*3:(row*200+x)*3+3]==b'\xff\xff\xff']
    assert points,'Expected the action icon to remain available'
    return {'pixels':len(points),'left':min(x for x,_ in points),'right':max(x for x,_ in points),
            'centerX':sum(x for x,_ in points)/len(points)}
install(ROOT/'build/fixture-mode5.pbw');stop();install(native)
tap('Select');tap('Up') # Remaining editor, minutes selected; +/− both available.
checks={}
for name,y in (('Up',54),('Down',174),('Select',114)):
    if name=='Select':tap('Back') # Hours→minutes keeps the same Next icon.
    before=ink(shot(name.lower()+'-before'),y)
    press(name);time.sleep(.075);pressed=ink(shot(name.lower()+'-pressed'),y)
    release();time.sleep(.45);settled=ink(shot(name.lower()+'-settled'),y)
    assert pressed['pixels']==before['pixels'],(name,'icon shape/availability changed',before,pressed)
    assert pressed['left']-before['left']==-5 and pressed['right']-before['right']==-5,(name,before,pressed)
    assert abs(pressed['centerX']-before['centerX']+5)<.01,(name,before,pressed)
    assert pressed['left']>=168,(name,'Pressed icon leaves the action rail inset',pressed)
    assert pressed['right']<=196,(name,'Pressed icon violates3px screen-edge inset',pressed)
    assert settled==before,(name,'release did not settle',before,settled)
    checks[name]={'idle':before,'pressed':pressed,'settled':settled,'shiftPixels':-5}
assert sha(pbw.read_bytes())==candidate_sha,'Candidate changed during native action QA'
report={'result':'PASS','version':VERSION,'pbwSha256':candidate_sha,'nativePayloadSha256':payload,'checks':checks,'captures':captures,
 'scope':'Actual distribution native payload in isolated Emery QEMU; each still-available physical icon checked independently. No real phone/watch or animation timing claim.'}
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'result':'PASS','path':str(OUT/'report.json'),'checks':checks}))
