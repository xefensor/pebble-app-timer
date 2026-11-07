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
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(root / "node_modules"),
        str(root / "src/c/timer_model.c"), str(root / "tests/test_timer_model.c"),
        "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-collection-test-") as scratch:
    binary = Path(scratch) / "timer-collection"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(root / "node_modules"),
        str(root / "src/c/timer_model.c"), str(root / "src/c/timer_collection.c"),
        str(root / "tests/test_timer_collection.c"), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-ui-test-") as scratch:
    for name in ("timer_fonts", "timer_ui", "timer_ui_motion"):
        binary = Path(scratch) / name
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(root / "node_modules"),
            str(root / ("tests/test_" + name + ".c")), "-o", str(binary)
        ], check=True)
        subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-glance-test-") as scratch:
    binary = Path(scratch) / "timer-glance"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(root / "node_modules"),
        str(root / "src/c/timer_model.c"), str(root / "src/c/timer_collection.c"),
        str(root / "tests/test_timer_glance.c"), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-glance-runtime-test-") as scratch:
    binary = Path(scratch) / "timer-glance-runtime"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(root / "node_modules"),
        "-I", str(root / "tests/fake-sdk"),
        str(root / "src/c/timer_model.c"), str(root / "src/c/timer_collection.c"),
        str(root / "src/c/timer_glance.c"), str(root / "src/c/timer_appearance.c"),
        str(root / "tests/test_timer_glance_runtime.c"),
        "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-alarm-feedback-test-") as scratch:
    binary = Path(scratch) / "timer-alarm-feedback"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(root / "node_modules"),
        "-I", str(root / "tests/fake-sdk"),
        str(root / "src/c/timer_alarm_feedback.c"),
        str(root / "tests/test_timer_alarm_feedback.c"), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-wakeup-test-") as scratch:
    binary = Path(scratch) / "timer-wakeup"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-I", str(root / "tests/fake-sdk"), str(root / "tests/test_timer_wakeup.c"),
        "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
with tempfile.TemporaryDirectory(prefix="timer-appearance-test-") as scratch:
    binary = Path(scratch) / "timer-appearance"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(root / "node_modules"),
        "-I", str(root / "tests/fake-sdk"),
        str(root / "src/c/timer_appearance.c"),
        str(root / "tests/test_timer_appearance.c"), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
subprocess.run(["node", "--test", str(root / "tests/handoff.test.js"), str(root / "tests/settings.test.js")], check=True)
with tempfile.TemporaryDirectory(prefix="timer-ui-state-test-") as scratch:
    binary = Path(scratch) / "timer-ui-state"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(root / "node_modules"),
        str(root / "src/c/timer_model.c"), str(root / "src/c/timer_collection.c"),
        str(root / "src/c/timer_ui_state.c"), str(root / "tests/test_timer_ui_state.c"),
        "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
