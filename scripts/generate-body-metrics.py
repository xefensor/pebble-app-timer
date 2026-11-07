#!/usr/bin/env python3
"""Derive alignment tables from exact SDK PFOs; --check never changes files."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def parse_pfo(data):
    version, height, count, wild, buckets, cpbytes, header, features = struct.unpack_from('<BBHHBBBB', data)
    if version != 3 or cpbytes not in (2, 4):
        raise ValueError('Unsupported SDK PFO header')
    offbytes = 2 if features & 1 else 4
    table = header + buckets * 4
    entries = {}
    for bucket in range(buckets):
        _, number, position = struct.unpack_from('<BBH', data, header + bucket * 4)
        for i in range(number):
            at = table + position + i * (cpbytes + offbytes)
            entries[int.from_bytes(data[at:at+cpbytes], 'little')] = int.from_bytes(data[at+cpbytes:at+cpbytes+offbytes], 'little')
    if len(entries) != count:
        raise ValueError('PFO glyph count differs from index')
    base = table + count * (cpbytes + offbytes)
    result = {}
    for code, offset in entries.items():
        at = base + offset
        width, rows, left, top, advance = struct.unpack_from('<BBbbb', data, at)
        ink = any(data[at+5:at+5+(width*rows+7)//8])
        result[code] = {'top': top if ink else 255, 'bottom': top+rows-1 if ink else 0,
                        'advance': advance, 'left': left, 'width': width}
    return wild, result


def generate(packed, media):
    if struct.unpack_from('<I', packed)[0] != len(media):
        raise ValueError('Packaged resource count differs from utility roles')
    lines = ['// Generated from the exact SDK PFO resources by scripts/generate-body-metrics.py.', '#pragma once']
    wildcards, proof = [], []
    for i, item in enumerate(media):
        if item['type'] != 'font':
            continue
        number, offset, size, crc = struct.unpack_from('<4I', packed, 12+i*16)
        if number != i+1:
            raise ValueError('Unexpected resource ID ordering')
        data = packed[4108+offset:4108+offset+size]
        if len(data) != size:
            raise ValueError('Truncated font resource')
        wildcard, rows = parse_pfo(data)
        required = [ord('Ř'), ord('Ů'), ord('ž'), 0x2019, 0x2026, 0x218, 0x219, 0x21a, 0x21b]
        printable = list(range(0x20, 0x7f)) + list(range(0xa0, 0x180))
        if not all(code in rows for code in printable):
            raise ValueError('Font missing declared printable Latin codepoint')
        if not all(code in rows and rows[code]['top'] != 255 for code in required):
            raise ValueError('Font truncated before declared Latin coverage; use extended:true')
        wildcards.append(wildcard)
        lines.append('static const TimerGlyphInk s_body_ink_%d[]={' % i)
        entries = ['{%d,%d,%d}' % (code, rows[code]['top'], rows[code]['bottom']) for code in sorted(rows)]
        for start in range(0, len(entries), 8):
            lines.append('  '+','.join(entries[start:start+8])+',')
        lines.append('};')
        proof.append({'resource': item['name'], 'bytes': len(data), 'glyphs': len(rows),
                      'sha256': hashlib.sha256(data).hexdigest(),
                      'extendedLatinGlyphsVerified': ['Ř', 'Ů', 'ž', '’', '…']})
    lines.append('static const uint16_t s_body_wildcards[]={'+','.join(str(v) for v in wildcards)+'};')
    return '\n'.join(lines)+'\n', proof


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--pbw', type=Path)
    args = parser.parse_args()
    package = json.loads((ROOT/'package.json').read_text())
    if args.pbw:
        with zipfile.ZipFile(args.pbw) as archive:
            packed = archive.read('emery/app_resources.pbpack')
    else:
        packed = (ROOT/'build/emery/app_resources.pbpack').read_bytes()
    header, proof = generate(packed, package['pebble']['resources']['media'])
    target = ROOT/'src/c/generated_body_metrics.h'
    if args.check:
        if target.read_text() != header:
            raise ValueError('Generated utility metrics differ from exact native resources')
    else:
        if not target.exists() or target.read_text() != header:
            target.write_text(header)
        from repo_workflow import qa_output
        out = qa_output(ROOT, 'fonts')
        out.mkdir(parents=True, exist_ok=True)
        (out/'body-metrics.json').write_text(json.dumps({'scope': 'Exact SDK PFO ink bounds; extended Latin017F, apostrophe and ellipsis verified', 'resources': proof}, indent=2)+'\n')
    print(('Verified' if args.check else 'Generated')+' exact body metrics:', json.dumps(proof))


if __name__ == '__main__':
    main()
