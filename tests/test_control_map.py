import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "tools" / "validate_control_map.py"

class ControlMapTest(unittest.TestCase):
    def test_checked_in_maps_validate(self):
        result = subprocess.run([sys.executable, str(VALIDATOR)], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_validator_rejects_duplicate_ids(self):
        control = json.loads((ROOT / "provenance/control-map.json").read_text())
        control["controls"][1]["id"] = control["controls"][0]["id"]
        bad = ROOT / "provenance" / "control-map-invalid.json"
        bad.write_text(json.dumps(control))
        try:
            result = subprocess.run([sys.executable, str(VALIDATOR), "--control-map", str(bad)], cwd=ROOT, text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unique", result.stdout)
        finally:
            bad.unlink(missing_ok=True)

if __name__ == "__main__": unittest.main()
