#!/usr/bin/env python3
"""Flatten the committed Fredoka clock font into bounded native GPath polygons.

Requires fontTools and skia-pathops when regenerating/checking this asset.
Coordinates stay in the original y-up font units. Native code can scale every
frame without switching fonts. Outer contours precede background-filled holes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

import fontTools
import pathops
from fontTools.pens.basePen import BasePen
from fontTools.ttLib import TTFont
from fontTools.ttLib.removeOverlaps import removeOverlaps

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "resources/fonts/Fredoka-Bold-Clock.ttf"
HEADER = ROOT / "src/c/generated_clock_glyphs.h"
CHARS = "0123456789:"
TOLERANCE = 2.0  # 0.16px at the largest 80px clock scale.


def distance(point, first, last):
    dx, dy = last[0] - first[0], last[1] - first[1]
    if dx == 0 and dy == 0:
        return math.hypot(point[0] - first[0], point[1] - first[1])
    return abs(dy * point[0] - dx * point[1] + last[0] * first[1]
               - last[1] * first[0]) / math.hypot(dx, dy)


def midpoint(first, last):
    return ((first[0] + last[0]) / 2, (first[1] + last[1]) / 2)


class PolygonPen(BasePen):
    def __init__(self, glyph_set):
        super().__init__(glyph_set)
        self.contours = []
        self.current = []

    def _moveTo(self, point):
        assert not self.current
        self.current = [point]

    def _lineTo(self, point):
        self.current.append(point)

    def quadratic(self, first, control, last, depth=0):
        if distance(control, first, last) <= TOLERANCE or depth >= 16:
            self.current.append(last)
            return
        a, b = midpoint(first, control), midpoint(control, last)
        center = midpoint(a, b)
        self.quadratic(first, a, center, depth + 1)
        self.quadratic(center, b, last, depth + 1)

    def _qCurveToOne(self, control, last):
        self.quadratic(self.current[-1], control, last)

    def cubic(self, first, control1, control2, last, depth=0):
        if (max(distance(control1, first, last),
                distance(control2, first, last)) <= TOLERANCE or depth >= 16):
            self.current.append(last)
            return
        a, b, c = midpoint(first, control1), midpoint(control1, control2), midpoint(control2, last)
        ab, bc = midpoint(a, b), midpoint(b, c)
        center = midpoint(ab, bc)
        self.cubic(first, a, ab, center, depth + 1)
        self.cubic(center, bc, c, last, depth + 1)

    def _curveToOne(self, control1, control2, last):
        self.cubic(self.current[-1], control1, control2, last)

    def _closePath(self):
        points = []
        for x, y in self.current:
            point = (round(x), round(y))
            if not points or point != points[-1]:
                points.append(point)
        if points and points[-1] == points[0]:
            points.pop()
        # Preserve geometry while dropping exactly collinear middle vertices.
        changed = True
        while changed and len(points) > 3:
            changed = False
            for i, point in enumerate(points):
                a, b = points[i - 1], points[(i + 1) % len(points)]
                if (point[0] - a[0]) * (b[1] - point[1]) == (point[1] - a[1]) * (b[0] - point[0]):
                    del points[i]
                    changed = True
                    break
        assert len(points) >= 3
        self.contours.append(points)
        self.current = []

    def _endPath(self):
        raise ValueError("A font glyph contains an open path")


def area(points):
    return sum(a[0] * b[1] - b[0] * a[1]
               for a, b in zip(points, points[1:] + points[:1])) / 2


def contains(points, point):
    x, y = point
    inside = False
    for a, b in zip(points, points[1:] + points[:1]):
        if ((a[1] > y) != (b[1] > y)
                and x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0]):
            inside = not inside
    return inside


def build():
    font = TTFont(FONT)
    cmap = font.getBestCmap()
    # A variable weight instance can retain overlapping strokes. TrueType's
    # nonzero winding fills these, while a simple native polygon can cut them
    # out under even-odd filling. Boolean-union the original curved contours
    # before flattening. Do not alter the committed source TTF.
    removeOverlaps(font, glyphNames=[cmap[ord(char)] for char in CHARS])
    glyph_set = font.getGlyphSet()
    points, contours, glyphs = [], [], []
    for char in CHARS:
        name = cmap[ord(char)]
        pen = PolygonPen(glyph_set)
        glyph_set[name].draw(pen)
        assert not pen.current
        shapes = []
        for polygon in pen.contours:
            a = abs(area(polygon))
            assert a > 0
            depth = sum(abs(area(parent)) > a and contains(parent, polygon[0])
                        for parent in pen.contours)
            shapes.append((depth, a, polygon))
        shapes.sort(key=lambda shape: (shape[0], -shape[1]))
        first_contour = len(contours)
        for depth, _, polygon in shapes:
            assert all(-32768 <= coordinate <= 32767 for point in polygon for coordinate in point)
            contours.append((len(points), len(polygon), depth % 2 == 1))
            points.extend(polygon)
        glyph = font["glyf"][name]
        glyph.recalcBounds(font["glyf"])
        glyphs.append((char, font["hmtx"].metrics[name][0], first_contour, len(shapes),
                       glyph.xMin, glyph.yMin, glyph.xMax, glyph.yMax))
    assert len(points) <= 65535 and len(contours) <= 65535
    maximum = max(contour[1] for contour in contours)
    maximum_glyph_points = max(sum(contours[i][1] for i in range(glyph[2], glyph[2] + glyph[3]))
                               for glyph in glyphs)
    maximum_glyph_contours = max(glyph[3] for glyph in glyphs)
    assert maximum <= 256
    sha = hashlib.sha256(FONT.read_bytes()).hexdigest()
    lines = [
        "// Generated by scripts/generate-clock-glyphs.py; do not edit.",
        "// Fredoka Bold, Copyright 2016 The Fredoka Project Authors.",
        "// Font-derived data: SIL Open Font License 1.1; see resources/fonts/OFL-Fredoka.txt.",
        "// Source TTF SHA256: " + sha,
        "// Nonzero-winding union before flattening preserves overlapping strokes.",
        "#pragma once", "", "#include <stdbool.h>", "#include <stdint.h>", "",
        "#define CLOCK_FONT_UNITS_PER_EM " + str(font["head"].unitsPerEm),
        "#define CLOCK_CONTOUR_MAX_POINTS " + str(maximum),
        "#define CLOCK_GLYPH_MAX_POINTS " + str(maximum_glyph_points),
        "#define CLOCK_GLYPH_MAX_CONTOURS " + str(maximum_glyph_contours),
        "#define CLOCK_POINT_COUNT " + str(len(points)),
        "#define CLOCK_CONTOUR_COUNT " + str(len(contours)),
        "#define CLOCK_GLYPH_COUNT " + str(len(glyphs)),
        '#define CLOCK_FONT_SOURCE_SHA256 "' + sha + '"',
        "",
        "typedef struct { int16_t x, y; } ClockPoint;",
        "typedef struct { uint16_t first_point, point_count; bool hole; } ClockContour;",
        "typedef struct {",
        "  uint32_t code;",
        "  uint16_t advance, first_contour, contour_count;",
        "  int16_t x_min, y_min, x_max, y_max;",
        "} ClockGlyph;", "",
        "static const ClockPoint clock_points[CLOCK_POINT_COUNT] = {",
    ]
    for at in range(0, len(points), 8):
        lines.append("  " + ", ".join("{%d,%d}" % point for point in points[at:at + 8]) + ",")
    lines.extend(["};", "", "static const ClockContour clock_contours[CLOCK_CONTOUR_COUNT] = {"])
    for first, count, hole in contours:
        lines.append("  {%d,%d,%s}," % (first, count, "true" if hole else "false"))
    lines.extend(["};", "", "static const ClockGlyph clock_glyphs[CLOCK_GLYPH_COUNT] = {"])
    for char, advance, first, count, xmin, ymin, xmax, ymax in glyphs:
        code = "'%s'" % char if ord(char) < 128 else "0x%04x" % ord(char)
        lines.append("  {%s,%d,%d,%d,%d,%d,%d,%d}," %
                     (code, advance, first, count, xmin, ymin, xmax, ymax))
    lines.extend(["};", ""])
    info = {
        "fontSha256": sha,
        "unitsPerEm": font["head"].unitsPerEm,
        "glyphCount": len(glyphs),
        "contourCount": len(contours),
        "pointCount": len(points),
        "maximumContourPoints": maximum,
        "maximumGlyphPoints": maximum_glyph_points,
        "maximumGlyphContours": maximum_glyph_contours,
        "quadraticCubicFlatnessToleranceFontUnits": TOLERANCE,
        "maximumCurveFlatnessPxAt80": TOLERANCE * 80 / font["head"].unitsPerEm,
        "characters": CHARS,
        "holeContours": sum(contour[2] for contour in contours),
        "overlapRemoval": "fontTools.ttLib.removeOverlaps with skia-pathops; curved nonzero winding union before flattening",
        "fontToolsVersion": fontTools.__version__,
        "pathopsVersion": pathops.__version__,
        "glyphAdvances": {glyph[0]: glyph[1] for glyph in glyphs},
    }
    return "\n".join(lines), info


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="check exact header bytes without writing")
    parser.add_argument("--json", action="store_true", help="print machine-readable generation metrics")
    args = parser.parse_args()
    generated, info = build()
    if args.check:
        if not HEADER.is_file() or HEADER.read_text() != generated:
            raise SystemExit("Generated clock header is stale; rerun scripts/generate-clock-glyphs.py")
    else:
        HEADER.write_text(generated)
    if args.json:
        print(json.dumps({"result": "PASS", **info}, indent=2))
    else:
        print(("Verified" if args.check else "Generated") + " " + str(HEADER.relative_to(ROOT)))
        print("%d glyphs, %d contours, %d points; largest contour %d points" %
              (info["glyphCount"], info["contourCount"], info["pointCount"], info["maximumContourPoints"]))


if __name__ == "__main__":
    main()
