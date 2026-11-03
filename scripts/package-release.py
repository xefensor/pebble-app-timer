#!/usr/bin/env python3
"""Freeze the packaged Timer PBW and its standalone source inputs."""
from pathlib import Path
import hashlib
import json
import subprocess
import zipfile


def package(root):
    metadata = json.loads((root / 'package.json').read_text())
    name = 'Timer-' + metadata['version']
    pbw = (root / 'dist' / (name + '.pbw')).read_bytes()
    import io
    with zipfile.ZipFile(io.BytesIO(pbw)) as archive:
        if archive.testzip() is not None:
            raise ValueError('Corrupt Timer PBW')
        info = json.loads(archive.read('appinfo.json'))
        if (info.get('uuid') != metadata['pebble']['uuid'] or
                info.get('versionLabel') != metadata['version'] or
                info.get('manifestVersion') != 2 or 'plugin.js' not in archive.namelist()):
            raise ValueError('Build the packaged Timer PBW for this version first')
    releases = root / 'releases'
    releases.mkdir(exist_ok=True)
    target = releases / (name + '.pbw')
    target.write_bytes(pbw)
    debug = releases / (name + '-debug.zip')
    debug.write_bytes((root / 'dist' / debug.name).read_bytes())
    names = subprocess.check_output(['git', 'ls-files', '-co', '--exclude-standard', '-z'], cwd=root).decode().split('\0')
    source = releases / (name + '-source.zip')
    with zipfile.ZipFile(source, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(set(filter(None, names))):
            parts = Path(path).parts
            if (parts[0] in ('releases', 'dist', 'build', 'node_modules', '.pebble', '.git') or
                    '__pycache__' in parts or Path(path).suffix.lower() in ('.pbw', '.pyc')):
                continue
            entry = zipfile.ZipInfo(path, (1980, 1, 1, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, (root / path).read_bytes(), compresslevel=9)
    sums = []
    for artifact in (target, source, debug):
        checksum = hashlib.sha256(artifact.read_bytes()).hexdigest()
        sums.append(checksum + '  ' + artifact.name)
        print(artifact.name, artifact.stat().st_size, checksum)
    (releases / 'SHA256SUMS.txt').write_text('\n'.join(sums) + '\n')


if __name__ == '__main__':
    package(Path(__file__).resolve().parents[1])
