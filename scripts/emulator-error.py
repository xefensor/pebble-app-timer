"""Capture Timer's real alarm-scheduling error without changing its binary.

The emulator-only different-UUID fixture owns an external Wakeup inside the
SDK's 60-second exclusion interval. It is explicitly cleared before returning.
Never run this storage-replacing procedure against a physical watch.
"""
from pathlib import Path
from repo_workflow import candidate as candidate_pbw, qa_output
import argparse
import hashlib
import json
from os import environ
import sys
import time
import uuid
import zipfile

from pebble_tool.commands.base import PebbleCommand
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState, AppRunStateStart
from libpebble2.services.screenshot import Screenshot
from libpebble2.communication.transports.qemu.protocol import QemuButton
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
VERSION = json.loads((ROOT / 'package.json').read_text())['version']
OUT = Path(environ.get('TIMER_QA_OUTPUT', str(qa_output(ROOT, '') / 'native'))) / 'errors'
OUT.mkdir(parents=True, exist_ok=True)
FINAL = candidate_pbw(ROOT, 'Timer')
NATIVE = Path(environ.get('TIMER_QA_NATIVE_PBW', ROOT / 'build/timer-error-native-only.pbw'))
FIXTURE = ROOT / 'tests/wakeup-conflict-fixture/build/wakeup-conflict-fixture.pbw'
TARGET = uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
CONFLICT = uuid.UUID('d012c8aa-b7ce-4fc3-82f7-f45c8a58f4d1')
candidate_hash = hashlib.sha256(FINAL.read_bytes()).hexdigest()
if environ.get('TIMER_QA_EXPECT_SHA256'):
    assert candidate_hash == environ['TIMER_QA_EXPECT_SHA256'], 'Candidate differs from the reviewed build'


class Connection(PebbleCommand):
    valid_connections = {'emulator'}


if environ.get('TIMER_QA_PIPE') == '1':
    sys.path.insert(0, str(ROOT.parent / 'recovery-validation'))
    from pipe_emulator import connect
    p = connect()
else:
    command = Connection()
    command._set_debugging(0)
    p = command._connect(argparse.Namespace(v=0, emulator='emery', sdk=None, vnc=False, sunlight=False))
captures = []
report = {
    'version': VERSION,
    'scope': 'Actual native Timer scheduling-error integration in Emery QEMU; no phone JavaScript or physical watch',
    'candidatePbwSha256': candidate_hash,
    'fixtureUuid': str(CONFLICT), 'fixturePbwSha256': hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
    'fixtureSourceSha256': hashlib.sha256((ROOT / 'tests/wakeup-conflict-fixture/src/c/main.c').read_bytes()).hexdigest(),
    'fixtureWakeupOffsetSeconds': 600, 'phoneJavascriptExecuted': False,
    'captures': captures, 'fixtureCleanupCompleted': False, 'result': 'INCOMPLETE',
    'visualReview': {'reviewed': False},
}
with zipfile.ZipFile(FINAL) as distribution, zipfile.ZipFile(NATIVE, 'w') as native:
    report['nativePayloadSha256'] = {}
    for name in distribution.namelist():
        if name.startswith('emery/'):
            report['nativePayloadSha256'][name] = hashlib.sha256(distribution.read(name)).hexdigest()
        if name == 'appinfo.json' or name.startswith('emery/'):
            native.writestr(name, distribution.read(name))


def launch(app=TARGET):
    p.send_packet(AppRunState(data=AppRunStateStart(uuid=app)))
    time.sleep(.8)


def install(path, app=TARGET):
    ToolAppInstaller(p, str(path), quiet=True).install()
    launch(app)


def button(name, count=1):
    for _ in range(count):
        send_data_to_qemu(p.transport, QemuButton(state=int(getattr(QemuButton.Button, name))))
        time.sleep(.06)
        send_data_to_qemu(p.transport, QemuButton(state=0))
        time.sleep(.18)


def delete_timer():
    button('Down', 3)
    button('Select')
    button('Down')
    button('Select')


def save():
    (OUT / 'errors.json').write_text(json.dumps(report, indent=2) + '\n')


def shot(name):
    time.sleep(.4)
    path = OUT / (name + '.png')
    Image.frombytes('RGB', (200, 228), b''.join(Screenshot(p).grab_image())).save(path)
    captures.append({'name': name, 'file': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                     'pixels': [200, 228], 'nativePixelsModified': False,
                     'source': 'External Wakeup fixture, not Timer' if name in ('external-wakeup-fixture', 'fixture-cleared') else 'Exact candidate Timer native binary'})
    save()
    print(name, flush=True)
    return Image.open(path).convert('RGB')


# Begin with a known single paused legacy timer, then clear it through the UI.
install(ROOT / 'tests/legacy-fixture/build/legacy-fixture.pbw')
install(NATIVE)
button('Select')
delete_timer()
shot('empty-before-conflict')
try:
    install(FIXTURE, CONFLICT)
    shot('external-wakeup-fixture')
    install(NATIVE)
    # Empty list: Select opens the default editor at Minutes; two more Select
    # presses advance to Seconds and start. A fourth press would pause it.
    button('Select', 3)
    failed = shot('alarm-not-set')
    assert failed.getpixel((1, 200)) == (255, 170, 0), 'Expected the actual Timer critical-warning footer'
    button('Down')
    time.sleep(.7)
    failed_options = shot('alarm-not-set-options')
    assert failed_options.getpixel((1, 200)) == (255, 170, 0), 'Alarm warning disappeared in Options'
finally:
    # Launching the fixture replaces its own Wakeup; Select then cancels it.
    # It cannot cancel or modify Timer's separate native ownership/storage.
    install(FIXTURE, CONFLICT)
    button('Select')
    shot('fixture-cleared')
    report['fixtureCleanupCompleted'] = True
    save()
install(NATIVE)
button('Select')
recovered = shot('alarm-recovered')
assert recovered.getpixel((1, 200)) == (255, 255, 255), 'Critical footer remained after native scheduling recovered'
delete_timer()
shot('empty-after-recovery')
assert Image.open(OUT / 'empty-before-conflict.png').tobytes() == Image.open(OUT / 'empty-after-recovery.png').tobytes(), 'Recovery did not return to the same empty list'
report['result'] = 'PASS'
report['proofs'] = {'alarmWarningVisibleInDetailAndOptions': True, 'warningClearsAfterNativeRecovery': True, 'emptyListRestored': True}
save()
print('Alarm-scheduling error and recovery captured; fixture Wakeup cleared', flush=True)
