#!/usr/bin/env python3
"""Build the stock SDK timer and include its independently callable plugin."""

from pathlib import Path
import hashlib
import json
import subprocess
import zipfile

root = Path(__file__).resolve().parents[1]
from repo_workflow import build_output

output_directory = build_output(root, "Timer")
library = root / "node_modules/pebble-activities"
package = json.loads((root / "package.json").read_text())
subprocess.run(["python3", str(root / "scripts/generate-localization.py")], cwd=root, check=True)
subprocess.run(["node", str(root / "scripts/generate-config.cjs")], check=True, cwd=root)
# SDK incremental JS signatures do not include transitive npm source changes.
# Distribution builds must rebundle the pinned dependency as well as native code.
subprocess.run(["pebble", "clean"], check=True, cwd=root)
build = root / "build"
build.mkdir(exist_ok=True)
subprocess.run(
    [
        "python3",
        str(root / "scripts/bundle-js.py"),
        str(root / "src/plugin/index.js"),
        str(build / "plugin.js"),
    ],
    check=True,
    cwd=root,
)
subprocess.run(["pebble", "build"], check=True, cwd=root)
# Production watch text uses the system's bitmap fonts. The old resource-font
# generator remains a historical comparison tool and is not a build input.
# The stock SDK names the development bundle after the project directory.
pbw = build / (root.name + ".pbw")
with zipfile.ZipFile(pbw) as source:
    entries = {name: source.read(name) for name in source.namelist()}
appinfo = json.loads(entries["appinfo.json"])
appinfo["manifestVersion"] = 2
appinfo["configPage"] = package["pebble"]["configPage"]
entries["config.html"] = (root / "config.html").read_bytes()
appinfo["usesPermissions"] = package["pebble"]["usesPermissions"]
appinfo["plugin"] = package["pebble"]["plugin"]
entries["appinfo.json"] = json.dumps(appinfo, indent=2).encode()
entries["plugin.js"] = (build / "plugin.js").read_bytes()
# Keep full source identity/debugging inputs outside the watch installation.
# Release scripts are reproduced by the pinned, compression-disabled minifier.
debug = {}
for name in ("pebble-js-app.js", "plugin.js"):
    debug[name] = entries[name]
    mapping = entries.pop(name + ".map", None)
    if mapping is not None:
        debug[name + ".map"] = mapping
    original = build / ("original-" + name)
    original.write_bytes(entries[name])
    output = build / "optimized" / name
    output_map = build / "optimized" / (name + ".map")
    command = [
        "node",
        str(root / "scripts/minify-js.cjs"),
        str(original),
        str(output),
        str(output_map),
    ]
    if mapping is not None:
        input_map = build / ("original-" + name + ".map")
        input_map.write_bytes(mapping)
        command.append(str(input_map))
    subprocess.run(command, check=True, cwd=root)
    entries[name] = output.read_bytes()
    debug["minified/" + name + ".map"] = output_map.read_bytes()
debug["optimization.json"] = (
    json.dumps(
        {
            "schema": 1,
            "version": package["version"],
            "minifier": "terser@5.46.0",
            "scripts": {
                name: {
                    "sourceSha256": hashlib.sha256(debug[name]).hexdigest(),
                    "releaseSha256": hashlib.sha256(entries[name]).hexdigest(),
                    "mapSha256": hashlib.sha256(debug["minified/" + name + ".map"]).hexdigest(),
                }
                for name in ("pebble-js-app.js", "plugin.js")
            },
        },
        indent=2,
    ).encode()
    + b"\n"
)
entries["LICENSE"] = (root / "LICENSE").read_bytes()
entries["licenses/pebble-activities-LICENSE.txt"] = (library / "LICENSE").read_bytes()
entries["THIRD-PARTY-NOTICES.md"] = (root / "THIRD-PARTY-NOTICES.md").read_bytes()
entries["licenses/Apache-2.0.txt"] = (root / "licenses/Apache-2.0.txt").read_bytes()
if (root / "src/c/generated_clock_glyphs.h").exists():
    for notice in ("OFL-Fredoka.txt", "OFL-RobotoCondensed.txt"):
        entries["licenses/" + notice] = (root / "resources/fonts" / notice).read_bytes()
destination = output_directory / ("Timer-" + package["version"] + ".pbw")
destination.parent.mkdir(parents=True, exist_ok=True)
debug_destination = destination.with_name("Timer-" + package["version"] + "-debug.zip")
for path, contents in ((destination, entries), (debug_destination, debug)):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as output:
        for name, content in sorted(contents.items()):
            entry = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o644 << 16
            output.writestr(entry, content)
(destination.parent / "SHA256SUMS.txt").write_text(
    "".join(
        hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name + "\n"
        for path in (destination, debug_destination)
    )
)
print(destination)
print(debug_destination)

from repo_workflow import record_build

record_build(root, destination, debug_destination)
