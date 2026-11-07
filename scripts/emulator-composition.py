#!/usr/bin/env python3
"""Early composition review of the actual packaged native app; no phone/hardware."""
from pathlib import Path
import hashlib,json,os,sys,time,uuid,zipfile
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
from libpebble2.protocol.apps import AppRunState,AppRunStateStart,AppRunStateStop
from libpebble2.communication.transports.qemu.protocol import QemuButton
from libpebble2.services.screenshot import Screenshot
from PIL import Image
root=Path(__file__).resolve().parents[1];version=json.loads((root/'package.json').read_text())['version']
pbw=root/'dist'/('Timer-'+version+'.pbw');native=root/'build/composition-native-only.pbw'
sha=lambda b:hashlib.sha256(b).hexdigest()
candidate_sha=sha(pbw.read_bytes());out=root/'docs/qa'/version/'composition-review'/candidate_sha[:12]
assert not out.exists(),'Use the completed immutable report for this exact candidate, or a new candidate build.'
out.mkdir(parents=True)
with zipfile.ZipFile(pbw) as source,zipfile.ZipFile(native,'w') as dest:
    info=json.loads(source.read('appinfo.json'));payload={n:sha(source.read(n)) for n in source.namelist() if n.startswith('emery/')}
    for entry in source.infolist():
        if entry.filename=='appinfo.json' or entry.filename.startswith('emery/'):dest.writestr(entry,source.read(entry.filename))
os.environ['PEBBLE_QA_STATE']=str(root.parent/'.recovery-tools'/('timer-composition-'+str(time.time_ns())))
sys.path.insert(0,str(root.parent/'recovery-validation'))
from pipe_emulator import connect
p=connect();target=uuid.UUID(info['uuid']);captures=[]
def launch():p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.4)
def stop():p.send_packet(AppRunState(data=AppRunStateStop(uuid=target)));time.sleep(.2)
def install(path):ToolAppInstaller(p,str(path),quiet=True).install();launch()
def tap(button,count=1):
    for _ in range(count):
        send_data_to_qemu(p.transport,QemuButton(state=int(getattr(QemuButton.Button,button))));time.sleep(.04)
        send_data_to_qemu(p.transport,QemuButton(state=0));time.sleep(.25)
def shot(name):
    time.sleep(.3);data=b''.join(Screenshot(p).grab_image());path=out/(name+'.png')
    frame=Image.frombytes('RGB',(200,228),data);frame.save(path)
    clock_pixels=sum(max(frame.getpixel((x,y)))<96 for y in range(1,30) for x in range(94,158))
    assert clock_pixels>=160,(name,'header clock missing',clock_pixels)
    captures.append({'file':path.name,'sha256':sha(path.read_bytes()),'headerClockInkPixels':clock_pixels})
def seed(mode):install(root/'build'/('fixture-mode'+str(mode)+'.pbw'));stop();install(native)
seed(5);shot('list-light');tap('Select');shot('running-light');tap('Up');shot('editor-light');tap('Back',2);tap('Down');shot('options-light')
tap('Back');tap('Back');tap('Down',2);tap('Select');shot('long-running-light')
stop();seed(6);shot('critical-list-light');tap('Select');tap('Down');shot('critical-options-light');tap('Down',2);tap('Select');shot('critical-delete-light')
(out/'report.json').write_text(json.dumps({'version':version,'pbwSha256':candidate_sha,'nativePayloadSha256':payload,'captures':captures,
 'scope':'Actual distribution-native Emery QEMU representative compositions; isolated stock-data fixtures. Early visual review, not full QA or physical-watch proof.'},indent=2)+'\n')
print(out)
