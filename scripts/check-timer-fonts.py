#!/usr/bin/env python3
"""Audit exact native Timer font assets; native captures verify visual fitting."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('body_metrics', ROOT/'scripts/generate-body-metrics.py')
METRICS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(METRICS)


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
    require(len(media) == 3 and [m['name'] for m in media] == ['FONT_BODY_22', 'FONT_BODY_24', 'FONT_BODY_26'],
            'Utility font resource roles differ')
    require(all(m['type']=='font' and m['file']=='fonts/RobotoCondensed-Bold.ttf' and
                m['extended'] is True and m['characterRegex']==r'[\u0020-\u017F\u2019\u2026]' for m in media),
            'Utility font source, extended indexing or declared coverage differs')
    utility = (root / 'src/c/timer_fonts.c').read_text()
    require(all('RESOURCE_ID_'+m['name'] in utility for m in media) and
            'fonts_load_custom_font' in utility and 'fonts_unload_custom_font' in utility and
            '#include "generated_body_metrics.h"' in utility,
            'Utility resource lifetime or exact alignment tables are missing')
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
        body_header, body_proof = METRICS.generate(packed, media)
        require(body_header == (root/'src/c/generated_body_metrics.h').read_text(),
                'Utility alignment table differs from packaged font bytes')
        binary = archive.read('emery/pebble-app.bin')
        for i in range(3):
            array = re.search(r's_body_ink_'+str(i)+r'\[\]=\{(.*?)\n\};', body_header, re.S)[1]
            encoded = b''.join(struct.pack('<Hhh', *(int(v) for v in row.split(',')))
                               for row in re.findall(r'\{([^{}]+)\}', array))
            require(encoded in binary, 'Native binary lacks exact utility alignment table '+str(i))
        tables = native_tables(header.decode(), binary)
    return {'result': 'PASS', 'version': package['version'], 'pbwSha256': sha256(pbw.read_bytes()),
            'bodyFonts': body_proof, 'packagedResourcesBytes': len(packed),
            'roundedClockTables': tables, 'fontLicensesMatchSource': True,
            'scope': 'Exact unchanged font sources/notices, linked Fredoka outlines and three extended Roboto Condensed PFOs/alignment tables. Coverage verified in compiled PFOs, including Czech glyphs. Native captures separately verify whole-composition ink, fitting and motion.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pbw', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.pbw), indent=2))
    except (OSError, KeyError, ValueError, IndexError, struct.error, zipfile.BadZipFile) as error:
        parser.exit(1, 'Timer font audit: FAIL: ' + str(error) + '\n')
