"""Standalone Timer provenance and packaging regressions; no SDK/network."""
import base64
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import uuid
import zipfile

import test_github_release as generic

release = generic.release
zip_bytes = generic.zip_bytes
REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("package_release", REPO / "scripts/package-release.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class TimerReleaseTests(unittest.TestCase):
    run_git = generic.ReleaseTests.run_git
    write = generic.ReleaseTests.write
    commit = generic.ReleaseTests.commit

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.run_git("init", "-b", "main")
        self.run_git("config", "user.name", "Release Test")
        self.run_git("config", "user.email", "test@example.invalid")
        self.app_uuid = "50e9ef12-a5ab-4369-a3b5-3447bfba5151"
        self.dependency_path = "vendor/pebble-activities-0.1.0.tgz"
        self.package = {"name": "pebble-app-timer", "version": "1.2.3",
            "dependencies": {"pebble-activities": "file:" + self.dependency_path},
            "pebble": {"uuid": self.app_uuid, "displayName": "Timer",
                "plugin": {"script": "plugin.js", "sources": [{"category": "activity", "items": ["session"],
                    "properties": {"unconfirmed": ["boolean"]}}]},
                "targetPlatforms": ["emery"], "usesPermissions": [], "messageKeys": {"ACTIVITY_RUN": 1001}}}
        self.dependency = {"package.json": json.dumps({"name": "pebble-activities", "version": "0.1.0"}).encode(),
            "LICENSE": b"fixture Activities license", "src/js/wire.js": b"module.exports={version:1};\n",
            "src/js/publisher.js": b"var wire=require('./wire');\nmodule.exports=wire;\n",
            "publisher.js": b"module.exports=require('./src/js/publisher');\n"}
        self.dependency["dist.zip"] = zip_bytes({
            "js/publisher.js": self.dependency["src/js/publisher.js"],
            "js/wire.js": self.dependency["src/js/wire.js"]})
        packed = generic.tar_bytes(self.dependency)
        self.write(self.dependency_path, packed)
        self.lock = {"packages": {"node_modules/pebble-activities": {"version": "0.1.0",
            "resolved": "file:" + self.dependency_path,
            "integrity": "sha512-" + base64.b64encode(hashlib.sha512(packed).digest()).decode()}}}
        self.write("package-lock.json", json.dumps(self.lock).encode())
        for path, content in self.dependency.items():
            self.write("node_modules/pebble-activities/" + path, content)
        pkjs = "var publisher=require('pebble-activities/publisher');\n"
        self.write("src/pkjs/index.js", pkjs.encode())
        self.write("src/plugin/index.js", b"var provider=require('pebble-activities/publisher');\n")
        self.write("LICENSE", b"fixture license")
        self.write("THIRD-PARTY-NOTICES.md", b"fixture SDK notice")
        self.write("licenses/Apache-2.0.txt", b"fixture Apache license")
        self.write("package.json", json.dumps(self.package).encode())
        self.write(".gitignore", b"node_modules/\nbuild/\n")
        self.write("scripts/bundle-js.py", (REPO / "scripts/bundle-js.py").read_bytes())
        self.write("scripts/verify-timer-bundle.js", b"require('fs').accessSync(process.argv[2]+'/plugin.js');\n")
        plugin = self.root / "build/plugin.js"
        subprocess.run(["python3", str(self.root / "scripts/bundle-js.py"),
            str(self.root / "src/plugin/index.js"), str(plugin)], check=True)
        native = bytearray(128)
        native[:8] = b"PBLAPP\0\0"
        native[8:10] = bytes((16, 0))
        native[12:14] = bytes((1, 2))
        native[104:120] = uuid.UUID(self.app_uuid).bytes
        resource = bytes(range(128))
        manifest = {"manifestVersion": 2, "type": "application",
            "application": {"name": "pebble-app.bin", "size": len(native), "crc": release.pebble_crc(native)},
            "resources": {"name": "app_resources.pbpack", "size": len(resource), "crc": release.pebble_crc(resource)}}
        paths = ["_pkjs_shared_additions.js", "./src/pkjs/index.js",
            "./~/pebble-activities/dist/js/publisher.js", "./~/pebble-activities/dist/js/wire.js"]
        texts = ["var sdk=true;\n", pkjs, self.dependency["src/js/publisher.js"].decode(),
            self.dependency["src/js/wire.js"].decode()]
        mapped, modules = [], ["/* 0 */\n/***/ (function(module, exports, __webpack_require__) {\n\n"
            "\t__webpack_require__(1);\n\tmodule.exports = __webpack_require__(2);\n\n/***/ })"]
        for number, text in enumerate(texts, 1):
            mapped.append(text + "\n\n//////////////////\n// WEBPACK FOOTER\n// module id = " + str(number) + "\n")
            compiled = text.replace("require('pebble-activities/publisher')", "__webpack_require__(3)")
            compiled = compiled.replace("require('./wire')", "__webpack_require__(4)")
            modules.append("/* %d */\n/***/ (function(module, exports, __webpack_require__) {\n\n" % number +
                "\n".join("\t" + line for line in compiled.rstrip().splitlines()) + "\n\n/***/ })")
        info = {**self.package["pebble"], "versionLabel": "1.2.3", "manifestVersion": 2,
            "shortName": "Timer", "longName": "Timer", "watchapp": {"watchface": False},
            "projectType": "native", "appKeys": {"ACTIVITY_RUN": 1001}}
        self.pbw = {"appinfo.json": json.dumps(info).encode(), "LICENSE": b"fixture license",
            "licenses/pebble-activities-LICENSE.txt": b"fixture Activities license",
            "THIRD-PARTY-NOTICES.md": b"fixture SDK notice", "licenses/Apache-2.0.txt": b"fixture Apache license",
            "plugin.js": plugin.read_bytes(), "pebble-js-app.js": ",\n".join(modules).encode(),
            "pebble-js-app.js.map": json.dumps({"sources": paths, "sourcesContent": mapped}).encode(),
            "emery/pebble-app.bin": bytes(native), "emery/app_resources.pbpack": resource,
            "emery/manifest.json": json.dumps(manifest).encode()}
        self.config = {"app_uuid": self.app_uuid}
        self.commit_sha = self.commit()

    def validate(self, pbw=None):
        release.validate_timer(self.root, self.config, self.commit_sha, self.package, pbw or self.pbw)

    def mutate_json(self, name, mutate):
        files = dict(self.pbw)
        value = json.loads(files[name])
        mutate(value)
        files[name] = json.dumps(value).encode()
        return files

    def test_app_and_library_versions_are_independent_and_bundles_validate(self):
        self.assertNotEqual(self.package["version"], self.lock["packages"]["node_modules/pebble-activities"]["version"])
        self.assertIn("dist.zip", self.dependency)
        self.assertFalse(any(name.startswith("dist/") for name in self.dependency))
        self.validate()

    def test_full_version_uuid_names_and_plugin_manifest_cannot_drift(self):
        for field, value in [("versionLabel", "1.2.4"), ("uuid", "0" * 36), ("shortName", "Other"),
                ("manifestVersion", 1), ("plugin", {"script": "missing.js"}), ("appKeys", {})]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.validate(self.mutate_json("appinfo.json", lambda info: info.update({field: value})))

    def test_license_and_plugin_bytes_must_match_committed_sources(self):
        for name in ["LICENSE", "THIRD-PARTY-NOTICES.md", "licenses/Apache-2.0.txt",
                "licenses/pebble-activities-LICENSE.txt", "plugin.js"]:
            with self.subTest(member=name), self.assertRaises(ValueError):
                self.validate({**self.pbw, name: b"stale"})

    def with_custom_fonts(self):
        resources = {"media": [{"type": "font", "name": "FONT_BODY_16",
                                "file": "fonts/RobotoCondensed-Bold.ttf"}]}
        self.package["pebble"]["resources"] = resources
        self.write("package.json", json.dumps(self.package).encode())
        files = dict(self.pbw)
        info = json.loads(files["appinfo.json"])
        info["resources"] = resources
        files["appinfo.json"] = json.dumps(info).encode()
        for notice in ("OFL-Fredoka.txt", "OFL-RobotoCondensed.txt"):
            content = ("fixture original " + notice).encode()
            self.write("resources/fonts/" + notice, content)
            files["licenses/" + notice] = content
        self.commit_sha = self.commit()
        return files

    def test_custom_font_notices_are_mandatory_and_exact_when_fonts_are_declared(self):
        files = self.with_custom_fonts()
        self.validate(files)
        for name in ("licenses/OFL-Fredoka.txt", "licenses/OFL-RobotoCondensed.txt"):
            for missing in (False, True):
                with self.subTest(name=name, missing=missing), self.assertRaisesRegex(ValueError, "font license"):
                    changed = dict(files)
                    if missing:
                        del changed[name]
                    else:
                        changed[name] = b"stale font license"
                    self.validate(changed)

    def test_custom_font_manifest_cannot_drift_from_committed_resource_definitions(self):
        files = self.with_custom_fonts()
        info = json.loads(files["appinfo.json"])
        info["resources"]["media"][0]["name"] = "FONT_BODY_24"
        with self.assertRaisesRegex(ValueError, "font resource metadata"):
            self.validate({**files, "appinfo.json": json.dumps(info).encode()})

    def test_outline_font_notices_remain_mandatory_without_bitmap_resources(self):
        files = self.with_custom_fonts()
        self.package["pebble"]["resources"] = {"media": []}
        self.write("package.json", json.dumps(self.package).encode())
        self.write("src/c/generated_clock_glyphs.h", b"fixture linked outline input")
        info = json.loads(files["appinfo.json"])
        info["resources"] = {"media": []}
        files["appinfo.json"] = json.dumps(info).encode()
        self.commit_sha = self.commit()
        self.validate(files)
        for name in ("licenses/OFL-Fredoka.txt", "licenses/OFL-RobotoCondensed.txt"):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "font license"):
                self.validate({key: value for key, value in files.items() if key != name})

    def test_pkjs_map_and_executed_module_both_must_match(self):
        # Workspace-local SDK installs emit a relative source path. Normalizing
        # that SDK module name retains duplicate and compiled-code checks.
        mapping = json.loads(self.pbw["pebble-js-app.js.map"])
        mapping["sources"][0] = "../sdk/pebble/common/include/_pkjs_shared_additions.js"
        self.validate({**self.pbw, "pebble-js-app.js.map": json.dumps(mapping).encode()})
        mapping["sources"].append("_pkjs_shared_additions.js")
        mapping["sourcesContent"].append(mapping["sourcesContent"][0])
        with self.assertRaisesRegex(ValueError, "Duplicate timer PKJS source"):
            self.validate({**self.pbw, "pebble-js-app.js.map": json.dumps(mapping).encode()})
        for name in ["pebble-js-app.js", "pebble-js-app.js.map"]:
            with self.subTest(member=name), self.assertRaises(ValueError):
                self.validate({**self.pbw, name: self.pbw[name].replace(b"version:1", b"version:2")})

    def test_native_binary_and_resource_integrity_are_checked(self):
        for name in ["emery/pebble-app.bin", "emery/app_resources.pbpack"]:
            with self.subTest(member=name), self.assertRaisesRegex(ValueError, "native integrity"):
                self.validate({**self.pbw, name: self.pbw[name][:-1] + b"x"})

    def test_act2_view_module_is_required_and_matches_source_and_compiled_bytes(self):
        dependency = {**self.dependency,
            "src/js/publisher.js": b"var wire=require('./wire');\nvar view=require('./view');\nmodule.exports=view;\n",
            "src/js/view.js": b"var wire=require('./wire');\nmodule.exports={wire:wire,cardVersion:2};\n"}
        mapping = json.loads(self.pbw["pebble-js-app.js.map"])
        mapping["sources"].append("./~/pebble-activities/dist/js/view.js")
        texts = ["var sdk=true;\n", (self.root / "src/pkjs/index.js").read_text(),
                 dependency["src/js/publisher.js"].decode(),
                 dependency["src/js/wire.js"].decode(), dependency["src/js/view.js"].decode()]
        mapping["sourcesContent"] = []
        modules = ["/* 0 */\n/***/ (function(module, exports, __webpack_require__) {\n\n"
                   "\t__webpack_require__(1);\n\tmodule.exports = __webpack_require__(2);\n\n/***/ })"]
        for number, text in enumerate(texts, 1):
            mapping["sourcesContent"].append(text +
                "\n\n//////////////////\n// WEBPACK FOOTER\n// module id = " + str(number) + "\n")
            compiled = text.replace("require('pebble-activities/publisher')", "__webpack_require__(3)")
            compiled = compiled.replace("require('./wire')", "__webpack_require__(4)")
            compiled = compiled.replace("require('./view')", "__webpack_require__(5)")
            modules.append("/* %d */\n/***/ (function(module, exports, __webpack_require__) {\n\n" % number +
                "\n".join("\t" + line for line in compiled.rstrip().splitlines()) + "\n\n/***/ })")
        files = {**self.pbw, "pebble-js-app.js": ",\n".join(modules).encode(),
                 "pebble-js-app.js.map": json.dumps(mapping).encode()}
        release.validate_timer_pkjs(self.root, self.commit_sha, "", files, dependency)
        for name in ("pebble-js-app.js", "pebble-js-app.js.map"):
            with self.subTest(member=name), self.assertRaises(ValueError):
                release.validate_timer_pkjs(self.root, self.commit_sha, "",
                    {**files, name: files[name].replace(b"cardVersion:2", b"cardVersion:3")}, dependency)
        mapping["sources"].pop()
        mapping["sourcesContent"].pop()
        with self.assertRaisesRegex(ValueError, "omits required sources"):
            release.validate_timer_pkjs(self.root, self.commit_sha, "",
                {**files, "pebble-js-app.js.map": json.dumps(mapping).encode()}, dependency)

    def test_crc_correct_native_wrong_uuid_version_or_header_is_rejected(self):
        for offset in [8, 12, 104]:
            with self.subTest(offset=offset):
                files = dict(self.pbw)
                native = bytearray(files["emery/pebble-app.bin"])
                native[offset] ^= 1
                files["emery/pebble-app.bin"] = bytes(native)
                manifest = json.loads(files["emery/manifest.json"])
                manifest["application"]["crc"] = release.pebble_crc(native)
                files["emery/manifest.json"] = json.dumps(manifest).encode()
                with self.assertRaisesRegex(ValueError, "native identity/version"):
                    self.validate(files)

    def test_missing_platform_or_native_manifest_is_rejected(self):
        files = dict(self.pbw)
        del files["emery/manifest.json"]
        with self.assertRaisesRegex(ValueError, "platform set"):
            self.validate(files)

    def test_bundled_runtime_failure_blocks_release(self):
        self.write("scripts/verify-timer-bundle.js", b"process.exit(7);\n")
        self.commit_sha = self.commit()
        with self.assertRaisesRegex(ValueError, "bundled-runtime handoff failed"):
            self.validate()

    def test_dependency_tarball_is_locked_and_registry_or_sibling_paths_fail(self):
        packed = (self.root / self.dependency_path).read_bytes()
        self.write(self.dependency_path, packed + b"unexpected bytes")
        self.commit_sha = self.commit()
        with self.assertRaisesRegex(ValueError, "dependency lock"):
            self.validate()
        for request in ("^0.1.0", "file:../pebble-activities", "file:vendor/../outside.tgz"):
            with self.subTest(request=request), self.assertRaises(ValueError):
                release.timer_dependency(self.root, self.commit_sha,
                    {**self.package, "dependencies": {"pebble-activities": request}})

    def test_dependency_archive_identity_must_match_lock(self):
        self.lock["packages"]["node_modules/pebble-activities"]["version"] = "0.1.1"
        self.write("package-lock.json", json.dumps(self.lock).encode())
        self.commit_sha = self.commit()
        with self.assertRaisesRegex(ValueError, "dependency identity"):
            self.validate()

    def test_pebble_crc_matches_published_sdk_vectors(self):
        self.assertEqual(release.pebble_crc(b"123 567 901 34"), 0x89f3bab2)
        self.assertEqual(release.pebble_crc(b"123456789"), 0xaff19057)
        self.assertEqual(release.pebble_crc(b"\xfe\xff\xfe\xff\x88"), 0x495e02ca)

    def test_packaging_freezes_pbw_and_self_contained_source_with_checksums(self):
        self.write("dist/Timer-1.2.3.pbw", zip_bytes(self.pbw))
        debug_bytes = zip_bytes({'optimization.json': b'fixture debug'})
        self.write("dist/Timer-1.2.3-debug.zip", debug_bytes)
        for name in ("build/private.js", "node_modules/other/index.js", ".pebble/cache.json",
                "random.PBW", "old.pyc", "__pycache__/old.pyc", "releases/old-source.zip"):
            self.write(name, b"excluded fixture")
        self.write("scripts/package-release.py", (REPO / "scripts/package-release.py").read_bytes())
        packager.package(self.root)
        timer = self.root / "releases/Timer-1.2.3.pbw"
        self.assertEqual(timer.read_bytes(), zip_bytes(self.pbw))
        self.assertEqual((self.root / 'releases/Timer-1.2.3-debug.zip').read_bytes(), debug_bytes)
        source = self.root / "releases/Timer-1.2.3-source.zip"
        with zipfile.ZipFile(source) as archive:
            names = set(archive.namelist())
            self.assertTrue({self.dependency_path, "package-lock.json", "src/pkjs/index.js",
                "src/plugin/index.js", "scripts/bundle-js.py", "scripts/package-release.py"} <= names)
            self.assertFalse(any(name.startswith(("dist/", "build/", "node_modules/", ".pebble/", "releases/")) or
                Path(name).suffix.lower() in (".pbw", ".pyc") for name in names))
        selected = release.checksum_map((self.root / "releases/SHA256SUMS.txt").read_bytes(),
            ["releases/Timer-1.2.3.pbw", "releases/Timer-1.2.3-source.zip", "releases/Timer-1.2.3-debug.zip"], "releases/SHA256SUMS.txt")
        self.assertEqual(selected["releases/Timer-1.2.3.pbw"], release.digest(timer.read_bytes()))
        self.assertEqual(selected["releases/Timer-1.2.3-source.zip"], release.digest(source.read_bytes()))
        self.assertEqual(selected['releases/Timer-1.2.3-debug.zip'], release.digest(debug_bytes))
        frozen = {path: path.read_bytes() for path in self.root.joinpath('releases').glob('Timer-1.2.3*')}
        with self.assertRaisesRegex(ValueError, 'Frozen Timer artifacts'):
            packager.package(self.root)
        for path, content in frozen.items():
            self.assertEqual(path.read_bytes(), content)

    def test_unbuilt_or_wrong_identity_pbw_does_not_touch_frozen_artifacts(self):
        self.write("releases/Timer-1.2.3.pbw", b"frozen")
        with self.assertRaises(FileNotFoundError):
            packager.package(self.root)
        self.write("dist/Timer-1.2.3.pbw", zip_bytes(self.mutate_json("appinfo.json", lambda info: info.update(versionLabel="1.2.4"))))
        with self.assertRaises(ValueError):
            packager.package(self.root)
        self.assertEqual((self.root / "releases/Timer-1.2.3.pbw").read_bytes(), b"frozen")


if __name__ == "__main__":
    unittest.main()
