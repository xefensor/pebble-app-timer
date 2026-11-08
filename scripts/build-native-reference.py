#!/usr/bin/env python3
"""Build disposable SDK-native menu/center-focus comparisons, separate UUID."""
from pathlib import Path
import json, os, shutil, subprocess
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/qa/menu-reference'
project=out/'project'
project.mkdir(parents=True,exist_ok=True)
for folder in ('src','resources','vendor','node_modules'):
    shutil.copytree(ROOT/folder,project/folder,dirs_exist_ok=True)
shutil.copyfile(ROOT/'tests/native-menu-reference/main.c',project/'src/c/main.c')
shutil.copyfile(ROOT/'package-lock.json',project/'package-lock.json')
package=json.loads((ROOT/'package.json').read_text())
package['pebble']['uuid']='130e1cec-c819-46c5-bf91-468f63c44876'
package['pebble']['displayName']='Native menu reference'
package['pebble'].pop('plugin',None)
(project/'package.json').write_text(json.dumps(package))
wscript=(ROOT/'wscript').read_text().replace("ctx.env = ctx.all_envs[platform]", "ctx.env = ctx.all_envs[platform]\n        ctx.env.append_unique('DEFINES', 'REFERENCE_MODE=' + environ['REFERENCE_MODE'])")
(project/'wscript').write_text(wscript)
for mode,name in ((0,'stock-cells'),(1,'xoce-center-focused')):
    env={**os.environ,'REFERENCE_MODE':str(mode)}
    subprocess.run(['pebble','clean'],cwd=project,env=env,check=True)
    subprocess.run(['pebble','build'],cwd=project,env=env,check=True)
    shutil.copyfile(project/'build/project.pbw',out/(name+'.pbw'))
# Separate instrumentation/locale fixtures. Their native bytes deliberately
# differ from the final candidate; reports must not conflate that evidence.
(project/'src/c/main.c').write_bytes((ROOT/'src/c/main.c').read_bytes())
(project/'package.json').write_bytes((ROOT/'package.json').read_bytes())
for name,locale in (('heap-diagnostic',None),('locale-cs','cs_CZ'),('locale-de','de_DE')):
    localization=(ROOT/'src/c/localization.c').read_text()
    if locale:localization=localization.replace('i18n_get_system_locale()',json.dumps(locale))
    (project/'src/c/localization.c').write_text(localization)
    env={**os.environ,'REFERENCE_MODE':'2','ACTIVITY_TIMER_DIAGNOSTICS':'1' if not locale else '0'}
    subprocess.run(['pebble','clean'],cwd=project,env=env,check=True)
    subprocess.run(['pebble','build'],cwd=project,env=env,check=True)
    shutil.copyfile(project/'build/project.pbw',out/(name+'.pbw'))
print(out)
