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


def resources(packed, expected):
    require(len(packed) >= 4108 and struct.unpack_from('<I', packed)[0] == expected,
            'Native font resource count differs')
    result = []
    for index in range(expected):
        number, offset, length, _crc = struct.unpack_from('<4I', packed, 12 + index * 16)
        start = 4108 + offset
        require(number == index + 1 and length > 0 and start + length <= len(packed),
                'Invalid native font resource span')
        result.append(packed[start:start + length])
    return result


def font_metrics(data):
    require(len(data) >= 10, 'Truncated bitmap font')
    version, height, count, wildcard, buckets, cp_bytes, header, features = struct.unpack_from('<BBHHBBBB', data)
    require(version == 3 and cp_bytes in (2, 4) and header >= 10 and buckets and features & ~1 == 0,
            'Unsupported bitmap font format')
    offset_bytes = 2 if features & 1 else 4
    table = header + buckets * 4
    entries, total = {}, 0
    for bucket in range(buckets):
        _hash, number, position = struct.unpack_from('<BBH', data, header + bucket * 4)
        total += number
        for index in range(number):
            at = table + position + index * (cp_bytes + offset_bytes)
            require(at + cp_bytes + offset_bytes <= len(data), 'Truncated glyph lookup')
            code = int.from_bytes(data[at:at + cp_bytes], 'little')
            offset = int.from_bytes(data[at + cp_bytes:at + cp_bytes + offset_bytes], 'little')
            require(code not in entries, 'Duplicate glyph lookup')
            entries[code] = offset
    require(total == count, 'Bitmap font lookup count differs')
    base = table + total * (cp_bytes + offset_bytes)
    result = {}
    for code, offset in entries.items():
        at = base + offset
        require(at + 5 <= len(data), 'Truncated glyph metrics')
        width, rows, left, top, advance = struct.unpack_from('<BBbbb', data, at)
        size = (width * rows + 7) // 8
        require(at + 5 + size <= len(data), 'Truncated glyph bitmap')
        result[code] = {'advance': advance, 'hasInk': any(data[at + 5:at + 5 + size]),
                        'left': left, 'top': top, 'width': width, 'height': rows}
    return height, wildcard, result


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
    require(media == provenance['bodyResources'] and
            [item['name'] for item in media] == ['FONT_BODY_' + str(n) for n in (14, 15, 16, 18, 19, 23)],
            'Body resource definitions differ from reviewed provenance')
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
        raw = resources(archive.read('emery/app_resources.pbpack'), len(media))
        tables = native_tables(header.decode(), archive.read('emery/pebble-app.bin'))
    definitions = re.findall(r'#define\s+RESOURCE_ID_(FONT_BODY_\d+)\s+(\d+)',
                             (root / 'build/emery/src/resource_ids.auto.h').read_text())
    require(definitions == [(item['name'], str(i + 1)) for i, item in enumerate(media)],
            'Generated font resource IDs differ')
    required = set(range(32, 127)) | {0xb7, 0x2019, 0x2026, 0x25af}
    samples = {sample['point']: sample for sample in provenance['sdk']['standalonePfoSamples']}
    fonts = []
    for item, data in zip(media, raw):
        point = int(item['name'].rsplit('_', 1)[1])
        sample = samples[point]
        require(sha256(data) == sample['sha256'] and len(data) == sample['bytes'],
                'Packaged bitmap differs from exact standalone SDK output: ' + item['name'])
        height, wildcard, glyphs = font_metrics(data)
        require(height == point and wildcard == 0x25af and set(glyphs) == required and
                len(glyphs) == sample['glyphCount'], 'Bitmap glyph set/height differs: ' + item['name'])
        require(all(glyphs[cp]['hasInk'] for cp in required if cp != 32),
                'A visible Timer label glyph is empty: ' + item['name'])
        fonts.append({'name': item['name'], 'point': point, 'glyphs': len(glyphs),
                      'bytes': len(data), 'sha256': sha256(data)})
    return {'result': 'PASS', 'version': package['version'], 'pbwSha256': sha256(pbw.read_bytes()),
            'bodyFonts': fonts, 'roundedClockTables': tables, 'fontLicensesMatchSource': True,
            'scope': 'Exact source/SDK bitmap and native outline identity; native captures separately verify pixels, fitting and motion.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pbw', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.pbw), indent=2))
    except (OSError, KeyError, ValueError, IndexError, struct.error, zipfile.BadZipFile) as error:
        parser.exit(1, 'Timer font audit: FAIL: ' + str(error) + '\n')
