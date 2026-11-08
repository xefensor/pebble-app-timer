#!/usr/bin/env python3
"""Exact packaged phone scripts -> native watch -> persisted application receipt.
Phone plugin/PKJS API is simulated over pipes; native app runs unmodified in QEMU.
"""
from pathlib import Path
import hashlib,json,os,queue,subprocess,sys,threading,time,uuid,zipfile
from repo_workflow import candidate,check_build
from pebble_tool.commands.install import ToolAppInstaller
from libpebble2.protocol.apps import AppRunState,AppRunStateStart
from libpebble2.services.appmessage import AppMessageService,ByteArray,Uint32,CString
ROOT=Path(__file__).resolve().parents[1];PBW=candidate(ROOT,'Timer');check_build(ROOT,PBW)
OUT=ROOT/'build/qa/preferences-bridge'/str(time.time_ns());OUT.mkdir(parents=True)
os.environ['PEBBLE_QA_STATE']=str(OUT/'emulator');sys.path.insert(0,str(ROOT.parent/'recovery-validation'))
from pipe_emulator import connect
with zipfile.ZipFile(PBW) as source,zipfile.ZipFile(OUT/'native.pbw','w') as native:
    info=json.loads(source.read('appinfo.json'))
    for name in ('plugin.js','pebble-js-app.js'):(OUT/name).write_bytes(source.read(name))
    for name in source.namelist():
        if name=='appinfo.json' or name.startswith('emery/'):native.writestr(name,source.read(name))
keys=info['messageKeys'];inverse={value:key for key,value in keys.items()};target=uuid.UUID(info['uuid'])
p=connect();service=AppMessageService(p);lines=queue.Queue();lock=threading.Lock();transactions={};replies=[];commands=0;responses={}
phone=subprocess.Popen(['node',str(ROOT/'scripts/emulator-preferences-phone.cjs'),str(OUT)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
def write(value):
    with lock:phone.stdin.write(json.dumps(value)+'\n');phone.stdin.flush()
def read():
    for line in phone.stdout:lines.put(json.loads(line))
threading.Thread(target=read,daemon=True).start()
def receive(transaction,app,data):
    if app!=target:return
    payload={inverse[key]:list(value) if isinstance(value,(bytes,bytearray)) else value for key,value in data.items() if key in inverse}
    if 1027 in data:replies.append(list(data[1027]))
    write({'type':'native','payload':payload})
def transport(transaction,app,ok):
    identifier=transactions.pop(transaction,None)
    if identifier is not None:write({'type':'transport','id':identifier,'ok':ok})
service.register_handler('appmessage',receive)
service.register_handler('ack',lambda tid,app:transport(tid,app,True))
service.register_handler('nack',lambda tid,app:transport(tid,app,False))
def pump():
    try:line=lines.get(timeout=.1)
    except queue.Empty:return
    if line['type']=='response':responses[line['id']]=line['result'];return
    if line['type']=='send':
        values={keys[key]:ByteArray(bytes(value)) if isinstance(value,list) else CString(value) if isinstance(value,str) else Uint32(value) for key,value in line['payload'].items()}
        transactions[service.send_message(target,values)]=line['id']
def request(message=None,start=False):
    global commands
    commands+=1;write({'type':'start' if start else 'request','id':commands,'message':message})
    until=time.monotonic()+8
    while commands not in responses and time.monotonic()<until:pump()
    assert commands in responses,'Packaged phone response timed out'
    return responses.pop(commands)
try:
    chosen={'version':2,'textSize':'large','appearance':'follow','lastExplicit':'dark'}
    saved=request({'type':'saveUiPreferences','settings':chosen})
    assert saved['saved'] and saved['delivery']['pending'] and not saved['delivery']['applied']
    ToolAppInstaller(p,str(OUT/'native.pbw'),quiet=True).install()
    p.send_packet(AppRunState(data=AppRunStateStart(uuid=target)));time.sleep(.3)
    request(start=True)
    until=time.monotonic()+10;applied=None
    while time.monotonic()<until:
        applied=request({'type':'getUiPreferences'})
        if applied['delivery']['applied']:break
        pump()
    assert applied['delivery']['applied'] and applied['settings']==chosen,applied
    assert any(frame[2:4]==[0,0] and frame[8:20]==[3,0,0,0,0,0,0,0,2,0,0,0] for frame in replies),replies
    check_build(ROOT,PBW)
    report={'result':'PASS','pbwSha256':hashlib.sha256(PBW.read_bytes()).hexdigest(),'pendingBeforeWatch':saved,'appliedAfterNativeReceipt':applied,'nativeReplies':replies,'scope':'Exact optimized PBW plugin and PKJS, actual Emery AppMessage transport ACKs and native save/readback/application receipts. Phone SDK and storage simulated;not a physical phone/watch test.'}
    (OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','path':str(OUT/'report.json')}),flush=True)
finally:
    phone.terminate();phone.wait(timeout=3)
