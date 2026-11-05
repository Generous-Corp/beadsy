#!/usr/bin/env python3
"""Drive the Beadsy in-process UI proof inside REAPER (AUv2).

Reuses Pulp's REAPER smoke session (fresh portable config, transient AU
install, scripted insert + float) and sets BEADSY_UI_PROOF so the editor's
own proof driver sends real AppKit events to Pulp's NSView inside REAPER and
logs JSON lines, which arrive through REAPER's captured stdout/stderr.

Usage: reaper_ui_proof.py <component> <plugin-name> <out-dir> [--step all]
"""
from __future__ import annotations

import argparse
import json
import os
import plistlib
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

SMOKE = Path("/Users/danielraffel/Code/pulp/tools/testing/daw-smoke")
sys.path.insert(0, str(SMOKE))
import reaper_smoke as rs  # noqa: E402


def proof_identity_copy(component: Path, work: Path, proof_name: str) -> Path:
    """Copy the built component under a distinct AudioComponent identity.

    The shipped 1.0.8 component of the same type/subtype/manufacturer lives in
    /Library and cannot be removed from an unattended session; the registry
    kept resolving the shared identity to it (REAPER logged the old 350x558
    editor). A proof-only subtype and bundle id make the freshly built binary
    the only candidate for its name, and the driver logs the bundle path it
    actually ran from."""
    dest = work / f"{proof_name}.component"
    shutil.copytree(component, dest, symlinks=True)
    plist = dest / "Contents" / "Info.plist"
    with open(plist, "rb") as fh:
        info = plistlib.load(fh)
    info["CFBundleIdentifier"] = info["CFBundleIdentifier"] + ".proof"
    info["CFBundleName"] = proof_name
    for comp in info.get("AudioComponents", []):
        manufacturer, _, _ = comp.get("name", "").partition(": ")
        comp["name"] = f"{manufacturer}: {proof_name}"
        comp["subtype"] = comp["subtype"][:2] + "Pf"
    with open(plist, "wb") as fh:
        plistlib.dump(info, fh)
    subprocess.run(["codesign", "-f", "-s", "-", "--deep", str(dest)], check=True,
                   capture_output=True)
    return dest


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("component")
    ap.add_argument("plugin_name")
    ap.add_argument("out_dir")
    ap.add_argument("--step", default="all")
    ap.add_argument("--settle", type=float, default=16.0,
                    help="seconds to leave REAPER up for the in-process driver")
    ap.add_argument("--timeout", type=int, default=45)
    a = ap.parse_args()

    out = Path(a.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="beadsy-reaper-proof-"))
    proof_name = f"{a.plugin_name} Proof"
    component = proof_identity_copy(Path(a.component).resolve(), work, proof_name)

    args = rs.build_parser().parse_args([
        "--mode", "editor-open", "--format", "au", "--plugin-name", proof_name,
        "--plugin-path", str(component), "--timeout", str(a.timeout)])
    reaper = rs.find_reaper()
    if reaper is None:
        print("REAPER not installed", file=sys.stderr)
        return 2
    rs._announce(f'"{a.plugin_name}" (au) Beadsy UI proof', a.timeout + int(a.settle))
    session = rs.ReaperSession(reaper, args)
    try:
        placed = session.place_plugin()
        if placed is not None:
            print(f"place_plugin failed: {placed}", file=sys.stderr)
            return placed
        status = session.portable / "status.txt"
        env = rs._common_env(args, status)
        env["BEADSY_UI_PROOF"] = a.step
        not_shown = session.run_until_fx_shown(env, status)
        shown = status.read_text() if status.exists() else "(no status)"
        print(f"status: {shown}")
        if not_shown is not None:
            print(f"fx not shown: {not_shown}", file=sys.stderr)
        time.sleep(a.settle)
        session.terminate()
        text = session.captured_log()
        (out / f"reaper-{a.plugin_name.replace(' ', '-')}-{a.step}.log").write_text(text)
        lines = [ln.split("] ", 1)[1] for ln in text.splitlines() if "[beadsy-ui-proof]" in ln]
        for ln in lines:
            try:
                d = json.loads(ln)
            except json.JSONDecodeError:
                print(ln)
                continue
            keys = [k for k in ("phase", "freeze", "seed_mode", "quality", "assign",
                                "menu_target", "view", "window", "bundle", "view_class",
                                "error") if k in d]
            print({k: d[k] for k in keys})
        return 0 if lines else 1
    finally:
        session.cleanup()
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
