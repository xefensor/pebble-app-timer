#!/usr/bin/env python3
"""Fresh exact-native Emery motion/input QA over the isolated FIFO transport.

Requires build/fixture-mode5.pbw from the documented isolated UX fixture.
The screenshot RPC samples frames; this is not hardware motion/battery timing.
"""
from pathlib import Path
import hashlib,json,os,sys,time,uuid,zipfile
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState,AppRunStateStart,AppRunStateStop
from libpebble2.protocol.phone import PhoneNotification,IncomingCall,CallEnd
from libpebble2.communication.transports.qemu.protocol import QemuButton
from libpebble2.services.screenshot import Screenshot
from libpebble2.services.appmessage import AppMessageService,Uint8,Uint32,CString
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
VERSION=json.loads((ROOT/'package.json').read_text())['version']
OUT=ROOT/'docs/qa'/VERSION/'native-motion';OUT.mkdir(parents=True,exist_ok=True)
state=ROOT.parent/'.recovery-tools'/('emulator-timer-redesign-'+str(time.time_ns()))
os.environ['PEBBLE_QA_STATE']=str(state)
sys.path.insert(0,str(ROOT.parent/'recovery-validation'))
from pipe_emulator import connect
p=connect();target=uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
pbw=ROOT/'dist'/('Timer-'+VERSION+'.pbw');native=ROOT/'build/redesign-native-only.pbw'
sha=lambda data:hashlib.sha256(data).hexdigest()
with zipfile.ZipFile(pbw) as archive,zipfile.ZipFile(native,'w') as output:
    payload={n:sha(archive.read(n)) for n in archive.namelist() if n.startswith('emery/')}
    for entry in archive.infolist():
        if entry.filename=='appinfo.json' or entry.filename.startswith('emery/'):
            output.writestr(entry,archive.read(entry.filename))
records={};service=AppMessageService(p);captures=[]
def receive(transaction,app,data):
    if app!=target or 1000 not in data:return
    records[int(data[1021])]=dict(data)
    service.send_message(target,{1014:CString(data[1001]),1015:Uint32(data[1002]),1020:Uint32(data[1020])})
service.register_handler('appmessage',receive)
def launch():p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.5)
def stop():p.send_packet(AppRunState(data=AppRunStateStop(uuid=target)));time.sleep(.3)
def install(path):ToolAppInstaller(p,str(path),quiet=True).install();launch()
def press(name):send_data_to_qemu(p.transport,QemuButton(state=int(getattr(QemuButton.Button,name))))
def release():send_data_to_qemu(p.transport,QemuButton(state=0))
def tap(name,wait=.45):press(name);time.sleep(.035);release();time.sleep(wait)
def pixels():return b''.join(Screenshot(p).grab_image())
def body(data):return b''.join(data[y*600:y*600+166*3] for y in range(34,228))
def shot(name,wait=0):
    time.sleep(wait);data=pixels();assert len(data)==200*228*3
    image=Image.frombytes('RGB',(200,228),data);path=OUT/(name+'.png');image.save(path)
    captures.append({'file':path.name,'sha256':sha(path.read_bytes())});return data
def series(name,button):
    frames=[Image.frombytes('RGB',(200,228),shot(name+'-before'))]
    started=time.monotonic();tap(button,0)
    samples=[]
    for n,delay in enumerate((.025,.055,.08,.25)):
        frame=shot(name+'-'+str(n),delay);samples.append(round((time.monotonic()-started)*1000))
        frames.append(Image.frombytes('RGB',(200,228),frame))
    frames[0].save(OUT/(name+'.gif'),save_all=True,append_images=frames[1:],duration=[500]+[100]*3+[700],loop=0)
    return samples
def snapshot():
    records.clear();service.send_message(target,{1017:Uint8(1),1019:Uint32(0)})
    for _ in range(100):
        if len(records)==8:break
        time.sleep(.05)
    assert len(records)==8;return {str(k):dict(v) for k,v in records.items()}

install(ROOT/'build/fixture-mode5.pbw');stop();install(native)
before=snapshot()
menu_samples=series('elastic-list','Down')
# Move back, then activate the new logical target before its 350ms feedback ends.
tap('Up');tap('Down',.025);tap('Select',.025);tap('Select',.4)
after=snapshot()
assert after['1'][1003]=='paused' and after['0'][1003]=='active'
assert after['0'][1011]==before['0'][1011],'Selection feedback changed the sibling deadline'
shot('select-during-motion-paused-correct-slot')
# Paused owner body remains static; only user input drives decorative timers.
still=shot('paused-static-before');later=shot('paused-static-after',.7)
assert body(still)==body(later)

# A held Up opens the editor once; its repeats must not change fields after
# that screen transition. Compare with a fresh single press of the same editor.
tap('Up');editor=shot('single-up-editor');tap('Back');tap('Back')
press('Up');time.sleep(.75);release();held=shot('held-up-editor',.4)
assert body(editor)==body(held),'Held entry press leaked repeats into the editor'
value_samples=series('editor-value-bump','Up')
field_samples=series('editor-field-advance','Select')
# Cancel all editor fields, preserving native pause state and sibling clocks.
tap('Back');tap('Back');tap('Back')
cancelled=snapshot()
assert all(cancelled[k][key]==after[k][key] for k in after for key in (1001,1003,1010,1011,1012,1022,1023))

# Physical feedback: action icon moves 5px right on a held press and returns.
tap('Down');idle=shot('action-idle')
press('Down');time.sleep(.05);pressed=shot('action-pressed');release()
settled=shot('action-release-settled',.45)
bar=lambda data:b''.join(data[y*600+166*3:y*600+200*3] for y in range(170,200))
assert bar(idle)!=bar(pressed),'Physical Down press did not move the action icon'
assert bar(idle)==bar(settled),'Action release did not settle'

# Cover a live Options transition using the stock OS's synthetic call surface.
tap('Up');tap('Down',.025)
cookie=0x544d5201
p.send_packet(PhoneNotification(cookie=cookie,message=IncomingCall(number='000',name='Timer redesign QA')))
shot('os-cover',.45)
p.send_packet(PhoneNotification(cookie=cookie,message=CallEnd()))
ended=shot('os-call-ended',.7)
if ended[:3]==bytes((255,0,0)):tap('Back')
for _ in range(40):
    returned=pixels()
    if returned[:3]==bytes((255,255,255)) and returned[199*3:200*3]==bytes((0,0,0)):break
    time.sleep(.1)
else:raise AssertionError('Stock call surface did not reveal Timer')
returned=shot('after-cover-settled',.2);later=shot('after-cover-still',.7)
assert body(returned)==body(later),'Focus return replayed decorative motion'
stop();launch();reopened=shot('cold-reopen',.3)
assert reopened[:3]==bytes((255,255,255))
report={'result':'PASS','version':VERSION,'pbwSha256':sha(pbw.read_bytes()),
 'scope':'Actual exact-distribution Emery native payload; FIFO QEMU, QA fixture and simulated AppMessage receipts; no phone JS, physical watch or battery/timing measurement',
 'nativePayloadSha256':payload,'captures':captures,'sampleElapsedMilliseconds':{'list':menu_samples,'value':value_samples,'field':field_samples},
 'gifNote':'Actual sampled native frames with presentation dwell time; GIF is not a frame-rate/timing measurement.',
 'proofs':{'selectDuringSelectionMotionUsesImmediateCorrectTarget':True,'holdDoesNotLeakAcrossEditorEntry':True,'editorCancelPreservesOwnerState':True,'pausedBodyStatic':True,'physicalPressMovesIconAndReleaseSettles':True,'coverCancelsAndReturnStaysStill':True,'coldReopen':True},
 'physicalDeviceVerified':False,'freshStockDerivedFlash':True,'qemuState':str(state)}
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'result':'PASS','path':str(OUT/'report.json')}))
