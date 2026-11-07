"""Derive every native persistence namespace from its actual C definitions."""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


def definitions(path):
    source = (ROOT / path).read_text()
    return {name: int(value) for name, value in
            re.findall(r"^#define\s+(\w+)\s+(\d+)u?\s*$", source, re.M)}


class TimerPersistenceKeys(unittest.TestCase):
    def test_native_namespaces_are_pairwise_disjoint(self):
        native = definitions("src/c/main.c")
        namespaces = {
            name: {native[name]} for name in (
                "LEGACY_MODEL_KEY", "WAKEUP_KEY", "PUBLICATION_KEY", "MIGRATION_KEY")
        }
        namespaces["bank headers"] = set(range(native["BANK_HEADER_KEY"],
                                                native["BANK_HEADER_KEY"] + 2))
        namespaces["bank data"] = set(range(native["BANK_DATA_KEY"],
                                             native["BANK_DATA_KEY"] + 2 * native["BANK_STRIDE"]))
        namespaces["AppGlance"] = {
            definitions("src/c/timer_glance.c")["GLANCE_CHECKPOINT_KEY"]}
        namespaces["Appearance"] = {
            definitions("src/c/timer_appearance.c")["APPEARANCE_KEY"]}
        entries = list(namespaces.items())
        for i, (left, keys) in enumerate(entries):
            for right, other in entries[i + 1:]:
                self.assertFalse(keys & other,
                                 f"{left} and {right} overwrite keys {keys & other}")


if __name__ == "__main__":
    unittest.main()
