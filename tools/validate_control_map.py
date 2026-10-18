#!/usr/bin/env python3
"""Deterministic, dependency-free validation for Beadsy's UI provenance maps."""
from __future__ import annotations
import argparse, json, math
from pathlib import Path

# These are the controls the current Beadsy editor actually binds.  The five
# grain controls are joined by seven source-audited auxiliary pots whose
# product parameter contracts now exist in beadsy.hpp.  Keep the allow-list
# here as a deliberate release gate: a placement alone must never promote a
# geometry candidate to an automatable control.
EXPECTED_BOUND = {
    "density", "time", "pitch", "size", "shape",
    "feedback", "dry_wet", "reverb",
    "time_cv_amount", "size_cv_amount", "shape_cv_amount",
    "pitch_cv_amount",
}
EXPECTED_PROVISIONAL = 12
EXPECTED_SKINS = {"major-ornate", "major-large", "small-black", "auxiliary"}

def validate(control_path: Path, skin_path: Path) -> list[str]:
    errors: list[str] = []
    try: control = json.loads(control_path.read_text())
    except Exception as e: return [f"control map unreadable: {e}"]
    try: skin = json.loads(skin_path.read_text())
    except Exception as e: return [f"skin manifest unreadable: {e}"]
    if control.get("schema") != "beadsy.control-map.v1": errors.append("control schema must be beadsy.control-map.v1")
    if skin.get("schema") != "beadsy.knob-skin-manifest.v1": errors.append("skin schema must be beadsy.knob-skin-manifest.v1")
    source = control.get("source", {}); panel = source.get("panel_mm", {})
    W,H = panel.get("width"), panel.get("height")
    if not (isinstance(W,(int,float)) and isinstance(H,(int,float)) and W > 0 and H > 0): errors.append("source.panel_mm must have positive width and height")
    controls = control.get("controls", [])
    if len(controls) != 24: errors.append(f"expected 24 controls, found {len(controls)}")
    ids = [c.get("id") for c in controls]
    if len(ids) != len(set(ids)): errors.append("control ids must be unique")
    bound = set(); handles = set()
    for c in controls:
        cid = c.get("id"); g = c.get("geometry_ref", {}); center = g.get("center_mm", {}); x,y = center.get("x"),center.get("y"); r = g.get("radius_mm")
        if not isinstance(cid,str) or not cid: errors.append("each control needs an id")
        if g.get("source") != "dxf": errors.append(f"{cid}: geometry_ref.source must be dxf")
        if g.get("handle") in handles: errors.append(f"{cid}: duplicate DXF handle")
        handles.add(g.get("handle"))
        if not all(isinstance(v,(int,float)) and math.isfinite(v) for v in (x,y,r)) or r <= 0: errors.append(f"{cid}: invalid geometry")
        elif x-r < 0 or x+r > W or y-r < 0 or y+r > H: errors.append(f"{cid}: geometry outside panel")
        n=c.get("normalized",{}).get("center",{}); nr=c.get("normalized",{}).get("radius")
        if not all(isinstance(v,(int,float)) and 0 <= v <= 1 for v in (n.get("x"),n.get("y"),nr)): errors.append(f"{cid}: normalized geometry must be in [0,1]")
        skin_ref=c.get("skin_ref")
        if skin_ref not in EXPECTED_SKINS: errors.append(f"{cid}: unknown skin_ref {skin_ref!r}")
        if c.get("binding_status") == "bound":
            bound.add(cid)
            if not isinstance(c.get("parameter_id"), str) or not c.get("parameter_id"):
                errors.append(f"{cid}: bound controls require parameter_id")
            if not isinstance(c.get("automation_id"), str) or not c.get("automation_id"):
                errors.append(f"{cid}: bound controls require automation_id")
        elif c.get("binding_status") != "provisional": errors.append(f"{cid}: binding_status must be bound or provisional")
    if bound != EXPECTED_BOUND: errors.append(f"bound controls mismatch: {sorted(bound)}")
    provisional = sum(c.get("binding_status") == "provisional" for c in controls)
    if provisional != EXPECTED_PROVISIONAL: errors.append(f"expected {EXPECTED_PROVISIONAL} provisional controls, found {provisional}")
    review = control.get("validation",{}).get("parameter_binding_review", {})
    if set(review.get("bound_ids", [])) != EXPECTED_BOUND: errors.append("parameter review bound_ids must match the 12 editor-bound controls")
    if review.get("provisional_count") != EXPECTED_PROVISIONAL: errors.append(f"parameter review must record {EXPECTED_PROVISIONAL} provisional controls")
    skins = skin.get("skins", []); skin_ids = {s.get("id") for s in skins}
    if skin_ids != EXPECTED_SKINS: errors.append(f"skin families mismatch: {sorted(skin_ids)}")
    if skin.get("source_boundary",{}).get("rights",{}).get("state") != "pending_rights_review": errors.append("skin rights state must remain pending_rights_review")
    return errors

def main() -> int:
    p=argparse.ArgumentParser(); p.add_argument("--control-map",type=Path,default=Path("provenance/control-map.json")); p.add_argument("--skins",type=Path,default=Path("provenance/knob-skin-manifest.json")); args=p.parse_args()
    errors=validate(args.control_map,args.skins)
    if errors:
        for error in errors: print(f"ERROR: {error}")
        return 1
    print(f"OK: {args.control_map} and {args.skins} validated (24 controls; 12 bound; 12 provisional)")
    return 0
if __name__ == "__main__": raise SystemExit(main())
