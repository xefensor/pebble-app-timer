"""Optimized scripts must reproduce exact code and maps from original inputs."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('timer_release_optimization', ROOT / 'scripts/github-release.py')
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


class Optimization(unittest.TestCase):
    def setUp(self):
        self.files, self.debug = {}, {}
        for name in ('pebble-js-app.js', 'plugin.js'):
            source = '/* Copyright test */\nfunction sample(value) { var original = value + 1; return original; }\nsample(2);\n'
            self.debug[name] = source.encode()
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory)
                (path / 'input.js').write_text(source)
                command = ['node', str(ROOT / 'scripts/minify-js.cjs'), str(path / 'input.js'),
                           str(path / name), str(path / (name + '.map'))]
                if name == 'pebble-js-app.js':
                    mapping = json.dumps({'version': 3, 'sources': ['src/pkjs/index.js'],
                        'sourcesContent': [source], 'names': [], 'mappings': ''}).encode()
                    self.debug[name + '.map'] = mapping
                    (path / 'input.map').write_bytes(mapping)
                    command.append(str(path / 'input.map'))
                subprocess.run(command, check=True, capture_output=True)
                self.files[name] = (path / name).read_bytes()
                self.debug['minified/' + name + '.map'] = (path / (name + '.map')).read_bytes()
        self.package = {'version': '0.10.0'}
        self.metadata()

    def metadata(self):
        self.debug['optimization.json'] = json.dumps({'schema': 1, 'version': '0.10.0',
            'minifier': 'terser@5.46.0', 'scripts': {name: {
                'sourceSha256': release.digest(self.debug[name]),
                'releaseSha256': release.digest(self.files[name]),
                'mapSha256': release.digest(self.debug['minified/' + name + '.map'])}
                for name in self.files if name.endswith('.js')}}).encode()

    def validate(self):
        with patch.object(release, 'committed', return_value=(ROOT / 'scripts/minify-js.cjs').read_bytes()):
            release.validate_timer_optimization(ROOT, 'fixture', self.package, self.files, self.debug)

    def test_originals_and_maps_reproduce_release_bytes(self):
        self.validate()
        self.assertIn(b'Copyright test', self.files['plugin.js'])

    def test_tampered_code_fails_even_after_hashes_are_recomputed(self):
        self.files['plugin.js'] += b'other();\n'
        self.metadata()
        with self.assertRaisesRegex(ValueError, 'code/map differs'):
            self.validate()

    def test_tampered_map_fails_even_after_hashes_are_recomputed(self):
        self.debug['minified/plugin.js.map'] += b' '
        self.metadata()
        with self.assertRaisesRegex(ValueError, 'code/map differs'):
            self.validate()

    def test_runtime_map_is_rejected(self):
        self.files['plugin.js.map'] = b'{}'
        with self.assertRaisesRegex(ValueError, 'Debug map in release'):
            self.validate()
