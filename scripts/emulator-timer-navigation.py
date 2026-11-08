#!/usr/bin/env python3
"""Exact-native Timer editor/options/confirmation/alarm navigation on Emery.
Data seeds and the read-only bank probe are emulator-only; no phone runtime.
"""
from pathlib import Path
import argparse,hashlib,json,os,sys,time,uuid,zipfile
from repo_workflow import candidate,check_build
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState,AppRunStateStart,AppRunStateStop
from libpebble2.protocol.logs import AppLogMessage,AppLogShippingControl
from libpebble2.communication.transports.qemu.protocol import QemuButton,QemuContentSize
from libpebble2.services.screenshot import Screenshot
from libpebble2.services.appmessage import AppMessageService,Uint8,Uint32,CString
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--pbw',type=Path);parser.add_argument('--label',default='candidate');args=parser.parse_args()
PBW=args.pbw or candidate(ROOT,'Timer');check_build(ROOT,PBW)
sha=lambda b:hashlib.sha256(b).hexdigest();digest=sha(PBW.read_bytes())
OUT=ROOT/'build/qa/native-navigation'/digest[:12]/str(time.time_ns());OUT.mkdir(parents=True)
os.environ['PEBBLE_QA_STATE']=str(OUT/'emulator');sys.path.insert(0,str(ROOT.parent/'recovery-validation'))
from pipe_emulator import connect
p=connect();target=uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151');service=AppMessageService(p)
records={};probes=[];logs=[];captures=[];checks={}
p.register_endpoint(AppLogMessage,lambda packet:logs.append(packet.message));p.send_packet(AppLogShippingControl(enable=True))
NATIVE=OUT/'native-only.pbw'
with zipfile.ZipFile(PBW) as source,zipfile.ZipFile(NATIVE,'w') as out:
    payload={n:sha(source.read(n)) for n in source.namelist() if n.startswith('emery/')}
    for name in source.namelist():
        if name=='appinfo.json' or name.startswith('emery/'):out.writestr(name,source.read(name))
def receive(transaction,app,data):
    if app!=target:return
    if data.get(3000)==1:probes.append(dict(data));return
    if 1000 not in data:return
    records[int(data[1021])]=dict(data)
    service.send_message(target,{1014:CString(data[1001]),1015:Uint32(data[1002]),1020:Uint32(data[1020])})
service.register_handler('appmessage',receive)
def launch():p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.45)
def stop():p.send_packet(AppRunState(data=AppRunStateStop(uuid=target)));time.sleep(.2)
def install(path):ToolAppInstaller(p,str(path),quiet=True).install();launch()
def seed(mode):stop();install(ROOT/f'build/fixture-mode{mode}.pbw');stop();install(NATIVE)
def tap(name,n=1,wait=.5):
    for _ in range(n):
        send_data_to_qemu(p.transport,QemuButton(state=int(getattr(QemuButton.Button,name))));time.sleep(.045)
        send_data_to_qemu(p.transport,QemuButton(state=0));time.sleep(wait)
def size(value):stop();send_data_to_qemu(p.transport,QemuContentSize(size=value));time.sleep(.25);launch()
def shot(name,alarm=False):
    image=Image.frombytes('RGB',(200,228),b''.join(Screenshot(p).grab_image()));path=OUT/(name+'.png');image.save(path)
    rail=(85,85,85) if alarm else (0,0,0)
    assert all(image.getpixel((x,y))==rail for x in (166,167,198,199) for y in range(228)),name+' full-height rail'
    assert any(image.getpixel((x,y))!=(255,255,255) for x in range(106,158) for y in range(2,29)),name+' clock'
    captures.append({'file':path.name,'sha256':sha(path.read_bytes())});return image
def bank():
    stop();probes.clear();install(ROOT/'build/fixture-mode8.pbw')
    for _ in range(200):
        if probes:break
        time.sleep(.04)
    assert probes
    result=probes[-1][3007];stop();install(NATIVE);return result
def snapshot():
    records.clear();service.send_message(target,{1017:Uint8(1),1019:Uint32(0)})
    for _ in range(160):
        if len(records)==8:break
        time.sleep(.04)
    assert len(records)==8;return {i:dict(value) for i,value in records.items()}
for value,name in enumerate(('small','medium','large'),1):
    seed(11);size(value);before=bank();tap('Select');shot(name+'-details')
    tap('Down');shot(name+'-options-first');tap('Down',2);shot(name+'-options-delete')
    tap('Select');shot(name+'-confirmation-keep-first');tap('Select');shot(name+'-keep-returns-options')
    tap('Select');shot(name+'-confirmation-reopens-safe');tap('Back');tap('Up',2);tap('Select');shot(name+'-editor-minutes')
    tap('Back');shot(name+'-editor-hours');tap('Back');shot(name+'-editor-back-to-options')
    after=bank();assert after==before,(name,'cancel/Back changed authoritative banks')
    tap('Select');tap('Up');tap('Up');shot(name+'-edited-eleven-minutes');tap('Select',2);shot(name+'-edit-saved-details')
    state=snapshot();assert state[0][1003]=='paused' and state[0][1012]==660 and state[0][1022]==660
    tap('Down');tap('Down');shot(name+'-reset-saved-repeat-ten-minutes');tap('Select');shot(name+'-reset-ready')
    tap('Select');shot(name+'-restart');state=snapshot();assert state[0][1003]=='active' and state[0][1022]==600
    tap('Select');state=snapshot();assert state[0][1003]=='paused' and 570<=state[0][1012]<=600
    checks[name]={'cancelBankChecksum':before,'editedRemaining':660,'repeatDurationAfterResetStart':600,'pauseWorks':True}
    # Explicit destructive choice; Keep was always the first focused choice.
    seed(11);tap('Select');tap('Down');tap('Down',2);tap('Select');tap('Down');shot(name+'-explicit-delete');tap('Select');shot(name+'-deleted-empty')
    assert all(row[1003]=='cancelled' for row in snapshot().values())
    # Genuine Add -> editor -> new details, with a running production timer.
    seed(0);tap('Select');shot(name+'-add-editor');tap('Up');tap('Select');tap('Up');tap('Select');shot(name+'-add-saved-details')
    state=snapshot();assert state[0][1003]=='active' and state[0][1022]==661
    checks[name]['createdDuration']=661
# Real storage refusal: confirmation stays safe and its complete context can scroll.
seed(9);size(3);before=bank();tap('Select');tap('Down');tap('Down',2);tap('Select');shot('critical-confirmation-safe')
tap('Up',4);shot('critical-confirmation-context-top');tap('Back');tap('Back');tap('Back');assert bank()==before
seed(6);tap('Select');tap('Down');tap('Select');shot('alarm-before-editor-interruption');time.sleep(31)
shot('alarm-preempts-editor',alarm=True);tap('Back');shot('alarm-storage-refusal-back-to-list');checks['alarmPreemptsEditorAndBackWorks']=True
# Repeated complete nested paths; each destination keeps its lifetime through motion.
seed(11)
for _ in range(10):
    tap('Select');tap('Down');tap('Down',2);tap('Select');tap('Select');tap('Back',2)
shot('after-ten-nested-navigation-cycles');checks['nestedCycles']=10
assert sha(PBW.read_bytes())==digest;check_build(ROOT,PBW)
report={'result':'PASS','label':args.label,'pbwSha256':digest,'nativePayloadSha256':payload,
        'checks':checks,'captures':captures,'diagnosticLogs':logs,
        'scope':'Exact supplied native bytes in isolated Emery v4.33.2; public native input APIs, production model, no phone runtime. Any locale/diagnostic fixture is identified by label and native hashes.'}
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','path':str(OUT/'report.json'),'captures':len(captures)}),flush=True)
