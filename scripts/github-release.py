#!/usr/bin/env python3
"""Validate committed release artifacts and publish them without rebuilding or replacing assets."""
import argparse
import base64
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import tempfile
import uuid
import zipfile


ROOT = Path(__file__).resolve().parents[1]
VERSION = re.compile(r"[0-9]+\.[0-9]+\.[0-9]+")
SHA = re.compile(r"[a-f0-9]{40}")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(content):
    return hashlib.sha256(content).hexdigest()


def safe_path(value):
    require(isinstance(value, str) and value and "\\" not in value and ":" not in value,
            "Invalid repository path")
    path = PurePosixPath(value)
    require(not path.is_absolute() and ".." not in path.parts and str(path) == value,
            "Unsafe repository path: " + value)
    return value


def git(root, *args):
    result = subprocess.run(["git", "-C", str(root), *args], capture_output=True)
    require(result.returncode == 0, "Cannot read committed release input: " + " ".join(args[:2]))
    return result.stdout


def committed(root, commit, path):
    return git(root, "show", commit + ":" + safe_path(path))


def release_id(package):
    version = package.get("version")
    require(isinstance(version, str) and VERSION.fullmatch(version) and
            all(int(part) <= 255 for part in version.split(".")), "Invalid package version")
    qualifiers = [(name, package[name]) for name in
                  ("halftoneReleaseCandidate", "halftoneBeta", "releaseCandidate") if name in package]
    require(len(qualifiers) <= 1, "Multiple prerelease counters")
    if qualifiers:
        name, number = qualifiers[0]
        require(type(number) is int and number > 0, "Invalid prerelease counter")
        return version + ("-beta." if name == "halftoneBeta" else "-rc.") + str(number)
    return version


def expand(value, package):
    return safe_path(value.format(version=package["version"], release_id=release_id(package)))


def zip_files(content):
    with zipfile.ZipFile(io.BytesIO(content)) as archive:
        require(archive.testzip() is None, "Corrupt ZIP artifact")
        entries = [entry for entry in archive.infolist() if not entry.is_dir()]
        require(len(entries) == len({entry.filename for entry in entries}), "Duplicate ZIP member")
        require(sum(entry.file_size for entry in entries) <= 256 * 1024 * 1024,
                "ZIP artifact exceeds validation budget")
        for entry in entries:
            safe_path(entry.filename)
            require((entry.external_attr >> 16) & 0o170000 != 0o120000, "ZIP symlink is not supported")
        return {entry.filename: archive.read(entry) for entry in entries}


def critical(path):
    return path.startswith(("src/", "resources/", "vendor/", "include/", "licenses/")) or path in {
        "package.json", "package-lock.json", "config.html", "wscript", "LICENSE", "THIRD-PARTY-NOTICES.md",
        "THIRD_PARTY.md", "scripts/build.py", "scripts/build-js.js", "scripts/package-release.py",
        "scripts/package-source.py", "scripts/release_licenses.py"} or path.startswith("scripts/")


def validate_source(root, commit, source, prefix=""):
    files = zip_files(source)
    if prefix:
        safe_path(prefix.rstrip("/"))
        require(prefix.endswith("/") and all(name.startswith(prefix) for name in files),
                "Unexpected source archive prefix")
        files = {name[len(prefix):]: content for name, content in files.items()}
    for path in files:
        member = PurePosixPath(path)
        require(member.parts[0].lower() not in {"build", "node_modules", ".git", ".pebble"},
                "Source archive contains an excluded working directory: " + path)
        require(member.suffix.lower() != ".pbw", "Source archive contains a compiled PBW: " + path)
    names = git(root, "ls-tree", "-r", "--name-only", "-z", commit).decode().split("\0")
    for path in names:
        if path and critical(path):
            require(path in files, "Source archive omits build input: " + path)
    for path, content in files.items():
        if critical(path):
            require(content == committed(root, commit, path), "Stale source archive input: " + path)
    return files


def checksum_map(content, asset_paths, checksums_path):
    result = {}
    for line in content.decode().splitlines():
        match = re.fullmatch(r"([a-f0-9]{64}) [ *](.+)", line)
        require(match, "Invalid checksum line")
        name = safe_path(match[2])
        require(name not in result, "Duplicate checksum entry: " + name)
        result[name] = match[1]
    selected = {}
    for path in asset_paths:
        candidates = [key for key in (path, str(PurePosixPath(path).relative_to(
            PurePosixPath(checksums_path).parent)) if
            PurePosixPath(checksums_path).parent in PurePosixPath(path).parents else "") if key in result]
        require(candidates and len({result[key] for key in candidates}) == 1,
                "Missing or ambiguous checksum: " + path)
        selected[path] = result[candidates[0]]
    return selected


def pebble_crc(content):
    """STM32 word CRC used by the Pebble PBW manifest (not ZIP/zlib CRC)."""
    crc = 0xffffffff
    for offset in range(0, len(content), 4):
        word = content[offset:offset + 4]
        crc ^= int.from_bytes(word, "little" if len(word) == 4 else "big")
        for _ in range(32):
            crc = ((crc << 1) ^ (0x04c11db7 if crc & 0x80000000 else 0)) & 0xffffffff
    return crc


def webpack_source(text, original, path):
    tail = text[len(original):]
    require(text.startswith(original) and (not tail or re.fullmatch(
        r"\n{2,}//////////////////\n// WEBPACK FOOTER(?:\n//[^\n]*)*\s*", tail)),
        "Reference timer PKJS/source differs: " + path)


def validate_timer_pkjs(root, commit, prefix, files, dependency=None):
    script = files.get("pebble-js-app.js", b"").decode()
    require(script and "pebble-js-app.js.map" in files, "Reference timer PKJS is missing")
    mapping = json.loads(files["pebble-js-app.js.map"])
    require(len(mapping["sources"]) == len(mapping["sourcesContent"]), "Invalid timer PKJS source map")
    expected = {"./src/pkjs/index.js": prefix + "/src/pkjs/index.js",
                "publisher.js": "src/js/publisher.js", "wire.js": "src/js/wire.js"}
    if dependency is not None:
        expected = {"./src/pkjs/index.js": "src/pkjs/index.js",
            "./~/pebble-activities/dist/js/publisher.js": "src/js/publisher.js",
            "./~/pebble-activities/dist/js/wire.js": "src/js/wire.js"}
        if "src/js/view.js" in dependency:
            expected["./~/pebble-activities/dist/js/view.js"] = "src/js/view.js"
        if "src/js/activity.js" in dependency:
            expected["./~/pebble-activities/dist/js/activity.js"] = "src/js/activity.js"
    entry_source = committed(root, commit, expected["./src/pkjs/index.js"]).decode()
    if re.search(r"require\(\s*['\"]\./settings['\"]\s*\)", entry_source):
        expected["./src/pkjs/settings.js"] = "src/pkjs/settings.js"
    mapped = {}
    originals = {}
    for path, text in zip(mapping["sources"], mapping["sourcesContent"]):
        if path.startswith("webpack/bootstrap "):
            continue
        if PurePosixPath(path).name == "_pkjs_shared_additions.js":
            path = "_pkjs_shared_additions.js"
        require(path in expected or path == "_pkjs_shared_additions.js", "Unexpected timer PKJS module: " + path)
        require(path not in mapped, "Duplicate timer PKJS source: " + path)
        module = re.search(r"\n// module id = ([0-9]+)\n", text)
        require(module, "Timer PKJS source has no module identity: " + path)
        mapped[path] = int(module[1])
        if path in expected:
            original = (dependency[expected[path]] if dependency is not None and path.startswith("./~/")
                        else committed(root, commit, expected[path])).decode()
            webpack_source(text, original, path)
        else:
            original = re.split(r"\n{2,}//////////////////\n// WEBPACK FOOTER", text)[0]
        originals[path] = original
    require(set(mapped) == set(expected) | {"_pkjs_shared_additions.js"}, "Timer PKJS omits required sources")
    modules = {int(number): body for number, body in re.findall(
        r"/\* ([0-9]+) \*/\n/\*\*\*/ \(function\([^)]*\) \{\n\n(.*?)\n\n/\*\*\*/ \}\)", script, re.S)}
    require(set(modules) == {0} | set(mapped.values()), "Unexpected timer PKJS compiled modules")
    for path, original in originals.items():
        def rewrite(match):
            target = ({"pebble-activities/publisher": "./~/pebble-activities/dist/js/publisher.js",
                       "./wire": "./~/pebble-activities/dist/js/wire.js",
                       "./activity": "./~/pebble-activities/dist/js/activity.js",
                       "./view": "./~/pebble-activities/dist/js/view.js",
                       "./settings": "./src/pkjs/settings.js"} if dependency is not None else
                      {"pebble-activities/publisher": "publisher.js", "./wire": "wire.js"}).get(match[2])
            require(target in mapped, "Unsupported timer PKJS import: " + match[2])
            return "__webpack_require__(" + str(mapped[target]) + ")"
        compiled = re.sub(r"\brequire\(\s*(['\"])([^'\"]+)\1\s*\)", rewrite, original)
        body = "\n".join(line[1:] if line.startswith("\t") else line
                         for line in modules[mapped[path]].splitlines())
        require(body.strip() == compiled.strip(), "Timer PKJS compiled code differs: " + path)
    entry = modules[0].strip()
    require(entry == "__webpack_require__(%d);\n\tmodule.exports = __webpack_require__(%d);" %
            (mapped["_pkjs_shared_additions.js"], mapped["./src/pkjs/index.js"]),
            "Timer PKJS entry differs")


def timer_dependency(root, commit, package):
    request = package["dependencies"]["pebble-activities"]
    require(request.startswith("file:vendor/"), "Timer must use a vendored Activities release")
    path = safe_path(request[5:])
    content = committed(root, commit, path)
    lock = json.loads(committed(root, commit, "package-lock.json"))
    record = lock["packages"]["node_modules/pebble-activities"]
    require(record["resolved"] == request and record["integrity"] ==
            "sha512-" + base64.b64encode(hashlib.sha512(content).digest()).decode(),
            "Activities dependency lock differs")
    with tarfile.open(fileobj=io.BytesIO(content), mode="r:gz") as archive:
        members = archive.getmembers()
        require(all(member.isfile() and member.name.startswith("package/") for member in members),
                "Unsafe Activities dependency member")
        require(len(members) == len({member.name for member in members}) and
                sum(member.size for member in members) <= 16 * 1024 * 1024, "Invalid Activities dependency archive")
        files = {safe_path(member.name[8:]): archive.extractfile(member).read() for member in members}
    metadata = json.loads(files["package.json"])
    require(metadata["name"] == "pebble-activities" and metadata["version"] == record["version"],
            "Activities dependency identity differs")
    return files


def validate_timer_optimization(root, commit, package, files, debug):
    """Prove optimized runtime bytes from separately audited SDK/source inputs."""
    require(not any(name.endswith('.map') for name in files), 'Debug map in release PBW')
    require(set(debug) == {'pebble-js-app.js', 'pebble-js-app.js.map', 'plugin.js',
            'minified/pebble-js-app.js.map', 'minified/plugin.js.map', 'optimization.json'},
            'Unexpected optimization debug inputs')
    metadata = json.loads(debug['optimization.json'])
    require(metadata.get('schema') == 1 and metadata.get('version') == package['version'] and
            metadata.get('minifier') == 'terser@5.46.0', 'Invalid optimization metadata')
    with tempfile.TemporaryDirectory(prefix='timer-release-minifier-') as directory:
        checkout = Path(directory)
        script = checkout / 'minify-js.cjs'
        script.write_bytes(committed(root, commit, 'scripts/minify-js.cjs'))
        environment = dict(os.environ, NODE_PATH=str(root / 'node_modules'))
        for name in ('pebble-js-app.js', 'plugin.js'):
            record = metadata.get('scripts', {}).get(name, {})
            require(record == {'sourceSha256': digest(debug[name]), 'releaseSha256': digest(files[name]),
                    'mapSha256': digest(debug['minified/' + name + '.map'])},
                    'Optimization input identity differs: ' + name)
            source = checkout / ('source-' + name)
            source.write_bytes(debug[name])
            output = checkout / name
            mapping = checkout / (name + '.map')
            command = ['node', str(script), str(source), str(output), str(mapping)]
            if name + '.map' in debug:
                input_map = checkout / ('source-' + name + '.map')
                input_map.write_bytes(debug[name + '.map'])
                command.append(str(input_map))
            result = subprocess.run(command, env=environment, capture_output=True)
            require(result.returncode == 0 and output.is_file(),
                    'Cannot reproduce pinned release minification: ' + result.stderr.decode())
            require(output.read_bytes() == files[name] and
                    mapping.read_bytes() == debug['minified/' + name + '.map'],
                    'Optimized release code/map differs: ' + name)


def validate_timer(root, config, commit, package, files, debug=None):
    source_files = files if debug is None else {**files, **{
        name: debug[name] for name in ('pebble-js-app.js', 'pebble-js-app.js.map', 'plugin.js')}}
    expected = package["pebble"]
    info = json.loads(files["appinfo.json"])
    require(info.get("uuid") == expected["uuid"] == config["app_uuid"] and
            info.get("versionLabel") == package["version"], "Timer PBW identity/version differs")
    require(info.get("manifestVersion") == 2 and info.get("watchapp", {}).get("watchface") is False and
            info.get("projectType") == "native", "Timer PBW is not a manifestVersion 2 native watchapp")
    for field in ("plugin", "messageKeys", "usesPermissions", "targetPlatforms"):
        require(info.get(field) == expected[field], "Timer manifest differs: " + field)
    require(info.get("shortName") == info.get("longName") == expected["displayName"], "Timer display name differs")
    require(info.get("appKeys") == expected["messageKeys"], "Timer native keys differ")
    require(info["plugin"].get("script") == "plugin.js" and files.get("plugin.js"), "Timer plugin is missing")
    if expected.get("configPage"):
        require(info.get("configPage") == expected["configPage"] and
                files.get(expected["configPage"]) == committed(root, commit, expected["configPage"]),
                "Timer configuration page differs")
    dependency = timer_dependency(root, commit, package)
    require(files.get("LICENSE") == committed(root, commit, "LICENSE"), "Timer license differs")
    require(files.get("licenses/pebble-activities-LICENSE.txt") == dependency["LICENSE"], "Activities license differs")
    for notice in ("THIRD-PARTY-NOTICES.md", "licenses/Apache-2.0.txt"):
        require(files.get(notice) == committed(root, commit, notice), "Timer SDK notice differs: " + notice)
    font_media = [item for item in expected.get("resources", {}).get("media", [])
                  if item.get("type") == "font"]
    outlined_clock = bool(git(root, 'ls-tree', commit, 'src/c/generated_clock_glyphs.h'))
    if font_media or outlined_clock:
        require(info.get("resources") == expected["resources"], "Timer font resource metadata differs")
        for notice in ("OFL-Fredoka.txt", "OFL-RobotoCondensed.txt"):
            require(files.get("licenses/" + notice) ==
                    committed(root, commit, "resources/fonts/" + notice),
                    "Timer font license differs: " + notice)
    platforms = expected["targetPlatforms"]
    require({name.split("/")[0] for name in files if name.endswith("/manifest.json")} == set(platforms),
            "Timer platform set differs")
    for platform in platforms:
        manifest = json.loads(files[platform + "/manifest.json"])
        require(manifest.get("manifestVersion") == 2 and manifest.get("type") == "application", "Invalid timer native manifest")
        for field, name in (("application", "pebble-app.bin"), ("resources", "app_resources.pbpack")):
            metadata = manifest[field]
            binary = files.get(platform + "/" + name, b"")
            require(metadata.get("name") == name and len(binary) >= 128 and
                    metadata.get("size") == len(binary) and metadata.get("crc") == pebble_crc(binary),
                    "Timer native integrity differs: " + name)
            if field == "application":
                require(binary[:8] == b"PBLAPP\0\0" and binary[8:10] == bytes((16, 0)) and
                        binary[104:120] == uuid.UUID(expected["uuid"]).bytes and
                        list(binary[12:14]) == [int(value) for value in package["version"].split(".")[:2]],
                        "Timer native identity/version differs")
    validate_timer_pkjs(root, commit, "", source_files, dependency)
    with tempfile.TemporaryDirectory(prefix="timer-release-plugin-") as directory:
        checkout = Path(directory)
        inputs = ["scripts/bundle-js.py", "scripts/verify-timer-bundle.js", "src/plugin/index.js"]
        if expected.get("configPage"):
            inputs.append("src/pkjs/settings.js")
        for name in inputs:
            destination = checkout / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(committed(root, commit, name))
        for name, content in dependency.items():
            destination = checkout / "node_modules/pebble-activities" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(content)
        output = checkout / "provider.js"
        result = subprocess.run(["python3", str(checkout / "scripts/bundle-js.py"),
                                 str(checkout / "src/plugin/index.js"), str(output)], capture_output=True)
        require(result.returncode == 0 and output.is_file(), "Cannot rebuild committed timer plugin")
        require(source_files["plugin.js"] == output.read_bytes(), "Timer plugin/source differs")
        extracted = checkout / "pbw"
        extracted.mkdir()
        for name, content in files.items():
            destination = extracted / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(content)
        result = subprocess.run(["node", str(checkout / "scripts/verify-timer-bundle.js"), str(extracted)], capture_output=True)
        require(result.returncode == 0, "Timer bundled-runtime handoff failed")


def validate_artifacts(root, config, commit, package, assets):
    kind = config["kind"]
    if kind in ("core", "activities"):
        packed = [data for path, data in assets.items() if path.endswith(".tgz")]
        require(len(packed) == 1 and "dist.zip" in assets, "Expected one Core tarball and dist.zip")
        with tarfile.open(fileobj=io.BytesIO(packed[0]), mode="r:gz") as archive:
            members = archive.getmembers()
            require(all(member.isfile() and member.name.startswith("package/") for member in members),
                    "Unsafe Core tarball member")
            require(len(members) == len({member.name for member in members}), "Duplicate Core member")
            files = {safe_path(member.name[8:]): archive.extractfile(member).read() for member in members}
        require(files.get("dist.zip") == assets["dist.zip"], "Core tarball contains another SDK package")
        require(json.loads(files["package.json"])["version"] == package["version"], "Core tar version differs")
        for path, content in files.items():
            if critical(path):
                require(content == committed(root, commit, path), "Stale Core tar input: " + path)
            if kind == "activities":
                require(content == committed(root, commit, path), "Stale Activities package input: " + path)
        names = git(root, "ls-tree", "-r", "--name-only", "-z", commit).decode().split("\0")
        for path in names:
            if path.startswith(("src/", "include/")):
                require(path in files, "Core tarball omits source: " + path)
        sdk = zip_files(assets["dist.zip"])
        for path, content in files.items():
            if path.startswith("src/js/"):
                require(sdk.get("js/" + path[7:]) == content, "Core SDK JavaScript differs: " + path)
            elif path.startswith("include/") and path.endswith(".h"):
                require(sdk.get("include/" + package["name"] + "/" + path[8:]) == content,
                        "Core SDK exported header differs: " + path)
        return
    source_config = config["source_archive"]
    source_path = expand(source_config["path"], package)
    source = validate_source(root, commit, assets[source_path], source_config.get("prefix", ""))
    require(json.loads(source["package.json"]) == package, "Source package metadata differs")
    pbws = [(path, data) for path, data in assets.items() if path.endswith(".pbw")]
    require(len(pbws) == 1, "Expected exactly one public PBW")
    pbw_path, pbw_data = pbws[0]
    pbw = zip_files(pbw_data)
    info = json.loads(pbw["appinfo.json"])
    require(info.get("uuid") == config["app_uuid"] == package["pebble"]["uuid"] and
            info.get("versionLabel") == package["version"], "PBW identity/version differs")
    if kind == "timer":
        debug = None
        if config.get('debug_archive'):
            debug = zip_files(assets[expand(config['debug_archive'], package)])
            validate_timer_optimization(root, commit, package, pbw, debug)
        validate_timer(root, config, commit, package, pbw, debug)
    elif kind == "chmu":
        require(info.get("plugin", {}).get("script") == "plugin.js" and
                pbw["plugin.js"].startswith(b"// CHMU_BUILD_VARIANT=public\n"), "Not a public CHMU build")
        require(b"chmu.rails.cz" not in pbw["plugin.js"] and
                "chmu.rails.cz" not in json.dumps(info), "Personal CHMU backend in public PBW")
        require(b'data-build-variant="public"' in pbw["config.html"] and
                pbw["config.html"] == source["src/config.html"].replace(b"__CHMU_BUILD_VARIANT__", b"public"),
                "CHMU configuration is not the public source build")
    else:
        require(info.get("watchapp", {}).get("watchface") is True, "Not a watchface PBW")
        mapping = json.loads(pbw["pebble-js-app.js.map"])
        require(len(mapping["sources"]) == len(mapping["sourcesContent"]), "Invalid PBW source map")
        mapped = 0
        for path, text in zip(mapping["sources"], mapping["sourcesContent"]):
            if path.startswith("./src/"):
                original = source[path[2:]].decode()
                tail = text[len(original):]
                require(text.startswith(original) and (not tail or re.fullmatch(
                    r"\n{2,}//////////////////\n// WEBPACK FOOTER(?:\n//[^\n]*)*\s*", tail)),
                    "PBW JavaScript/source differs: " + path)
                mapped += 1
        require(mapped > 0, "PBW has no application source mapping")
    for path, content in assets.items():
        if path.endswith(("-store-package.zip", "-store-release.zip")):
            store = zip_files(content)
            embedded = [name for name in store if name.endswith(".pbw")]
            require(embedded == [PurePosixPath(pbw_path).name] and store[embedded[0]] == pbw_data,
                    "Store archive embeds a different or additional PBW")


def validate_dependencies(root, commit, package):
    """Check every local dependency against its lock and provenance record."""
    import base64
    requests = {name: value for name, value in package.get('dependencies', {}).items()
                if value.startswith('file:')}
    if not requests:
        return
    lock = json.loads(committed(root, commit, 'package-lock.json'))
    names = git(root, 'ls-tree', '-r', '--name-only', '-z', commit).decode().split('\0')
    pins = [json.loads(committed(root, commit, name)) for name in names
            if name.startswith('vendor/') and name.endswith('.json')]
    for name, request in requests.items():
        path = safe_path(request[5:])
        require(path.startswith('vendor/'), 'Dependencies must be self-contained vendor archives')
        content = committed(root, commit, path)
        record = lock['packages']['node_modules/' + name]
        integrity = 'sha512-' + base64.b64encode(hashlib.sha512(content).digest()).decode()
        require(record['resolved'] == request and record['integrity'] == integrity,
                'Dependency lock differs: ' + name)
        with tarfile.open(fileobj=io.BytesIO(content), mode='r:gz') as archive:
            metadata = json.load(archive.extractfile('package/package.json'))
        require(metadata['name'] == name and metadata['version'] == record['version'],
                'Dependency package identity differs: ' + name)
        matching = [pin for pin in pins if pin.get('package') == name]
        require(len(matching) == 1, 'Missing or ambiguous dependency provenance: ' + name)
        pin = matching[0]
        require(pin.get('archive', pin.get('artifact')) == PurePosixPath(path).name and
                pin['sha256'] == digest(content) and pin['version'] == metadata['version'] and
                pin.get('integrity', integrity) == integrity,
                'Dependency provenance differs: ' + name)


def require_manual_workflow():
    repository = os.getenv('GITHUB_REPOSITORY', '')
    require(os.getenv('GITHUB_ACTIONS') == 'true' and
            os.getenv('GITHUB_EVENT_NAME') == 'workflow_dispatch' and
            os.getenv('GITHUB_REF') == 'refs/heads/main' and
            os.getenv('GITHUB_WORKFLOW_REF') == repository +
                '/.github/workflows/release.yml@refs/heads/main',
            'Publication requires the manually dispatched release.yml workflow on main')


def validate_prepared(root, config_path, folder):
    """Recompute provenance before publication; a manifest is not trusted input."""
    manifest = json.loads((folder / 'release-manifest.json').read_text())
    require(manifest['trigger_commit'] == git(root, 'rev-parse', 'HEAD').decode().strip(),
            'Prepared release belongs to another checkout commit')
    with tempfile.TemporaryDirectory(prefix='pebble-release-audit-') as scratch:
        expected_folder = Path(scratch)
        expected = prepare(root, config_path, expected_folder, manifest['trigger_commit'])
        require(manifest == expected, 'Prepared manifest differs from committed release inputs')
        require({path.name for path in folder.iterdir()} ==
                {path.name for path in expected_folder.iterdir()}, 'Prepared inventory differs')
        for path in expected_folder.iterdir():
            require((folder / path.name).read_bytes() == path.read_bytes(),
                    'Prepared asset changed: ' + path.name)


def prepare(root, config_path, output, trigger_commit=None):
    config = json.loads(config_path.read_text())
    require(config["kind"] in ("watchface", "core", "activities", "chmu", "timer"), "Unsupported release kind")
    require(re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", config["repository"]), "Invalid repository")
    require(type(config["prerelease"]) is bool and config["display_name"], "Invalid release metadata")
    trigger = trigger_commit or git(root, "rev-parse", "HEAD").decode().strip()
    require(SHA.fullmatch(trigger), "Release commit must be a full SHA")
    package = json.loads(committed(root, trigger, "package.json"))
    identity = release_id(package)
    require(config["prerelease"] or not re.search(r"-(?:rc|beta)\.", identity),
            "Release candidates and betas must be marked prerelease")
    tag = config.get("tag_prefix", "v") + identity
    tagged = subprocess.run(["git", "-C", str(root), "rev-parse", "--verify",
                             "refs/tags/" + tag + "^{commit}"], capture_output=True)
    bootstrap = config.get("bootstrap", {})
    commit = (tagged.stdout.decode().strip() if tagged.returncode == 0 else
              bootstrap.get("source_commit") if bootstrap.get("version") == identity else trigger)
    require(isinstance(commit, str) and SHA.fullmatch(commit), "Invalid source commit")
    require(subprocess.run(["git", "-C", str(root), "merge-base", "--is-ancestor", commit, trigger],
                           capture_output=True).returncode == 0, "Release tag/source commit is not an ancestor")
    original_package = json.loads(committed(root, commit, "package.json"))
    require(release_id(original_package) == identity, "Bootstrap package version differs")
    paths = [expand(path, package) for path in config["assets"]]
    require(paths and len(paths) <= 8 and len(paths) == len(set(paths)) and
            len({PurePosixPath(path).name for path in paths}) == len(paths), "Duplicate release assets")
    require(all(re.fullmatch(r"[A-Za-z0-9_.-]+", PurePosixPath(path).name) for path in paths),
            "Unsafe asset basename")
    checksums_path = expand(config["checksums_file"], package)
    expected = checksum_map(committed(root, commit, checksums_path), paths, checksums_path)
    current = checksum_map(committed(root, trigger, checksums_path), paths, checksums_path)
    assets = {}
    for path in paths:
        content = committed(root, commit, path)
        require(content == committed(root, trigger, path),
                "An existing version's asset changed; restore it or bump the version: " + path)
        require(digest(content) == expected[path] == current[path], "Artifact checksum differs: " + path)
        assets[path] = content
    validate_dependencies(root, commit, original_package)
    validate_artifacts(root, config, commit, original_package, assets)
    tag = config.get("tag_prefix", "v") + identity
    require(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", tag), "Unsafe release tag")
    note_config = config["notes"]
    notes = committed(root, commit, expand(note_config["path"], package)).decode().strip()
    if note_config.get("heading"):
        heading = note_config["heading"].format(version=package["version"], release_id=identity)
        match = re.search(r"^" + re.escape(heading) + r"\s*\n(.*?)(?=^##\s|\Z)", notes, re.M | re.S)
        require(match, "Release notes heading is missing")
        notes = match.group(1).strip()
    require(notes, "Release notes are empty")
    if output.exists():
        allowed = {PurePosixPath(path).name for path in paths} | {
            "SHA256SUMS.txt", "release-notes.md", "release-manifest.json"}
        require(all(path.is_file() and path.name in allowed for path in output.iterdir()),
                "Unexpected file in release output; use an empty output directory")
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for path, content in assets.items():
        name = PurePosixPath(path).name
        (output / name).write_bytes(content)
        records.append({"name": name, "repository_path": path, "sha256": digest(content), "bytes": len(content)})
    checksum_bytes = "".join(item["sha256"] + "  " + item["name"] + "\n" for item in records).encode()
    require("SHA256SUMS.txt" not in [item["name"] for item in records], "Reserved checksum asset name")
    (output / "SHA256SUMS.txt").write_bytes(checksum_bytes)
    records.append({"name": "SHA256SUMS.txt", "sha256": digest(checksum_bytes), "bytes": len(checksum_bytes)})
    notes += "\n\nSource: https://github.com/" + config["repository"] + "/tree/" + commit
    notes += "\n\nDownload the files below together with SHA256SUMS.txt to verify their integrity.\n"
    (output / "release-notes.md").write_text(notes)
    manifest = {"repository": config["repository"], "display_name": config["display_name"],
                "version": package["version"], "release_id": identity, "tag": tag,
                "prerelease": config["prerelease"], "source_commit": commit,
                "trigger_commit": trigger, "assets": records}
    (output / "release-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


class GitHub:
    def __init__(self, repository):
        self.repository = repository

    def api(self, method, path, data=None, missing=False):
        command = ["gh", "api", "repos/" + self.repository + "/" + path, "--method", method]
        if data is not None:
            command += ["--input", "-"]
        result = subprocess.run(command, input=json.dumps(data).encode() if data is not None else None,
                                capture_output=True)
        status = re.search(rb"\bHTTP\s*([0-9]{3})\b", result.stderr)
        if missing and result.returncode and status and status.group(1) == b"404":
            return None
        detail = " (HTTP " + status.group(1).decode() + ")" if status else ""
        hint = next((line for line in result.stderr.decode(errors="replace").splitlines()
                     if line.startswith("gh:")), "")
        for key in ("GH_TOKEN", "GITHUB_TOKEN"):
            if os.getenv(key):
                hint = hint.replace(os.environ[key], "[redacted]")
        require(result.returncode == 0, "GitHub request failed" + detail + ": " + method + " " + path +
                ("; " + hint[:300] if hint else ""))
        return json.loads(result.stdout) if result.stdout.strip() else None

    def release(self, tag):
        return self.api("GET", "releases/tags/" + tag, missing=True)

    def tag_commit(self, tag):
        ref = self.api("GET", "git/ref/tags/" + tag, missing=True)
        if ref is None:
            return None
        obj = ref["object"]
        for _ in range(10):
            if obj["type"] == "commit":
                return obj["sha"]
            require(obj["type"] == "tag", "Tag does not resolve to a commit")
            obj = self.api("GET", "git/tags/" + obj["sha"])["object"]
        raise ValueError("Annotated tag chain is too long")

    def create_tag(self, tag, commit):
        self.api("POST", "git/refs", {"ref": "refs/tags/" + tag, "sha": commit})

    def create_release(self, manifest, notes):
        return self.api("POST", "releases", {"tag_name": manifest["tag"],
            "target_commitish": manifest["source_commit"], "draft": True,
            "prerelease": manifest["prerelease"], "name": manifest["display_name"] + " " + manifest["release_id"],
            "body": notes})

    def assets(self, release):
        return self.api("GET", "releases/" + str(release["id"]) + "/assets?per_page=100")

    def verify_asset(self, release, asset, expected):
        require(asset["size"] == expected["bytes"], "Published asset size differs: " + expected["name"])
        actual_digest = asset.get("digest")
        if actual_digest:
            require(actual_digest == "sha256:" + expected["sha256"],
                    "Published asset hash differs: " + expected["name"])
        else:
            with tempfile.TemporaryDirectory(prefix="verify-gh-release-") as folder:
                result = subprocess.run(["gh", "release", "download", release["tag_name"], "--repo",
                    self.repository, "--pattern", expected["name"], "--dir", folder], capture_output=True)
                require(result.returncode == 0, "Cannot verify legacy release asset")
                require(digest((Path(folder) / expected["name"]).read_bytes()) == expected["sha256"],
                        "Published asset hash differs: " + expected["name"])

    def upload(self, tag, path):
        result = subprocess.run(["gh", "release", "upload", tag, str(path), "--repo", self.repository],
                                capture_output=True)
        require(result.returncode == 0, "Cannot upload release asset: " + path.name)

    def publish(self, release, prerelease):
        return self.api("PATCH", "releases/" + str(release["id"]),
                        {"draft": False, "prerelease": prerelease,
                         "make_latest": "false" if prerelease else "legacy"})


def publish(root, folder, github=None):
    if github is None:
        require_manual_workflow()
        validate_prepared(root, root / ".github/release.json", folder)
    manifest = json.loads((folder / "release-manifest.json").read_text())
    require(re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", manifest["repository"]), "Invalid repository")
    require(not os.getenv("GITHUB_REPOSITORY") or os.environ["GITHUB_REPOSITORY"] == manifest["repository"],
            "Release repository differs from this workflow")
    require(SHA.fullmatch(manifest["source_commit"]) and
            re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", manifest["tag"]), "Invalid release target")
    require(type(manifest["prerelease"]) is bool and (manifest["prerelease"] or
            not re.search(r"-(?:rc|beta)\.", manifest["release_id"])), "Invalid prerelease policy")
    expected = {}
    for item in manifest["assets"]:
        name = safe_path(item["name"])
        require(PurePosixPath(name).name == name and name not in expected, "Duplicate/unsafe release filename")
        content = (folder / name).read_bytes()
        require(len(content) == item["bytes"] and digest(content) == item["sha256"],
                "Prepared asset changed: " + name)
        expected[name] = item
    github = github or GitHub(manifest["repository"])
    release = github.release(manifest["tag"])
    tag_commit = github.tag_commit(manifest["tag"])
    if release and not release["draft"]:
        require(tag_commit == manifest["source_commit"], "Published tag points elsewhere; tags are never moved")
        require(release["prerelease"] == manifest["prerelease"], "Published release prerelease flag differs")
        package = json.loads(committed(root, tag_commit, "package.json"))
        require(release_id(package) == manifest["release_id"], "Published tag has another package version")
        for item in manifest["assets"]:
            if "repository_path" in item:
                require(digest(committed(root, tag_commit, item["repository_path"])) == item["sha256"],
                        "Published tag contains a different artifact")
    else:
        require(tag_commit is None or tag_commit == manifest["source_commit"],
                "Existing tag points elsewhere; tags are never moved")
    existing = github.assets(release) if release else []
    by_name = {item["name"]: item for item in existing}
    require(len(by_name) == len(existing) and set(by_name) <= set(expected), "Unexpected existing release assets")
    for name, item in by_name.items():
        github.verify_asset(release, item, expected[name])
    if release and not release["draft"]:
        require(set(by_name) == set(expected), "Published release is incomplete; assets will not be replaced")
        return "unchanged"
    if tag_commit is None:
        github.create_tag(manifest["tag"], manifest["source_commit"])
    if release is None:
        release = github.create_release(manifest, (folder / "release-notes.md").read_text())
    for name in expected:
        if name not in by_name:
            github.upload(manifest["tag"], folder / name)
    uploaded = github.assets(release)
    require(len(uploaded) == len(expected) and {item["name"] for item in uploaded} == set(expected),
            "Release asset upload is incomplete")
    for item in uploaded:
        github.verify_asset(release, item, expected[item["name"]])
    github.publish(release, manifest["prerelease"])
    return "published"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("prepare", "publish"))
    parser.add_argument("--config", type=Path, default=ROOT / ".github/release.json")
    parser.add_argument("--output", type=Path, default=ROOT / "build/release")
    parser.add_argument("--commit", help="Full triggering commit SHA; defaults to HEAD")
    args = parser.parse_args()
    if args.command == "prepare":
        manifest = prepare(ROOT, args.config, args.output, args.commit)
        if os.getenv("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a") as handle:
                handle.write("release_id=" + manifest["release_id"] + "\n")
        print("Prepared " + manifest["tag"] + " from " + manifest["source_commit"])
    else:
        require(args.config.resolve() == (ROOT / ".github/release.json").resolve(),
                "Publication must use the repository release configuration")
        require_manual_workflow()
        print("GitHub release: " + publish(ROOT, args.output))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, zipfile.BadZipFile, tarfile.TarError) as error:
        raise SystemExit("Release failed: " + str(error))
