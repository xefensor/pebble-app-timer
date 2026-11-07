#!/usr/bin/env python3
"""Capture historical option motion without changing the historical artifact."""
from pathlib import Path
import hashlib,json,os,sys,time,uuid,zipfile
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState,AppRunStateStart,AppRunStateStop
from libpebble2.communication.transports.qemu.protocol import QemuButton
from libpebble2.services.screenshot import Screenshot
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
VERSION=json.loads((ROOT/'package.json').read_text())['version']
OUT=ROOT/'docs/qa'/VERSION/'previous-option-motion';OUT.mkdir(parents=True,exist_ok=True)
pbw=ROOT/'releases/Timer-0.14.0.pbw';native=ROOT/'build/previous-option-native-only.pbw'
sha=lambda value:hashlib.sha256(value).hexdigest()
with zipfile.ZipFile(pbw) as source,zipfile.ZipFile(native,'w') as dest:
    info=json.loads(source.read('appinfo.json'));assert info['versionLabel']=='0.14.0'
    payload={n:sha(source.read(n)) for n in source.namelist() if n.startswith('emery/')}
    for entry in source.infolist():
        if entry.filename=='appinfo.json' or entry.filename.startswith('emery/'):
            dest.writestr(entry,source.read(entry.filename))
os.environ['PEBBLE_QA_STATE']=str(ROOT.parent/'.recovery-tools'/('timer-previous-option-'+str(time.time_ns())))
sys.path.insert(0,str(ROOT.parent/'recovery-validation'))
from pipe_emulator import connect
p=connect();target=uuid.UUID(info['uuid'])
def launch():p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.5)
def stop():p.send_packet(AppRunState(data=AppRunStateStop(uuid=target)));time.sleep(.3)
def install(path):ToolAppInstaller(p,str(path),quiet=True).install();launch()
def tap(button):
    send_data_to_qemu(p.transport,QemuButton(state=int(getattr(QemuButton.Button,button))));time.sleep(.035)
    send_data_to_qemu(p.transport,QemuButton(state=0))
def image():return Image.frombytes('RGB',(200,228),b''.join(Screenshot(p).grab_image()))
install(ROOT/'build/fixture-mode5.pbw');stop();install(native)
tap('Select');time.sleep(.4);tap('Down');time.sleep(.4)
frames=[image()];samples=[];started=time.monotonic();tap('Down')
for n,offset in enumerate((0,.025,.055,.085,.12,.16,.21,.27,.34,.42,.52)):
    time.sleep(max(0,started+offset-time.monotonic()))
    frame=image();name='previous-0.14.0-option-%02d.png'%n;frame.save(OUT/name)
    frames.append(frame);samples.append({'file':name,'elapsedMs':round((time.monotonic()-started)*1000),
      'sha256':sha((OUT/name).read_bytes())})
durations=[350]+[max(20,b['elapsedMs']-a['elapsedMs']) for a,b in zip(samples,samples[1:])]+[500]
frames[0].save(OUT/'previous-0.14.0-options-switch.gif',save_all=True,append_images=frames[1:],duration=durations,loop=0)
(OUT/'report.json').write_text(json.dumps({'version':'0.14.0','pbwSha256':sha(pbw.read_bytes()),
 'nativePayloadSha256':payload,'captures':samples,'historicalArtifactChanged':False,
 'scope':'Actual old0.14.0 distribution-native option frames in isolated QEMU. Stored with current review evidence for comparison; no old QA or artifact was changed.',
 'gifTiming':'Host screenshot acquisition intervals;350/500ms before/after dwell. Not physical-watch timing.'},indent=2)+'\n')
print('Historical0.14.0 option clip captured:',OUT)
