#!/usr/bin/env python3
"""One-timer placement and editor motion in isolated Emery QEMU.

Build the candidate and mode-0 fixture first. Screenshot-service acquisition is
host-timed evidence of actual rendered frames, not physical-watch latency.
"""
from pathlib import Path
from repo_workflow import candidate as candidate_pbw, qa_output
import hashlib, json, os, sys, time, uuid, zipfile
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState, AppRunStateStart, AppRunStateStop
from libpebble2.communication.transports.qemu.protocol import QemuButton
from libpebble2.services.screenshot import Screenshot
from libpebble2.services.appmessage import AppMessageService, Uint8, Uint32, CString
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = qa_output(ROOT, 'native-editor-motion')
OUT.mkdir(parents=True, exist_ok=True)
os.environ['PEBBLE_QA_STATE'] = str(Path('/tmp') / ('timer-editor-' + str(time.time_ns())))
sys.path.insert(0, str(ROOT.parent / 'recovery-validation'))
from pipe_emulator import connect

p = connect()
target = uuid.UUID('50e9ef12-a5ab-4369-a3b5-3447bfba5151')
pbw = candidate_pbw(ROOT, 'Timer')
native = ROOT / 'build/editor-motion-native-only.pbw'
sha = lambda data: hashlib.sha256(data).hexdigest()
candidate_sha = sha(pbw.read_bytes())
with zipfile.ZipFile(pbw) as source, zipfile.ZipFile(native, 'w') as output:
    payload = {name: sha(source.read(name)) for name in source.namelist() if name.startswith('emery/')}
    for entry in source.infolist():
        if entry.filename == 'appinfo.json' or entry.filename.startswith('emery/'):
            output.writestr(entry, source.read(entry.filename))

service = AppMessageService(p)
records = {}
captures = []
def receive(transaction, app, data):
    if app != target or 1000 not in data:
        return
    records[int(data[1021])] = dict(data)
    service.send_message(target, {1014: CString(data[1001]), 1015: Uint32(data[1002]), 1020: Uint32(data[1020])})
service.register_handler('appmessage', receive)
def launch():
    p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)))
    time.sleep(.5)
def stop():
    p.send_packet(AppRunState(data=AppRunStateStop(uuid=target)))
    time.sleep(.3)
def install(path):
    ToolAppInstaller(p, str(path), quiet=True).install()
    launch()
def raw(name):
    send_data_to_qemu(p.transport, QemuButton(state=int(getattr(QemuButton.Button, name))))
def release():
    send_data_to_qemu(p.transport, QemuButton(state=0))
def tap(name, wait=.4):
    raw(name)
    time.sleep(.035)
    release()
    time.sleep(wait)
def shot(name, start=None):
    before = time.monotonic()
    data = b''.join(Screenshot(p).grab_image())
    after = time.monotonic()
    assert len(data) == 200 * 228 * 3
    image = Image.frombytes('RGB', (200, 228), data)
    path = OUT / (name + '.png')
    image.save(path)
    item = {'file': path.name, 'sha256': sha(path.read_bytes())}
    if start is not None:
        item.update(acquisitionStartMs=round((before-start)*1000, 1), acquisitionEndMs=round((after-start)*1000, 1))
    captures.append(item)
    return image
def theme(dark):
    service.send_message(target, {1024: Uint8(dark), 1025: Uint8(1)})
    time.sleep(.35)
def snapshot():
    records.clear()
    service.send_message(target, {1017: Uint8(1), 1019: Uint32(0)})
    for _ in range(100):
        if len(records) == 8:
            break
        time.sleep(.05)
    assert len(records) == 8
    return {str(slot): {str(key): value.get(key) for key in (1001, 1003, 1011, 1012, 1022)} for slot, value in records.items()}
def one_timer(name, dark):
    image = shot(name)
    bg = (0, 0, 0) if dark else (255, 255, 255)
    ink = (255, 255, 255) if dark else (0, 0, 0)
    selected = [y for y in range(32, 228) if image.getpixel((1, y)) == ink]
    assert selected == list(range(50, 130)), (name, selected)
    assert all(image.getpixel((x, y)) == ink for x in (166, 199) for y in range(228)), name
    assert all(image.getpixel((1, y)) == bg for y in range(130, 228)), name
    return {'top': 50, 'height': 80, 'railWidth': 34, 'railHeight': 228}
def motion(name, button):
    frames = [shot(name + '-before')]
    start = time.monotonic()
    tap(button, 0)
    for index in range(10):
        frames.append(shot(name + '-%02d' % index, start))
        time.sleep(.012)
    time.sleep(.35)
    frames.append(shot(name + '-settled', start))
    frames[0].save(OUT / (name + '.gif'), save_all=True, append_images=frames[1:], duration=90, loop=0)
    # Geometry at an empty scanline above the digit ink: the field may grow
    # by its 3 px gap, never span two fields as the old implementation did.
    widths = []
    for image in frames:
        points = [x for x in range(166) if image.getpixel((x, 100)) == (0, 0, 0)]
        if points:
            width = max(points) - min(points) + 1
            assert 50 <= width <= 53, (name, width)
            widths.append(width)
    crops = {sha(image.crop((0, 86, 166, 154)).tobytes()) for image in frames}
    assert len(crops) >= 2, (name, 'No actual changed editor frame captured')
    return {'distinctEditorFrames': len(crops), 'observedFieldWidths': sorted(set(widths)),
            'elapsedMs': round((time.monotonic()-start)*1000, 1),
            'gifNote': 'Fixed90ms playback is illustrative; PNG acquisition windows are the timing evidence.'}

install(ROOT / 'build/fixture-mode0.pbw')
stop()
install(native)
tap('Select')                         # New timer, minutes selected.
tap('Select')                         # Seconds selected.
tap('Select')                         # Save/start the real native timer.
tap('Back')                           # Ordinary single-timer list.
checks = {'light': one_timer('one-timer-light', False)}
initial = snapshot()
theme(1)
checks['dark'] = one_timer('one-timer-dark', True)
assert snapshot() == initial, 'Appearance changed timer state/deadline/duration'
stop()
launch()
tap('Down')                           # Keep the existing timer selected.
checks['reopened'] = one_timer('one-timer-reopened-dark', True)
assert snapshot() == initial, 'Closing/reopening changed owner state'
theme(0)
tap('Select')                         # Detail.
tap('Select')                         # Pause: editing now has a frozen baseline.
paused = snapshot()
remaining = next(value['1012'] for value in paused.values() if value['1003'] == 'paused')
tap('Up')                             # Remaining editor, minutes selected.
motions = {'field-forward': motion('field-forward', 'Select'),
           'field-back': motion('field-back', 'Back'),
           'value-up': motion('value-up', 'Up'),
           'value-down': motion('value-down', 'Down')}
tap('Up', .075)                       # Activate before the321ms feedback ends.
tap('Select', .075)                   # Brief interval between distinct taps.
tap('Select')                         # Save from seconds before feedback can finish.
shot('immediate-edit-saved')
saved = snapshot()
active = [value for value in saved.values() if value['1003'] == 'paused']
assert len(active) == 1 and active[0]['1012'] == remaining + 60, (remaining, saved)
stop()
launch()
assert snapshot() == saved, 'Edited timer changed after close/reopen'
assert sha(pbw.read_bytes()) == candidate_sha, 'Candidate changed during QA'
report = {'result': 'PASS', 'pbwSha256': candidate_sha, 'nativePayloadSha256': payload,
          'scope': 'Actual candidate native payload in fresh isolated Emery QEMU; simulated AppMessages and buttons. No phone runtime or physical device. Screenshot acquisition is host timed.',
          'oneTimer': checks, 'motion': motions, 'immediateEditSavedAndReopened': True, 'captures': captures}
(OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'result': 'PASS', 'report': str(OUT / 'report.json'), 'pbwSha256': candidate_sha}))
