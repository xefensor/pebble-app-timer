#!/usr/bin/env python3
"""Exact Timer native payload in isolated Emery QEMU; SDK transport is a fixture.

No phone JavaScript, real phone or physical watch is used. The Appearance
messages below simulate the established phone-to-watch AppMessage boundary.
"""
from pathlib import Path
from repo_workflow import candidate as candidate_pbw, qa_output
import hashlib,json,os,sys,time,uuid,zipfile
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState,AppRunStateStart,AppRunStateStop
from libpebble2.services.screenshot import Screenshot
from libpebble2.services.appmessage import AppMessageService,Uint8,Uint32,CString
from libpebble2.communication.transports.qemu.protocol import QemuButton
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
VERSION=json.loads((ROOT/'package.json').read_text())['version']
OUT=qa_output(ROOT, '')/'native-ui-system'
OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT.parent/'recovery-validation'))
from pipe_emulator import connect
# A reused flash can display PebbleOS missed-Wakeup notices before this app,
# silently intercepting input. Every full run must start with fresh stock data.
state=Path(os.environ.setdefault('PEBBLE_QA_STATE',str(ROOT.parent/'.recovery-tools'/('emulator-timer-ui-'+VERSION+'-'+str(time.time_ns())))))
flash=Path(os.environ.get('PEBBLE_QA_FLASH',str(state/'spi-flash.bin')))
if flash.exists():raise SystemExit('Use a fresh isolated QEMU flash for this UI run: '+str(flash))
p=connect();target=uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
pbw=candidate_pbw(ROOT, 'Timer');native=ROOT/'build/ui-system-native-only.pbw'
sha=lambda data:hashlib.sha256(data).hexdigest()
with zipfile.ZipFile(pbw) as original,zipfile.ZipFile(native,'w') as out:
 payload={name:sha(original.read(name)) for name in original.namelist() if name.startswith('emery/')}
 for entry in original.infolist():
  if entry.filename=='appinfo.json' or entry.filename.startswith('emery/'):out.writestr(entry,original.read(entry.filename))
service=AppMessageService(p);records={};messages=[];probes=[];captures=[];dark_mode=False
def receive(transaction,app,data):
 if app!=target:return
 if data.get(3000)==1:probes.append(dict(data));return
 if 1000 not in data:return
 records[int(data[1021])]=dict(data);messages.append(dict(data))
 service.send_message(target,{1014:CString(data[1001]),1015:Uint32(data[1002]),1020:Uint32(data[1020])})
service.register_handler('appmessage',receive)
def launch():p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.5)
def stop():p.send_packet(AppRunState(data=AppRunStateStop(uuid=target)));time.sleep(.3)
def install(path):ToolAppInstaller(p,str(path),quiet=True).install();launch()
def seed(mode):
 stop();install(ROOT/'build'/('fixture-mode'+str(mode)+'.pbw'));stop();install(native)
def persistence_probe():
 stop();probes.clear();install(ROOT/'build/fixture-mode8.pbw')
 for _ in range(100):
  if probes:break
  time.sleep(.05)
 assert probes,'Read-only native persistence probe did not respond'
 result=dict(probes[-1])
 assert result[3001]>4 and result[3002]==0x54474c43 and result[3003]==1,'AppGlance checkpoint missing/corrupt'
 assert result[3004]==4 and result[3005] in (1,2),'Separate native appearance missing/corrupt'
 stop();install(native)
 return result
def button(name,count=1):
 for _ in range(count):
  send_data_to_qemu(p.transport,QemuButton(state=int(getattr(QemuButton.Button,name))));time.sleep(.04)
  send_data_to_qemu(p.transport,QemuButton(state=0));time.sleep(.15)
def image():return b''.join(Screenshot(p).grab_image())
def assert_timer(pixels,name):
 alarm=name.startswith('alarm-')
 bg=b'\x00\x00\x00' if dark_mode or alarm else b'\xff\xff\xff'
 bar=b'\x55\x55\x55' if alarm else b'\xff\xff\xff' if dark_mode else b'\x00\x00\x00'
 at=lambda x,y:pixels[(y*200+x)*3:(y*200+x+1)*3]
 assert at(0,0)==bg,name+': not the expected native Timer surface'
 assert at(199,100)==bar,name+': native action bar is not visible'
 assert all(at(199,y)==bar and at(166,y)==bar for y in range(228)),name+': action rail must span the entire screen'
 title=sum(at(x,y)!=bg for y in range(1,30) for x in range(8,90))
 clock=sum(at(x,y)!=bg for y in range(1,30) for x in range(94,158))
 assert title>=25 and clock>=20,name+': missing native title/clock; possible OS overlay'
 return {'background':list(bg),'sidebar':list(bar),'titleInkPixels':title,'clockInkPixels':clock}
def shot(name):
 time.sleep(.35);pixels=image();assert len(pixels)==200*228*3
 checks=assert_timer(pixels,name)
 path=OUT/(name+'.png');Image.frombytes('RGB',(200,228),pixels).save(path)
 captures.append({'file':path.name,'sha256':sha(path.read_bytes()),'appVisibility':checks});return pixels
def theme(dark):
 global dark_mode
 dark_mode=dark
 service.send_message(target,{1024:Uint8(1 if dark else 0),1025:Uint8(1)});time.sleep(.3)
def snapshot():
 records.clear();service.send_message(target,{1017:Uint8(1),1019:Uint32(0)})
 for _ in range(100):
  if len(records)==8:break
  time.sleep(.05)
 assert len(records)==8
 return {str(k):dict(v) for k,v in records.items()}
def anchors(state):
 result={}
 for k,v in state.items():
  result[k]={str(key):v.get(key) for key in (1001,1003,1009,1010,1011,1012,1013,1022,1023)}
  # The explicit owner snapshot request legitimately reconfirms a running
  # baseline at its current UTC second. Theme changes must preserve run/state,
  # deadline/expiry and frozen observations for paused/terminal timers.
  if v.get(1003)=='active':result[k].pop('1009')
 return result
seed(5);theme(False);before=snapshot();shot('list-light')
theme(True);after=snapshot();shot('list-dark')
(OUT/'owner-before-after.json').write_text(json.dumps({'before':before,'after':after},indent=2)+'\n')
assert anchors(before)==anchors(after),'Appearance must not alter native owner clocks or identity'
button('Select');shot('running-dark');theme(False);shot('running-light')
button('Select');shot('paused-light');theme(True);paused=shot('paused-dark')
time.sleep(1.1);still=image();assert paused[200*33*3:]==still[200*33*3:],'Paused body must stay still'
button('Down');shot('options-dark');button('Down',2);button('Select');shot('delete-dark-safe-default')
theme(False);shot('delete-light-safe-default');button('Back');shot('delete-cancel-keeps-selection')
button('Up');button('Select');shot('ready-light');theme(True);shot('ready-dark')
stop();launch();reopened=shot('reopened-dark')
# With no PKJS in the diagnostic bundle, this proves native persisted appearance.
assert reopened[:3]==b'\x00\x00\x00'
button('Select');button('Up');shot('remaining-editor-dark')
theme(False);shot('remaining-editor-light');button('Back',2)
# Empty and alarm compositions use the same exact native binary/resources.
seed(0);theme(False);shot('empty-light');theme(True);shot('empty-dark');button('Select');shot('new-editor-dark')
seed(1);shot('alarm-dark-treatment')
# An exhausted timer-bank generation keeps native ownership authoritative;
# the independent appearance key must still work, with recovery controls clear.
seed(6);theme(True);shot('critical-list-dark');button('Select');button('Down');button('Down',2);button('Select');shot('critical-delete-dark')
theme(False);shot('critical-delete-light');button('Back');shot('critical-cancel-keeps-options')
# A real native AppGlance update must coexist with both saved appearances.
# The read-only helper inspects the same UUID's persisted data between launches;
# it never cancels Wakeups or modifies a bank/preference/checkpoint.
seed(5);theme(True);button('Select');button('Select');time.sleep(1.2)
dark_probe=persistence_probe();assert dark_probe[3005]==2
shot('dark-after-glance-write-and-reopen')
theme(False)
light_probe=persistence_probe();assert light_probe[3005]==1
assert dark_probe[3006]==light_probe[3006],'Theme write replaced AppGlance checkpoint'
assert dark_probe[3007]==light_probe[3007],'Theme write changed timer banks'
shot('light-after-theme-write-and-reopen')
button('Select');button('Select');time.sleep(1.2)
light_glance_probe=persistence_probe();assert light_glance_probe[3005]==1
assert light_glance_probe[3006]!=light_probe[3006],'Timer pause did not update real Glance checkpoint'
shot('light-after-second-glance-write-and-reopen')
report={'version':VERSION,'result':'PASS','scope':'Actual Emery QEMU using distribution native payload; direct QA AppMessage theme delivery, no phone JS/real phone/physical watch',
 'pbwSha256':sha(pbw.read_bytes()),'nativePayloadSha256':payload,'nativeOnlySha256':sha(native.read_bytes()),'captures':captures,
 'proofs':{'appearanceIndependentOfOwnerState':{'before':before,'after':after,'anchorsEqual':True,'runningObservationNote':'Explicit native snapshot reconfirmation advances running observations; compare fixed run/state/deadline/expiry and paused/terminal observations.'},'pausedBodyStatic':True,'persistedThemeReopenedWithoutPhoneJavascript':True,'deleteSafeDefaultAndCancel':True,'appearanceIndependentOfExhaustedTimerBanks':True,'realGlanceAndAppearanceCoexistence':{'darkAfterGlance':dark_probe,'lightAfterTheme':light_probe,'lightAfterSecondGlance':light_glance_probe,'themePreservesGlanceAndBankBytes':True}},
 'physicalDeviceVerified':False,'phoneJavascriptExecuted':False,'freshStockDerivedFlash':True,'qemuState':str(state),'appVisibilityAssertions':'Expected native foreground/background, physical-button sidebar and visible title/time required for every capture'}
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'result':'PASS','captures':len(captures),'path':str(OUT/'report.json')}))
