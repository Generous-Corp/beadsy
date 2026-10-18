#!/usr/bin/env python3
"""Validate local Grainfield AU/VST3 packaging without changing the project.

AU registration is opt-in and reversible: --register-au copies the AU bundle
to a uniquely named user Components directory, restarts Apple's registrar,
runs auval, and removes the copy in a finally block. No sudo or system install
is used. VST3 validation is reported as blocked when its SDK artifact or
vst3validator is unavailable.
"""
from __future__ import annotations

import argparse
import json
import os
import plistlib
import shutil
import subprocess
import sys
import time
from pathlib import Path


def run(command: list[str], timeout: int = 45) -> tuple[int, str]:
    try:
        completed = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, timeout=timeout,
                                   check=False)
        return completed.returncode, completed.stdout
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 124, str(exc)


def au_receipt(build: Path, register: bool) -> dict:
    bundle = build / "AU" / "Grainfield.component"
    binary = bundle / "Contents" / "MacOS" / "Grainfield"
    entry = {"format": "AUv2", "bundle": str(bundle), "status": "blocked"}
    if not binary.is_file():
        entry["reason"] = "AU artifact is absent; run pulp build --target Grainfield_AU"
        return entry
    plist_path = bundle / "Contents" / "Info.plist"
    try:
        info = plistlib.loads(plist_path.read_bytes())
        component = info["AudioComponents"][0]
        expected = {"type": "aumu", "subtype": "Grai", "manufacturer": "Inde"}
        if any(component.get(k) != v for k, v in expected.items()):
            entry["reason"] = "Info.plist AudioComponents does not match Grainfield AU identity"
            entry["component"] = component
            return entry
    except (OSError, KeyError, IndexError, plistlib.InvalidFileException) as exc:
        entry["reason"] = f"invalid AU bundle metadata: {exc}"
        return entry
    entry["metadata"] = {k: component[k] for k in ("type", "subtype", "manufacturer")}
    if not register:
        entry["status"] = "metadata-pass"
        entry["reason"] = "bundle metadata is valid; registration not requested"
        return entry

    destination_root = Path.home() / "Library" / "Audio" / "Plug-Ins" / "Components"
    # AudioComponentRegistrar discovers the conventional bundle name.  Refuse
    # to replace an existing install so this path remains reversible.
    destination = destination_root / "Grainfield.component"
    destination_root.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        entry["reason"] = f"refusing to replace existing user install: {destination}"
        return entry
    try:
        shutil.copytree(bundle, destination)
        run(["killall", "AudioComponentRegistrar"], timeout=10)
        time.sleep(2)
        code, output = run(["auval", "-v", "aumu", "Grai", "Inde"])
        # AudioComponentRegistrar can race its first daemon restart.  A
        # single bounded retry keeps this validation deterministic without
        # polling or leaving a registrar process running.
        if "didn't find the component" in output:
            run(["killall", "AudioComponentRegistrar"], timeout=10)
            time.sleep(2)
            code, retry_output = run(["auval", "-v", "aumu", "Grai", "Inde"])
            output += "\n--- retry ---\n" + retry_output
        entry["auval_output"] = output
        entry["auval_exit_code"] = code
        if "AU VALIDATION SUCCEEDED" in output:
            entry["status"] = "pass"
            entry["reason"] = "temporary user registration passed auval"
        else:
            entry["reason"] = "temporary user registration did not pass auval"
    finally:
        if destination.exists():
            shutil.rmtree(destination)
    return entry


def vst3_receipt(build: Path, validator: str | None) -> dict:
    bundle = build / "VST3" / "Grainfield.vst3"
    binary = bundle / "Contents" / "MacOS" / "Grainfield"
    entry = {"format": "VST3", "bundle": str(bundle), "status": "blocked"}
    if not binary.is_file():
        entry["reason"] = "VST3 artifact absent; the installed SDK did not provide a VST3 target"
        return entry
    if not validator:
        entry["reason"] = "VST3 artifact exists but vst3validator is unavailable"
        return entry
    code, output = run([validator, str(bundle)])
    entry["validator"] = validator
    entry["validator_exit_code"] = code
    entry["validator_output"] = output
    entry["status"] = "pass" if code == 0 else "fail"
    return entry


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--output", type=Path)
    parser.add_argument("--register-au", action="store_true")
    parser.add_argument("--vst3validator", default=shutil.which("vst3validator"))
    args = parser.parse_args()
    build = args.build_dir.resolve()
    receipt = {
        "schema": "grainfield.packaging-validation.v1",
        "build_dir": str(build),
        "checks": [au_receipt(build, args.register_au),
                   vst3_receipt(build, args.vst3validator)],
    }
    receipt["status"] = "pass" if all(c["status"] in ("pass", "metadata-pass")
                                       for c in receipt["checks"]) else "partial"
    encoded = json.dumps(receipt, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded)
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
