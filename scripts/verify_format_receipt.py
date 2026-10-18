#!/usr/bin/env python3
"""Verify a Beasy format receipt against the exact source and build artifacts.

The format inspector records evidence; this verifier prevents that evidence from
being reused after the source or binaries change.  It checks the receipt's git
head, each artifact's path confinement, byte count, and SHA-256.  A blocked
format is valid only when its declared artifact is absent; no blocked format is
promoted by this check.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from typing import Any


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def current_head(source: Path) -> str:
    return subprocess.check_output(
        ["git", "-C", str(source), "rev-parse", "HEAD"], text=True
    ).strip()


def has_tracked_changes(source: Path) -> bool:
    result = subprocess.run(
        ["git", "-C", str(source), "diff", "--quiet", "HEAD", "--"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return result.returncode != 0


def receipt_tip_is_metadata_only(source: Path, receipt_path: Path,
                                 expected_head: str, actual_head: str) -> bool:
    """Allow a receipt-only commit to publish evidence for its source parent.

    A tracked receipt cannot record the hash of the commit that contains it
    without becoming self-invalidating. The exception is deliberately narrow:
    HEAD must have exactly one parent, the recorded head must be that parent,
    and the tip commit may change only this receipt.
    """
    try:
        parent = subprocess.check_output(
            ["git", "-C", str(source), "rev-parse", f"{actual_head}^"], text=True
        ).strip()
        changed = subprocess.check_output(
            ["git", "-C", str(source), "diff-tree", "--no-commit-id", "--name-only",
             "-r", actual_head], text=True
        ).splitlines()
    except (OSError, subprocess.CalledProcessError):
        return False
    try:
        relative_receipt = receipt_path.resolve().relative_to(source.resolve()).as_posix()
    except ValueError:
        return False
    return parent == expected_head and changed == [relative_receipt]


def _confined(build: Path, relative: str) -> Path:
    candidate = (build / relative).resolve()
    root = build.resolve()
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise ValueError(f"artifact path escapes build directory: {relative!r}") from exc
    return candidate


def verify(receipt_path: Path, source: Path | None = None,
           build: Path | None = None) -> list[str]:
    """Return deterministic errors; an empty list means the receipt is current."""
    try:
        receipt: dict[str, Any] = json.loads(receipt_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return [f"receipt unreadable: {exc}"]
    if receipt.get("schema") != "beasy.format-status.v1":
        return ["receipt schema must be beasy.format-status.v1"]

    source = (source or Path(receipt.get("source", {}).get("path", "."))).resolve()
    build = (build or Path(receipt.get("build", {}).get("path", "build"))).resolve()
    errors: list[str] = []
    # A matching commit hash alone is insufficient when the checkout has
    # tracked edits: the built artifact and receipt may describe the committed
    # tree while the source being reviewed is already different. Keep the
    # exact-head gate closed until those edits are committed (or discarded).
    if has_tracked_changes(source):
        errors.append("source has tracked changes; receipt cannot be treated as exact-head")
    expected_head = receipt.get("source", {}).get("git_head")
    try:
        actual_head = current_head(source)
    except (OSError, subprocess.CalledProcessError) as exc:
        errors.append(f"cannot resolve source git head: {exc}")
    else:
        if not expected_head:
            errors.append("receipt is missing source.git_head")
        elif expected_head != actual_head and not receipt_tip_is_metadata_only(
                source, receipt_path, expected_head, actual_head):
            errors.append(f"stale source head: receipt={expected_head}, current={actual_head}")

    for entry in receipt.get("formats", []):
        fmt = entry.get("format", "<unknown>")
        artifact = entry.get("evidence", {}).get("artifact", {})
        relative = artifact.get("path")
        if not isinstance(relative, str) or not relative:
            errors.append(f"{fmt}: artifact path is missing")
            continue
        try:
            path = _confined(build, relative)
        except ValueError as exc:
            errors.append(f"{fmt}: {exc}")
            continue
        exists = path.is_file()
        recorded_exists = artifact.get("exists") is True
        status = entry.get("status")
        if status == "supported":
            if not exists or not recorded_exists:
                errors.append(f"{fmt}: supported receipt points to a missing artifact")
                continue
            recorded_bytes = artifact.get("bytes")
            if recorded_bytes != path.stat().st_size:
                errors.append(f"{fmt}: byte count changed: receipt={recorded_bytes}, current={path.stat().st_size}")
            recorded_sha = artifact.get("sha256")
            actual_sha = sha256(path)
            if recorded_sha != actual_sha:
                errors.append(f"{fmt}: SHA-256 changed: receipt={recorded_sha}, current={actual_sha}")
        elif status == "blocked":
            if exists:
                errors.append(f"{fmt}: blocked receipt has an artifact present; regenerate the receipt")
        else:
            errors.append(f"{fmt}: unsupported status {status!r}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    parser.add_argument("--source-dir", type=Path)
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    errors = verify(args.receipt, args.source_dir, args.build_dir)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print(f"OK: {args.receipt} matches the exact source head and artifact hashes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
