#!/usr/bin/env python3
"""Inspect Beadsy's declared Pulp formats against one build tree.

This is an artifact/metadata receipt only. It does not inspect installed hosts,
load plugins, or run format validators. Missing artifacts are reported as
blocked rather than inferred to be supported.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

FORMAT_ARTIFACTS = {
    "VST3": "VST3/Beadsy.vst3/Contents/MacOS/Beadsy",
    "CLAP": "CLAP/Beadsy.clap/Contents/MacOS/Beadsy",
    "LV2": "LV2/Beadsy.lv2/Beadsy.so",
    "AU": "AU/Beadsy.component/Contents/MacOS/Beadsy",
    "AUv3": "AUv3/Beadsy.appex/Contents/MacOS/Beadsy",
    "AAX": "AAX/Beadsy.aaxplugin/Contents/MacOS/Beadsy",
    "Standalone": "Beadsy.app/Contents/MacOS/Beadsy",
}


def git_head(source: Path) -> str | None:
    try:
        return subprocess.check_output(
            ["git", "-C", str(source), "rev-parse", "HEAD"], text=True
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def declared_formats(source: Path) -> list[str]:
    text = (source / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"pulp_add_plugin\s*\(.*?\n\s*FORMATS\s+([^\n]+)", text, re.S)
    if not match:
        raise SystemExit("could not find FORMATS in pulp_add_plugin")
    return match.group(1).split()


def cache_value(cache: Path, key: str) -> str | None:
    if not cache.exists():
        return None
    prefix = key + ":"
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1]
    return None


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def target_present(build: Path, fmt: str) -> bool:
    ninja = build / "build.ninja"
    if not ninja.exists():
        return False
    target = f"Beadsy_{fmt}"
    return re.search(rf"(?:target_|TARGET_FILE\s*=\s*).*{re.escape(target)}", ninja.read_text(errors="replace")) is not None


def inspect(source: Path, build: Path) -> dict:
    formats = declared_formats(source)
    pulp_dir = cache_value(build / "CMakeCache.txt", "Pulp_DIR")
    entries = []
    for fmt in formats:
        relative = FORMAT_ARTIFACTS.get(fmt)
        artifact = build / relative if relative else None
        exists = artifact is not None and artifact.is_file()
        target = target_present(build, fmt)
        evidence = {
            "artifact": {
                "path": relative,
                "exists": exists,
                "is_file": exists,
            },
            "pulp_target_present": target,
            "host_validation": "not_run",
        }
        if exists:
            evidence["artifact"].update({"bytes": artifact.stat().st_size, "sha256": sha256(artifact)})
            status = "supported"
            reason = "declared target produced an artifact in the inspected build tree"
        else:
            status = "blocked"
            if fmt == "AAX":
                reason = "AAX SDK, entry point, and validator/host evidence are unavailable"
            elif fmt == "VST3":
                reason = "declared VST3 target produced no artifact in the inspected build tree"
            else:
                reason = "declared target produced no artifact in the inspected build tree"
        entries.append({"format": fmt, "status": status, "reason": reason, "evidence": evidence})
    return {
        "schema": "beadsy.format-status.v1",
        "generated_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "project": "Beadsy",
        "source": {"path": str(source), "git_head": git_head(source)},
        "build": {
            "path": str(build),
            "cmake_cache_present": (build / "CMakeCache.txt").is_file(),
            "build_ninja_present": (build / "build.ninja").is_file(),
            "pulp_dir": pulp_dir,
        },
        "host_validation": "not_run",
        "formats": entries,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path, default=None)
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument("--strict", action="store_true", help="return 1 when any declared format is blocked")
    args = parser.parse_args()
    source = args.source_dir.resolve()
    build = (args.build_dir or source / "build").resolve()
    receipt = inspect(source, build)
    encoded = json.dumps(receipt, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")
    return 1 if args.strict and any(item["status"] == "blocked" for item in receipt["formats"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
