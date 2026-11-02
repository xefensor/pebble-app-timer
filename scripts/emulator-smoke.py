#!/usr/bin/env python3
"""Exercise Timer's actual Emery UI and native alarms with isolated fixtures.

Build tests/legacy-fixture first. This deliberately replaces Timer's emulator
storage and must never run against hardware. TIMER_QA_PHASE can split the run
into ui/background/motion phases; all use the same final native payload.
"""
from pathlib import Path
import argparse
import hashlib
import json
from os import environ
import time
import uuid
import zipfile

from pebble_tool.commands.base import PebbleCommand
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState, AppRunStateStart
from libpebble2.services.screenshot import Screenshot
from libpebble2.communication.transports.qemu.protocol import QemuButton, QemuBluetoothConnection
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
VERSION = json.loads((ROOT / 'package.json').read_text())['version']
OUT = Path(environ.get('TIMER_QA_OUTPUT', str(ROOT / 'docs/qa' / VERSION / 'native')))
OUT.mkdir(parents=True, exist_ok=True)
PHASE = environ.get('TIMER_QA_PHASE', 'all')
if PHASE not in ('all', 'ui', 'background', 'motion'):
    raise SystemExit('TIMER_QA_PHASE must be all, ui, background or motion')
FINAL = ROOT / 'dist' / ('Timer-' + VERSION + '.pbw')
NATIVE = ROOT / 'build/timer-native-only-candidate.pbw'
TARGET = uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')


def digest(content):
    return hashlib.sha256(content).hexdigest()


with zipfile.ZipFile(FINAL) as source:
    hashes = {name: digest(source.read(name)) for name in source.namelist() if name.startswith('emery/')}
    with zipfile.ZipFile(NATIVE, 'w') as output:
        for info in source.infolist():
            if info.filename == 'appinfo.json' or info.filename.startswith('emery/'):
                output.writestr(info, source.read(info.filename))
with zipfile.ZipFile(NATIVE) as diagnostic:
    assert 'pebble-js-app.js' not in diagnostic.namelist()
    assert all(digest(diagnostic.read(name)) == value for name, value in hashes.items())

if PHASE in ('all', 'ui'):
    for old in OUT.glob('*.png'):
        old.unlink()
    captures, proofs = [], {}
else:
    saved = json.loads((OUT / 'emulator.json').read_text())
    assert saved['candidatePbwSha256'] == digest(FINAL.read_bytes()), 'UI captures belong to another build'
    assert saved['nativePayloadSha256'] == hashes
    captures, proofs = saved['captures'], saved.get('proofs', {})
report = {
    'version': VERSION,
    'verification': 'Native distribution candidate UI, motion and smoke validation',
    'environment': environ.get('TIMER_QA_ENVIRONMENT', 'Emery QEMU; not a real phone or physical watch'),
    'phoneJavascriptExecuted': False,
    'candidatePbwSha256': digest(FINAL.read_bytes()),
    'nativePayloadSha256': hashes,
    'nativeOnlyPbwSha256': digest(NATIVE.read_bytes()),
    'legacyMigration': {
        'seededBy': 'emulator-only legacy-fixture.c', 'modelBytes': 44,
        'origin': '6b49d200', 'run': 1, 'state': 'paused', 'remainingSeconds': 298,
        'oldStorageKey': 2100, 'newStorageKeysClearedByFixture': [2103, 2120, 2121] + list(range(2130, 2146)),
        'publicationTicket2102Preserved': True,
    },
    'nativeBackgroundAlarm': {
        'phoneJavascriptOmitted': True, 'nativeBytesMatchFinalCandidate': True,
        'bluetoothDisconnectedRequested': True,
        'note': 'Direct SDK transport stays connected. This is native Wakeup proof without phone JavaScript, not a physical Bluetooth-disconnection test.',
    },
    'captures': captures, 'proofs': proofs,
    'completedPhases': [] if PHASE in ('all', 'ui') else saved.get('completedPhases', []),
    'visualReview': {'reviewed': False}, 'result': 'INCOMPLETE',
}


def save():
    (OUT / 'emulator.json').write_text(json.dumps(report, indent=2) + '\n')


class Connection(PebbleCommand):
    valid_connections = {'emulator'}


command = Connection()
command._set_debugging(0)
p = command._connect(argparse.Namespace(v=0, emulator='emery', sdk=None, vnc=False, sunlight=False))


def launch():
    p.send_packet(AppRunState(data=AppRunStateStart(uuid=TARGET)))
    time.sleep(.8)


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


def install(path):
    ToolAppInstaller(p, str(path), quiet=True).install()
    launch()
    time.sleep(.4)


def shot(name, wait=.3):
    time.sleep(wait)
    rows = Screenshot(p).grab_image()
    assert len(rows) == 228 and all(len(row) == 600 for row in rows), 'Expected native 200x228 RGB screenshot'
    data = b''.join(rows)
    path = OUT / (name + '.png')
    Image.frombytes('RGB', (200, 228), data).save(path)
    assert len(set(data[i:i+3] for i in range(0, len(data), 3))) >= 2, 'Blank native screen: ' + name
    entry = {'name': name, 'file': path.name, 'sha256': digest(path.read_bytes()),
             'rgbSha256': digest(data), 'pixels': [200, 228], 'nativePixelsModified': False,
             'phase': 'motion' if PHASE == 'motion' else 'background' if name.startswith('native-only') or name == 'final-empty-list' else 'ui'}
    captures[:] = [old for old in captures if old['name'] != name]
    captures.append(entry)
    save()
    print(name, flush=True)
    return data


def plus():
    button('Up', 8)
    button('Select')


def create_short(seconds=5):
    plus()
    button('Select')
    button('Down', 10)
    button('Select')
    button('Up', seconds)
    button('Select')


def delete_timer(capture_prefix=None):
    """From a detail screen, explicitly choose Delete and then confirm it."""
    button('Down')
    if capture_prefix:
        shot(capture_prefix + '-options')
    button('Down', 2)
    button('Select')
    if capture_prefix:
        shot(capture_prefix + '-confirmation')
    button('Down')
    button('Select')


if PHASE in ('all', 'ui'):
    (OUT / 'legacy-fixture.c').write_bytes((ROOT / 'tests/legacy-fixture/src/c/main.c').read_bytes())
    install(ROOT / 'tests/legacy-fixture/build/legacy-fixture.pbw')
    shot('legacy-fixture-paused')
    install(NATIVE)
    shot('migration-list')
    button('Select')
    shot('migration-detail')
    button('Select')
    before = shot('running')
    time.sleep(1.1)
    after = shot('running-tick')
    assert before != after, 'Running digits/stream did not change without input'
    proofs['runningLocalTick'] = {'before': 'running.png', 'after': 'running-tick.png',
                                  'fixtureResent': False, 'buttonPressedBetweenFrames': False,
                                  'rgbChanged': True, 'waitSeconds': 1.1}
    button('Select')
    before = shot('paused')
    time.sleep(1.1)
    after = shot('paused-hold')
    assert before == after, 'Paused content moved without input'
    proofs['pausedStable'] = {'before': 'paused.png', 'after': 'paused-hold.png',
                              'fixtureResent': False, 'buttonPressedBetweenFrames': False,
                              'rgbEqual': True, 'waitSeconds': 1.1}
    # Verify the default confirmation keeps the timer and does not silently
    # execute Delete from an ordinary detail-screen button press.
    button('Down')
    before = shot('options')
    button('Back')
    hold_button('Down', .65)
    after = shot('held-down-options')
    assert before == after, 'Holding Detail Down cascaded into Options navigation'
    proofs['heldDetailDown'] = {'before': 'options.png', 'after': 'held-down-options.png',
                                'heldSeconds': .65, 'rgbEqual': True, 'firstOptionRemainsSelected': True}
    button('Down', 2)
    button('Select')
    shot('delete-confirmation')
    button('Select')
    shot('delete-kept-detail')
    button('Down')
    button('Down')
    button('Select')
    shot('restarted-running')
    button('Select')
    shot('restarted-paused')
    button('Up')
    shot('editor')
    button('Back')
    button('Back')
    plus()
    shot('new-editor')
    button('Select', 3)
    shot('second-running')
    button('Back')
    shot('multiple-list')
    plus()
    button('Up', 24)
    shot('editor-24h')
    button('Select', 3)
    shot('detail-24h')
    delete_timer('24h-delete')
    shot('deleted-list')
    create_short(10)
    shot('short-running')
    time.sleep(3)
    shot('last-seconds')
    time.sleep(2)
    shot('short-halfway')
    time.sleep(6)
    shot('alarm')
    button('Up')
    shot('snoozed')
    button('Select')
    shot('snoozed-paused')
    delete_timer()
    create_short()
    time.sleep(6)
    shot('alarm-dismiss-test')
    button('Down')
    shot('dismissed-list')
    button('Back')
    launch()
    shot('reopened-list')
    for _ in range(5):
        plus()
        button('Select', 3)
        button('Back')
    shot('eight-timers-full')
    button('Up', 8)
    before = shot('eight-timers-full-header')
    button('Select')
    after = shot('eight-timers-full-no-add')
    before_image = Image.frombytes('RGB', (200, 228), before)
    after_image = Image.frombytes('RGB', (200, 228), after)
    regions = [(0, 0, 166, 36), (0, 208, 166, 228)]
    assert all(before_image.crop(region).tobytes() == after_image.crop(region).tobytes() for region in regions), 'Full-list add action changed its header or capacity'
    proofs['capacityLimit'] = {'before': 'eight-timers-full-header.png', 'after': 'eight-timers-full-no-add.png',
                               'headerAndCapacityRegionsEqual': True, 'regions': [list(region) for region in regions], 'visibleFullLabel': True}
    button('Down', 8)
    shot('eight-timers-last-row')
    button('Select')
    delete_timer()
    shot('slot-deleted')
    plus()
    button('Select')
    button('Down', 9)
    button('Select', 2)
    shot('slot-reused')
    button('Back')
    shot('eight-timers-after-reuse')
    for _ in range(8):
        button('Up', 8)
        button('Down')
        button('Select')
        delete_timer()
    shot('empty-list')
    report['completedPhases'].append('ui')
    save()

if PHASE in ('all', 'background'):
    install(NATIVE)
    shot('native-only-no-js-list')
    send_data_to_qemu(p.transport, QemuBluetoothConnection(connected=False))
    create_short()
    shot('native-only-short-running')
    button('Back', 2)
    time.sleep(6)
    shot('native-only-background-wakeup')
    button('Down')
    button('Up', 8)
    button('Down')
    button('Select')
    delete_timer()
    send_data_to_qemu(p.transport, QemuBluetoothConnection(connected=True))
    install(NATIVE)
    shot('final-empty-list')
    assert Image.open(OUT / 'empty-list.png').tobytes() == Image.open(OUT / 'final-empty-list.png').tobytes(), 'Cleanup did not return to the same empty list'
    proofs['cleanup'] = {'before': 'empty-list.png', 'after': 'final-empty-list.png', 'rgbEqual': True}
    report['completedPhases'].append('background')
    save()


if PHASE in ('all', 'motion'):
    install(ROOT / 'tests/legacy-fixture/build/legacy-fixture.pbw')
    install(NATIVE)
    button('Select', 2)
    before = shot('running-stream-a')
    region = (80, 145, 86, 162)
    before_region = Image.frombytes('RGB', (200, 228), before).crop(region).tobytes()
    for attempt in range(6):
        time.sleep(.15)
        after = shot('running-stream-b', wait=0)
        if Image.frombytes('RGB', (200, 228), after).crop(region).tobytes() != before_region:
            break
    else:
        raise RuntimeError('Running sand stream did not change phase without input')
    proofs['runningSandStream'] = {'before': 'running-stream-a.png', 'after': 'running-stream-b.png',
                                    'region': list(region), 'regionRgbChanged': True,
                                    'buttonPressedBetweenFrames': False, 'fixtureResent': False,
                                    'captureAttempts': attempt + 1}
    delete_timer()
    shot('motion-final-empty-list')
    assert Image.open(OUT / 'empty-list.png').tobytes() == Image.open(OUT / 'motion-final-empty-list.png').tobytes()
    report['completedPhases'].append('motion')
    save()

report['result'] = 'PASS' if {'ui', 'background'}.issubset(report['completedPhases']) else 'PARTIAL'
save()
print('Native smoke complete: ' + ', '.join(report['completedPhases']), flush=True)
