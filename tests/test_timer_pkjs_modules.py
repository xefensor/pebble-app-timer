"""The release auditor verifies the pinned simple-API module graph exactly."""
import importlib.util
import json
from pathlib import Path
import re
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    "timer_release_modules", Path(__file__).resolve().parents[1] / "scripts/github-release.py")
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


class TimerModules(unittest.TestCase):
    def fixture(self, appearance=False):
        modules = {
            "./src/pkjs/index.js": "module.exports=require('pebble-activities/publisher');\n",
            "./~/pebble-activities/dist/js/publisher.js": "module.exports=require('./activity');\n",
            "./~/pebble-activities/dist/js/wire.js": "module.exports={};\n",
            "./~/pebble-activities/dist/js/view.js": "module.exports=require('./wire');\n",
            "./~/pebble-activities/dist/js/activity.js": "var wire=require('./wire');module.exports=require('./view');\n",
            "_pkjs_shared_additions.js": "module.exports={};\n",
        }
        if appearance:
            modules["./src/pkjs/index.js"] += "var settings=require('./settings');\n"
            modules["./src/pkjs/settings.js"] = "module.exports={theme:'light'};\n"
        numbers = {name: i + 1 for i, name in enumerate(modules)}
        targets = {"pebble-activities/publisher": "./~/pebble-activities/dist/js/publisher.js",
                   **{"./" + name: "./~/pebble-activities/dist/js/" + name + ".js"
                      for name in ("wire", "view", "activity")}}
        targets["./settings"] = "./src/pkjs/settings.js"
        bodies = {0: "__webpack_require__(%d);\n\tmodule.exports = __webpack_require__(1);" % numbers["_pkjs_shared_additions.js"]}
        for name, source in modules.items():
            bodies[numbers[name]] = re.sub(
                r"require\(['\"]([^'\"]+)['\"]\)",
                lambda match: "__webpack_require__(%d)" % numbers[targets[match[1]]], source)
        script = "\n".join("/* %d */\n/***/ (function(module, exports) {\n\n%s\n\n/***/ })" % item
                           for item in bodies.items())
        mapping = {"sources": list(modules), "sourcesContent": [
            source + "\n\n//////////////////\n// WEBPACK FOOTER\n// module id = %d\n" % numbers[name]
            for name, source in modules.items()]}
        dependency = {"src/js/" + name.rsplit("/", 1)[1]: source.encode()
                      for name, source in modules.items() if name.startswith("./~/")}
        return {"pebble-js-app.js": script.encode(), "pebble-js-app.js.map": json.dumps(mapping).encode()}, dependency, modules["./src/pkjs/index.js"]

    def validate(self, files, dependency, entry):
        def source(root, commit, name):
            return ("module.exports={theme:'light'};\n" if name == "src/pkjs/settings.js" else entry).encode()
        with patch.object(release, "committed", side_effect=source):
            release.validate_timer_pkjs(Path("."), "test", "", files, dependency)

    def test_pinned_simple_api_module_and_imports_are_accepted(self):
        self.validate(*self.fixture())

    def test_activity_module_is_rejected_when_absent_from_the_pin(self):
        files, dependency, entry = self.fixture()
        del dependency["src/js/activity.js"]
        with self.assertRaisesRegex(ValueError, "Unexpected timer PKJS module"):
            self.validate(files, dependency, entry)

    def test_modified_compiled_simple_api_is_rejected(self):
        files, dependency, entry = self.fixture()
        files["pebble-js-app.js"] = files["pebble-js-app.js"].replace(b"var wire=", b"var altered=")
        with self.assertRaisesRegex(ValueError, "compiled code differs"):
            self.validate(files, dependency, entry)

    def test_local_appearance_module_and_its_import_are_checked(self):
        self.validate(*self.fixture(appearance=True))

    def test_modified_compiled_appearance_module_is_rejected(self):
        files, dependency, entry = self.fixture(appearance=True)
        files["pebble-js-app.js"] = files["pebble-js-app.js"].replace(b"theme:'light'", b"theme:'dark'")
        with self.assertRaisesRegex(ValueError, "compiled code differs"):
            self.validate(files, dependency, entry)
