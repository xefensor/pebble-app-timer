#!/usr/bin/env python3
"""Verify current installable identity, exact fonts and actual packaged runtime."""
from pathlib import Path
import argparse,json,subprocess,tempfile,zipfile
from repo_workflow import candidate,check_build
import importlib.util
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pbw',type=Path,default=candidate(root,'Timer'))
args=parser.parse_args();check_build(root,args.pbw)
package=json.loads((root/'package.json').read_text())
with tempfile.TemporaryDirectory(prefix='timer-artifact-') as directory:
    with zipfile.ZipFile(args.pbw) as archive:
        assert archive.testzip() is None
        info=json.loads(archive.read('appinfo.json'))
        assert info['versionLabel']==package['version']
        assert info['uuid']==package['pebble']['uuid']
        assert info['usesPermissions']==package['pebble']['usesPermissions']
        assert archive.read('config.html')==(root/'config.html').read_bytes()
        archive.extractall(directory)
    subprocess.run(['node',str(root/'scripts/verify-timer-bundle.js'),directory],cwd=root,check=True)
spec=importlib.util.spec_from_file_location('fonts',root/'scripts/check-timer-fonts.py')
fonts=importlib.util.module_from_spec(spec);spec.loader.exec_module(fonts)
fonts.audit(args.pbw)
print('Timer candidate: identity, current inputs, runtime, settings and exact fonts PASS')
