#!/usr/bin/env python3
"""Portable timer-owner checks; no SDK, phone or hardware claims."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="activity-timer-test-") as scratch:
    binary = Path(scratch) / "timer-model"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
        str(root / "src/c/timer_model.c"), str(root / "tests/test_timer_model.c"),
        "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-collection-test-") as scratch:
    binary = Path(scratch) / "timer-collection"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
        str(root / "src/c/timer_model.c"), str(root / "src/c/timer_collection.c"),
        str(root / "tests/test_timer_collection.c"), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-ui-test-") as scratch:
    for name in ("timer_fonts", "timer_ui"):
        binary = Path(scratch) / name
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
            str(root / ("tests/test_" + name + ".c")), "-o", str(binary)
        ], check=True)
        subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-glance-test-") as scratch:
    binary = Path(scratch) / "timer-glance"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
        str(root / "src/c/timer_model.c"), str(root / "src/c/timer_collection.c"),
        str(root / "tests/test_timer_glance.c"), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-glance-runtime-test-") as scratch:
    binary = Path(scratch) / "timer-glance-runtime"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-I", str(root / "tests/fake-sdk"),
        str(root / "src/c/timer_model.c"), str(root / "src/c/timer_collection.c"),
        str(root / "src/c/timer_glance.c"), str(root / "tests/test_timer_glance_runtime.c"),
        "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
subprocess.run(["node", "--test", str(root / "tests/handoff.test.js")], check=True)
