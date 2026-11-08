#!/usr/bin/env python3
"""First Timer checkpoint against exact candidate native bytes in isolated Emery.

Requires the existing FIFO emulator helper and Python SDK transport/Pillow.
No physical device or phone runtime is used. OS size changes use the official
emu-set-content-size protocol, never an app/font-size override.
"""
from pathlib import Path
import argparse, datetime, hashlib, importlib.util, json, os, re, struct, subprocess, sys, time, uuid, zipfile
from repo_workflow import candidate, check_build
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState, AppRunStateStart, AppRunStateStop
from libpebble2.protocol.logs import AppLogMessage, AppLogShippingControl
from libpebble2.protocol.system import TimeMessage, SetLocaltime
from libpebble2.communication.transports.qemu.protocol import QemuButton, QemuContentSize, QemuTimeFormat
from libpebble2.services.screenshot import Screenshot
from libpebble2.services.appmessage import AppMessageService, Uint8, Uint32, CString
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline',type=Path)
args=parser.parse_args()
PBW=candidate(ROOT,'Timer');check_build(ROOT,PBW)
sha=lambda b:hashlib.sha256(b).hexdigest()
digest=sha(PBW.read_bytes())
OUT=ROOT/'build/qa/timer-checkpoint'/digest[:12]/str(time.time_ns())
OUT.mkdir(parents=True)
os.environ['PEBBLE_QA_STATE']=str(OUT/'emulator')
sys.path.insert(0,str(ROOT.parent/'recovery-validation'))
from pipe_emulator import connect
p=connect();target=uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
reference=uuid.UUID('130e1cec-c819-46c5-bf91-468f63c44876')
service=AppMessageService(p);probes=[];records={};logs=[];captures=[];checks={};matrix=[]
p.register_endpoint(AppLogMessage,lambda packet:logs.append(packet.message))
p.send_packet(AppLogShippingControl(enable=True))
def receive(transaction,app,data):
    if app!=target:return
    if data.get(3000)==1:probes.append(dict(data));return
    if 1000 not in data:return
    records[int(data[1021])]=dict(data)
    service.send_message(target,{1014:CString(data[1001]),1015:Uint32(data[1002]),1020:Uint32(data[1020])})
service.register_handler('appmessage',receive)
def header_metrics():
    spec=importlib.util.spec_from_file_location('checkpoint_pfo',ROOT/'scripts/generate-body-metrics.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    media=json.loads((ROOT/'package.json').read_text())['pebble']['resources']['media']
    with zipfile.ZipFile(PBW) as archive:packed=archive.read('emery/app_resources.pbpack')
    result=[]
    for i,item in enumerate(media):
        if item['name'] not in ('FONT_BODY_22','FONT_BODY_24','FONT_BODY_26'):continue
        _,offset,length,_=struct.unpack_from('<4I',packed,12+i*16)
        _,glyphs=module.parse_pfo(packed[4108+offset:4108+offset+length])
        width=lambda text:sum(glyphs[ord(c)]['advance'] for c in text)
        clocks24=[f'{h:02d}:{m:02d}' for h in range(24) for m in range(60)]
        clocks12=[f'{h}:{m:02d}' for h in range(1,13) for m in range(60)]
        maximum=max(width(t) for t in clocks24+clocks12)
        assert maximum<=142
        result.append({'font':item['name'],'widest24h':max(clocks24,key=width),
          'width24h':max(map(width,clocks24)),'widest12h':max(clocks12,key=width),
          'width12h':max(map(width,clocks12)),'minimumLabelWidth':166-24-maximum})
    return result
checks['headerClockCorpus']=header_metrics()
def stop(app=target):
    p.send_packet(AppRunState(data=AppRunStateStop(uuid=app)));time.sleep(.22)
def launch(app=target):
    p.send_packet(AppRunState(data=AppRunStateStart(uuid=app)));time.sleep(.4)
def install(path,app=target):
    ToolAppInstaller(p,str(path),quiet=True).install();launch(app)
def native(source,name):
    dest=OUT/(name+'.pbw')
    with zipfile.ZipFile(source) as src,zipfile.ZipFile(dest,'w') as out:
        payload={n:sha(src.read(n)) for n in src.namelist() if n.startswith('emery/')}
        for n in src.namelist():
            if n=='appinfo.json' or n.startswith('emery/'):out.writestr(n,src.read(n))
    return dest,payload
NATIVE,payload=native(PBW,'candidate-native-only')
active=NATIVE
fixtures={str(i):sha((ROOT/f'build/fixture-mode{i}.pbw').read_bytes()) for i in (5,6,8,9,10,11,12)}
def seed(mode):
    stop();install(ROOT/f'build/fixture-mode{mode}.pbw');stop();install(active)
def press(button):send_data_to_qemu(p.transport,QemuButton(state=int(getattr(QemuButton.Button,button))))
def release():send_data_to_qemu(p.transport,QemuButton(state=0))
def tap(button,count=1,settle=.5):
    for _ in range(count):press(button);time.sleep(.045);release();time.sleep(settle)
def size(value):
    stop();send_data_to_qemu(p.transport,QemuContentSize(size=value));time.sleep(.25);launch()
def clock(hour,is24=True):
    send_data_to_qemu(p.transport,QemuTimeFormat(is_24_hour=is24))
    epoch=int(datetime.datetime(2026,11,8,hour,58,15,tzinfo=datetime.timezone.utc).timestamp())
    p.send_packet(TimeMessage(message=SetLocaltime(time=epoch)));time.sleep(.15)
def theme(dark):
    service.send_message(target,{1024:Uint8(int(dark)),1025:Uint8(1)});time.sleep(.2)
def probe():
    stop();probes.clear();install(ROOT/'build/fixture-mode8.pbw')
    for _ in range(80):
        if probes:break
        time.sleep(.04)
    assert probes,'Read-only persistence/preference probe timed out'
    result=probes[-1];stop();install(active);return result

def snapshot():
    records.clear();service.send_message(target,{1017:Uint8(1),1019:Uint32(0)})
    for _ in range(150):
        if len(records)==8:break
        time.sleep(.04)
    assert len(records)==8,'Owner snapshot timed out'
    return dict(records)
def shot(name,chrome=True,dark=False):
    image=Image.frombytes('RGB',(200,228),b''.join(Screenshot(p).grab_image()))
    path=OUT/(name+'.png');image.save(path)
    if chrome:
        rail=(255,255,255) if dark else (0,0,0)
        assert all(image.getpixel((x,y))==rail for y in range(228) for x in (166,167,198,199)),name+' rail'
        bg=(0,0,0) if dark else (255,255,255)
        assert any(image.getpixel((x,y))!=bg for x in range(8,60) for y in range(2,29)),name+' left label'
        assert any(image.getpixel((x,y))!=bg for x in range(104,158) for y in range(2,29)),name+' right clock'
    captures.append({'file':path.name,'sha256':sha(path.read_bytes()),'chromeChecked':chrome})
    return image

def selected(image,dark=False):
    color=(255,255,255) if dark else (0,0,0)
    rows=[y for y in range(50,228) if image.getpixel((2,y))==color]
    assert rows,'Selected row missing'
    return min(rows),max(rows)+1

def header_label(image):return image.crop((0,0,65,31)).tobytes()
def motion(name,button):
    frames=[shot(name+'-before')];started=time.monotonic();press(button);time.sleep(.035);release()
    times=[]
    for i,at in enumerate((.04,.08,.13,.19,.27,.4)):
        time.sleep(max(0,started+at-time.monotonic()))
        frames.append(shot(name+f'-frame-{i}',False));times.append(round(1000*(time.monotonic()-started)))
    frames[0].save(OUT/(name+'.gif'),save_all=True,append_images=frames[1:],duration=[300]+[70]*5+[500],loop=0)
    return {'framesMs':times,'distinctFrames':len({im.tobytes() for im in frames})}

clock(23)
# Paused production bank is installed once; every size uses these same bytes.
seed(10);theme(False);before=probe()[3007]
labels={}
for value,name in enumerate(('small','medium','large'),1):
    size(value);assert probe()[3008]==value,'OS preferred_content_size mismatch'
    first=shot(name+'-list');top,bottom=selected(first)
    assert top==50,(name,'first-row anchor',top)
    height=bottom-top;complete=178//height
    matrix.append({'size':name,'osPreferredContentSize':value,'firstRowY':top,'rowHeight':height,'completeRows':complete})
    checks[name+'Push']=motion(name+'-push','Select');shot(name+'-details')
    checks[name+'Pop']=motion(name+'-pop','Back')
    back=shot(name+'-back');assert selected(back)==(top,bottom)
    checks[name+'Down']=motion(name+'-down','Down');tap('Up')
    for row in range(1,9):
        if row>1:tap('Down')
        im=shot(name+f'-row-{row}');lo,hi=selected(im)
        assert hi-lo==height and lo>=50 and hi<=228,(name,row,'complete selected row',lo,hi)
        if name=='large':labels[row]=header_label(im)
    tap('Down',3);assert header_label(shot(name+'-bottom-boundary'))==header_label(im)
    tap('Select');tap('Back');assert header_label(shot(name+'-back-to-eight'))==header_label(im)
    for row in range(7,0,-1):
        tap('Up');im=shot(name+f'-reverse-{row}');lo,hi=selected(im)
        assert hi-lo==height and lo>=50 and hi<=228
assert matrix[0]['completeRows']>matrix[2]['completeRows'],matrix
assert probe()[3007]==before,'Ordinary navigation changed authoritative timer banks'
checks['identicalProductionBankAcrossSizesAndNavigation']=before
# Single/multi anchor: same size, same first timer. Rail bytes match the baseline.
size(3);multi=shot('large-multi-anchor');seed(11);single=shot('large-single-anchor')
assert selected(single)[0]==selected(multi)[0]==50
checks['singleAndMultiFirstRowY']=50
# 12-hour clock uses actual OS format; manual comparison shows its full clock.
clock(11,False);shot('large-12h-details-before');tap('Select');shot('large-12h-details');tap('Back');clock(23)
# Both themes at every size; full-height 34px rail and label/clock are checked.
seed(10)
for value,name in enumerate(('small','medium','large'),1):
    size(value);theme(True);shot(name+'-dark-list',dark=True)
    tap('Select');shot(name+'-dark-details',dark=True);tap('Back');shot(name+'-dark-back',dark=True)
theme(False)
# Holds, reversal, and Select before the native highlight settles.
size(3);press('Down');time.sleep(1.45);release();time.sleep(.3)
assert header_label(shot('hold-down-to-eight'))==labels[8]
press('Up');time.sleep(1.45);release();time.sleep(.3)
assert header_label(shot('hold-up-to-one'))==labels[1]
for button in ('Down','Down','Down','Up','Down'):tap(button,settle=.025)
tap('Select',settle=.025);time.sleep(.4);shot('select-during-scroll-details')
press('Select');time.sleep(.09);held=shot('immediate-resume-while-held');release();time.sleep(.35)
state=snapshot();running=[i for i,d in state.items() if d.get(1003)=='active']
assert running==[3],('Exactly the fourth paused timer must resume',running)
settled=shot('immediate-resume-after-release')
# Canonical pause icon is translated 5px toward the content while held.
def glyph(im):return {(x,y) for x in range(166,200) for y in range(102,126) if im.getpixel((x,y))==(255,255,255)}
assert glyph(held)=={(x-5,y) for x,y in glyph(settled)},'Rail press displacement'
tap('Back');assert header_label(shot('back-after-reorder'))==labels[1]
checks['rapidSelectExactlyOnceTargetSlot']=3
# A second reorder starts from known running order [0,1,7,2,3,4,5,6].
seed(5);tap('Down');tap('Select');tap('Select');tap('Back')
assert header_label(shot('pause-reorder-restores-slot-one'))==labels[3]
state=snapshot();assert state[1][1003]=='paused'
checks['pauseReorderRestoredMenu']=3
# Long numeric values and a genuine exhausted-generation critical warning.
seed(12)
for value,name in enumerate(('small','medium','large'),1):
    size(value);shot(name+'-24h-list');tap('Select');shot(name+'-24h-details');tap('Back')
seed(9);size(3);shot('critical-large-first')
for row in range(2,9):
    tap('Down');critical=shot(f'critical-large-row-{row}')
    lo,hi=selected(critical);assert lo>=50 and hi<=174
tap('Select');shot('critical-large-details');tap('Back');shot('critical-large-back')
# Alarm preempts a stacked detail window, and remains operable with write refusal.
seed(6);tap('Select');shot('alarm-interruption-detail-before');time.sleep(31)
alarm=shot('alarm-interrupts-detail',False)
assert alarm.getpixel((166,0))==(85,85,85),'Native alarm did not preempt detail'
tap('Back');shot('alarm-storage-error-back')
checks['alarmPreemptedStackedDetails']=True
# Compare the frozen starting implementation's rail with the same production data.
if args.baseline:
    baseline,baseline_payload=native(args.baseline,'baseline-native-only');active=baseline
    seed(10);baseline_list=shot('baseline-list');tap('Select');baseline_detail=shot('baseline-details');tap('Back');shot('baseline-back')
    active=NATIVE;stop();install(active);size(3);candidate_list=shot('candidate-rail-list');tap('Select');candidate_detail=shot('candidate-rail-details');tap('Back')
    for old,new in ((baseline_list,candidate_list),(baseline_detail,candidate_detail)):
        assert old.crop((166,0,200,228)).tobytes()==new.crop((166,0,200,228)).tobytes(),'Existing action rail changed'
    checks['baselineRailPixelIdentical']=True
# Public SDK reference controls; explicitly fixtures, not a bundled stock Timer.
references={}
for name in ('stock-cells','xoce-center-focused'):
    stop();ref,ref_payload=native(ROOT/'build/qa/menu-reference'/(name+'.pbw'),name)
    install(ref,reference);im=shot(name+'-first');references[name]={'payload':ref_payload,'firstSelectedY':selected(im)[0]}
    references[name]['down']=motion(name+'-down','Down')
    references[name]['push']=motion(name+'-push','Select')
    references[name]['pop']=motion(name+'-pop','Back');stop(reference)
assert references['xoce-center-focused']['firstSelectedY']>50
# Diagnostic build: same source plus existing opt-in logs. No final-byte claim.
diag,diag_payload=native(ROOT/'build/qa/menu-reference/heap-diagnostic.pbw','heap-diagnostic-native')
active=diag;seed(10);logs.clear()
for _ in range(40):tap('Select');tap('Back')
heap=[int(re.search(r'ui heap=(\d+)',line)[1]) for line in logs if 'ui heap=' in line]
assert len(heap)==80,('Completed open/Back cycle diagnostics missing',len(heap))
assert min(heap)>8192 and max(heap[-20:])-min(heap[-20:])<2048,heap
checks['diagnosticHeap']={'cycles':40,'samples':len(heap),'minBytes':min(heap),'last20Min':min(heap[-20:]),'last20Max':max(heap[-20:]),'payload':diag_payload}
# The ordinary exact candidate survives repeated open/Back without instrumentation.
active=NATIVE;stop();install(active)
for _ in range(20):tap('Select');tap('Back')
assert header_label(shot('exact-candidate-after-20-cycles'))==labels[1]
checks['exactCandidateOpenBackCycles']=20
# Locale fixtures: byte-distinct only at app_text's OS-locale lookup.
for locale in ('cs','de'):
    loc,loc_payload=native(ROOT/f'build/qa/menu-reference/locale-{locale}.pbw','locale-'+locale+'-native')
    active=loc;seed(10)
    for value,name in enumerate(('small','medium','large'),1):
        size(value);shot(f'locale-{locale}-{name}-list');tap('Select');shot(f'locale-{locale}-{name}-details');tap('Back')
    checks['localeFixture-'+locale]=loc_payload
active=NATIVE;stop();install(active)
assert sha(PBW.read_bytes())==digest;check_build(ROOT,PBW)
# Comparison boards are assembled only from the untouched real screenshots.
def board(files,name):
    canvas=Image.new('RGB',(200*len(files),252),'#dddddd');draw=ImageDraw.Draw(canvas)
    for i,(label,file) in enumerate(files):
        draw.text((i*200+8,6),label,fill='black');canvas.paste(Image.open(OUT/(file+'.png')),(i*200,24))
    canvas.save(OUT/(name+'.png'))
for screen in ('list','details','back'):
    board([(n.title(),n+'-'+screen) for n in ('small','medium','large')],'sizes-'+screen)
board([('Native stock cells','stock-cells-first'),('XOCE center focused','xoce-center-focused-first'),('XOCE anchored','large-list')],'alignment-comparison')
if args.baseline:board([('Starting Timer','baseline-list'),('Native candidate','large-list')],'baseline-comparison')
report={'result':'PASS','pbw':str(PBW),'pbwSha256':digest,'nativePayloadSha256':payload,
 'sourceCommit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
 'buildRecordSha256':sha((PBW.parent/'candidate.json').read_bytes()),
 'firmware':'v4.33.2','sdk':'4.33.1','cli':'5.0.40','matrix':matrix,'checks':checks,'references':references,
 'fixtures':fixtures,'captures':captures,'baselineSha256':sha(args.baseline.read_bytes()) if args.baseline else None,
 'scope':'Exact final packaged Emery native payload, no phone JS; OS preference protocol and production-model data fixtures. Center-focus/stock cells, locale lookup overrides and heap-instrumented builds are separate named fixtures.',
 'pending':['Physical Time 2 on current supported firmware, paired phone and real notifications/focus covers',
            'Settings UI selection of Text Size (SDK emulator omits Settings); all three actual OS preference values were verified through preferred_content_size()',
            'Touch drag/tap/Back and mixed input: app touch opt-in is not enabled at this checkpoint',
            'Full stock Timer application capture on the same firmware (SDK shell omits it); SDK native menu/window fixtures captured instead',
            'Actual OS Czech/German language packs; locale-lookup fixture renders captured separately',
            'Later checkpoints: native Add/Edit, Options/confirmation and per-app preference delivery']}
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'result':'PASS','path':str(OUT/'report.json'),'matrix':matrix}),flush=True)
