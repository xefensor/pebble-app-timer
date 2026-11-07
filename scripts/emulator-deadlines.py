"""Exercise native background Wakeup with equal and nearby deadlines.

Build both tests/deadline-fixture offsets before running. Replaces isolated
emulator storage; never select a physical watch. No phone JavaScript is run.
"""
from pathlib import Path
from repo_workflow import candidate as candidate_pbw, qa_output
import argparse
import hashlib
import json
from os import environ
import time
import uuid
import zipfile

from pebble_tool.commands.base import PebbleCommand
from libpebble2.protocol.apps import AppRunState, AppRunStateStart
from libpebble2.communication.transports.qemu.protocol import QemuButton, QemuBluetoothConnection
from libpebble2.services.screenshot import Screenshot
from pebble_tool.commands.emucontrol import send_data_to_qemu
from pebble_tool.commands.install import ToolAppInstaller
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
VERSION = json.loads((ROOT / 'package.json').read_text())['version']
OUT = Path(environ.get('TIMER_QA_OUTPUT', str(qa_output(ROOT, '') / 'native'))) / 'deadlines'
OUT.mkdir(parents=True, exist_ok=True)
FINAL = candidate_pbw(ROOT, 'Timer')
NATIVE = ROOT / 'build/timer-native-only-candidate.pbw'
for filename in ('timer_model.c', 'timer_model.h', 'timer_collection.c', 'timer_collection.h'):
    assert (ROOT / 'tests/deadline-fixture/src/c' / filename).read_bytes() == (ROOT / 'src/c' / filename).read_bytes(), 'Refresh deadline fixture source copies'
native_hashes = {}
with zipfile.ZipFile(FINAL) as distribution, zipfile.ZipFile(NATIVE) as diagnostic:
    for name in distribution.namelist():
        if name.startswith('emery/'):
            assert distribution.read(name) == diagnostic.read(name)
            native_hashes[name] = hashlib.sha256(diagnostic.read(name)).hexdigest()


class Connection(PebbleCommand):
    valid_connections = {'emulator'}


command = Connection()
command._set_debugging(0)
p = command._connect(argparse.Namespace(v=0, emulator='emery', sdk=None, vnc=False, sunlight=False))
target = uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
captures = []
report = {
    'version': VERSION,
    'scope': 'Native-only Emery QEMU buttons and background Wakeup; no phone runtime or physical watch',
    'environment': environ.get('TIMER_QA_ENVIRONMENT', 'Emery QEMU'),
    'phoneJavascriptExecuted': False,
    'nativeOnlyPbwSha256': hashlib.sha256(NATIVE.read_bytes()).hexdigest(),
    'nativePayloadSha256': native_hashes,
    'candidatePbwSha256': hashlib.sha256(FINAL.read_bytes()).hexdigest(),
    'deadlineOffsetsSeconds': [0, 5], 'durationSeconds': 20,
    'setup': 'Native persistence fixture seeds two running timers before Timer schedules its own Wakeup. The watch clock is not changed during scheduled alarms.',
    'captures': captures,
    'alarmDismissHoldSeconds': .65,
    'result': 'INCOMPLETE',
    'visualReview': {'reviewed': False},
}


def button(name, count=1):
    for _ in range(count):
        send_data_to_qemu(p.transport, QemuButton(state=int(getattr(QemuButton.Button, name))))
        time.sleep(.06)
        send_data_to_qemu(p.transport, QemuButton(state=0))
        time.sleep(.18)


def hold_button(name, seconds):
    send_data_to_qemu(p.transport, QemuButton(state=int(getattr(QemuButton.Button, name))))
    time.sleep(seconds)
    send_data_to_qemu(p.transport, QemuButton(state=0))
    time.sleep(.3)


def shot(name):
    time.sleep(.4)
    pixels = b''.join(Screenshot(p).grab_image())
    path = OUT / (name + '.png')
    image = Image.frombytes('RGB', (200, 228), pixels)
    image.save(path)
    if name.endswith('alarm'):
        # The countdown/configured duration varies. Compare only the native
        # TIME'S UP heading, above that number and outside the action bar.
        reference = Image.open(OUT.parent / 'alarm.png').crop((0, 0, 166, 34)).tobytes()
        actual = image.crop((0, 0, 166, 34)).tobytes()
        if actual != reference:
            raise RuntimeError('Expected the native alarm heading: ' + name)
    captures.append({'name': name, 'file': path.name,
                     'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                     'pixels': [200, 228], 'nativePixelsModified': False})
    (OUT / 'deadlines.json').write_text(json.dumps(report, indent=2) + '\n')
    print(name, flush=True)


def delete_timer():
    button('Down')
    button('Down', 2)
    button('Select')
    button('Down')
    button('Select')


def create(seconds):
    button('Up', 8)
    button('Select')
    button('Select')
    button('Down', 10)
    button('Select')
    button('Up', seconds)
    button('Select')


def clear(count):
    for _ in range(count):
        button('Up', 8)
        button('Down')
        button('Select')
        delete_timer()


def install(path):
    ToolAppInstaller(p, str(path), quiet=True).install()
    p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)))
    time.sleep(1)


install(ROOT / 'tests/legacy-fixture/build/legacy-fixture.pbw')
install(NATIVE)
shot('legacy-migrated-list')
clear(1)
shot('empty-starting-list')
send_data_to_qemu(p.transport, QemuBluetoothConnection(connected=False))
for name, delta in [('simultaneous', 0), ('nearby', 5)]:
    install(ROOT / ('build/deadline-' + name + '-fixture.pbw'))
    install(NATIVE)
    shot(name + '-running-list')
    button('Back')
    time.sleep(22)
    shot(name + '-background-first-alarm')
    if delta:
        time.sleep(delta + 1)
    hold_button('Down', .65)
    shot(name + '-second-alarm')
    button('Down')
    shot(name + '-dismissed-list')
    clear(2)
for _ in range(8):
    button('Up', 8)
    button('Select', 4)
    button('Back')
shot('cleanup-eight-full')
clear(8)
shot('cleanup-empty-list')
before = Image.open(OUT / 'empty-starting-list.png').tobytes()
assert Image.open(OUT / 'cleanup-empty-list.png').tobytes() == before, 'Eight-slot cleanup did not return to the empty list'
create(5)
button('Back', 2)
time.sleep(7)
shot('cleanup-background-alarm')
button('Down')
clear(1)
shot('cleanup-final-empty-list')
assert Image.open(OUT / 'cleanup-final-empty-list.png').tobytes() == before
report['result'] = 'PASS'
report['alarmHoldDoesNotDismissSecond'] = True
report['cleanup'] = {'initial': 'empty-starting-list.png', 'afterEightSlots': 'cleanup-empty-list.png',
                     'afterBackgroundAlarm': 'cleanup-final-empty-list.png', 'rgbEqual': True}
send_data_to_qemu(p.transport, QemuBluetoothConnection(connected=True))
(OUT / 'deadlines.json').write_text(json.dumps(report, indent=2) + '\n')
print('Deadline broker and cleanup scenarios complete', flush=True)
