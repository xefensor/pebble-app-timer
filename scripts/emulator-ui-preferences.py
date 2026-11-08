#!/usr/bin/env python3
"""Exact candidate UI receipt, native-size equivalence and persistence on Emery.
Only run on disposable QEMU storage; seed/probe fixtures share Timer's UUID.
"""
from pathlib import Path
import hashlib,json,os,struct,sys,time,uuid,zipfile
from repo_workflow import candidate,check_build
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState,AppRunStateStart,AppRunStateStop
from libpebble2.protocol.system import TimeMessage,SetUTC
from libpebble2.communication.transports.qemu.protocol import QemuContentSize
from libpebble2.services.appmessage import AppMessageService,ByteArray
from libpebble2.services.screenshot import Screenshot
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];PBW=candidate(ROOT,'Timer');check_build(ROOT,PBW)
sha=lambda value:hashlib.sha256(value).hexdigest()
OUT=ROOT/'build/qa/ui-preferences'/str(time.time_ns());OUT.mkdir(parents=True)
os.environ['PEBBLE_QA_STATE']=str(OUT/'emulator');sys.path.insert(0,str(ROOT.parent/'recovery-validation'))
from pipe_emulator import connect
p=connect();p.send_packet(TimeMessage(message=SetUTC(unix_time=1794143100,utc_offset=0,tz_name='UTC')))
target=uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151');service=AppMessageService(p)
replies=[];probes=[];captures=[];checks=[];revision=0
native=OUT/'native-only.pbw'
with zipfile.ZipFile(PBW) as source,zipfile.ZipFile(native,'w') as out:
    payload={name:sha(source.read(name)) for name in source.namelist() if name.startswith('emery/')}
    for name in source.namelist():
        if name=='appinfo.json' or name.startswith('emery/'):out.writestr(name,source.read(name))
def receive(transaction,app,data):
    if app!=target:return
    if 1027 in data:replies.append(bytes(data[1027]))
    if data.get(3000)==1:probes.append(dict(data))
service.register_handler('appmessage',receive)
def wait(condition):
    for _ in range(160):
        if condition():return
        time.sleep(.04)
    raise AssertionError('Native response timeout')
def stop():p.send_packet(AppRunState(data=AppRunStateStop(uuid=target)));time.sleep(.2)
def launch():
    replies.clear();p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)))
    wait(lambda:replies);time.sleep(.25)
    assert replies[-1][3]==1
    return replies[-1]
def install(path):ToolAppInstaller(p,str(path),quiet=True).install()
def bank():
    stop();probes.clear();install(ROOT/'build/fixture-mode8.pbw')
    p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));wait(lambda:probes)
    value=probes[-1];stop();install(native);launch();return value
def size(value):
    stop();send_data_to_qemu(p.transport,QemuContentSize(size=value));time.sleep(.2);return launch()
def apply(text,appearance,last):
    global revision
    revision+=1;frame=struct.pack('<BBBBIIII',1,3,0,0,revision,text,appearance,last)
    replies.clear();service.send_message(target,{1026:ByteArray(frame)})
    wait(lambda:frame in replies);time.sleep(.25);return frame
def shot(name):
    image=Image.frombytes('RGB',(200,228),b''.join(Screenshot(p).grab_image()));path=OUT/(name+'.png');image.save(path)
    captures.append({'file':path.name,'sha256':sha(path.read_bytes())});return image
install(ROOT/'build/fixture-mode10.pbw');p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.8)
stop();send_data_to_qemu(p.transport,QemuContentSize(size=1));install(native);launch()
before=bank()[3007]
for raw in (1,2,3):
    size(raw);apply(0,1,1);reference=shot(f'follow-os-{raw}-light')
    size(3 if raw==1 else 1);frame=apply(raw,1,1);manual=shot(f'manual-{raw}-light')
    # Full content area excludes only the moving system clock and action rail.
    assert reference.crop((0,40,166,228)).tobytes()==manual.crop((0,40,166,228)).tobytes(),('manual differs from native OS size',raw)
    stop();hello=launch();assert hello[:3]+bytes([0])+hello[4:]==frame
    reopened=shot(f'manual-{raw}-light-reopened')
    assert reopened.crop((0,40,166,228)).tobytes()==manual.crop((0,40,166,228)).tobytes()
    apply(raw,2,2);dark=shot(f'manual-{raw}-dark')
    assert dark.getpixel((0,5))==(0,0,0)
    assert manual.getpixel((0,5))==(255,255,255)
    frame=apply(raw,0,2);fallback=shot(f'follow-appearance-{raw}-dark-fallback')
    assert fallback.crop((0,40,166,228)).tobytes()==dark.crop((0,40,166,228)).tobytes()
    stored=bank();assert bytes(stored[3016])==frame
    checks.append({'textSize':raw,'manualMatchesActualOsSize':True,'survivesRelaunch':True,'lightDark':True,'unsupportedFollowChoiceRetainedWithDarkFallback':True})
after=bank()[3007];assert before==after,(before,after)
check_build(ROOT,PBW)
report={'result':'PASS','pbwSha256':sha(PBW.read_bytes()),'nativePayloadSha256':payload,'firmware':str(p.watch_info.running),'checks':checks,'unchangedAuthoritativeTimerBankChecksum':before,'captures':captures,'scope':'Exact candidate native bytes in isolated Emery QEMU;real OS sizes, native receipts and persisted bytes. SDK bridge supplied by host;not physical phone/watch.'}
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','captures':len(captures),'path':str(OUT/'report.json')}),flush=True)
