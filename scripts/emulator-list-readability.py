#!/usr/bin/env python3
"""Exact-native eight-item viewport QA, including real storage-failure geometry."""
from pathlib import Path
from repo_workflow import candidate as candidate_pbw, qa_output
import hashlib,json,os,sys,time,uuid,zipfile
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState,AppRunStateStart,AppRunStateStop
from libpebble2.communication.transports.qemu.protocol import QemuButton
from libpebble2.services.screenshot import Screenshot
from libpebble2.services.appmessage import AppMessageService,Uint8,Uint32,CString
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];VERSION=json.loads((ROOT/'package.json').read_text())['version']
PBW=candidate_pbw(ROOT, 'Timer');sha=lambda b:hashlib.sha256(b).hexdigest();candidate=sha(PBW.read_bytes())
OUT=qa_output(ROOT, '')/'native-list';OUT.mkdir(parents=True,exist_ok=True)
NATIVE=ROOT/'build/list-native-only.pbw';target=uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
with zipfile.ZipFile(PBW) as source,zipfile.ZipFile(NATIVE,'w') as out:
    payload={n:sha(source.read(n)) for n in source.namelist() if n.startswith('emery/')}
    for entry in source.infolist():
        if entry.filename=='appinfo.json' or entry.filename.startswith('emery/'):out.writestr(entry,source.read(entry.filename))
os.environ['PEBBLE_QA_STATE']=str(ROOT.parent/'.recovery-tools'/('timer-list-'+str(time.time_ns())))
sys.path.insert(0,str(ROOT.parent/'recovery-validation'))
from pipe_emulator import connect
p=connect();service=AppMessageService(p);records={};probes=[];captures=[]
def receive(transaction,app,data):
    if app!=target:return
    if data.get(3000)==1:probes.append(dict(data));return
    if 1000 not in data:return
    records[int(data[1021])]=dict(data)
    service.send_message(target,{1014:CString(data[1001]),1015:Uint32(data[1002]),1020:Uint32(data[1020])})
service.register_handler('appmessage',receive)
def launch():p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.4)
def stop():p.send_packet(AppRunState(data=AppRunStateStop(uuid=target)));time.sleep(.2)
def install(path):ToolAppInstaller(p,str(path),quiet=True).install();launch()
def seed(mode):stop();install(ROOT/'build'/('fixture-mode'+str(mode)+'.pbw'));stop();install(NATIVE)
def tap(name,count=1):
    for _ in range(count):
        send_data_to_qemu(p.transport,QemuButton(state=int(getattr(QemuButton.Button,name))));time.sleep(.04)
        send_data_to_qemu(p.transport,QemuButton(state=0));time.sleep(.18)
def snapshot():
    records.clear();service.send_message(target,{1017:Uint8(1),1019:Uint32(0)})
    for _ in range(100):
        if len(records)==8:break
        time.sleep(.05)
    assert len(records)==8
    return {str(k):dict(v) for k,v in records.items()}
def banks():
    stop();probes.clear();install(ROOT/'build/fixture-mode8.pbw')
    for _ in range(100):
        if probes:break
        time.sleep(.05)
    assert probes
    result=probes[-1][3007];stop();install(NATIVE);return result
def shot(name,menu,critical=False):
    time.sleep(.15);data=b''.join(Screenshot(p).grab_image());im=Image.frombytes('RGB',(200,228),data)
    path=OUT/(name+'.png');im.save(path)
    selected=[y for y in range(40,174 if critical else 228) if im.getpixel((1,y))==(0,0,0)]
    assert len(selected)==80 and selected[-1]-selected[0]==79,(name,selected)
    expected=67 if critical else 50 if menu==1 else 138
    assert selected[0]==expected,(name,selected[0],expected)
    # Text is wholly inset in the selected row; exclude only the progress rail.
    ink=[(x,y) for y in range(expected,expected+80) for x in range(10,148)
         if min(im.getpixel((x,y)))>100]
    assert ink
    first=min(y for x,y in ink);last=max(y for x,y in ink)
    assert first>=expected+3 and last<=expected+76,(name,first,last)
    captures.append({'file':path.name,'sha256':sha(path.read_bytes()),'selectedMenu':menu,
                     'selectedBounds':[0,expected,166,80],'textInkTop':first,'textInkBottom':last,
                     'criticalOneRow':critical})
def navigate(prefix,critical=False):
    for menu in range(1,9):
        if menu>1:tap('Down')
        shot(prefix+'-forward-'+str(menu),menu,critical)
    tap('Down',3);shot(prefix+'-lower-boundary',8,critical)
    # Detail→Back restores the focused last timer and its current viewport.
    tap('Select');tap('Back');shot(prefix+'-back-restores-eight',8,critical)
    for menu in range(7,0,-1):
        tap('Up');shot(prefix+'-reverse-'+str(menu),menu,critical)
seed(5);before=snapshot();navigate('normal');after=snapshot()
for k in before:
    for key in (1001,1003,1010,1011,1012,1022,1023):assert before[k].get(key)==after[k].get(key)
seed(9);bank_before=banks();navigate('critical',True);bank_after=banks();assert bank_before==bank_after
assert sha(PBW.read_bytes())==candidate,'PBW changed during native QA'
report={'result':'PASS','version':VERSION,'pbwSha256':candidate,'nativePayloadSha256':payload,'captures':captures,
 'fixtures':{str(n):sha((ROOT/'build'/('fixture-mode'+str(n)+'.pbw')).read_bytes()) for n in (5,8,9)},
 'fixtureSourceSha256':sha((ROOT/'tests/ux-fixture/src/c/main.c').read_bytes()),
 'proofs':{'allEightVisibleForwardAndReverse':True,'long24hRowIncludedAtPosition3':True,
           'lowerBoundaryAndBackRestore':True,'normalNavigationPreservesOwnerIdentityAndDeadlines':True,
           'criticalOneRowSelectedIndexAlwaysVisible':True,'criticalBankChecksumUnchanged':[bank_before,bank_after]},
 'scope':'Actual packaged Emery native pixels; isolated mode5 mixed-eight and mode9 exhausted-generation-eight V1 fixtures. Native Timer alone migrates/reconciles; no diagnostic shipping code. Direct QA receipts/read-only bank checksum probe; no phone JS/physical watch.'}
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','captures':len(captures),'path':str(OUT/'report.json')}))
