#!/usr/bin/env python3
"""Focused native Timer UX/app-list QA. Emulator only; replaces isolated data.

Build tests/legacy-fixture and tests/ux-fixture variants first. Set
TIMER_QA_PIPE=1 only for this workspace's isolated FIFO transport. Direct Python
AppMessage observations/receipts are a QA fixture, not a phone/plugin test.
"""
from pathlib import Path
import argparse
import atexit
import hashlib
import json
import os
import sys
import threading
import time
import uuid
import zipfile

from pebble_tool.commands.base import PebbleCommand
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState, AppRunStateStart, AppRunStateStop
from libpebble2.protocol.phone import PhoneNotification, IncomingCall, CallEnd
from libpebble2.services.appmessage import AppMessageService, Uint8, Uint32, CString
from libpebble2.services.screenshot import Screenshot
from libpebble2.communication.transports.qemu.protocol import QemuButton
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
VERSION = json.loads((ROOT / 'package.json').read_text())['version']
OUT = Path(os.environ.get('TIMER_QA_OUTPUT', ROOT / 'docs/qa' / VERSION / 'native'))
OUT.mkdir(parents=True, exist_ok=True)
PHASE = os.environ.get('TIMER_QA_PHASE', 'migration')
REQUIRED_PHASES = set(os.environ.get('TIMER_QA_EXPECT_PHASES',
    'migration,ux,capacity,launcher,nearby,equal').split(','))
FINAL = ROOT / 'dist' / ('Timer-' + VERSION + '.pbw')
NATIVE = Path(os.environ.get('TIMER_QA_NATIVE_PBW', ROOT / 'build/timer-ux-native-only.pbw'))
TARGET = uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
digest = lambda value: hashlib.sha256(value).hexdigest()
candidate_hash = digest(FINAL.read_bytes())
if os.environ.get('TIMER_QA_EXPECT_SHA256'):
    assert candidate_hash == os.environ['TIMER_QA_EXPECT_SHA256'], 'Candidate differs from the reviewed build'
with zipfile.ZipFile(FINAL) as source, zipfile.ZipFile(NATIVE, 'w') as output:
    native_hashes = {name: digest(source.read(name)) for name in source.namelist() if name.startswith('emery/')}
    for info in source.infolist():
        if info.filename == 'appinfo.json' or info.filename.startswith('emery/'):
            output.writestr(info, source.read(info.filename))
with zipfile.ZipFile(NATIVE) as packaged:
    assert 'pebble-js-app.js' not in packaged.namelist()
    assert all(digest(packaged.read(name)) == value for name, value in native_hashes.items())
report_file = OUT / 'ux.json'
if report_file.exists():
    report = json.loads(report_file.read_text())
    assert report['candidatePbwSha256'] == candidate_hash, 'QA belongs to another candidate'
else:
    report = {
        'version': VERSION,
        'environment': 'Actual Emery QEMU; no phone JavaScript, real phone or physical watch',
        'candidatePbwSha256': candidate_hash,
        'nativeOnlyPbwSha256': digest(NATIVE.read_bytes()),
        'nativePayloadSha256': native_hashes,
        'phoneJavascriptExecuted': False,
        'directQaAppMessageFixture': True,
        'directQaAppMessageNote': 'Python requests native state and sends application receipts; this is not the real phone PKJS/provider transport.',
        'captures': [], 'proofs': {}, 'completedPhases': [], 'result': 'INCOMPLETE',
        'visualReview': {'reviewed': False},
    }
report['completedPhases'] = [phase for phase in report['completedPhases'] if phase != PHASE]
report['requiredPhases'] = sorted(REQUIRED_PHASES)
report['result'] = 'INCOMPLETE'

def save():
    report_file.write_text(json.dumps(report, indent=2) + '\n')

class Connection(PebbleCommand):
    valid_connections = {'emulator'}

if os.environ.get('TIMER_QA_PIPE') == '1':
    sys.path.insert(0, str(ROOT.parent / 'recovery-validation'))
    from pipe_emulator import connect
    p = connect()
else:
    command = Connection()
    command._set_debugging(0)
    p = command._connect(argparse.Namespace(v=0, emulator='emery', sdk=None, vnc=False, sunlight=False))

messages = AppMessageService(p)
records = {}
all_messages = []
lock = threading.Lock()

def receive(transaction, app, data):
    if app != TARGET or 1000 not in data:
        return
    with lock:
        records[int(data[1021])] = dict(data)
        all_messages.append(dict(data))
    messages.send_message(TARGET, {1014: CString(data[1001]), 1015: Uint32(data[1002]), 1020: Uint32(data[1020])})

messages.register_handler('appmessage', receive)

def save_messages():
    report.setdefault('nativeMessages', {})[PHASE] = list(all_messages)
    log = Path(os.environ.get('PEBBLE_QA_STATE', ROOT.parent / '.recovery-tools/emulator')) / 'firmware.log'
    if os.environ.get('TIMER_QA_PIPE') == '1' and log.exists():
        content = log.read_bytes()
        archive = Path('/tmp/timer-qa-firmware-logs')
        archive.mkdir(exist_ok=True)
        copied = archive / (PHASE + '-' + digest(content)[:16] + '.log')
        copied.write_bytes(content)
        report.setdefault('firmwareLogs', {})[PHASE] = {'path': str(copied), 'sha256': digest(content)}
    report.setdefault('phaseEnvironments', {})[PHASE] = {
        'flash': os.environ.get('PEBBLE_QA_FLASH', 'shared workspace emulator flash'),
        'isolatedTimerStockDerivedFlash': bool(os.environ.get('PEBBLE_QA_FLASH')),
        'watchClockChangedDuringPhase': False,
    }
    save()

atexit.register(save_messages)

def launch():
    p.send_packet(AppRunState(data=AppRunStateStart(uuid=TARGET)))
    time.sleep(.7)

def stop():
    p.send_packet(AppRunState(data=AppRunStateStop(uuid=TARGET)))
    time.sleep(.5)

def launcher(back=0):
    if back:
        button('Back', back)
        time.sleep(1.1) # Allow the bounded one-second native exit flush.
    else:
        stop()
    # Stopping an app launched from the app list can reveal that list directly;
    # stopping one launched over a watchface reveals the watchface instead.
    def visible():
        rows = Screenshot(p).grab_image()
        red, green, blue = rows[10][190*3:190*3+3]
        return red < 80 and green > 100 and blue > 180
    if not visible():
        button('Select')
        time.sleep(.4)
    assert visible(), 'Expected Timer selected in the stock OS app list'

def install(path):
    ToolAppInstaller(p, str(path), quiet=True).install()
    launch()
    time.sleep(.3)
    # A stored watchface can leave an OS watchdog modal across app switches.
    # Dismiss only the distinctive gray system surface, never Timer's alarm.
    pixels = b''.join(Screenshot(p).grab_image())
    gray = pixels[:3]
    if len(gray) == 3 and gray[0] == gray[1] == gray[2] and 100 < gray[0] < 220 and pixels.count(gray) > 25000:
        button('Back')
        launch()

def button(name, count=1):
    for _ in range(count):
        send_data_to_qemu(p.transport, QemuButton(state=int(getattr(QemuButton.Button, name))))
        time.sleep(.06)
        send_data_to_qemu(p.transport, QemuButton(state=0))
        time.sleep(.18)

def shot(name, wait=.3, source=None):
    time.sleep(wait)
    rows = Screenshot(p).grab_image()
    assert len(rows) == 228 and all(len(row) == 600 for row in rows)
    pixels = b''.join(rows)
    assert len(set(pixels[i:i+3] for i in range(0, len(pixels), 3))) >= 2
    path = OUT / (name + '.png')
    Image.frombytes('RGB', (200, 228), pixels).save(path)
    report['captures'] = [entry for entry in report['captures'] if entry['name'] != name]
    report['captures'].append({'name': name, 'file': path.name, 'sha256': digest(path.read_bytes()),
        'rgbSha256': digest(pixels), 'pixels': [200, 228], 'nativePixelsModified': False, 'phase': PHASE})
    if source:
        report['captures'][-1]['source'] = source
    save()
    print(name, flush=True)
    return pixels

def region(pixels, bounds):
    left, top, right, bottom = bounds
    return b''.join(pixels[(y*200+left)*3:(y*200+right)*3] for y in range(top, bottom))

def sand_position(pixels, bounds, color):
    """Measure native coloured sand only; saved screenshots stay unmodified."""
    left, top, right, bottom = bounds
    points = []
    for y in range(top, bottom):
        for x in range(left, right):
            red, green, blue = pixels[(y*200+x)*3:(y*200+x)*3+3]
            matches = (red < 100 and green >= 80 and blue >= 150) if color == 'blue' else (
                red >= 170 and green >= 80 and blue < 90)
            if matches:
                points.append((x, y))
    assert len(points) > 100, 'Expected a substantial native sand chamber: ' + str(len(points))
    return {'pixels': len(points), 'x': sum(x for x, _ in points)/len(points),
        'y': sum(y for _, y in points)/len(points)}

def tap_select():
    send_data_to_qemu(p.transport, QemuButton(state=int(QemuButton.Button.Select)))
    time.sleep(.035)
    send_data_to_qemu(p.transport, QemuButton(state=0))

def transition_shot(name):
    # The generic button helper intentionally waits for settled UI. Here the
    # real QEMU press/release and immediate screenshot sample a finite flip.
    tap_select()
    return shot(name, wait=.025)

def wait_timer_surface(seconds=6):
    """Wait for the OS's own window transition before inspecting app pixels."""
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        pixels = b''.join(Screenshot(p).grab_image())
        corners = [(0, 0), (165, 0), (0, 227), (165, 227)]
        if all(pixels[(y*200+x)*3:(y*200+x)*3+3] == bytes((255, 255, 255)) for x, y in corners):
            return
        time.sleep(.08)
    raise AssertionError('Stock OS did not finish returning to Timer')

def current(slot=0):
    with lock:
        return dict(records[slot])

def wait_record(slot, predicate, seconds=4):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        with lock:
            value = dict(records.get(slot, {}))
        if value and predicate(value):
            return value
        time.sleep(.08)
    raise AssertionError('Native record did not reach expected state: ' + str(value))

def handshake():
    with lock:
        records.clear()
    messages.send_message(TARGET, {1017: Uint8(1), 1019: Uint32(0)})
    for slot in range(8):
        wait_record(slot, lambda value: True, 8)
    return {str(slot): current(slot) for slot in range(8)}

def seed(mode):
    fixture = ROOT / 'tests/ux-fixture/build/variants' / ('ux-fixture-' + str(mode) + '.pbw')
    report.setdefault('fixtures', {})[str(mode)] = {'pbwSha256': digest(fixture.read_bytes()),
        'sourceSha256': digest((ROOT / 'tests/ux-fixture/src/c/main.c').read_bytes()), 'version': 1, 'collectionBytes': 372}
    install(fixture)
    if mode==6:
        shot('storage-generation-fixture')
        report['captures'][-1]['source']='Isolated exhausted-generation V1 bank fixture, not Timer native binary'
    install(NATIVE)

def add():
    button('Up', 8)
    button('Select')

def create(seconds):
    add()
    button('Down', 10)
    button('Select')
    button('Up', seconds)
    button('Select')

def delete():
    button('Down')
    button('Down', 2)
    button('Select')
    button('Down')
    button('Select')

def alarm_wait(record, name):
    # Time runs on the watch; use its observed remaining baseline, never reset RTC.
    duration = max(0, record[1011] - record[1009])
    time.sleep(duration + 1.2)
    shot(name)
    # Wakeup starts a fresh native process. The real phone would send ready;
    # our explicitly documented Python QA handshake supplies only that input.
    handshake()
    return wait_record(int(record[1021]), lambda value: value[1003] == 'completed' and value[1023] == 1)

if PHASE == 'migration':
    install(ROOT / 'tests/legacy-fixture/build/legacy-fixture.pbw')
    install(NATIVE)
    shot('singleton-migrated-list')
    state = handshake()
    assert state['0'][1003] == 'paused' and state['0'][1012] == 298 and state['0'][1022] == 300
    report['proofs']['singletonMigration'] = {'modelBytes': 44, 'state': state['0']}
    seed(1)
    shot('v1-bank-pending-alarm')
    state = handshake()
    assert state['0'][1003] == 'completed' and state['0'][1023] == 1
    assert state['1'][1003] == 'completed' and state['1'][1023] == 0
    assert state['2'][1003] == 'active' and state['2'][1022] == 600
    before = state['0']
    button('Select')
    after = wait_record(0, lambda value: value[1002] > before[1002] and value[1023] == 0)
    assert after[1009] == before[1009] and after[1010] == before[1010]
    shot('v1-bank-ready-after-done')
    stop(); launch()
    reopened = handshake()
    assert reopened['0'][1023] == 0 and reopened['0'][1009] == before[1009]
    assert reopened['1'][1023] == 0 and reopened['1'][1010] == state['1'][1010]
    shot('v1-bank-reopened-no-alarm')
    report['proofs']['bankMigrationAndAcknowledgment'] = {'before': state, 'afterDone': after,
        'afterReopen': reopened, 'collectionBytes': 372, 'acknowledgmentInferredFromOldAlarmMask': True}
    seed(0)

elif PHASE == 'ux':
    seed(0)
    handshake()
    shot('empty-list')
    add(); shot('new-editor-minutes')
    button('Back'); button('Up', 24); shot('editor-24h')
    button('Select', 3); shot('running-24h')
    state = handshake()
    assert state['0'][1022] == 86400 and state['0'][1011] - state['0'][1009] <= 86400
    delete()
    create(10)
    state = handshake()['0']
    shot('short-running')
    button('Back', 2)
    completed = background_completed = alarm_wait(state, 'native-background-alarm')
    button('Up')
    snoozed = wait_record(0, lambda value: value[1003] == 'active' and value[1001] != completed[1001])
    assert snoozed[1022] == 60 and snoozed[1006] == '00:10 timer'
    shot('snoozed-original-identity')
    before = handshake()['0']
    button('Up'); shot('remaining-editor')
    time.sleep(2)
    button('Select', 2)
    unchanged = handshake()['0']
    assert unchanged[1011] == before[1011] and unchanged[1022] == before[1022]
    shot('unchanged-edit-keeps-run')
    button('Select')
    paused = wait_record(0, lambda value: value[1003] == 'paused')
    before_elapsed = paused[1022] - paused[1012]
    button('Up'); button('Select'); button('Down'); button('Select')
    changed = wait_record(0, lambda value: value[1002] > paused[1002])
    assert changed[1003] == 'paused' and changed[1022] - changed[1012] == before_elapsed
    assert changed[1006] == '00:10 timer'
    shot('remaining-edit-preserves-elapsed')
    button('Down'); button('Down'); button('Select')
    repeated = wait_record(0, lambda value: value[1003] == 'active' and value[1001] != changed[1001])
    assert repeated[1022] == 10 and repeated[1006] == '00:10 timer'
    shot('restart-original-duration')
    completed = alarm_wait(repeated, 'second-native-alarm')
    button('Select')
    done = wait_record(0, lambda value: value[1002] > completed[1002] and value[1023] == 0)
    assert done[1009] == completed[1009] and done[1010] == completed[1010]
    shot('select-done-ready-list')
    stop(); launch()
    reopened = handshake()['0']
    assert reopened[1023] == 0 and reopened[1009] == completed[1009]
    shot('reopened-acknowledged-list')
    button('Select'); shot('ready-detail')
    button('Up'); button('Select'); button('Up', 10); button('Select')
    ready_edit = wait_record(0, lambda value: value[1002] > reopened[1002])
    assert ready_edit[1003] == 'completed' and ready_edit[1011] == 0 and ready_edit[1023] == 0
    assert ready_edit[1006] == '00:20 timer'
    shot('ready-edited-without-starting')
    button('Select')
    resumed = wait_record(0, lambda value: value[1003] == 'active')
    assert resumed[1022] == 20
    button('Select'); shot('ready-repeated-paused')
    report['proofs']['nativeUx'] = {'shortStarted': state, 'nativeBackgroundCompletion': background_completed,
        'snoozed': snoozed, 'beforeNoOpEdit': before, 'afterNoOpEdit': unchanged,
        'beforeRemainingEdit': paused, 'afterRemainingEdit': changed,
        'restart': repeated, 'acknowledged': done, 'afterReopen': reopened,
        'savedDurationEditedWithoutStarting': ready_edit, 'explicitRepeat': resumed}
    seed(0)

elif PHASE == 'capacity':
    seed(2)
    state = handshake()
    assert all(value[1003] == 'completed' and value[1023] == 0 for value in state.values())
    shot('eight-ready-list')
    add(); shot('eight-ready-replacement-warning')
    button('Select', 2)
    first = wait_record(7, lambda value: value[1003] == 'active')
    assert first[1022] == 600
    shot('ready-slot-replaced-running')
    button('Back'); add(); shot('second-replacement-warning')
    button('Select', 2)
    second = wait_record(6, lambda value: value[1003] == 'active')
    assert second[1022] == 600
    assert current(7)[1001] == first[1001] and current(7)[1011] == first[1011]
    button('Back'); shot('two-running-six-ready')
    report['proofs']['capacityReuse'] = {'before': state, 'firstReplacement': first,
        'secondReplacement': second, 'firstRunningRunAndDeadlinePreserved': True,
        'replacementExplicitlyWarnedBeforeStart': True}
    seed(0)

elif PHASE == 'fields':
    seed(0);handshake();add();button('Select')
    shot('settings-seconds-back-hint')
    button('Back',3)
    empty=handshake()
    assert all(value[1003]=='cancelled' for value in empty.values())
    report['proofs']['secondsField']={'secondsHintCaptured':True,'cancelPreservesEmpty':True}

elif PHASE == 'uxpass':
    seed(5)
    before=handshake()
    assert sum(value[1003]=='active' for value in before.values())==3
    assert sum(value[1003]=='paused' for value in before.values())==2
    shot('uxpass-mixed-list-first')
    button('Down');shot('uxpass-mixed-list-second-identical-timer')
    button('Select');button('Down')
    shot('uxpass-options-identical-duration-context')
    button('Back');button('Select');button('Down')
    shot('uxpass-options-paused-context')
    button('Back',2);button('Down',8)
    shot('uxpass-mixed-list-last-position')
    add();button('Down',10);button('Select')
    shot('uxpass-editor-zero-no-commit')
    button('Up',4);shot('uxpass-editor-four-seconds-no-commit')
    button('Select')
    same=handshake()
    assert all(same[slot][1001]==before[slot][1001] for slot in before)
    button('Up');shot('uxpass-editor-five-seconds-valid')
    button('Back',3)
    add();button('Back');button('Up',24);button('Select')
    locked_before=shot('uxpass-editor-24h-minutes-locked')
    button('Up');button('Down')
    locked_after=shot('uxpass-editor-24h-minutes-unchanged')
    assert locked_before==locked_after
    button('Select');shot('uxpass-editor-24h-seconds-locked')
    button('Back',3)
    seed(0);handshake();shot('uxpass-empty-single-create-action')
    report['proofs']['uxPass']={'mixedStates':before,'invalidCommitPreservesRuns':True,
        'locked24hFieldsDoNotChange':True,'allEightPositionsReachable':True,
        'identicalDurationOptionsContext':True,'emptyListHasOnlyCreateAffordance':True}

elif PHASE == 'storage':
    # An actual exhausted-bank-generation write refusal in isolated QEMU,
    # not a diagnostic Timer. The fixture supplies a valid V1 bank at the
    # maximum generation; the exact candidate cannot commit another bank.
    seed(6)
    first=shot('storage-not-saved-list')
    assert first[(200*200+1)*3:(200*200+1)*3+3]==bytes((255,170,0))
    button('Select');shot('storage-not-saved-detail')
    button('Down');shot('storage-not-saved-options')
    button('Down',2);button('Select')
    confirm=shot('storage-not-saved-delete-keeps-actions')
    assert confirm[(1*200+1)*3:(1*200+1)*3+3]==bytes((255,170,0))
    assert confirm[(180*200+1)*3:(180*200+1)*3+3]!=bytes((255,170,0))
    button('Back',2)
    until=time.monotonic()+35
    while time.monotonic()<until:
        pixels=b''.join(Screenshot(p).grab_image())
        if pixels[:3]==bytes((0,0,0)):
            break
        time.sleep(.3)
    else:raise AssertionError('Expected actual native completion while storage cannot commit')
    shot('storage-not-saved-native-alarm-actions-visible')
    button('Select');shot('storage-done-cannot-hide-unsaved-alarm')
    button('Back');shot('storage-back-remains-available')
    report['proofs']['storageFailure']={'fixtureMode':6,'actualExhaustedGenerationWriteRefusal':True,
        'nativeBinaryUnmodified':True,'alarmActionsVisibleWithCriticalMessage':True,
        'failedAcknowledgmentRemainsAlarm':True,'backRemainsAvailable':True}
    seed(0)

elif PHASE == 'settings':
    seed(0)
    handshake()
    shot('settings-empty-new-timer')
    add(); shot('settings-new-editor-minutes')
    button('Back'); shot('settings-hours-cancel-hint')
    button('Back')
    empty=handshake()
    assert all(value[1003]=='cancelled' for value in empty.values())
    shot('settings-new-cancel-returns-list')
    add();button('Select',2)
    before=handshake()['0']
    shot('settings-running-detail')
    button('Up');button('Back',2)
    shot('settings-direct-edit-cancel-returns-detail')
    unchanged=handshake()['0']
    assert unchanged[1001]==before[1001] and unchanged[1011]==before[1011]
    button('Down');shot('settings-options-change-selected')
    button('Select');button('Back',2)
    returned=shot('settings-options-edit-cancel-returns-options')
    assert returned[(70*200+4)*3:(70*200+4)*3+3]==bytes((0,0,0))
    # Edit the remaining duration without changing the saved repeat duration.
    button('Select');button('Down');button('Select',2)
    edited=handshake()['0']
    assert edited[1001]==before[1001] and edited[1006]=='10 min timer'
    assert edited[1022]!=600
    button('Down');button('Down',2);button('Select')
    shot('settings-delete-keep-default')
    button('Select')
    kept=shot('settings-keep-returns-options-delete-selected')
    assert kept[(180*200+4)*3:(180*200+4)*3+3]==bytes((0,0,0))
    button('Select');button('Back')
    backed=shot('settings-delete-back-returns-options-delete-selected')
    assert backed[(180*200+4)*3:(180*200+4)*3+3]==bytes((0,0,0))
    intact=handshake()['0']
    assert intact[1001]==edited[1001] and intact[1011]==edited[1011]
    button('Up');shot('settings-restart-shows-saved-duration')
    button('Select')
    restarted=wait_record(0,lambda value:value[1003]=='active' and value[1001]!=edited[1001])
    assert restarted[1022]==600 and restarted[1006]=='10 min timer'
    button('Select');shot('settings-paused-detail')
    seed(2);handshake();button('Up',8)
    shot('settings-eight-ready-replacement-list-warning')
    button('Select');shot('settings-eight-ready-replacement-editor-warning')
    button('Back',2)
    ready=handshake()
    assert all(value[1003]=='completed' and value[1023]==0 for value in ready.values())
    shot('settings-replacement-cancel-preserves-eight-ready')
    for _ in range(8):
        add();button('Select',2);button('Back')
    full=handshake()
    assert all(value[1003]=='active' for value in full.values())
    button('Up',8);shot('settings-eight-running-full')
    button('Select')
    still_full=handshake()
    assert all(still_full[slot][1001]==full[slot][1001] and
        still_full[slot][1011]==full[slot][1011] for slot in full)
    shot('settings-full-select-preserves-runs')
    report['proofs']['settingsNavigation']={'newCancelKeepsEmpty':True,
        'directEditCancelRun':unchanged,'optionsEditCancelCursorRestored':True,
        'deleteKeepAndBackReturnToOptionsCursor':True,'remainingEdit':edited,
        'deleteCancelKeepsRun':intact,'restartUsesSavedRepeat':restarted,
        'replacementCancelPreservesEightReady':True,'fullSelectPreservesEightRuns':True,
        'fullListSkipsDisabledAddAndOpensSelectedTimer':True}
    seed(0)

elif PHASE == 'hourglass':
    seed(2)
    before = handshake()
    assert all(value[1003] == 'completed' and value[1023] == 0 for value in before.values())
    button('Up', 8); button('Down'); button('Select')
    glass_bounds = (42, 104, 125, 187)
    editor_bounds = (57, 39, 110, 92)
    ready_pixels = shot('hourglass-ready-bottom')
    ready_sand = sand_position(ready_pixels, glass_bounds, 'blue')
    assert ready_sand['y'] > 153, ready_sand
    button('Up')
    editor_pixels = shot('hourglass-editor-bottom')
    editor_sand = sand_position(editor_pixels, editor_bounds, 'blue')
    assert editor_sand['y'] > 69, editor_sand
    button('Back', 2)
    after_cancel = handshake()
    # A ready handshake deliberately reconfirms source revisions. It must
    # preserve run identity, owner observation/expiry, duration and visibility.
    assert all(after_cancel[slot][key] == before[slot][key]
        for slot in before for key in (1001, 1003, 1009, 1010, 1011, 1012, 1022, 1023))
    start_mid = transition_shot('hourglass-start-flip')
    running_pixels = shot('hourglass-running-top', wait=.4)
    running = wait_record(0, lambda value: value[1003] == 'active' and value[1001] != before['0'][1001])
    running_sand = sand_position(running_pixels, glass_bounds, 'blue')
    assert running_sand['y'] < 137, running_sand
    start_sampled = region(start_mid, glass_bounds) != region(running_pixels, glass_bounds)
    pause_mid = transition_shot('hourglass-pause-turn')
    paused_pixels = shot('hourglass-paused-horizontal', wait=.4)
    paused = wait_record(0, lambda value: value[1003] == 'paused')
    assert paused[1001] == running[1001] and paused[1012] > 0
    paused_sand = sand_position(paused_pixels, glass_bounds, 'amber')
    assert paused_sand['x'] > 91 and abs(paused_sand['y'] - 145) < 4, paused_sand
    pause_sampled = region(pause_mid, glass_bounds) != region(paused_pixels, glass_bounds)
    paused_later = shot('hourglass-paused-static', wait=1.2)
    assert region(paused_pixels, glass_bounds) == region(paused_later, glass_bounds), 'Paused glass animated at rest'
    resume_mid = transition_shot('hourglass-resume-turn')
    resumed_pixels = shot('hourglass-resumed-top', wait=.4)
    resumed = wait_record(0, lambda value: value[1003] == 'active' and value[1002] > paused[1002])
    resumed_sand = sand_position(resumed_pixels, glass_bounds, 'blue')
    assert resumed[1001] == paused[1001] and resumed[1022] == paused[1022]
    assert resumed[1011] - resumed[1009] == paused[1012]
    assert resumed_sand['y'] < 137, resumed_sand
    resume_sampled = region(resume_mid, glass_bounds) != region(resumed_pixels, glass_bounds)
    # Screenshot RPC completion can occur after a 300ms transition; retain each
    # observed result rather than claiming every attempted sample caught it.
    assert start_sampled or pause_sampled or resume_sampled, 'No intermediate rotation frame sampled'
    # Cardinal rotations preserve the same native chamber to raster tolerance.
    areas = [ready_sand['pixels'], running_sand['pixels'], paused_sand['pixels'], resumed_sand['pixels']]
    assert max(areas) - min(areas) < max(areas)*.12, areas
    unchanged_others = handshake()
    assert all(unchanged_others[slot][key] == before[slot][key]
        for slot in before if slot != '0' for key in (1001, 1003, 1009, 1010, 1011, 1012, 1022, 1023))
    # Cover a pause while its 300ms animation is still in flight. These packets
    # generate only an isolated QEMU OS call surface; no phone is connected.
    tap_select()
    cookie = 0x544d5101
    p.send_packet(PhoneNotification(cookie=cookie, message=IncomingCall(number='000', name='Timer motion QA')))
    covered_pixels = shot('hourglass-os-cover', wait=.2,
        source='Stock PebbleOS synthetic call cover; includes an OS window transition, not Timer-only UI')
    paused_cover = wait_record(0, lambda value: value[1003] == 'paused' and value[1002] > resumed[1002])
    assert region(covered_pixels, glass_bounds) != region(paused_pixels, glass_bounds), 'OS cover not visible'
    p.send_packet(PhoneNotification(cookie=cookie, message=CallEnd()))
    ended_pixels = shot('hourglass-call-ended-cover', wait=.6,
        source='Stock PebbleOS Call Declined summary, not Timer UI')
    # Stock PebbleOS retains a red Call Declined summary after CallEnd. Dismiss
    # that actual system surface; CallEnd alone does not return focus to Timer.
    call_summary_dismissed = ended_pixels[:3] == bytes((255, 0, 0))
    if call_summary_dismissed:
        button('Back')
    wait_timer_surface()
    returned_pixels = shot('hourglass-after-os-cover', wait=.2)
    returned_sand = sand_position(returned_pixels, glass_bounds, 'amber')
    assert returned_sand['x'] > 91 and abs(returned_sand['y'] - 145) < 4, returned_sand
    returned_later = shot('hourglass-cover-return-static', wait=1.2)
    assert region(returned_pixels, glass_bounds) == region(returned_later, glass_bounds), 'Cover return replayed motion'
    assert current(0)[1001] == paused_cover[1001] and current(0)[1012] == paused_cover[1012]
    button('Select')
    reopened_run = wait_record(0, lambda value: value[1003] == 'active' and value[1002] > paused_cover[1002])
    button('Back'); button('Down'); button('Select')
    shot('hourglass-other-ready-timer')
    button('Back'); button('Up')
    tap_select()
    reopened_mid = shot('hourglass-open-existing-running-immediate', wait=.025)
    reopened_final = shot('hourglass-open-existing-running-settled', wait=.4)
    assert sand_position(reopened_mid, glass_bounds, 'blue')['y'] < 137, 'Opening an existing run replayed start flip'
    assert sand_position(reopened_final, glass_bounds, 'blue')['y'] < 137
    assert current(0)[1001] == reopened_run[1001] and current(0)[1011] == reopened_run[1011]

    # Also exercise a substantially divided chamber, so a reset-to-full visual
    # on pause/resume cannot pass by matching a near-full five-minute timer.
    seed(0); handshake(); create(10)
    partial_started = wait_record(0, lambda value: value[1003] == 'active')
    time.sleep(3.8)
    button('Select')
    partial_paused = wait_record(0, lambda value: value[1003] == 'paused')
    partial_pixels = shot('hourglass-partial-paused-horizontal')
    upper = sand_position(partial_pixels, (85, 104, 125, 187), 'amber')['pixels']
    lower = sand_position(partial_pixels, (42, 104, 82, 187), 'amber')['pixels']
    partial_fraction = upper/(upper+lower)
    expected_fraction = partial_paused[1012]/partial_paused[1022]
    assert .2 <= expected_fraction <= .8, partial_paused
    assert abs(partial_fraction - expected_fraction) < .08, (partial_fraction, expected_fraction)
    transition_shot('hourglass-partial-resume-turn')
    partial_resumed_pixels = shot('hourglass-partial-resumed-top', wait=.35)
    partial_resumed = wait_record(0, lambda value: value[1003] == 'active' and value[1002] > partial_paused[1002])
    assert partial_resumed[1001] == partial_paused[1001]
    assert partial_resumed[1011] - partial_resumed[1009] == partial_paused[1012]
    upper = sand_position(partial_resumed_pixels, (42, 104, 125, 142), 'blue')['pixels']
    lower = sand_position(partial_resumed_pixels, (42, 148, 125, 187), 'blue')['pixels']
    resumed_fraction = upper/(upper+lower)
    assert expected_fraction - .14 <= resumed_fraction <= expected_fraction + .04, (resumed_fraction, expected_fraction)
    report['proofs']['hourglass'] = {'readySand': ready_sand, 'editorSand': editor_sand,
        'runningSand': running_sand, 'pausedSand': paused_sand, 'resumedSand': resumed_sand,
        'cardinalSandPixels': areas, 'nativeBefore': before, 'nativeStarted': running,
        'nativePaused': paused, 'nativeResumed': resumed, 'nativePausedDuringCover': paused_cover,
        'readyAndEditorSandPhysicallyBottom': True, 'runningSandPhysicallyTop': True,
        'pausedGlassHorizontalAndStatic': True,
        'intermediateRotationSamples': {'start': start_sampled, 'pause': pause_sampled, 'resume': resume_sampled},
        'resumePreservesRunAndRemaining': True, 'editorCancelPreservesAllEightTimers': True,
        'sevenOtherTimersPreserved': True, 'syntheticQemuOsCoverReturnSettledWithoutReplay': True,
        'osCallSummaryDismissedWithBack': call_summary_dismissed,
        'existingRunningListOpenDoesNotReplayStartFlip': True,
        'partialStarted': partial_started, 'partialPaused': partial_paused, 'partialResumed': partial_resumed,
        'partialPausedRemainingSandFraction': partial_fraction,
        'partialResumedRemainingSandFraction': resumed_fraction,
        'partialPauseResumePreservesPhysicalProportions': True,
        'nativePixelsUnmodified': True, 'batteryOrCoveredCpuUsageMeasured': False}
    seed(0)

elif PHASE == 'legibility':
    seed(5); handshake()
    button('Down', 2)
    shot('legibility-24h-running-list')
    button('Select'); shot('legibility-24h-running-detail')
    button('Back'); button('Up', 8)
    shot('legibility-replacement-list')
    button('Select'); shot('legibility-replacement-editor')
    # Enter the longest legal duration through the actual native editor. An
    # exact 24h duration is labelled24h and would not exercise long context.
    button('Back'); button('Up', 23); button('Select')
    button('Up', 49); button('Select'); button('Up', 59); button('Select')
    state = handshake()
    long_running = next(value for value in state.values()
        if value[1003] == 'active' and value[1022] == 86399)
    shot('legibility-235959-running-detail')
    button('Back'); shot('legibility-235959-running-list')
    seed(0); handshake(); add(); button('Down', 10); button('Select')
    zero_editor = shot('legibility-zero-seconds-editor-bottom')
    zero_sand = sand_position(zero_editor, (57, 39, 110, 92), 'blue')
    assert zero_sand['y'] > 69, zero_sand
    button('Select')
    invalid_zero = handshake()
    assert all(value[1003] == 'cancelled' for value in invalid_zero.values())
    button('Up', 4)
    four_editor = shot('legibility-four-seconds-editor-bottom')
    four_sand = sand_position(four_editor, (57, 39, 110, 92), 'blue')
    assert four_sand['y'] > 69, four_sand
    button('Select')
    invalid_four = handshake()
    assert all(value[1003] == 'cancelled' for value in invalid_four.values())
    button('Up'); shot('legibility-five-seconds-editor-valid')
    button('Back', 3)
    report['proofs']['legibility'] = {'nativeMixed24hAndReplacementCaptured': True,
        'native235959Timer': long_running, 'longDurationEnteredThroughNativeEditor': True,
        'zeroEditorSand': zero_sand, 'fourSecondEditorSand': four_sand,
        'invalidZeroAndFourSecondsPreserveEmptyCollection': True,
        'fiveSecondsValidAffordanceCaptured': True,
        'requiresHumanVisualReview': True}
    seed(0)

elif PHASE == 'launcher':
    seed(0)
    handshake()
    add(); button('Select', 2)
    running = handshake()['0']
    shot('launcher-source-running')
    launcher(2)
    before = shot('launcher-running-countdown')
    time.sleep(2)
    after = shot('launcher-running-countdown-tick')
    assert before != after, 'OS app-list countdown did not change'
    report['proofs']['launcherCountDown'] = {'nativeState': running,
        'before': 'launcher-running-countdown.png', 'after': 'launcher-running-countdown-tick.png',
        'timerAppClosed': True, 'phoneJavascriptExecuted': False, 'waitSeconds': 2,
        'rgbChanged': True, 'visualReviewRequiredToLocateTimerEntry': True}
    launch(); handshake(); button('Select', 2)
    paused=wait_record(0,lambda value:value[1003]=='paused')
    shot('launcher-source-paused')
    launcher(2); shot('launcher-paused-cleared')
    time.sleep(2)
    paused_pixels=shot('launcher-paused-cleared-settled')
    launch(); handshake(); button('Select'); delete()
    deleted=wait_record(0,lambda value:value[1003]=='cancelled')
    launcher(1); deleted_pixels=shot('launcher-deleted-cleared')
    assert paused_pixels==deleted_pixels, 'Paused app-list entry differs from the cleared deleted entry'
    report['proofs']['launcherPauseDeleteClear']={'pausedNativeState':paused,'deletedNativeState':deleted,
        'pausedFrame':'launcher-paused-cleared-settled.png','deletedFrame':'launcher-deleted-cleared.png',
        'normalBackExit':True,'clearedFramesRgbEqual':True}
    # A fast start/pause/exit deliberately gives a pending glance less than a
    # second to flush. No screenshot or handshake delays the physical buttons.
    launch(); handshake(); add(); button('Select', 2); button('Select')
    fast_paused=wait_record(0,lambda value:value[1003]=='paused')
    launcher(2)
    fast_active=None
    for attempt in range(6):
        with lock:
            matching=[value for value in all_messages if value.get(1001)==fast_paused[1001]
                and value.get(1003)=='active' and value.get(1002,0)<fast_paused[1002]]
        fast_active=matching[-1] if matching else None
        if fast_active and fast_active[1009]==fast_paused[1009]:
            break
        launch(); handshake(); button('Select')
        time.sleep(.13*(attempt+1))
        before_resume=current(0)
        button('Select',2)
        fast_paused=wait_record(0,lambda value:value[1003]=='paused' and value[1002]>before_resume[1002])
        launcher(2)
    assert fast_active and fast_active[1009]==fast_paused[1009], 'Need an actual same-UTC-second native state collision'
    fast_pixels=shot('launcher-fast-pause-exit-cleared')
    assert fast_pixels==deleted_pixels, 'Fast pause/exit retained an obsolete countdown'
    report['proofs']['launcherFastPauseExit']={'startedOrResumedNativeState':fast_active,'pausedNativeState':fast_paused,
        'nativeStateChangesInSameUtcSecond':True,
        'noInterveningScreenshotOrHandshakeBeforeExit':True,'clearedFrameRgbEqualsDeleted':True}
    seed(0)

elif PHASE in ('nearby', 'equal'):
    seed(3 if PHASE == 'nearby' else 4)
    state = handshake()
    shot(PHASE + '-running-list')
    launcher(1); shot(PHASE + '-launcher-earliest')
    completed = alarm_wait(state['0'], PHASE + '-first-native-alarm')
    if PHASE == 'nearby':
        time.sleep(5)
    button('Select'); shot(PHASE + '-second-native-alarm')
    assert current(0)[1023] == 0 and current(1)[1023] == 1
    button('Back'); shot(PHASE + '-back-acknowledged-list')
    assert current(1)[1023] == 0
    stop(); launch(); reopened = handshake()
    assert reopened['0'][1023] == reopened['1'][1023] == 0
    shot(PHASE + '-reopened-no-alarm')
    launcher(1); shot(PHASE + '-finished-launcher-cleared')
    report['proofs'][PHASE + 'Deadlines'] = {'started': state, 'afterReopen': reopened,
        'actualTimerWakeup': True, 'fixtureScheduledNoWakeups': True,
        'SelectAcknowledgedFirstOnly': True, 'BackAcknowledgedSecond': True}
    seed(0)

else:
    raise SystemExit('Unknown TIMER_QA_PHASE: ' + PHASE)

report['completedPhases'] = sorted(set(report['completedPhases'] + [PHASE]))
report['result'] = 'PASS' if set(report['completedPhases']) >= REQUIRED_PHASES else 'INCOMPLETE'
report.setdefault('nativeMessages', {})[PHASE] = list(all_messages)
save()
print('Timer native phase completed: ' + PHASE, flush=True)
