#!/usr/bin/env python3
"""Measure the portable Timer raster on this host, never watch battery/latency.

This builds temporary instrumentation around the exact committed scanline
implementation. It counts edge tests/intersections and hashes coverage while
timing repeated digit corpora at all four UI clock sizes. No shipping code or
font data is changed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PROGRAM = r'''
#include <stdint.h>
#include <stdio.h>
#include <time.h>
static uint64_t edge_tests, divisions, checksum;
#include "font_raster.h"
#include "generated_clock_glyphs.h"
static FontRasterPoint points[CLOCK_GLYPH_MAX_POINTS];
static FontRasterContour contours[CLOCK_GLYPH_MAX_CONTOURS];
static int32_t intersections[CLOCK_GLYPH_MAX_POINTS];
static uint8_t coverage[200];
static int32_t rounded(int64_t n) {return (int32_t)((n < 0 ? n-500 : n+500)/1000);}
static void row(void *context,int y,int first,int end,const uint8_t *pixels) {
  (void)context;
  for(int x=first;x<end;++x)checksum=checksum*33+pixels[x]+(unsigned)y;
}
int main(void) {
  const int sizes[]={24,30,36,58};
  for(unsigned size=0;size<4;++size) {
    edge_tests=divisions=checksum=0;
    const int point=sizes[size];
    const clock_t start=clock();
    for(unsigned run=0;run<RUNS;++run)for(unsigned digit=0;digit<CLOCK_GLYPH_COUNT;++digit) {
      const ClockGlyph *glyph=&clock_glyphs[digit];
      unsigned count=0;
      for(unsigned i=0;i<glyph->contour_count;++i) {
        const ClockContour *part=&clock_contours[glyph->first_contour+i];
        contours[i]=(FontRasterContour){count,part->point_count};
        for(unsigned j=0;j<part->point_count;++j) {
          const ClockPoint p=clock_points[part->first_point+j];
          points[count++]=(FontRasterPoint){50*256+rounded((int64_t)p.x*point*256),
              90*256-rounded((int64_t)p.y*point*256)};
        }
      }
      const FontRasterScratch scratch={coverage,sizeof(coverage),intersections,CLOCK_GLYPH_MAX_POINTS};
      if(!font_raster_rows(points,count,contours,glyph->contour_count,200,228,scratch,row,NULL))return 1;
    }
    printf("%d %.9f %llu %llu %llu\n",point,(double)(clock()-start)/CLOCKS_PER_SEC,
      (unsigned long long)edge_tests,(unsigned long long)divisions,(unsigned long long)checksum);
  }
  return 0;
}
'''


def benchmark(runs):
    header = (ROOT / 'src/c/font_raster.h').read_text()
    edge = 'const FontRasterPoint b = points[c.first_point + i];'
    division = 'const int64_t numerator ='
    assert header.count(edge) == header.count(division) == 1
    instrumented = header.replace(edge, '++edge_tests; ' + edge).replace(division, '++divisions; ' + division)
    compiler = os.environ.get('CC', 'cc')
    with tempfile.TemporaryDirectory(prefix='timer-raster-profile-') as directory:
        path = Path(directory)
        (path / 'font_raster.h').write_text(instrumented)
        (path / 'generated_clock_glyphs.h').write_bytes((ROOT / 'src/c/generated_clock_glyphs.h').read_bytes())
        (path / 'benchmark.c').write_text(PROGRAM)
        subprocess.run([compiler, '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
            '-DRUNS=' + str(runs), str(path / 'benchmark.c'), '-o', str(path / 'benchmark')], check=True)
        output = subprocess.check_output([str(path / 'benchmark')], text=True)
    samples = []
    for line in output.splitlines():
        point, seconds, edges, divisions, checksum = line.split()
        samples.append({'point': int(point), 'digitCorpora': runs, 'glyphsPerCorpus': 11,
            'hostCpuSeconds': float(seconds), 'edgeTests': int(edges),
            'intersectionDivisions': int(divisions), 'coverageChecksum': checksum})
    return {'result': 'PASS', 'environment': 'Portable host CPU only: ' + platform.machine(),
        'compiler': subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
        'rasterSha256': hashlib.sha256(header.encode()).hexdigest(), 'samples': samples,
        'decision': 'Keep bounded allocation-free raster scratch. Remove unnecessary frames first; a glyph coverage cache adds RAM and phase/invalidation handling and needs actual watch timings to justify it.',
        'limits': 'Not SDK/QEMU render timing, native power, frame latency, or physical battery evidence.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runs', type=int, default=1000)
    args = parser.parse_args()
    if not 1 <= args.runs <= 100000:
        parser.error('--runs must be between 1 and 100000')
    print(json.dumps(benchmark(args.runs), indent=2))
