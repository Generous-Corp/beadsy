#!/usr/bin/env python3
"""Regression tests for exact-head Beasy format receipt verification."""
from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location("verify_format_receipt", ROOT / "scripts" / "verify_format_receipt.py")
assert _spec and _spec.loader
_verify = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_verify)


class FormatReceiptTests(unittest.TestCase):
    def _receipt(self, root: Path, source_head: str | None = None) -> tuple[Path, Path]:
        build = root / "build"
        artifact = build / "VST3" / "Beasy.vst3" / "Contents" / "MacOS" / "Beasy"
        artifact.parent.mkdir(parents=True)
        artifact.write_bytes(b"known artifact")
        receipt = {
            "schema": "beasy.format-status.v1",
            "source": {"path": str(ROOT), "git_head": source_head or _verify.current_head(ROOT)},
            "build": {"path": str(build)},
            "formats": [{
                "format": "VST3", "status": "supported",
                "evidence": {"artifact": {
                    "path": "VST3/Beasy.vst3/Contents/MacOS/Beasy",
                    "exists": True, "is_file": True,
                    "bytes": artifact.stat().st_size,
                    "sha256": _verify.sha256(artifact),
                }},
            }, {
                "format": "AAX", "status": "blocked",
                "evidence": {"artifact": {
                    "path": "AAX/Beasy.aaxplugin/Contents/MacOS/Beasy",
                    "exists": False, "is_file": False,
                }},
            }],
        }
        path = root / "receipt.json"
        path.write_text(json.dumps(receipt), encoding="utf-8")
        return path, build

    def test_current_head_and_hashes_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            receipt, build = self._receipt(Path(directory))
            self.assertEqual([], _verify.verify(receipt, ROOT, build))

    def test_stale_head_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            receipt, build = self._receipt(Path(directory), "0" * 40)
            errors = _verify.verify(receipt, ROOT, build)
            self.assertTrue(any("stale source head" in error for error in errors))

    def test_artifact_hash_change_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            receipt, build = self._receipt(Path(directory))
            artifact = build / "VST3" / "Beasy.vst3" / "Contents" / "MacOS" / "Beasy"
            artifact.write_bytes(b"changed artifact")
            errors = _verify.verify(receipt, ROOT, build)
            self.assertTrue(any("SHA-256 changed" in error for error in errors))

    def test_path_escape_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            receipt, build = self._receipt(Path(directory))
            payload = json.loads(receipt.read_text(encoding="utf-8"))
            payload["formats"][0]["evidence"]["artifact"]["path"] = "../../outside"
            receipt.write_text(json.dumps(payload), encoding="utf-8")
            errors = _verify.verify(receipt, ROOT, build)
            self.assertTrue(any("escapes build directory" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
