"""Exercise native background Wakeup broker with simultaneous and nearby deadlines."""
from pathlib import Path
import hashlib, json, time, uuid, zipfile
from pebble_tool.commands.base import PebbleCommand
import argparse
def connect():
    class Connection(PebbleCommand): valid_connections = {'emulator'}
    command = Connection(); command._set_debugging(0)
    return command._connect(argparse.Namespace(v=0,emulator='emery',sdk=None,vnc=False,sunlight=False))
from libpebble2.protocol.apps import AppRunState, AppRunStateStart
from libpebble2.communication.transports.qemu.protocol import QemuButton, QemuBluetoothConnection
from libpebble2.services.screenshot import Screenshot
from pebble_tool.commands.emucontrol import send_data_to_qemu
from pebble_tool.commands.install import ToolAppInstaller
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/qa/0.2.0/local-recovery/deadlines'
OUT.mkdir(parents=True,exist_ok=True)
for filename in ('timer_model.c','timer_model.h','timer_collection.c','timer_collection.h'):
    assert (ROOT/'tests/deadline-fixture/src/c'/filename).read_bytes() == (ROOT/'src/c'/filename).read_bytes(), 'Refresh deadline fixture source copies'
p = connect(); target = uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
captures = []
def button(name, count=1):
    for _ in range(count):
        send_data_to_qemu(p.transport, QemuButton(state=int(getattr(QemuButton.Button, name)))); time.sleep(.07)
        send_data_to_qemu(p.transport, QemuButton(state=0)); time.sleep(.18)
def shot(name):
    time.sleep(.4)
    pixels = b''.join(Screenshot(p).grab_image())
    path = OUT / (name + '.png'); Image.frombytes('RGB', (200,228), pixels).save(path)
    if name.endswith('alarm'):
        reference = Image.open(OUT.parent / 'alarm.png').crop((0,0,200,125)).tobytes()
        actual = Image.open(path).crop((0,0,200,125)).tobytes()
        if actual != reference: raise RuntimeError('Expected the native alarm screen: ' + name)
    captures.append({'file':path.name, 'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    print(name, flush=True)
def create(seconds):
    button('Up',8); button('Select'); button('Select'); button('Down',5)
    button('Select'); button('Up',seconds); button('Select')
def clear():
    # The list retains dismissed completed timers until explicitly deleted.
    for _ in range(2):
        button('Up',8); button('Down'); button('Select'); button('Down')

native = ROOT / 'build/timer-native-only-candidate.pbw'
with zipfile.ZipFile(ROOT/'dist/Timer-0.2.0.pbw') as distribution, zipfile.ZipFile(native) as diagnostic:
    for name in distribution.namelist():
        if name.startswith('emery/'): assert distribution.read(name) == diagnostic.read(name)
def install(path):
    ToolAppInstaller(p, str(path), quiet=True).install()
    p.send_packet(AppRunState(data=AppRunStateStart(uuid=target))); time.sleep(1)
    pixels = b''.join(Screenshot(p).grab_image())
    if sum(pixels[i:i+3] == b'\xff\xff\xff' for i in range(0,len(pixels),3)) < 12000:
        button('Back'); p.send_packet(AppRunState(data=AppRunStateStart(uuid=target))); time.sleep(1)
install(ROOT / 'tests/legacy-fixture/build/legacy-fixture.pbw')
install(native)
shot('legacy-migrated-list')
button('Up',8); button('Down'); button('Select'); button('Down')
shot('empty-starting-list')
send_data_to_qemu(p.transport,QemuBluetoothConnection(connected=False))
for name, delta in [('simultaneous',0), ('nearby',5)]:
    install(ROOT / ('build/deadline-' + name + '-fixture.pbw'))
    install(native); shot(name+'-running-list')
    button('Back'); time.sleep(22)
    shot(name+'-background-first-alarm')
    if delta: time.sleep(delta+1)
    button('Down'); shot(name+'-second-alarm')
    button('Down'); shot(name+'-dismissed-list'); clear()
# Replay corrected cleanup with all eight slots occupied, then the isolated
# background alarm. Retain unaffected UI captures from the earlier smoke phase.
for _ in range(8):
    button('Up',8);button('Select');button('Select');button('Select');button('Select');button('Back')
shot('cleanup-eight-full')
for _ in range(8):
    button('Up',8);button('Down');button('Select');button('Down')
shot('cleanup-empty-list')
before = Image.open(OUT / 'empty-starting-list.png').crop((0,0,200,200)).tobytes()
assert Image.open(OUT / 'cleanup-empty-list.png').crop((0,0,200,200)).tobytes() == before
Image.open(OUT / 'cleanup-empty-list.png').save(OUT.parent / 'empty-list.png')
create(5); button('Back'); button('Back'); time.sleep(7)
shot('cleanup-background-alarm'); button('Down')
button('Up',8);button('Down');button('Select');button('Down')
shot('cleanup-final-empty-list')
assert Image.open(OUT / 'cleanup-final-empty-list.png').crop((0,0,200,200)).tobytes() == before
Image.open(OUT / 'cleanup-final-empty-list.png').save(OUT.parent / 'final-empty-list.png')
smoke_path = OUT.parent / 'emulator.json'
smoke = json.loads(smoke_path.read_text())
for capture in smoke['captures']:
    if capture['name'] in ('empty-list','final-empty-list'):
        capture['sha256'] = hashlib.sha256((OUT.parent/capture['file']).read_bytes()).hexdigest()
        capture['capturePhase'] = 'Focused replay of corrected eight-slot cleanup and isolated background alarm'
smoke['cleanupReplay'] = 'Corrected header navigation replayed with eight occupied slots; empty list content equals initial empty-list pixels; transport status footer is excluded from equality.'
smoke_path.write_text(json.dumps(smoke,indent=2)+'\n')
send_data_to_qemu(p.transport,QemuBluetoothConnection(connected=True))
report={'version':'0.2.0','scope':'Fresh native-only Emery QEMU button and background Wakeup checks; no phone runtime or physical watch',
        'nativeOnlyPbwSha256':hashlib.sha256(native.read_bytes()).hexdigest(),
        'candidatePbwSha256':hashlib.sha256((ROOT/'dist/Timer-0.2.0.pbw').read_bytes()).hexdigest(),'deadlineOffsetsSeconds':[0,5],'durationSeconds':20,'setup':'Native persistence fixture seeds two running timers before Timer schedules its own Wakeup; the watch clock is not changed during scheduled alarms','captures':captures,
        'visualReviewRequired':True}
(OUT/'deadlines.json').write_text(json.dumps(report,indent=2)+'\n')
print('Deadline broker scenarios captured; inspect both alarm screens', flush=True)
