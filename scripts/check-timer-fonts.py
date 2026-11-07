#!/usr/bin/env python3
"""Audit exact native Timer font assets; native captures verify visual fitting."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def native_tables(header, binary):
    definitions = [('ClockPoint', 'clock_points', '<hh', 1049),
                   ('ClockContour', 'clock_contours', '<HHB1x', 19),
                   ('ClockGlyph', 'clock_glyphs', '<IHHHhhhh2x', 11)]
    summary = {}
    for kind, name, fmt, expected in definitions:
        match = re.search(r'static const ' + kind + r' ' + name + r'\[[^]]+\]\s*=\s*\{(.*?)\n\};', header, re.S)
        require(match, 'Missing clock asset table: ' + name)
        rows = re.findall(r'\{([^{}]+)\}', match[1])
        require(len(rows) == expected, 'Clock asset table size differs: ' + name)
        encoded = bytearray()
        for row in rows:
            tokens = re.findall(r"'[^']*'|0x[0-9a-fA-F]+|-?\d+|true|false", row)
            values = [ord(token[1:-1]) if token.startswith("'") else
                      int(token == 'true') if token in ('true', 'false') else int(token, 0)
                      for token in tokens]
            encoded.extend(struct.pack(fmt, *values))
        require(encoded in binary, 'Native binary lacks the exact reviewed clock table: ' + name)
        summary[name] = {'rows': expected, 'bytes': len(encoded), 'nativeIdentity': True}
    return summary


def audit(pbw, root=ROOT):
    package = json.loads((root / 'package.json').read_text())
    provenance = json.loads((root / 'resources/fonts/font-provenance.json').read_text())
    media = package['pebble']['resources']['media']
    require(media == [], 'Native Gothic utility roles must not package unused custom bitmap fonts')
    utility = (root / 'src/c/timer_fonts.c').read_text()
    require(all(key in utility for key in ('FONT_KEY_GOTHIC_18_BOLD', 'FONT_KEY_GOTHIC_24', 'FONT_KEY_GOTHIC_24_BOLD')) and
            'fonts_load_custom_font' not in utility and 'RESOURCE_ID_FONT_BODY' not in utility,
            'Utility font roles differ from the native Gothic candidate')
    for record in provenance['files']:
        data = (root / record['file']).read_bytes()
        require(len(data) == record['bytes'] and sha256(data) == record['sha256'],
                'Font source or license differs: ' + record['file'])
    clock = provenance['clock']
    header = (root / clock['outlineHeader']).read_bytes()
    require(sha256(header) == clock['outlineHeaderSha256'], 'Rounded font outline header differs')
    with zipfile.ZipFile(pbw) as archive:
        info = json.loads(archive.read('appinfo.json'))
        require(info['versionLabel'] == package['version'] and info['resources']['media'] == media,
                'Final PBW font identity/version differs')
        for notice in ('OFL-Fredoka.txt', 'OFL-RobotoCondensed.txt'):
            require(archive.read('licenses/' + notice) == (root / 'resources/fonts' / notice).read_bytes(),
                    'Final PBW font license differs: ' + notice)
        packed = archive.read('emery/app_resources.pbpack')
        require(struct.unpack_from('<I', packed)[0] == 0, 'Unused custom resources are still packaged')
        tables = native_tables(header.decode(), archive.read('emery/pebble-app.bin'))
    return {'result': 'PASS', 'version': package['version'], 'pbwSha256': sha256(pbw.read_bytes()),
            'bodyFonts': 'OS Gothic 18 Bold / 24 / 24 Bold; no packaged bitmap fonts', 'roundedClockTables': tables, 'fontLicensesMatchSource': True,
            'scope': 'Exact unchanged source/notices and linked Fredoka outlines; zero packaged bitmap fonts and OS Gothic source roles. Native captures separately verify pixels, fitting and motion.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pbw', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.pbw), indent=2))
    except (OSError, KeyError, ValueError, IndexError, struct.error, zipfile.BadZipFile) as error:
        parser.exit(1, 'Timer font audit: FAIL: ' + str(error) + '\n')
