#!/usr/bin/env python3
"""Validate Grainfield's converted panel and control-map sidecar.

This is intentionally a build-time check. It validates the small, reviewed
authoring surface without attempting to parse Illustrator or CAD files at
runtime.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fail(message: str) -> None:
    raise ValueError(message)


def validate(svg_path: Path, map_path: Path) -> dict:
    try:
        root = ET.parse(svg_path).getroot()
    except (ET.ParseError, OSError) as exc:
        fail(f"SVG parse failed: {exc}")

    view_box = root.attrib.get("viewBox", "").split()
    if len(view_box) != 4:
        fail("SVG must declare a four-component viewBox")
    try:
        _, _, svg_width, svg_height = map(float, view_box)
    except ValueError:
        fail("SVG viewBox must contain numeric values")
    if svg_width <= 0 or svg_height <= 0:
        fail("SVG viewBox dimensions must be positive")

    data = json.loads(map_path.read_text())
    if data.get("schema") != "pulp.beads.control-map.draft.v1":
        fail("unexpected control-map schema")
    panel = data.get("source", {}).get("panel_mm", {})
    panel_width = float(panel.get("width", 0))
    panel_height = float(panel.get("height", 0))
    if panel_width <= 0 or panel_height <= 0:
        fail("control-map panel dimensions must be positive")

    expected_width = panel_width * (svg_width / panel_width)
    expected_height = panel_height * (svg_height / panel_height)
    if abs(expected_width - svg_width) > 1e-3 or abs(expected_height - svg_height) > 1e-3:
        fail("control-map and SVG dimensions do not define a consistent affine scale")

    controls = data.get("controls")
    if not isinstance(controls, list) or not controls:
        fail("control-map must contain controls")
    ids = set()
    for control in controls:
        ident = control.get("id")
        if not isinstance(ident, str) or not re.fullmatch(r"[a-z][a-z0-9_]*", ident):
            fail(f"invalid control id: {ident!r}")
        if ident in ids:
            fail(f"duplicate control id: {ident}")
        ids.add(ident)
        geometry = control.get("geometry_ref", {})
        center = geometry.get("center_mm", {})
        x, y, radius = float(center.get("x", -1)), float(center.get("y", -1)), float(geometry.get("radius_mm", 0))
        if not (0 <= x <= panel_width and 0 <= y <= panel_height and radius > 0):
            fail(f"control geometry out of bounds: {ident}")

    return {
        "schema": "grainfield.asset-validation.v1",
        "status": "pass",
        "svg": {"path": str(svg_path), "sha256": sha256(svg_path), "viewBox": view_box},
        "control_map": {"path": str(map_path), "sha256": sha256(map_path), "controls": len(controls)},
        "rights": "authoring_probe_only; artwork_release_blocked_pending_rights_review",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--svg", type=Path, default=Path("provenance/generated/beads-panel.svg"))
    parser.add_argument("--control-map", type=Path, default=Path("provenance/control-map.draft.json"))
    parser.add_argument("--receipt", type=Path)
    args = parser.parse_args()
    try:
        receipt = validate(args.svg, args.control_map)
    except (ValueError, OSError, json.JSONDecodeError) as exc:
        print(f"asset validation failed: {exc}", file=sys.stderr)
        return 1
    if args.receipt:
        args.receipt.write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
