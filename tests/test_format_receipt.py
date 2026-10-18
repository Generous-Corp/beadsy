#!/usr/bin/env python3
"""Regression tests for exact-head Beasy format receipt verification."""
from __future__ import annotations

import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location("verify_format_receipt", ROOT / "scripts" / "verify_format_receipt.py")
assert _spec and _spec.loader
_verify = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_verify)


class FormatReceiptTests(unittest.TestCase):
    def _source_fixture(self, root: Path) -> Path:
        source = root / "source"
        source.mkdir()
        run = lambda *args: subprocess.run(
            args, cwd=source, check=True,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        run("git", "init", "-q")
        run("git", "config", "user.email", "beasy-tests@example.invalid")
        run("git", "config", "user.name", "Beasy tests")
        tracked = source / "tracked.txt"
        tracked.write_text("committed\n", encoding="utf-8")
        run("git", "add", "tracked.txt")
        run("git", "commit", "-qm", "fixture")
        return source

    def _receipt(self, root: Path, source_head: str | None = None,
                 source: Path | None = None) -> tuple[Path, Path]:
        build = root / "build"
        artifact = build / "VST3" / "Beasy.vst3" / "Contents" / "MacOS" / "Beasy"
        artifact.parent.mkdir(parents=True)
        artifact.write_bytes(b"known artifact")
        source = source or ROOT
        receipt = {
            "schema": "beasy.format-status.v1",
            "source": {"path": str(source),
                       "git_head": source_head or _verify.current_head(source)},
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
            root = Path(directory)
            source = self._source_fixture(root)
            receipt, build = self._receipt(root, source=source)
            self.assertEqual([], _verify.verify(receipt, source, build))

    def test_stale_head_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self._source_fixture(root)
            receipt, build = self._receipt(root, "0" * 40, source=source)
            errors = _verify.verify(receipt, source, build)
            self.assertTrue(any("stale source head" in error for error in errors))

    def test_artifact_hash_change_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self._source_fixture(root)
            receipt, build = self._receipt(root, source=source)
            artifact = build / "VST3" / "Beasy.vst3" / "Contents" / "MacOS" / "Beasy"
            artifact.write_bytes(b"changed artifact")
            errors = _verify.verify(receipt, source, build)
            self.assertTrue(any("SHA-256 changed" in error for error in errors))

    def test_path_escape_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self._source_fixture(root)
            receipt, build = self._receipt(root, source=source)
            payload = json.loads(receipt.read_text(encoding="utf-8"))
            payload["formats"][0]["evidence"]["artifact"]["path"] = "../../outside"
            receipt.write_text(json.dumps(payload), encoding="utf-8")
            errors = _verify.verify(receipt, source, build)
            self.assertTrue(any("escapes build directory" in error for error in errors))

    def test_tracked_source_changes_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self._source_fixture(root)
            tracked = source / "tracked.txt"
            receipt, build = self._receipt(root, source=source)
            tracked.write_text("modified\n", encoding="utf-8")
            errors = _verify.verify(receipt, source, build)
            self.assertTrue(any("tracked changes" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
