"""Regress the public font keys against independently pinned PebbleOS role data."""
from pathlib import Path
import json, os, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class NativeType(unittest.TestCase):
    def test_font_keys_equal_stock_semantic_roles_on_both_firmware_families(self):
        expected=json.loads((ROOT/'docs/native-type.json').read_text())
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory)
            source=folder/'profiles.c'
            source.write_text('''#define TIMER_NATIVE_TYPE_PROFILE_ONLY
#include "timer_native_type.h"
#include <stdio.h>
int main(void) {
  for(unsigned family=0;family<2;++family)
    for(unsigned size=0;size<3;++size)
      for(unsigned role=0;role<TIMER_TYPE_ROLE_COUNT;++role)
        puts(timer_native_type_key(size,(TimerNativeTypeRole)role,family));
  return 0;
}
''')
            binary=folder/'profiles'
            subprocess.run([os.environ.get('CC','cc'),'-std=c99','-Wall','-Wextra','-Werror',
                '-I',str(ROOT/'src/c'),'-I',str(ROOT/'node_modules'),str(source),'-o',str(binary)],check=True)
            actual=subprocess.check_output([str(binary)],text=True).splitlines()
        wanted=[row[role] for family in ('baseline','modern') for row in expected[family] for role in expected['roles']]
        self.assertEqual(actual,wanted)
