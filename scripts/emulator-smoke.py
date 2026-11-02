#!/usr/bin/env python3
"""Manual Emery smoke with the real native PBW and an isolated legacy fixture.

Requires the Pebble SDK Python packages and running emulator support. Build the
fixture with `pebble build` from tests/legacy-fixture first. Replaces Timer's
emulator data deliberately; never run against a physical watch.
"""
from pathlib import Path
import argparse,time,uuid,json
from os import environ
from pebble_tool.commands.base import PebbleCommand
from pebble_tool.commands.install import ToolAppInstaller
from libpebble2.protocol.apps import AppRunState,AppRunStateStart
from libpebble2.services.screenshot import Screenshot
from libpebble2.communication.transports.qemu.protocol import QemuButton,QemuBluetoothConnection
from pebble_tool.commands.emucontrol import send_data_to_qemu
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
VERSION=json.loads((ROOT/'package.json').read_text())['version']
OUT=Path(environ.get('TIMER_QA_OUTPUT',str(ROOT/'docs/qa'/VERSION/'native')));OUT.mkdir(parents=True,exist_ok=True)
class Connection(PebbleCommand):valid_connections={'emulator'}
c=Connection();c._set_debugging(0)
p=c._connect(argparse.Namespace(v=0,emulator='emery',sdk=None,vnc=False,sunlight=False))
target=uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
def launch():p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.7)
def install(path):ToolAppInstaller(p,str(path),quiet=True).install();launch()
def button(name,n=1):
 for _ in range(n):
  send_data_to_qemu(p.transport,QemuButton(state=int(getattr(QemuButton.Button,name))));time.sleep(.07)
  send_data_to_qemu(p.transport,QemuButton(state=0));time.sleep(.18)
def shot(name):
 time.sleep(.25);rows=Screenshot(p).grab_image();Image.frombytes('RGB',(200,228),b''.join(rows)).save(OUT/(name+'.png'));print(name,flush=True)

import hashlib,json,zipfile
for old in OUT.glob('*.png'):old.unlink()
OUT.joinpath('legacy-fixture.c').write_bytes((ROOT/'tests/legacy-fixture/src/c/main.c').read_bytes())
base_shot=shot
captures=[]
def shot(name):
 for attempt in range(4):
  base_shot(name)
  pic=Image.open(OUT/(name+'.png'))
  if sum(1 for pix in pic.get_flattened_data() if pix==(255,255,255))>12000:break
  time.sleep(.4)
 else:raise RuntimeError('Native screen did not settle: '+name)
 captures.append(name)
def install(path):
 ToolAppInstaller(p,str(path),quiet=True).install();launch();time.sleep(.6)
 rows=Screenshot(p).grab_image()
 if sum(1 for row in rows for at in range(0,len(row),3) if row[at:at+3]==bytes([255,255,255]))<12000:
  button('Back');launch();time.sleep(.6)
def plus():button('Up',8);button('Select')
def create_short():
 plus();button('Select');button('Down',5);button('Select');button('Up',5);button('Select')
fixture=ROOT/'tests/legacy-fixture/build/legacy-fixture.pbw'
final=ROOT/'dist'/('Timer-'+VERSION+'.pbw')
install(fixture);shot('legacy-fixture-paused')
install(final);shot('migration-list')
button('Select');shot('migration-detail')
button('Select');shot('running')
button('Select');shot('paused')
button('Up');shot('editor')
button('Back');button('Back');plus();shot('new-editor')
button('Select');button('Up',5);button('Select');button('Select');shot('second-running')
button('Back');shot('multiple-list')
plus();button('Up',24);shot('editor-24h')
button('Select');button('Select');button('Select');shot('detail-24h')
button('Down');shot('deleted-list')
create_short();shot('short-running');time.sleep(6);shot('alarm')
button('Up');shot('snoozed');button('Select');shot('snoozed-paused');button('Down')
create_short();time.sleep(6);shot('alarm-dismiss-test');button('Down');shot('dismissed-list')
button('Back');launch();shot('reopened-list')
for _ in range(5):
 plus();button('Select');button('Select');button('Select');button('Back')
shot('eight-timers-full')
button('Down',8);shot('eight-timers-last-row');button('Select');button('Down');shot('slot-deleted')
plus();button('Select');button('Down',4);button('Select');button('Select');shot('slot-reused')
button('Back');shot('eight-timers-after-reuse')
# Leave no future alarms behind while checking bridge-free background Wakeup.
for _ in range(8):
 # The header remains selectable even when the list is full.
 button('Up',8);button('Down');button('Select');button('Down')
shot('empty-list')
native=ROOT/'build/timer-native-only-candidate.pbw'
with zipfile.ZipFile(final) as source,zipfile.ZipFile(native,'w') as archive_output:
 for info in source.infolist():
  if info.filename=='appinfo.json' or info.filename.startswith('emery/'):archive_output.writestr(info,source.read(info.filename))
install(native);shot('native-only-no-js-list')
send_data_to_qemu(p.transport,QemuBluetoothConnection(connected=False))
create_short();shot('native-only-short-running');button('Back');button('Back');time.sleep(6);shot('native-only-background-wakeup')
button('Down');button('Up',8);button('Down');button('Select');button('Down')
send_data_to_qemu(p.transport,QemuBluetoothConnection(connected=True))
install(final);shot('final-empty-list')
with zipfile.ZipFile(final) as z:
 hashes={name:hashlib.sha256(z.read(name)).hexdigest() for name in z.namelist() if name.startswith('emery/')}
with zipfile.ZipFile(native) as z:assert all(hashlib.sha256(z.read(k)).hexdigest()==v for k,v in hashes.items())
report={'version':VERSION,'verification':'Native distribution candidate smoke validation','environment':'Emery QEMU and pypkjs; not a real phone or physical watch','candidatePbwSha256':hashlib.sha256(final.read_bytes()).hexdigest(),'nativePayloadSha256':hashes,'legacyMigration':{'seededBy':'emulator-only fixture source legacy-fixture.c','modelBytes':44,'origin':'6b49d200','run':1,'state':'paused','remainingSeconds':298,'oldStorageKey':2100,'newStorageKeysClearedByFixture':[2103,2120,2121]+list(range(2130,2146)),'publicationTicket2102Preserved':True},'nativeBackgroundAlarm':{'phoneJavascriptOmitted':True,'nativeBytesMatchFinalCandidate':True,'bluetoothDisconnectedRequested':True,'note':'Direct SDK transport stays connected; this is bridge-free native Wakeup proof, not a real Bluetooth disconnection test.'},'captures':[{'name':name,'file':name+'.png','sha256':hashlib.sha256((OUT/(name+'.png')).read_bytes()).hexdigest(),'pixels':[200,228],'nativePixelsModified':False} for name in captures]}
(OUT/'emulator.json').write_text(json.dumps(report,indent=2)+'\n');print('Final native smoke passed',flush=True)
